#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <propidl.h>
#include <gdiplus.h>
#include <sapi.h>
#include <wincrypt.h>
#include <shlobj.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define ARG_WINDOW_CLASS "ARG_Overlay"
#define ARG_HOTKEY_ID 1001
#define ARG_GLITCH_TIMER_ID 1002
#define ARG_STATIC_TIMER_ID 1004
#define ARG_MAX_TEXT_PROCESSES 25
#define ARG_TEXT_TIMER_ID 1003
#define ARG_MAX_POEMS 16
#define ARG_RESERVED_POEMS 8
#define ARG_TEXT_BUFFER_SIZE 4096

static const char *ARG_SESSION_FILE = "setting.mem";

static HWND g_overlay = NULL;
static volatile BOOL g_audioThreadStop = FALSE;
static BOOL g_treeStartupInstalled = FALSE;
static GpBitmap *g_staticImage = NULL;
static ULONG_PTR g_gdiplusToken = 0;
static BOOL g_staticVisible = FALSE;
static DWORD g_staticEndsAt = 0;

static BOOL g_glitchesEnabled = FALSE;
static DWORD g_glitchEndsAt = 0;
static int g_glitchStyle = 0;
static HDC g_transformDC = NULL;
static HBITMAP g_transformBitmap = NULL;
static HGDIOBJ g_transformPrevious = NULL;
static int g_transformWidth = 0;
static int g_transformHeight = 0;
static unsigned int g_glitchSeed = 0;
static BOOL g_mouseInversionAllowed = FALSE;
static BOOL g_keyboardBlockAllowed = FALSE;
static BOOL g_appLaunchAllowed = FALSE;
static BOOL g_browserAllowed = FALSE;
static BOOL g_localInfoAllowed = FALSE;
static BOOL g_mouseInversionActive = FALSE;
static BOOL g_ignoreMouseMove = FALSE;
static POINT g_lastMousePosition;
static HHOOK g_mouseHook = NULL;
static HHOOK g_keyboardHook = NULL;
static DWORD g_mouseInversionEndsAt = 0;
static DWORD g_nextMouseInversionAt = 0;
static DWORD g_keyboardBlockEndsAt = 0;
static DWORD g_nextKeyboardBlockAt = 0;
static BOOL g_keyboardBlockActive = FALSE;
static BOOL g_ttsStarted = FALSE;
static BOOL g_ttsComplete = FALSE;
static BOOL g_splitOverlayActive = FALSE;
static BOOL g_wallpaperChanged = FALSE;
static BOOL g_originalWallpaperCaptured = FALSE;
static char g_originalWallpaper[MAX_PATH];
static BOOL g_step6Active = FALSE;
static BOOL g_step6Complete = FALSE;
static int g_step6BlockedClicks = 0;
static HRGN g_smileRegion = NULL;
static HFONT g_smileFont = NULL;

static void appendSessionProcessState(FILE *file);
static void startStaticVisual(DWORD milliseconds);
static void stopStaticVisual(void);

static volatile BOOL
g_shutdownRequested = FALSE;

static char g_basePath[MAX_PATH];

/*
 * ------------------------------------------------------------
 * Path utilities
 * ------------------------------------------------------------
 */

static BOOL getBasePath(void)
{
    DWORD length =
        GetModuleFileNameA(
            NULL,
            g_basePath,
            sizeof(g_basePath)
        );

    if (length == 0 ||
        length >= sizeof(g_basePath)) {
        return FALSE;
    }

    char *slash =
        strrchr(
            g_basePath,
            '\\'
        );

    if (slash == NULL) {
        return FALSE;
    }

    *slash = '\0';

    return TRUE;
}

/*
 * ------------------------------------------------------------
 * Audio
 * ------------------------------------------------------------
 */

static void stopMP3(void)
{
    mciSendStringA(
        "stop ARG_AUDIO",
        NULL,
        0,
        NULL
    );

    mciSendStringA(
        "close ARG_AUDIO",
        NULL,
        0,
        NULL
    );
}

static BOOL playMP3(
    const char *path,
    int volume
)
{
    char command[1024];

    stopMP3();

    snprintf(
        command,
        sizeof(command),
        "open \"%s\" type mpegvideo alias ARG_AUDIO",
        path
    );

    if (mciSendStringA(
        command,
        NULL,
        0,
        NULL
    ) != 0) {
        return FALSE;
    }

    snprintf(
        command,
        sizeof(command),
        "setaudio ARG_AUDIO volume to %d",
        volume
    );

    mciSendStringA(
        command,
        NULL,
        0,
        NULL
    );

    if (mciSendStringA(
        "play ARG_AUDIO",
        NULL,
        0,
        NULL
    ) != 0) {
        stopMP3();
        return FALSE;
    }

    return TRUE;
}

static BOOL sleepInterruptible(
    DWORD milliseconds
)
{
    const DWORD interval = 50;

    DWORD elapsed = 0;

    while (
        elapsed < milliseconds &&
        !g_shutdownRequested &&
        !g_audioThreadStop
    ) {
        DWORD remaining =
            milliseconds - elapsed;

        DWORD sleepTime =
            remaining < interval
                ? remaining
                : interval;

        Sleep(sleepTime);

        elapsed += sleepTime;
    }

    return !g_shutdownRequested && !g_audioThreadStop;
}

static void playStatic(void)
{
    char path[MAX_PATH * 2];

    if (g_audioThreadStop) {
        return;
    }

    snprintf(
        path,
        sizeof(path),
        "%s\\mus\\static.mp3",
        g_basePath
    );

    if (!playMP3(
        path,
        450
    )) {
        return;
    }

    /*
     * 0.3 to 1.0 seconds.
     */
    int duration =
        300 + rand() % 701;

    startStaticVisual((DWORD)duration);
    sleepInterruptible(
        duration
    );

    if (!g_audioThreadStop) {
        stopMP3();
    }
}

static void playWhisper(void)
{
    char path[MAX_PATH * 2];

    if (g_audioThreadStop) {
        return;
    }

    snprintf(
        path,
        sizeof(path),
        "%s\\mus\\whisper.mp3",
        g_basePath
    );

    if (!playMP3(
        path,
        180
    )) {
        return;
    }

    /*
     * Whisper is approximately 18 seconds.
     * Volume is intentionally reduced.
     */
    sleepInterruptible(
        18000
    );

    if (!g_audioThreadStop) {
        stopMP3();
    }
}

/*
 * ------------------------------------------------------------
 * Generated machine whirr
 * ------------------------------------------------------------
 */

static void generateMachineWhirr(void)
{
    if (g_audioThreadStop) {
        return;
    }

    const int sampleRate = 44100;
    const int channels = 1;
    const int bitsPerSample = 16;

    const double duration = 2.0;

    const int sampleCount =
        (int)(
            sampleRate *
            duration
        );

    const int dataSize =
        sampleCount *
        channels *
        sizeof(short);

    const int fileSize =
        44 +
        dataSize;

    BYTE *wav =
        malloc(fileSize);

    if (wav == NULL) {
        return;
    }

    ZeroMemory(
        wav,
        fileSize
    );

    memcpy(
        wav + 0,
        "RIFF",
        4
    );

    *(DWORD *)(wav + 4) =
        fileSize - 8;

    memcpy(
        wav + 8,
        "WAVE",
        4
    );

    memcpy(
        wav + 12,
        "fmt ",
        4
    );

    *(DWORD *)(wav + 16) = 16;
    *(WORD *)(wav + 20) = 1;
    *(WORD *)(wav + 22) = channels;
    *(DWORD *)(wav + 24) = sampleRate;

    *(DWORD *)(wav + 28) =
        sampleRate *
        channels *
        bitsPerSample /
        8;

    *(WORD *)(wav + 32) =
        channels *
        bitsPerSample /
        8;

    *(WORD *)(wav + 34) =
        bitsPerSample;

    memcpy(
        wav + 36,
        "data",
        4
    );

    *(DWORD *)(wav + 40) =
        dataSize;

    short *samples =
        (short *)(wav + 44);

    double phase1 = 0.0;
    double phase2 = 0.0;
    double phase3 = 0.0;

    for (
        int i = 0;
        i < sampleCount;
        i++
    ) {
        double t =
            (double)i /
            sampleRate;

        double freq1 =
            55.0 +
            sin(t * 1.7) * 7.0;

        double freq2 =
            110.0 +
            sin(t * 2.3) * 12.0;

        double freq3 =
            220.0 +
            sin(t * 3.1) * 18.0;

        phase1 +=
            2.0 *
            M_PI *
            freq1 /
            sampleRate;

        phase2 +=
            2.0 *
            M_PI *
            freq2 /
            sampleRate;

        phase3 +=
            2.0 *
            M_PI *
            freq3 /
            sampleRate;

        double signal =
            sin(phase1) * 0.35 +
            sin(phase2) * 0.15 +
            sin(phase3) * 0.06;

        double noise =
            ((double)rand() /
             RAND_MAX) *
            2.0 -
            1.0;

        signal +=
            noise * 0.035;

        double envelope = 1.0;

        if (t < 0.15) {
            envelope =
                t / 0.15;
        }

        if (t > duration - 0.25) {
            envelope =
                (duration - t) /
                0.25;
        }

        signal *= envelope;

        if (signal > 1.0)
            signal = 1.0;

        if (signal < -1.0)
            signal = -1.0;

        samples[i] =
            (short)(
                signal *
                32767.0
            );
    }

    PlaySoundA(
        (LPCSTR)wav,
        NULL,
        SND_MEMORY |
        SND_ASYNC |
        SND_NODEFAULT
    );

    sleepInterruptible(
        2100
    );

    PlaySoundA(
        NULL,
        NULL,
        0
    );

    free(wav);
}

