#include "arg_internal.h"

void releaseTransformScreen(void) {
    if (g_transformDC != NULL) {
        SelectObject(g_transformDC, g_transformPrevious);
        DeleteDC(g_transformDC);
        g_transformDC = NULL;
        g_transformPrevious = NULL;
    }

    if (g_transformBitmap != NULL) {
        DeleteObject(g_transformBitmap);
        g_transformBitmap = NULL;
    }
}

void stopGlitchEffects(void) {
    g_glitchesEnabled = FALSE;
    g_glitchEndsAt = 0;
    g_glitchStyle = 0;
    releaseTransformScreen();

    if (g_overlay != NULL) {
        KillTimer(
            g_overlay,
            ARG_GLITCH_TIMER_ID
        );

        SetLayeredWindowAttributes(
            g_overlay,
            0,
            0,
            LWA_ALPHA
        );
    }
}

BOOL captureTransformScreen(void) {
    HDC screen;
    int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int top = GetSystemMetrics(SM_YVIRTUALSCREEN);

    releaseTransformScreen();
    g_transformWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_transformHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    screen = GetDC(NULL);

    if (screen == NULL || g_transformWidth <= 0 ||
        g_transformHeight <= 0) {
        if (screen != NULL) ReleaseDC(NULL, screen);
        return FALSE;
    }

    g_transformDC = CreateCompatibleDC(screen);
    g_transformBitmap = CreateCompatibleBitmap(screen, g_transformWidth,
                                               g_transformHeight);
    if (g_transformDC == NULL || g_transformBitmap == NULL) {
        ReleaseDC(NULL, screen);
        releaseTransformScreen();
        return FALSE;
    }

    g_transformPrevious = SelectObject(g_transformDC, g_transformBitmap);
    BitBlt(g_transformDC, 0, 0, g_transformWidth, g_transformHeight, screen,
           left, top, SRCCOPY);
    ReleaseDC(NULL, screen);
    return TRUE;
}

BOOL loadStaticImage(void) {
    char imagePath[MAX_PATH * 2];
    wchar_t widePath[MAX_PATH * 2];
    GdiplusStartupInput startupInput;

    if (g_staticImage != NULL) {
        return TRUE;
    }

    if (g_gdiplusToken == 0) {
        ZeroMemory(&startupInput, sizeof(startupInput));
        startupInput.GdiplusVersion = 1;

        if (GdiplusStartup(&g_gdiplusToken, &startupInput, NULL) != Ok) {
            g_gdiplusToken = 0;
            return FALSE;
        }
    }

    snprintf(imagePath, sizeof(imagePath), "%s\\img\\static.jpg",
             g_basePath);
    if (MultiByteToWideChar(CP_ACP, 0, imagePath, -1, widePath,
                            sizeof(widePath) / sizeof(widePath[0])) == 0 ||
        GdipCreateBitmapFromFile(widePath, &g_staticImage) != Ok) {
        g_staticImage = NULL;
        return FALSE;
    }

    return TRUE;
}

