#include "LensIt.h"
#include "WinHandles.h"
#include <windowsx.h>

namespace {

enum ControlId : int {
    ID_BTN_TRIGGER_KEY = 1001,
    ID_BTN_FREEZE_KEY,
    ID_BTN_RECT_KEY,
    ID_CHK_RESET_ZOOM,
    ID_CHK_HOLD_LASER,
    ID_CHK_KEEP_DRAWINGS,
    ID_CHK_HIDE_TOASTS,
    ID_CHK_AUTO_START,
    ID_BTN_COLOR_LINE,
    ID_SLIDER_LINE,
    ID_BTN_COLOR_ARROW,
    ID_SLIDER_ARROW,
    ID_BTN_COLOR_RECT,
    ID_SLIDER_RECT,
    ID_BTN_COLOR_BADGE,
    ID_BTN_ALL_SHORTCUTS,
    ID_BTN_CLOSE_SHORTCUTS
};

struct ControlPlacement {
    HWND hwnd = NULL;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

HFONT g_hFontNormal = NULL;
HFONT g_hFontBold = NULL;
HBRUSH g_hbrDarkBg = NULL;
WNDPROC g_origGroupBoxProc = NULL;
HWND g_hwndShortcuts = NULL;

std::vector<ControlPlacement> g_settingsControls;
float g_settingsScale = 1.0f;
int g_settingsScrollPos = 0;
int g_maxSettingsScroll = 0;
int g_totalSettingsContentH = 650;

float g_shortcutsScale = 1.0f;
int g_shortcutsScrollPos = 0;
int g_maxShortcutsScroll = 0;
int g_totalShortcutsContentH = 810;
int g_shortcutsFooterH = 46;

COLORREF PickColorDialog(HWND hwndParent, COLORREF initColor) {
    static COLORREF customColors[16] = {
        RGB(255, 45, 45), RGB(45, 200, 255), RGB(46, 204, 113), RGB(241, 196, 15),
        RGB(155, 89, 182), RGB(230, 126, 34), RGB(52, 73, 94), RGB(255, 255, 255),
        RGB(0, 0, 0), RGB(128, 128, 128), RGB(255, 105, 180), RGB(0, 255, 255),
        RGB(255, 215, 0), RGB(138, 43, 226), RGB(0, 128, 0), RGB(0, 0, 128)
    };
    CHOOSECOLORW cc = { sizeof(CHOOSECOLORW) };
    cc.hwndOwner = hwndParent;
    cc.lpCustColors = customColors;
    cc.rgbResult = initColor;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&cc)) {
        return cc.rgbResult;
    }
    return initColor;
}

void DrawColorButton(LPDRAWITEMSTRUCT dis, COLORREF col) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    HBRUSH bg = CreateSolidBrush(col);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(70, 70, 75));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
}

void DrawDarkButton(LPDRAWITEMSTRUCT dis, bool isBinding) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    wchar_t text[128] = { 0 };
    GetWindowTextW(dis->hwndItem, text, 128);

    bool isPressed = (dis->itemState & ODS_SELECTED) != 0;

    COLORREF bgCol = isBinding ? RGB(20, 48, 76) : (isPressed ? RGB(45, 45, 52) : RGB(36, 36, 42));
    COLORREF borderCol = isBinding ? RGB(0, 160, 255) : (isPressed ? RGB(90, 90, 105) : RGB(62, 62, 72));
    COLORREF textCol = isBinding ? RGB(0, 190, 255) : RGB(235, 235, 240);

    HBRUSH bgBrush = CreateSolidBrush(bgCol);
    HPEN borderPen = CreatePen(PS_SOLID, isBinding ? 2 : 1, borderCol);

    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, bgBrush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textCol);
    SelectObject(hdc, g_hFontNormal ? g_hFontNormal : (HFONT)GetStockObject(DEFAULT_GUI_FONT));

    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

LRESULT CALLBACK DarkGroupBoxProc(HWND hCtrl, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hCtrl, &ps);
        RECT rc;
        GetClientRect(hCtrl, &rc);

        wchar_t text[128] = { 0 };
        GetWindowTextW(hCtrl, text, 128);

        FillRect(hdc, &rc, g_hbrDarkBg);

        HPEN pen = CreatePen(PS_SOLID, 1, RGB(55, 55, 64));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));

        RECT cardRc = rc;
        cardRc.top += 7;
        cardRc.bottom -= 1;
        cardRc.right -= 1;
        RoundRect(hdc, cardRc.left, cardRc.top, cardRc.right, cardRc.bottom, 6, 6);

        if (text[0]) {
            SelectObject(hdc, g_hFontBold ? g_hFontBold : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            SetBkColor(hdc, RGB(28, 28, 32));
            SetTextColor(hdc, RGB(0, 160, 255));

            SIZE sz;
            GetTextExtentPoint32W(hdc, text, static_cast<int>(wcslen(text)), &sz);

            RECT textRc = { rc.left + 10, rc.top, rc.left + 14 + sz.cx, rc.top + sz.cy };
            FillRect(hdc, &textRc, g_hbrDarkBg);
            TextOutW(hdc, textRc.left + 2, textRc.top, text, static_cast<int>(wcslen(text)));
        }

        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);

        EndPaint(hCtrl, &ps);
        return 0;
    }
    return CallWindowProcW(g_origGroupBoxProc, hCtrl, msg, wParam, lParam);
}

