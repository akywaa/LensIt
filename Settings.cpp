#include "LensIt.h"
#include "WinHandles.h"
#include <windowsx.h>

namespace {

constexpr int kWindowWidth = 340;
constexpr float kWindowHeightCollapsed = 555.0f;
constexpr float kWindowHeightExpanded = 785.0f;

}

static float g_uiScale = 1.0f;
static bool g_shortcutsExpanded = false;

static float s_animHeight = kWindowHeightCollapsed;
static float s_targetHeight = kWindowHeightCollapsed;

struct FormLayout {
    RECT btnBind;
    RECT chkResetZoomRow;
    RECT chkKeepDrawingsRow;
    RECT chkHideToastsRow;
    RECT colorLine;
    RECT rectSliderLine;
    RECT colorArrow;
    RECT rectSliderArrow;
    RECT btnBindRect;
    RECT colorRect;
    RECT rectSliderRect;
    RECT colorBadge;
    RECT btnShortcutsHeader;
};

const FormLayout& CurrentLayout() {
    static const FormLayout l = [] {
        FormLayout l;

        int y = 32;
        l.btnBind = { 20, y, 320, y + 34 }; y += 42;
        l.chkResetZoomRow = { 20, y, 320, y + 24 }; y += 28;
        l.chkKeepDrawingsRow = { 20, y, 320, y + 24 }; y += 28;
        l.chkHideToastsRow = { 20, y, 320, y + 24 }; y += 32;

        y = 183;
        l.colorLine = { 20, y, 50, y + 30 };
        l.rectSliderLine = { 65, y, 320, y + 30 }; y += 60;
        l.colorArrow = { 20, y, 50, y + 30 };
        l.rectSliderArrow = { 65, y, 320, y + 30 }; y += 61;
        l.btnBindRect = { 20, y, 320, y + 34 }; y += 64;
        l.colorRect = { 20, y, 50, y + 30 };
        l.rectSliderRect = { 65, y, 320, y + 30 }; y += 60;
        l.colorBadge = { 20, y, 50, y + 30 }; y += 52;
        l.btnShortcutsHeader = { 20, y, 320, y + 30 };

        return l;
    }();
    return l;
}

int draggingSlider = 0;

enum class ColorPickerTarget {
    None,
    Line,
    Arrow,
    Rect,
    Badge
};

static bool s_pickerOpen = false;
static ColorPickerTarget s_pickerTarget = ColorPickerTarget::None;
static COLORREF s_pickerColor = RGB(255, 255, 255);
static COLORREF s_pickerOriginalColor = RGB(255, 255, 255);
static int s_pickerDraggingSlider = 0; // 1 = R, 2 = G, 3 = B

static const COLORREF s_paletteColors[16] = {
    RGB(255, 0, 0),
    RGB(255, 102, 0),
    RGB(255, 255, 0),
    RGB(0, 255, 0),
    RGB(0, 204, 204),
    RGB(0, 128, 255),
    RGB(0, 0, 255),
    RGB(153, 0, 255),
    RGB(255, 0, 255),
    RGB(255, 128, 192),
    RGB(0, 128, 0),
    RGB(128, 64, 0),
    RGB(255, 255, 255),
    RGB(128, 128, 128),
    RGB(64, 64, 64),
    RGB(0, 0, 0)
};

static const RECT pickerCard = { 22, 90, 318, 415 };
static const RECT btnPickerClose = { 286, 98, 310, 122 };
static const RECT btnPickerCancel = { 34, 366, 166, 400 };
static const RECT btnPickerApply = { 174, 366, 306, 400 };

static const RECT rectTrackR = { 86, 258, 270, 276 };
static const RECT rectTrackG = { 86, 288, 270, 306 };
static const RECT rectTrackB = { 86, 318, 270, 336 };

// Helper drawing routines
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
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);
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
    float percent = static_cast<float>(val - minVal) / static_cast<float>(maxVal - minVal);
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 1.0f) percent = 1.0f;

    float totalW = static_cast<float>(trackRect.right - trackRect.left);
    float centerY = static_cast<float>(trackRect.top + (trackRect.bottom - trackRect.top) / 2);
    float startX = static_cast<float>(trackRect.left);

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

    RECT wr = { 0, 0, static_cast<int>(kWindowWidth * g_uiScale), static_cast<int>(s_animHeight * g_uiScale) };
    AdjustWindowRectExForDpi(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOPMOST, GetDpiForWindow(g_hwndSettings));
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;

    SetWindowPos(g_hwndSettings, HWND_TOPMOST, 0, 0, w, h, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
}

