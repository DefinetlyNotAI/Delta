#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#pragma comment(lib, "winmm.lib")

static const char CLASS_NAME[] = "ARGOverlayWindow";

static HWND g_hwnd = NULL;
static char g_basePath[MAX_PATH];

/*
 * ------------------------------------------------------------
 * Utility
 * ------------------------------------------------------------
 */

static void buildPath(char *output, size_t outputSize, const char *relativePath) {
    snprintf(output, outputSize, "%s\\%s", g_basePath, relativePath);
}

/*
 * ------------------------------------------------------------
 * MP3 playback using Windows MCI
 * ------------------------------------------------------------
 *
 * MCI is used because PlaySound does not reliably support MP3.
 */

static void stopMP3(void) {
    mciSendStringA("stop ARG_AUDIO", NULL, 0, NULL);
    mciSendStringA("close ARG_AUDIO", NULL, 0, NULL);
}

static BOOL playMP3(const char *path, int volume) {
    char command[1024];

    stopMP3();

    snprintf(
        command,
        sizeof(command),
        "open \"%s\" type mpegvideo alias ARG_AUDIO",
        path
    );

    if (mciSendStringA(command, NULL, 0, NULL) != 0) {
        return FALSE;
    }

    /*
     * MCI volume range is generally 0-1000.
     */
    snprintf(
        command,
        sizeof(command),
        "setaudio ARG_AUDIO volume to %d",
        volume
    );

    mciSendStringA(command, NULL, 0, NULL);

    if (mciSendStringA("play ARG_AUDIO", NULL, 0, NULL) != 0) {
        stopMP3();
        return FALSE;
    }

    return TRUE;
}

/*
 * ------------------------------------------------------------
 * Machine whirr generator
 * ------------------------------------------------------------
 *
 * Generates a short synthetic mechanical/electrical sound.
 *
 * This does NOT use the computer's actual hardware.
 * It generates the waveform mathematically.
 */

static void generateMachineWhirr(void) {
    const int sampleRate = 44100;
    const int channels = 1;
    const int bitsPerSample = 16;

    const double duration = 2.0;

    const int sampleCount =
        (int)(sampleRate * duration);

    const int dataSize =
        sampleCount * channels * sizeof(short);

    const int fileSize =
        44 + dataSize;

    BYTE *wav = malloc(fileSize);

    if (!wav) {
        return;
    }

    ZeroMemory(wav, fileSize);

    /*
     * RIFF header
     */

    memcpy(wav + 0, "RIFF", 4);

    *(DWORD *)(wav + 4) =
        fileSize - 8;

    memcpy(wav + 8, "WAVE", 4);

    /*
     * fmt chunk
     */

    memcpy(wav + 12, "fmt ", 4);

    *(DWORD *)(wav + 16) = 16;

    *(WORD *)(wav + 20) = 1;

    *(WORD *)(wav + 22) = channels;

    *(DWORD *)(wav + 24) = sampleRate;

    *(DWORD *)(wav + 28) =
        sampleRate *
        channels *
        bitsPerSample / 8;

    *(WORD *)(wav + 32) =
        channels *
        bitsPerSample / 8;

    *(WORD *)(wav + 34) =
        bitsPerSample;

    /*
     * data chunk
     */

    memcpy(wav + 36, "data", 4);

    *(DWORD *)(wav + 40) =
        dataSize;

    short *samples =
        (short *)(wav + 44);

    /*
     * Generate several frequencies simultaneously.
     *
     * The slowly changing frequencies make it sound more
     * mechanical than a simple sine wave.
     */

    double phase1 = 0.0;
    double phase2 = 0.0;
    double phase3 = 0.0;

    for (int i = 0; i < sampleCount; i++) {

        double t =
            (double)i / sampleRate;

        /*
         * Slowly changing motor frequencies.
         */

        double freq1 =
            55.0 + sin(t * 1.7) * 7.0;

        double freq2 =
            110.0 + sin(t * 2.3) * 12.0;

        double freq3 =
            220.0 + sin(t * 3.1) * 18.0;

        phase1 +=
            2.0 * M_PI * freq1 / sampleRate;

        phase2 +=
            2.0 * M_PI * freq2 / sampleRate;

        phase3 +=
            2.0 * M_PI * freq3 / sampleRate;

        /*
         * Mechanical components.
         */

        double signal =
            sin(phase1) * 0.35 +
            sin(phase2) * 0.15 +
            sin(phase3) * 0.06;

        /*
         * Add subtle noise.
         */

        double noise =
            ((double)rand() / RAND_MAX) * 2.0 - 1.0;

        signal += noise * 0.035;

        /*
         * Fade in/out so the sound doesn't click.
         */

        double envelope = 1.0;

        if (t < 0.15) {
            envelope = t / 0.15;
        }

        if (t > duration - 0.25) {
            envelope =
                (duration - t) / 0.25;
        }

        signal *= envelope;

        if (signal > 1.0)
            signal = 1.0;

        if (signal < -1.0)
            signal = -1.0;

        samples[i] =
            (short)(signal * 32767.0);
    }

    /*
     * Play the generated WAV directly from memory.
     */

    PlaySoundA(
        (LPCSTR)wav,
        NULL,
        SND_MEMORY |
        SND_ASYNC |
        SND_NODEFAULT
    );

    /*
     * We cannot free the buffer immediately because
     * PlaySound(SND_ASYNC) still needs it.
     *
     * Give the sound time to finish.
     */

    Sleep(2100);

    PlaySoundA(NULL, NULL, 0);

    free(wav);
}

