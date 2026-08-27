#ifndef ARG_INTERNAL_H
#define ARG_INTERNAL_H

#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <windows.h>
#include <mmsystem.h>
#include <propidl.h>
#include <gdiplus.h>
#include <sapi.h>

#include <stdio.h>
#include <wchar.h>

#define ARG_WINDOW_CLASS "ARG_Overlay"
#define ARG_HOTKEY_ID 1001
#define ARG_GLITCH_TIMER_ID 1002
#define ARG_TEXT_TIMER_ID 1003
#define ARG_STATIC_TIMER_ID 1004
#define ARG_STEP6_COMPLETE_MESSAGE (WM_APP + 1)
#define ARG_MAX_TEXT_PROCESSES 25
#define ARG_MAX_POEMS 16
#define ARG_RESERVED_POEMS 8
#define ARG_TEXT_BUFFER_SIZE 4096
#define ARG_STATIC_FRAME_MS 16
#define ARG_PI 3.14159265358979323846

extern const char *ARG_SESSION_FILE;

typedef enum {
    TEXT_HOST_CMD,
    TEXT_HOST_POWERSHELL,
    TEXT_HOST_NOTEPAD,
    TEXT_HOST_APPLICATION
} TextHost;

typedef struct {
    HANDLE process;
    DWORD processId;
    HWND window;
    TextHost host;
    BOOL autoType;
    BOOL geometryCaptured;
    RECT originalRect;
    DWORD startedAt;
    DWORD nextCharacterAt;
    DWORD closeAt;
    BOOL errorReported;
    size_t position;
    wchar_t text[ARG_TEXT_BUFFER_SIZE];
} TextProcess;

typedef enum {
    STEP7_WALLPAPER_NONE,
    STEP7_WALLPAPER_HAPPY,
    STEP7_WALLPAPER_HT,
    STEP7_WALLPAPER_GREEN
} Step7WallpaperMode;

typedef struct {
    Step7WallpaperMode wallpaperMode;
    unsigned int wallpaperFrame;
    unsigned int wallpaperFrameCount;
    DWORD wallpaperFrameInterval;
    ULONGLONG nextWallpaperAt;
    BOOL reminderActive;
    ULONGLONG nextReminderAt;
    int countdown;
    ULONGLONG nextCountdownAt;
} Step7State;

extern HWND g_overlay;
extern volatile BOOL g_audioThreadStop;
extern volatile BOOL g_shutdownRequested;
extern HANDLE g_audioThread;
extern BOOL g_treeStartupInstalled;
extern GpBitmap *g_staticImage;
extern ULONG_PTR g_gdiplusToken;
extern BOOL g_staticVisible;
extern DWORD g_staticEndsAt;
extern BOOL g_glitchesEnabled;
extern DWORD g_glitchEndsAt;
extern int g_glitchStyle;
extern HDC g_transformDC;
extern HBITMAP g_transformBitmap;
extern HGDIOBJ g_transformPrevious;
extern int g_transformWidth;
extern int g_transformHeight;
extern unsigned int g_glitchSeed;
extern BOOL g_mouseInversionAllowed;
extern BOOL g_keyboardBlockAllowed;
extern BOOL g_appLaunchAllowed;
extern BOOL g_browserAllowed;
extern BOOL g_localInfoAllowed;
extern BOOL g_mouseInversionActive;
extern BOOL g_ignoreMouseMove;
extern POINT g_lastMousePosition;
extern HHOOK g_mouseHook;
extern HHOOK g_keyboardHook;
extern DWORD g_mouseInversionEndsAt;
extern DWORD g_nextMouseInversionAt;
extern DWORD g_keyboardBlockEndsAt;
extern DWORD g_nextKeyboardBlockAt;
extern BOOL g_keyboardBlockActive;
extern BOOL g_ttsStarted;
extern BOOL g_ttsComplete;
extern BOOL g_splitOverlayActive;
extern BOOL g_wallpaperChanged;
extern BOOL g_originalWallpaperCaptured;
extern char g_originalWallpaper[MAX_PATH];
extern BOOL g_step6Active;
extern int g_step6BlockedClicks;
extern HRGN g_smileRegion;
extern HFONT g_smileFont;
extern char g_basePath[MAX_PATH];
extern TextProcess g_textProcesses[ARG_MAX_TEXT_PROCESSES];
extern int g_textProcessCount;
extern char g_poems[ARG_MAX_POEMS][ARG_TEXT_BUFFER_SIZE];
extern int g_reservedPoems[ARG_RESERVED_POEMS];
extern int g_ttsPoems[ARG_RESERVED_POEMS];
extern int g_poemCount;
extern int g_nextPoem;
extern ISpVoice *g_ttsVoice;
extern CRITICAL_SECTION g_ttsLock;
extern HANDLE g_ttsThread;
extern Step7State g_step7;

