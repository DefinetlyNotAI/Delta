#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlobj.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TREE_WINDOW_CLASS "ARG_Tree"
#define TREE_TIMER_ID 2001
#define TREE_IMAGE_RESOURCE 101
#define TREE_TEXT_RESOURCE 102
#define TREE_TYPE_INTERVAL_MS 42
#define TREE_FINISH_DELAY_MS 4000

static HWND g_window = NULL;
static ULONG_PTR g_gdiplusToken = 0;
static GpBitmap *g_treeImage = NULL;
static char *g_story = NULL;
static char *g_visibleStory = NULL;
static size_t g_storyLength = 0;
static size_t g_typedLength = 0;
static ULONGLONG g_finishAt = 0;

/*
 * This intentionally mirrors the worker's terminal contract. It does not
 * trigger an operating-system crash.
 */
static void BSOD(void) {
    exit(EXIT_SUCCESS);
}

static BOOL loadResourceBytes(int identifier, BYTE **data, DWORD *length) {
    HRSRC resource;
    HGLOBAL loaded;
    void *resourceData;
    DWORD resourceLength;
    BYTE *copy;

    if (data == NULL || length == NULL) {
        return FALSE;
    }

    resource = FindResourceA(NULL, MAKEINTRESOURCEA(identifier), RT_RCDATA);
    if (resource == NULL) {
        return FALSE;
    }

    loaded = LoadResource(NULL, resource);
    resourceData = LockResource(loaded);
    resourceLength = SizeofResource(NULL, resource);

    if (resourceData == NULL || resourceLength == 0) {
        return FALSE;
    }

    copy = malloc(resourceLength);
    if (copy == NULL) {
        return FALSE;
    }

    memcpy(copy, resourceData, resourceLength);
    *data = copy;
    *length = resourceLength;
    return TRUE;
}

static BOOL loadTreeImage(void) {
    BYTE *data = NULL;
    DWORD length = 0;
    HGLOBAL memory;
    void *memoryData;
    IStream *stream = NULL;
    GpStatus status;

    if (!loadResourceBytes(TREE_IMAGE_RESOURCE, &data, &length)) {
        return FALSE;
    }

    memory = GlobalAlloc(GMEM_MOVEABLE, length);
    if (memory == NULL) {
        free(data);
        return FALSE;
    }

    memoryData = GlobalLock(memory);
    if (memoryData == NULL) {
        GlobalFree(memory);
        free(data);
        return FALSE;
    }

    memcpy(memoryData, data, length);
    GlobalUnlock(memory);
    free(data);

    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
        GlobalFree(memory);
        return FALSE;
    }

    status = GdipCreateBitmapFromStream(stream, &g_treeImage);
    stream->lpVtbl->Release(stream);
    return status == Ok && g_treeImage != NULL;
}

static BOOL loadTreeStory(void) {
    BYTE *data = NULL;
    DWORD length = 0;

    if (!loadResourceBytes(TREE_TEXT_RESOURCE, &data, &length)) {
        return FALSE;
    }

    g_story = calloc((size_t) length + 1, 1);
    g_visibleStory = calloc((size_t) length + 1, 1);

    if (g_story == NULL || g_visibleStory == NULL) {
        free(data);
        free(g_story);
        free(g_visibleStory);
        g_story = NULL;
        g_visibleStory = NULL;
        return FALSE;
    }

    memcpy(g_story, data, length);
    free(data);
    g_storyLength = length;
    return TRUE;
}

static void releaseTreeResources(void) {
    if (g_treeImage != NULL) {
        GdipDisposeImage((GpImage *) g_treeImage);
        g_treeImage = NULL;
    }

    if (g_gdiplusToken != 0) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }

    free(g_story);
    free(g_visibleStory);
    g_story = NULL;
    g_visibleStory = NULL;
}

static BOOL getStartupExecutablePath(char *path, size_t pathSize) {
    char startupPath[MAX_PATH];

    if (path == NULL ||
        FAILED(SHGetFolderPathA(NULL, CSIDL_STARTUP, NULL,
            SHGFP_TYPE_CURRENT, startupPath))) {
        return FALSE;
    }

    return snprintf(path, pathSize, "%s\\tree.exe", startupPath) > 0;
}

