#include "arg_internal.h"

BOOL CALLBACK findWindowForProcess(
    HWND hwnd,
    LPARAM parameter
) {
    TextProcess *textProcess = (TextProcess *) parameter;
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

void closeTextProcess(
    TextProcess *textProcess
) {
    if (textProcess == NULL) {
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

        if (textProcess->process == NULL) {
            DestroyWindow(textProcess->window);
        } else {
            PostMessageA(
                textProcess->window,
                WM_CLOSE,
                0,
                0
            );
        }
    }

    if (textProcess->process != NULL) {
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
    }

    ZeroMemory(textProcess, sizeof(*textProcess));
}

void reapTextProcesses(void) {
    DWORD now = GetTickCount();

    for (int index = g_textProcessCount - 1;
         index >= 0;
         index--) {
        TextProcess *textProcess =
                &g_textProcesses[index];

        if ((textProcess->process != NULL &&
             WaitForSingleObject(
                 textProcess->process,
                 0
             ) == WAIT_OBJECT_0) ||
            (textProcess->process == NULL &&
             (textProcess->window == NULL ||
              !IsWindow(textProcess->window))) ||
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

void closeAllTextProcesses(void) {
    while (g_textProcessCount > 0) {
        closeTextProcess(&g_textProcesses[
            g_textProcessCount - 1
        ]);
        g_textProcessCount--;
    }
}

void appendCharacter(
    wchar_t *buffer,
    size_t bufferSize,
    size_t *position,
    wchar_t character
) {
    if (*position + 1 >= bufferSize) {
        return;
    }

    buffer[*position] = character;
    (*position)++;
    buffer[*position] = L'\0';
}

void createGarbageText(
    wchar_t *buffer,
    size_t bufferSize,
    TextHost host
) {
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
    } else if (host == TEXT_HOST_POWERSHELL) {
        const wchar_t prefix[] = L"# ";

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

BOOL startTextProcess(
    TextHost host,
    const wchar_t *text
) {
    if (g_textProcessCount >= ARG_MAX_TEXT_PROCESSES ||
        text == NULL) {
        return FALSE;
    }

    TextProcess *textProcess =
            &g_textProcesses[g_textProcessCount];
    const wchar_t *prompt = L"";
    int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int top = GetSystemMetrics(SM_YVIRTUALSCREEN);

    ZeroMemory(textProcess, sizeof(*textProcess));

    /*
     * A native edit window is program-owned and always accepts WM_CHAR.
     * External terminal and Notepad hosts can delegate their UI to another
     * process, which made reliable typing and safe cleanup impossible.
     */
    HWND textWindow = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        L"EDIT",
        L"",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL,
        left + 80 + (g_textProcessCount % 3) * 48,
        top + 80 + (g_textProcessCount % 3) * 48,
        720,
        420,
        NULL,
        NULL,
        GetModuleHandleW(NULL),
        NULL
    );

    if (textWindow == NULL) {
        return FALSE;
    }

    if (host == TEXT_HOST_CMD) {
        prompt = L"Microsoft Windows [ARG session]\r\nC:\\> ";
    } else if (host == TEXT_HOST_POWERSHELL) {
        prompt = L"Windows PowerShell [ARG session]\r\nPS C:\\> ";
    }

    SendMessageW(textWindow, WM_SETFONT,
                 (WPARAM) GetStockObject(ANSI_FIXED_FONT), TRUE);
    SetWindowTextW(textWindow, prompt);
    SendMessageW(textWindow, EM_SETSEL, (WPARAM) - 1, (LPARAM) - 1);
    textProcess->window = textWindow;
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

BOOL startApplicationProcess(
    const wchar_t *commandLine,
    DWORD duration
) {
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

void applySplitGeometry(
    TextProcess *textProcess,
    int side
) {
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

void updateTextProcesses(void) {
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
                (LPARAM) textProcess
            );

            if (textProcess->window == NULL &&
                textProcess->host == TEXT_HOST_CMD) {
                wchar_t title[64];

                swprintf(
                    title,
                    sizeof(title) / sizeof(title[0]),
                    L"ARG_TEXT_%lu",
                    (unsigned long) GetCurrentProcessId()
                );

                textProcess->window = FindWindowW(NULL, title);
            }
        }

        if (textProcess->window == NULL &&
            !textProcess->errorReported &&
            (LONG)(now - textProcess->startedAt) >= 10000) {
            textProcess->errorReported = TRUE;
            textProcess->closeAt = now;
        }

        if (g_splitOverlayActive &&
            textProcess
        ->
        window != NULL
        )
        {
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

BOOL loadPoems(void) {
    char path[MAX_PATH * 2];
    char line[512];
    size_t position = 0;

    snprintf(
        path,
        sizeof(path),
        "%s\\text\\poems.txt",
        g_basePath
    );

    FILE *file = fopen(path, "r");

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

    return g_poemCount >= ARG_MAX_POEMS;
}

void reservePoems(void) {
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

DWORD WINAPI ttsThread(
    LPVOID parameter
) {
    (void) parameter;

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
        (void **) &voice
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
            (size_t) length,
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

        /* Keep each poem distinct without ever blocking the UI message loop. */
        if (index + 1 < ARG_RESERVED_POEMS) {
            DWORD pauseEndsAt = GetTickCount() + 1200;

            while (!g_shutdownRequested &&
                   (LONG)(GetTickCount() - pauseEndsAt) < 0) {
                Sleep(50);
            }
        }
    }

    EnterCriticalSection(&g_ttsLock);
    g_ttsVoice = NULL;
    LeaveCriticalSection(&g_ttsLock);

    voice->lpVtbl->Release(voice);
    CoUninitialize();
    g_ttsComplete = TRUE;

    return 0;
}

void startTts(void) {
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

void stopTts(void) {
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

DWORD WINAPI step6SpeechThread(
    LPVOID parameter
) {
    (void) parameter;

    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) {
        MessageBoxA(NULL, "You can't smile there, try again.",
                    "ARG information", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    ISpVoice *voice = NULL;

    if (FAILED(CoCreateInstance(&CLSID_SpVoice, NULL, CLSCTX_ALL,
                                &IID_ISpVoice, (void **) &voice))) {
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

void sayStep6Warning(void) {
    HANDLE thread = CreateThread(NULL, 0, step6SpeechThread,
                                 NULL, 0, NULL);

    if (thread != NULL) {
        CloseHandle(thread);
    }
}

void startRandomTextWindow(void) {
    wchar_t text[ARG_TEXT_BUFFER_SIZE];
    TextHost host =
            (TextHost)(nextGlitchRandom() % 3);

    createGarbageText(text, sizeof(text) / sizeof(text[0]), host);
    startTextProcess(host, text);
}

void startPoemWindow(void) {
    if (g_nextPoem >= ARG_RESERVED_POEMS) {
        return;
    }

    wchar_t text[ARG_TEXT_BUFFER_SIZE];
    TextHost host =
            (TextHost)(nextGlitchRandom() % 3);
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
            (host == TEXT_HOST_CMD || host == TEXT_HOST_POWERSHELL)) {
            const wchar_t *prefix = host == TEXT_HOST_CMD ? L"REM " : L"# ";

            for (size_t prefixIndex = 0;
                 prefix[prefixIndex] != L'\0';
                 prefixIndex++) {
                appendCharacter(text, ARG_TEXT_BUFFER_SIZE, &position,
                                prefix[prefixIndex]);
            }
        }

        appendCharacter(
            text,
            ARG_TEXT_BUFFER_SIZE,
            &position,
            (unsigned char) poem[index]
        );
    }

    if (startTextProcess(host, text)) {
        g_nextPoem++;
    }
}

void startStep5Window(
    DWORD duration
) {
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
        choice
    ==
    sizeof(applications) /
            sizeof(applications[0])
    )
    {
        startApplicationProcess(
            L"msedge.exe --inprivate about:blank",
            duration
        );
    }
    else
    if (g_localInfoAllowed) {
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