/*
 * ------------------------------------------------------------
 * Audio thread
 * ------------------------------------------------------------
 */

static DWORD WINAPI audioThread(
    LPVOID parameter
)
{
    (void)parameter;

    while (!g_shutdownRequested && !g_audioThreadStop) {

        int delay =
            5000 +
            rand() % 15001;

        if (!sleepInterruptible(
            delay
        )) {
            break;
        }

        if (g_shutdownRequested || g_audioThreadStop) {
            break;
        }

        switch (rand() % 3) {

            case 0:
                playStatic();
                break;

            case 1:
                playWhisper();
                break;

            case 2:
                generateMachineWhirr();
                break;
        }
    }

    return 0;
}

/*
 * ------------------------------------------------------------
 * Overlay
 * ------------------------------------------------------------
 */

static void releaseTransformScreen(void)
{
    if (g_transformDC != NULL) {
        SelectObject(g_transformDC, g_transformPrevious);
        DeleteDC(g_transformDC);
        g_transformDC = NULL;
        g_transformPrevious = NULL;
    }

    if (g_transformBitmap != NULL) {
        DeleteObject(g_transformBitmap);
        g_transformBitmap = NULL;
    }
}

static void stopGlitchEffects(void)
{
    g_glitchesEnabled = FALSE;
    g_glitchEndsAt = 0;
    g_glitchStyle = 0;
    releaseTransformScreen();

    if (g_overlay != NULL) {
        KillTimer(
            g_overlay,
            ARG_GLITCH_TIMER_ID
        );

        SetLayeredWindowAttributes(
            g_overlay,
            0,
            0,
            LWA_ALPHA
        );
    }
}

static BOOL captureTransformScreen(void)
{
    HDC screen;
    int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int top = GetSystemMetrics(SM_YVIRTUALSCREEN);

    releaseTransformScreen();
    g_transformWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_transformHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    screen = GetDC(NULL);

    if (screen == NULL || g_transformWidth <= 0 ||
        g_transformHeight <= 0) {
        if (screen != NULL) ReleaseDC(NULL, screen);
        return FALSE;
    }

    g_transformDC = CreateCompatibleDC(screen);
    g_transformBitmap = CreateCompatibleBitmap(screen, g_transformWidth,
                                                g_transformHeight);
    if (g_transformDC == NULL || g_transformBitmap == NULL) {
        ReleaseDC(NULL, screen);
        releaseTransformScreen();
        return FALSE;
    }

    g_transformPrevious = SelectObject(g_transformDC, g_transformBitmap);
    BitBlt(g_transformDC, 0, 0, g_transformWidth, g_transformHeight, screen,
           left, top, SRCCOPY);
    ReleaseDC(NULL, screen);
    return TRUE;
}

static BOOL loadStaticImage(void)
{
    char imagePath[MAX_PATH * 2];
    wchar_t widePath[MAX_PATH * 2];
    GdiplusStartupInput startupInput;

    if (g_staticImage != NULL) {
        return TRUE;
    }

    if (g_gdiplusToken == 0) {
        ZeroMemory(&startupInput, sizeof(startupInput));
        startupInput.GdiplusVersion = 1;

        if (GdiplusStartup(&g_gdiplusToken, &startupInput, NULL) != Ok) {
            g_gdiplusToken = 0;
            return FALSE;
        }
    }

    snprintf(imagePath, sizeof(imagePath), "%s\\img\\static.jpg",
             g_basePath);
    if (MultiByteToWideChar(CP_ACP, 0, imagePath, -1, widePath,
                            sizeof(widePath) / sizeof(widePath[0])) == 0 ||
        GdipCreateBitmapFromFile(widePath, &g_staticImage) != Ok) {

        g_staticImage = NULL;
        return FALSE;
    }

    return TRUE;
}

