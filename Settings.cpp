#include "LensIt.h"

static float g_uiScale = 1.0f;
static bool g_shortcutsExpanded = false;

static float s_animHeight = 555.0f;
static float s_targetHeight = 555.0f;

RECT btnBind = { 20, 32, 320, 66 };
RECT chkResetZoomRow = { 20, 74, 320, 98 };
RECT chkKeepDrawingsRow = { 20, 102, 320, 126 };
RECT chkHideToastsRow = { 20, 130, 320, 154 };

RECT colorLine = { 20, 183, 50, 213 };
RECT rectSliderLine = { 65, 183, 320, 213 };

RECT colorArrow = { 20, 243, 50, 273 };
RECT rectSliderArrow = { 65, 243, 320, 273 };

RECT btnBindRect = { 20, 304, 320, 338 };
RECT colorRect = { 20, 368, 50, 398 };
RECT rectSliderRect = { 65, 368, 320, 398 };

RECT colorBadge = { 20, 428, 50, 458 };
RECT btnShortcutsHeader = { 20, 480, 320, 510 };

int draggingSlider = 0;

static void AddRoundedRect(GraphicsPath& path, float x, float y, float w, float h, float radius) {
    float d = radius * 2.0f;
    if (d > w) d = w;
    if (d > h) d = h;
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

static void DrawModernToggle(Graphics& g, int x, int y, bool isChecked) {
    float fx = (float)x;
    float fy = (float)y;
    float fw = 34.0f;
    float fh = 18.0f;

    GraphicsPath path;
    AddRoundedRect(path, fx, fy, fw, fh, 9.0f);

    Color bgCol = isChecked ? Color(255, 0, 120, 215) : Color(255, 48, 48, 52);
    SolidBrush bgBrush(bgCol);
    g.FillPath(&bgBrush, &path);

    Pen borderPen(isChecked ? Color(255, 0, 140, 255) : Color(255, 80, 80, 85), 1.0f);
    g.DrawPath(&borderPen, &path);

    float thumbX = isChecked ? (fx + fw - 15.0f) : (fx + 3.0f);
    SolidBrush thumbBrush(Color(255, 255, 255));
    g.FillEllipse(&thumbBrush, thumbX, fy + 3.0f, 12.0f, 12.0f);
}

static void DrawModernSlider(Graphics& g, RECT trackRect, int val, int minVal, int maxVal, COLORREF accentColor) {
    float percent = (float)(val - minVal) / (float)(maxVal - minVal);
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 1.0f) percent = 1.0f;

    float totalW = (float)(trackRect.right - trackRect.left);
    float centerY = (float)(trackRect.top + (trackRect.bottom - trackRect.top) / 2);
    float startX = (float)trackRect.left;

    GraphicsPath bgPath;
    AddRoundedRect(bgPath, startX, centerY - 2.5f, totalW, 5.0f, 2.5f);
    SolidBrush bgTrack(Color(255, 42, 42, 45));
    g.FillPath(&bgTrack, &bgPath);

    float filledW = percent * totalW;
    if (filledW > 4.0f) {
        GraphicsPath fillPath;
        AddRoundedRect(fillPath, startX, centerY - 2.5f, filledW, 5.0f, 2.5f);
        SolidBrush fillTrack(Color(255, GetRValue(accentColor), GetGValue(accentColor), GetBValue(accentColor)));
        g.FillPath(&fillTrack, &fillPath);
    }

    float thumbX = startX + filledW;
    SolidBrush thumbBrush(Color(255, 240, 240, 240));
    Pen thumbBorder(Color(255, 20, 20, 20), 1.5f);
    g.FillEllipse(&thumbBrush, thumbX - 7.0f, centerY - 7.0f, 14.0f, 14.0f);
    g.DrawEllipse(&thumbBorder, thumbX - 7.0f, centerY - 7.0f, 14.0f, 14.0f);
}

