#include "LensIt.h"
#include <windowsx.h>

namespace {

constexpr int kWindowWidth = 400;
constexpr int kWindowHeight = 420;
constexpr DWORD kDwmImmersiveDarkModeAttribute = 20;

constexpr int kMargin = 30;
constexpr int kTitleTop = 20;
constexpr int kSubtitleTop = 52;

constexpr int kCardTop = 85;
constexpr int kCardWidth = 340;
constexpr int kCardHeight = 180;
constexpr int kCardTextLeft = 42;
constexpr int kCardHeaderTop = 95;
constexpr int kCardBodyTop = 122;

constexpr int kRowRight = 360;
constexpr int kRowAutoTop = 280;
constexpr int kRowAutoBoxTop = 282;
constexpr int kRowShortcutTop = 312;
constexpr int kRowShortcutBoxTop = 314;
constexpr int kRowGap = 4;

constexpr int kCheckboxSize = 18;
constexpr int kCheckInset = 3;
constexpr int kCheckInsetSize = 12;
constexpr int kCheckMarkInsetLeft = 4;
constexpr int kCheckMarkInsetMid = 8;
constexpr int kCheckMarkInsetRight = 15;
constexpr int kCheckMarkTop = 6;
constexpr int kCheckMarkMidTop = 9;
constexpr int kCheckMarkBottom = 14;
constexpr int kCheckboxLabelGap = 10;

constexpr int kButtonLeft = 30;
constexpr int kButtonTop = 355;
constexpr int kButtonRight = 370;
constexpr int kButtonBottom = 395;

constexpr float kTitleFontSize = 16.0f;
constexpr float kHeaderFontSize = 11.0f;
constexpr float kBodyFontSize = 9.5f;

}

static HWND g_hwndWelcome = NULL;
static bool s_chkAutoStart = true;
static bool s_chkCreateShortcut = true;
static float g_uiScale = 1.0f;

static RECT rectChkAuto = { kMargin, kRowAutoTop, kRowRight, kRowAutoTop + kCheckboxSize + kRowGap };
static RECT rectChkAutoBox = { kMargin, kRowAutoBoxTop, kMargin + kCheckboxSize, kRowAutoBoxTop + kCheckboxSize };

static RECT rectChkShort = { kMargin, kRowShortcutTop, kRowRight, kRowShortcutTop + kCheckboxSize + kRowGap };
static RECT rectChkShortBox = { kMargin, kRowShortcutBoxTop, kMargin + kCheckboxSize, kRowShortcutBoxTop + kCheckboxSize };

static RECT btnStart = { kButtonLeft, kButtonTop, kButtonRight, kButtonBottom };

static bool PtInRectCustom(RECT r, int x, int y) {
    return (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom);
}

static void DrawCheckbox(Graphics& g, const RECT& box, bool checked) {
    SolidBrush chkBg(Color(36, 36, 36));
    Pen borderPen(Color(80, 80, 80), 1.0f);

    g.FillRectangle(&chkBg, box.left, box.top, kCheckboxSize, kCheckboxSize);
    g.DrawRectangle(&borderPen, box.left, box.top, kCheckboxSize, kCheckboxSize);
    if (!checked) return;

    SolidBrush accentBlue(Color(0, 122, 204));
    g.FillRectangle(&accentBlue, box.left + kCheckInset, box.top + kCheckInset, kCheckInsetSize, kCheckInsetSize);

    Pen checkPen(Color(255, 255, 255), 2.0f);
    Point pts[3] = {
        Point(box.left + kCheckMarkInsetLeft, box.top + kCheckMarkMidTop),
        Point(box.left + kCheckMarkInsetMid, box.top + kCheckMarkBottom),
        Point(box.left + kCheckMarkInsetRight, box.top + kCheckMarkTop)
    };
    g.DrawLines(&checkPen, pts, 3);
}

