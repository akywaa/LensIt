#include "LensIt.h"

UINT WM_TASKBARCREATED = 0;

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

static bool s_framePending = false;

bool g_keycastText = false;
std::wstring g_keycastTextValue;
ULONGLONG g_keycastUntilTick = 0;
static POINT s_cursorPos = { 0, 0 };

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

void GetBreakTimerCenter(float& cx, float& cy) {
    int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    POINT pt;
    HMONITOR hMon = GetCursorPos(&pt) ? MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST) : NULL;
    MONITORINFO mi = { sizeof(mi) };
    if (!hMon || !GetMonitorInfo(hMon, &mi)) {
        cx = GetSystemMetrics(SM_CXVIRTUALSCREEN) / 2.0f;
        cy = GetSystemMetrics(SM_CYVIRTUALSCREEN) / 2.0f;
        return;
    }
    cx = ((mi.rcMonitor.left + mi.rcMonitor.right) / 2.0f) - (float)vScreenX;
    cy = ((mi.rcMonitor.top + mi.rcMonitor.bottom) / 2.0f) - (float)vScreenY;
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

    float cx = 0.0f;
    float cy = 0.0f;
    GetBreakTimerCenter(cx, cy);

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

    POINT curPt = s_cursorPos;
    {
        POINT pt;
        if (GetCursorPos(&pt)) {
            s_cursorPos = pt;
            curPt = pt;
        }
    }

    {
        Bitmap surface(g_backWidth, g_backHeight, g_backStride, PixelFormat32bppPARGB, (BYTE*)g_dibBits);
        Graphics g(&surface);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

        if (g_spotlightMode) {
            GraphicsPath spotPath;
            spotPath.AddRectangle(Rect(0, 0, g_backWidth, g_backHeight));
            float r = 180.0f;
            float cx = (float)(curPt.x - GetSystemMetrics(SM_XVIRTUALSCREEN));
            float cy = (float)(curPt.y - GetSystemMetrics(SM_YVIRTUALSCREEN));
            spotPath.AddEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
            spotPath.SetFillMode(FillModeAlternate);
            SolidBrush dimBrush(Color(180, 0, 0, 0));
            g.FillPath(&dimBrush, &spotPath);
        }

        if (g_isBreakTimerActive) {
            DrawBreakTimerUI(g, g_backWidth, g_backHeight);
        }
        else {
            int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
            int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);

            if (g_boardMode == BoardMode::White) {
                SolidBrush boardBrush(Color(255, 255, 255, 255));
                g.FillRectangle(&boardBrush, 0, 0, g_backWidth, g_backHeight);
            }
            else if (g_boardMode == BoardMode::Dark) {
                SolidBrush boardBrush(Color(255, 24, 24, 27));
                g.FillRectangle(&boardBrush, 0, 0, g_backWidth, g_backHeight);
            }

            for (const auto& s : g_strokes) DrawStroke(g, s, vScreenX, vScreenY);
            if (!g_currentStroke.points.empty()) DrawStroke(g, g_currentStroke, vScreenX, vScreenY);
            if (g_isTextInputActive && !g_textDraft.points.empty()) {
                Stroke caretDraft = g_textDraft;
                caretDraft.text = g_textDraft.text + (((GetTickCount64() / 500) % 2) ? L" " : L"|");
                DrawStroke(g, caretDraft, vScreenX, vScreenY);
            }

            if (g_cropMode) {
                SolidBrush dimCrop(Color(140, 0, 0, 0));
                if (g_cropDragging) {
                    int l = min(g_cropStart.x, g_cropEnd.x) - vScreenX;
                    int t = min(g_cropStart.y, g_cropEnd.y) - vScreenY;
                    int r = max(g_cropStart.x, g_cropEnd.x) - vScreenX;
                    int b = max(g_cropStart.y, g_cropEnd.y) - vScreenY;
                    g.FillRectangle(&dimCrop, 0, 0, g_backWidth, t);
                    g.FillRectangle(&dimCrop, 0, b, g_backWidth, g_backHeight - b);
                    g.FillRectangle(&dimCrop, 0, t, l, b - t);
                    g.FillRectangle(&dimCrop, r, t, g_backWidth - r, b - t);
                    Pen cropPen(Color(255, 0, 160, 255), 1.5f);
                    cropPen.SetDashStyle(DashStyleDash);
                    g.DrawRectangle(&cropPen, (REAL)l, (REAL)t, (REAL)(r - l), (REAL)(b - t));
                }
                else {
                    g.FillRectangle(&dimCrop, 0, 0, g_backWidth, g_backHeight);
                }
            }
        }

        if (g_keycastText) {
            const int kw = 220, kh = 56;
            int kx = g_backWidth - kw - 28;
            int ky = g_backHeight - kh - 88;

            GraphicsPath cardPath;
            float radius = 12.0f;
            float d = radius * 2.0f;
            cardPath.AddArc((float)kx, (float)ky, d, d, 180.0f, 90.0f);
            cardPath.AddArc((float)(kx + kw - d), (float)ky, d, d, 270.0f, 90.0f);
            cardPath.AddArc((float)(kx + kw - d), (float)(ky + kh - d), d, d, 0.0f, 90.0f);
            cardPath.AddArc((float)kx, (float)(ky + kh - d), d, d, 90.0f, 90.0f);
            cardPath.CloseFigure();
            SolidBrush bgCard(Color(215, 18, 18, 24));
            g.FillPath(&bgCard, &cardPath);
            Pen borderPen(Color(200, 90, 90, 110), 1.0f);
            g.DrawPath(&borderPen, &cardPath);

            StringFormat fmtCenter;
            fmtCenter.SetAlignment(StringAlignmentCenter);
            fmtCenter.SetLineAlignment(StringAlignmentCenter);
            Font fontKey(L"Consolas", 13.0f, FontStyleBold);
            SolidBrush textWhite(Color(255, 240, 240, 250));
            g.DrawString(g_keycastTextValue.c_str(), -1, &fontKey, RectF((REAL)kx, (REAL)ky, (REAL)kw, (REAL)kh), &fmtCenter, &textWhite);
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

    SyncOverlayVisibility();
}

