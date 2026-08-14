#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "state.h"

static char g_launcherPath[MAX_PATH];
static char g_workerTemplate[MAX_PATH];

/*
 * Characters allowed in Windows filenames.
 *
 * We deliberately avoid characters that Windows does not allow
 * in filenames: < > : " / \ | ? *
 */
static const char NAME_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "_-";

static void getLauncherDirectory(void) {
    GetModuleFileNameA(
        NULL,
        g_launcherPath,
        sizeof(g_launcherPath)
    );

    char *slash =
        strrchr(g_launcherPath, '\\');

    if (slash != NULL) {
        *slash = '\0';
    }

    snprintf(
        g_workerTemplate,
        sizeof(g_workerTemplate),
        "%s\\ARGWorkerTemplate.exe",
        g_launcherPath
    );
}

static void generateWorkerName(
    char *output,
    size_t outputSize
) {
    int length =
        4 + rand() % 10;

    int charsetSize =
        (int)(sizeof(NAME_CHARS) - 1);

    if (length + 5 >= (int)outputSize) {
        length = (int)outputSize - 6;
    }

    for (int i = 0; i < length; i++) {
        output[i] =
            NAME_CHARS[
                rand() % charsetSize
            ];
    }

    output[length] = '\0';

    /*
     * Every generated worker is an EXE.
     */
    strncat(
        output,
        ".exe",
        outputSize - strlen(output) - 1
    );
}

static BOOL createWorker(
    const ARGState *state,
    char *workerPath,
    size_t workerPathSize
) {
    char filename[64];

    generateWorkerName(
        filename,
        sizeof(filename)
    );

    snprintf(
        workerPath,
        workerPathSize,
        "%s\\%s",
        g_launcherPath,
        filename
    );

    /*
     * Copy the stable worker template to the randomized filename.
     */
    if (!CopyFileA(
        g_workerTemplate,
        workerPath,
        FALSE
    )) {
        return FALSE;
    }

    /*
     * Pass the state through the command line.
     */
    char commandLine[512];

    snprintf(
        commandLine,
        sizeof(commandLine),
        "\"%s\" %u %d %d %d %d %u",
        workerPath,
        state->magic,
        state->stage,
        state->audioEvent,
        state->glitchCount,
        state->interactionCount,
        state->seed
    );

    STARTUPINFOA startupInfo;

    ZeroMemory(
        &startupInfo,
        sizeof(startupInfo)
    );

    startupInfo.cb =
        sizeof(startupInfo);

    PROCESS_INFORMATION processInfo;

    ZeroMemory(
        &processInfo,
        sizeof(processInfo)
    );

    /*
     * CreateProcess may modify its command-line buffer,
     * so commandLine must be writable.
     */
    BOOL result =
        CreateProcessA(
            NULL,
            commandLine,
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            g_launcherPath,
            &startupInfo,
            &processInfo
        );

    if (!result) {
        DeleteFileA(workerPath);
        return FALSE;
    }

    /*
     * We don't need the thread handle.
     */
    CloseHandle(
        processInfo.hThread
    );

    /*
     * Return the process handle to the caller through
     * a temporary mechanism is unnecessary here.
     *
     * The launcher waits using the process ID below.
     */
    CloseHandle(
        processInfo.hProcess
    );

    return TRUE;
}

static DWORD launchWorker(
    const ARGState *state,
    char *workerPath,
    size_t workerPathSize
) {
    char filename[64];

    generateWorkerName(
        filename,
        sizeof(filename)
    );

    snprintf(
        workerPath,
        workerPathSize,
        "%s\\%s",
        g_launcherPath,
        filename
    );

    if (!CopyFileA(
        g_workerTemplate,
        workerPath,
        FALSE
    )) {
        return 0;
    }

    char commandLine[512];

    snprintf(
        commandLine,
        sizeof(commandLine),
        "\"%s\" %u %d %d %d %d %u",
        workerPath,
        state->magic,
        state->stage,
        state->audioEvent,
        state->glitchCount,
        state->interactionCount,
        state->seed
    );

    STARTUPINFOA startupInfo;

    ZeroMemory(
        &startupInfo,
        sizeof(startupInfo)
    );

    startupInfo.cb =
        sizeof(startupInfo);

    PROCESS_INFORMATION processInfo;

    ZeroMemory(
        &processInfo,
        sizeof(processInfo)
    );

    BOOL result =
        CreateProcessA(
            NULL,
            commandLine,
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            g_launcherPath,
            &startupInfo,
            &processInfo
        );

    if (!result) {
        DeleteFileA(workerPath);
        return 0;
    }

    CloseHandle(
        processInfo.hThread
    );

    DWORD processId =
        processInfo.dwProcessId;

    CloseHandle(
        processInfo.hProcess
    );

    return processId;
}

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    srand(
        (unsigned int)
        time(NULL)
    );

    getLauncherDirectory();

    /*
     * Initial ARG state.
     */
    ARGState state;

    state.magic =
        ARG_STATE_MAGIC;

    state.stage = 0;
    state.audioEvent = 0;
    state.glitchCount = 0;
    state.interactionCount = 0;
    state.seed =
        (unsigned int)rand();

    while (1) {
        char workerPath[MAX_PATH];

        DWORD workerPID =
            launchWorker(
                &state,
                workerPath,
                sizeof(workerPath)
            );

        if (workerPID == 0) {
            Sleep(1000);
            continue;
        }

        /*
         * Wait for this worker to exit.
         *
         * The worker currently exits after ~500 ms.
         */
        HANDLE process =
            OpenProcess(
                SYNCHRONIZE,
                FALSE,
                workerPID
            );

        if (process != NULL) {
            WaitForSingleObject(
                process,
                INFINITE
            );

            CloseHandle(process);
        }

        /*
         * Worker has handed control back to us.
         *
         * Advance the state for testing.
         */
        state.stage++;

        /*
         * Remove the old executable.
         */
        DeleteFileA(
            workerPath
        );
    }

    return 0;
}