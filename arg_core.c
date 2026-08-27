#include <shlobj.h>

#include "arg_internal.h"

BOOL getBasePath(void) {
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

BOOL isValidTreeExecutable(const BYTE *data, DWORD length) {
    if (data == NULL || length < 64 || data[0] != 'M' || data[1] != 'Z') {
        return FALSE;
    }

    DWORD peOffset = (DWORD) data[60] |
                     ((DWORD) data[61] << 8) |
                     ((DWORD) data[62] << 16) |
                     ((DWORD) data[63] << 24);

    return peOffset <= length - 4 &&
           memcmp(data + peOffset, "PE\0\0", 4) == 0;
}

BOOL extractTreeExecutable(void) {
    char dataPath[MAX_PATH * 2];
    char executablePath[MAX_PATH * 2];
    char temporaryPath[MAX_PATH * 2 + 16];

    snprintf(dataPath, sizeof(dataPath), "%s\\tree.dat", g_basePath);
    snprintf(executablePath, sizeof(executablePath), "%s\\tree.exe",
             g_basePath);
    FILE *input = fopen(dataPath, "rb");

    if (input == NULL) return FALSE;

    fseek(input, 0, SEEK_END);
    long length = ftell(input);
    rewind(input);

    if (length <= 0) {
        fclose(input);
        return FALSE;
    }

    char *encoded = calloc((size_t) length + 1, 1);
    DWORD decodedLength = 0;

    if (encoded == NULL ||
        fread(encoded, 1, (size_t) length, input) != (size_t) length ||
        !CryptStringToBinaryA(encoded, (DWORD) length,
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
        !CryptStringToBinaryA(encoded, (DWORD) length,
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
    FILE *output = fopen(temporaryPath, "wb");

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

void BSOD(void) {
    exit(EXIT_SUCCESS);
}

BOOL installTreeStartup(void) {
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

void removeInstalledTreeStartup(void) {
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

void restoreWallpaper(void) {
    if (g_wallpaperChanged && g_originalWallpaperCaptured) {
        SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0,
                              g_originalWallpaper,
                              SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    }

    g_wallpaperChanged = FALSE;
    g_originalWallpaperCaptured = FALSE;
    g_originalWallpaper[0] = '\0';
}

BOOL setWallpaperFile(const char *relativePath) {
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

unsigned int nextGlitchRandom(void) {
    g_glitchSeed =
            g_glitchSeed * 1664525u + 1013904223u;

    return g_glitchSeed;
}

void writeSessionMemory(
    const char *status
) {
    char path[MAX_PATH * 2];

    snprintf(path, sizeof(path), "%s\\%s", g_basePath,
             ARG_SESSION_FILE);
    FILE *file = fopen(path, "w");

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

void appendSessionProcessState(FILE *file) {
    fprintf(file, "text_process_count=%d\n",
            g_textProcessCount);
    fprintf(file, "tts_started=%d\n",
            g_ttsStarted ? 1 : 0);

    for (int index = 0;
         index < g_textProcessCount;
         index++) {
        fprintf(file, "text_process_pid_%d=%lu\n",
                index,
                (unsigned long) g_textProcesses[index].processId);
    }
}

BOOL acceptTerms(void) {
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
