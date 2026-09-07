#include "LensIt.h"

static char g_configBuf[MAX_PATH];
static bool g_configPathReady = false;

static HDC g_memDC = NULL;
static HBITMAP g_memBitmap = NULL;
static HBITMAP g_oldBitmap = NULL;
static void* g_dibBits = NULL;
static int g_backWidth = 0;
static int g_backHeight = 0;
static int g_backStride = 0;

static std::wstring s_toastTitle;
static std::wstring s_toastMsg;
static COLORREF s_toastAccent = RGB(0, 150, 255);
static float s_toastAlpha = 0.0f;
static int s_toastHoldFrames = 0;
static const int TOAST_W = 270;
static const int TOAST_H = 68;

// Break timer variables
bool g_isBreakTimerActive = false;
bool g_isBreakTimerPaused = false;
bool g_isBreakTimerEditing = false;
int g_breakTimerTotalSec = 300;
int g_breakTimerRemainingSec = 300;
std::wstring g_breakTimerInputStr;

void DrawStroke(Graphics& g, const Stroke& stroke, int offX, int offY);

static void EnsureConfigPath() {
    if (g_configPathReady) return;
    GetModuleFileNameA(NULL, g_configBuf, MAX_PATH);
    char* p = strrchr(g_configBuf, '\\');
    if (p) *p = '\0';
    strcat_s(g_configBuf, MAX_PATH, "\\config.ini");
    g_configPathReady = true;
}

void LoadConfig() {
    EnsureConfigPath();
    g_config.triggerKey = (DWORD)GetPrivateProfileIntA("Config", "TriggerKey", (int)g_config.triggerKey, g_configBuf);
    g_config.rectKey = (DWORD)GetPrivateProfileIntA("Config", "RectKey", (int)g_config.rectKey, g_configBuf);
    g_config.lineColor = (COLORREF)GetPrivateProfileIntA("Config", "LineColor", (int)g_config.lineColor, g_configBuf);
    g_config.lineWidth = (int)GetPrivateProfileIntA("Config", "LineWidth", g_config.lineWidth, g_configBuf);
    g_config.arrowColor = (COLORREF)GetPrivateProfileIntA("Config", "ArrowColor", (int)g_config.arrowColor, g_configBuf);
    g_config.arrowWidth = (int)GetPrivateProfileIntA("Config", "ArrowWidth", g_config.arrowWidth, g_configBuf);
    g_config.rectColor = (COLORREF)GetPrivateProfileIntA("Config", "RectColor", (int)g_config.rectColor, g_configBuf);
    g_config.rectWidth = (int)GetPrivateProfileIntA("Config", "RectWidth", g_config.rectWidth, g_configBuf);
    g_config.badgeColor = (COLORREF)GetPrivateProfileIntA("Config", "BadgeColor", (int)g_config.badgeColor, g_configBuf);
    g_config.resetZoomOnRelease = GetPrivateProfileIntA("Config", "ResetZoomOnRelease", g_config.resetZoomOnRelease ? 1 : 0, g_configBuf) != 0;
    g_config.keepDrawingsOnRelease = GetPrivateProfileIntA("Config", "KeepDrawingsOnRelease", g_config.keepDrawingsOnRelease ? 1 : 0, g_configBuf) != 0;
    g_config.hideToastsFromCapture = GetPrivateProfileIntA("Config", "HideToastsFromCapture", g_config.hideToastsFromCapture ? 1 : 0, g_configBuf) != 0;
    g_config.isFirstRun = GetPrivateProfileIntA("Config", "FirstRun", 1, g_configBuf) != 0;
}