void ShowSettingsWindow(HINSTANCE hInstance) {
    if (g_hwndSettings) {
        SetForegroundWindow(g_hwndSettings);
        return;
    }

    s_animHeight = s_targetHeight = g_shortcutsExpanded ? kWindowHeightExpanded : kWindowHeightCollapsed;

    g_hwndSettings = CreateWindowEx(
        WS_EX_TOPMOST, L"LensItSettings", L"LensIt Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, kWindowWidth, static_cast<int>(s_animHeight), NULL, NULL, hInstance, NULL
    );

    UINT dpi = GetDpiForWindow(g_hwndSettings);
    g_uiScale = (dpi > 0) ? (static_cast<float>(dpi) / 96.0f) : 1.0f;

    RECT wr = { 0, 0, static_cast<int>(kWindowWidth * g_uiScale), static_cast<int>(s_animHeight * g_uiScale) };
    AdjustWindowRectExForDpi(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOPMOST, dpi);
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;
    int cx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int cy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    SetWindowPos(g_hwndSettings, NULL, cx, cy, w, h, SWP_NOZORDER | SWP_NOACTIVATE);

    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_hwndSettings, 20, &dark, sizeof(dark));

    ShowWindow(g_hwndSettings, SW_SHOW);
}

bool PtInRectCust(RECT r, int x, int y) {
    return (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom);
}

// Color Picker Helpers
static COLORREF* GetTargetColorPtr(ColorPickerTarget target) {
    switch (target) {
    case ColorPickerTarget::Line:  return &g_config.lineColor;
    case ColorPickerTarget::Arrow: return &g_config.arrowColor;
    case ColorPickerTarget::Rect:  return &g_config.rectColor;
    case ColorPickerTarget::Badge: return &g_config.badgeColor;
    default: return nullptr;
    }
}

static void OpenColorPicker(ColorPickerTarget target) {
    COLORREF* p = GetTargetColorPtr(target);
    if (!p) return;
    s_pickerTarget = target;
    s_pickerColor = *p;
    s_pickerOriginalColor = *p;
    s_pickerOpen = true;
    s_pickerDraggingSlider = 0;
}

static void ApplyColorPicker() {
    COLORREF* p = GetTargetColorPtr(s_pickerTarget);
    if (p) {
        *p = s_pickerColor;
        SaveConfig();
    }
    s_pickerOpen = false;
    s_pickerTarget = ColorPickerTarget::None;
}

static void CancelColorPicker() {
    COLORREF* p = GetTargetColorPtr(s_pickerTarget);
    if (p) {
        *p = s_pickerOriginalColor;
        SaveConfig();
    }
    s_pickerOpen = false;
    s_pickerTarget = ColorPickerTarget::None;
}

static RECT GetPaletteSwatchRect(int index) {
    int row = index / 8;
    int col = index % 8;
    int left = 34 + col * 34;
    int top = 188 + row * 30;
    return RECT{ left, top, left + 26, top + 22 };
}