static void queueStartupSelfRemoval(void) {
    char executablePath[MAX_PATH];
    char startupExecutablePath[MAX_PATH];
    char commandLine[MAX_PATH * 3];
    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION processInfo;

    if (GetModuleFileNameA(NULL, executablePath,
                           sizeof(executablePath)) == 0 ||
        !getStartupExecutablePath(startupExecutablePath,
                                  sizeof(startupExecutablePath)) ||
        _stricmp(executablePath, startupExecutablePath) != 0) {
        return;
    }

    snprintf(commandLine, sizeof(commandLine),
             "cmd.exe /d /c ping 127.0.0.1 -n 2 > nul & del /f /q \"%s\"",
             executablePath);
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    ZeroMemory(&processInfo, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;

    if (CreateProcessA(NULL, commandLine, NULL, NULL, FALSE,
                       CREATE_NO_WINDOW, NULL, NULL, &startupInfo,
                       &processInfo)) {
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
    }
}

static void paintTree(HDC deviceContext, const RECT *clientRect) {
    RECT textRect;
    UINT imageWidth = 0;
    UINT imageHeight = 0;
    int imageX;
    int imageY;
    GpGraphics *graphics = NULL;
    HFONT font;
    HGDIOBJ oldFont;

    FillRect(deviceContext, clientRect, (HBRUSH) GetStockObject(BLACK_BRUSH));

    if (g_treeImage != NULL &&
        GdipGetImageWidth((GpImage *) g_treeImage, &imageWidth) == Ok &&
        GdipGetImageHeight((GpImage *) g_treeImage, &imageHeight) == Ok &&
        GdipCreateFromHDC(deviceContext, &graphics) == Ok) {
        imageX = (clientRect->right - (int) imageWidth) / 2;
        imageY = 40;
        GdipDrawImageRectI(graphics, (GpImage *) g_treeImage, imageX, imageY,
                           (INT) imageWidth, (INT) imageHeight);
        GdipDeleteGraphics(graphics);
    }

    textRect.left = 64;
    textRect.right = clientRect->right - 64;
    textRect.top = clientRect->bottom - 300;
    textRect.bottom = clientRect->bottom - 40;
    font = CreateFontA(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                       FF_SCRIPT, "Lucida Calligraphy");
    oldFont = SelectObject(deviceContext, font);
    SetTextColor(deviceContext, RGB(255, 255, 255));
    SetBkMode(deviceContext, TRANSPARENT);
    DrawTextA(deviceContext, g_visibleStory != NULL ? g_visibleStory : "",
              -1, &textRect, DT_WORDBREAK | DT_CENTER | DT_TOP);
    SelectObject(deviceContext, oldFont);
    DeleteObject(font);
}

static LRESULT CALLBACK TreeWindowProc(HWND hwnd, UINT message,
                                       WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_TIMER:
            if (wParam == TREE_TIMER_ID) {
                if (g_typedLength < g_storyLength) {
                    g_visibleStory[g_typedLength] = g_story[g_typedLength];
                    g_typedLength++;
                    g_visibleStory[g_typedLength] = '\0';
                    InvalidateRect(hwnd, NULL, FALSE);
                } else if (g_finishAt == 0) {
                    g_finishAt = GetTickCount64() + TREE_FINISH_DELAY_MS;
                } else if (GetTickCount64() >= g_finishAt) {
                    DestroyWindow(hwnd);
                }
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            RECT clientRect;
            HDC deviceContext = BeginPaint(hwnd, &paint);
            GetClientRect(hwnd, &clientRect);
            paintTree(deviceContext, &clientRect);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_F10) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, TREE_TIMER_ID);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcA(hwnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previousInstance,
                   LPSTR commandLine, int showCommand) {
    WNDCLASSEXA windowClass;
    GdiplusStartupInput startupInput;
    MSG message;
    int left;
    int top;
    int width;
    int height;

    (void) previousInstance;
    (void) commandLine;
    (void) showCommand;

    ZeroMemory(&startupInput, sizeof(startupInput));
    startupInput.GdiplusVersion = 1;

    if (GdiplusStartup(&g_gdiplusToken, &startupInput, NULL) != Ok ||
        !loadTreeImage() || !loadTreeStory()) {
        releaseTreeResources();
        return 1;
    }

    ZeroMemory(&windowClass, sizeof(windowClass));
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = TreeWindowProc;
    windowClass.lpszClassName = TREE_WINDOW_CLASS;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassExA(&windowClass)) {
        releaseTreeResources();
        return 1;
    }

    left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    g_window = CreateWindowExA(WS_EX_TOPMOST, TREE_WINDOW_CLASS, "TREE",
                               WS_POPUP, left, top, width, height,
                               NULL, NULL, instance, NULL);

    if (g_window == NULL) {
        releaseTreeResources();
        return 1;
    }

    SetWindowPos(g_window, HWND_TOPMOST, left, top, width, height,
                 SWP_SHOWWINDOW);
    UpdateWindow(g_window);
    SetTimer(g_window, TREE_TIMER_ID, TREE_TYPE_INTERVAL_MS, NULL);

    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    queueStartupSelfRemoval();
    releaseTreeResources();
    BSOD();
    return 0;
}