void releaseStaticImage(void) {
    if (g_staticImage != NULL) {
        GdipDisposeImage((GpImage *) g_staticImage);
        g_staticImage = NULL;
    }

    if (g_gdiplusToken != 0) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

void startStaticVisual(DWORD milliseconds) {
    if (g_overlay == NULL || !loadStaticImage()) {
        return;
    }

    g_staticVisible = TRUE;
    g_staticEndsAt = GetTickCount() + milliseconds;
    SetTimer(g_overlay, ARG_STATIC_TIMER_ID, ARG_STATIC_FRAME_MS, NULL);
    InvalidateRect(g_overlay, NULL, FALSE);
}

void stopStaticVisual(void) {
    g_staticVisible = FALSE;
    g_staticEndsAt = 0;

    if (g_overlay != NULL) {
        KillTimer(g_overlay, ARG_STATIC_TIMER_ID);
        InvalidateRect(g_overlay, NULL, FALSE);
    }
}

void paintStaticVisual(HDC deviceContext, const RECT *clientRect) {
    GpGraphics *graphics = NULL;

    if (!g_staticVisible || g_staticImage == NULL ||
        GdipCreateFromHDC(deviceContext, &graphics) != Ok) {
        return;
    }

    GdipDrawImageRectI(graphics, (GpImage *) g_staticImage,
                       clientRect->left, clientRect->top,
                       clientRect->right - clientRect->left,
                       clientRect->bottom - clientRect->top);
    GdipDeleteGraphics(graphics);
}

void stopStaticOverlay(void) {
    mciSendStringA("stop ARG_STATIC", NULL, 0, NULL);
    mciSendStringA("close ARG_STATIC", NULL, 0, NULL);
    stopStaticVisual();
}

void playStaticOverlayFor(DWORD milliseconds) {
    char path[MAX_PATH * 2];
    char command[1024];

    snprintf(path, sizeof(path), "%s\\mus\\static.mp3", g_basePath);
    stopStaticOverlay();
    snprintf(command, sizeof(command),
             "open \"%s\" type mpegvideo alias ARG_STATIC", path);

    if (mciSendStringA(command, NULL, 0, NULL) == 0) {
        snprintf(command, sizeof(command),
                 "play ARG_STATIC from 0 to %lu",
                 (unsigned long) milliseconds);
        mciSendStringA(command, NULL, 0, NULL);
        startStaticVisual(milliseconds);
    }
}

void playStaticOverlay(void) {
    playStaticOverlayFor(50);
}

void startGlitchEffects(void) {
    if (g_overlay == NULL) {
        return;
    }

    g_glitchesEnabled = TRUE;
    g_glitchSeed =
            ((unsigned int) GetTickCount()) ^
            ((unsigned int) GetCurrentProcessId());

    SetTimer(
        g_overlay,
        ARG_GLITCH_TIMER_ID,
        180,
        NULL
    );
}

LRESULT CALLBACK mouseHookProc(
    int code,
    WPARAM message,
    LPARAM parameter
) {
    if (code == HC_ACTION &&
        message == WM_MOUSEMOVE &&
        g_mouseInversionActive) {
        MSLLHOOKSTRUCT *mouse =
                (MSLLHOOKSTRUCT *) parameter;

        if (g_ignoreMouseMove) {
            g_ignoreMouseMove = FALSE;
            g_lastMousePosition = mouse->pt;

            return CallNextHookEx(
                g_mouseHook,
                code,
                message,
                parameter
            );
        }

        LONG deltaX = mouse->pt.x - g_lastMousePosition.x;
        LONG deltaY = mouse->pt.y - g_lastMousePosition.y;
        POINT inverted = {
            g_lastMousePosition.x - deltaX,
            g_lastMousePosition.y - deltaY
        };

        int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int right = left + GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1;
        int bottom = top + GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1;

        if (inverted.x < left) inverted.x = left;
        if (inverted.x > right) inverted.x = right;
        if (inverted.y < top) inverted.y = top;
        if (inverted.y > bottom) inverted.y = bottom;

        g_ignoreMouseMove = TRUE;
        SetCursorPos(inverted.x, inverted.y);
        g_lastMousePosition = inverted;

        return 1;
    }

    return CallNextHookEx(
        g_mouseHook,
        code,
        message,
        parameter
    );
}

void stopMouseInversion(void) {
    g_mouseInversionActive = FALSE;
    g_mouseInversionEndsAt = 0;
    g_ignoreMouseMove = FALSE;

    if (g_mouseHook != NULL) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = NULL;
    }

    writeSessionMemory("active");
}

void startMouseInversion(
    DWORD duration
) {
    if (!g_mouseInversionAllowed ||
        g_mouseInversionActive) {
        return;
    }

    GetCursorPos(&g_lastMousePosition);
    g_mouseHook = SetWindowsHookExA(
        WH_MOUSE_LL,
        mouseHookProc,
        GetModuleHandleA(NULL),
        0
    );

    if (g_mouseHook == NULL) {
        return;
    }

    g_mouseInversionActive = TRUE;
    g_mouseInversionEndsAt = GetTickCount() + duration;
    writeSessionMemory("active");
}

LRESULT CALLBACK keyboardHookProc(
    int code,
    WPARAM message,
    LPARAM parameter
) {
    if (code == HC_ACTION && g_keyboardBlockActive &&
        (message == WM_KEYDOWN || message == WM_SYSKEYDOWN ||
         message == WM_KEYUP || message == WM_SYSKEYUP)) {
        KBDLLHOOKSTRUCT *keyboard = (KBDLLHOOKSTRUCT *) parameter;

        if (keyboard->vkCode != VK_F10) {
            return 1;
        }
    }

    return CallNextHookEx(g_keyboardHook, code, message, parameter);
}