static void UpdateSettingsWindowSize() {
    if (!g_hwndSettings) return;

    RECT wr = { 0, 0, (int)(340 * g_uiScale), (int)(s_animHeight * g_uiScale) };
    AdjustWindowRectEx(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOPMOST);
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;

    SetWindowPos(g_hwndSettings, HWND_TOPMOST, 0, 0, w, h, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
}

void ShowSettingsWindow(HINSTANCE hInstance) {
    if (g_hwndSettings) {
        SetForegroundWindow(g_hwndSettings);
        return;
    }

    UINT dpi = GetDpiForSystem();
    g_uiScale = (dpi > 0) ? ((float)dpi / 96.0f) : 1.0f;

    s_animHeight = s_targetHeight = g_shortcutsExpanded ? 725.0f : 555.0f;
    RECT wr = { 0, 0, (int)(340 * g_uiScale), (int)(s_animHeight * g_uiScale) };
    AdjustWindowRectEx(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOPMOST);
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;

    int cx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int cy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hwndSettings = CreateWindowEx(
        WS_EX_TOPMOST, L"LensItSettings", L"LensIt Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        cx, cy, w, h, NULL, NULL, hInstance, NULL
    );

    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_hwndSettings, 20, &dark, sizeof(dark));

    ShowWindow(g_hwndSettings, SW_SHOW);
}

bool PtInRectCust(RECT r, int x, int y) {
    return (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom);
}