static void releaseStaticImage(void)
{
    if (g_staticImage != NULL) {
        GdipDisposeImage((GpImage *)g_staticImage);
        g_staticImage = NULL;
    }

    if (g_gdiplusToken != 0) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

static void startStaticVisual(DWORD milliseconds)
{
    if (g_overlay == NULL || !loadStaticImage()) {
        return;
    }

    g_staticVisible = TRUE;
    g_staticEndsAt = GetTickCount() + milliseconds;
    SetTimer(g_overlay, ARG_STATIC_TIMER_ID, 1, NULL);
    InvalidateRect(g_overlay, NULL, FALSE);
}

static void stopStaticVisual(void)
{
    g_staticVisible = FALSE;
    g_staticEndsAt = 0;

    if (g_overlay != NULL) {
        KillTimer(g_overlay, ARG_STATIC_TIMER_ID);
        InvalidateRect(g_overlay, NULL, FALSE);
    }
}

static void paintStaticVisual(HDC deviceContext, const RECT *clientRect)
{
    GpGraphics *graphics = NULL;

    if (!g_staticVisible || g_staticImage == NULL ||
        GdipCreateFromHDC(deviceContext, &graphics) != Ok) {

        return;
    }

    GdipDrawImageRectI(graphics, (GpImage *)g_staticImage,
                       clientRect->left, clientRect->top,
                       clientRect->right - clientRect->left,
                       clientRect->bottom - clientRect->top);
    GdipDeleteGraphics(graphics);
}

static void stopStaticOverlay(void)
{
    mciSendStringA("stop ARG_STATIC", NULL, 0, NULL);
    mciSendStringA("close ARG_STATIC", NULL, 0, NULL);
    stopStaticVisual();
}

static void playStaticOverlayFor(DWORD milliseconds)
{
    char path[MAX_PATH * 2];
    char command[1024];

    snprintf(path, sizeof(path), "%s\\mus\\static.mp3", g_basePath);
    stopStaticOverlay();
    snprintf(command, sizeof(command),
             "open \"%s\" type mpegvideo alias ARG_STATIC", path);

    if (mciSendStringA(command, NULL, 0, NULL) == 0) {
        snprintf(command, sizeof(command),
                 "play ARG_STATIC from 0 to %lu",
                 (unsigned long)milliseconds);
        mciSendStringA(command, NULL, 0, NULL);
        startStaticVisual(milliseconds);
    }
}

static void playStaticOverlay(void)
{
    playStaticOverlayFor(50);
}

static BOOL isValidTreeExecutable(const BYTE *data, DWORD length)
{
    DWORD peOffset;

    if (data == NULL || length < 64 || data[0] != 'M' || data[1] != 'Z') {
        return FALSE;
    }

    peOffset = (DWORD)data[60] |
        ((DWORD)data[61] << 8) |
        ((DWORD)data[62] << 16) |
        ((DWORD)data[63] << 24);

    return peOffset <= length - 4 &&
        memcmp(data + peOffset, "PE\0\0", 4) == 0;
}

static BOOL extractTreeExecutable(void)
{
    char dataPath[MAX_PATH * 2];
    char executablePath[MAX_PATH * 2];
    char temporaryPath[MAX_PATH * 2 + 16];
    FILE *input;
    FILE *output;
    long length;

    snprintf(dataPath, sizeof(dataPath), "%s\\tree.dat", g_basePath);
    snprintf(executablePath, sizeof(executablePath), "%s\\tree.exe",
             g_basePath);
    input = fopen(dataPath, "rb");

    if (input == NULL) return FALSE;

    fseek(input, 0, SEEK_END);
    length = ftell(input);
    rewind(input);

    if (length <= 0) {
        fclose(input);
        return FALSE;
    }

    char *encoded = calloc((size_t)length + 1, 1);
    DWORD decodedLength = 0;

    if (encoded == NULL ||
        fread(encoded, 1, (size_t)length, input) != (size_t)length ||
        !CryptStringToBinaryA(encoded, (DWORD)length,
                              CRYPT_STRING_BASE64, NULL,
                              &decodedLength, NULL, NULL)) {
        free(encoded);
        fclose(input);
        return FALSE;
    }

    if (decodedLength == 0) {
        free(encoded);
        fclose(input);
        return FALSE;
    }

    BYTE *decoded = malloc(decodedLength);

    if (decoded == NULL ||
        !CryptStringToBinaryA(encoded, (DWORD)length,
                              CRYPT_STRING_BASE64, decoded,
                              &decodedLength, NULL, NULL)) {
        free(decoded);
        free(encoded);
        fclose(input);
        return FALSE;
    }

    if (!isValidTreeExecutable(decoded, decodedLength)) {
        free(decoded);
        free(encoded);
        fclose(input);
        return FALSE;
    }

    snprintf(temporaryPath, sizeof(temporaryPath), "%s.tmp",
             executablePath);
    output = fopen(temporaryPath, "wb");

    if (output == NULL ||
        fwrite(decoded, 1, decodedLength, output) != decodedLength) {
        if (output != NULL) fclose(output);
        DeleteFileA(temporaryPath);
        free(decoded);
        free(encoded);
        fclose(input);
        return FALSE;
    }

    if (fclose(output) != 0 ||
        !MoveFileExA(temporaryPath, executablePath,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {

        DeleteFileA(temporaryPath);
        free(decoded);
        free(encoded);
        fclose(input);
        return FALSE;
    }
    free(decoded);
    free(encoded);
    fclose(input);
    return TRUE;
}

/*
 * Final-stage contract.  This is intentionally a stub while the
 * final sequence is being developed: it behaves like the terminal
 * action without forcing an operating-system crash.
 */
static void BSOD(void)
{
    exit(EXIT_SUCCESS);
}

static BOOL installTreeStartup(void)
{
    char startupPath[MAX_PATH];
    char sourcePath[MAX_PATH * 2];
    char destinationPath[MAX_PATH * 2];

    if (FAILED(SHGetFolderPathA(NULL, CSIDL_STARTUP, NULL,
                                SHGFP_TYPE_CURRENT, startupPath))) {
        return FALSE;
    }

    snprintf(sourcePath, sizeof(sourcePath), "%s\\tree.exe", g_basePath);
    snprintf(destinationPath, sizeof(destinationPath), "%s\\tree.exe",
             startupPath);

    return CopyFileA(sourcePath, destinationPath, FALSE);
}

static void removeInstalledTreeStartup(void)
{
    char startupPath[MAX_PATH];
    char destinationPath[MAX_PATH * 2];

    if (!g_treeStartupInstalled ||
        FAILED(SHGetFolderPathA(NULL, CSIDL_STARTUP, NULL,
                                SHGFP_TYPE_CURRENT, startupPath))) {
        return;
    }

    snprintf(destinationPath, sizeof(destinationPath), "%s\\tree.exe",
             startupPath);
    DeleteFileA(destinationPath);
    g_treeStartupInstalled = FALSE;
}

static void restoreWallpaper(void)
{
    if (g_wallpaperChanged && g_originalWallpaperCaptured) {
        SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0,
                              g_originalWallpaper,
                              SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    }

    g_wallpaperChanged = FALSE;
    g_originalWallpaperCaptured = FALSE;
    g_originalWallpaper[0] = '\0';
}

static BOOL setWallpaperFile(const char *relativePath)
{
    char path[MAX_PATH * 2];

    snprintf(path, sizeof(path), "%s\\%s", g_basePath, relativePath);

    if (!g_wallpaperChanged) {
        if (!SystemParametersInfoA(SPI_GETDESKWALLPAPER,
                                   sizeof(g_originalWallpaper),
                                   g_originalWallpaper, 0)) {

            return FALSE;
        }

        g_originalWallpaperCaptured = TRUE;
    }

    if (!SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, path,
                               SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
        return FALSE;
    }

    g_wallpaperChanged = TRUE;
    return TRUE;
}

static void startGlitchEffects(void)
{
    if (g_overlay == NULL) {
        return;
    }

    g_glitchesEnabled = TRUE;
    g_glitchSeed =
        ((unsigned int)GetTickCount()) ^
        ((unsigned int)GetCurrentProcessId());

    SetTimer(
        g_overlay,
        ARG_GLITCH_TIMER_ID,
        180,
        NULL
    );
}

static unsigned int nextGlitchRandom(void)
{
    g_glitchSeed =
        g_glitchSeed * 1664525u + 1013904223u;

    return g_glitchSeed;
}

static void writeSessionMemory(
    const char *status
)
{
    char path[MAX_PATH * 2];
    FILE *file;

    snprintf(path, sizeof(path), "%s\\%s", g_basePath,
             ARG_SESSION_FILE);
    file = fopen(path, "w");

    if (file == NULL) {
        return;
    }

    fprintf(file, "format=ARG_SESSION_1\n");
    fprintf(file, "status=%s\n", status);
    fprintf(file, "mouse_inversion_allowed=%d\n",
            g_mouseInversionAllowed ? 1 : 0);
    fprintf(file, "mouse_inversion_active=%d\n",
            g_mouseInversionActive ? 1 : 0);
    fprintf(file, "keyboard_block_allowed=%d\n",
            g_keyboardBlockAllowed ? 1 : 0);
    fprintf(file, "keyboard_block_active=%d\n",
            g_keyboardBlockActive ? 1 : 0);
    appendSessionProcessState(file);
    fclose(file);
}

static LRESULT CALLBACK mouseHookProc(
    int code,
    WPARAM message,
    LPARAM parameter
)
{
    if (code == HC_ACTION &&
        message == WM_MOUSEMOVE &&
        g_mouseInversionActive) {

        MSLLHOOKSTRUCT *mouse =
            (MSLLHOOKSTRUCT *)parameter;

        if (g_ignoreMouseMove) {
            g_ignoreMouseMove = FALSE;
            g_lastMousePosition = mouse->pt;

            return CallNextHookEx(
                g_mouseHook,
                code,
                message,
                parameter
            );
        }

        LONG deltaX = mouse->pt.x - g_lastMousePosition.x;
        LONG deltaY = mouse->pt.y - g_lastMousePosition.y;
        POINT inverted = {
            g_lastMousePosition.x - deltaX,
            g_lastMousePosition.y - deltaY
        };

        int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int right = left + GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1;
        int bottom = top + GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1;

        if (inverted.x < left) inverted.x = left;
        if (inverted.x > right) inverted.x = right;
        if (inverted.y < top) inverted.y = top;
        if (inverted.y > bottom) inverted.y = bottom;

        g_ignoreMouseMove = TRUE;
        SetCursorPos(inverted.x, inverted.y);
        g_lastMousePosition = inverted;

        return 1;
    }

    return CallNextHookEx(
        g_mouseHook,
        code,
        message,
        parameter
    );
}

static void stopMouseInversion(void)
{
    g_mouseInversionActive = FALSE;
    g_mouseInversionEndsAt = 0;
    g_ignoreMouseMove = FALSE;

    if (g_mouseHook != NULL) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = NULL;
    }

    writeSessionMemory("active");
}

static void startMouseInversion(
    DWORD duration
)
{
    if (!g_mouseInversionAllowed ||
        g_mouseInversionActive) {

        return;
    }

    GetCursorPos(&g_lastMousePosition);
    g_mouseHook = SetWindowsHookExA(
        WH_MOUSE_LL,
        mouseHookProc,
        GetModuleHandleA(NULL),
        0
    );

    if (g_mouseHook == NULL) {
        return;
    }

    g_mouseInversionActive = TRUE;
    g_mouseInversionEndsAt = GetTickCount() + duration;
    writeSessionMemory("active");
}

static LRESULT CALLBACK keyboardHookProc(
    int code,
    WPARAM message,
    LPARAM parameter
)
{
    if (code == HC_ACTION && g_keyboardBlockActive &&
        (message == WM_KEYDOWN || message == WM_SYSKEYDOWN ||
         message == WM_KEYUP || message == WM_SYSKEYUP)) {

        KBDLLHOOKSTRUCT *keyboard = (KBDLLHOOKSTRUCT *)parameter;

        if (keyboard->vkCode != VK_F10) {
            return 1;
        }
    }

    return CallNextHookEx(g_keyboardHook, code, message, parameter);
}

static void stopKeyboardBlock(void)
{
    g_keyboardBlockActive = FALSE;
    g_keyboardBlockEndsAt = 0;

    if (g_keyboardHook != NULL) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = NULL;
    }

    writeSessionMemory("active");
}

static void startKeyboardBlock(DWORD duration)
{
    if (!g_keyboardBlockAllowed || g_keyboardBlockActive) {
        return;
    }

    g_keyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, keyboardHookProc,
                                       GetModuleHandleA(NULL), 0);
    if (g_keyboardHook == NULL) {
        return;
    }

    g_keyboardBlockActive = TRUE;
    g_keyboardBlockEndsAt = GetTickCount() + duration;
    writeSessionMemory("active");
}

static void updateKeyboardBlock(DWORD now)
{
    if (!g_keyboardBlockAllowed) {
        return;
    }

    if (g_keyboardBlockActive &&
        (LONG)(now - g_keyboardBlockEndsAt) >= 0) {

        stopKeyboardBlock();
        g_nextKeyboardBlockAt = now + 4000 + nextGlitchRandom() % 10001;
    } else if (!g_keyboardBlockActive &&
               (LONG)(now - g_nextKeyboardBlockAt) >= 0) {

        startKeyboardBlock(900 + nextGlitchRandom() % 2101);
    }
}

static BOOL acceptTerms(void)
{
    int accepted = MessageBoxA(
        NULL,
        "This is a psych-horror ARG puzzle. It can show flashing "
        "visual effects, play audio and locally installed TTS, and "
        "open program-owned Windows applications. It can optionally "
        "display local account and network information. Close important "
        "work before continuing. Step 6 temporarily blocks clicks outside "
        "two transparent smile glyphs; clicks through those glyphs can "
        "reach whatever is underneath. Step 7 can create a supplied "
        "tree.exe beside this program and copy it to your current-user "
        "Startup folder for the separate next-boot finale. Press F10 at "
        "any time to stop and clean up.\n\n"
        "Do you accept these terms and want to begin?",
        "ARG safety notice",
        MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2
    );

    if (accepted != IDOK) {
        return FALSE;
    }

    int mouseChoice = MessageBoxA(
        NULL,
        "Enable temporary inverted mouse movement during Step 4? "
        "The effect runs only for short intervals and F10 restores "
        "normal movement immediately.",
        "ARG optional mouse effect",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2
    );

    g_mouseInversionAllowed = mouseChoice == IDYES;

    g_keyboardBlockAllowed = MessageBoxA(
        NULL,
        "Enable temporary keyboard blocking during Steps 4 and 5? "
        "Each interval is brief, and F10 always remains available to stop "
        "and clean up immediately.",
        "ARG optional keyboard effect",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2
    ) == IDYES;

    g_appLaunchAllowed = MessageBoxA(
        NULL,
        "Allow Step 5 to open and temporarily resize only applications "
        "created by ARG (Calculator, Paint, Notepad, or CMD)? F10 closes "
        "only ARG-created applications.",
        "ARG optional application effect",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2
    ) == IDYES;

    g_browserAllowed = g_appLaunchAllowed &&
        MessageBoxA(
            NULL,
            "Allow ARG to launch Microsoft Edge in InPrivate mode with "
            "an empty about:blank tab during Step 5?",
            "ARG optional browser effect",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2
        ) == IDYES;

    g_localInfoAllowed = g_appLaunchAllowed &&
        MessageBoxA(
            NULL,
            "Allow ARG to show only local, non-changing command output: "
            "whoami, hostname, IPv4 addresses, and MAC addresses? "
            "Nothing is sent anywhere.",
            "ARG optional local information",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2
        ) == IDYES;

    return TRUE;
}