void stopKeyboardBlock(void) {
    g_keyboardBlockActive = FALSE;
    g_keyboardBlockEndsAt = 0;

    if (g_keyboardHook != NULL) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = NULL;
    }

    writeSessionMemory("active");
}

void startKeyboardBlock(DWORD duration) {
    if (!g_keyboardBlockAllowed || g_keyboardBlockActive) {
        return;
    }

    g_keyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, keyboardHookProc,
                                       GetModuleHandleA(NULL), 0);
    if (g_keyboardHook == NULL) {
        return;
    }

    g_keyboardBlockActive = TRUE;
    g_keyboardBlockEndsAt = GetTickCount() + duration;
    writeSessionMemory("active");
}

void updateKeyboardBlock(DWORD now) {
    if (!g_keyboardBlockAllowed) {
        return;
    }

    if (g_keyboardBlockActive &&
        (LONG) (now - g_keyboardBlockEndsAt) >= 0) {
        stopKeyboardBlock();
        g_nextKeyboardBlockAt = now + 4000 + nextGlitchRandom() % 10001;
    } else if (!g_keyboardBlockActive &&
               (LONG) (now - g_nextKeyboardBlockAt) >= 0) {
        startKeyboardBlock(900 + nextGlitchRandom() % 2101);
    }
}

void startSplitOverlay(void) {
    g_splitOverlayActive = TRUE;

    SetLayeredWindowAttributes(
        g_overlay,
        0,
        48,
        LWA_ALPHA
    );

    InvalidateRect(g_overlay, NULL, FALSE);
}

void stopSplitOverlay(void) {
    g_splitOverlayActive = FALSE;

    if (g_glitchStyle == 0 && g_overlay != NULL) {
        SetLayeredWindowAttributes(
            g_overlay,
            0,
            0,
            LWA_ALPHA
        );
    }
}

void stopStep6(void) {
    g_step6Active = FALSE;

    if (g_smileRegion != NULL) {
        DeleteObject(g_smileRegion);
        g_smileRegion = NULL;
    }

    if (g_smileFont != NULL) {
        DeleteObject(g_smileFont);
        g_smileFont = NULL;
    }

    if (g_overlay != NULL) {
        SetLayeredWindowAttributes(
            g_overlay,
            0,
            0,
            LWA_ALPHA
        );
    }
}

BOOL startStep6(void) {
    RECT clientRect;
    HDC overlay;
    int width;
    int height;

    if (g_overlay == NULL || !GetClientRect(g_overlay, &clientRect)) {
        return FALSE;
    }

    width = clientRect.right - clientRect.left;
    height = clientRect.bottom - clientRect.top;
    overlay = GetDC(g_overlay);

    if (overlay == NULL || width <= 0 || height <= 0) {
        return FALSE;
    }

    g_smileFont = CreateFontA(
        height / 2,
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        FF_DONTCARE,
        "Arial"
    );

    if (g_smileFont == NULL) {
        ReleaseDC(g_overlay, overlay);
        return FALSE;
    }

    HFONT previous = SelectObject(overlay, g_smileFont);
    SIZE size;
    GetTextExtentPoint32A(overlay, ":)", 2, &size);
    int y = (height - size.cy) / 2;
    int leftX = width / 4 - size.cx / 2;
    int rightX = width * 3 / 4 - size.cx / 2;

    BeginPath(overlay);
    TextOutA(overlay, leftX, y, ":)", 2);
    EndPath(overlay);
    g_smileRegion = PathToRegion(overlay);

    BeginPath(overlay);
    TextOutA(overlay, rightX, y, ":)", 2);
    EndPath(overlay);
    HRGN rightRegion = PathToRegion(overlay);

    if (g_smileRegion != NULL && rightRegion != NULL) {
        CombineRgn(g_smileRegion, g_smileRegion, rightRegion, RGN_OR);
    } else {
        if (g_smileRegion != NULL) {
            DeleteObject(g_smileRegion);
            g_smileRegion = NULL;
        }
    }

    if (rightRegion != NULL) {
        DeleteObject(rightRegion);
    }

    SelectObject(overlay, previous);
    ReleaseDC(g_overlay, overlay);

    g_step6BlockedClicks = 0;
    g_step6Active = g_smileRegion != NULL;

    SetLayeredWindowAttributes(
        g_overlay,
        RGB(255, 0, 255),
        255,
        LWA_COLORKEY
    );
    InvalidateRect(g_overlay, NULL, FALSE);

    return g_step6Active;
}