void DrawCustomUI(Graphics& g) {
    g.Clear(Color(255, 18, 18, 20));
    if (g_uiScale != 1.0f) g.ScaleTransform(g_uiScale, g_uiScale);

    Font fontTitle(L"Segoe UI", 9.5f, FontStyleBold);
    Font fontReg(L"Segoe UI", 9.0f);
    Font fontSmall(L"Segoe UI", 8.0f);
    Font fontCode(L"Consolas", 8.2f);

    SolidBrush textBrush(Color(230, 230, 230));
    SolidBrush textDim(Color(150, 150, 155));
    SolidBrush borderPenBrush(Color(65, 65, 70));
    Pen borderPen(&borderPenBrush, 1.0f);
    Pen sepPen(Color(36, 36, 40), 1.0f);

    StringFormat fmtCenter;
    fmtCenter.SetAlignment(StringAlignmentCenter);
    fmtCenter.SetLineAlignment(StringAlignmentCenter);

    // 1. Trigger Key
    g.DrawString(L"Trigger Key (Hold):", -1, &fontReg, PointF(20, 12), &textBrush);
    bool isTrigBinding = (g_bindingMode == BindingMode::TriggerKey);

    GraphicsPath btnPath;
    AddRoundedRect(btnPath, (float)btnBind.left, (float)btnBind.top, (float)(btnBind.right - btnBind.left), (float)(btnBind.bottom - btnBind.top), 5.0f);
    SolidBrush trigBrush(isTrigBinding ? Color(255, 0, 120, 215) : Color(255, 32, 32, 36));
    g.FillPath(&trigBrush, &btnPath);
    Pen trigBorder(isTrigBinding ? Color(255, 0, 160, 255) : Color(255, 60, 60, 65), 1.0f);
    g.DrawPath(&trigBorder, &btnPath);

    std::wstring bindTxt = isTrigBinding ? L"Press any key or mouse button..." : (L"[  " + GetKeyNameStr(g_config.triggerKey) + L"  ]");
    g.DrawString(bindTxt.c_str(), -1, &fontTitle,
        RectF((REAL)btnBind.left, (REAL)btnBind.top, (REAL)(btnBind.right - btnBind.left), (REAL)(btnBind.bottom - btnBind.top)),
        &fmtCenter, &textBrush);

    // 2. Toggle: Reset zoom on release
    DrawModernToggle(g, btnBind.left, chkResetZoomRow.top + 3, g_config.resetZoomOnRelease);
    g.DrawString(L"Reset zoom on trigger release", -1, &fontReg, PointF(62, (REAL)chkResetZoomRow.top + 2), &textBrush);

    // 3. Toggle: Keep drawings on screen
    DrawModernToggle(g, btnBind.left, chkKeepDrawingsRow.top + 3, g_config.keepDrawingsOnRelease);
    g.DrawString(L"Keep drawings on screen (Click-through)", -1, &fontReg, PointF(62, (REAL)chkKeepDrawingsRow.top + 2), &textBrush);

    // 4. Toggle: Hide toasts from screen capture
    DrawModernToggle(g, btnBind.left, chkHideToastsRow.top + 3, g_config.hideToastsFromCapture);
    g.DrawString(L"Hide notifications from screen capture", -1, &fontReg, PointF(62, (REAL)chkHideToastsRow.top + 2), &textBrush);

    g.DrawLine(&sepPen, 20, 162, 320, 162);

    // 5. Line
    std::wstring lineTitle = L"Line (LMB)  *  " + std::to_wstring(g_config.lineWidth) + L" px";
    g.DrawString(lineTitle.c_str(), -1, &fontReg, PointF(20, 165), &textBrush);

    GraphicsPath cLinePath;
    AddRoundedRect(cLinePath, (float)colorLine.left, (float)colorLine.top, 30.0f, 30.0f, 5.0f);
    SolidBrush brushLine(Color(255, GetRValue(g_config.lineColor), GetGValue(g_config.lineColor), GetBValue(g_config.lineColor)));
    g.FillPath(&brushLine, &cLinePath);
    g.DrawPath(&borderPen, &cLinePath);

    DrawModernSlider(g, rectSliderLine, g_config.lineWidth, 1, 20, g_config.lineColor);

    // 6. Arrow
    std::wstring arrowTitle = L"Arrow (RMB)  *  " + std::to_wstring(g_config.arrowWidth) + L" px";
    g.DrawString(arrowTitle.c_str(), -1, &fontReg, PointF(20, 225), &textBrush);

    GraphicsPath cArrowPath;
    AddRoundedRect(cArrowPath, (float)colorArrow.left, (float)colorArrow.top, 30.0f, 30.0f, 5.0f);
    SolidBrush brushArrow(Color(255, GetRValue(g_config.arrowColor), GetGValue(g_config.arrowColor), GetBValue(g_config.arrowColor)));
    g.FillPath(&brushArrow, &cArrowPath);
    g.DrawPath(&borderPen, &cArrowPath);

    DrawModernSlider(g, rectSliderArrow, g_config.arrowWidth, 1, 20, g_config.arrowColor);

    // 7. Rectangle modifier key & tool
    g.DrawString(L"Rectangle Modifier Key:", -1, &fontReg, PointF(20, 283), &textBrush);
    bool isRectBinding = (g_bindingMode == BindingMode::RectKey);

    GraphicsPath btnRectPath;
    AddRoundedRect(btnRectPath, (float)btnBindRect.left, (float)btnBindRect.top, (float)(btnBindRect.right - btnBindRect.left), (float)(btnBindRect.bottom - btnBindRect.top), 5.0f);
    SolidBrush rectBindBrush(isRectBinding ? Color(255, 0, 120, 215) : Color(255, 32, 32, 36));
    g.FillPath(&rectBindBrush, &btnRectPath);
    Pen rectBorder(isRectBinding ? Color(255, 0, 160, 255) : Color(255, 60, 60, 65), 1.0f);
    g.DrawPath(&rectBorder, &btnRectPath);

    std::wstring bindRectTxt = isRectBinding ? L"Press any key or mouse button..." : (L"[  " + GetKeyNameStr(g_config.rectKey) + L"  ]");
    g.DrawString(bindRectTxt.c_str(), -1, &fontTitle,
        RectF((REAL)btnBindRect.left, (REAL)btnBindRect.top, (REAL)(btnBindRect.right - btnBindRect.left), (REAL)(btnBindRect.bottom - btnBindRect.top)),
        &fmtCenter, &textBrush);

    std::wstring rectTitle = L"Rectangle  *  " + std::to_wstring(g_config.rectWidth) + L" px";
    g.DrawString(rectTitle.c_str(), -1, &fontReg, PointF(20, 348), &textBrush);

    GraphicsPath cRectPath;
    AddRoundedRect(cRectPath, (float)colorRect.left, (float)colorRect.top, 30.0f, 30.0f, 5.0f);
    SolidBrush brushRect(Color(255, GetRValue(g_config.rectColor), GetGValue(g_config.rectColor), GetBValue(g_config.rectColor)));
    g.FillPath(&brushRect, &cRectPath);
    g.DrawPath(&borderPen, &cRectPath);

    DrawModernSlider(g, rectSliderRect, g_config.rectWidth, 1, 20, g_config.rectColor);

    // 8. Badge
    g.DrawString(L"Step Badge (Middle Mouse Click):", -1, &fontReg, PointF(20, 408), &textBrush);
    GraphicsPath cBadgePath;
    AddRoundedRect(cBadgePath, (float)colorBadge.left, (float)colorBadge.top, 30.0f, 30.0f, 5.0f);
    SolidBrush brushBadge(Color(255, GetRValue(g_config.badgeColor), GetGValue(g_config.badgeColor), GetBValue(g_config.badgeColor)));
    g.FillPath(&brushBadge, &cBadgePath);
    g.DrawPath(&borderPen, &cBadgePath);
    g.DrawString(L"Click color box to change", -1, &fontSmall, PointF(65, 435), &textDim);

    g.DrawLine(&sepPen, 20, 470, 320, 470);

    // 9. Shortcuts Header
    GraphicsPath hdrPath;
    AddRoundedRect(hdrPath, (float)btnShortcutsHeader.left, (float)btnShortcutsHeader.top, (float)(btnShortcutsHeader.right - btnShortcutsHeader.left), (float)(btnShortcutsHeader.bottom - btnShortcutsHeader.top), 5.0f);
    SolidBrush hdrBg(Color(255, 28, 28, 32));
    g.FillPath(&hdrBg, &hdrPath);
    Pen hdrBorder(Color(255, 50, 50, 56), 1.0f);
    g.DrawPath(&hdrBorder, &hdrPath);

    Pen chevronPen(Color(255, 180, 180, 185), 2.0f);
    chevronPen.SetStartCap(LineCapRound);
    chevronPen.SetEndCap(LineCapRound);

    int chX = btnShortcutsHeader.left + 14;
    int chY = btnShortcutsHeader.top + 15;

    if (g_shortcutsExpanded) {
        Point pts[3] = { Point(chX - 4, chY - 2), Point(chX, chY + 2), Point(chX + 4, chY - 2) };
        g.DrawLines(&chevronPen, pts, 3);
    }
    else {
        Point pts[3] = { Point(chX - 2, chY - 4), Point(chX + 2, chY), Point(chX - 2, chY + 4) };
        g.DrawLines(&chevronPen, pts, 3);
    }

    std::wstring toggleTxt = g_shortcutsExpanded ? L"Shortcuts & Hotkeys" : L"Shortcuts & Hotkeys (click to expand)";
    StringFormat fmtNear;
    fmtNear.SetAlignment(StringAlignmentNear);
    fmtNear.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(toggleTxt.c_str(), -1, &fontReg,
        RectF((REAL)btnShortcutsHeader.left + 28, (REAL)btnShortcutsHeader.top, (REAL)(btnShortcutsHeader.right - btnShortcutsHeader.left - 30), (REAL)(btnShortcutsHeader.bottom - btnShortcutsHeader.top)),
        &fmtNear, &textBrush);

    float startY = 520.0f;
    float maxCardHeight = 188.0f;
    float visibleHeight = s_animHeight - startY - 14.0f;

    if ((g_shortcutsExpanded || s_animHeight > 555.0f) && visibleHeight > 10.0f) {
        float drawHeight = (visibleHeight < maxCardHeight) ? visibleHeight : maxCardHeight;

        RectF clipRect(20.0f, startY, 300.0f, drawHeight);
        g.SetClip(clipRect);

        SolidBrush cardBg(Color(255, 24, 24, 28));
        GraphicsPath listPath;
        AddRoundedRect(listPath, 20.0f, startY, 300.0f, maxCardHeight, 5.0f);
        g.FillPath(&cardBg, &listPath);
        Pen listBorder(Color(255, 40, 40, 46), 1.0f);
        g.DrawPath(&listBorder, &listPath);

        std::wstring shortcutsInfo =
            L"[Trigger] + Wheel     : Smooth Zoom\n"
            L"[Trigger] + LMB       : Draw Line\n"
            L"[Trigger] + RMB       : Draw Arrow\n"
            L"[Trigger] + Shift+LMB : Draw Rectangle\n"
            L"[Trigger] + O / H     : Blackout / Highlighter\n"
            L"[Trigger] + T         : Break Timer\n"
            L"  * Wheel / Shift+Wh  : +/- min / sec\n"
            L"  * Click clock to edit time directly\n"
            L"[Trigger] + MMB       : Step Badge (1, 2, 3..)\n"
            L"[Trigger] + P         : Pin drawings on screen\n"
            L"[Trigger] + Z / C     : Undo / Screenshot";

        g.DrawString(shortcutsInfo.c_str(), -1, &fontCode, PointF(28, startY + 8.0f), &textDim);

        g.ResetClip();
    }
}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER: {
        if (wParam == 99) {
            float diff = s_targetHeight - s_animHeight;
            if (fabs(diff) < 1.5f) {
                s_animHeight = s_targetHeight;
                KillTimer(hwnd, 99);
            }
            else {
                s_animHeight += diff * 0.35f;
            }
            UpdateSettingsWindowSize();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        int w = rcClient.right - rcClient.left;
        int h = rcClient.bottom - rcClient.top;

        if (w > 0 && h > 0) {
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            {
                Graphics g(memDC);
                g.SetSmoothingMode(SmoothingModeAntiAlias);
                DrawCustomUI(g);
            }

            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = (int)(LOWORD(lParam) / g_uiScale), y = (int)(HIWORD(lParam) / g_uiScale);

        if (PtInRectCust(btnBind, x, y)) {
            g_bindingMode = BindingMode::TriggerKey;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(btnBindRect, x, y)) {
            g_bindingMode = BindingMode::RectKey;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(chkResetZoomRow, x, y)) {
            g_config.resetZoomOnRelease = !g_config.resetZoomOnRelease;
            SaveConfig();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(chkKeepDrawingsRow, x, y)) {
            g_config.keepDrawingsOnRelease = !g_config.keepDrawingsOnRelease;
            SaveConfig();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(chkHideToastsRow, x, y)) {
            g_config.hideToastsFromCapture = !g_config.hideToastsFromCapture;
            ApplyToastCaptureAffinity();
            SaveConfig();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(btnShortcutsHeader, x, y)) {
            g_shortcutsExpanded = !g_shortcutsExpanded;
            s_targetHeight = g_shortcutsExpanded ? 725.0f : 555.0f;
            SetTimer(hwnd, 99, 14, NULL);
        }
        else if (PtInRectCust(colorLine, x, y) || PtInRectCust(colorArrow, x, y) || PtInRectCust(colorRect, x, y) || PtInRectCust(colorBadge, x, y)) {
            COLORREF* pTargetColor = nullptr;
            if (PtInRectCust(colorLine, x, y)) pTargetColor = &g_config.lineColor;
            else if (PtInRectCust(colorArrow, x, y)) pTargetColor = &g_config.arrowColor;
            else if (PtInRectCust(colorRect, x, y)) pTargetColor = &g_config.rectColor;
            else if (PtInRectCust(colorBadge, x, y)) pTargetColor = &g_config.badgeColor;

            if (!pTargetColor) return 0;

            COLORREF customColors[16] = { 0 };
            CHOOSECOLOR cc = { sizeof(CHOOSECOLOR), hwnd, NULL, *pTargetColor, customColors, CC_RGBINIT | CC_FULLOPEN };
            if (ChooseColor(&cc)) {
                *pTargetColor = cc.rgbResult;
                SaveConfig();
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        else if (PtInRectCust(rectSliderLine, x, y)) { draggingSlider = 1; SetCapture(hwnd); }
        else if (PtInRectCust(rectSliderArrow, x, y)) { draggingSlider = 2; SetCapture(hwnd); }
        else if (PtInRectCust(rectSliderRect, x, y)) { draggingSlider = 3; SetCapture(hwnd); }

        if (draggingSlider != 0) SendMessage(hwnd, WM_MOUSEMOVE, wParam, lParam);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (draggingSlider != 0 && (wParam & MK_LBUTTON)) {
            int x = (int)(LOWORD(lParam) / g_uiScale);
            float percent = (float)(x - (int)rectSliderLine.left) / (float)(rectSliderLine.right - rectSliderLine.left);
            if (percent < 0.0f) percent = 0.0f;
            if (percent > 1.0f) percent = 1.0f;
            int val = 1 + (int)(percent * 19.0f);

            if (draggingSlider == 1) g_config.lineWidth = val;
            else if (draggingSlider == 2) g_config.arrowWidth = val;
            else if (draggingSlider == 3) g_config.rectWidth = val;

            SaveConfig();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (draggingSlider != 0) {
            ReleaseCapture();
            draggingSlider = 0;
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 99);
        g_hwndSettings = NULL;
        g_bindingMode = BindingMode::None;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}