static void DrawColorPickerModal(Graphics& g) {
    if (!s_pickerOpen) return;

    // Backdrop & card
    SolidBrush dimBrush(Color(180, 8, 8, 12));
    g.FillRectangle(&dimBrush, 0, 0, kWindowWidth, static_cast<int>(s_animHeight) + 40);

    GraphicsPath cardPath;
    AddRoundedRect(cardPath, static_cast<float>(pickerCard.left), static_cast<float>(pickerCard.top),
        static_cast<float>(pickerCard.right - pickerCard.left), static_cast<float>(pickerCard.bottom - pickerCard.top), 8.0f);

    SolidBrush cardBg(Color(255, 26, 26, 30));
    g.FillPath(&cardBg, &cardPath);

    Pen cardBorder(Color(255, 62, 62, 70), 1.0f);
    g.DrawPath(&cardBorder, &cardPath);

    // Header & title
    Font fontTitle(L"Segoe UI", 10.0f, FontStyleBold);
    Font fontReg(L"Segoe UI", 9.0f);
    Font fontSmall(L"Segoe UI", 8.0f);
    Font fontCode(L"Consolas", 9.5f, FontStyleBold);

    SolidBrush textWhite(Color(255, 240, 240, 245));
    SolidBrush textDim(Color(255, 150, 150, 158));

    std::wstring title = L"Select Color";
    if (s_pickerTarget == ColorPickerTarget::Line) title = L"Line Color";
    else if (s_pickerTarget == ColorPickerTarget::Arrow) title = L"Arrow Color";
    else if (s_pickerTarget == ColorPickerTarget::Rect) title = L"Rectangle Color";
    else if (s_pickerTarget == ColorPickerTarget::Badge) title = L"Step Badge Color";

    g.DrawString(title.c_str(), -1, &fontTitle, PointF(36.0f, 104.0f), &textWhite);

    // Close button [x]
    Pen xPen(Color(255, 170, 170, 175), 1.5f);
    xPen.SetStartCap(LineCapRound);
    xPen.SetEndCap(LineCapRound);
    g.DrawLine(&xPen, 293, 105, 303, 115);
    g.DrawLine(&xPen, 303, 105, 293, 115);

    // Old vs new color, plus hex value
    GraphicsPath oldBoxPath;
    AddRoundedRect(oldBoxPath, 34.0f, 136.0f, 38.0f, 30.0f, 4.0f);
    SolidBrush oldColorBrush(Color(255, GetRValue(s_pickerOriginalColor), GetGValue(s_pickerOriginalColor), GetBValue(s_pickerOriginalColor)));
    g.FillPath(&oldColorBrush, &oldBoxPath);
    Pen borderPen(Color(255, 55, 55, 60), 1.0f);
    g.DrawPath(&borderPen, &oldBoxPath);
    g.DrawString(L"OLD", -1, &fontSmall, PointF(41.0f, 168.0f), &textDim);

    GraphicsPath newBoxPath;
    AddRoundedRect(newBoxPath, 80.0f, 136.0f, 48.0f, 30.0f, 4.0f);
    SolidBrush newColorBrush(Color(255, GetRValue(s_pickerColor), GetGValue(s_pickerColor), GetBValue(s_pickerColor)));
    g.FillPath(&newColorBrush, &newBoxPath);
    Pen newBorderPen(Color(255, 90, 90, 100), 1.0f);
    g.DrawPath(&newBorderPen, &newBoxPath);
    g.DrawString(L"NEW", -1, &fontSmall, PointF(92.0f, 168.0f), &textDim);

    GraphicsPath hexPath;
    AddRoundedRect(hexPath, 138.0f, 136.0f, 168.0f, 30.0f, 4.0f);
    SolidBrush hexBg(Color(255, 20, 20, 22));
    g.FillPath(&hexBg, &hexPath);
    Pen hexBorder(Color(255, 48, 48, 55), 1.0f);
    g.DrawPath(&hexBorder, &hexPath);

    wchar_t hexBuf[32];
    swprintf_s(hexBuf, L"#%02X%02X%02X", GetRValue(s_pickerColor), GetGValue(s_pickerColor), GetBValue(s_pickerColor));
    StringFormat fmtCenter;
    fmtCenter.SetAlignment(StringAlignmentCenter);
    fmtCenter.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(hexBuf, -1, &fontCode, RectF(138.0f, 136.0f, 168.0f, 30.0f), &fmtCenter, &textWhite);

    // Palette swatches
    for (int i = 0; i < 16; ++i) {
        RECT r = GetPaletteSwatchRect(i);
        GraphicsPath swatchPath;
        AddRoundedRect(swatchPath, static_cast<float>(r.left), static_cast<float>(r.top), static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top), 4.0f);

        COLORREF c = s_paletteColors[i];
        SolidBrush swatchBrush(Color(255, GetRValue(c), GetGValue(c), GetBValue(c)));
        g.FillPath(&swatchBrush, &swatchPath);

        if (c == s_pickerColor) {
            Pen selPen(Color(255, 255, 255, 255), 2.0f);
            g.DrawPath(&selPen, &swatchPath);
        }
        else {
            Pen swatchBorder(Color(255, 45, 45, 50), 1.0f);
            g.DrawPath(&swatchBorder, &swatchPath);
        }
    }

    // RGB custom sliders
    int rVal = GetRValue(s_pickerColor);
    int gVal = GetGValue(s_pickerColor);
    int bVal = GetBValue(s_pickerColor);

    wchar_t bufVal[16];
    swprintf_s(bufVal, L"R  %3d", rVal);
    g.DrawString(bufVal, -1, &fontSmall, PointF(34.0f, 260.0f), &textDim);
    DrawModernSlider(g, rectTrackR, rVal, 0, 255, RGB(235, 60, 60));

    swprintf_s(bufVal, L"G  %3d", gVal);
    g.DrawString(bufVal, -1, &fontSmall, PointF(34.0f, 290.0f), &textDim);
    DrawModernSlider(g, rectTrackG, gVal, 0, 255, RGB(60, 200, 90));

    swprintf_s(bufVal, L"B  %3d", bVal);
    g.DrawString(bufVal, -1, &fontSmall, PointF(34.0f, 320.0f), &textDim);
    DrawModernSlider(g, rectTrackB, bVal, 0, 255, RGB(60, 145, 255));

    // Cancel / Apply buttons
    GraphicsPath btnCancelPath;
    AddRoundedRect(btnCancelPath, static_cast<float>(btnPickerCancel.left), static_cast<float>(btnPickerCancel.top),
        static_cast<float>(btnPickerCancel.right - btnPickerCancel.left), static_cast<float>(btnPickerCancel.bottom - btnPickerCancel.top), 5.0f);
    SolidBrush cancelBg(Color(255, 34, 34, 40));
    g.FillPath(&cancelBg, &btnCancelPath);
    Pen cancelBorder(Color(255, 60, 60, 68), 1.0f);
    g.DrawPath(&cancelBorder, &btnCancelPath);
    g.DrawString(L"Cancel", -1, &fontReg,
        RectF(static_cast<REAL>(btnPickerCancel.left), static_cast<REAL>(btnPickerCancel.top), static_cast<REAL>(btnPickerCancel.right - btnPickerCancel.left), static_cast<REAL>(btnPickerCancel.bottom - btnPickerCancel.top)),
        &fmtCenter, &textWhite);

    GraphicsPath btnApplyPath;
    AddRoundedRect(btnApplyPath, static_cast<float>(btnPickerApply.left), static_cast<float>(btnPickerApply.top),
        static_cast<float>(btnPickerApply.right - btnPickerApply.left), static_cast<float>(btnPickerApply.bottom - btnPickerApply.top), 5.0f);
    SolidBrush applyBg(Color(255, 0, 120, 215));
    g.FillPath(&applyBg, &btnApplyPath);
    Pen applyBorder(Color(255, 0, 150, 255), 1.0f);
    g.DrawPath(&applyBorder, &btnApplyPath);
    Font fontBold(L"Segoe UI", 9.0f, FontStyleBold);
    g.DrawString(L"Apply", -1, &fontBold,
        RectF(static_cast<REAL>(btnPickerApply.left), static_cast<REAL>(btnPickerApply.top), static_cast<REAL>(btnPickerApply.right - btnPickerApply.left), static_cast<REAL>(btnPickerApply.bottom - btnPickerApply.top)),
        &fmtCenter, &textWhite);
}

