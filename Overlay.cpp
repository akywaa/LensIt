#include "LensIt.h"
#include "WinHandles.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <string_view>
#include <shlobj.h>
#include <knownfolders.h>

UINT WM_TASKBARCREATED = 0;

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

static const ULONGLONG KEYCAST_FADE_IN_MS = 180;
static const ULONGLONG KEYCAST_FADE_OUT_MS = 240;
static const int KEYCAST_TOAST_GAP = 12;

static float s_keycastAlpha = 0.0f;
static ULONGLONG s_keycastShownTick = 0;

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
static float s_breakTimerCenterX = 0.0f;
static float s_breakTimerCenterY = 0.0f;

void DrawStroke(Graphics& g, const Stroke& stroke, int offX, int offY);
std::shared_ptr<Bitmap> BakeBlurredBitmap(RECT rc);
static void ComputeBreakTimerCenter(float& cx, float& cy);
static POINT ToastAnchor();

namespace {

constexpr const char* kConfigFileName = "config.ini";

std::filesystem::path WritableConfigPath(const std::filesystem::path& portablePath) {
    std::ofstream probe(portablePath, std::ios::app);
    if (probe.is_open()) return portablePath;

    PWSTR appData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        return portablePath;
    }

    std::filesystem::path directory = std::filesystem::path(appData) / L"LensIt";
    CoTaskMemFree(appData);

    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    return directory / kConfigFileName;
}