typedef enum {
    TEXT_HOST_CMD,
    TEXT_HOST_NOTEPAD,
    TEXT_HOST_APPLICATION
} TextHost;

typedef struct {
    HANDLE process;
    DWORD processId;
    HWND window;
    TextHost host;
    BOOL autoType;
    BOOL geometryCaptured;
    RECT originalRect;
    DWORD startedAt;
    DWORD nextCharacterAt;
    DWORD closeAt;
    BOOL errorReported;
    size_t position;
    wchar_t text[ARG_TEXT_BUFFER_SIZE];
} TextProcess;

static TextProcess g_textProcesses[ARG_MAX_TEXT_PROCESSES];
static int g_textProcessCount = 0;
static char g_poems[ARG_MAX_POEMS][ARG_TEXT_BUFFER_SIZE];
static int g_reservedPoems[ARG_RESERVED_POEMS];
static int g_ttsPoems[ARG_RESERVED_POEMS];
static int g_poemCount = 0;
static int g_nextPoem = 0;
static ISpVoice *g_ttsVoice = NULL;
static CRITICAL_SECTION g_ttsLock;
static HANDLE g_ttsThread = NULL;

typedef enum {
    STEP7_WALLPAPER_NONE,
    STEP7_WALLPAPER_HAPPY,
    STEP7_WALLPAPER_HT,
    STEP7_WALLPAPER_GREEN
} Step7WallpaperMode;

typedef struct {
    Step7WallpaperMode wallpaperMode;
    unsigned int wallpaperFrame;
    unsigned int wallpaperFrameCount;
    DWORD wallpaperFrameInterval;
    ULONGLONG nextWallpaperAt;
    BOOL reminderActive;
    ULONGLONG nextReminderAt;
    int countdown;
    ULONGLONG nextCountdownAt;
} Step7State;

static Step7State g_step7;

static const char *g_step7RegularWallpapers[] = {
    "img\\creep\\everythingg2.png",
    "img\\creep\\everythingg3.png",
    "img\\creep\\everythingg4.png",
    "img\\creep\\everythingg5.png",
    "img\\creep\\everythingg6.png",
    "img\\creep\\clownguy.webp",
    "img\\creep\\artpicture.webp",
    "img\\creep\\dream.jpeg",
    "img\\creep\\colorbars.png",
    "img\\creep\\worldlol.jpg",
    "img\\creep\\ohno.png",
    "img\\creep\\notclean.webp",
    "img\\creep\\mm7.webp",
    "img\\creep\\somethinglikethis.webp",
    "img\\creep\\white.png"
};

static void appendSessionProcessState(FILE *file)
{
    fprintf(file, "text_process_count=%d\n",
            g_textProcessCount);
    fprintf(file, "tts_started=%d\n",
            g_ttsStarted ? 1 : 0);

    for (int index = 0;
         index < g_textProcessCount;
         index++) {

        fprintf(file, "text_process_pid_%d=%lu\n",
                index,
                (unsigned long)g_textProcesses[index].processId);
    }
}

static BOOL CALLBACK findWindowForProcess(
    HWND hwnd,
    LPARAM parameter
)
{
    TextProcess *textProcess = (TextProcess *)parameter;
    DWORD processId = 0;

    GetWindowThreadProcessId(
        hwnd,
        &processId
    );

    if (processId == textProcess->processId &&
        IsWindowVisible(hwnd)) {

        textProcess->window = hwnd;
        return FALSE;
    }

    return TRUE;
}

static void closeTextProcess(
    TextProcess *textProcess
)
{
    if (textProcess->process == NULL) {
        return;
    }

    if (textProcess->window != NULL) {
        if (textProcess->geometryCaptured) {
            SetWindowPos(
                textProcess->window,
                NULL,
                textProcess->originalRect.left,
                textProcess->originalRect.top,
                textProcess->originalRect.right -
                    textProcess->originalRect.left,
                textProcess->originalRect.bottom -
                    textProcess->originalRect.top,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
        }

        PostMessageA(
            textProcess->window,
            WM_CLOSE,
            0,
            0
        );
    }

    if (WaitForSingleObject(
        textProcess->process,
        250
    ) == WAIT_TIMEOUT) {

        TerminateProcess(
            textProcess->process,
            0
        );
    }

    CloseHandle(textProcess->process);
    ZeroMemory(textProcess, sizeof(*textProcess));
}

static void reapTextProcesses(void)
{
    DWORD now = GetTickCount();

    for (int index = g_textProcessCount - 1;
         index >= 0;
         index--) {

        TextProcess *textProcess =
            &g_textProcesses[index];

        if (WaitForSingleObject(
            textProcess->process,
            0
        ) == WAIT_OBJECT_0 ||
            (textProcess->closeAt != 0 &&
             (LONG)(now - textProcess->closeAt) >= 0)) {

            closeTextProcess(textProcess);

            if (index < g_textProcessCount - 1) {
                memmove(
                    textProcess,
                    textProcess + 1,
                    (g_textProcessCount - index - 1) *
                    sizeof(*textProcess)
                );
            }

            g_textProcessCount--;
            writeSessionMemory("active");
        }
    }
}

static void closeAllTextProcesses(void)
{
    while (g_textProcessCount > 0) {
        closeTextProcess(&g_textProcesses[
            g_textProcessCount - 1
        ]);
        g_textProcessCount--;
    }
}

static void appendCharacter(
    wchar_t *buffer,
    size_t bufferSize,
    size_t *position,
    wchar_t character
)
{
    if (*position + 1 >= bufferSize) {
        return;
    }

    buffer[*position] = character;
    (*position)++;
    buffer[*position] = L'\0';
}

static void createGarbageText(
    wchar_t *buffer,
    size_t bufferSize,
    TextHost host
)
{
    static const wchar_t characters[] =
        L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        L"abcdefghijklmnopqrstuvwxyz"
        L"0123456789<>[]{}!@%^*_-+=|/\\";
    static const wchar_t symbols[] = {
        0x2591, 0x2592, 0x2593, 0x25A0, 0x25C6
    };
    size_t position = 0;

    buffer[0] = L'\0';

    if (host == TEXT_HOST_CMD) {
        const wchar_t prefix[] = L"REM ";

        for (size_t index = 0;
             prefix[index] != L'\0';
             index++) {

            appendCharacter(
                buffer,
                bufferSize,
                &position,
                prefix[index]
            );
        }
    }

    int length = 48 + nextGlitchRandom() % 65;

    for (int index = 0; index < length; index++) {
        wchar_t character;

        if (host == TEXT_HOST_NOTEPAD &&
            (nextGlitchRandom() % 7) == 0) {

            character = symbols[
                nextGlitchRandom() %
                (sizeof(symbols) / sizeof(symbols[0]))
            ];
        } else {
            character = characters[
                nextGlitchRandom() %
                (sizeof(characters) / sizeof(characters[0]) - 1)
            ];
        }

        appendCharacter(
            buffer,
            bufferSize,
            &position,
            character
        );
    }

    appendCharacter(buffer, bufferSize, &position, L'\r');
    appendCharacter(buffer, bufferSize, &position, L'\n');
}

static BOOL startTextProcess(
    TextHost host,
    const wchar_t *text
)
{
    if (g_textProcessCount >= ARG_MAX_TEXT_PROCESSES ||
        text == NULL) {

        return FALSE;
    }

    TextProcess *textProcess =
        &g_textProcesses[g_textProcessCount];
    STARTUPINFOW startupInfo;
    PROCESS_INFORMATION processInfo;
    wchar_t commandLine[256];

    ZeroMemory(textProcess, sizeof(*textProcess));
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    ZeroMemory(&processInfo, sizeof(processInfo));

    startupInfo.cb = sizeof(startupInfo);

    if (host == TEXT_HOST_CMD) {
        swprintf(
            commandLine,
            sizeof(commandLine) / sizeof(commandLine[0]),
            L"cmd.exe /k title ARG_TEXT_%lu",
            (unsigned long)GetCurrentProcessId()
        );
    } else {
        wcscpy(commandLine, L"notepad.exe");
    }

    if (!CreateProcessW(
        NULL,
        commandLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        NULL,
        &startupInfo,
        &processInfo
    )) {

        MessageBoxA(
            NULL,
            "Could not create an ARG text display.",
            "ARG information",
            MB_OK | MB_ICONINFORMATION
        );

        return FALSE;
    }

    CloseHandle(processInfo.hThread);
    textProcess->process = processInfo.hProcess;
    textProcess->processId = processInfo.dwProcessId;
    textProcess->host = host;
    textProcess->autoType = TRUE;
    textProcess->startedAt = GetTickCount();
    textProcess->nextCharacterAt = textProcess->startedAt + 500;
    wcsncpy(
        textProcess->text,
        text,
        ARG_TEXT_BUFFER_SIZE - 1
    );
    textProcess->text[ARG_TEXT_BUFFER_SIZE - 1] = L'\0';
    g_textProcessCount++;
    writeSessionMemory("active");

    return TRUE;
}

static BOOL startApplicationProcess(
    const wchar_t *commandLine,
    DWORD duration
)
{
    if (g_textProcessCount >= ARG_MAX_TEXT_PROCESSES ||
        commandLine == NULL) {

        return FALSE;
    }

    TextProcess *textProcess =
        &g_textProcesses[g_textProcessCount];
    STARTUPINFOW startupInfo;
    PROCESS_INFORMATION processInfo;
    wchar_t mutableCommandLine[512];

    ZeroMemory(textProcess, sizeof(*textProcess));
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    ZeroMemory(&processInfo, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);

    wcsncpy(
        mutableCommandLine,
        commandLine,
        sizeof(mutableCommandLine) /
        sizeof(mutableCommandLine[0]) - 1
    );
    mutableCommandLine[
        sizeof(mutableCommandLine) /
        sizeof(mutableCommandLine[0]) - 1
    ] = L'\0';

    if (!CreateProcessW(
        NULL,
        mutableCommandLine,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &startupInfo,
        &processInfo
    )) {

        return FALSE;
    }

    CloseHandle(processInfo.hThread);
    textProcess->process = processInfo.hProcess;
    textProcess->processId = processInfo.dwProcessId;
    textProcess->host = TEXT_HOST_APPLICATION;
    textProcess->autoType = FALSE;
    textProcess->startedAt = GetTickCount();
    textProcess->closeAt = textProcess->startedAt + duration;
    g_textProcessCount++;
    writeSessionMemory("active");

    return TRUE;
}

static void applySplitGeometry(
    TextProcess *textProcess,
    int side
)
{
    if (textProcess->window == NULL ||
        textProcess->geometryCaptured) {

        return;
    }

    if (!GetWindowRect(
        textProcess->window,
        &textProcess->originalRect
    )) {

        return;
    }

    int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN) / 2;
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN) / 2;

    SetWindowPos(
        textProcess->window,
        HWND_TOP,
        left + (side ? width : 0),
        top + (side ? height : 0),
        width,
        height,
        SWP_NOACTIVATE
    );

    textProcess->geometryCaptured = TRUE;
}

