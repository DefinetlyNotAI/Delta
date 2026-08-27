#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define UNICODE
#define _UNICODE // NOLINT(*-reserved-identifier)

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlobj.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define TREE_WINDOW_CLASS       L"ARG_Tree"
#define TREE_TIMER_ID           2001

#define TREE_IMAGE_RESOURCE     101
#define TREE_TEXT_RESOURCE      102

#define TREE_TYPE_INTERVAL_MS   42
#define TREE_FINISH_DELAY_MS    4000

#define TREE_MARGIN_X           64
#define TREE_MARGIN_TOP         40
#define TREE_TEXT_HEIGHT        280
#define TREE_IMAGE_TEXT_GAP     40

#define TREE_FONT_HEIGHT        32

static HWND g_window = NULL;

static ULONG_PTR g_gdiplusToken = 0;
static GpBitmap *g_treeImage = NULL;

/*
 * GDI+ requires the stream to stay alive for the lifetime
 * of an image created with GdipCreateBitmapFromStream().
 */
static IStream *g_treeImageStream = NULL;

static WCHAR *g_story = NULL;
static WCHAR *g_visibleStory = NULL;

static size_t g_storyLength = 0;
static size_t g_typedLength = 0;

static ULONGLONG g_finishAt = 0;

static HFONT g_storyFont = NULL;


/* ------------------------------------------------------------------------- */
/* Finale exit                                                               */
/* ------------------------------------------------------------------------- */

/*
 * Mirrors the original finale contract.
 *
 * This does not crash Windows. It simply terminates tree.exe successfully.
 */
static void BSOD(void) {
    ExitProcess(EXIT_SUCCESS);
}


/* ------------------------------------------------------------------------- */
/* DPI                                                                       */
/* ------------------------------------------------------------------------- */

static void enableDpiAwareness(void) {
    typedef BOOL (WINAPI
    *SetProcessDpiAwarenessContextFn
    )
    (HANDLE);

    const HMODULE user32 = GetModuleHandleW(L"user32.dll");

    if (user32 == NULL) {
        return;
    }

    SetProcessDpiAwarenessContextFn setAwareness =
            (SetProcessDpiAwarenessContextFn)
            GetProcAddress(
                user32,
                "SetProcessDpiAwarenessContext"
            );

    if (setAwareness != NULL) {
        /*
         * DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
         */
        setAwareness((HANDLE) - 4);
    } else {
        SetProcessDPIAware();
    }
}


/* ------------------------------------------------------------------------- */
/* Resources                                                                 */
/* ------------------------------------------------------------------------- */

static BYTE *loadResourceBytes(
    const int identifier,
    DWORD *const length
) {
    *length = 0;

    HRSRC resource =
            FindResourceW(
                NULL,
                MAKEINTRESOURCEW(identifier),
                RT_RCDATA
            );

    if (resource == NULL) {
        return NULL;
    }

    const DWORD resourceLength =
            SizeofResource(
                NULL,
                resource
            );

    if (resourceLength == 0) {
        return NULL;
    }

    const HGLOBAL loaded =
            LoadResource(
                NULL,
                resource
            );

    if (loaded == NULL) {
        return NULL;
    }

    const void *const resourceData =
            LockResource(loaded);

    if (resourceData == NULL) {
        return NULL;
    }

    BYTE *const copy =
            (BYTE *) malloc(resourceLength);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(
        copy,
        resourceData,
        resourceLength
    );

    *length = resourceLength;

    return copy;
}


/* ------------------------------------------------------------------------- */
/* Tree image                                                                */
/* ------------------------------------------------------------------------- */

