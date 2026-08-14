#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "state.h"

#pragma comment(lib, "winmm.lib")

#define ARG_PREFIX "ARG_"
#define ARG_WINDOW_CLASS "ARG_Overlay"
#define ARG_HOTKEY_ID 1001

static HWND g_overlay = NULL;

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
        !g_shutdownRequested
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

    return !g_shutdownRequested;
}

static void playStatic(void)
{
    char path[MAX_PATH];

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

    sleepInterruptible(
        duration
    );

    stopMP3();
}

static void playWhisper(void)
{
    char path[MAX_PATH];

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

    stopMP3();
}

/*
 * ------------------------------------------------------------
 * Generated machine whirr
 * ------------------------------------------------------------
 */

static void generateMachineWhirr(void)
{
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

    while (!g_shutdownRequested) {

        int delay =
            5000 +
            rand() % 15001;

        if (!sleepInterruptible(
            delay
        )) {
            break;
        }

        if (g_shutdownRequested) {
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
 * State
 * ------------------------------------------------------------
 */

static ARGState createInitialState(void)
{
    ARGState state;

    ZeroMemory(
        &state,
        sizeof(state)
    );

    state.magic =
        ARG_STATE_MAGIC;

    state.stage = 0;
    state.audioEvent = 0;
    state.glitchCount = 0;
    state.interactionCount = 0;

    state.seed =
        ((unsigned int)time(NULL)) ^
        ((unsigned int)GetCurrentProcessId());

    state.shutdownRequested = 0;

    return state;
}

static BOOL parseState(
    const char *commandLine,
    ARGState *state
)
{
    if (commandLine == NULL ||
        state == NULL) {
        return FALSE;
    }

    unsigned int magic;
    int stage;
    int audioEvent;
    int glitchCount;
    int interactionCount;
    unsigned int seed;
    int shutdownRequested;

    int count =
        sscanf(
            commandLine,
            "%u %d %d %d %d %u %d",
            &magic,
            &stage,
            &audioEvent,
            &glitchCount,
            &interactionCount,
            &seed,
            &shutdownRequested
        );

    if (count != 7) {
        return FALSE;
    }

    if (magic != ARG_STATE_MAGIC) {
        return FALSE;
    }

    state->magic = magic;
    state->stage = stage;
    state->audioEvent = audioEvent;
    state->glitchCount = glitchCount;
    state->interactionCount =
        interactionCount;
    state->seed = seed;
    state->shutdownRequested =
        shutdownRequested;

    return TRUE;
}

/*
 * ------------------------------------------------------------
 * Worker creation
 * ------------------------------------------------------------
 */

static BOOL generateWorkerPath(
    char *path,
    size_t pathSize
)
{
    unsigned int value =
        ((unsigned int)GetTickCount()) ^
        ((unsigned int)GetCurrentProcessId()) ^
        ((unsigned int)time(NULL));

    unsigned int randomValue =
        value;

    randomValue ^= randomValue << 13;
    randomValue ^= randomValue >> 17;
    randomValue ^= randomValue << 5;

    snprintf(
        path,
        pathSize,
        "%s\\" ARG_PREFIX "%08X.exe",
        g_basePath,
        randomValue
    );

    return TRUE;
}

static BOOL spawnNextWorker(
    ARGState *state
)
{
    if (state == NULL) {
        return FALSE;
    }

    if (g_shutdownRequested) {
        return FALSE;
    }

    state->stage++;

    char destination[MAX_PATH];

    if (!generateWorkerPath(
        destination,
        sizeof(destination)
    )) {
        return FALSE;
    }

    /*
     * Copy the currently running worker.
     */
    char currentExecutable[MAX_PATH];

    DWORD length =
        GetModuleFileNameA(
            NULL,
            currentExecutable,
            sizeof(currentExecutable)
        );

    if (length == 0 ||
        length >= sizeof(currentExecutable)) {
        return FALSE;
    }

    if (!CopyFileA(
        currentExecutable,
        destination,
        TRUE
    )) {
        return FALSE;
    }

    char commandLine[1024];

    snprintf(
        commandLine,
        sizeof(commandLine),
        "\"%s\" %u %d %d %d %d %u %d",
        destination,
        state->magic,
        state->stage,
        state->audioEvent,
        state->glitchCount,
        state->interactionCount,
        state->seed,
        state->shutdownRequested
    );

    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION processInfo;

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

    /*
     * No console window.
     */
    BOOL created =
        CreateProcessA(
            destination,
            commandLine,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            g_basePath,
            &startupInfo,
            &processInfo
        );

    if (!created) {

        DeleteFileA(
            destination
        );

        return FALSE;
    }

    CloseHandle(
        processInfo.hThread
    );

    CloseHandle(
        processInfo.hProcess
    );

    return TRUE;
}

/*
 * ------------------------------------------------------------
 * Overlay
 * ------------------------------------------------------------
 */

static LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (message) {

        case WM_NCHITTEST:
            return HTTRANSPARENT;

        case WM_ERASEBKGND:
            return 1;

        case WM_HOTKEY:

            if (wParam ==
                ARG_HOTKEY_ID) {

                int result =
                    MessageBoxA(
                        NULL,
                        "Do you want to close the ARG?",
                        "ARG",
                        MB_YESNO |
                        MB_ICONQUESTION |
                        MB_SYSTEMMODAL
                    );

                if (result ==
                    IDYES) {

                    g_shutdownRequested =
                        TRUE;

                    PostMessageA(
                        hwnd,
                        WM_CLOSE,
                        0,
                        0
                    );
                }
            }

            return 0;

        case WM_CLOSE:

            g_shutdownRequested =
                TRUE;

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

    ARGState state;

    if (lpCmdLine != NULL &&
        lpCmdLine[0] != '\0') {

        if (!parseState(
            lpCmdLine,
            &state
        )) {
            state =
                createInitialState();
        }

    } else {
        state =
            createInitialState();
    }

    srand(
        state.seed ^
        (unsigned int)GetCurrentProcessId()
    );

    /*
     * If a shutdown was already requested,
     * never continue the chain.
     */
    if (state.shutdownRequested) {
        return 0;
    }

    if (!createOverlay(
        hInstance
    )) {
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

    /*
     * Example handoff timing.
     *
     * Replace this later with the actual ARG
     * event system.
     */
    BOOL handoffTimerExpired =
        FALSE;

    DWORD elapsed = 0;

    while (
        !g_shutdownRequested &&
        elapsed < 100
    ) {
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

        Sleep(50);

        elapsed += 50;
    }

    if (!g_shutdownRequested) {
        handoffTimerExpired = TRUE;
    }

    /*
     * Stop everything before handoff.
     */
    stopMP3();

    if (handoffTimerExpired &&
        !g_shutdownRequested) {

        /*
         * This is the only point at which
         * another worker is created.
         */
        if (spawnNextWorker(
            &state
        )) {
            /*
             * New worker now owns the state.
             */
            g_shutdownRequested =
                TRUE;
        }
    }

    stopMP3();

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

    /*
     * A worker that successfully handed off
     * simply exits.
     */
    return 0;
}