static void updateTextProcesses(void)
{
    DWORD now = GetTickCount();

    reapTextProcesses();

    for (int index = 0;
         index < g_textProcessCount;
         index++) {

        TextProcess *textProcess =
            &g_textProcesses[index];

        if (textProcess->window == NULL) {
            EnumWindows(
                findWindowForProcess,
                (LPARAM)textProcess
            );

            if (textProcess->window == NULL &&
                textProcess->host == TEXT_HOST_CMD) {

                wchar_t title[64];

                swprintf(
                    title,
                    sizeof(title) / sizeof(title[0]),
                    L"ARG_TEXT_%lu",
                    (unsigned long)GetCurrentProcessId()
                );

                textProcess->window = FindWindowW(NULL, title);
            }
        }

        if (textProcess->window == NULL &&
            !textProcess->errorReported &&
            (LONG)(now - textProcess->startedAt) >= 5000) {

            textProcess->errorReported = TRUE;
            textProcess->closeAt = now;

            MessageBoxA(
                NULL,
                "ARG could not identify its text window.",
                "ARG information",
                MB_OK | MB_ICONINFORMATION
            );
        }

        if (g_splitOverlayActive &&
            textProcess->window != NULL) {

            applySplitGeometry(
                textProcess,
                index % 2
            );
        }

        if (textProcess->window == NULL ||
            (LONG)(now - textProcess->nextCharacterAt) < 0 ||
            textProcess->closeAt != 0 ||
            !textProcess->autoType) {

            continue;
        }

        wchar_t character =
            textProcess->text[textProcess->position];

        if (character == L'\0') {
            textProcess->closeAt = now + 3000;
            continue;
        }

        if (character == L'\r') {
            textProcess->position++;
        } else if (character == L'\n') {
            PostMessageW(
                textProcess->window,
                WM_KEYDOWN,
                VK_RETURN,
                0
            );
            PostMessageW(
                textProcess->window,
                WM_KEYUP,
                VK_RETURN,
                0
            );
            textProcess->position++;
        } else {
            PostMessageW(
                textProcess->window,
                WM_CHAR,
                character,
                0
            );
            textProcess->position++;
        }

        textProcess->nextCharacterAt =
            now + 45 + nextGlitchRandom() % 96;
    }
}

static DWORD step7RandomWallpaperDelay(void)
{
    return 2000 + nextGlitchRandom() % 2001;
}

static void step7ShowWallpaperFrame(void)
{
    char relativePath[64];

    switch (g_step7.wallpaperMode) {
        case STEP7_WALLPAPER_HAPPY:
            snprintf(relativePath, sizeof(relativePath),
                     "img\\creep\\happy%u.png",
                     g_step7.wallpaperFrame % 9 + 1);
            break;

        case STEP7_WALLPAPER_HT:
            snprintf(relativePath, sizeof(relativePath),
                     "img\\creep\\ht%u.png",
                     g_step7.wallpaperFrame % 4 + 1);
            break;

        case STEP7_WALLPAPER_GREEN:
            snprintf(relativePath, sizeof(relativePath),
                     "img\\creep\\thisisgreen%u.webp",
                     g_step7.wallpaperFrame % 4 + 1);
            break;

        default:
            return;
    }

    setWallpaperFile(relativePath);
    playStaticOverlay();
}

static void step7StartWallpaperEvent(ULONGLONG now)
{
    size_t choice = nextGlitchRandom() %
        (sizeof(g_step7RegularWallpapers) /
         sizeof(g_step7RegularWallpapers[0]) + 3);

    if (choice < sizeof(g_step7RegularWallpapers) /
                 sizeof(g_step7RegularWallpapers[0])) {

        setWallpaperFile(g_step7RegularWallpapers[choice]);
        playStaticOverlay();
        g_step7.nextWallpaperAt = now + step7RandomWallpaperDelay();
        return;
    }

    g_step7.wallpaperFrame = 0;

    if (choice == sizeof(g_step7RegularWallpapers) /
                  sizeof(g_step7RegularWallpapers[0])) {

        g_step7.wallpaperMode = STEP7_WALLPAPER_HAPPY;
        g_step7.wallpaperFrameCount = 27;
        g_step7.wallpaperFrameInterval = 100;
    } else if (choice == sizeof(g_step7RegularWallpapers) /
                         sizeof(g_step7RegularWallpapers[0]) + 1) {

        g_step7.wallpaperMode = STEP7_WALLPAPER_HT;
        g_step7.wallpaperFrameCount = 4;
        g_step7.wallpaperFrameInterval = 500;
    } else {
        g_step7.wallpaperMode = STEP7_WALLPAPER_GREEN;
        g_step7.wallpaperFrameCount = 16;
        g_step7.wallpaperFrameInterval = 200;
    }

    step7ShowWallpaperFrame();
    g_step7.nextWallpaperAt = now + g_step7.wallpaperFrameInterval;
}

static void updateStep7Wallpapers(ULONGLONG now)
{
    if (now < g_step7.nextWallpaperAt) {
        return;
    }

    if (g_step7.wallpaperMode == STEP7_WALLPAPER_NONE) {
        step7StartWallpaperEvent(now);
        return;
    }

    g_step7.wallpaperFrame++;

    if (g_step7.wallpaperFrame >= g_step7.wallpaperFrameCount) {
        g_step7.wallpaperMode = STEP7_WALLPAPER_NONE;
        g_step7.nextWallpaperAt = now + step7RandomWallpaperDelay();
        return;
    }

    step7ShowWallpaperFrame();
    g_step7.nextWallpaperAt += g_step7.wallpaperFrameInterval;

    if (g_step7.nextWallpaperAt <= now) {
        g_step7.nextWallpaperAt = now + g_step7.wallpaperFrameInterval;
    }
}