void paintGlitch(
    HDC deviceContext,
    const RECT *clientRect
) {
    if (g_staticVisible) {
        paintStaticVisual(deviceContext, clientRect);
        return;
    }

    if (g_step6Active) {
        HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
        HFONT previous = SelectObject(deviceContext, g_smileFont);
        SIZE size;

        FillRect(deviceContext, clientRect, black);
        DeleteObject(black);
        GetTextExtentPoint32A(deviceContext, ":)", 2, &size);
        SetTextColor(deviceContext, RGB(255, 0, 255));
        SetBkMode(deviceContext, TRANSPARENT);
        TextOutA(deviceContext,
                 clientRect->right / 4 - size.cx / 2,
                 (clientRect->bottom - size.cy) / 2,
                 ":)", 2);
        TextOutA(deviceContext,
                 clientRect->right * 3 / 4 - size.cx / 2,
                 (clientRect->bottom - size.cy) / 2,
                 ":)", 2);
        SelectObject(deviceContext, previous);
        return;
    }

    if (g_transformDC != NULL) {
        int width = clientRect->right - clientRect->left;
        int height = clientRect->bottom - clientRect->top;

        if (g_glitchStyle == 5) {
            BitBlt(deviceContext, 0, 0, width, height, g_transformDC, 0, 0,
                   SRCCOPY);
            PatBlt(deviceContext, 0, 0, width, height, DSTINVERT);
        } else if (g_glitchStyle == 6) {
            POINT points[] = {{width, height}, {0, height}, {width, 0}};
            PlgBlt(deviceContext, points, g_transformDC, 0, 0,
                   g_transformWidth, g_transformHeight, NULL, 0, 0);
        } else {
            POINT points[] = {{width, 0}, {width, height}, {0, 0}};
            PlgBlt(deviceContext, points, g_transformDC, 0, 0,
                   g_transformWidth, g_transformHeight, NULL, 0, 0);
        }

        return;
    }

    if (deviceContext == NULL ||
        clientRect == NULL ||
        (g_glitchStyle == 0 &&
         !g_splitOverlayActive)) {
        return;
    }

    int width = clientRect->right - clientRect->left;
    int height = clientRect->bottom - clientRect->top;

    if (width <= 0 || height <= 0) {
        return;
    }

    if (g_splitOverlayActive) {
        RECT vertical = {
            width / 2 - 6,
            0,
            width / 2 + 6,
            height
        };
        RECT horizontal = {
            0,
            height / 2 - 6,
            width,
            height / 2 + 6
        };
        HBRUSH divider = CreateSolidBrush(RGB(255, 255, 255));

        if (divider != NULL) {
            FillRect(deviceContext, &vertical, divider);
            FillRect(deviceContext, &horizontal, divider);
            DeleteObject(divider);
        }
    }

    if (g_glitchStyle == 0) {
        return;
    }

    for (int index = 0; index < 16; index++) {
        RECT block;

        block.left = nextGlitchRandom() % width;
        block.top = nextGlitchRandom() % height;
        block.right = block.left +
                      30 + nextGlitchRandom() % 220;
        block.bottom = block.top +
                       12 + nextGlitchRandom() % 100;

        if (block.right > width) {
            block.right = width;
        }

        if (block.bottom > height) {
            block.bottom = height;
        }

        COLORREF color = RGB(0, 0, 0);

        if (g_glitchStyle == 2) {
            color = RGB(
                96 + nextGlitchRandom() % 160,
                nextGlitchRandom() % 96,
                96 + nextGlitchRandom() % 160
            );
        } else if (g_glitchStyle == 3) {
            int noise = nextGlitchRandom() % 256;

            color = RGB(noise, noise, noise);
        } else if (g_glitchStyle == 4) {
            color = RGB(255, 255, 255);
        }

        HBRUSH brush = CreateSolidBrush(color);

        if (brush != NULL) {
            FillRect(
                deviceContext,
                &block,
                brush
            );

            DeleteObject(brush);
        }
    }
}

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (message) {
        case WM_NCHITTEST:
            if (g_step6Active) {
                POINT point;

                GetCursorPos(&point);
                ScreenToClient(hwnd, &point);

                return PtInRegion(
                           g_smileRegion,
                           point.x,
                           point.y
                       )
                           ? HTTRANSPARENT
                           : HTCLIENT;
            }

            return HTTRANSPARENT;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(
                hwnd,
                &paint
            );

            paintGlitch(
                deviceContext,
                &paint.rcPaint
            );

            EndPaint(
                hwnd,
                &paint
            );

            return 0;
        }

        case WM_TIMER:

            if (wParam == ARG_STATIC_TIMER_ID && g_staticVisible) {
                DWORD now = GetTickCount();

                if ((LONG) (now - g_staticEndsAt) >= 0) {
                    stopStaticVisual();
                } else if (g_staticImage != NULL) {
                    GdipImageRotateFlip((GpImage *) g_staticImage,
                                        Rotate180FlipNone);
                    InvalidateRect(hwnd, NULL, FALSE);
                }

                return 0;
            }

            if (wParam == ARG_GLITCH_TIMER_ID &&
                g_glitchesEnabled &&
                !g_shutdownRequested) {
                DWORD now = GetTickCount();

                if (g_glitchStyle != 0 &&
                    (LONG) (now - g_glitchEndsAt) >= 0) {
                    g_glitchStyle = 0;
                    releaseTransformScreen();

                    SetLayeredWindowAttributes(
                        hwnd,
                        0,
                        0,
                        LWA_ALPHA
                    );
                } else if (g_glitchStyle == 0 &&
                           (nextGlitchRandom() % 14) == 0) {
                    g_glitchStyle = 1 + nextGlitchRandom() % 7;
                    g_glitchEndsAt =
                            now + 800 +
                            nextGlitchRandom() % 2201;

                    if (g_glitchStyle >= 5 && !captureTransformScreen()) {
                        g_glitchStyle = 1;
                    }

                    SetLayeredWindowAttributes(hwnd, 0,
                                               g_glitchStyle >= 5 ? 255 : 72,
                                               LWA_ALPHA);
                }

                InvalidateRect(
                    hwnd,
                    NULL,
                    FALSE
                );
            }

            return 0;

        case WM_HOTKEY:

            if (wParam ==
                ARG_HOTKEY_ID) {
                g_shutdownRequested =
                        TRUE;
                g_audioThreadStop = TRUE;

                PostMessageA(
                    hwnd,
                    WM_CLOSE,
                    0,
                    0
                );
            }

            return 0;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wParam == VK_F10) {
                g_shutdownRequested = TRUE;
                g_audioThreadStop = TRUE;
                PostMessageA(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;

        case WM_LBUTTONDOWN:
            if (g_step6Active) {
                g_step6BlockedClicks++;
                sayStep6Warning();

                if (g_step6BlockedClicks >= 7) {
                    stopStep6();
                    g_step6Complete = TRUE;
                }
            }

            return 0;

        case WM_CLOSE:

            g_shutdownRequested =
                    TRUE;
            stopAudioThread();

            stopGlitchEffects();
            stopSplitOverlay();
            stopStep6();
            stopMP3();
            stopStaticOverlay();
            releaseStaticImage();
            removeInstalledTreeStartup();
            restoreWallpaper();
            stopMouseInversion();
            stopKeyboardBlock();
            stopTts();
            closeAllTextProcesses();

            DestroyWindow(hwnd);

            return 0;

        case WM_DESTROY:

            UnregisterHotKey(hwnd, ARG_HOTKEY_ID);

            PostQuitMessage(0);

            return 0;
    }

    return DefWindowProcA(
        hwnd,
        message,
        wParam,
        lParam
    );
}

BOOL createOverlay(
    HINSTANCE hInstance
) {
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
            ARG_WINDOW_CLASS;

    if (!RegisterClassExA(
        &wc
    )) {
        return FALSE;
    }

    int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    DWORD extendedStyle =
            WS_EX_LAYERED |
            WS_EX_TOOLWINDOW |
            WS_EX_NOACTIVATE;

    g_overlay =
            CreateWindowExA(
                extendedStyle,
                ARG_WINDOW_CLASS,
                "ARG",
                WS_POPUP,
                left,
                top,
                width,
                height,
                NULL,
                NULL,
                hInstance,
                NULL
            );

    if (g_overlay == NULL) {
        return FALSE;
    }

    SetLayeredWindowAttributes(
        g_overlay,
        0,
        0,
        LWA_ALPHA
    );

    SetWindowPos(
        g_overlay,
        HWND_TOPMOST,
        left,
        top,
        width,
        height,
        SWP_NOACTIVATE |
        SWP_SHOWWINDOW
    );

    return TRUE;
}