void SaveConfig() {
    EnsureConfigPath();
    char buf[64];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_config.triggerKey);
    WritePrivateProfileStringA("Config", "TriggerKey", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_config.rectKey);
    WritePrivateProfileStringA("Config", "RectKey", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_config.lineColor);
    WritePrivateProfileStringA("Config", "LineColor", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%d", g_config.lineWidth);
    WritePrivateProfileStringA("Config", "LineWidth", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_config.arrowColor);
    WritePrivateProfileStringA("Config", "ArrowColor", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%d", g_config.arrowWidth);
    WritePrivateProfileStringA("Config", "ArrowWidth", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_config.rectColor);
    WritePrivateProfileStringA("Config", "RectColor", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%d", g_config.rectWidth);
    WritePrivateProfileStringA("Config", "RectWidth", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_config.badgeColor);
    WritePrivateProfileStringA("Config", "BadgeColor", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%d", g_config.resetZoomOnRelease ? 1 : 0);
    WritePrivateProfileStringA("Config", "ResetZoomOnRelease", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%d", g_config.keepDrawingsOnRelease ? 1 : 0);
    WritePrivateProfileStringA("Config", "KeepDrawingsOnRelease", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%d", g_config.hideToastsFromCapture ? 1 : 0);
    WritePrivateProfileStringA("Config", "HideToastsFromCapture", buf, g_configBuf);
    snprintf(buf, sizeof(buf), "%d", g_config.isFirstRun ? 1 : 0);
    WritePrivateProfileStringA("Config", "FirstRun", buf, g_configBuf);

    WritePrivateProfileStringA(NULL, NULL, NULL, g_configBuf);
}

static bool ParseTimerString(const std::wstring& s, int& outSec) {
    if (s.empty()) return false;
    size_t colon = s.find_first_of(L":., ");
    if (colon != std::wstring::npos) {
        std::wstring mStr = s.substr(0, colon);
        std::wstring sStr = s.substr(colon + 1);
        int m = mStr.empty() ? 0 : _wtoi(mStr.c_str());
        int sec = sStr.empty() ? 0 : _wtoi(sStr.c_str());
        outSec = m * 60 + sec;
        return outSec > 0;
    }
    int val = _wtoi(s.c_str());
    if (val <= 0) return false;
    if (s.length() == 3 || s.length() == 4) {
        int sec = val % 100;
        int m = val / 100;
        if (sec < 60) {
            outSec = m * 60 + sec;
            return outSec > 0;
        }
    }
    outSec = val * 60;
    return outSec > 0;
}

void StartBreakTimer(int minutes) {
    if (minutes < 1) minutes = 1;
    g_breakTimerTotalSec = minutes * 60;
    g_breakTimerRemainingSec = g_breakTimerTotalSec;
    g_isBreakTimerActive = true;
    g_isBreakTimerPaused = false;
    g_isBreakTimerEditing = false;
    g_breakTimerInputStr.clear();

    if (g_hwndOverlay) {
        SetTimer(g_hwndOverlay, 2, 1000, NULL);
    }
    RedrawOverlay();
    ShowNotification(L"Break Timer", L"Started (" + std::to_wstring(minutes) + L" min)", RGB(0, 150, 255));
}

void StopBreakTimer() {
    if (!g_isBreakTimerActive) return;
    g_isBreakTimerActive = false;
    g_isBreakTimerPaused = false;
    g_isBreakTimerEditing = false;
    g_breakTimerInputStr.clear();
    if (g_hwndOverlay) {
        KillTimer(g_hwndOverlay, 2);
    }
    RedrawOverlay();
}

void ToggleBreakTimer(int minutes) {
    if (g_isBreakTimerActive) {
        StopBreakTimer();
        ShowNotification(L"Break Timer", L"Dismissed", RGB(220, 70, 70));
    }
    else {
        StartBreakTimer(minutes);
    }
}

void CommitBreakTimerInput() {
    if (!g_isBreakTimerEditing) return;
    int newSec = 0;
    if (ParseTimerString(g_breakTimerInputStr, newSec)) {
        if (newSec > 5999) newSec = 5999;
        g_breakTimerRemainingSec = newSec;
        g_breakTimerTotalSec = newSec;

        int m = newSec / 60;
        int s = newSec % 60;
        wchar_t buf[32];
        swprintf_s(buf, L"Set to %02d:%02d", m, s);
        ShowNotification(L"Timer Updated", buf, RGB(0, 150, 255));
    }
    g_isBreakTimerEditing = false;
    g_isBreakTimerPaused = false;
    g_breakTimerInputStr.clear();
    RedrawOverlay();
}

static void DrawBreakTimerUI(Graphics& g, int w, int h) {
    SolidBrush dimBg(Color(220, 10, 10, 14));
    g.FillRectangle(&dimBg, 0, 0, w, h);

    Font fontClock(L"Segoe UI", 84.0f, FontStyleBold);
    Font fontSub(L"Segoe UI", 13.0f, FontStyleBold);
    Font fontHint(L"Segoe UI", 10.5f, FontStyleRegular);

    StringFormat fmtCenter;
    fmtCenter.SetAlignment(StringAlignmentCenter);
    fmtCenter.SetLineAlignment(StringAlignmentCenter);

    float cx = (float)w / 2.0f;
    float cy = (float)h / 2.0f;

    float totalW = 340.0f;
    float barH = 6.0f;
    float progress = (g_breakTimerTotalSec > 0) ? ((float)g_breakTimerRemainingSec / (float)g_breakTimerTotalSec) : 0.0f;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    float barX = cx - totalW / 2.0f;
    float barY = cy + 65.0f;

    SolidBrush trackBg(Color(255, 45, 45, 52));
    g.FillRectangle(&trackBg, barX, barY, totalW, barH);

    Color accentCol = (g_breakTimerRemainingSec <= 30 && !g_isBreakTimerEditing) ? Color(255, 235, 60, 60) : Color(255, 0, 140, 255);
    SolidBrush fillBrush(accentCol);
    g.FillRectangle(&fillBrush, barX, barY, totalW * progress, barH);

    RectF clockRect(cx - 300.0f, cy - 90.0f, 600.0f, 130.0f);

    if (g_isBreakTimerEditing) {
        // Interactive edit box frame
        Pen editBorder(Color(255, 0, 160, 255), 2.0f);
        SolidBrush editBg(Color(120, 20, 30, 45));
        g.FillRectangle(&editBg, cx - 220.0f, cy - 85.0f, 440.0f, 125.0f);
        g.DrawRectangle(&editBorder, cx - 220.0f, cy - 85.0f, 440.0f, 125.0f);

        std::wstring disp = g_breakTimerInputStr.empty() ? L"__ : __" : (g_breakTimerInputStr + L"|");
        SolidBrush textEdit(Color(255, 255, 255, 255));
        g.DrawString(disp.c_str(), -1, &fontClock, clockRect, &fmtCenter, &textEdit);

        SolidBrush textAccent(Color(255, 0, 160, 255));
        RectF titleRect(cx - 300.0f, cy - 125.0f, 600.0f, 30.0f);
        g.DrawString(L"SET CUSTOM TIME (TYPE AND PRESS ENTER)", -1, &fontSub, titleRect, &fmtCenter, &textAccent);

        SolidBrush textHint(Color(255, 180, 180, 185));
        RectF hintRect(cx - 300.0f, cy + 90.0f, 600.0f, 25.0f);
        g.DrawString(L"Enter: confirm  *  Esc: cancel  *  Format: 3:50 or 5", -1, &fontHint, hintRect, &fmtCenter, &textHint);
    }
    else {
        int mins = g_breakTimerRemainingSec / 60;
        int secs = g_breakTimerRemainingSec % 60;
        wchar_t timeBuf[32];
        swprintf_s(timeBuf, L"%02d:%02d", mins, secs);

        SolidBrush textWhite(g_breakTimerRemainingSec <= 30 ? Color(255, 255, 100, 100) : Color(255, 245, 245, 250));
        g.DrawString(timeBuf, -1, &fontClock, clockRect, &fmtCenter, &textWhite);

        SolidBrush textAccent(accentCol);
        std::wstring titleStr = g_isBreakTimerPaused ? L"BREAK TIMER  *  [PAUSED]" : L"BREAK IN PROGRESS";
        RectF titleRect(cx - 300.0f, cy - 125.0f, 600.0f, 30.0f);
        g.DrawString(titleStr.c_str(), -1, &fontSub, titleRect, &fmtCenter, &textAccent);

        SolidBrush textHint(Color(255, 160, 160, 165));
        std::wstring hintStr = L"Wheel: min (+Shift: sec)  *  Click time: edit  *  Space: pause  *  Esc: exit";
        RectF hintRect(cx - 300.0f, cy + 90.0f, 600.0f, 25.0f);
        g.DrawString(hintStr.c_str(), -1, &fontHint, hintRect, &fmtCenter, &textHint);
    }
}

void DestroyOverlayBackbuffer() {
    if (g_memDC) {
        if (g_oldBitmap) SelectObject(g_memDC, g_oldBitmap);
        DeleteDC(g_memDC);
        g_memDC = NULL;
    }
    if (g_memBitmap) {
        DeleteObject(g_memBitmap);
        g_memBitmap = NULL;
    }
    g_oldBitmap = NULL;
    g_dibBits = NULL;
    g_backStride = 0;
    g_backWidth = g_backHeight = 0;
}

void CreateOverlayBackbuffer() {
    DestroyOverlayBackbuffer();
    g_backWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_backHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    g_backStride = g_backWidth * 4;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_backWidth;
    bmi.bmiHeader.biHeight = -g_backHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(NULL);
    if (!screenDC) return;

    g_memDC = CreateCompatibleDC(screenDC);
    if (g_memDC) {
        g_memBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &g_dibBits, NULL, 0);
        if (g_memBitmap) {
            g_oldBitmap = (HBITMAP)SelectObject(g_memDC, g_memBitmap);
        }
        else {
            DeleteDC(g_memDC);
            g_memDC = NULL;
            g_dibBits = NULL;
        }
    }
    ReleaseDC(NULL, screenDC);
}

void PresentOverlayFrame() {
    if (!g_hwndOverlay) return;
    if (!g_memDC || !g_dibBits || g_backWidth <= 0 || g_backHeight <= 0) {
        CreateOverlayBackbuffer();
        if (!g_memDC || !g_dibBits) return;
    }

    memset(g_dibBits, 0, (size_t)g_backStride * (size_t)g_backHeight);

    {
        Bitmap surface(g_backWidth, g_backHeight, g_backStride, PixelFormat32bppPARGB, (BYTE*)g_dibBits);
        Graphics g(&surface);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

        if (g_isBreakTimerActive) {
            DrawBreakTimerUI(g, g_backWidth, g_backHeight);
        }
        else {
            int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
            int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);

            for (const auto& s : g_strokes) DrawStroke(g, s, vScreenX, vScreenY);
            if (!g_currentStroke.points.empty()) DrawStroke(g, g_currentStroke, vScreenX, vScreenY);
        }
    }

    HDC screenDC = GetDC(NULL);
    if (screenDC) {
        SIZE size = { g_backWidth, g_backHeight };
        POINT ptSrc = { 0, 0 };
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(g_hwndOverlay, screenDC, NULL, &size, g_memDC, &ptSrc, 0, &blend, ULW_ALPHA);
        ReleaseDC(NULL, screenDC);
    }
}

void UndoLastStroke() {
    if (!g_strokes.empty()) {
        if (g_strokes.back().type == StrokeType::Badge && g_stepCounter > 1) {
            g_stepCounter--;
        }
        g_strokes.pop_back();
        if (g_strokes.empty()) {
            g_persistentDrawingsActive = false;
        }
        RedrawOverlay();
    }
}

void RedrawOverlay() {
    PresentOverlayFrame();
}

void UpdateCamera() {
    if (fabs(g_currentZoom - g_targetZoom) > 0.002f) {
        g_currentZoom += (g_targetZoom - g_currentZoom) * 0.25f;
    }
    else {
        g_currentZoom = g_targetZoom;
    }

    if (g_currentZoom <= 1.001f) {
        MagSetFullscreenTransform(1.0f, 0, 0);
        g_camX = g_camY = 0;
        return;
    }

    POINT pt;
    GetCursorPos(&pt);

    int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);

    float relX = (float)(pt.x - vScreenX);
    float relY = (float)(pt.y - vScreenY);

    float targetX = (float)vScreenX + relX - (relX / g_currentZoom);
    float targetY = (float)vScreenY + relY - (relY / g_currentZoom);

    g_camX += (targetX - g_camX) * 0.35f;
    g_camY += (targetY - g_camY) * 0.35f;
    MagSetFullscreenTransform(g_currentZoom, (int)g_camX, (int)g_camY);
}

void RepositionOverlay() {
    if (!g_hwndOverlay) return;
    int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    SetWindowPos(g_hwndOverlay, HWND_TOPMOST,
        vScreenX, vScreenY, screenW, screenH,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    CreateOverlayBackbuffer();
    PresentOverlayFrame();
}

void DrawArrow(Graphics& g, Pen& pen, SolidBrush& brush, POINT p1, POINT p2, int width, int offX, int offY) {
    float x1 = (float)(p1.x - offX);
    float y1 = (float)(p1.y - offY);
    float x2 = (float)(p2.x - offX);
    float y2 = (float)(p2.y - offY);

    float dx = x2 - x1;
    float dy = y2 - y1;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 4.0f) return;

    float ux = dx / dist;
    float uy = dy / dist;
    float nx = -uy;
    float ny = ux;

    float arrowLen = (float)width * 3.2f + 10.0f;
    if (arrowLen > dist * 0.85f) arrowLen = dist * 0.85f;
    float arrowHalfWidth = arrowLen * 0.55f;

    float overlap = 2.0f;
    float lineEndX = x2 - ux * (arrowLen - overlap);
    float lineEndY = y2 - uy * (arrowLen - overlap);

    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapFlat);
    g.DrawLine(&pen, x1, y1, lineEndX, lineEndY);

    PointF pts[3];
    pts[0] = PointF(x2, y2);
    pts[1] = PointF(x2 - ux * arrowLen + nx * arrowHalfWidth, y2 - uy * arrowLen + ny * arrowHalfWidth);
    pts[2] = PointF(x2 - ux * arrowLen - nx * arrowHalfWidth, y2 - uy * arrowLen - ny * arrowHalfWidth);

    g.FillPolygon(&brush, pts, 3);
}

std::shared_ptr<Bitmap> BakeBlurredBitmap(RECT rc) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 8 || h < 8) return nullptr;

    HDC screenDC = GetDC(NULL);
    if (!screenDC) return nullptr;
    HDC capDC = CreateCompatibleDC(screenDC);
    HBITMAP capBmp = CreateCompatibleBitmap(screenDC, w, h);
    HBITMAP oldBmp = (HBITMAP)SelectObject(capDC, capBmp);
    BitBlt(capDC, 0, 0, w, h, screenDC, rc.left, rc.top, SRCCOPY);
    SelectObject(capDC, oldBmp);
    DeleteDC(capDC);
    ReleaseDC(NULL, screenDC);

    auto resultBmp = std::make_shared<Bitmap>(w, h, PixelFormat32bppPARGB);
    {
        Bitmap src(capBmp, NULL);
        int sw = max(2, w / 14);
        int sh = max(2, h / 14);

        Bitmap smallBmp(sw, sh, PixelFormat32bppPARGB);
        {
            Graphics gSmall(&smallBmp);
            gSmall.SetInterpolationMode(InterpolationModeBilinear);
            gSmall.DrawImage(&src, 0, 0, sw, sh);
        }

        Graphics gDest(resultBmp.get());
        gDest.SetInterpolationMode(InterpolationModeBilinear);
        gDest.DrawImage(&smallBmp, 0, 0, w, h);

        SolidBrush blackTint(Color(185, 12, 12, 14));
        gDest.FillRectangle(&blackTint, 0, 0, w, h);

        Pen framePen(Color(190, 45, 45, 50), 1.0f);
        gDest.DrawRectangle(&framePen, 0, 0, w - 1, h - 1);
    }
    DeleteObject(capBmp);
    return resultBmp;
}