struct ShortcutRow {
    std::wstring key;
    std::wstring desc;
};

struct ShortcutSection {
    std::wstring title;
    std::vector<ShortcutRow> rows;
};

static const std::vector<ShortcutSection>& GetShortcutsGuide() {
    static const std::vector<ShortcutSection> guide = {
        {
            L"QUICK VIEW (HOLD TRIGGER / ALT)",
            {
                { L"Alt + Wheel", L"Zoom screen smoothly in / out" },
                { L"Alt + LMB drag", L"Draw freehand line (uses Laser Ink by default)" },
                { L"Alt + RMB drag", L"Draw directional arrow" },
                { L"Alt + Shift + Drag", L"Draw rectangle or snap line/arrow to 0/45/90 deg" },
                { L"Alt + MMB click", L"Drop numbered step badge (1, 2, 3..)" },
                { L"Alt + V", L"Toggle Laser Ink (auto-fading strokes)" },
                { L"Alt + H", L"Toggle Highlighter mode" },
                { L"Alt + O", L"Toggle Blur redaction box" },
                { L"Alt + S", L"Toggle Spotlight mode (darkens screen around cursor)" },
                { L"Alt + X", L"On-screen text tool (Enter: confirm, Esc: cancel)" },
                { L"Alt + W", L"Cycle Whiteboard -> Blackboard -> Screen" },
                { L"Alt + K", L"Toggle Keystroke HUD" },
                { L"Alt + T", L"Toggle 5-minute countdown break timer" },
                { L"Alt + R / G / B / Y", L"Switch pen color: Red, Green, Blue, Yellow" },
                { L"Alt + Z", L"Undo last stroke" },
                { L"Alt + C", L"Copy fullscreen screenshot to clipboard" },
                { L"Alt + Shift + C", L"Crop region screenshot to clipboard" },
                { L"Alt + P", L"Toggle Pin mode (keep drawings on screen)" },
                { L"Esc", L"Reset zoom and clear drawings" }
            }
        },
        {
            L"DRAW MODE (SCREEN FREEZE - DEFAULT: CTRL + 2)",
            {
                { L"Screen frozen", L"Cursor becomes a pen; no need to hold Alt key" },
                { L"Laser Ink disabled", L"Drawings stay permanent by default (toggle with V)" },
                { L"LMB drag", L"Draw line (or active tool: Highlighter / Blur)" },
                { L"RMB drag", L"Draw directional arrow" },
                { L"Shift + Drag", L"Draw rectangle or snap straight lines" },
                { L"MMB click", L"Drop numbered step badge" },
                { L"H / O / V / X / W", L"Highlighter / Blur / Laser / Text / Board" },
                { L"R / G / B / Y", L"Change pen color instantly" },
                { L"Ctrl + Z or Z", L"Undo last stroke" },
                { L"Ctrl + C or C", L"Copy screenshot to clipboard" },
                { L"Shift + C", L"Crop screenshot region to clipboard" },
                { L"P", L"Keep drawings pinned on screen when exiting" },
                { L"Esc", L"Exit Draw Mode and unlock screen" }
            }
        },
        {
            L"BREAK COUNTDOWN TIMER",
            {
                { L"Wheel / Up / Down", L"Adjust time by 1 minute" },
                { L"Shift + Wheel / Arrows", L"Adjust time by 5 seconds" },
                { L"Click clock text", L"Type custom time directly (e.g. 3:50 or 5)" },
                { L"Space", L"Pause / resume countdown" },
                { L"Esc", L"Cancel and dismiss break timer" }
            }
        }
    };
    return guide;
}

