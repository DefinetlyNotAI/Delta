#define WIN32_LEAN_AND_MEAN

#include "name.h"

#include <windows.h>
#include <stdlib.h>
#include <stdint.h>

static HWND g_nameWindow = NULL;
static HANDLE g_nameThread = NULL;
static volatile LONG g_nameRunning = 0;

/*
 * Characters used for the corruption effect.
 */
static const char g_charset[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "!@#$%^&*()_+-=[]{}"
    "\\|;:',.<>/?";

/*
 * Generates a corrupted version of a base name.
 *
 * The base characters remain recognizable sometimes, while
 * individual characters randomly become corrupted.
 */
static void generateCorruptedName(char *output, int outputSize) {
    const char base[] = "ARG";
    const int baseLength = 3;

    if (outputSize < 4) {
        return;
    }

    /*
     * Most of the time, keep ARG recognizable.
     */
    for (int i = 0; i < baseLength; i++) {
        int mode = rand() % 4;

        if (mode == 0) {
            output[i] = base[i];
        } else {
            int charsetSize =
                (int)(sizeof(g_charset) - 1);

            output[i] =
                g_charset[rand() % charsetSize];
        }
    }

    /*
     * Occasionally add corruption around it.
     */
    int length = baseLength;

    if (rand() % 3 == 0) {
        int extra = 1 + rand() % 8;

        if (length + extra >= outputSize) {
            extra = outputSize - length - 1;
        }

        for (int i = 0; i < extra; i++) {
            int charsetSize =
                (int)(sizeof(g_charset) - 1);

            output[length++] =
                g_charset[rand() % charsetSize];
        }
    }

    output[length] = '\0';
}

/*
 * Thread responsible for constantly changing the window title.
 */
static DWORD WINAPI nameThread(LPVOID parameter) {
    (void)parameter;

    char title[64];

    /*
     * Seed independently for this module.
     */
    srand(
        (unsigned int)
        (GetTickCount64() ^
         (uintptr_t)GetCurrentThreadId())
    );

    while (
        InterlockedCompareExchange(
            &g_nameRunning,
            0,
            0
        )
    ) {
        generateCorruptedName(
            title,
            sizeof(title)
        );

        if (g_nameWindow != NULL) {
            /*
             * This changes the actual window title.
             *
             * For a normal GUI application, Task Manager's
             * Processes/Apps representation can use this
             * application/window title.
             */
            SetWindowTextA(
                g_nameWindow,
                title
            );
        }

        /*
         * Very rapid corruption.
         *
         * 20-80 milliseconds.
         */
        Sleep(
            20 + rand() % 61
        );
    }

    return 0;
}

void Name_Init(HWND hwnd) {
    g_nameWindow = hwnd;
}

void Name_Start(void) {
    if (g_nameWindow == NULL) {
        return;
    }

    /*
     * Don't create another thread if one already exists.
     */
    if (g_nameThread != NULL) {
        return;
    }

    InterlockedExchange(
        &g_nameRunning,
        1
    );

    g_nameThread = CreateThread(
        NULL,
        0,
        nameThread,
        NULL,
        0,
        NULL
    );
}

void Name_Stop(void) {
    if (g_nameThread == NULL) {
        return;
    }

    InterlockedExchange(
        &g_nameRunning,
        0
    );

    /*
     * Wait for the title thread to finish.
     */
    WaitForSingleObject(
        g_nameThread,
        1000
    );

    CloseHandle(
        g_nameThread
    );

    g_nameThread = NULL;
}