static BOOL step7ReminderIsOpen(void)
{
    static const wchar_t reminder[] = L"Remember to smile :)\r\n";

    for (int index = 0; index < g_textProcessCount; index++) {
        if (wcscmp(g_textProcesses[index].text, reminder) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

static void updateStep7Reminder(ULONGLONG now)
{
    if (g_step7.reminderActive) {
        if (!step7ReminderIsOpen()) {
            g_step7.reminderActive = FALSE;
            g_step7.nextReminderAt = now + 7000;
        }
        return;
    }

    if (now >= g_step7.nextReminderAt &&
        startTextProcess(TEXT_HOST_NOTEPAD, L"Remember to smile :)\r\n")) {

        g_step7.reminderActive = TRUE;
    }
}

static void startStep7(ULONGLONG now)
{
    ZeroMemory(&g_step7, sizeof(g_step7));
    g_step7.nextWallpaperAt = now;
    g_step7.nextReminderAt = now;
}

static void startStep7Countdown(ULONGLONG now)
{
    g_step7.countdown = 5;
    g_step7.nextCountdownAt = now + 1000;
    setWallpaperFile("img\\countdown\\5.png");
    playStaticOverlayFor(100);
}

static BOOL updateStep7Countdown(ULONGLONG now)
{
    char relativePath[64];

    if (now < g_step7.nextCountdownAt) {
        return FALSE;
    }

    if (g_step7.countdown == 0) {
        return TRUE;
    }

    g_step7.countdown--;
    snprintf(relativePath, sizeof(relativePath), "img\\countdown\\%d.png",
             g_step7.countdown);
    setWallpaperFile(relativePath);
    playStaticOverlayFor(100);
    g_step7.nextCountdownAt += 1000;

    if (g_step7.nextCountdownAt <= now) {
        g_step7.nextCountdownAt = now + 1000;
    }

    return FALSE;
}

static BOOL loadPoems(void)
{
    char path[MAX_PATH * 2];
    FILE *file;
    char line[512];
    size_t position = 0;

    snprintf(
        path,
        sizeof(path),
        "%s\\text\\poems.txt",
        g_basePath
    );

    file = fopen(path, "r");

    if (file == NULL) {
        return FALSE;
    }

    g_poemCount = 0;
    g_poems[0][0] = '\0';

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strcmp(line, "\n") == 0 ||
            strcmp(line, "\r\n") == 0) {

            if (position > 0 &&
                g_poemCount + 1 < ARG_MAX_POEMS) {

                g_poems[g_poemCount][position] = '\0';
                g_poemCount++;
                position = 0;
                g_poems[g_poemCount][0] = '\0';
            }

            continue;
        }

        size_t lineLength = strlen(line);

        if (position + lineLength < ARG_TEXT_BUFFER_SIZE - 1) {
            memcpy(
                g_poems[g_poemCount] + position,
                line,
                lineLength
            );
            position += lineLength;
        }
    }

    if (position > 0 && g_poemCount < ARG_MAX_POEMS) {
        g_poems[g_poemCount][position] = '\0';
        g_poemCount++;
    }

    fclose(file);

    return g_poemCount >= ARG_RESERVED_POEMS;
}

static void reservePoems(void)
{
    int order[ARG_MAX_POEMS];

    for (int index = 0; index < g_poemCount; index++) {
        order[index] = index;
    }

    for (int index = g_poemCount - 1; index > 0; index--) {
        int swapIndex = nextGlitchRandom() % (index + 1);
        int value = order[index];

        order[index] = order[swapIndex];
        order[swapIndex] = value;
    }

    for (int index = 0;
         index < ARG_RESERVED_POEMS;
         index++) {

        g_reservedPoems[index] = order[index];
        g_ttsPoems[index] = order[
            index + ARG_RESERVED_POEMS
        ];
    }
}

static DWORD WINAPI ttsThread(
    LPVOID parameter
)
{
    (void)parameter;

    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) {
        g_ttsComplete = TRUE;
        return 0;
    }

    ISpVoice *voice = NULL;

    if (FAILED(CoCreateInstance(
        &CLSID_SpVoice,
        NULL,
        CLSCTX_ALL,
        &IID_ISpVoice,
        (void **)&voice
    ))) {

        CoUninitialize();
        g_ttsComplete = TRUE;
        return 0;
    }

    EnterCriticalSection(&g_ttsLock);
    g_ttsVoice = voice;
    LeaveCriticalSection(&g_ttsLock);

    for (int index = 0;
         index < ARG_RESERVED_POEMS &&
         !g_shutdownRequested;
         index++) {

        const char *poem = g_poems[g_ttsPoems[index]];
        int length = MultiByteToWideChar(
            CP_UTF8,
            0,
            poem,
            -1,
            NULL,
            0
        );

        if (length <= 0) {
            continue;
        }

        wchar_t *widePoem = calloc(
            (size_t)length,
            sizeof(*widePoem)
        );

        if (widePoem == NULL) {
            continue;
        }

        MultiByteToWideChar(
            CP_UTF8,
            0,
            poem,
            -1,
            widePoem,
            length
        );

        voice->lpVtbl->Speak(
            voice,
            widePoem,
            SPF_ASYNC,
            NULL
        );

        while (!g_shutdownRequested &&
               voice->lpVtbl->WaitUntilDone(
                   voice,
                   100
               ) == S_FALSE) {
        }

        free(widePoem);
    }

    EnterCriticalSection(&g_ttsLock);
    g_ttsVoice = NULL;
    LeaveCriticalSection(&g_ttsLock);

    voice->lpVtbl->Release(voice);
    CoUninitialize();
    g_ttsComplete = TRUE;

    return 0;
}

static void startTts(void)
{
    if (g_ttsStarted) {
        return;
    }

    if (g_poemCount < ARG_MAX_POEMS) {
        g_ttsComplete = TRUE;
        return;
    }

    g_ttsThread = CreateThread(
        NULL,
        0,
        ttsThread,
        NULL,
        0,
        NULL
    );

    if (g_ttsThread != NULL) {
        g_ttsStarted = TRUE;
    }
}

static void stopTts(void)
{
    EnterCriticalSection(&g_ttsLock);

    if (g_ttsVoice != NULL) {
        g_ttsVoice->lpVtbl->Speak(
            g_ttsVoice,
            NULL,
            SPF_PURGEBEFORESPEAK,
            NULL
        );
    }

    LeaveCriticalSection(&g_ttsLock);
}