std::filesystem::path ConfigPath() {
    wchar_t buffer[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return WritableConfigPath(std::filesystem::path(buffer).parent_path() / kConfigFileName);
}

std::map<std::string, long long> ReadConfigValues() {
    std::map<std::string, long long> values;
    std::ifstream file(ConfigPath());
    std::string line;
    while (std::getline(file, line)) {
        std::string_view view(line);
        const size_t firstNonSpace = view.find_first_not_of(" \t\r");
        if (firstNonSpace == std::string_view::npos) continue;
        view.remove_prefix(firstNonSpace);
        if (view.front() == '#' || view.front() == ';' || view.front() == '[') continue;

        const size_t separator = view.find('=');
        if (separator == std::string_view::npos) continue;

        std::string_view key = view.substr(0, separator);
        std::string_view value = view.substr(separator + 1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.remove_suffix(1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);

        try {
            values[std::string(key)] = std::stoll(std::string(value));
        }
        catch (const std::exception&) {
        }
    }
    return values;
}

long long ConfigValue(const std::map<std::string, long long>& values, const char* key, long long fallback) {
    const auto it = values.find(key);
    return (it == values.end()) ? fallback : it->second;
}

}

void LoadConfig() {
    const std::map<std::string, long long> values = ReadConfigValues();
    g_config.triggerKey = static_cast<DWORD>(ConfigValue(values, "TriggerKey", static_cast<int>(g_config.triggerKey)));
    g_config.rectKey = static_cast<DWORD>(ConfigValue(values, "RectKey", static_cast<int>(g_config.rectKey)));
    g_config.lineColor = static_cast<COLORREF>(ConfigValue(values, "LineColor", static_cast<int>(g_config.lineColor)));
    g_config.lineWidth = static_cast<int>(ConfigValue(values, "LineWidth", g_config.lineWidth));
    g_config.arrowColor = static_cast<COLORREF>(ConfigValue(values, "ArrowColor", static_cast<int>(g_config.arrowColor)));
    g_config.arrowWidth = static_cast<int>(ConfigValue(values, "ArrowWidth", g_config.arrowWidth));
    g_config.rectColor = static_cast<COLORREF>(ConfigValue(values, "RectColor", static_cast<int>(g_config.rectColor)));
    g_config.rectWidth = static_cast<int>(ConfigValue(values, "RectWidth", g_config.rectWidth));
    g_config.badgeColor = static_cast<COLORREF>(ConfigValue(values, "BadgeColor", static_cast<int>(g_config.badgeColor)));
    g_config.resetZoomOnRelease = ConfigValue(values, "ResetZoomOnRelease", g_config.resetZoomOnRelease ? 1 : 0) != 0;
    g_config.keepDrawingsOnRelease = ConfigValue(values, "KeepDrawingsOnRelease", g_config.keepDrawingsOnRelease ? 1 : 0) != 0;
    g_config.hideToastsFromCapture = ConfigValue(values, "HideToastsFromCapture", g_config.hideToastsFromCapture ? 1 : 0) != 0;
    g_config.isFirstRun = ConfigValue(values, "FirstRun", 1) != 0;
}

void SaveConfig() {
    std::ofstream file(ConfigPath(), std::ios::trunc);
    if (!file) return;

    file << "TriggerKey=" << g_config.triggerKey << '\n';
    file << "RectKey=" << g_config.rectKey << '\n';
    file << "LineColor=" << g_config.lineColor << '\n';
    file << "LineWidth=" << g_config.lineWidth << '\n';
    file << "ArrowColor=" << g_config.arrowColor << '\n';
    file << "ArrowWidth=" << g_config.arrowWidth << '\n';
    file << "RectColor=" << g_config.rectColor << '\n';
    file << "RectWidth=" << g_config.rectWidth << '\n';
    file << "BadgeColor=" << g_config.badgeColor << '\n';
    file << "ResetZoomOnRelease=" << (g_config.resetZoomOnRelease ? 1 : 0) << '\n';
    file << "KeepDrawingsOnRelease=" << (g_config.keepDrawingsOnRelease ? 1 : 0) << '\n';
    file << "HideToastsFromCapture=" << (g_config.hideToastsFromCapture ? 1 : 0) << '\n';
    file << "FirstRun=" << (g_config.isFirstRun ? 1 : 0) << '\n';
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
    ComputeBreakTimerCenter(s_breakTimerCenterX, s_breakTimerCenterY);

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

static void ComputeBreakTimerCenter(float& cx, float& cy) {
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
    cx = ((mi.rcMonitor.left + mi.rcMonitor.right) / 2.0f) - static_cast<float>(vScreenX);
    cy = ((mi.rcMonitor.top + mi.rcMonitor.bottom) / 2.0f) - static_cast<float>(vScreenY);
}

void GetBreakTimerCenter(float& cx, float& cy) {
    cx = s_breakTimerCenterX;
    cy = s_breakTimerCenterY;
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
    float progress = (g_breakTimerTotalSec > 0) ? (static_cast<float>(g_breakTimerRemainingSec) / static_cast<float>(g_breakTimerTotalSec)) : 0.0f;
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

    ScopedScreenDC screenDC;
    if (!screenDC) return;

    g_memDC = CreateCompatibleDC(screenDC.get());
    if (g_memDC) {
        g_memBitmap = CreateDIBSection(screenDC.get(), &bmi, DIB_RGB_COLORS, &g_dibBits, NULL, 0);
        if (g_memBitmap) {
            g_oldBitmap = static_cast<HBITMAP>(SelectObject(g_memDC, g_memBitmap));
        }
        else {
            DeleteDC(g_memDC);
            g_memDC = NULL;
            g_dibBits = NULL;
        }
    }
}

static void AddRoundedRectPath(GraphicsPath& path, REAL x, REAL y, REAL w, REAL h, REAL radius) {
    const REAL r = std::min(radius, std::min(w, h) / 2.0f);
    const REAL d = r * 2.0f;
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

static std::vector<std::wstring> SplitKeycastCombo(const std::wstring& combo) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (true) {
        const size_t pos = combo.find(L" + ", start);
        if (pos == std::wstring::npos) {
            parts.push_back(combo.substr(start));
            break;
        }
        parts.push_back(combo.substr(start, pos - start));
        start = pos + 3;
    }
    return parts;
}

static bool IsModifierKeyName(const std::wstring& name) {
    return name == L"Ctrl" || name == L"Alt" || name == L"Shift" || name == L"Win";
}

static Color WithFade(const Color& color, float alpha) {
    const int a = std::clamp(static_cast<int>(static_cast<float>(color.GetA()) * alpha + 0.5f), 0, 255);
    return Color(static_cast<BYTE>(a), color.GetR(), color.GetG(), color.GetB());
}

void PresentOverlayFrame() {
    if (!g_hwndOverlay) return;
    if (!g_memDC || !g_dibBits || g_backWidth <= 0 || g_backHeight <= 0) {
        CreateOverlayBackbuffer();
        if (!g_memDC || !g_dibBits) return;
    }

    memset(g_dibBits, 0, static_cast<size_t>(g_backStride) * static_cast<size_t>(g_backHeight));

    POINT curPt = s_cursorPos;
    {
        POINT pt;
        if (GetCursorPos(&pt)) {
            s_cursorPos = pt;
            curPt = pt;
        }
    }

    {
        Bitmap surface(g_backWidth, g_backHeight, g_backStride, PixelFormat32bppPARGB, static_cast<BYTE*>(g_dibBits));
        Graphics g(&surface);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

        if (g_spotlightMode) {
            GraphicsPath spotPath;
            spotPath.AddRectangle(Rect(0, 0, g_backWidth, g_backHeight));
            float r = 180.0f;
            float cx = static_cast<float>(curPt.x - GetSystemMetrics(SM_XVIRTUALSCREEN));
            float cy = static_cast<float>(curPt.y - GetSystemMetrics(SM_YVIRTUALSCREEN));
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
                    int l = std::min(g_cropStart.x, g_cropEnd.x) - vScreenX;
                    int t = std::min(g_cropStart.y, g_cropEnd.y) - vScreenY;
                    int r = std::max(g_cropStart.x, g_cropEnd.x) - vScreenX;
                    int b = std::max(g_cropStart.y, g_cropEnd.y) - vScreenY;
                    g.FillRectangle(&dimCrop, 0, 0, g_backWidth, t);
                    g.FillRectangle(&dimCrop, 0, b, g_backWidth, g_backHeight - b);
                    g.FillRectangle(&dimCrop, 0, t, l, b - t);
                    g.FillRectangle(&dimCrop, r, t, g_backWidth - r, b - t);
                    Pen cropPen(Color(255, 0, 160, 255), 1.5f);
                    cropPen.SetDashStyle(DashStyleDash);
                    g.DrawRectangle(&cropPen, static_cast<REAL>(l), static_cast<REAL>(t), static_cast<REAL>(r - l), static_cast<REAL>(b - t));
                }
                else {
                    g.FillRectangle(&dimCrop, 0, 0, g_backWidth, g_backHeight);
                }
            }
        }

        if (g_keycastText) {
            const float capPadX = 7.0f;
            const float capPadY = 4.0f;
            const float capRadius = 8.0f;
            const float plusWidth = 24.0f;
            const float cardPadX = 13.0f;
            const float cardPadY = 11.0f;
            const float cardRadius = 16.0f;

            Font fontKey(L"Segoe UI", 13.0f, FontStyleBold);
            Font fontPlus(L"Segoe UI", 12.0f, FontStyleRegular);
            StringFormat fmtKey;
            fmtKey.SetAlignment(StringAlignmentCenter);
            fmtKey.SetLineAlignment(StringAlignmentCenter);
            StringFormat fmtMeasure;
            fmtMeasure.SetFormatFlags(StringFormatFlagsNoWrap);

            const std::vector<std::wstring> parts = SplitKeycastCombo(g_keycastTextValue);

            std::vector<float> capWidths(parts.size(), 0.0f);
            float capHeight = 0.0f;
            for (size_t i = 0; i < parts.size(); ++i) {
                RectF bounds;
                g.MeasureString(parts[i].c_str(), -1, &fontKey, PointF(0.0f, 0.0f), &fmtMeasure, &bounds);
                capWidths[i] = bounds.Width + capPadX * 2.0f;
                capHeight = std::max(capHeight, bounds.Height + capPadY * 2.0f);
            }

            float contentWidth = 0.0f;
            for (size_t i = 0; i < parts.size(); ++i) {
                contentWidth += capWidths[i];
                if (i + 1 < parts.size()) contentWidth += plusWidth;
            }

            const float cardW = contentWidth + cardPadX * 2.0f;
            const float cardH = capHeight + cardPadY * 2.0f;
            const float fade = s_keycastAlpha;
            const float slide = (1.0f - fade) * 12.0f;

            const POINT toastAnchor = ToastAnchor();
            const float slotRight = static_cast<float>(toastAnchor.x + TOAST_W - GetSystemMetrics(SM_XVIRTUALSCREEN));
            const float slotBottom = static_cast<float>(toastAnchor.y - GetSystemMetrics(SM_YVIRTUALSCREEN) - KEYCAST_TOAST_GAP);
            const float cardX = std::max(8.0f, slotRight - cardW);
            const float cardY = slotBottom - cardH + slide;

            GraphicsPath shadowPath;
            AddRoundedRectPath(shadowPath, cardX, cardY + 4.0f, cardW, cardH, cardRadius);
            SolidBrush shadowBrush(WithFade(Color(85, 0, 0, 0), fade));
            g.FillPath(&shadowBrush, &shadowPath);

            GraphicsPath cardPath;
            AddRoundedRectPath(cardPath, cardX, cardY, cardW, cardH, cardRadius);
            LinearGradientBrush cardBrush(RectF(cardX, cardY, cardW, cardH), WithFade(Color(240, 34, 36, 47), fade), WithFade(Color(240, 15, 16, 23), fade), LinearGradientModeVertical);
            g.FillPath(&cardBrush, &cardPath);
            Pen cardPen(WithFade(Color(210, 120, 122, 148), fade), 1.0f);
            g.DrawPath(&cardPen, &cardPath);

            float cursorX = cardX + cardPadX;
            const float capY = cardY + cardPadY;
            for (size_t i = 0; i < parts.size(); ++i) {
                const RectF capRect(cursorX, capY, capWidths[i], capHeight);
                const bool isModifier = IsModifierKeyName(parts[i]);

                GraphicsPath capPath;
                AddRoundedRectPath(capPath, cursorX, capY, capWidths[i], capHeight, capRadius);
                LinearGradientBrush capBrush(capRect,
                    WithFade(isModifier ? Color(255, 84, 122, 216) : Color(255, 92, 92, 108), fade),
                    WithFade(isModifier ? Color(255, 38, 72, 158) : Color(255, 48, 48, 60), fade),
                    LinearGradientModeVertical);
                g.FillPath(&capBrush, &capPath);
                Pen capPen(WithFade(Color(170, 255, 255, 255), fade), 1.0f);
                g.DrawPath(&capPen, &capPath);

                SolidBrush keyBrush(WithFade(Color(255, 246, 247, 252), fade));
                g.DrawString(parts[i].c_str(), -1, &fontKey, capRect, &fmtKey, &keyBrush);

                cursorX += capWidths[i];
                if (i + 1 < parts.size()) {
                    SolidBrush plusBrush(WithFade(Color(200, 178, 180, 198), fade));
                    g.DrawString(L"+", -1, &fontPlus, RectF(cursorX, capY, plusWidth, capHeight), &fmtKey, &plusBrush);
                    cursorX += plusWidth;
                }
            }
        }
    }

    ScopedScreenDC screenDC;
    if (screenDC) {
        SIZE size = { g_backWidth, g_backHeight };
        POINT ptDst = { GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN) };
        POINT ptSrc = { 0, 0 };
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(g_hwndOverlay, screenDC.get(), &ptDst, &size, g_memDC, &ptSrc, 0, &blend, ULW_ALPHA);
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
        float age = static_cast<float>(now - s.birthTick);
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

    bool bakedBlur = false;
    for (auto& s : g_strokes) {
        if (s.type != StrokeType::Blur || s.cachedBitmap) continue;
        if ((s.cachedRect.right - s.cachedRect.left) < 8 || (s.cachedRect.bottom - s.cachedRect.top) < 8) continue;
        if (!bakedBlur && g_hwndOverlay) {
            ShowWindow(g_hwndOverlay, SW_HIDE);
            bakedBlur = true;
        }
        s.cachedBitmap = BakeBlurredBitmap(s.cachedRect);
        s_framePending = true;
    }
    if (bakedBlur) {
        ShowWindow(g_hwndOverlay, SW_SHOWNOACTIVATE);
    }

    POINT pt = { 0, 0 };
    bool mouseMoved = false;
    if (GetCursorPos(&pt)) {
        if (pt.x != s_cursorPos.x || pt.y != s_cursorPos.y) {
            s_cursorPos = pt;
            mouseMoved = true;
        }
    }

    const ULONGLONG nowTick = GetTickCount64();

    static bool s_prevSpotlight = false;
    static bool s_prevKeycast = false;
    static std::wstring s_prevKeycastValue;
    if (g_spotlightMode != s_prevSpotlight) {
        s_prevSpotlight = g_spotlightMode;
        s_framePending = true;
    }
    if (g_keycastText != s_prevKeycast || g_keycastTextValue != s_prevKeycastValue) {
        if (g_keycastText && !s_prevKeycast) s_keycastShownTick = nowTick;
        s_prevKeycast = g_keycastText;
        s_prevKeycastValue = g_keycastTextValue;
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

    static bool s_prevTextInputActive = false;
    if (g_isTextInputActive != s_prevTextInputActive) {
        s_prevTextInputActive = g_isTextInputActive;
        if (g_hwndOverlay) {
            if (g_isTextInputActive) SetTimer(g_hwndOverlay, 4, 500, NULL);
            else KillTimer(g_hwndOverlay, 4);
        }
        s_framePending = true;
    }

    if (g_keycastText && nowTick > g_keycastUntilTick) {
        g_keycastText = false;
        s_framePending = true;
    }

    if (g_keycastText) {
        const ULONGLONG elapsed = (nowTick > s_keycastShownTick) ? (nowTick - s_keycastShownTick) : 0;
        const ULONGLONG remaining = (g_keycastUntilTick > nowTick) ? (g_keycastUntilTick - nowTick) : 0;
        const float ramp = std::min(1.0f, std::min(static_cast<float>(elapsed) / static_cast<float>(KEYCAST_FADE_IN_MS),
                                              static_cast<float>(remaining) / static_cast<float>(KEYCAST_FADE_OUT_MS)));
        if (ramp < 1.0f) s_framePending = true;
        s_keycastAlpha = ramp * ramp * (3.0f - 2.0f * ramp);
    }
    else {
        s_keycastAlpha = 0.0f;
    }

    bool animating = g_keycastText;
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

    const int primaryW = GetSystemMetrics(SM_CXSCREEN);
    const int primaryH = GetSystemMetrics(SM_CYSCREEN);
    const bool onPrimary = (pt.x >= 0 && pt.x < primaryW && pt.y >= 0 && pt.y < primaryH);

    float targetX = 0.0f;
    float targetY = 0.0f;
    if (onPrimary) {
        targetX = static_cast<float>(pt.x) - (static_cast<float>(pt.x) / g_currentZoom);
        targetY = static_cast<float>(pt.y) - (static_cast<float>(pt.y) / g_currentZoom);
    }
    else {
        targetX = static_cast<float>(pt.x) - (primaryW / 2.0f) / g_currentZoom;
        targetY = static_cast<float>(pt.y) - (primaryH / 2.0f) / g_currentZoom;
    }

    g_camX += (targetX - g_camX) * 0.35f;
    g_camY += (targetY - g_camY) * 0.35f;
    MagSetFullscreenTransform(g_currentZoom, static_cast<int>(g_camX), static_cast<int>(g_camY));
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
    float x1 = static_cast<float>(p1.x - offX);
    float y1 = static_cast<float>(p1.y - offY);
    float x2 = static_cast<float>(p2.x - offX);
    float y2 = static_cast<float>(p2.y - offY);

    float dx = x2 - x1;
    float dy = y2 - y1;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 4.0f) return;

    float ux = dx / dist;
    float uy = dy / dist;
    float nx = -uy;
    float ny = ux;

    float arrowLen = static_cast<float>(width) * 3.2f + 10.0f;
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

    ScopedScreenDC screenDC;
    if (!screenDC) return nullptr;

    ScopedMemoryDC capDC(screenDC.get());
    UniqueBitmap capBmp(static_cast<HBITMAP>(CreateCompatibleBitmap(screenDC.get(), w, h)));
    if (!capDC || !capBmp) return nullptr;

    ScopedSelectedObject selected(capDC.get(), capBmp.get());
    BitBlt(capDC.get(), 0, 0, w, h, screenDC.get(), rc.left, rc.top, SRCCOPY);
    selected.restore();

    auto resultBmp = std::make_shared<Bitmap>(w, h, PixelFormat32bppPARGB);
    {
        Bitmap src(capBmp.get(), NULL);
        int sw = std::max(2, w / 14);
        int sh = std::max(2, h / 14);

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
    return resultBmp;
}

static void ApplyStrokeAlpha(const Stroke& stroke, Color& col) {
    if (stroke.opacity >= 0.999f) return;
    col = Color(static_cast<BYTE>(col.GetAlpha() * stroke.opacity), col.GetRed(), col.GetGreen(), col.GetBlue());
}

void DrawStroke(Graphics& g, const Stroke& stroke, int offX, int offY) {
    if (stroke.type == StrokeType::Text) {
        if (stroke.points.empty()) return;
        COLORREF c = stroke.color ? stroke.color : g_config.lineColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, col);
        SolidBrush textBrush(col);
        Font font(L"Segoe UI", 18.0f, FontStyleBold);
        PointF origin(static_cast<REAL>(stroke.points[0].x - offX), static_cast<REAL>(stroke.points[0].y - offY));
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

        RectF rect(static_cast<REAL>(x - radius), static_cast<REAL>(y - radius + 1), static_cast<REAL>(radius * 2), static_cast<REAL>(radius * 2));
        g.DrawString(numStr.c_str(), -1, &font, rect, &fmt, &textBrush);
        return;
    }

    if (stroke.points.size() < 2) return;

    if (stroke.type == StrokeType::Blur) {
        if (stroke.cachedBitmap) {
            g.DrawImage(stroke.cachedBitmap.get(),
                static_cast<REAL>(stroke.cachedRect.left - offX),
                static_cast<REAL>(stroke.cachedRect.top - offY));
        }
        else {
            POINT a = stroke.points.front();
            POINT b = stroke.points.back();
            int left = std::min(a.x, b.x) - offX;
            int right = std::max(a.x, b.x) - offX;
            int top = std::min(a.y, b.y) - offY;
            int bottom = std::max(a.y, b.y) - offY;

            SolidBrush previewBrush(Color(140, 16, 16, 18));
            g.FillRectangle(&previewBrush, static_cast<REAL>(left), static_cast<REAL>(top), static_cast<REAL>(right - left), static_cast<REAL>(bottom - top));

            Pen previewPen(Color(200, 200, 200, 200), 1.0f);
            previewPen.SetDashStyle(DashStyleDash);
            g.DrawRectangle(&previewPen, static_cast<REAL>(left), static_cast<REAL>(top), static_cast<REAL>(right - left), static_cast<REAL>(bottom - top));
        }
        return;
    }

    if (stroke.type == StrokeType::Highlight) {
        COLORREF c = stroke.color ? stroke.color : RGB(250, 205, 40);
        POINT a = stroke.points.front();
        POINT b = stroke.points.back();
        int left = std::min(a.x, b.x) - offX;
        int right = std::max(a.x, b.x) - offX;
        int top = std::min(a.y, b.y) - offY;
        int bottom = std::max(a.y, b.y) - offY;
        Color fillCol(110, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, fillCol);
        SolidBrush fill(fillCol);
        g.FillRectangle(&fill, static_cast<REAL>(left), static_cast<REAL>(top), static_cast<REAL>(right - left), static_cast<REAL>(bottom - top));
        return;
    }

    if (stroke.type == StrokeType::Line) {
        COLORREF c = stroke.color ? stroke.color : g_config.lineColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, col);
        Pen pen(col, static_cast<REAL>(g_config.lineWidth));
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
        Pen pen(col, static_cast<REAL>(g_config.arrowWidth));
        SolidBrush brush(col);
        DrawArrow(g, pen, brush, stroke.points.front(), stroke.points.back(), g_config.arrowWidth, offX, offY);
    }
    else if (stroke.type == StrokeType::Rectangle) {
        COLORREF c = stroke.color ? stroke.color : g_config.rectColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, col);
        Pen pen(col, static_cast<REAL>(g_config.rectWidth));
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        pen.SetLineJoin(LineJoinMiter);

        POINT a = stroke.points.front();
        POINT b = stroke.points.back();
        int left = std::min(a.x, b.x) - offX;
        int right = std::max(a.x, b.x) - offX;
        int top = std::min(a.y, b.y) - offY;
        int bottom = std::max(a.y, b.y) - offY;
        g.DrawRectangle(&pen, static_cast<REAL>(left), static_cast<REAL>(top), static_cast<REAL>(right - left), static_cast<REAL>(bottom - top));
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

    ScopedScreenDC screenDC;
    if (!screenDC) return;

    ScopedMemoryDC captureDC(screenDC.get());
    UniqueBitmap captureBmp(static_cast<HBITMAP>(CreateCompatibleBitmap(screenDC.get(), scrW, scrH)));
    if (!captureDC || !captureBmp) return;

    ScopedSelectedObject selected(captureDC.get(), captureBmp.get());
    BitBlt(captureDC.get(), 0, 0, scrW, scrH, screenDC.get(), vScreenX, vScreenY, SRCCOPY);

    {
        Graphics g(captureDC.get());
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        FillBoardBackground(g, 0, 0, scrW, scrH);
        for (const auto& s : g_strokes) DrawStroke(g, s, vScreenX, vScreenY);
        if (!g_currentStroke.points.empty()) DrawStroke(g, g_currentStroke, vScreenX, vScreenY);
    }

    selected.restore();

    ScopedClipboard clipboard(g_hwndOverlay);
    if (!clipboard.ok()) return;

    EmptyClipboard();
    if (SetClipboardData(CF_BITMAP, captureBmp.get())) captureBmp.release();
}