static BOOL loadTreeImage(void) {
    DWORD length = 0;

    BYTE *const data =
            loadResourceBytes(
                TREE_IMAGE_RESOURCE,
                &length
            );

    if (data == NULL || length == 0) {
        free(data);
        return FALSE;
    }

    const HGLOBAL memory =
            GlobalAlloc(
                GMEM_MOVEABLE,
                (SIZE_T) length
            );

    if (memory == NULL) {
        free(data);
        return FALSE;
    }

    void *const memoryData =
            GlobalLock(memory);

    if (memoryData == NULL) {
        GlobalFree(memory);
        free(data);
        return FALSE;
    }

    memcpy(
        memoryData,
        data,
        length
    );

    GlobalUnlock(memory);
    free(data);

    /*
     * TRUE transfers ownership of the HGLOBAL to the stream.
     */
    const HRESULT result =
            CreateStreamOnHGlobal(
                memory,
                TRUE,
                &g_treeImageStream
            );

    if (FAILED(result)) {
        GlobalFree(memory);
        g_treeImageStream = NULL;

        return FALSE;
    }

    const GpStatus status =
            GdipCreateBitmapFromStream(
                g_treeImageStream,
                &g_treeImage
            );

    if (
        status != Ok ||
        g_treeImage == NULL
    ) {
        IStream_Release(
            g_treeImageStream
        );

        g_treeImageStream = NULL;

        return FALSE;
    }

    return TRUE;
}


/* ------------------------------------------------------------------------- */
/* Story                                                                     */
/* ------------------------------------------------------------------------- */

static BOOL convertUtf8Story(
    const BYTE *const data,
    const DWORD length
) {
    if (length == 0) {
        return FALSE;
    }

    const BYTE *text = data;
    DWORD textLength = length;

    /*
     * Remove optional UTF-8 BOM.
     */
    if (
        textLength >= 3 &&
        text[0] == 0xEF &&
        text[1] == 0xBB &&
        text[2] == 0xBF
    ) {
        text += 3;
        textLength -= 3;
    }

    if (
        textLength == 0 ||
        textLength > INT_MAX
    ) {
        return FALSE;
    }

    const int wideLength =
            MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                (const char *) text,
                (int) textLength,
                NULL,
                0
            );

    if (wideLength <= 0) {
        return FALSE;
    }

    g_story =
            (WCHAR *) calloc(
                (size_t) wideLength + 1,
                sizeof(WCHAR)
            );

    g_visibleStory =
            (WCHAR *) calloc(
                (size_t) wideLength + 1,
                sizeof(WCHAR)
            );

    if (
        g_story == NULL ||
        g_visibleStory == NULL
    ) {
        free(g_story);
        free(g_visibleStory);

        g_story = NULL;
        g_visibleStory = NULL;

        return FALSE;
    }

    const int converted =
            MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                (const char *) text,
                (int) textLength,
                g_story,
                wideLength
            );

    if (converted != wideLength) {
        free(g_story);
        free(g_visibleStory);

        g_story = NULL;
        g_visibleStory = NULL;

        return FALSE;
    }

    g_story[wideLength] = L'\0';
    g_visibleStory[0] = L'\0';

    g_storyLength =
            (size_t) wideLength;

    g_typedLength = 0;
    g_finishAt = 0;

    return TRUE;
}


static BOOL loadTreeStory(void) {
    DWORD length = 0;

    BYTE *const data =
            loadResourceBytes(
                TREE_TEXT_RESOURCE,
                &length
            );

    if (data == NULL) {
        return FALSE;
    }

    const BOOL result =
            convertUtf8Story(
                data,
                length
            );

    free(data);

    return result;
}


/* ------------------------------------------------------------------------- */
/* Font                                                                      */
/* ------------------------------------------------------------------------- */