static DWORD WINAPI step6SpeechThread(
    LPVOID parameter
)
{
    (void)parameter;

    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) {
        MessageBoxA(NULL, "You can't smile there, try again.",
                    "ARG information", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    ISpVoice *voice = NULL;

    if (FAILED(CoCreateInstance(&CLSID_SpVoice, NULL, CLSCTX_ALL,
                                &IID_ISpVoice, (void **)&voice))) {
        MessageBoxA(NULL, "You can't smile there, try again.",
                    "ARG information", MB_OK | MB_ICONINFORMATION);
    } else {
        voice->lpVtbl->Speak(voice,
                              L"You can't smile there, try again.",
                              SPF_DEFAULT, NULL);
        voice->lpVtbl->Release(voice);
    }

    CoUninitialize();
    return 0;
}

static void sayStep6Warning(void)
{
    HANDLE thread = CreateThread(NULL, 0, step6SpeechThread,
                                 NULL, 0, NULL);

    if (thread != NULL) {
        CloseHandle(thread);
    }
}

static void startRandomTextWindow(void)
{
    wchar_t text[ARG_TEXT_BUFFER_SIZE];
    TextHost host =
        (nextGlitchRandom() % 2) == 0
            ? TEXT_HOST_CMD
            : TEXT_HOST_NOTEPAD;

    createGarbageText(text, sizeof(text) / sizeof(text[0]), host);
    startTextProcess(host, text);
}

static void startPoemWindow(void)
{
    if (g_nextPoem >= ARG_RESERVED_POEMS) {
        return;
    }

    wchar_t text[ARG_TEXT_BUFFER_SIZE];
    TextHost host =
        (nextGlitchRandom() % 2) == 0
            ? TEXT_HOST_CMD
            : TEXT_HOST_NOTEPAD;
    const char *poem = g_poems[
        g_reservedPoems[g_nextPoem]
    ];
    size_t position = 0;

    text[0] = L'\0';

    for (size_t index = 0;
         poem[index] != '\0' &&
         position + 5 < ARG_TEXT_BUFFER_SIZE;
         index++) {

        if ((index == 0 || poem[index - 1] == '\n') &&
            host == TEXT_HOST_CMD) {

            appendCharacter(text, ARG_TEXT_BUFFER_SIZE, &position, L'R');
            appendCharacter(text, ARG_TEXT_BUFFER_SIZE, &position, L'E');
            appendCharacter(text, ARG_TEXT_BUFFER_SIZE, &position, L'M');
            appendCharacter(text, ARG_TEXT_BUFFER_SIZE, &position, L' ');
        }

        appendCharacter(
            text,
            ARG_TEXT_BUFFER_SIZE,
            &position,
            (unsigned char)poem[index]
        );
    }

    if (startTextProcess(host, text)) {
        g_nextPoem++;
    }
}

static void startStep5Window(
    DWORD duration
)
{
    if (!g_appLaunchAllowed ||
        g_textProcessCount >= ARG_MAX_TEXT_PROCESSES) {

        return;
    }

    static const wchar_t *applications[] = {
        L"calc.exe",
        L"mspaint.exe",
        L"notepad.exe"
    };
    unsigned int choice = nextGlitchRandom() %
        (sizeof(applications) / sizeof(applications[0]) +
         (g_browserAllowed ? 1 : 0) +
         (g_localInfoAllowed ? 1 : 0));

    if (choice < sizeof(applications) / sizeof(applications[0])) {
        startApplicationProcess(applications[choice], duration);
    } else if (g_browserAllowed &&
               choice == sizeof(applications) /
                         sizeof(applications[0])) {

        startApplicationProcess(
            L"msedge.exe --inprivate about:blank",
            duration
        );
    } else if (g_localInfoAllowed) {
        static const wchar_t *commands[] = {
            L"whoami\r\n",
            L"hostname\r\n",
            L"ipconfig | findstr /R /C:\"IPv4\"\r\n",
            L"getmac /FO LIST /NH\r\n"
        };
        size_t command = nextGlitchRandom() %
            (sizeof(commands) / sizeof(commands[0]));

        startTextProcess(TEXT_HOST_CMD, commands[command]);
    }
}

static void startSplitOverlay(void)
{
    g_splitOverlayActive = TRUE;

    SetLayeredWindowAttributes(
        g_overlay,
        0,
        48,
        LWA_ALPHA
    );

    InvalidateRect(g_overlay, NULL, FALSE);
}

static void stopSplitOverlay(void)
{
    g_splitOverlayActive = FALSE;

    if (g_glitchStyle == 0 && g_overlay != NULL) {
        SetLayeredWindowAttributes(
            g_overlay,
            0,
            0,
            LWA_ALPHA
        );
    }
}

static void stopStep6(void)
{
    g_step6Active = FALSE;

    if (g_smileRegion != NULL) {
        DeleteObject(g_smileRegion);
        g_smileRegion = NULL;
    }

    if (g_smileFont != NULL) {
        DeleteObject(g_smileFont);
        g_smileFont = NULL;
    }

    if (g_overlay != NULL) {
        SetLayeredWindowAttributes(
            g_overlay,
            0,
            0,
            LWA_ALPHA
        );
    }
}

static void startStep6(void)
{
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    HDC screen = GetDC(NULL);

    if (screen == NULL || g_overlay == NULL) {
        return;
    }

    g_smileFont = CreateFontA(
        height / 2,
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        FF_DONTCARE,
        "Arial"
    );

    if (g_smileFont == NULL) {
        ReleaseDC(NULL, screen);
        return;
    }

    HFONT previous = SelectObject(screen, g_smileFont);
    SIZE size;
    GetTextExtentPoint32A(screen, ":)", 2, &size);
    int y = (height - size.cy) / 2;
    int leftX = width / 4 - size.cx / 2;
    int rightX = width * 3 / 4 - size.cx / 2;

    BeginPath(screen);
    TextOutA(screen, leftX, y, ":)", 2);
    EndPath(screen);
    g_smileRegion = PathToRegion(screen);

    BeginPath(screen);
    TextOutA(screen, rightX, y, ":)", 2);
    EndPath(screen);
    HRGN rightRegion = PathToRegion(screen);

    if (g_smileRegion != NULL && rightRegion != NULL) {
        CombineRgn(g_smileRegion, g_smileRegion, rightRegion, RGN_OR);
    }

    if (rightRegion != NULL) {
        DeleteObject(rightRegion);
    }

    SelectObject(screen, previous);
    ReleaseDC(NULL, screen);

    g_step6BlockedClicks = 0;
    g_step6Active = g_smileRegion != NULL;

    SetLayeredWindowAttributes(
        g_overlay,
        RGB(255, 0, 255),
        255,
        LWA_COLORKEY
    );
    InvalidateRect(g_overlay, NULL, FALSE);
}

static void paintGlitch(
    HDC deviceContext,
    const RECT *clientRect
)
{
    if (g_staticVisible) {
        paintStaticVisual(deviceContext, clientRect);
        return;
    }

    if (g_step6Active) {
        HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
        HFONT previous = SelectObject(deviceContext, g_smileFont);
        SIZE size;

        FillRect(deviceContext, clientRect, black);
        DeleteObject(black);
        GetTextExtentPoint32A(deviceContext, ":)", 2, &size);
        SetTextColor(deviceContext, RGB(255, 0, 255));
        SetBkMode(deviceContext, TRANSPARENT);
        TextOutA(deviceContext,
                 clientRect->right / 4 - size.cx / 2,
                 (clientRect->bottom - size.cy) / 2,
                 ":)", 2);
        TextOutA(deviceContext,
                 clientRect->right * 3 / 4 - size.cx / 2,
                 (clientRect->bottom - size.cy) / 2,
                 ":)", 2);
        SelectObject(deviceContext, previous);
        return;
    }

    if (g_transformDC != NULL) {
        int width = clientRect->right - clientRect->left;
        int height = clientRect->bottom - clientRect->top;

        if (g_glitchStyle == 5) {
            BitBlt(deviceContext, 0, 0, width, height, g_transformDC, 0, 0,
                   SRCCOPY);
            PatBlt(deviceContext, 0, 0, width, height, DSTINVERT);
        } else if (g_glitchStyle == 6) {
            POINT points[] = {{width, height}, {0, height}, {width, 0}};
            PlgBlt(deviceContext, points, g_transformDC, 0, 0,
                   g_transformWidth, g_transformHeight, NULL, 0, 0);
        } else {
            POINT points[] = {{width, 0}, {width, height}, {0, 0}};
            PlgBlt(deviceContext, points, g_transformDC, 0, 0,
                   g_transformWidth, g_transformHeight, NULL, 0, 0);
        }

        return;
    }

    if (deviceContext == NULL ||
        clientRect == NULL ||
        (g_glitchStyle == 0 &&
         !g_splitOverlayActive)) {
        return;
    }

    int width = clientRect->right - clientRect->left;
    int height = clientRect->bottom - clientRect->top;

    if (width <= 0 || height <= 0) {
        return;
    }

    if (g_splitOverlayActive) {
        RECT vertical = {
            width / 2 - 6,
            0,
            width / 2 + 6,
            height
        };
        RECT horizontal = {
            0,
            height / 2 - 6,
            width,
            height / 2 + 6
        };
        HBRUSH divider = CreateSolidBrush(RGB(255, 255, 255));

        if (divider != NULL) {
            FillRect(deviceContext, &vertical, divider);
            FillRect(deviceContext, &horizontal, divider);
            DeleteObject(divider);
        }
    }

    if (g_glitchStyle == 0) {
        return;
    }

    for (int index = 0; index < 16; index++) {
        RECT block;

        block.left = nextGlitchRandom() % width;
        block.top = nextGlitchRandom() % height;
        block.right = block.left +
            30 + nextGlitchRandom() % 220;
        block.bottom = block.top +
            12 + nextGlitchRandom() % 100;

        if (block.right > width) {
            block.right = width;
        }

        if (block.bottom > height) {
            block.bottom = height;
        }

        COLORREF color = RGB(0, 0, 0);

        if (g_glitchStyle == 2) {
            color = RGB(
                96 + nextGlitchRandom() % 160,
                nextGlitchRandom() % 96,
                96 + nextGlitchRandom() % 160
            );
        } else if (g_glitchStyle == 3) {
            int noise = nextGlitchRandom() % 256;

            color = RGB(noise, noise, noise);
        } else if (g_glitchStyle == 4) {
            color = RGB(255, 255, 255);
        }

        HBRUSH brush = CreateSolidBrush(color);

        if (brush != NULL) {
            FillRect(
                deviceContext,
                &block,
                brush
            );

            DeleteObject(brush);
        }
    }
}

static LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (message) {

        case WM_NCHITTEST:
            if (g_step6Active) {
                POINT point = {
                    (short)LOWORD(lParam),
                    (short)HIWORD(lParam)
                };

                ScreenToClient(hwnd, &point);

                return PtInRegion(
                    g_smileRegion,
                    point.x,
                    point.y
                ) ? HTTRANSPARENT : HTCLIENT;
            }

            return HTTRANSPARENT;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(
                hwnd,
                &paint
            );

            paintGlitch(
                deviceContext,
                &paint.rcPaint
            );

            EndPaint(
                hwnd,
                &paint
            );

            return 0;
        }

        case WM_TIMER:

            if (wParam == ARG_STATIC_TIMER_ID && g_staticVisible) {
                DWORD now = GetTickCount();

                if ((LONG)(now - g_staticEndsAt) >= 0) {
                    stopStaticVisual();
                } else if (g_staticImage != NULL) {
                    GdipImageRotateFlip((GpImage *)g_staticImage,
                                        Rotate180FlipNone);
                    InvalidateRect(hwnd, NULL, FALSE);
                }

                return 0;
            }

            if (wParam == ARG_GLITCH_TIMER_ID &&
                g_glitchesEnabled &&
                !g_shutdownRequested) {

                DWORD now = GetTickCount();

                if (g_glitchStyle != 0 &&
                    (LONG)(now - g_glitchEndsAt) >= 0) {

                    g_glitchStyle = 0;
                    releaseTransformScreen();

                    SetLayeredWindowAttributes(
                        hwnd,
                        0,
                        0,
                        LWA_ALPHA
                    );
                } else if (g_glitchStyle == 0 &&
                           (nextGlitchRandom() % 14) == 0) {

                    g_glitchStyle = 1 + nextGlitchRandom() % 7;
                    g_glitchEndsAt =
                        now + 800 +
                        nextGlitchRandom() % 2201;

                    if (g_glitchStyle >= 5 && !captureTransformScreen()) {
                        g_glitchStyle = 1;
                    }

                    SetLayeredWindowAttributes(hwnd, 0,
                                               g_glitchStyle >= 5 ? 255 : 72,
                                               LWA_ALPHA);
                }

                InvalidateRect(
                    hwnd,
                    NULL,
                    FALSE
                );
            }

            return 0;

        case WM_HOTKEY:

            if (wParam ==
                ARG_HOTKEY_ID) {

                g_shutdownRequested =
                    TRUE;

                PostMessageA(
                    hwnd,
                    WM_CLOSE,
                    0,
                    0
                );
            }

            return 0;

        case WM_LBUTTONDOWN:
            if (g_step6Active) {
                g_step6BlockedClicks++;
                sayStep6Warning();

                if (g_step6BlockedClicks >= 7) {
                    stopStep6();
                    g_step6Complete = TRUE;
                }
            }

            return 0;

        case WM_CLOSE:

            g_shutdownRequested =
                TRUE;
            g_audioThreadStop = TRUE;

            stopGlitchEffects();
            stopSplitOverlay();
            stopStep6();
            stopMP3();
            stopStaticOverlay();
            releaseStaticImage();
            removeInstalledTreeStartup();
            restoreWallpaper();
            stopMouseInversion();
            stopKeyboardBlock();
            stopTts();
            closeAllTextProcesses();

            DestroyWindow(hwnd);

            return 0;

        case WM_DESTROY:

            UnregisterHotKey(
                NULL,
                ARG_HOTKEY_ID
            );

            PostQuitMessage(0);

            return 0;
    }

    return DefWindowProcA(
        hwnd,
        message,
        wParam,
        lParam
    );
}

