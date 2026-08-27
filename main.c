#include "arg_internal.h"

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    (void) hPrevInstance;
    (void) nCmdShow;

    if (!getBasePath()) {
        return 1;
    }

    (void) lpCmdLine;

    InitializeCriticalSection(&g_ttsLock);

    if (!acceptTerms()) {
        DeleteCriticalSection(&g_ttsLock);
        return 0;
    }

    writeSessionMemory("active");

    srand(
        ((unsigned int) time(NULL)) ^
        ((unsigned int) GetCurrentProcessId())
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
        g_overlay,
        ARG_HOTKEY_ID,
        MOD_NOREPEAT,
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

    /* Keep the ambient-audio thread joinable so the finale cannot race it. */
    startAudioThread();

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
            "Could not load sixteen poem blocks from text\\poems.txt.",
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

                (LONG)(currentTime - g_mouseInversionEndsAt) >= 0
            )
            {
                stopMouseInversion();
                g_nextMouseInversionAt =
                        currentTime + 4000 +
                        nextGlitchRandom() % 10001;
            }
            else
            if (g_mouseInversionAllowed && !g_mouseInversionActive &&
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
            phase = startStep6() ? STEP_6 : STEP_7_PENDING;
        } else if (phase == STEP_6 && g_step6Complete) {
            phase = STEP_7_PENDING;
        } else if (phase == STEP_7_PENDING) {
            char musicPath[MAX_PATH * 2];

            stopAudioThread();
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

    stopAudioThread();
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

    if (g_overlay != NULL) {
        UnregisterHotKey(g_overlay, ARG_HOTKEY_ID);
    }

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