static BOOL createStoryFont(void) {
    if (g_storyFont != NULL) {
        return TRUE;
    }

    g_storyFont =
            CreateFontW(
                -TREE_FONT_HEIGHT,
                0,
                0,
                0,
                FW_NORMAL,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                VARIABLE_PITCH,
                L"Lucida Calligraphy"
            );

    /*
     * Fallback in case Lucida Calligraphy is unavailable.
     */
    if (g_storyFont == NULL) {
        g_storyFont =
                CreateFontW(
                    -TREE_FONT_HEIGHT,
                    0,
                    0,
                    0,
                    FW_NORMAL,
                    FALSE,
                    FALSE,
                    FALSE,
                    DEFAULT_CHARSET,
                    OUT_OUTLINE_PRECIS,
                    CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY,
                    VARIABLE_PITCH,
                    L"Segoe UI"
                );
    }

    return g_storyFont != NULL;
}


/* ------------------------------------------------------------------------- */
/* Cleanup                                                                   */
/* ------------------------------------------------------------------------- */

static void releaseTreeResources(void) {
    if (g_storyFont != NULL) {
        DeleteObject(g_storyFont);
        g_storyFont = NULL;
    }

    if (g_treeImage != NULL) {
        GdipDisposeImage(
            (GpImage *) g_treeImage
        );

        g_treeImage = NULL;
    }

    /*
     * Release the stream after destroying the image.
     */
    if (g_treeImageStream != NULL) {
        IStream_Release(
            g_treeImageStream
        );

        g_treeImageStream = NULL;
    }

    free(g_story);
    free(g_visibleStory);

    g_story = NULL;
    g_visibleStory = NULL;

    g_storyLength = 0;
    g_typedLength = 0;
    g_finishAt = 0;

    if (g_gdiplusToken != 0) {
        GdiplusShutdown(
            g_gdiplusToken
        );

        g_gdiplusToken = 0;
    }
}


/* ------------------------------------------------------------------------- */
/* Startup self-removal                                                      */
/* ------------------------------------------------------------------------- */

static BOOL getStartupExecutablePath(
    WCHAR path[MAX_PATH]
) {
    WCHAR startupPath[MAX_PATH];

    if (
        FAILED(
            SHGetFolderPathW(
                NULL,
                CSIDL_STARTUP,
                NULL,
                SHGFP_TYPE_CURRENT,
                startupPath
            )
        )
    ) {
        return FALSE;
    }

    const int written =
            _snwprintf_s(
                path,
                MAX_PATH,
                _TRUNCATE,
                L"%ls\\tree.exe",
                startupPath
            );

    return written >= 0;
}


static void queueStartupSelfRemoval(void) {
    WCHAR executablePath[MAX_PATH];
    WCHAR startupExecutablePath[MAX_PATH];
    WCHAR commandLine[MAX_PATH * 4];

    STARTUPINFOW startupInfo;
    PROCESS_INFORMATION processInfo;

    const DWORD executableLength =
            GetModuleFileNameW(
                NULL,
                executablePath,
                ARRAYSIZE(executablePath)
            );

    if (
        executableLength == 0 ||
        executableLength >= ARRAYSIZE(executablePath)
    ) {
        return;
    }

    if (!getStartupExecutablePath(startupExecutablePath)) {
        return;
    }

    /*
     * Only delete this executable when it is specifically the copy
     * installed as Startup\tree.exe.
     */
    if (
        _wcsicmp(
            executablePath,
            startupExecutablePath
        ) != 0
    ) {
        return;
    }

    /*
     * cmd.exe waits briefly for this process to terminate before
     * deleting the executable.
     */
    const int written =
            _snwprintf_s(
                commandLine,
                ARRAYSIZE(commandLine),
                _TRUNCATE,
                L"cmd.exe /d /s /c "
                L"\"ping 127.0.0.1 -n 3 >nul "
                L"& del /f /q \\\"%ls\\\"\"",
                executablePath
            );

    if (written < 0) {
        return;
    }

    ZeroMemory(
        &startupInfo,
        sizeof(startupInfo)
    );

    ZeroMemory(
        &processInfo,
        sizeof(processInfo)
    );

    startupInfo.cb =
            sizeof(startupInfo);

    startupInfo.dwFlags =
            STARTF_USESHOWWINDOW;

    startupInfo.wShowWindow =
            SW_HIDE;

    if (
        CreateProcessW(
            NULL,
            commandLine,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &startupInfo,
            &processInfo
        )
    ) {
        CloseHandle(
            processInfo.hThread
        );

        CloseHandle(
            processInfo.hProcess
        );
    }
}