/*
 * ------------------------------------------------------------
 * Audio events
 * ------------------------------------------------------------
 */

static void playStatic(void) {
    char path[MAX_PATH];

    buildPath(
        path,
        sizeof(path),
        "mus\\static.mp3"
    );

    /*
     * Static is intentionally short.
     */

    if (!playMP3(path, 450)) {
        return;
    }

    int duration =
        300 + rand() % 701;

    Sleep(duration);

    stopMP3();
}

static void playWhisper(void) {
    char path[MAX_PATH];

    buildPath(
        path,
        sizeof(path),
        "mus\\whisper.mp3"
    );

    /*
     * Whisper is played quietly.
     *
     * 180/1000 volume.
     */

    if (!playMP3(path, 180)) {
        return;
    }

    /*
     * whisper.mp3 is approximately 18 seconds.
     * We let the entire file play.
     */

    Sleep(19000);

    stopMP3();
}

static void playRandomSound(void) {
    /*
     * Three sound categories:
     *
     * 0 = static
     * 1 = whisper
     * 2 = machine
     */

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

/*
 * ------------------------------------------------------------
 * Audio thread
 * ------------------------------------------------------------
 */

static DWORD WINAPI audioThread(LPVOID parameter) {
    (void)parameter;

    while (1) {

        /*
         * Wait a random amount of time before making
         * another sound.
         *
         * 5 to 20 seconds.
         */

        int delay =
            5000 + rand() % 15001;

        Sleep(delay);

        playRandomSound();

        /*
         * Wait again before another event.
         */
    }

    return 0;
}

/*
 * ------------------------------------------------------------
 * Window
 * ------------------------------------------------------------
 */

LRESULT CALLBACK WindowProc(
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
 * Entry point
 * ------------------------------------------------------------
 */

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    srand((unsigned int)time(NULL));

    /*
     * Find the directory containing the EXE.
     */

    GetModuleFileNameA(
        NULL,
        g_basePath,
        MAX_PATH
    );

    char *lastSlash =
        strrchr(g_basePath, '\\');

    if (lastSlash) {
        *lastSlash = '\0';
    }

    /*
     * Register window class.
     */

    WNDCLASSEXA wc;

    ZeroMemory(
        &wc,
        sizeof(wc)
    );

    wc.cbSize =
        sizeof(WNDCLASSEXA);

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

    if (!RegisterClassExA(&wc)) {
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

    g_hwnd = CreateWindowExA(
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
     * Completely transparent for now.
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
     * Start the audio system.
     */

    HANDLE thread =
        CreateThread(
            NULL,
            0,
            audioThread,
            NULL,
            0,
            NULL
        );

    if (!thread) {
        return 1;
    }

    CloseHandle(thread);

    /*
     * Main Windows message loop.
     */

    MSG msg;

    while (
        GetMessageA(
            &msg,
            NULL,
            0,
            0
        ) > 0
    ) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    stopMP3();

    return 0;
}
