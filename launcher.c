#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define ARG_PREFIX "ARG_"

static BOOL getExecutableDirectory(
    char *buffer,
    DWORD bufferSize
)
{
    DWORD length = GetModuleFileNameA(
        NULL,
        buffer,
        bufferSize
    );

    if (length == 0 || length >= bufferSize) {
        return FALSE;
    }

    char *slash = strrchr(buffer, '\\');

    if (slash == NULL) {
        return FALSE;
    }

    *slash = '\0';

    return TRUE;
}

static BOOL getWorkerPath(
    char *path,
    size_t pathSize
)
{
    char directory[MAX_PATH];

    if (!getExecutableDirectory(
        directory,
        sizeof(directory)
    )) {
        return FALSE;
    }

    unsigned int value =
        ((unsigned int)GetTickCount()) ^
        ((unsigned int)GetCurrentProcessId()) ^
        ((unsigned int)time(NULL));

    srand(value);

    unsigned int randomValue =
        ((unsigned int)rand() << 16) ^
        (unsigned int)rand();

    snprintf(
        path,
        pathSize,
        "%s\\" ARG_PREFIX "%08X.exe",
        directory,
        randomValue
    );

    return TRUE;
}

static BOOL findWorkerSource(
    char *path,
    size_t pathSize
)
{
    char directory[MAX_PATH];

    if (!getExecutableDirectory(
        directory,
        sizeof(directory)
    )) {
        return FALSE;
    }

    snprintf(
        path,
        pathSize,
        "%s\\ARG.exe",
        directory
    );

    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    char source[MAX_PATH];
    char destination[MAX_PATH];

    if (!findWorkerSource(
        source,
        sizeof(source)
    )) {
        MessageBoxA(
            NULL,
            "ARG.exe could not be found.",
            "ARG Launcher",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    if (!getWorkerPath(
        destination,
        sizeof(destination)
    )) {
        MessageBoxA(
            NULL,
            "Could not generate worker filename.",
            "ARG Launcher",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    if (!CopyFileA(
        source,
        destination,
        TRUE
    )) {
        MessageBoxA(
            NULL,
            "Could not create ARG worker.",
            "ARG Launcher",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

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
     * ARG.exe is a GUI executable, so no
     * console window is created.
     */
    BOOL created = CreateProcessA(
        destination,
        NULL,
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &startupInfo,
        &processInfo
    );

    if (!created) {
        DeleteFileA(destination);

        MessageBoxA(
            NULL,
            "Could not start ARG worker.",
            "ARG Launcher",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    CloseHandle(
        processInfo.hThread
    );

    CloseHandle(
        processInfo.hProcess
    );

    /*
     * Launcher terminates immediately.
     */
    return 0;
}