/* ------------------------------------------------------------------------- */
/* Drawing                                                                   */
/* ------------------------------------------------------------------------- */

static void drawTreeImage(
    HDC deviceContext,
    const RECT *const clientRect
) {
    if (g_treeImage == NULL) {
        return;
    }

    UINT imageWidth = 0;
    UINT imageHeight = 0;

    if (
        GdipGetImageWidth(
            (GpImage *) g_treeImage,
            &imageWidth
        ) != Ok ||
        GdipGetImageHeight(
            (GpImage *) g_treeImage,
            &imageHeight
        ) != Ok ||
        imageWidth == 0 ||
        imageHeight == 0
    ) {
        return;
    }

    const int clientWidth =
            clientRect->right -
            clientRect->left;

    const int clientHeight =
            clientRect->bottom -
            clientRect->top;

    const int availableWidth =
            clientWidth -
            TREE_MARGIN_X * 2;

    const int availableHeight =
            clientHeight -
            TREE_MARGIN_TOP -
            TREE_TEXT_HEIGHT -
            TREE_IMAGE_TEXT_GAP;

    if (
        availableWidth <= 0 ||
        availableHeight <= 0
    ) {
        return;
    }

    const double scaleX =
            (double) availableWidth /
            (double) imageWidth;

    const double scaleY =
            (double) availableHeight /
            (double) imageHeight;

    double scale =
            scaleX < scaleY
                ? scaleX
                : scaleY;

    if (scale > 1.0) {
        scale = 1.0;
    }

    int drawWidth =
            (int) (
                (double) imageWidth *
                scale
            );

    int drawHeight =
            (int) (
                (double) imageHeight *
                scale
            );

    if (drawWidth < 1) {
        drawWidth = 1;
    }

    if (drawHeight < 1) {
        drawHeight = 1;
    }

    const int imageX =
            (clientWidth - drawWidth) / 2;

    const int imageY =
            TREE_MARGIN_TOP;

    GpGraphics *graphics = NULL;

    if (
        GdipCreateFromHDC(
            deviceContext,
            &graphics
        ) != Ok ||
        graphics == NULL
    ) {
        return;
    }

    GdipSetInterpolationMode(
        graphics,
        InterpolationModeHighQualityBicubic
    );

    GdipSetPixelOffsetMode(
        graphics,
        PixelOffsetModeHighQuality
    );

    GdipDrawImageRectI(
        graphics,
        (GpImage *) g_treeImage,
        imageX,
        imageY,
        drawWidth,
        drawHeight
    );

    GdipDeleteGraphics(graphics);
}


static void drawStory(
    HDC deviceContext,
    const RECT *const clientRect
) {
    if (g_storyFont == NULL) {
        return;
    }

    RECT textRect =
            *clientRect;

    textRect.left +=
            TREE_MARGIN_X;

    textRect.right -=
            TREE_MARGIN_X;

    textRect.top =
            clientRect->bottom -
            TREE_TEXT_HEIGHT;

    textRect.bottom =
            clientRect->bottom -
            40;

    HGDIOBJ previousFont =
            SelectObject(
                deviceContext,
                g_storyFont
            );

    const COLORREF previousColor =
            SetTextColor(
                deviceContext,
                RGB(255, 255, 255)
            );

    const int previousMode =
            SetBkMode(
                deviceContext,
                TRANSPARENT
            );

    DrawTextW(
        deviceContext,
        g_visibleStory != NULL
            ? g_visibleStory
            : L"",
        -1,
        &textRect,
        DT_WORDBREAK |
        DT_CENTER |
        DT_TOP |
        DT_NOPREFIX
    );

    SetBkMode(
        deviceContext,
        previousMode
    );

    SetTextColor(
        deviceContext,
        previousColor
    );

    SelectObject(
        deviceContext,
        previousFont
    );
}