bool UndoLastStroke() {
    if (!g_strokes.empty()) {
        if (g_strokes.back().type == StrokeType::Badge && g_stepCounter > 1) {
            g_stepCounter--;
        }
        g_strokes.pop_back();
        if (g_strokes.empty()) {
            g_persistentDrawingsActive = false;
        }
        RedrawOverlay();
        return true;
    }
    return false;
}

void RedrawOverlay() {
    s_framePending = true;
    if (g_hwndOverlay) SetTimer(g_hwndOverlay, 3, 14, NULL);
}

void PruneVanishingStrokes() {
    ULONGLONG now = GetTickCount64();
    bool changed = false;
    for (size_t i = g_strokes.size(); i > 0; ) {
        --i;
        Stroke& s = g_strokes[i];
        if (s.birthTick == 0) continue;
        float age = (float)(now - s.birthTick);
        if (age >= 1200.0f) {
            g_strokes.erase(g_strokes.begin() + i);
            changed = true;
        }
        else if (age >= 600.0f) {
            float k = 1.0f - (age - 600.0f) / 600.0f;
            if (k < 0.0f) k = 0.0f;
            if (k < s.opacity) {
                s.opacity = k;
                changed = true;
            }
        }
    }
    if (changed) {
        if (g_strokes.empty()) {
            g_persistentDrawingsActive = false;
        }
        s_framePending = true;
    }
}

void ProcessOverlayFrame() {
    PruneVanishingStrokes();

    POINT pt = { 0, 0 };
    bool mouseMoved = false;
    if (GetCursorPos(&pt)) {
        if (pt.x != s_cursorPos.x || pt.y != s_cursorPos.y) {
            s_cursorPos = pt;
            mouseMoved = true;
        }
    }

    static bool s_prevSpotlight = false;
    static bool s_prevKeycast = false;
    if (g_spotlightMode != s_prevSpotlight) {
        s_prevSpotlight = g_spotlightMode;
        s_framePending = true;
    }
    if (g_keycastText != s_prevKeycast) {
        s_prevKeycast = g_keycastText;
        s_framePending = true;
    }

    if (g_spotlightMode && mouseMoved) {
        s_framePending = true;
    }

    bool isDrawingStroke = g_isDrawingLine || g_isDrawingArrow || g_isDrawRectangle ||
                           g_isDrawingHighlight || g_isDrawingBlur || !g_currentStroke.points.empty();
    if (isDrawingStroke) {
        s_framePending = true;
    }

    if (g_isTextInputActive) {
        s_framePending = true;
    }

    if (g_keycastText && GetTickCount64() > g_keycastUntilTick) {
        g_keycastText = false;
        s_framePending = true;
    }

    bool animating = g_isTextInputActive || g_keycastText;
    if (!animating) {
        for (const auto& s : g_strokes) {
            if (s.birthTick != 0) {
                animating = true;
                break;
            }
        }
    }

    if (!s_framePending) {
        if (!animating && g_hwndOverlay) KillTimer(g_hwndOverlay, 3);
        return;
    }
    s_framePending = false;
    if (animating) SetTimer(g_hwndOverlay, 3, 14, NULL);
    PresentOverlayFrame();
}