static BOOL createOverlay(
    HINSTANCE hInstance
)
{
    WNDCLASSEXA wc;

    ZeroMemory(
        &wc,
        sizeof(wc)
    );

    wc.cbSize =
        sizeof(wc);

    wc.lpfnWndProc =
        WindowProc;

    wc.hInstance =
        hInstance;

    wc.lpszClassName =
        ARG_WINDOW_CLASS;

    if (!RegisterClassExA(
        &wc
    )) {
        return FALSE;
    }

    int width =
        GetSystemMetrics(
            SM_CXSCREEN
        );

    int height =
        GetSystemMetrics(
            SM_CYSCREEN
        );

    DWORD extendedStyle =
        WS_EX_LAYERED |
        WS_EX_TOOLWINDOW |
        WS_EX_TRANSPARENT |
        WS_EX_NOACTIVATE;

    g_overlay =
        CreateWindowExA(
            extendedStyle,
            ARG_WINDOW_CLASS,
            "ARG",
            WS_POPUP,
            0,
            0,
            width,
            height,
            NULL,
            NULL,
            hInstance,
            NULL
        );

    if (g_overlay == NULL) {
        return FALSE;
    }

    SetLayeredWindowAttributes(
        g_overlay,
        0,
        0,
        LWA_ALPHA
    );

    SetWindowPos(
        g_overlay,
        HWND_TOPMOST,
        0,
        0,
        width,
        height,
        SWP_NOACTIVATE |
        SWP_SHOWWINDOW
    );

    return TRUE;
}

/*
 * ------------------------------------------------------------
 * Main
 * ------------------------------------------------------------
 */

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
)
{
    (void)hPrevInstance;
    (void)nCmdShow;

    if (!getBasePath()) {
        return 1;
    }

    (void)lpCmdLine;

    InitializeCriticalSection(&g_ttsLock);

    if (!acceptTerms()) {
        DeleteCriticalSection(&g_ttsLock);
        return 0;
    }

    writeSessionMemory("active");

    srand(
        ((unsigned int)time(NULL)) ^
        ((unsigned int)GetCurrentProcessId())
    );

    if (!createOverlay(
        hInstance
    )) {
        writeSessionMemory("cleaned");
        DeleteCriticalSection(&g_ttsLock);
        return 1;
    }

    /*
     * F10 is global to this process.
     */
    if (!RegisterHotKey(
        NULL,
        ARG_HOTKEY_ID,
        0,
        VK_F10
    )) {

        MessageBoxA(
            NULL,
            "Could not register F10 shutdown hotkey.",
            "ARG",
            MB_OK |
            MB_ICONERROR
        );

        DestroyWindow(
            g_overlay
        );

        writeSessionMemory("cleaned");
        DeleteCriticalSection(&g_ttsLock);

        return 1;
    }

    /*
     * Start audio events.
     */
    HANDLE audio =
        CreateThread(
            NULL,
            0,
            audioThread,
            NULL,
            0,
            NULL
        );

    if (audio != NULL) {
        CloseHandle(audio);
    }

    enum {
        STEP_1,
        STEP_2,
        STEP_3A,
        STEP_3B,
        STEP_4,
        STEP_5,
        STEP_5_COMPLETE,
        STEP_6,
        STEP_7_PENDING,
        STEP_7_PLAYING,
        STEP_7_COUNTDOWN
    } phase = STEP_1;
    ULONGLONG phaseEndsAt = GetTickCount64() + 60000;
    ULONGLONG nextTextAt = 0;

    if (!loadPoems()) {
        MessageBoxA(
            NULL,
            "Could not load eight poem blocks from text\\poems.txt.",
            "ARG information",
            MB_OK | MB_ICONINFORMATION
        );
    } else {
        reservePoems();
    }

    while (!g_shutdownRequested) {
        MSG message;

        while (
            PeekMessageA(
                &message,
                NULL,
                0,
                0,
                PM_REMOVE
            )
        ) {
            if (message.message ==
                WM_QUIT) {

                g_shutdownRequested =
                    TRUE;

                break;
            }

            TranslateMessage(
                &message
            );

            DispatchMessageA(
                &message
            );
        }

        updateTextProcesses();

        ULONGLONG now = GetTickCount64();

        if (phase == STEP_1 && now >= phaseEndsAt) {
            phase = STEP_2;
            phaseEndsAt = now +
                60000 + nextGlitchRandom() % 60001;
            startGlitchEffects();
        } else if (phase == STEP_2 && now >= phaseEndsAt) {
            phase = STEP_3A;
            phaseEndsAt = now +
                60000 + nextGlitchRandom() % 60001;
            nextTextAt = now;
        } else if (phase == STEP_3A) {
            if (now >= phaseEndsAt) {
                closeAllTextProcesses();
                phase = STEP_3B;
                nextTextAt = now;
            } else if (now >= nextTextAt &&
                       g_textProcessCount <
                       ARG_MAX_TEXT_PROCESSES) {

                startRandomTextWindow();
                nextTextAt = now +
                    3000 + nextGlitchRandom() % 5001;
            }
        } else if (phase == STEP_3B &&
                   g_textProcessCount == 0) {

            if (g_nextPoem >= ARG_RESERVED_POEMS) {
                phase = STEP_4;
                g_nextMouseInversionAt =
                    GetTickCount() + 3000 +
                    nextGlitchRandom() % 7001;
                g_nextKeyboardBlockAt =
                    GetTickCount() + 4000 +
                    nextGlitchRandom() % 7001;
                startTts();
            } else if (now >= nextTextAt) {
                startPoemWindow();
                nextTextAt = now + 500;
            }
        } else if (phase == STEP_4) {

            DWORD currentTime = GetTickCount();

            updateKeyboardBlock(currentTime);

            if (g_mouseInversionAllowed && g_mouseInversionActive &&
                (LONG)(currentTime - g_mouseInversionEndsAt) >= 0) {

                stopMouseInversion();
                g_nextMouseInversionAt =
                    currentTime + 4000 +
                    nextGlitchRandom() % 10001;
            } else if (g_mouseInversionAllowed && !g_mouseInversionActive &&
                       (LONG)(currentTime -
                               g_nextMouseInversionAt) >= 0) {

                startMouseInversion(
                    1200 + nextGlitchRandom() % 2801
                );
            }
        }

        if (phase == STEP_4 && g_ttsComplete) {
            phase = STEP_5;
            phaseEndsAt = now +
                30000 + nextGlitchRandom() % 15001;
            nextTextAt = now;
            startSplitOverlay();
        } else if (phase == STEP_5) {
            DWORD currentTime = GetTickCount();

            updateKeyboardBlock(currentTime);

            if (g_mouseInversionAllowed) {
                if (g_mouseInversionActive &&
                    (LONG)(currentTime -
                           g_mouseInversionEndsAt) >= 0) {

                    stopMouseInversion();
                    g_nextMouseInversionAt =
                        currentTime + 4000 +
                        nextGlitchRandom() % 10001;
                } else if (!g_mouseInversionActive &&
                           (LONG)(currentTime -
                                   g_nextMouseInversionAt) >= 0) {

                    startMouseInversion(
                        1200 + nextGlitchRandom() % 2801
                    );
                }
            }

            if (now >= phaseEndsAt) {
                closeAllTextProcesses();
                stopSplitOverlay();
                stopMouseInversion();
                stopKeyboardBlock();
                stopGlitchEffects();
                phase = STEP_5_COMPLETE;
            } else if (g_textProcessCount <
                       ARG_MAX_TEXT_PROCESSES &&
                       now >= nextTextAt) {

                startStep5Window(
                    8000 + nextGlitchRandom() % 9001
                );
                nextTextAt = now +
                    1200 + nextGlitchRandom() % 2201;
            }
        } else if (phase == STEP_5_COMPLETE) {
            startStep6();
            phase = STEP_6;
        } else if (phase == STEP_6 && g_step6Complete) {
            phase = STEP_7_PENDING;
        } else if (phase == STEP_7_PENDING) {
            char musicPath[MAX_PATH * 2];

            g_audioThreadStop = TRUE;
            PlaySoundA(NULL, NULL, 0);
            if (extractTreeExecutable() && installTreeStartup()) {
                g_treeStartupInstalled = TRUE;
            }
            snprintf(musicPath, sizeof(musicPath),
                     "%s\\mus\\undoitall.mp3", g_basePath);
            playMP3(musicPath, 600);
            startStep7(now);
            phaseEndsAt = now + 227000;
            phase = STEP_7_PLAYING;
        } else if (phase == STEP_7_PLAYING) {
            if (now >= phaseEndsAt) {
                stopMP3();
                closeAllTextProcesses();
                startStep7Countdown(now);
                phase = STEP_7_COUNTDOWN;
            } else {
                updateStep7Wallpapers(now);
                updateStep7Reminder(now);
            }
        } else if (phase == STEP_7_COUNTDOWN) {
            if (updateStep7Countdown(now)) {
                BSOD();
            }
        }

        MsgWaitForMultipleObjectsEx(0, NULL, 50, QS_ALLINPUT,
                                    MWMO_INPUTAVAILABLE);
    }

    g_audioThreadStop = TRUE;
    stopMP3();
    stopStaticOverlay();
    releaseStaticImage();
    stopSplitOverlay();
    stopStep6();
    restoreWallpaper();
    stopMouseInversion();
    stopKeyboardBlock();
    stopTts();

    PlaySoundA(
        NULL,
        NULL,
        0
    );

    UnregisterHotKey(
        NULL,
        ARG_HOTKEY_ID
    );

    if (g_overlay != NULL) {
        DestroyWindow(
            g_overlay
        );

        g_overlay = NULL;
    }

    closeAllTextProcesses();

    if (g_ttsThread != NULL) {
        WaitForSingleObject(g_ttsThread, 2000);
        CloseHandle(g_ttsThread);
        g_ttsThread = NULL;
    }

    writeSessionMemory("cleaned");
    DeleteCriticalSection(&g_ttsLock);

    return 0;
}
