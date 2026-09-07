#include "LensIt.h"

static HWND g_hwndWelcome = NULL;
static bool s_chkAutoStart = true;
static bool s_chkCreateShortcut = true;
static float g_uiScale = 1.0f;

static RECT rectChkAuto = { 30, 280, 360, 302 };
static RECT rectChkAutoBox = { 30, 282, 48, 300 };

static RECT rectChkShort = { 30, 312, 360, 334 };
static RECT rectChkShortBox = { 30, 314, 48, 332 };

static RECT btnStart = { 30, 355, 370, 395 };

static bool PtInRectCustom(RECT r, int x, int y) {
    return (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom);
}

static void DrawWelcomeUI(Graphics& g) {
    g.Clear(Color(255, 20, 20, 20));
    if (g_uiScale != 1.0f) g.ScaleTransform(g_uiScale, g_uiScale);

    Font fontTitle(L"Segoe UI", 16, FontStyleBold);
    Font fontHeader(L"Segoe UI", 11, FontStyleBold);
    Font fontReg(L"Segoe UI", 9.5f);
    Font fontBold(L"Segoe UI", 9.5f, FontStyleBold);

    SolidBrush textWhite(Color(240, 240, 240));
    SolidBrush textDim(Color(160, 160, 160));
    SolidBrush accentBlue(Color(0, 122, 204));
    SolidBrush cardBg(Color(30, 30, 30));
    SolidBrush chkBg(Color(36, 36, 36));
    Pen borderPen(Color(80, 80, 80), 1.0f);
    Pen cardBorder(Color(50, 50, 50), 1.0f);

    g.DrawString(L"Welcome to LensIt!", -1, &fontTitle, PointF(30, 20), &textWhite);
    g.DrawString(L"Compact screen magnifier and on-screen annotations", -1, &fontReg, PointF(30, 52), &textDim);

    g.FillRectangle(&cardBg, 30, 85, 340, 180);
    g.DrawRectangle(&cardBorder, 30, 85, 340, 180);

    g.DrawString(L"How to use:", -1, &fontHeader, PointF(42, 95), &accentBlue);

    std::wstring shortcuts =
        L"* Alt + Mouse Wheel - Smooth zoom\n"
        L"* Alt + LMB - Draw line | RMB - Arrow\n"
        L"* Alt + Shift + LMB - Draw rectangle\n"
        L"* Alt + MMB (Wheel click) - Step Badge (1,2,3)\n"
        L"* Alt + H / O - one-shot Highlight / Blur\n"
        L"* Alt + R/G/B/Y - quick color switch\n"
        L"* Alt + Z - Undo | Alt + C - Copy screenshot\n"
        L"* Esc - Reset zoom & drawings";

    g.DrawString(shortcuts.c_str(), -1, &fontReg, PointF(42, 122), &textWhite);

    g.FillRectangle(&chkBg, (int)rectChkAutoBox.left, (int)rectChkAutoBox.top, 18, 18);
    g.DrawRectangle(&borderPen, (int)rectChkAutoBox.left, (int)rectChkAutoBox.top, 18, 18);
    if (s_chkAutoStart) {
        g.FillRectangle(&accentBlue, (int)rectChkAutoBox.left + 3, (int)rectChkAutoBox.top + 3, 12, 12);
        Pen checkPen(Color(255, 255, 255), 2.0f);
        Point pts[3] = {
            Point((int)rectChkAutoBox.left + 4, (int)rectChkAutoBox.top + 9),
            Point((int)rectChkAutoBox.left + 8, (int)rectChkAutoBox.top + 14),
            Point((int)rectChkAutoBox.left + 15, (int)rectChkAutoBox.top + 6)
        };
        g.DrawLines(&checkPen, pts, 3);
    }
    g.DrawString(L"Start automatically with Windows", -1, &fontReg,
        PointF((REAL)rectChkAutoBox.right + 10, (REAL)rectChkAutoBox.top), &textWhite);

    g.FillRectangle(&chkBg, (int)rectChkShortBox.left, (int)rectChkShortBox.top, 18, 18);
    g.DrawRectangle(&borderPen, (int)rectChkShortBox.left, (int)rectChkShortBox.top, 18, 18);
    if (s_chkCreateShortcut) {
        g.FillRectangle(&accentBlue, (int)rectChkShortBox.left + 3, (int)rectChkShortBox.top + 3, 12, 12);
        Pen checkPen(Color(255, 255, 255), 2.0f);
        Point pts[3] = {
            Point((int)rectChkShortBox.left + 4, (int)rectChkShortBox.top + 9),
            Point((int)rectChkShortBox.left + 8, (int)rectChkShortBox.top + 14),
            Point((int)rectChkShortBox.left + 15, (int)rectChkShortBox.top + 6)
        };
        g.DrawLines(&checkPen, pts, 3);
    }
    g.DrawString(L"Create Desktop shortcut", -1, &fontReg,
        PointF((REAL)rectChkShortBox.right + 10, (REAL)rectChkShortBox.top), &textWhite);

    g.FillRectangle(&accentBlue, (int)btnStart.left, (int)btnStart.top, (int)(btnStart.right - btnStart.left), (int)(btnStart.bottom - btnStart.top));
    StringFormat fmtCenter;
    fmtCenter.SetAlignment(StringAlignmentCenter);
    fmtCenter.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"Get Started", -1, &fontBold,
        RectF((REAL)btnStart.left, (REAL)btnStart.top, (REAL)(btnStart.right - btnStart.left), (REAL)(btnStart.bottom - btnStart.top)),
        &fmtCenter, &textWhite);
}