static void DrawWelcomeUI(Graphics& g) {
    g.Clear(Color(255, 20, 20, 20));
    if (g_uiScale != 1.0f) g.ScaleTransform(g_uiScale, g_uiScale);

    Font fontTitle(L"Segoe UI", kTitleFontSize, FontStyleBold);
    Font fontHeader(L"Segoe UI", kHeaderFontSize, FontStyleBold);
    Font fontReg(L"Segoe UI", kBodyFontSize);
    Font fontBold(L"Segoe UI", kBodyFontSize, FontStyleBold);

    SolidBrush textWhite(Color(240, 240, 240));
    SolidBrush textDim(Color(160, 160, 160));
    SolidBrush accentBlue(Color(0, 122, 204));
    SolidBrush cardBg(Color(30, 30, 30));
    Pen cardBorder(Color(50, 50, 50), 1.0f);

    g.DrawString(L"Welcome to LensIt!", -1, &fontTitle, PointF(static_cast<REAL>(kMargin), static_cast<REAL>(kTitleTop)), &textWhite);
    g.DrawString(L"Compact screen magnifier and on-screen annotations", -1, &fontReg, PointF(static_cast<REAL>(kMargin), static_cast<REAL>(kSubtitleTop)), &textDim);

    g.FillRectangle(&cardBg, kMargin, kCardTop, kCardWidth, kCardHeight);
    g.DrawRectangle(&cardBorder, kMargin, kCardTop, kCardWidth, kCardHeight);

    g.DrawString(L"How to use:", -1, &fontHeader, PointF(static_cast<REAL>(kCardTextLeft), static_cast<REAL>(kCardHeaderTop)), &accentBlue);

    std::wstring shortcuts =
        L"* Alt + Mouse Wheel - Smooth zoom\n"
        L"* Alt + LMB - Draw line | RMB - Arrow\n"
        L"* Alt + Shift + LMB - Draw rectangle\n"
        L"* Alt + MMB (Wheel click) - Step Badge (1,2,3)\n"
        L"* Alt + H / O - one-shot Highlight / Blur\n"
        L"* Alt + R/G/B/Y - quick color switch\n"
        L"* Alt + Z - Undo | Alt + C - Copy screenshot\n"
        L"* Esc - Reset zoom & drawings";

    g.DrawString(shortcuts.c_str(), -1, &fontReg, PointF(static_cast<REAL>(kCardTextLeft), static_cast<REAL>(kCardBodyTop)), &textWhite);

    DrawCheckbox(g, rectChkAutoBox, s_chkAutoStart);
    g.DrawString(L"Start automatically with Windows", -1, &fontReg,
        PointF(static_cast<REAL>(rectChkAutoBox.right) + kCheckboxLabelGap, static_cast<REAL>(rectChkAutoBox.top)), &textWhite);

    DrawCheckbox(g, rectChkShortBox, s_chkCreateShortcut);
    g.DrawString(L"Create Desktop shortcut", -1, &fontReg,
        PointF(static_cast<REAL>(rectChkShortBox.right) + kCheckboxLabelGap, static_cast<REAL>(rectChkShortBox.top)), &textWhite);

    g.FillRectangle(&accentBlue, static_cast<int>(btnStart.left), static_cast<int>(btnStart.top), static_cast<int>(btnStart.right - btnStart.left), static_cast<int>(btnStart.bottom - btnStart.top));
    StringFormat fmtCenter;
    fmtCenter.SetAlignment(StringAlignmentCenter);
    fmtCenter.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"Get Started", -1, &fontBold,
        RectF(static_cast<REAL>(btnStart.left), static_cast<REAL>(btnStart.top), static_cast<REAL>(btnStart.right - btnStart.left), static_cast<REAL>(btnStart.bottom - btnStart.top)),
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
        int x = static_cast<int>(GET_X_LPARAM(lParam) / g_uiScale), y = static_cast<int>(GET_Y_LPARAM(lParam) / g_uiScale);

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
    case WM_CLOSE:
        g_config.isFirstRun = false;
        SaveConfig();
        DestroyWindow(hwnd);
        return 0;

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

    g_hwndWelcome = CreateWindowEx(
        WS_EX_TOPMOST, L"LensItWelcome", L"Welcome to LensIt",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, kWindowWidth, kWindowHeight, NULL, NULL, hInstance, NULL
    );

    UINT dpi = GetDpiForWindow(g_hwndWelcome);
    g_uiScale = (dpi > 0) ? (static_cast<float>(dpi) / 96.0f) : 1.0f;

    RECT wr = { 0, 0, static_cast<int>(kWindowWidth * g_uiScale), static_cast<int>(kWindowHeight * g_uiScale) };
    AdjustWindowRectExForDpi(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOPMOST, dpi);
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;

    int cx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int cy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    SetWindowPos(g_hwndWelcome, HWND_TOPMOST, cx, cy, w, h, SWP_NOACTIVATE);

    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_hwndWelcome, kDwmImmersiveDarkModeAttribute, &dark, sizeof(dark));

    ShowWindow(g_hwndWelcome, SW_SHOW);
    UpdateWindow(g_hwndWelcome);
}