void DrawStroke(Graphics& g, const Stroke& stroke, int offX, int offY) {
    if (stroke.type == StrokeType::Badge) {
        if (stroke.points.empty()) return;
        int x = stroke.points[0].x - offX;
        int y = stroke.points[0].y - offY;
        int radius = 15;

        COLORREF bg = stroke.color ? stroke.color : g_config.badgeColor;
        Color bgCol(255, GetRValue(bg), GetGValue(bg), GetBValue(bg));
        SolidBrush bgBrush(bgCol);
        Pen borderPen(Color(255, 30, 30, 30), 2.0f);
        g.FillEllipse(&bgBrush, x - radius, y - radius, radius * 2, radius * 2);
        g.DrawEllipse(&borderPen, x - radius, y - radius, radius * 2, radius * 2);

        std::wstring numStr = std::to_wstring(stroke.badgeNumber);
        Font font(L"Segoe UI", 10.0f, FontStyleBold);
        SolidBrush textBrush(Color(255, 20, 20, 20));
        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentCenter);
        fmt.SetLineAlignment(StringAlignmentCenter);

        RectF rect((REAL)(x - radius), (REAL)(y - radius + 1), (REAL)(radius * 2), (REAL)(radius * 2));
        g.DrawString(numStr.c_str(), -1, &font, rect, &fmt, &textBrush);
        return;
    }

    if (stroke.points.size() < 2) return;

    if (stroke.type == StrokeType::Blur) {
        if (stroke.cachedBitmap) {
            g.DrawImage(stroke.cachedBitmap.get(),
                (REAL)(stroke.cachedRect.left - offX),
                (REAL)(stroke.cachedRect.top - offY));
        }
        else {
            POINT a = stroke.points.front();
            POINT b = stroke.points.back();
            int left = min(a.x, b.x) - offX;
            int right = max(a.x, b.x) - offX;
            int top = min(a.y, b.y) - offY;
            int bottom = max(a.y, b.y) - offY;

            SolidBrush previewBrush(Color(140, 16, 16, 18));
            g.FillRectangle(&previewBrush, (REAL)left, (REAL)top, (REAL)(right - left), (REAL)(bottom - top));

            Pen previewPen(Color(200, 200, 200, 200), 1.0f);
            previewPen.SetDashStyle(DashStyleDash);
            g.DrawRectangle(&previewPen, (REAL)left, (REAL)top, (REAL)(right - left), (REAL)(bottom - top));
        }
        return;
    }

    if (stroke.type == StrokeType::Highlight) {
        COLORREF c = stroke.color ? stroke.color : RGB(250, 205, 40);
        POINT a = stroke.points.front();
        POINT b = stroke.points.back();
        int left = min(a.x, b.x) - offX;
        int right = max(a.x, b.x) - offX;
        int top = min(a.y, b.y) - offY;
        int bottom = max(a.y, b.y) - offY;
        Color fillCol(110, GetRValue(c), GetGValue(c), GetBValue(c));
        SolidBrush fill(fillCol);
        g.FillRectangle(&fill, (REAL)left, (REAL)top, (REAL)(right - left), (REAL)(bottom - top));
        return;
    }

    if (stroke.type == StrokeType::Line) {
        COLORREF c = stroke.color ? stroke.color : g_config.lineColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        Pen pen(col, (REAL)g_config.lineWidth);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        pen.SetLineJoin(LineJoinRound);

        std::vector<Point> pts;
        pts.reserve(stroke.points.size());
        for (const auto& p : stroke.points) {
            pts.push_back(Point(p.x - offX, p.y - offY));
        }
        g.DrawLines(&pen, pts.data(), (INT)pts.size());
    }
    else if (stroke.type == StrokeType::Arrow) {
        COLORREF c = stroke.color ? stroke.color : g_config.arrowColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        Pen pen(col, (REAL)g_config.arrowWidth);
        SolidBrush brush(col);
        DrawArrow(g, pen, brush, stroke.points.front(), stroke.points.back(), g_config.arrowWidth, offX, offY);
    }
    else if (stroke.type == StrokeType::Rectangle) {
        COLORREF c = stroke.color ? stroke.color : g_config.rectColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        Pen pen(col, (REAL)g_config.rectWidth);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        pen.SetLineJoin(LineJoinMiter);

        POINT a = stroke.points.front();
        POINT b = stroke.points.back();
        int left = min(a.x, b.x) - offX;
        int right = max(a.x, b.x) - offX;
        int top = min(a.y, b.y) - offY;
        int bottom = max(a.y, b.y) - offY;
        g.DrawRectangle(&pen, (REAL)left, (REAL)top, (REAL)(right - left), (REAL)(bottom - top));
    }
}