LRESULT CALLBACK WelcomeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        DrawWelcomeUI(g);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = (int)(LOWORD(lParam) / g_uiScale), y = (int)(HIWORD(lParam) / g_uiScale);

        if (PtInRectCustom(rectChkAuto, x, y)) {
            s_chkAutoStart = !s_chkAutoStart;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCustom(rectChkShort, x, y)) {
            s_chkCreateShortcut = !s_chkCreateShortcut;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCustom(btnStart, x, y)) {
            if (s_chkAutoStart) SetAutoStart(true);
            if (s_chkCreateShortcut) CreateDesktopShortcut();

            g_config.isFirstRun = false;
            SaveConfig();

            g_nid.uFlags |= NIF_INFO;
            wcscpy_s(g_nid.szInfoTitle, L"LensIt is running!");
            wcscpy_s(g_nid.szInfo, L"Minimized to tray. Hold Alt and scroll wheel to zoom.");
            g_nid.dwInfoFlags = NIIF_INFO;
            Shell_NotifyIcon(NIM_MODIFY, &g_nid);
            g_nid.uFlags &= ~NIF_INFO;

            DestroyWindow(hwnd);
        }
        return 0;
    }
    case WM_DESTROY:
        g_hwndWelcome = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ShowWelcomeWindow(HINSTANCE hInstance) {
    if (g_hwndWelcome) {
        SetForegroundWindow(g_hwndWelcome);
        return;
    }

    UINT dpi = GetDpiForSystem();
    g_uiScale = (dpi > 0) ? ((float)dpi / 96.0f) : 1.0f;

    RECT wr = { 0, 0, (int)(400 * g_uiScale), (int)(420 * g_uiScale) };
    AdjustWindowRectEx(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOPMOST);
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;

    int cx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int cy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hwndWelcome = CreateWindowEx(
        WS_EX_TOPMOST, L"LensItWelcome", L"Welcome to LensIt",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        cx, cy, w, h, NULL, NULL, hInstance, NULL
    );

    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_hwndWelcome, 20, &dark, sizeof(dark));

    ShowWindow(g_hwndWelcome, SW_SHOW);
    UpdateWindow(g_hwndWelcome);
}
