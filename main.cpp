#include "LensIt.h"

// Global variables
AppConfig g_config;
HWND g_hwndOverlay = NULL;
HWND g_hwndSettings = NULL;
HHOOK g_kbdHook = NULL;
HHOOK g_mouseHook = NULL;
bool g_isTriggerHeld = false;
bool g_isDrawingLine = false;
bool g_isDrawingArrow = false;
bool g_isDrawRectangle = false;
BindingMode g_bindingMode = BindingMode::None;
float g_currentZoom = 1.0f;
float g_camX = 0.0f;
float g_camY = 0.0f;
std::vector<Stroke> g_strokes;
Stroke g_currentStroke;
HICON g_appIcon = NULL;
NOTIFYICONDATA g_nid = { sizeof(NOTIFYICONDATA) };

typedef BOOL(WINAPI* pfnMagSetFullscreenUseBitmapSmoothing)(BOOL);

void LoadAppIcon(HINSTANCE hInstance) {
    int cxSmall = GetSystemMetrics(SM_CXSMICON);
    int cySmall = GetSystemMetrics(SM_CYSMICON);

    g_appIcon = (HICON)LoadImageW(
        hInstance,
        MAKEINTRESOURCE(IDI_APP_ICON),
        IMAGE_ICON,
        cxSmall,
        cySmall,
        LR_DEFAULTCOLOR
    );

    if (!g_appIcon) {
        g_appIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    }
}

void InitTray(HWND hwnd, HINSTANCE hInstance) {
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAYMSG;
    g_nid.hIcon = g_appIcon;
    wcscpy_s(g_nid.szTip, L"LensIt");
    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    LoadAppIcon(hInstance);
    LoadConfig();

    if (!MagInitialize()) return 1;

    HMODULE hMag = GetModuleHandle(L"magnification.dll");
    if (hMag) {
        auto pSetSmoothing = (pfnMagSetFullscreenUseBitmapSmoothing)GetProcAddress(hMag, "MagSetFullscreenUseBitmapSmoothing");
        if (pSetSmoothing) pSetSmoothing(TRUE);
    }

    WNDCLASSEX wcOverlay = { sizeof(WNDCLASSEX), 0, OverlayWndProc, 0, 0, hInstance, g_appIcon, LoadCursor(NULL, IDC_ARROW), CreateSolidBrush(RGB(0, 0, 0)), NULL, L"LensItOverlay", g_appIcon };
    RegisterClassEx(&wcOverlay);

    WNDCLASSEX wcSettings = { sizeof(WNDCLASSEX), 0, SettingsWndProc, 0, 0, hInstance, g_appIcon, LoadCursor(NULL, IDC_ARROW), CreateSolidBrush(RGB(20, 20, 20)), NULL, L"LensItSettings", g_appIcon };
    RegisterClassEx(&wcSettings);

    int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    g_hwndOverlay = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"LensItOverlay", L"Overlay", WS_POPUP, vScreenX, vScreenY, screenW, screenH, NULL, NULL, hInstance, NULL
    );

    SetLayeredWindowAttributes(g_hwndOverlay, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(g_hwndOverlay, SW_SHOW);
    InitTray(g_hwndOverlay, hInstance);

    g_kbdHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);
    SetTimer(g_hwndOverlay, 1, 14, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Shell_NotifyIcon(NIM_DELETE, &g_nid);
    if (g_kbdHook) UnhookWindowsHookEx(g_kbdHook);
    if (g_mouseHook) UnhookWindowsHookEx(g_mouseHook);
    MagSetFullscreenTransform(1.0f, 0, 0);
    MagUninitialize();
    GdiplusShutdown(gdiplusToken);
    if (g_appIcon) DestroyIcon(g_appIcon);
    SaveConfig();
    return 0;
}