void CopyScreenshotToClipboard() {
    int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int scrW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int scrH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC screenDC = GetDC(NULL);
    if (!screenDC) return;
    HDC captureDC = CreateCompatibleDC(screenDC);
    HBITMAP hBmp = CreateCompatibleBitmap(screenDC, scrW, scrH);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(captureDC, hBmp);

    BitBlt(captureDC, 0, 0, scrW, scrH, screenDC, vScreenX, vScreenY, SRCCOPY);

    {
        Graphics g(captureDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        for (const auto& s : g_strokes) DrawStroke(g, s, vScreenX, vScreenY);
        if (!g_currentStroke.points.empty()) DrawStroke(g, g_currentStroke, vScreenX, vScreenY);
    }

    SelectObject(captureDC, hOldBmp);
    DeleteDC(captureDC);
    ReleaseDC(NULL, screenDC);

    if (OpenClipboard(g_hwndOverlay)) {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hBmp);
        CloseClipboard();
    }
    else {
        DeleteObject(hBmp);
    }
}

void ApplyToastCaptureAffinity() {
    if (!g_hwndToast) return;
    DWORD affinity = g_config.hideToastsFromCapture ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
    SetWindowDisplayAffinity(g_hwndToast, affinity);
}

static void RenderToastFrame() {
    if (!g_hwndToast) return;

    int w = TOAST_W;
    int h = TOAST_H;
    int stride = w * 4;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screenDC = GetDC(NULL);
    if (!screenDC) return;

    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP hBmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    HBITMAP hOld = (HBITMAP)SelectObject(memDC, hBmp);

    memset(bits, 0, (size_t)stride * (size_t)h);

    {
        Bitmap surface(w, h, stride, PixelFormat32bppPARGB, (BYTE*)bits);
        Graphics g(&surface);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

        GraphicsPath cardPath;
        float radius = 10.0f;
        float d = radius * 2.0f;
        cardPath.AddArc(0.0f, 0.0f, d, d, 180.0f, 90.0f);
        cardPath.AddArc((float)w - d - 1.0f, 0.0f, d, d, 270.0f, 90.0f);
        cardPath.AddArc((float)w - d - 1.0f, (float)h - d - 1.0f, d, d, 0.0f, 90.0f);
        cardPath.AddArc(0.0f, (float)h - d - 1.0f, d, d, 90.0f, 90.0f);
        cardPath.CloseFigure();

        SolidBrush bgCard(Color(235, 18, 18, 22));
        g.FillPath(&bgCard, &cardPath);

        Pen borderPen(Color(180, 55, 55, 62), 1.0f);
        g.DrawPath(&borderPen, &cardPath);

        GraphicsPath barPath;
        barPath.AddArc(6.0f, 10.0f, 6.0f, 6.0f, 180.0f, 180.0f);
        barPath.AddArc(6.0f, (float)h - 16.0f, 6.0f, 6.0f, 0.0f, 180.0f);
        barPath.CloseFigure();
        Color barCol(255, GetRValue(s_toastAccent), GetGValue(s_toastAccent), GetBValue(s_toastAccent));
        SolidBrush barBrush(barCol);
        g.FillPath(&barBrush, &barPath);

        Font fontTitle(L"Segoe UI", 9.5f, FontStyleBold);
        Font fontMsg(L"Segoe UI", 8.5f);
        SolidBrush textWhite(Color(255, 240, 240, 240));
        SolidBrush textDim(Color(255, 170, 170, 175));

        g.DrawString(s_toastTitle.c_str(), -1, &fontTitle, PointF(22.0f, 12.0f), &textWhite);
        g.DrawString(s_toastMsg.c_str(), -1, &fontMsg, PointF(22.0f, 34.0f), &textDim);
    }

    BYTE alphaByte = (BYTE)(s_toastAlpha * 255.0f);
    POINT ptSrc = { 0, 0 };
    SIZE sz = { w, h };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, alphaByte, AC_SRC_ALPHA };
    UpdateLayeredWindow(g_hwndToast, screenDC, NULL, &sz, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, hOld);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(NULL, screenDC);
}