void DrawCustomUI(Graphics& g) {
    const FormLayout& l = CurrentLayout();
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

    // Trigger key
    g.DrawString(L"Trigger Key (Hold):", -1, &fontReg, PointF(20, 12), &textBrush);
    bool isTrigBinding = (g_bindingMode == BindingMode::TriggerKey);

    GraphicsPath btnPath;
    AddRoundedRect(btnPath, static_cast<float>(l.btnBind.left), static_cast<float>(l.btnBind.top), static_cast<float>(l.btnBind.right - l.btnBind.left), static_cast<float>(l.btnBind.bottom - l.btnBind.top), 5.0f);
    SolidBrush trigBrush(isTrigBinding ? Color(255, 0, 120, 215) : Color(255, 32, 32, 36));
    g.FillPath(&trigBrush, &btnPath);
    Pen trigBorder(isTrigBinding ? Color(255, 0, 160, 255) : Color(255, 60, 60, 65), 1.0f);
    g.DrawPath(&trigBorder, &btnPath);

    std::wstring bindTxt = isTrigBinding ? L"Press any key or mouse button..." : (L"[  " + GetKeyNameStr(g_config.triggerKey) + L"  ]");
    g.DrawString(bindTxt.c_str(), -1, &fontTitle,
        RectF(static_cast<REAL>(l.btnBind.left), static_cast<REAL>(l.btnBind.top), static_cast<REAL>(l.btnBind.right - l.btnBind.left), static_cast<REAL>(l.btnBind.bottom - l.btnBind.top)),
        &fmtCenter, &textBrush);

    // Reset zoom on trigger release
    DrawModernToggle(g, l.btnBind.left, l.chkResetZoomRow.top + 3, g_config.resetZoomOnRelease);
    g.DrawString(L"Reset zoom on trigger release", -1, &fontReg, PointF(62, static_cast<REAL>(l.chkResetZoomRow.top) + 2), &textBrush);

    // Keep drawings on screen
    DrawModernToggle(g, l.btnBind.left, l.chkKeepDrawingsRow.top + 3, g_config.keepDrawingsOnRelease);
    g.DrawString(L"Keep drawings on screen (Click-through)", -1, &fontReg, PointF(62, static_cast<REAL>(l.chkKeepDrawingsRow.top) + 2), &textBrush);

    // Hide toasts from screen capture
    DrawModernToggle(g, l.btnBind.left, l.chkHideToastsRow.top + 3, g_config.hideToastsFromCapture);
    g.DrawString(L"Hide notifications from screen capture", -1, &fontReg, PointF(62, static_cast<REAL>(l.chkHideToastsRow.top) + 2), &textBrush);

    g.DrawLine(&sepPen, 20, 162, 320, 162);

    // Line tool
    std::wstring lineTitle = L"Line (LMB)  *  " + std::to_wstring(g_config.lineWidth) + L" px";
    g.DrawString(lineTitle.c_str(), -1, &fontReg, PointF(20, 165), &textBrush);

    GraphicsPath cLinePath;
    AddRoundedRect(cLinePath, static_cast<float>(l.colorLine.left), static_cast<float>(l.colorLine.top), 30.0f, 30.0f, 5.0f);
    SolidBrush brushLine(Color(255, GetRValue(g_config.lineColor), GetGValue(g_config.lineColor), GetBValue(g_config.lineColor)));
    g.FillPath(&brushLine, &cLinePath);
    g.DrawPath(&borderPen, &cLinePath);

    DrawModernSlider(g, l.rectSliderLine, g_config.lineWidth, 1, 20, g_config.lineColor);

    // Arrow tool
    std::wstring arrowTitle = L"Arrow (RMB)  *  " + std::to_wstring(g_config.arrowWidth) + L" px";
    g.DrawString(arrowTitle.c_str(), -1, &fontReg, PointF(20, 225), &textBrush);

    GraphicsPath cArrowPath;
    AddRoundedRect(cArrowPath, static_cast<float>(l.colorArrow.left), static_cast<float>(l.colorArrow.top), 30.0f, 30.0f, 5.0f);
    SolidBrush brushArrow(Color(255, GetRValue(g_config.arrowColor), GetGValue(g_config.arrowColor), GetBValue(g_config.arrowColor)));
    g.FillPath(&brushArrow, &cArrowPath);
    g.DrawPath(&borderPen, &cArrowPath);

    DrawModernSlider(g, l.rectSliderArrow, g_config.arrowWidth, 1, 20, g_config.arrowColor);

    // Rectangle modifier key & tool
    g.DrawString(L"Rectangle Modifier Key:", -1, &fontReg, PointF(20, 283), &textBrush);
    bool isRectBinding = (g_bindingMode == BindingMode::RectKey);

    GraphicsPath btnRectPath;
    AddRoundedRect(btnRectPath, static_cast<float>(l.btnBindRect.left), static_cast<float>(l.btnBindRect.top), static_cast<float>(l.btnBindRect.right - l.btnBindRect.left), static_cast<float>(l.btnBindRect.bottom - l.btnBindRect.top), 5.0f);
    SolidBrush rectBindBrush(isRectBinding ? Color(255, 0, 120, 215) : Color(255, 32, 32, 36));
    g.FillPath(&rectBindBrush, &btnRectPath);
    Pen rectBorder(isRectBinding ? Color(255, 0, 160, 255) : Color(255, 60, 60, 65), 1.0f);
    g.DrawPath(&rectBorder, &btnRectPath);

    std::wstring bindRectTxt = isRectBinding ? L"Press any key or mouse button..." : (L"[  " + GetKeyNameStr(g_config.rectKey) + L"  ]");
    g.DrawString(bindRectTxt.c_str(), -1, &fontTitle,
        RectF(static_cast<REAL>(l.btnBindRect.left), static_cast<REAL>(l.btnBindRect.top), static_cast<REAL>(l.btnBindRect.right - l.btnBindRect.left), static_cast<REAL>(l.btnBindRect.bottom - l.btnBindRect.top)),
        &fmtCenter, &textBrush);

    std::wstring rectTitle = L"Rectangle  *  " + std::to_wstring(g_config.rectWidth) + L" px";
    g.DrawString(rectTitle.c_str(), -1, &fontReg, PointF(20, 348), &textBrush);

    GraphicsPath cRectPath;
    AddRoundedRect(cRectPath, static_cast<float>(l.colorRect.left), static_cast<float>(l.colorRect.top), 30.0f, 30.0f, 5.0f);
    SolidBrush brushRect(Color(255, GetRValue(g_config.rectColor), GetGValue(g_config.rectColor), GetBValue(g_config.rectColor)));
    g.FillPath(&brushRect, &cRectPath);
    g.DrawPath(&borderPen, &cRectPath);

    DrawModernSlider(g, l.rectSliderRect, g_config.rectWidth, 1, 20, g_config.rectColor);

    // Step badge
    g.DrawString(L"Step Badge (Middle Mouse Click):", -1, &fontReg, PointF(20, 408), &textBrush);
    GraphicsPath cBadgePath;
    AddRoundedRect(cBadgePath, static_cast<float>(l.colorBadge.left), static_cast<float>(l.colorBadge.top), 30.0f, 30.0f, 5.0f);
    SolidBrush brushBadge(Color(255, GetRValue(g_config.badgeColor), GetGValue(g_config.badgeColor), GetBValue(g_config.badgeColor)));
    g.FillPath(&brushBadge, &cBadgePath);
    g.DrawPath(&borderPen, &cBadgePath);
    g.DrawString(L"Click color box to customize", -1, &fontSmall, PointF(65, 435), &textDim);

    g.DrawLine(&sepPen, 20, 470, 320, 470);

    // Shortcuts header
    GraphicsPath hdrPath;
    AddRoundedRect(hdrPath, static_cast<float>(l.btnShortcutsHeader.left), static_cast<float>(l.btnShortcutsHeader.top), static_cast<float>(l.btnShortcutsHeader.right - l.btnShortcutsHeader.left), static_cast<float>(l.btnShortcutsHeader.bottom - l.btnShortcutsHeader.top), 5.0f);
    SolidBrush hdrBg(Color(255, 28, 28, 32));
    g.FillPath(&hdrBg, &hdrPath);
    Pen hdrBorder(Color(255, 50, 50, 56), 1.0f);
    g.DrawPath(&hdrBorder, &hdrPath);

    Pen chevronPen(Color(255, 180, 180, 185), 2.0f);
    chevronPen.SetStartCap(LineCapRound);
    chevronPen.SetEndCap(LineCapRound);

    int chX = l.btnShortcutsHeader.left + 14;
    int chY = l.btnShortcutsHeader.top + 15;

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
        RectF(static_cast<REAL>(l.btnShortcutsHeader.left) + 28, static_cast<REAL>(l.btnShortcutsHeader.top), static_cast<REAL>(l.btnShortcutsHeader.right - l.btnShortcutsHeader.left - 30), static_cast<REAL>(l.btnShortcutsHeader.bottom - l.btnShortcutsHeader.top)),
        &fmtNear, &textBrush);

    float startY = 520.0f;
    float maxCardHeight = 244.0f;
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
            L"[Trigger] + V         : Laser Ink (vanishing)\n"
            L"[Trigger] + X         : Text on screen (Enter = commit)\n"
            L"[Trigger] + S         : Spotlight dimmer\n"
            L"[Trigger] + W         : Whiteboard (W again = dark, off)\n"
            L"[Trigger] + T         : Break Timer\n"
            L"  * Wheel / Shift+Wh  : +/- min / sec\n"
            L"  * Click clock to edit time directly\n"
            L"[Trigger] + MMB       : Step Badge (1, 2, 3..)\n"
            L"[Trigger] + P         : Pin drawings on screen\n"
            L"[Trigger] + Z / C     : Undo / Screenshot\n"
            L"[Trigger] + Shift+C   : Crop screenshot (drag area)\n"
            L"[Trigger] + Shift+drag: Snap line/arrow 0/45/90\n"
            L"[Trigger] + K         : Keystroke HUD on/off";

        g.DrawString(shortcutsInfo.c_str(), -1, &fontCode, PointF(28, startY + 8.0f), &textDim);

        g.ResetClip();
    }

    // Color picker modal on top
    DrawColorPickerModal(g);
}

