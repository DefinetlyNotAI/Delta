#include "arg_internal.h"

void stopMP3(void) {
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

BOOL playMP3(
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

BOOL sleepInterruptible(
    DWORD milliseconds
) {
    DWORD elapsed = 0;

    while (
        elapsed < milliseconds &&
        !g_shutdownRequested &&
        !g_audioThreadStop
    ) {
        const DWORD interval = 50;
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

void playStatic(void) {
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
     * Ambient static should read as a distinct effect rather than a click.
     * Step 7 retains its separately specified 50/100 ms overlays.
     */
    int duration =
            1200 + rand() % 1801;

    startStaticVisual((DWORD) duration);
    sleepInterruptible(
        duration
    );

    if (!g_audioThreadStop) {
        stopMP3();
    }
}

void playWhisper(void) {
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

void generateMachineWhirr(void) {
    if (g_audioThreadStop) {
        return;
    }

    const int sampleRate = 44100;
    const int channels = 1;
    const int bitsPerSample = 16;

    const double duration = 2.0;

    const int sampleCount =
            (int) (
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

    *(DWORD *) (wav + 4) =
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

    *(DWORD *) (wav + 16) = 16;
    *(WORD *) (wav + 20) = 1;
    *(WORD *) (wav + 22) = channels;
    *(DWORD *) (wav + 24) = sampleRate;

    *(DWORD *) (wav + 28) =
            sampleRate *
            channels *
            bitsPerSample /
            8;

    *(WORD *) (wav + 32) =
            channels *
            bitsPerSample /
            8;

    *(WORD *) (wav + 34) =
            bitsPerSample;

    memcpy(
        wav + 36,
        "data",
        4
    );

    *(DWORD *) (wav + 40) =
            dataSize;

    short *samples =
            (short *) (wav + 44);

    double phase1 = 0.0;
    double phase2 = 0.0;
    double phase3 = 0.0;

    for (
        int i = 0;
        i < sampleCount;
        i++
    ) {
        double t =
                (double) i /
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
                ARG_PI *
                freq1 /
                sampleRate;

        phase2 +=
                2.0 *
                ARG_PI *
                freq2 /
                sampleRate;

        phase3 +=
                2.0 *
                ARG_PI *
                freq3 /
                sampleRate;

        double signal =
                sin(phase1) * 0.35 +
                sin(phase2) * 0.15 +
                sin(phase3) * 0.06;

        double noise =
                (double) rand() /
                RAND_MAX *
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
                (short) (
                    signal *
                    32767.0
                );
    }

    PlaySoundA(
        (LPCSTR) wav,
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

DWORD WINAPI audioThread(
    LPVOID parameter
) {
    (void) parameter;

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

        // ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
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


BOOL startAudioThread(void) {
    if (g_audioThread != NULL) {
        return TRUE;
    }

    g_audioThreadStop = FALSE;
    g_audioThread = CreateThread(NULL, 0, audioThread, NULL, 0, NULL);
    return g_audioThread != NULL;
}

void stopAudioThread(void) {
    g_audioThreadStop = TRUE;

    if (g_audioThread != NULL) {
        WaitForSingleObject(g_audioThread, 3000);
        CloseHandle(g_audioThread);
        g_audioThread = NULL;
    }
}