void CopyRegionToClipboard(RECT rcScreen) {
    int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);

    int left = std::max(static_cast<int>(rcScreen.left), vScreenX);
    int top = std::max(static_cast<int>(rcScreen.top), vScreenY);
    int right = std::min(static_cast<int>(rcScreen.right), vScreenX + GetSystemMetrics(SM_CXVIRTUALSCREEN));
    int bottom = std::min(static_cast<int>(rcScreen.bottom), vScreenY + GetSystemMetrics(SM_CYVIRTUALSCREEN));

    int w = right - left;
    int h = bottom - top;
    if (w < 1 || h < 1) return;

    ScopedScreenDC screenDC;
    if (!screenDC) return;

    ScopedMemoryDC captureDC(screenDC.get());
    UniqueBitmap captureBmp(static_cast<HBITMAP>(CreateCompatibleBitmap(screenDC.get(), w, h)));
    if (!captureDC || !captureBmp) return;

    ScopedSelectedObject selected(captureDC.get(), captureBmp.get());
    BitBlt(captureDC.get(), 0, 0, w, h, screenDC.get(), left, top, SRCCOPY);

    {
        Graphics g(captureDC.get());
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        FillBoardBackground(g, 0, 0, w, h);
        for (const auto& s : g_strokes) DrawStroke(g, s, left, top);
        if (!g_currentStroke.points.empty()) DrawStroke(g, g_currentStroke, left, top);
    }

    selected.restore();

    ScopedClipboard clipboard(g_hwndOverlay);
    if (!clipboard.ok()) return;

    EmptyClipboard();
    if (SetClipboardData(CF_BITMAP, captureBmp.get())) captureBmp.release();
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
    ScopedScreenDC screenDC;
    if (!screenDC) return;

    ScopedMemoryDC memDC(screenDC.get());
    UniqueBitmap hBmp(CreateDIBSection(screenDC.get(), &bmi, DIB_RGB_COLORS, &bits, NULL, 0));
    if (!memDC || !hBmp || !bits) return;

    ScopedSelectedObject selected(memDC.get(), hBmp.get());
    memset(bits, 0, static_cast<size_t>(stride) * static_cast<size_t>(h));

    {
        Bitmap surface(w, h, stride, PixelFormat32bppPARGB, static_cast<BYTE*>(bits));
        Graphics g(&surface);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

        GraphicsPath cardPath;
        AddRoundedRectPath(cardPath, 0.0f, 0.0f, static_cast<REAL>(w), static_cast<REAL>(h), 12.0f);
        LinearGradientBrush bgCard(RectF(0.0f, 0.0f, static_cast<REAL>(w), static_cast<REAL>(h)), Color(242, 34, 36, 47), Color(242, 15, 16, 23), LinearGradientModeVertical);
        g.FillPath(&bgCard, &cardPath);

        GraphicsPath borderPath;
        AddRoundedRectPath(borderPath, 0.5f, 0.5f, static_cast<REAL>(w) - 1.0f, static_cast<REAL>(h) - 1.0f, 12.0f);
        Pen borderPen(Color(190, 120, 122, 148), 1.0f);
        g.DrawPath(&borderPen, &borderPath);

        const Color accentCol(255, GetRValue(s_toastAccent), GetGValue(s_toastAccent), GetBValue(s_toastAccent));

        GraphicsPath glowPath;
        AddRoundedRectPath(glowPath, 6.0f, 6.0f, 16.0f, static_cast<REAL>(h) - 12.0f, 8.0f);
        SolidBrush glowBrush(Color(45, accentCol.GetR(), accentCol.GetG(), accentCol.GetB()));
        g.FillPath(&glowBrush, &glowPath);

        GraphicsPath barPath;
        AddRoundedRectPath(barPath, 11.0f, 11.0f, 4.0f, static_cast<REAL>(h) - 22.0f, 2.0f);
        SolidBrush barBrush(accentCol);
        g.FillPath(&barBrush, &barPath);

        Font fontTitle(L"Segoe UI", 10.5f, FontStyleBold);
        Font fontMsg(L"Segoe UI", 9.0f);
        SolidBrush textWhite(Color(255, 242, 243, 248));
        SolidBrush textDim(Color(255, 176, 178, 190));
        StringFormat fmtText;
        fmtText.SetFormatFlags(StringFormatFlagsNoWrap);
        fmtText.SetTrimming(StringTrimmingEllipsisCharacter);

        g.DrawString(s_toastTitle.c_str(), -1, &fontTitle, RectF(27.0f, 12.0f, static_cast<REAL>(w) - 39.0f, 20.0f), &fmtText, &textWhite);
        g.DrawString(s_toastMsg.c_str(), -1, &fontMsg, RectF(27.0f, 34.0f, static_cast<REAL>(w) - 39.0f, 20.0f), &fmtText, &textDim);
    }

    BYTE alphaByte = static_cast<BYTE>(s_toastAlpha * 255.0f);
    POINT ptSrc = { 0, 0 };
    SIZE sz = { w, h };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, alphaByte, AC_SRC_ALPHA };
    UpdateLayeredWindow(g_hwndToast, screenDC.get(), NULL, &sz, memDC.get(), &ptSrc, 0, &blend, ULW_ALPHA);
}