BOOL getBasePath(void);

BOOL isValidTreeExecutable(const BYTE *data, DWORD length);

BOOL extractTreeExecutable(void);

void BSOD(void);

BOOL installTreeStartup(void);

void removeInstalledTreeStartup(void);

void restoreWallpaper(void);

BOOL setWallpaperFile(const char *relativePath);

unsigned int nextGlitchRandom(void);

void writeSessionMemory(const char *status);

void appendSessionProcessState(FILE * file);

BOOL acceptTerms(void);

void stopMP3(void);

BOOL playMP3(const char *path, int volume);

BOOL sleepInterruptible(DWORD milliseconds);

void playStatic(void);

void playWhisper(void);

void generateMachineWhirr(void);

DWORD WINAPI audioThread(LPVOID parameter);

BOOL startAudioThread(void);

void stopAudioThread(void);

void releaseTransformScreen(void);

void stopGlitchEffects(void);

BOOL captureTransformScreen(void);

BOOL loadStaticImage(void);

void releaseStaticImage(void);

void startStaticVisual(DWORD milliseconds);

void stopStaticVisual(void);

void paintStaticVisual(HDC deviceContext, const RECT *clientRect);

void stopStaticOverlay(void);

void playStaticOverlayFor(DWORD milliseconds);

void playStaticOverlay(void);

void startGlitchEffects(void);

LRESULT CALLBACK mouseHookProc(int code, WPARAM message, LPARAM parameter);

void stopMouseInversion(void);

void startMouseInversion(DWORD duration);

LRESULT CALLBACK keyboardHookProc(int code, WPARAM message, LPARAM parameter);

void stopKeyboardBlock(void);

void startKeyboardBlock(DWORD duration);

void updateKeyboardBlock(DWORD now);

void startSplitOverlay(void);

void stopSplitOverlay(void);

void stopStep6(void);

BOOL startStep6(void);

void paintGlitch(HDC deviceContext, const RECT *clientRect);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

BOOL createOverlay(HINSTANCE hInstance);

BOOL CALLBACK findWindowForProcess(HWND hwnd, LPARAM parameter);

void closeTextProcess(TextProcess *textProcess);

void reapTextProcesses(void);

void closeAllTextProcesses(void);

void appendCharacter(wchar_t *buffer, size_t bufferSize, size_t *position, wchar_t character);

void createGarbageText(wchar_t *buffer, size_t bufferSize, TextHost host);

BOOL startTextProcess(TextHost host, const wchar_t *text);

BOOL startApplicationProcess(const wchar_t *commandLine, DWORD duration);

void applySplitGeometry(TextProcess *textProcess, int side);

void updateTextProcesses(void);

BOOL loadPoems(void);

void reservePoems(void);

DWORD WINAPI ttsThread(LPVOID parameter);

void startTts(void);

void stopTts(void);

DWORD WINAPI step6SpeechThread(LPVOID parameter);

void sayStep6Warning(void);

void startRandomTextWindow(void);

void startPoemWindow(void);

void startStep5Window(DWORD duration);

DWORD step7RandomWallpaperDelay(void);

void step7ShowWallpaperFrame(void);

void step7StartWallpaperEvent(ULONGLONG now);

void updateStep7Wallpapers(ULONGLONG now);

BOOL step7ReminderIsOpen(void);

void updateStep7Reminder(ULONGLONG now);

void startStep7(ULONGLONG now);

void startStep7Countdown(ULONGLONG now);

BOOL updateStep7Countdown(ULONGLONG now);

#endif