void SyncOverlayVisibility() {
    if (!g_hwndOverlay) return;

    bool shouldBeVisible = g_isBreakTimerActive ||
                           g_isTriggerHeld ||
                           g_spotlightMode ||
                           g_isTextInputActive ||
                           g_keycastText ||
                           g_cropMode ||
                           (g_boardMode != BoardMode::None) ||
                           g_persistentDrawingsActive ||
                           !g_strokes.empty() ||
                           !g_currentStroke.points.empty() ||
                           (fabsf(g_currentZoom - 1.0f) > 0.002f) ||
                           (fabsf(g_targetZoom - 1.0f) > 0.002f);

    bool isVisible = (IsWindowVisible(g_hwndOverlay) != FALSE);

    if (shouldBeVisible && !isVisible) {
        ShowWindow(g_hwndOverlay, SW_SHOWNOACTIVATE);
    }
    else if (!shouldBeVisible && isVisible) {
        ShowWindow(g_hwndOverlay, SW_HIDE);
    }
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
        SyncOverlayVisibility();
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
        SWP_NOACTIVATE);

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

static void ApplyStrokeAlpha(const Stroke& stroke, Color& col) {
    if (stroke.opacity >= 0.999f) return;
    col = Color((BYTE)(col.GetAlpha() * stroke.opacity), col.GetRed(), col.GetGreen(), col.GetBlue());
}

void DrawStroke(Graphics& g, const Stroke& stroke, int offX, int offY) {
    if (stroke.type == StrokeType::Text) {
        if (stroke.points.empty()) return;
        COLORREF c = stroke.color ? stroke.color : g_config.lineColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, col);
        SolidBrush textBrush(col);
        Font font(L"Segoe UI", 18.0f, FontStyleBold);
        PointF origin((REAL)(stroke.points[0].x - offX), (REAL)(stroke.points[0].y - offY));
        g.DrawString(stroke.text.c_str(), -1, &font, origin, &textBrush);
        return;
    }

    if (stroke.type == StrokeType::Badge) {
        if (stroke.points.empty()) return;
        int x = stroke.points[0].x - offX;
        int y = stroke.points[0].y - offY;
        int radius = 15;

        COLORREF bg = stroke.color ? stroke.color : g_config.badgeColor;
        Color bgCol(255, GetRValue(bg), GetGValue(bg), GetBValue(bg));
        ApplyStrokeAlpha(stroke, bgCol);
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
        ApplyStrokeAlpha(stroke, fillCol);
        SolidBrush fill(fillCol);
        g.FillRectangle(&fill, (REAL)left, (REAL)top, (REAL)(right - left), (REAL)(bottom - top));
        return;
    }

    if (stroke.type == StrokeType::Line) {
        COLORREF c = stroke.color ? stroke.color : g_config.lineColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, col);
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
        ApplyStrokeAlpha(stroke, col);
        Pen pen(col, (REAL)g_config.arrowWidth);
        SolidBrush brush(col);
        DrawArrow(g, pen, brush, stroke.points.front(), stroke.points.back(), g_config.arrowWidth, offX, offY);
    }
    else if (stroke.type == StrokeType::Rectangle) {
        COLORREF c = stroke.color ? stroke.color : g_config.rectColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, col);
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