void InitToastWindow(HINSTANCE hInstance) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = sw - TOAST_W - 24;
    int y = sh - TOAST_H - 48;

    g_hwndToast = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"LensItToast", L"Notification", WS_POPUP,
        x, y, TOAST_W, TOAST_H, NULL, NULL, hInstance, NULL
    );

    ApplyToastCaptureAffinity();
}

void ShowNotification(const std::wstring& title, const std::wstring& message, COLORREF accentColor) {
    if (!g_hwndToast) return;
    s_toastTitle = title;
    s_toastMsg = message;
    s_toastAccent = accentColor;
    s_toastHoldFrames = 90;
    s_toastAlpha = 0.05f;

    ShowWindow(g_hwndToast, SW_SHOWNOACTIVATE);
    SetTimer(g_hwndToast, 101, 16, NULL);
    RenderToastFrame();
}

LRESULT CALLBACK ToastWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER: {
        if (wParam == 101) {
            if (s_toastHoldFrames > 0) {
                if (s_toastAlpha < 1.0f) {
                    s_toastAlpha += 0.15f;
                    if (s_toastAlpha > 1.0f) s_toastAlpha = 1.0f;
                }
                else {
                    s_toastHoldFrames--;
                }
            }
            else {
                s_toastAlpha -= 0.08f;
                if (s_toastAlpha <= 0.0f) {
                    s_toastAlpha = 0.0f;
                    KillTimer(hwnd, 101);
                    ShowWindow(hwnd, SW_HIDE);
                }
            }
            RenderToastFrame();
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE:
        MagSetFullscreenTransform(1.0f, 0, 0);
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_APP_TRAYMSG:
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_OWNERDRAW, ID_TRAY_SETTINGS, (LPCTSTR)ID_TRAY_SETTINGS);
            AppendMenu(hMenu, MF_OWNERDRAW, ID_TRAY_EXIT, (LPCTSTR)ID_TRAY_EXIT);
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
            if (cmd == ID_TRAY_SETTINGS) ShowSettingsWindow(GetModuleHandle(NULL));
            else if (cmd == ID_TRAY_EXIT) PostQuitMessage(0);
        }
        return 0;

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
        mis->itemWidth = 140; mis->itemHeight = 32;
        return TRUE;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        Graphics g(dis->hDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        bool isHover = (dis->itemState & ODS_SELECTED);
        SolidBrush bgBrush(isHover ? Color(255, 60, 60, 60) : Color(255, 30, 30, 30));
        g.FillRectangle(&bgBrush, (int)dis->rcItem.left, (int)dis->rcItem.top, 160, 32);

        SolidBrush textBrush(Color(255, 220, 220, 220));
        Font font(L"Segoe UI", 11);
        StringFormat format; format.SetAlignment(StringAlignmentNear); format.SetLineAlignment(StringAlignmentCenter);
        RectF textRect((REAL)dis->rcItem.left + 36, (REAL)dis->rcItem.top, 120.0f, 32.0f);

        Pen iconPen(Color(220, 220, 220), 2.0f);
        if (dis->itemID == ID_TRAY_SETTINGS) {
            g.DrawString(L"Settings", -1, &font, textRect, &format, &textBrush);
            g.DrawEllipse(&iconPen, (int)dis->rcItem.left + 12, (int)dis->rcItem.top + 8, 14, 14);
            g.DrawEllipse(&iconPen, (int)dis->rcItem.left + 15, (int)dis->rcItem.top + 11, 8, 8);
        }
        else if (dis->itemID == ID_TRAY_EXIT) {
            g.DrawString(L"Exit", -1, &font, textRect, &format, &textBrush);
            g.DrawLine(&iconPen, (int)dis->rcItem.left + 13, (int)dis->rcItem.top + 10, (int)dis->rcItem.left + 25, (int)dis->rcItem.top + 22);
            g.DrawLine(&iconPen, (int)dis->rcItem.left + 25, (int)dis->rcItem.top + 10, (int)dis->rcItem.left + 13, (int)dis->rcItem.top + 22);
        }
        return TRUE;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER:
        if (wParam == 1) {
            UpdateCamera();
            if (!RequiresZoomTimer()) StopZoomTimer();
            return 0;
        }
        else if (wParam == 2) {
            if (g_isBreakTimerActive && !g_isBreakTimerPaused && !g_isBreakTimerEditing) {
                if (g_breakTimerRemainingSec > 0) {
                    g_breakTimerRemainingSec--;
                    RedrawOverlay();
                    if (g_breakTimerRemainingSec == 0) {
                        MessageBeep(MB_ICONASTERISK);
                        ShowNotification(L"Break Ended", L"Time is up!", RGB(46, 204, 113));
                    }
                }
            }
            return 0;
        }
        break;

    case WM_DISPLAYCHANGE:
        g_targetZoom = 1.0f;
        g_currentZoom = 1.0f;
        ResetDrawingState();
        RepositionOverlay();
        UpdateCamera();
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}