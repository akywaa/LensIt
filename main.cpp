#include "LensIt.h"

AppConfig g_config;
HWND g_hwndOverlay = NULL;
HWND g_hwndSettings = NULL;
HWND g_hwndToast = NULL;
HHOOK g_kbdHook = NULL;
HHOOK g_mouseHook = NULL;
bool g_isTriggerHeld = false;
bool g_isDrawingLine = false;
bool g_isDrawingArrow = false;
bool g_isDrawRectangle = false;
BindingMode g_bindingMode = BindingMode::None;
float g_currentZoom = 1.0f;
float g_targetZoom = 1.0f;
float g_camX = 0.0f;
float g_camY = 0.0f;
int g_stepCounter = 1;
std::vector<Stroke> g_strokes;
Stroke g_currentStroke;
HICON g_appIcon = NULL;
NOTIFYICONDATA g_nid = { sizeof(NOTIFYICONDATA) };
bool g_isDrawingHighlight = false;
bool g_isDrawingBlur = false;
COLORREF g_inkOverride = 0;
bool g_inkOverrideSet = false;
bool g_persistentDrawingsActive = false;
ActiveToolMode g_activeToolMode = ActiveToolMode::None;
BoardMode g_boardMode = BoardMode::None;
bool g_cropMode = false;
bool g_cropDragging = false;
POINT g_cropStart = { 0, 0 };
POINT g_cropEnd = { 0, 0 };
bool g_laserMode = false;
bool g_spotlightMode = false;
bool g_keycastEnabled = true;
bool g_isTextInputActive = false;
Stroke g_textDraft;

typedef BOOL(WINAPI* pfnMagSetFullscreenUseBitmapSmoothing)(BOOL);
typedef BOOL(WINAPI* pfnMagSetFullscreenWindowFilterList)(DWORD, int, HWND*);

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
    (void)hInstance;
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAYMSG;
    g_nid.hIcon = g_appIcon;
    wcscpy_s(g_nid.szTip, L"LensIt");
    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

void StartZoomTimer() {
    if (g_hwndOverlay) SetTimer(g_hwndOverlay, 1, 14, NULL);
}

void StopZoomTimer() {
    ProcessOverlayFrame();
    if (g_hwndOverlay) KillTimer(g_hwndOverlay, 1);
}

bool RequiresZoomTimer() {
    if (g_isDrawingLine || g_isDrawingArrow || g_isDrawRectangle || g_isDrawingHighlight || g_isDrawingBlur) return true;
    if (fabsf(g_currentZoom - g_targetZoom) > 0.001f) return true;
    return false;
}

int APIENTRY WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    HANDLE hSingleInstanceMutex = CreateMutexW(NULL, TRUE, L"LensIt_SingleInstance_Mutex");
    if (hSingleInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"LensItOverlay", NULL);
        if (hExisting) {
            PostMessageW(hExisting, WM_APP_SHOWSETTINGS, 0, 0);
        }
        if (hSingleInstanceMutex) {
            ReleaseMutex(hSingleInstanceMutex);
            CloseHandle(hSingleInstanceMutex);
        }
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT hrCo = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    LoadAppIcon(hInstance);
    LoadConfig();

    if (!MagInitialize()) {
        GdiplusShutdown(gdiplusToken);
        if (SUCCEEDED(hrCo)) CoUninitialize();
        return 1;
    }

    MagShowSystemCursor(TRUE);

    HMODULE hMag = GetModuleHandle(L"magnification.dll");
    if (hMag) {
        auto pSetSmoothing = (pfnMagSetFullscreenUseBitmapSmoothing)GetProcAddress(hMag, "MagSetFullscreenUseBitmapSmoothing");
        if (pSetSmoothing) pSetSmoothing(TRUE);
    }

    WNDCLASSEX wcOverlay = { sizeof(WNDCLASSEX), 0, OverlayWndProc, 0, 0, hInstance, g_appIcon, NULL, (HBRUSH)GetStockObject(BLACK_BRUSH), NULL, L"LensItOverlay", g_appIcon };
    RegisterClassEx(&wcOverlay);

    WNDCLASSEX wcSettings = { sizeof(WNDCLASSEX), 0, SettingsWndProc, 0, 0, hInstance, g_appIcon, LoadCursor(NULL, IDC_ARROW), NULL, NULL, L"LensItSettings", g_appIcon };
    RegisterClassEx(&wcSettings);

    WNDCLASSEX wcWelcome = { sizeof(WNDCLASSEX), 0, WelcomeWndProc, 0, 0, hInstance, g_appIcon, LoadCursor(NULL, IDC_ARROW), NULL, NULL, L"LensItWelcome", g_appIcon };
    RegisterClassEx(&wcWelcome);

    WNDCLASSEX wcToast = { sizeof(WNDCLASSEX), 0, ToastWndProc, 0, 0, hInstance, NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, L"LensItToast", NULL };
    RegisterClassEx(&wcToast);

    InitToastWindow(hInstance);

    int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    g_hwndOverlay = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"LensItOverlay", L"Overlay", WS_POPUP, vScreenX, vScreenY, screenW, screenH, NULL, NULL, hInstance, NULL
    );

    SetWindowDisplayAffinity(g_hwndOverlay, WDA_EXCLUDEFROMCAPTURE);

    if (hMag) {
        auto pSetFilter = (pfnMagSetFullscreenWindowFilterList)GetProcAddress(hMag, "MagSetFullscreenWindowFilterList");
        if (pSetFilter) {
            HWND hExclude[1] = { g_hwndOverlay };
            pSetFilter(MW_FILTERMODE_EXCLUDE, 1, hExclude);
        }
    }

    ShowWindow(g_hwndOverlay, SW_HIDE);

    RepositionOverlay();
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
    InitTray(g_hwndOverlay, hInstance);

    if (g_config.isFirstRun) {
        ShowWelcomeWindow(hInstance);
    }

    g_kbdHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Shell_NotifyIcon(NIM_DELETE, &g_nid);
    if (g_kbdHook) UnhookWindowsHookEx(g_kbdHook);
    if (g_mouseHook) UnhookWindowsHookEx(g_mouseHook);

    if (g_hwndToast) DestroyWindow(g_hwndToast);
    DestroyOverlayBackbuffer();
    MagSetFullscreenTransform(1.0f, 0, 0);
    MagUninitialize();
    GdiplusShutdown(gdiplusToken);
    if (SUCCEEDED(hrCo)) CoUninitialize();
    if (g_appIcon) DestroyIcon(g_appIcon);
    SaveConfig();
    if (hSingleInstanceMutex) {
        ReleaseMutex(hSingleInstanceMutex);
        CloseHandle(hSingleInstanceMutex);
    }
    return 0;
}