static void paintTree(
    HDC deviceContext,
    const RECT *const clientRect
) {
    if (deviceContext == NULL) {
        return;
    }

    /*
     * Intentionally render directly to the window instead of using
     * double buffering.
     *
     * Background erasure between frames preserves the original black
     * flicker used by the tree finale.
     */
    FillRect(
        deviceContext,
        clientRect,
        GetStockObject(BLACK_BRUSH)
    );

    drawTreeImage(
        deviceContext,
        clientRect
    );

    drawStory(
        deviceContext,
        clientRect
    );
}


/* ------------------------------------------------------------------------- */
/* Animation                                                                 */
/* ------------------------------------------------------------------------- */

static void updateTyping(HWND hwnd) {
    if (
        g_story == NULL ||
        g_visibleStory == NULL
    ) {
        return;
    }

    if (
        g_typedLength <
        g_storyLength
    ) {
        const WCHAR character =
                g_story[g_typedLength];

        g_visibleStory[g_typedLength] =
                character;

        ++g_typedLength;

        /*
         * Keep UTF-16 surrogate pairs together.
         */
        if (
            character >= 0xD800 &&
            character <= 0xDBFF &&
            g_typedLength < g_storyLength
        ) {
            const WCHAR next =
                    g_story[g_typedLength];

            if (
                next >= 0xDC00 &&
                next <= 0xDFFF
            ) {
                g_visibleStory[g_typedLength] =
                        next;

                ++g_typedLength;
            }
        }

        g_visibleStory[g_typedLength] =
                L'\0';

        /*
         * TRUE intentionally requests background erasure.
         * Combined with direct painting this preserves the black flash.
         */
        InvalidateRect(
            hwnd,
            NULL,
            TRUE
        );

        UpdateWindow(hwnd);

        return;
    }

    if (g_finishAt == 0) {
        g_finishAt =
                GetTickCount64() +
                TREE_FINISH_DELAY_MS;

        return;
    }

    if (
        GetTickCount64() >=
        g_finishAt
    ) {
        KillTimer(
            hwnd,
            TREE_TIMER_ID
        );

        DestroyWindow(hwnd);
    }
}


/* ------------------------------------------------------------------------- */
/* Window procedure                                                          */
/* ------------------------------------------------------------------------- */

static LRESULT CALLBACK TreeWindowProc(
    HWND hwnd,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam
) {
    switch (message) {
        case WM_TIMER:
            if (
                wParam ==
                TREE_TIMER_ID
            ) {
                updateTyping(hwnd);
            }

            return 0;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            RECT clientRect;

            HDC deviceContext =
                    BeginPaint(
                        hwnd,
                        &paint
                    );

            if (deviceContext != NULL) {
                GetClientRect(
                    hwnd,
                    &clientRect
                );

                paintTree(
                    deviceContext,
                    &clientRect
                );
            }

            EndPaint(
                hwnd,
                &paint
            );

            return 0;
        }

        case WM_KEYDOWN:
            if (
                wParam == VK_F10 ||
                wParam == VK_ESCAPE
            ) {
                DestroyWindow(hwnd);

                return 0;
            }

            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);

            return 0;

        case WM_DESTROY:
            KillTimer(
                hwnd,
                TREE_TIMER_ID
            );

            g_window = NULL;

            PostQuitMessage(0);

            return 0;

        default:
            break;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}