static POINT ToastAnchor() {
    POINT cursor = { 0, 0 };
    GetCursorPos(&cursor);

    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = { sizeof(MONITORINFO) };
    if (monitor && GetMonitorInfo(monitor, &info)) {
        return { info.rcWork.right - TOAST_W - 24, info.rcWork.bottom - TOAST_H - 24 };
    }
    return { GetSystemMetrics(SM_CXSCREEN) - TOAST_W - 24, GetSystemMetrics(SM_CYSCREEN) - TOAST_H - 48 };
}

void InitToastWindow(HINSTANCE hInstance) {
    const POINT anchor = ToastAnchor();

    g_hwndToast = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"LensItToast", L"Notification", WS_POPUP,
        anchor.x, anchor.y, TOAST_W, TOAST_H, NULL, NULL, hInstance, NULL
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
    const POINT anchor = ToastAnchor();
    SetWindowPos(g_hwndToast, HWND_TOPMOST, anchor.x, anchor.y, TOAST_W, TOAST_H, SWP_NOACTIVATE);
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
            AppendMenu(hMenu, MF_OWNERDRAW, ID_TRAY_SETTINGS, reinterpret_cast<LPCTSTR>(ID_TRAY_SETTINGS));
            AppendMenu(hMenu, MF_OWNERDRAW, ID_TRAY_EXIT, reinterpret_cast<LPCTSTR>(ID_TRAY_EXIT));
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
            PostMessageW(hwnd, WM_NULL, 0, 0);
            DestroyMenu(hMenu);
            if (cmd == ID_TRAY_SETTINGS) ShowSettingsWindow(GetModuleHandle(NULL));
            else if (cmd == ID_TRAY_EXIT) PostQuitMessage(0);
        }
        return 0;

    case WM_APP_SHOWSETTINGS:
        ShowSettingsWindow(GetModuleHandle(NULL));
        return 0;

    case WM_APP_TAKE_SCREENSHOT:
        CopyScreenshotToClipboard();
        ShowNotification(L"Screenshot", L"Copied to clipboard!", RGB(46, 204, 113));
        return 0;

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        mis->itemWidth = 140; mis->itemHeight = 32;
        return TRUE;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        Graphics g(dis->hDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        bool isHover = (dis->itemState & ODS_SELECTED);
        SolidBrush bgBrush(isHover ? Color(255, 60, 60, 60) : Color(255, 30, 30, 30));
        g.FillRectangle(&bgBrush, static_cast<int>(dis->rcItem.left), static_cast<int>(dis->rcItem.top), 160, 32);

        SolidBrush textBrush(Color(255, 220, 220, 220));
        Font font(L"Segoe UI", 11);
        StringFormat format; format.SetAlignment(StringAlignmentNear); format.SetLineAlignment(StringAlignmentCenter);
        RectF textRect(static_cast<REAL>(dis->rcItem.left) + 36, static_cast<REAL>(dis->rcItem.top), 120.0f, 32.0f);

        Pen iconPen(Color(220, 220, 220), 2.0f);
        if (dis->itemID == ID_TRAY_SETTINGS) {
            g.DrawString(L"Settings", -1, &font, textRect, &format, &textBrush);
            g.DrawEllipse(&iconPen, static_cast<int>(dis->rcItem.left) + 12, static_cast<int>(dis->rcItem.top) + 8, 14, 14);
            g.DrawEllipse(&iconPen, static_cast<int>(dis->rcItem.left) + 15, static_cast<int>(dis->rcItem.top) + 11, 8, 8);
        }
        else if (dis->itemID == ID_TRAY_EXIT) {
            g.DrawString(L"Exit", -1, &font, textRect, &format, &textBrush);
            g.DrawLine(&iconPen, static_cast<int>(dis->rcItem.left) + 13, static_cast<int>(dis->rcItem.top) + 10, static_cast<int>(dis->rcItem.left) + 25, static_cast<int>(dis->rcItem.top) + 22);
            g.DrawLine(&iconPen, static_cast<int>(dis->rcItem.left) + 25, static_cast<int>(dis->rcItem.top) + 10, static_cast<int>(dis->rcItem.left) + 13, static_cast<int>(dis->rcItem.top) + 22);
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
        else if (wParam == 4) {
            RedrawOverlay();
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