static void ApplySettingsScroll(HWND hwnd, int newPos) {
    newPos = std::clamp(newPos, 0, g_maxSettingsScroll);
    if (newPos == g_settingsScrollPos) return;
    g_settingsScrollPos = newPos;

    HDWP hdwp = BeginDeferWindowPos(static_cast<int>(g_settingsControls.size()));
    for (const auto& cp : g_settingsControls) {
        if (!cp.hwnd || !hdwp) continue;
        int sx = static_cast<int>(cp.x * g_settingsScale);
        int sy = static_cast<int>(cp.y * g_settingsScale) - g_settingsScrollPos;
        int sw = static_cast<int>(cp.w * g_settingsScale);
        int sh = static_cast<int>(cp.h * g_settingsScale);
        hdwp = DeferWindowPos(hdwp, cp.hwnd, NULL, sx, sy, sw, sh, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (hdwp) EndDeferWindowPos(hdwp);

    SCROLLINFO si = { sizeof(si), SIF_POS };
    si.nPos = g_settingsScrollPos;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    InvalidateRect(hwnd, NULL, TRUE);
}

static void UpdateSettingsScrollRange(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int clientH = rc.bottom - rc.top;
    int totalH = static_cast<int>(g_totalSettingsContentH * g_settingsScale);

    g_maxSettingsScroll = std::max(0, totalH - clientH);

    SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS };
    si.nMin = 0;
    si.nMax = totalH;
    si.nPage = clientH;
    si.nPos = std::clamp(g_settingsScrollPos, 0, g_maxSettingsScroll);
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    ShowScrollBar(hwnd, SB_VERT, g_maxSettingsScroll > 0);
}

LRESULT CALLBACK ShortcutsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_hbrDarkBg);
        return 1;
    }

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int clientW = rc.right - rc.left;
        int clientH = rc.bottom - rc.top;

        int fH = static_cast<int>(g_shortcutsFooterH * g_shortcutsScale);
        int scrollAreaH = std::max(50, clientH - fH);
        int totalH = static_cast<int>(g_totalShortcutsContentH * g_shortcutsScale);
        g_maxShortcutsScroll = std::max(0, totalH - scrollAreaH);

        SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS };
        si.nMin = 0;
        si.nMax = totalH;
        si.nPage = scrollAreaH;
        si.nPos = std::clamp(g_shortcutsScrollPos, 0, g_maxShortcutsScroll);
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        ShowScrollBar(hwnd, SB_VERT, g_maxShortcutsScroll > 0);

        HWND hBtnClose = GetDlgItem(hwnd, ID_BTN_CLOSE_SHORTCUTS);
        if (hBtnClose) {
            int bw = static_cast<int>(130 * g_shortcutsScale);
            int bh = static_cast<int>(28 * g_shortcutsScale);
            int bx = (clientW - bw) / 2;
            int by = scrollAreaH + (fH - bh) / 2;
            SetWindowPos(hBtnClose, NULL, bx, by, bw, bh, SWP_NOZORDER);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int step = -static_cast<int>((delta / static_cast<float>(WHEEL_DELTA)) * (48.0f * g_shortcutsScale));
        int newPos = std::clamp(g_shortcutsScrollPos + step, 0, g_maxShortcutsScroll);
        if (newPos != g_shortcutsScrollPos) {
            g_shortcutsScrollPos = newPos;
            SCROLLINFO si = { sizeof(si), SIF_POS };
            si.nPos = g_shortcutsScrollPos;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_VSCROLL: {
        int newPos = g_shortcutsScrollPos;
        int lineStep = static_cast<int>(24 * g_shortcutsScale);
        int pageStep = static_cast<int>(120 * g_shortcutsScale);
        switch (LOWORD(wParam)) {
        case SB_LINEUP: newPos -= lineStep; break;
        case SB_LINEDOWN: newPos += lineStep; break;
        case SB_PAGEUP: newPos -= pageStep; break;
        case SB_PAGEDOWN: newPos += pageStep; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si = { sizeof(si), SIF_TRACKPOS };
            GetScrollInfo(hwnd, SB_VERT, &si);
            newPos = si.nTrackPos;
            break;
        }
        }
        newPos = std::clamp(newPos, 0, g_maxShortcutsScroll);
        if (newPos != g_shortcutsScrollPos) {
            g_shortcutsScrollPos = newPos;
            SCROLLINFO si = { sizeof(si), SIF_POS };
            si.nPos = g_shortcutsScrollPos;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
        if (dis->CtlID == ID_BTN_CLOSE_SHORTCUTS) {
            DrawDarkButton(dis, false);
            return TRUE;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);
        int clientW = clientRc.right - clientRc.left;
        int clientH = clientRc.bottom - clientRc.top;
        int fH = static_cast<int>(g_shortcutsFooterH * g_shortcutsScale);
        int scrollAreaH = std::max(50, clientH - fH);

        HRGN rgnClip = CreateRectRgn(0, 0, clientW, scrollAreaH);
        SelectClipRgn(hdc, rgnClip);

        SetBkMode(hdc, TRANSPARENT);
        HFONT fontSec = CreateFontW(static_cast<int>(-12 * g_shortcutsScale), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT fontKey = CreateFontW(static_cast<int>(-11 * g_shortcutsScale), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT fontDesc = CreateFontW(static_cast<int>(-11 * g_shortcutsScale), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        int y = static_cast<int>(14 * g_shortcutsScale) - g_shortcutsScrollPos;
        const auto& guide = GetShortcutsGuide();

        for (const auto& sec : guide) {
            SelectObject(hdc, fontSec);
            SetTextColor(hdc, RGB(0, 160, 255));
            TextOutW(hdc, static_cast<int>(20 * g_shortcutsScale), y, sec.title.c_str(), static_cast<int>(sec.title.length()));
            y += static_cast<int>(20 * g_shortcutsScale);

            HPEN pen = CreatePen(PS_SOLID, 1, RGB(46, 46, 54));
            HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
            MoveToEx(hdc, static_cast<int>(20 * g_shortcutsScale), y, NULL);
            LineTo(hdc, clientW - static_cast<int>(24 * g_shortcutsScale), y);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
            y += static_cast<int>(6 * g_shortcutsScale);

            for (const auto& row : sec.rows) {
                SelectObject(hdc, fontKey);
                SetTextColor(hdc, RGB(245, 245, 250));
                RECT rcKey = {
                    static_cast<int>(24 * g_shortcutsScale),
                    y,
                    static_cast<int>(195 * g_shortcutsScale),
                    y + static_cast<int>(18 * g_shortcutsScale)
                };
                DrawTextW(hdc, row.key.c_str(), -1, &rcKey, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

                SelectObject(hdc, fontDesc);
                SetTextColor(hdc, RGB(180, 180, 192));
                RECT rcDesc = {
                    static_cast<int>(200 * g_shortcutsScale),
                    y,
                    clientW - static_cast<int>(24 * g_shortcutsScale),
                    y + static_cast<int>(18 * g_shortcutsScale)
                };
                DrawTextW(hdc, row.desc.c_str(), -1, &rcDesc, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

                y += static_cast<int>(18 * g_shortcutsScale);
            }
            y += static_cast<int>(12 * g_shortcutsScale);
        }

        DeleteObject(fontSec);
        DeleteObject(fontKey);
        DeleteObject(fontDesc);

        SelectClipRgn(hdc, NULL);
        DeleteObject(rgnClip);

        RECT footerRc = { 0, scrollAreaH, clientW, clientH };
        HBRUSH footerBr = CreateSolidBrush(RGB(22, 22, 25));
        FillRect(hdc, &footerRc, footerBr);
        DeleteObject(footerBr);

        HPEN divPen = CreatePen(PS_SOLID, 1, RGB(45, 45, 52));
        HPEN oldDivPen = static_cast<HPEN>(SelectObject(hdc, divPen));
        MoveToEx(hdc, 0, scrollAreaH, NULL);
        LineTo(hdc, clientW, scrollAreaH);
        SelectObject(hdc, oldDivPen);
        DeleteObject(divPen);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_BTN_CLOSE_SHORTCUTS) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_hwndShortcuts = NULL;
        g_shortcutsScrollPos = 0;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ShowShortcutsWindow(HWND hwndParent) {
    if (g_hwndShortcuts) {
        SetForegroundWindow(g_hwndShortcuts);
        return;
    }

    HINSTANCE hInst = GetModuleHandle(NULL);
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    if (!GetClassInfoExW(hInst, L"LensItShortcutsModal", &wc)) {
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = ShortcutsWndProc;
        wc.hInstance = hInst;
        wc.hIcon = g_appIcon;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"LensItShortcutsModal";
        RegisterClassExW(&wc);
    }

    POINT ptCursor = { 0, 0 };
    GetCursorPos(&ptCursor);
    HMONITOR hMon = MonitorFromPoint(ptCursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfo(hMon, &mi);
    int maxWorkH = (mi.rcWork.bottom - mi.rcWork.top) - 40;

    UINT dpi = 96;
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        typedef UINT(WINAPI* GetDpiForSystemFn)();
        auto pFn = reinterpret_cast<GetDpiForSystemFn>(GetProcAddress(hUser, "GetDpiForSystem"));
        if (pFn) dpi = pFn();
    }
    g_shortcutsScale = (dpi > 0) ? (static_cast<float>(dpi) / 96.0f) : 1.0f;

    const int baseW = 560;
    const int baseH = 580;
    int targetClientW = static_cast<int>(baseW * g_shortcutsScale);
    int targetClientH = std::min(static_cast<int>(baseH * g_shortcutsScale), maxWorkH);

    RECT wr = { 0, 0, targetClientW, targetClientH };
    AdjustWindowRectExForDpi(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VSCROLL, FALSE, WS_EX_TOPMOST, dpi);
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;
    int cx = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2;
    int cy = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - h) / 2;

    g_hwndShortcuts = CreateWindowExW(
        WS_EX_TOPMOST, L"LensItShortcutsModal", L"LensIt - All Shortcuts & Controls",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VSCROLL,
        cx, cy, w, h, hwndParent, NULL, hInst, NULL
    );

    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_hwndShortcuts, 20, &dark, sizeof(dark));

    int fH = static_cast<int>(g_shortcutsFooterH * g_shortcutsScale);
    int bw = static_cast<int>(130 * g_shortcutsScale);
    int bh = static_cast<int>(28 * g_shortcutsScale);
    int bx = (targetClientW - bw) / 2;
    int by = (targetClientH - fH) + (fH - bh) / 2;

    CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        bx, by, bw, bh, g_hwndShortcuts, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_CLOSE_SHORTCUTS)), hInst, NULL);

    ShowWindow(g_hwndShortcuts, SW_SHOW);
    UpdateWindow(g_hwndShortcuts);
}

}

void UpdateSettingsUI() {
    if (!g_hwndSettings) return;

    HWND hBtnTrigger = GetDlgItem(g_hwndSettings, ID_BTN_TRIGGER_KEY);
    HWND hBtnFreeze = GetDlgItem(g_hwndSettings, ID_BTN_FREEZE_KEY);
    HWND hBtnRect = GetDlgItem(g_hwndSettings, ID_BTN_RECT_KEY);

    if (g_bindingMode == BindingMode::TriggerKey) {
        SetWindowTextW(hBtnTrigger, L"[ Press any key / mouse button... ]");
    }
    else {
        SetWindowTextW(hBtnTrigger, (L"Key: [ " + GetKeyNameStr(g_config.triggerKey) + L" ]").c_str());
    }

    if (g_bindingMode == BindingMode::FreezeKey) {
        SetWindowTextW(hBtnFreeze, L"[ Press key... ]");
    }
    else {
        SetWindowTextW(hBtnFreeze, (L"Hotkey: [ Ctrl + " + GetKeyNameStr(g_config.freezeKey) + L" ]").c_str());
    }

    if (g_bindingMode == BindingMode::RectKey) {
        SetWindowTextW(hBtnRect, L"[ Press key... ]");
    }
    else {
        SetWindowTextW(hBtnRect, (L"Modifier: [ " + GetKeyNameStr(g_config.rectKey) + L" ]").c_str());
    }

    Button_SetCheck(GetDlgItem(g_hwndSettings, ID_CHK_RESET_ZOOM), g_config.resetZoomOnRelease ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(g_hwndSettings, ID_CHK_HOLD_LASER), g_config.holdUsesLaser ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(g_hwndSettings, ID_CHK_KEEP_DRAWINGS), g_config.keepDrawingsOnRelease ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(g_hwndSettings, ID_CHK_HIDE_TOASTS), g_config.hideToastsFromCapture ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(g_hwndSettings, ID_CHK_AUTO_START), IsAutoStartEnabled() ? BST_CHECKED : BST_UNCHECKED);

    SendMessageW(GetDlgItem(g_hwndSettings, ID_SLIDER_LINE), TBM_SETPOS, TRUE, g_config.lineWidth);
    SendMessageW(GetDlgItem(g_hwndSettings, ID_SLIDER_ARROW), TBM_SETPOS, TRUE, g_config.arrowWidth);
    SendMessageW(GetDlgItem(g_hwndSettings, ID_SLIDER_RECT), TBM_SETPOS, TRUE, g_config.rectWidth);

    InvalidateRect(g_hwndSettings, NULL, TRUE);
}

static void CreateSettingsControls(HWND hwnd) {
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    g_settingsControls.clear();
    g_settingsScrollPos = 0;

    UINT dpi = GetDpiForWindow(hwnd);
    g_settingsScale = (dpi > 0) ? (static_cast<float>(dpi) / 96.0f) : 1.0f;

    int fontH = -MulDiv(9, dpi, 72);
    g_hFontNormal = CreateFontW(fontH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_hFontBold = CreateFontW(fontH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_hbrDarkBg = CreateSolidBrush(RGB(28, 28, 32));

    auto registerCtrl = [hwnd](LPCWSTR cls, LPCWSTR text, DWORD style, int x, int y, int w, int h, int id) -> HWND {
        int sx = static_cast<int>(x * g_settingsScale);
        int sy = static_cast<int>(y * g_settingsScale);
        int sw = static_cast<int>(w * g_settingsScale);
        int sh = static_cast<int>(h * g_settingsScale);

        HWND hCtrl = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, sx, sy, sw, sh, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandle(NULL), NULL);
        if (g_hFontNormal) SendMessageW(hCtrl, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFontNormal), TRUE);
        g_settingsControls.push_back({ hCtrl, x, y, w, h });
        return hCtrl;
    };

    auto registerGroupBox = [&](LPCWSTR text, int x, int y, int w, int h) {
        HWND hGb = registerCtrl(L"BUTTON", text, BS_GROUPBOX, x, y, w, h, 0);
        if (!g_origGroupBoxProc) {
            g_origGroupBoxProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hGb, GWLP_WNDPROC));
        }
        SetWindowLongPtrW(hGb, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DarkGroupBoxProc));
        return hGb;
    };

    registerGroupBox(L" Quick View (Hold) ", 15, 10, 410, 115);
    registerCtrl(L"BUTTON", L"", BS_OWNERDRAW, 30, 32, 380, 26, ID_BTN_TRIGGER_KEY);
    registerCtrl(L"BUTTON", L"Reset zoom on trigger key release", BS_AUTOCHECKBOX, 30, 64, 380, 20, ID_CHK_RESET_ZOOM);
    registerCtrl(L"BUTTON", L"Laser ink while holding trigger", BS_AUTOCHECKBOX, 30, 90, 380, 20, ID_CHK_HOLD_LASER);

    registerGroupBox(L" Draw Mode (Screen Freeze) ", 15, 135, 410, 95);
    registerCtrl(L"BUTTON", L"", BS_OWNERDRAW, 30, 158, 380, 26, ID_BTN_FREEZE_KEY);
    registerCtrl(L"STATIC", L"Freehand drawing without holding keys. Screen freezes, cursor becomes a pen. Press Esc to exit.", 0, 30, 190, 380, 32, 0);

    registerGroupBox(L" Tools && Colors ", 15, 240, 410, 195);
    registerCtrl(L"STATIC", L"Line (LMB):", 0, 30, 265, 90, 20, 0);
    registerCtrl(L"BUTTON", L"", BS_OWNERDRAW, 130, 262, 36, 22, ID_BTN_COLOR_LINE);
    HWND sLine = registerCtrl(TRACKBAR_CLASSW, L"", TBS_AUTOTICKS | TBS_NOTICKS, 180, 262, 230, 24, ID_SLIDER_LINE);
    SendMessageW(sLine, TBM_SETRANGE, TRUE, MAKELPARAM(1, 20));

    registerCtrl(L"STATIC", L"Arrow (RMB):", 0, 30, 298, 90, 20, 0);
    registerCtrl(L"BUTTON", L"", BS_OWNERDRAW, 130, 295, 36, 22, ID_BTN_COLOR_ARROW);
    HWND sArrow = registerCtrl(TRACKBAR_CLASSW, L"", TBS_AUTOTICKS | TBS_NOTICKS, 180, 295, 230, 24, ID_SLIDER_ARROW);
    SendMessageW(sArrow, TBM_SETRANGE, TRUE, MAKELPARAM(1, 20));

    registerCtrl(L"STATIC", L"Rectangle:", 0, 30, 331, 90, 20, 0);
    registerCtrl(L"BUTTON", L"", BS_OWNERDRAW, 130, 328, 36, 22, ID_BTN_COLOR_RECT);
    HWND sRect = registerCtrl(TRACKBAR_CLASSW, L"", TBS_AUTOTICKS | TBS_NOTICKS, 180, 328, 230, 24, ID_SLIDER_RECT);
    SendMessageW(sRect, TBM_SETRANGE, TRUE, MAKELPARAM(1, 20));

    registerCtrl(L"STATIC", L"Modifier:", 0, 30, 365, 90, 20, 0);
    registerCtrl(L"BUTTON", L"", BS_OWNERDRAW, 130, 362, 140, 24, ID_BTN_RECT_KEY);

    registerCtrl(L"STATIC", L"Badges (MMB):", 0, 30, 400, 90, 20, 0);
    registerCtrl(L"BUTTON", L"", BS_OWNERDRAW, 130, 397, 36, 22, ID_BTN_COLOR_BADGE);
    registerCtrl(L"STATIC", L"Middle-click drops numbered step badge (1, 2, 3..)", 0, 180, 396, 230, 32, 0);

    registerGroupBox(L" Options ", 15, 445, 410, 75);
    registerCtrl(L"BUTTON", L"Keep drawings on screen on release (Pin mode)", BS_AUTOCHECKBOX, 30, 468, 380, 20, ID_CHK_KEEP_DRAWINGS);
    registerCtrl(L"BUTTON", L"Hide notifications from capture", BS_AUTOCHECKBOX, 30, 492, 230, 20, ID_CHK_HIDE_TOASTS);
    registerCtrl(L"BUTTON", L"Start with Windows", BS_AUTOCHECKBOX, 270, 492, 145, 20, ID_CHK_AUTO_START);

    registerGroupBox(L" Shortcuts (Alt not required in Draw Mode) ", 15, 530, 410, 105);
    registerCtrl(L"BUTTON", L"All Shortcuts...", BS_OWNERDRAW, 292, 526, 120, 22, ID_BTN_ALL_SHORTCUTS);
    registerCtrl(L"STATIC",
        L"Esc : exit draw mode / clear drawings\n"
        L"R / G / B / Y : Red, Green, Blue, Yellow ink\n"
        L"W : Whiteboard / Blackboard   |   H / O : Highlighter / Blur\n"
        L"Ctrl + Z : undo stroke   |   Ctrl + C : copy to clipboard\n"
        L"Shift + drag : rectangle or snap line/arrow to 0/45/90 deg",
        0, 30, 552, 380, 75, 0);

    UpdateSettingsScrollRange(hwnd);
    UpdateSettingsUI();
}