// Window Procedure
LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            if (g_bindingMode != BindingMode::None) {
                g_bindingMode = BindingMode::None;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            if (draggingSlider != 0 || s_pickerDraggingSlider != 0) {
                ReleaseCapture();
                if (draggingSlider != 0) SaveConfig();
                draggingSlider = 0;
                s_pickerDraggingSlider = 0;
            }
        }
        return 0;

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

    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE && s_pickerOpen) {
            CancelColorPicker();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        break;
    }

    case WM_DPICHANGED: {
        g_uiScale = static_cast<float>(LOWORD(wParam)) / 96.0f;
        RECT* const prcNewWindow = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd, NULL,
            prcNewWindow->left, prcNewWindow->top,
            prcNewWindow->right - prcNewWindow->left,
            prcNewWindow->bottom - prcNewWindow->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, NULL, FALSE);
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
            ScopedMemoryDC memDC(hdc);
            UniqueBitmap memBmp(static_cast<HBITMAP>(CreateCompatibleBitmap(hdc, w, h)));

            if (memDC && memBmp) {
                ScopedSelectedObject selected(memDC.get(), memBmp.get());
                {
                    Graphics g(memDC.get());
                    g.SetSmoothingMode(SmoothingModeAntiAlias);
                    DrawCustomUI(g);
                }

                BitBlt(hdc, 0, 0, w, h, memDC.get(), 0, 0, SRCCOPY);
            }
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = static_cast<int>(GET_X_LPARAM(lParam) / g_uiScale), y = static_cast<int>(GET_Y_LPARAM(lParam) / g_uiScale);

        // Intercept inputs when Modern Color Picker is open
        if (s_pickerOpen) {
            if (!PtInRectCust(pickerCard, x, y)) {
                CancelColorPicker();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            if (PtInRectCust(btnPickerClose, x, y) || PtInRectCust(btnPickerCancel, x, y)) {
                CancelColorPicker();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            if (PtInRectCust(btnPickerApply, x, y)) {
                ApplyColorPicker();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // Check swatches
            for (int i = 0; i < 16; ++i) {
                RECT r = GetPaletteSwatchRect(i);
                if (PtInRectCust(r, x, y)) {
                    s_pickerColor = s_paletteColors[i];
                    COLORREF* p = GetTargetColorPtr(s_pickerTarget);
                    if (p) *p = s_pickerColor; // Live preview
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Check RGB Sliders
            if (PtInRectCust(rectTrackR, x, y)) s_pickerDraggingSlider = 1;
            else if (PtInRectCust(rectTrackG, x, y)) s_pickerDraggingSlider = 2;
            else if (PtInRectCust(rectTrackB, x, y)) s_pickerDraggingSlider = 3;

            if (s_pickerDraggingSlider != 0) {
                SetCapture(hwnd);
                SendMessage(hwnd, WM_MOUSEMOVE, wParam, lParam);
            }
            return 0;
        }

        // Standard settings clicks
        const FormLayout& l = CurrentLayout();
        if (PtInRectCust(l.btnBind, x, y)) {
            g_bindingMode = BindingMode::TriggerKey;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(l.btnBindRect, x, y)) {
            g_bindingMode = BindingMode::RectKey;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(l.chkResetZoomRow, x, y)) {
            g_config.resetZoomOnRelease = !g_config.resetZoomOnRelease;
            SaveConfig();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(l.chkKeepDrawingsRow, x, y)) {
            g_config.keepDrawingsOnRelease = !g_config.keepDrawingsOnRelease;
            SaveConfig();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(l.chkHideToastsRow, x, y)) {
            g_config.hideToastsFromCapture = !g_config.hideToastsFromCapture;
            ApplyToastCaptureAffinity();
            SaveConfig();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(l.btnShortcutsHeader, x, y)) {
            g_shortcutsExpanded = !g_shortcutsExpanded;
            s_targetHeight = g_shortcutsExpanded ? 785.0f : 555.0f;
            SetTimer(hwnd, 99, 14, NULL);
        }
        else if (PtInRectCust(l.colorLine, x, y)) {
            OpenColorPicker(ColorPickerTarget::Line);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(l.colorArrow, x, y)) {
            OpenColorPicker(ColorPickerTarget::Arrow);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(l.colorRect, x, y)) {
            OpenColorPicker(ColorPickerTarget::Rect);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(l.colorBadge, x, y)) {
            OpenColorPicker(ColorPickerTarget::Badge);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (PtInRectCust(l.rectSliderLine, x, y)) { draggingSlider = 1; SetCapture(hwnd); }
        else if (PtInRectCust(l.rectSliderArrow, x, y)) { draggingSlider = 2; SetCapture(hwnd); }
        else if (PtInRectCust(l.rectSliderRect, x, y)) { draggingSlider = 3; SetCapture(hwnd); }

        if (draggingSlider != 0) SendMessage(hwnd, WM_MOUSEMOVE, wParam, lParam);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = static_cast<int>(GET_X_LPARAM(lParam) / g_uiScale);

        if (s_pickerOpen) {
            if (s_pickerDraggingSlider != 0 && (wParam & MK_LBUTTON)) {
                float percent = static_cast<float>(x - rectTrackR.left) / static_cast<float>(rectTrackR.right - rectTrackR.left);
                if (percent < 0.0f) percent = 0.0f;
                if (percent > 1.0f) percent = 1.0f;
                int val = static_cast<int>(percent * 255.0f + 0.5f);

                int r = GetRValue(s_pickerColor);
                int g = GetGValue(s_pickerColor);
                int b = GetBValue(s_pickerColor);

                if (s_pickerDraggingSlider == 1) r = val;
                else if (s_pickerDraggingSlider == 2) g = val;
                else if (s_pickerDraggingSlider == 3) b = val;

                s_pickerColor = RGB(r, g, b);
                COLORREF* pTarget = GetTargetColorPtr(s_pickerTarget);
                if (pTarget) *pTarget = s_pickerColor; // Live preview

                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        if (draggingSlider != 0 && (wParam & MK_LBUTTON)) {
            const FormLayout& l = CurrentLayout();
            float percent = static_cast<float>(x - l.rectSliderLine.left) / static_cast<float>(l.rectSliderLine.right - l.rectSliderLine.left);
            if (percent < 0.0f) percent = 0.0f;
            if (percent > 1.0f) percent = 1.0f;
            int val = 1 + static_cast<int>(percent * 19.0f);

            if (draggingSlider == 1) g_config.lineWidth = val;
            else if (draggingSlider == 2) g_config.arrowWidth = val;
            else if (draggingSlider == 3) g_config.rectWidth = val;

            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (s_pickerDraggingSlider != 0) {
            ReleaseCapture();
            s_pickerDraggingSlider = 0;
        }
        if (draggingSlider != 0) {
            ReleaseCapture();
            draggingSlider = 0;
            SaveConfig();
        }
        return 0;

    case WM_CLOSE:
        if (s_pickerOpen) {
            CancelColorPicker();
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 99);
        g_hwndSettings = NULL;
        g_bindingMode = BindingMode::None;
        s_pickerOpen = false;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
