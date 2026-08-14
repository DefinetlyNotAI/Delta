#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#include "state.h"

#pragma comment(lib, "winmm.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char CLASS_NAME[] =
    "ARGOverlayWindow";

static HWND g_hwnd = NULL;

static char g_basePath[MAX_PATH];

/*
 * ------------------------------------------------------------
 * Path handling
 * ------------------------------------------------------------
 */

static void getBasePath(void) {
    GetModuleFileNameA(
        NULL,
        g_basePath,
        sizeof(g_basePath)
    );

    char *slash =
        strrchr(g_basePath, '\\');

    if (slash != NULL) {
        *slash = '\0';
    }
}

/*
 * ------------------------------------------------------------
 * MP3 playback
 * ------------------------------------------------------------
 */

static void stopMP3(void) {
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
) {
    char command[1024];

    stopMP3();

    snprintf(
        command,
        sizeof(command),
        "open \"%s\" type mpegvideo alias ARG_AUDIO",
        path
    );

    if (
        mciSendStringA(
            command,
            NULL,
            0,
            NULL
        ) != 0
    ) {
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

    if (
        mciSendStringA(
            "play ARG_AUDIO",
            NULL,
            0,
            NULL
        ) != 0
    ) {
        stopMP3();
        return FALSE;
    }

    return TRUE;
}

/*
 * ------------------------------------------------------------
 * Static
 * ------------------------------------------------------------
 */

static void playStatic(void) {
    char path[MAX_PATH];

    snprintf(
        path,
        sizeof(path),
        "%s\\mus\\static.mp3",
        g_basePath
    );

    if (!playMP3(path, 450)) {
        return;
    }

    /*
     * 300 ms to 1000 ms.
     */
    int duration =
        300 + rand() % 701;

    Sleep(duration);

    stopMP3();
}

/*
 * ------------------------------------------------------------
 * Whisper
 * ------------------------------------------------------------
 */

static void playWhisper(void) {
    char path[MAX_PATH];

    snprintf(
        path,
        sizeof(path),
        "%s\\mus\\whisper.mp3",
        g_basePath
    );

    if (!playMP3(path, 180)) {
        return;
    }

    /*
     * Let the entire whisper play.
     */
    Sleep(19000);

    stopMP3();
}

/*
 * ------------------------------------------------------------
 * Machine whirr
 * ------------------------------------------------------------
 */

static void generateMachineWhirr(void) {
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

    if (!wav) {
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

    *(DWORD *)(wav + 16) =
        16;

    *(WORD *)(wav + 20) =
        1;

    *(WORD *)(wav + 22) =
        channels;

    *(DWORD *)(wav + 24) =
        sampleRate;

    *(DWORD *)(wav + 28) =
        sampleRate *
        channels *
        bitsPerSample / 8;

    *(WORD *)(wav + 32) =
        channels *
        bitsPerSample / 8;

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

    for (int i = 0; i < sampleCount; i++) {
        double t =
            (double)i /
            sampleRate;

        double freq1 =
            55.0 +
            sin(t * 1.7) *
            7.0;

        double freq2 =
            110.0 +
            sin(t * 2.3) *
            12.0;

        double freq3 =
            220.0 +
            sin(t * 3.1) *
            18.0;

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
            noise *
            0.035;

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

    /*
     * PlaySound with SND_MEMORY needs the buffer to remain
     * valid until playback finishes.
     */
    PlaySoundA(
        (LPCSTR)wav,
        NULL,
        SND_MEMORY |
        SND_ASYNC |
        SND_NODEFAULT
    );

    Sleep(2100);

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
) {
    (void)parameter;

    while (1) {
        /*
         * 5-20 seconds between sounds.
         */
        int delay =
            5000 +
            rand() %
            15001;

        Sleep(delay);

        int sound =
            rand() % 3;

        switch (sound) {
            case 0:
                playStatic();
                break;

            case 1:
                playWhisper();
                break;

            case 2:
                generateMachineWhirr();
                break;

            default:
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

static LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (uMsg) {
        case WM_NCHITTEST:
            return HTTRANSPARENT;

        case WM_ERASEBKGND:
            return 1;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcA(
                hwnd,
                uMsg,
                wParam,
                lParam
            );
    }
}

/*
 * ------------------------------------------------------------
 * Worker
 * ------------------------------------------------------------
 */

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    (void)hPrevInstance;
    (void)nCmdShow;

    srand(
        (unsigned int)(
            time(NULL) ^
            GetCurrentProcessId()
        )
    );

    getBasePath();

    /*
     * Parse state from launcher.
     *
     * argv-like parsing isn't used because WinMain receives
     * the command line directly.
     */
    unsigned int magic = 0;
    int stage = 0;
    int audioEvent = 0;
    int glitchCount = 0;
    int interactionCount = 0;
    unsigned int seed = 0;

    sscanf(
        lpCmdLine,
        "%u %d %d %d %d %u",
        &magic,
        &stage,
        &audioEvent,
        &glitchCount,
        &interactionCount,
        &seed
    );

    /*
     * Validate that this is actually an ARG worker launch.
     */
    if (magic != ARG_STATE_MAGIC) {
        return 1;
    }

    /*
     * Restore RNG state.
     */
    srand(
        seed ^
        (unsigned int)GetCurrentProcessId()
    );

    /*
     * --------------------------------------------------------
     * Register window
     * --------------------------------------------------------
     */

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
        CLASS_NAME;

    wc.hCursor =
        LoadCursorA(
            NULL,
            IDC_ARROW
        );

    if (
        !RegisterClassExA(&wc)
    ) {
        return 1;
    }

    int screenWidth =
        GetSystemMetrics(
            SM_CXSCREEN
        );

    int screenHeight =
        GetSystemMetrics(
            SM_CYSCREEN
        );

    DWORD exStyle =
        WS_EX_LAYERED |
        WS_EX_TOOLWINDOW |
        WS_EX_TRANSPARENT |
        WS_EX_NOACTIVATE;

    g_hwnd =
        CreateWindowExA(
            exStyle,
            CLASS_NAME,
            "",
            WS_POPUP,
            0,
            0,
            screenWidth,
            screenHeight,
            NULL,
            NULL,
            hInstance,
            NULL
        );

    if (!g_hwnd) {
        return 1;
    }

    /*
     * Completely transparent.
     */
    SetLayeredWindowAttributes(
        g_hwnd,
        0,
        0,
        LWA_ALPHA
    );

    SetWindowPos(
        g_hwnd,
        HWND_TOPMOST,
        0,
        0,
        screenWidth,
        screenHeight,
        SWP_NOACTIVATE |
        SWP_SHOWWINDOW
    );

    /*
     * Start audio.
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
     * --------------------------------------------------------
     * Test handoff
     * --------------------------------------------------------
     *
     * For now, the worker lives for 500 ms and exits.
     *
     * The launcher notices the exit and creates another
     * executable with a different filename.
     */
    Sleep(500);

    /*
     * Clean up current audio.
     */
    stopMP3();

    /*
     * Exit.
     *
     * The launcher will immediately create the next worker
     * using the preserved state.
     */
    return 0;
}