static void FillBoardBackground(Graphics& g, int x, int y, int w, int h) {
    if (g_boardMode == BoardMode::White) {
        SolidBrush boardBrush(Color(255, 255, 255, 255));
        g.FillRectangle(&boardBrush, x, y, w, h);
    }
    else if (g_boardMode == BoardMode::Dark) {
        SolidBrush boardBrush(Color(255, 24, 24, 27));
        g.FillRectangle(&boardBrush, x, y, w, h);
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
        FillBoardBackground(g, 0, 0, scrW, scrH);
        for (const auto& s : g_strokes) DrawStroke(g, s, vScreenX, vScreenY);
        if (!g_currentStroke.points.empty()) DrawStroke(g, g_currentStroke, vScreenX, vScreenY);
    }

    SelectObject(captureDC, hOldBmp);
    DeleteDC(captureDC);
    ReleaseDC(NULL, screenDC);

    bool opened = false;
    for (int i = 0; i < 5; ++i) {
        if (OpenClipboard(g_hwndOverlay)) {
            opened = true;
            break;
        }
        Sleep(10);
    }

    if (opened) {
        EmptyClipboard();
        if (!SetClipboardData(CF_BITMAP, hBmp)) {
            DeleteObject(hBmp);
        }
        CloseClipboard();
    }
    else {
        DeleteObject(hBmp);
    }
}

void CopyRegionToClipboard(RECT rcScreen) {
    int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);

    int left = max(rcScreen.left, vScreenX);
    int top = max(rcScreen.top, vScreenY);
    int right = min(rcScreen.right, vScreenX + GetSystemMetrics(SM_CXVIRTUALSCREEN));
    int bottom = min(rcScreen.bottom, vScreenY + GetSystemMetrics(SM_CYVIRTUALSCREEN));

    int w = right - left;
    int h = bottom - top;
    if (w < 1 || h < 1) return;

    HDC screenDC = GetDC(NULL);
    if (!screenDC) return;
    HDC captureDC = CreateCompatibleDC(screenDC);
    HBITMAP hBmp = CreateCompatibleBitmap(screenDC, w, h);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(captureDC, hBmp);

    BitBlt(captureDC, 0, 0, w, h, screenDC, left, top, SRCCOPY);

    {
        Graphics g(captureDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        FillBoardBackground(g, 0, 0, w, h);
        for (const auto& s : g_strokes) DrawStroke(g, s, left, top);
        if (!g_currentStroke.points.empty()) DrawStroke(g, g_currentStroke, left, top);
    }

    SelectObject(captureDC, hOldBmp);
    DeleteDC(captureDC);
    ReleaseDC(NULL, screenDC);

    bool opened = false;
    for (int i = 0; i < 5; ++i) {
        if (OpenClipboard(g_hwndOverlay)) {
            opened = true;
            break;
        }
        Sleep(10);
    }

    if (opened) {
        EmptyClipboard();
        if (!SetClipboardData(CF_BITMAP, hBmp)) {
            DeleteObject(hBmp);
        }
        CloseClipboard();
    }
    else {
        DeleteObject(hBmp);
    }
}

void StartCropSelection() {
    if (!g_hwndOverlay) return;
    g_cropMode = true;
    ShowWindow(g_hwndOverlay, SW_SHOWNOACTIVATE);
    SetCursor(LoadCursor(NULL, IDC_CROSS));
    RedrawOverlay();
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

    RepositionToast();
    ShowWindow(g_hwndToast, SW_SHOWNOACTIVATE);
    SetTimer(g_hwndToast, 101, 16, NULL);
    RenderToastFrame();
}

void RepositionToast() {
    if (!g_hwndToast) return;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = sw - TOAST_W - 24;
    int y = sh - TOAST_H - 48;
    SetWindowPos(g_hwndToast, HWND_TOPMOST, x, y, TOAST_W, TOAST_H, SWP_NOACTIVATE);
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
    if (WM_TASKBARCREATED != 0 && msg == WM_TASKBARCREATED) {
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        Shell_NotifyIcon(NIM_ADD, &g_nid);
        return 0;
    }

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

    case WM_APP_SHOWSETTINGS:
        ShowSettingsWindow(GetModuleHandle(NULL));
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
        if (wParam == 3) {
            ProcessOverlayFrame();
            return 0;
        }
        else if (wParam == 1) {
            UpdateCamera();
            ProcessOverlayFrame();
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