/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR lpCmdLine, // NOLINT(*-non-const-parameter)
    const int nShowCmd
) {
    (void) hPrevInstance;
    (void) lpCmdLine;
    (void) nShowCmd;

    enableDpiAwareness();

    GdiplusStartupInput startupInput;

    ZeroMemory(
        &startupInput,
        sizeof(startupInput)
    );

    startupInput.GdiplusVersion = 1;

    if (
        GdiplusStartup(
            &g_gdiplusToken,
            &startupInput,
            NULL
        ) != Ok
    ) {
        return EXIT_FAILURE;
    }

    if (
        !loadTreeImage() ||
        !loadTreeStory() ||
        !createStoryFont()
    ) {
        releaseTreeResources();

        return EXIT_FAILURE;
    }

    WNDCLASSEXW windowClass;

    ZeroMemory(
        &windowClass,
        sizeof(windowClass)
    );

    windowClass.cbSize =
            sizeof(windowClass);

    windowClass.hInstance =
            hInstance;

    windowClass.lpfnWndProc =
            TreeWindowProc;

    windowClass.lpszClassName =
            TREE_WINDOW_CLASS;

    windowClass.hCursor =
            LoadCursorW(
                NULL,
                IDC_ARROW
            );

    windowClass.hbrBackground =
            (HBRUSH) GetStockObject(
                BLACK_BRUSH
            );

    windowClass.style =
            CS_HREDRAW |
            CS_VREDRAW;

    if (
        !RegisterClassExW(
            &windowClass
        )
    ) {
        releaseTreeResources();

        return EXIT_FAILURE;
    }

    const int left =
            GetSystemMetrics(
                SM_XVIRTUALSCREEN
            );

    const int top =
            GetSystemMetrics(
                SM_YVIRTUALSCREEN
            );

    const int width =
            GetSystemMetrics(
                SM_CXVIRTUALSCREEN
            );

    const int height =
            GetSystemMetrics(
                SM_CYVIRTUALSCREEN
            );

    if (
        width <= 0 ||
        height <= 0
    ) {
        UnregisterClassW(
            TREE_WINDOW_CLASS,
            hInstance
        );

        releaseTreeResources();

        return EXIT_FAILURE;
    }

    g_window =
            CreateWindowExW(
                WS_EX_TOPMOST |
                WS_EX_TOOLWINDOW,
                TREE_WINDOW_CLASS,
                L"TREE",
                WS_POPUP,
                left,
                top,
                width,
                height,
                NULL,
                NULL,
                hInstance,
                NULL
            );

    if (g_window == NULL) {
        UnregisterClassW(
            TREE_WINDOW_CLASS,
            hInstance
        );

        releaseTreeResources();

        return EXIT_FAILURE;
    }

    ShowWindow(
        g_window,
        SW_SHOW
    );

    SetWindowPos(
        g_window,
        HWND_TOPMOST,
        left,
        top,
        width,
        height,
        SWP_SHOWWINDOW
    );

    UpdateWindow(g_window);

    if (
        SetTimer(
            g_window,
            TREE_TIMER_ID,
            TREE_TYPE_INTERVAL_MS,
            NULL
        ) == 0
    ) {
        DestroyWindow(g_window);

        UnregisterClassW(
            TREE_WINDOW_CLASS,
            hInstance
        );

        releaseTreeResources();

        return EXIT_FAILURE;
    }

    MSG message;

    int getMessageResult = 0;

    while (
        (
            getMessageResult =
            GetMessageW(
                &message,
                NULL,
                0,
                0
            )
        ) > 0
    ) {
        TranslateMessage(
            &message
        );

        DispatchMessageW(
            &message
        );
    }

    const BOOL messageLoopFailed =
            getMessageResult == -1;

    /*
     * Queue removal before terminating tree.exe.
     *
     * It only applies if the executable is actually:
     *
     * %APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\tree.exe
     */
    queueStartupSelfRemoval();

    UnregisterClassW(
        TREE_WINDOW_CLASS,
        hInstance
    );

    releaseTreeResources();

    if (messageLoopFailed) {
        return EXIT_FAILURE;
    }

    /*
     * Original finale behavior.
     * BSOD() is deliberately just the final process exit.
     */
    BSOD();

    return EXIT_SUCCESS;
}