void ShowSettingsWindow(HINSTANCE hInstance) {
    if (g_hwndSettings) {
        SetForegroundWindow(g_hwndSettings);
        return;
    }

    POINT ptCursor = { 0, 0 };
    GetCursorPos(&ptCursor);
    HMONITOR hMon = MonitorFromPoint(ptCursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfo(hMon, &mi);
    int maxWorkH = (mi.rcWork.bottom - mi.rcWork.top) - 40;

    UINT dpi = 96;
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        typedef UINT(WINAPI* GetDpiForSystemFn)();
        auto pFn = reinterpret_cast<GetDpiForSystemFn>(GetProcAddress(hUser, "GetDpiForSystem"));
        if (pFn) dpi = pFn();
    }
    float scale = (dpi > 0) ? (static_cast<float>(dpi) / 96.0f) : 1.0f;

    const int baseW = 455;
    const int baseH = 685;
    int targetClientW = static_cast<int>(baseW * scale);
    int targetClientH = std::min(static_cast<int>(baseH * scale), maxWorkH);

    RECT wr = { 0, 0, targetClientW, targetClientH };
    AdjustWindowRectExForDpi(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VSCROLL, FALSE, WS_EX_TOPMOST, dpi);
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;
    int cx = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2;
    int cy = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - h) / 2;

    g_hwndSettings = CreateWindowExW(
        WS_EX_TOPMOST, L"LensItSettings", L"LensIt - Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VSCROLL,
        cx, cy, w, h, NULL, NULL, hInstance, NULL
    );

    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_hwndSettings, 20, &dark, sizeof(dark));

    CreateSettingsControls(g_hwndSettings);
    ShowWindow(g_hwndSettings, SW_SHOW);
    UpdateWindow(g_hwndSettings);
}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_hbrDarkBg);
        return 1;
    }

    case WM_SIZE:
        UpdateSettingsScrollRange(hwnd);
        return 0;

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int step = -static_cast<int>((delta / static_cast<float>(WHEEL_DELTA)) * (48.0f * g_settingsScale));
        ApplySettingsScroll(hwnd, g_settingsScrollPos + step);
        return 0;
    }

    case WM_VSCROLL: {
        int newPos = g_settingsScrollPos;
        int lineStep = static_cast<int>(24 * g_settingsScale);
        int pageStep = static_cast<int>(120 * g_settingsScale);
        switch (LOWORD(wParam)) {
        case SB_LINEUP: newPos -= lineStep; break;
        case SB_LINEDOWN: newPos += lineStep; break;
        case SB_PAGEUP: newPos -= pageStep; break;
        case SB_PAGEDOWN: newPos += pageStep; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si = { sizeof(si), SIF_TRACKPOS };
            GetScrollInfo(hwnd, SB_VERT, &si);
            newPos = si.nTrackPos;
            break;
        }
        }
        ApplySettingsScroll(hwnd, newPos);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, RGB(230, 230, 235));
        SetBkColor(hdc, RGB(28, 28, 32));
        return reinterpret_cast<INT_PTR>(g_hbrDarkBg);
    }

    case WM_CTLCOLORBTN: {
        return reinterpret_cast<INT_PTR>(g_hbrDarkBg);
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
        if (dis->CtlID == ID_BTN_COLOR_LINE) DrawColorButton(dis, g_config.lineColor);
        else if (dis->CtlID == ID_BTN_COLOR_ARROW) DrawColorButton(dis, g_config.arrowColor);
        else if (dis->CtlID == ID_BTN_COLOR_RECT) DrawColorButton(dis, g_config.rectColor);
        else if (dis->CtlID == ID_BTN_COLOR_BADGE) DrawColorButton(dis, g_config.badgeColor);
        else if (dis->CtlID == ID_BTN_TRIGGER_KEY) DrawDarkButton(dis, g_bindingMode == BindingMode::TriggerKey);
        else if (dis->CtlID == ID_BTN_FREEZE_KEY) DrawDarkButton(dis, g_bindingMode == BindingMode::FreezeKey);
        else if (dis->CtlID == ID_BTN_RECT_KEY) DrawDarkButton(dis, g_bindingMode == BindingMode::RectKey);
        else if (dis->CtlID == ID_BTN_ALL_SHORTCUTS) DrawDarkButton(dis, false);
        return TRUE;
    }

    case WM_HSCROLL: {
        HWND hTrack = reinterpret_cast<HWND>(lParam);
        int val = static_cast<int>(SendMessageW(hTrack, TBM_GETPOS, 0, 0));
        if (hTrack == GetDlgItem(hwnd, ID_SLIDER_LINE)) g_config.lineWidth = val;
        else if (hTrack == GetDlgItem(hwnd, ID_SLIDER_ARROW)) g_config.arrowWidth = val;
        else if (hTrack == GetDlgItem(hwnd, ID_SLIDER_RECT)) g_config.rectWidth = val;
        SaveConfig();
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case ID_BTN_TRIGGER_KEY:
            g_bindingMode = (g_bindingMode == BindingMode::TriggerKey) ? BindingMode::None : BindingMode::TriggerKey;
            UpdateSettingsUI();
            break;

        case ID_BTN_FREEZE_KEY:
            g_bindingMode = (g_bindingMode == BindingMode::FreezeKey) ? BindingMode::None : BindingMode::FreezeKey;
            UpdateSettingsUI();
            break;

        case ID_BTN_RECT_KEY:
            g_bindingMode = (g_bindingMode == BindingMode::RectKey) ? BindingMode::None : BindingMode::RectKey;
            UpdateSettingsUI();
            break;

        case ID_BTN_ALL_SHORTCUTS:
            ShowShortcutsWindow(hwnd);
            break;

        case ID_CHK_RESET_ZOOM:
            g_config.resetZoomOnRelease = (Button_GetCheck(GetDlgItem(hwnd, ID_CHK_RESET_ZOOM)) == BST_CHECKED);
            SaveConfig();
            break;

        case ID_CHK_HOLD_LASER:
            g_config.holdUsesLaser = (Button_GetCheck(GetDlgItem(hwnd, ID_CHK_HOLD_LASER)) == BST_CHECKED);
            SaveConfig();
            break;

        case ID_CHK_KEEP_DRAWINGS:
            g_config.keepDrawingsOnRelease = (Button_GetCheck(GetDlgItem(hwnd, ID_CHK_KEEP_DRAWINGS)) == BST_CHECKED);
            SaveConfig();
            break;

        case ID_CHK_HIDE_TOASTS:
            g_config.hideToastsFromCapture = (Button_GetCheck(GetDlgItem(hwnd, ID_CHK_HIDE_TOASTS)) == BST_CHECKED);
            ApplyToastCaptureAffinity();
            SaveConfig();
            break;

        case ID_CHK_AUTO_START: {
            bool enable = (Button_GetCheck(GetDlgItem(hwnd, ID_CHK_AUTO_START)) == BST_CHECKED);
            SetAutoStart(enable);
            break;
        }

        case ID_BTN_COLOR_LINE:
            g_config.lineColor = PickColorDialog(hwnd, g_config.lineColor);
            SaveConfig();
            InvalidateRect(GetDlgItem(hwnd, ID_BTN_COLOR_LINE), NULL, TRUE);
            break;

        case ID_BTN_COLOR_ARROW:
            g_config.arrowColor = PickColorDialog(hwnd, g_config.arrowColor);
            SaveConfig();
            InvalidateRect(GetDlgItem(hwnd, ID_BTN_COLOR_ARROW), NULL, TRUE);
            break;

        case ID_BTN_COLOR_RECT:
            g_config.rectColor = PickColorDialog(hwnd, g_config.rectColor);
            SaveConfig();
            InvalidateRect(GetDlgItem(hwnd, ID_BTN_COLOR_RECT), NULL, TRUE);
            break;

        case ID_BTN_COLOR_BADGE:
            g_config.badgeColor = PickColorDialog(hwnd, g_config.badgeColor);
            SaveConfig();
            InvalidateRect(GetDlgItem(hwnd, ID_BTN_COLOR_BADGE), NULL, TRUE);
            break;
        }
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_hwndSettings = NULL;
        g_bindingMode = BindingMode::None;
        g_origGroupBoxProc = NULL;
        g_settingsControls.clear();
        g_settingsScrollPos = 0;
        if (g_hwndShortcuts) DestroyWindow(g_hwndShortcuts);
        if (g_hFontNormal) { DeleteObject(g_hFontNormal); g_hFontNormal = NULL; }
        if (g_hFontBold) { DeleteObject(g_hFontBold); g_hFontBold = NULL; }
        if (g_hbrDarkBg) { DeleteObject(g_hbrDarkBg); g_hbrDarkBg = NULL; }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}