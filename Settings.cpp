#include "LensIt.h"

// UI Layout elements
RECT btnBind = { 20, 32, 320, 68 };
RECT chkResetZoomRow = { 20, 78, 320, 102 };
RECT chkResetZoomBox = { 20, 81, 37, 98 };

RECT colorLine = { 20, 137, 50, 167 };
RECT rectSliderLine = { 65, 137, 320, 167 };

RECT colorArrow = { 20, 202, 50, 232 };
RECT rectSliderArrow = { 65, 202, 320, 232 };

RECT btnBindRect = { 20, 266, 320, 302 };
RECT colorRect = { 20, 336, 50, 366 };
RECT rectSliderRect = { 65, 336, 320, 366 };

int draggingSlider = 0; // 1 = Line, 2 = Arrow, 3 = Rect

void ShowSettingsWindow(HINSTANCE hInstance) {
    if (g_hwndSettings) {
        SetForegroundWindow(g_hwndSettings);
        return;
    }

    RECT wr = { 0, 0, 340, 480 };
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
    g.Clear(Color(255, 20, 20, 20));

    Font fontTitle(L"Segoe UI", 10, FontStyleBold);
    Font fontReg(L"Segoe UI", 9.5f);
    Font fontSmall(L"Segoe UI", 8.5f);
    SolidBrush textBrush(Color(225, 225, 225));
    SolidBrush textDim(Color(150, 150, 150));
    SolidBrush accent(Color(55, 55, 55));
    SolidBrush activeAccent(Color(0, 122, 204));
    SolidBrush bindActiveBrush(Color(80, 80, 80));
    SolidBrush thumbBrush(Color(215, 215, 215));
    Pen borderPen(Color(100, 100, 100), 1.0f);
    Pen sepPen(Color(45, 45, 45), 1.0f);

    StringFormat fmtCenter;
    fmtCenter.SetAlignment(StringAlignmentCenter);
    fmtCenter.SetLineAlignment(StringAlignmentCenter);

    // 1. Trigger Key
    g.DrawString(L"Trigger Key (Hold):", -1, &fontReg, PointF(20, 12), &textBrush);
    bool isTrigBinding = (g_bindingMode == BindingMode::TriggerKey);
    g.FillRectangle(isTrigBinding ? &bindActiveBrush : &accent,
        (int)btnBind.left, (int)btnBind.top, (int)(btnBind.right - btnBind.left), (int)(btnBind.bottom - btnBind.top));
    g.DrawRectangle(&borderPen,
        (int)btnBind.left, (int)btnBind.top, (int)(btnBind.right - btnBind.left), (int)(btnBind.bottom - btnBind.top));

    std::wstring bindTxt = isTrigBinding ? L"Press any key..." : (L"[ " + GetKeyNameStr(g_config.triggerKey) + L" ]");
    g.DrawString(bindTxt.c_str(), -1, &fontTitle,
        RectF((REAL)btnBind.left, (REAL)btnBind.top, (REAL)(btnBind.right - btnBind.left), (REAL)(btnBind.bottom - btnBind.top)),
        &fmtCenter, &textBrush);

    // 2. Checkbox: Reset zoom on release
    SolidBrush chkBg(Color(32, 32, 32));
    g.FillRectangle(&chkBg, (int)chkResetZoomBox.left, (int)chkResetZoomBox.top, 17, 17);
    g.DrawRectangle(&borderPen, (int)chkResetZoomBox.left, (int)chkResetZoomBox.top, 17, 17);

    if (g_config.resetZoomOnRelease) {
        g.FillRectangle(&activeAccent, (int)chkResetZoomBox.left + 3, (int)chkResetZoomBox.top + 3, 11, 11);
        Pen checkPen(Color(255, 255, 255), 2.0f);
        Point checkPts[3] = {
            Point((int)chkResetZoomBox.left + 4, (int)chkResetZoomBox.top + 8),
            Point((int)chkResetZoomBox.left + 7, (int)chkResetZoomBox.top + 13),
            Point((int)chkResetZoomBox.left + 14, (int)chkResetZoomBox.top + 5)
        };
        g.DrawLines(&checkPen, checkPts, 3);
    }
    g.DrawString(L"Reset zoom on trigger key release", -1, &fontReg, PointF((REAL)chkResetZoomBox.right + 8, (REAL)chkResetZoomBox.top - 1), &textBrush);

    // Separator
    g.DrawLine(&sepPen, 20, 108, 320, 108);

    // 3. Line
    std::wstring lineTitle = L"Line (LMB) - " + std::to_wstring(g_config.lineWidth) + L" px:";
    g.DrawString(lineTitle.c_str(), -1, &fontReg, PointF(20, 115), &textBrush);
    Color cL(255, GetRValue(g_config.lineColor), GetGValue(g_config.lineColor), GetBValue(g_config.lineColor));
    SolidBrush brushLine(cL);
    g.FillRectangle(&brushLine, (int)colorLine.left, (int)colorLine.top, 30, 30);
    g.DrawRectangle(&borderPen, (int)colorLine.left, (int)colorLine.top, 30, 30);

    g.FillRectangle(&accent, (int)rectSliderLine.left, (int)rectSliderLine.top + 13, 255, 4);
    int thumbLineX = (int)rectSliderLine.left + (int)(((g_config.lineWidth - 1) / 19.0f) * 255.0f);
    g.FillEllipse(&thumbBrush, thumbLineX - 7, (int)rectSliderLine.top + 8, 14, 14);

    // 4. Arrow
    std::wstring arrowTitle = L"Arrow (RMB) - " + std::to_wstring(g_config.arrowWidth) + L" px:";
    g.DrawString(arrowTitle.c_str(), -1, &fontReg, PointF(20, 180), &textBrush);
    Color cA(255, GetRValue(g_config.arrowColor), GetGValue(g_config.arrowColor), GetBValue(g_config.arrowColor));
    SolidBrush brushArrow(cA);
    g.FillRectangle(&brushArrow, (int)colorArrow.left, (int)colorArrow.top, 30, 30);
    g.DrawRectangle(&borderPen, (int)colorArrow.left, (int)colorArrow.top, 30, 30);

    g.FillRectangle(&accent, (int)rectSliderArrow.left, (int)rectSliderArrow.top + 13, 255, 4);
    int thumbArrowX = (int)rectSliderArrow.left + (int)(((g_config.arrowWidth - 1) / 19.0f) * 255.0f);
    g.FillEllipse(&thumbBrush, thumbArrowX - 7, (int)rectSliderArrow.top + 8, 14, 14);

    // 5. Rectangle Key & Tools
    g.DrawString(L"Rectangle Modifier Key:", -1, &fontReg, PointF(20, 245), &textBrush);
    bool isRectBinding = (g_bindingMode == BindingMode::RectKey);
    g.FillRectangle(isRectBinding ? &bindActiveBrush : &accent,
        (int)btnBindRect.left, (int)btnBindRect.top, (int)(btnBindRect.right - btnBindRect.left), (int)(btnBindRect.bottom - btnBindRect.top));
    g.DrawRectangle(&borderPen,
        (int)btnBindRect.left, (int)btnBindRect.top, (int)(btnBindRect.right - btnBindRect.left), (int)(btnBindRect.bottom - btnBindRect.top));

    std::wstring bindRectTxt = isRectBinding ? L"Press any key..." : (L"[ " + GetKeyNameStr(g_config.rectKey) + L" ]");
    g.DrawString(bindRectTxt.c_str(), -1, &fontTitle,
        RectF((REAL)btnBindRect.left, (REAL)btnBindRect.top, (REAL)(btnBindRect.right - btnBindRect.left), (REAL)(btnBindRect.bottom - btnBindRect.top)),
        &fmtCenter, &textBrush);

    std::wstring rectTitle = L"Rectangle - " + std::to_wstring(g_config.rectWidth) + L" px:";
    g.DrawString(rectTitle.c_str(), -1, &fontReg, PointF(20, 314), &textBrush);
    Color cR(255, GetRValue(g_config.rectColor), GetGValue(g_config.rectColor), GetBValue(g_config.rectColor));
    SolidBrush brushRect(cR);
    g.FillRectangle(&brushRect, (int)colorRect.left, (int)colorRect.top, 30, 30);
    g.DrawRectangle(&borderPen, (int)colorRect.left, (int)colorRect.top, 30, 30);

    g.FillRectangle(&accent, (int)rectSliderRect.left, (int)rectSliderRect.top + 13, 255, 4);
    int thumbRectX = (int)rectSliderRect.left + (int)(((g_config.rectWidth - 1) / 19.0f) * 255.0f);
    g.FillEllipse(&thumbBrush, thumbRectX - 7, (int)rectSliderRect.top + 8, 14, 14);

    // 6. Footer guide
    g.DrawLine(&sepPen, 20, 385, 320, 385);
    g.DrawString(L"Shortcuts:", -1, &fontSmall, PointF(20, 395), &textDim);
    g.DrawString(L"* [Trigger] + Wheel: Zoom In / Out\n* [Trigger] + LMB: Line  |  RMB: Arrow\n* [Trigger] + [Rect Key] + LMB: Rectangle\n* ESC: Reset Zoom & Clear drawings", -1, &fontSmall, PointF(20, 412), &textDim);
}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        DrawCustomUI(g);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam), y = HIWORD(lParam);

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
        else if (PtInRectCust(colorLine, x, y) || PtInRectCust(colorArrow, x, y) || PtInRectCust(colorRect, x, y)) {
            COLORREF* pTargetColor = nullptr;
            if (PtInRectCust(colorLine, x, y)) pTargetColor = &g_config.lineColor;
            else if (PtInRectCust(colorArrow, x, y)) pTargetColor = &g_config.arrowColor;
            else if (PtInRectCust(colorRect, x, y)) pTargetColor = &g_config.rectColor;

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
            int x = LOWORD(lParam);
            float percent = (float)(x - (int)rectSliderLine.left) / 255.0f;
            if (percent < 0.0f) percent = 0.0f;
            if (percent > 1.0f) percent = 1.0f;
            int val = 1 + (int)(percent * 19.0f); // 1..20

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

    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        g_hwndSettings = NULL;
        g_bindingMode = BindingMode::None;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}