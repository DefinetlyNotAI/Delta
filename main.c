#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static const char CLASS_NAME[] = "ARGOverlayWindow";

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (uMsg) {
        case WM_NCHITTEST:
            // Make the overlay completely click-through.
            return HTTRANSPARENT;

        case WM_ERASEBKGND:
            return 1;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Register the window class.
    WNDCLASSEXA wc = {
        0
    };

    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassExA(&wc)) {
        return 1;
    }

    // Get the primary monitor dimensions.
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    /*
     * WS_EX_TOOLWINDOW:
     * Prevents the window from appearing in the taskbar.
     *
     * WS_EX_LAYERED:
     * Allows the window to have transparency.
     *
     * WS_EX_TRANSPARENT:
     * Makes the window visually/input transparent to underlying
     * windows in the appropriate circumstances.
     *
     * WS_EX_NOACTIVATE:
     * Prevents the overlay from taking keyboard focus.
     */
    DWORD exStyle =
        WS_EX_LAYERED |
        WS_EX_TOOLWINDOW |
        WS_EX_TRANSPARENT |
        WS_EX_NOACTIVATE;

    HWND hwnd = CreateWindowExA(
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

    if (!hwnd) {
        return 1;
    }

    /*
     * Make the entire window completely transparent.

     * Alpha = 0 means nothing is rendered.
     * Later we can change this to draw the ARG effects.
     */
    SetLayeredWindowAttributes(
        hwnd,
        0,
        0,
        LWA_ALPHA
    );

    /*
     * Put the overlay above normal windows without activating it.
     */
    SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        0,
        0,
        screenWidth,
        screenHeight,
        SWP_NOACTIVATE | SWP_SHOWWINDOW
    );

    /*
     * Message loop.
     *
     * The program now effectively does nothing until it receives
     * a Windows message.
     */
    MSG msg;

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return 0;
}
