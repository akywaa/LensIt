#include "BreakTimer.h"
#include "LensIt.h"

static float s_breakTimerCenterX = 0.0f;
static float s_breakTimerCenterY = 0.0f;

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
    g_app.breakTimer.totalSec = minutes * 60;
    g_app.breakTimer.remainingSec = g_app.breakTimer.totalSec;
    g_app.breakTimer.active = true;
    g_app.breakTimer.paused = false;
    g_app.breakTimer.editing = false;
    g_app.breakTimer.inputStr.clear();
    ComputeBreakTimerCenter(s_breakTimerCenterX, s_breakTimerCenterY);

    if (g_hwndOverlay) {
        SetTimer(g_hwndOverlay, 2, 1000, NULL);
    }
    RedrawOverlay();
    ShowNotification(L"Break Timer", L"Started (" + std::to_wstring(minutes) + L" min)", RGB(0, 150, 255));
}

void StopBreakTimer() {
    if (!g_app.breakTimer.active) return;
    g_app.breakTimer.active = false;
    g_app.breakTimer.paused = false;
    g_app.breakTimer.editing = false;
    g_app.breakTimer.inputStr.clear();
    if (g_hwndOverlay) {
        KillTimer(g_hwndOverlay, 2);
    }
    RedrawOverlay();
}

void ToggleBreakTimer(int minutes) {
    if (g_app.breakTimer.active) {
        StopBreakTimer();
        ShowNotification(L"Break Timer", L"Dismissed", RGB(220, 70, 70));
    }
    else {
        StartBreakTimer(minutes);
    }
}

void CommitBreakTimerInput() {
    if (!g_app.breakTimer.editing) return;
    int newSec = 0;
    if (ParseTimerString(g_app.breakTimer.inputStr, newSec)) {
        if (newSec > 5999) newSec = 5999;
        g_app.breakTimer.remainingSec = newSec;
        g_app.breakTimer.totalSec = newSec;

        wchar_t buf[32];
        swprintf_s(buf, L"Set to %02d:%02d", newSec / 60, newSec % 60);
        ShowNotification(L"Timer Updated", buf, RGB(0, 150, 255));
    }
    g_app.breakTimer.editing = false;
    g_app.breakTimer.paused = false;
    g_app.breakTimer.inputStr.clear();
    RedrawOverlay();
}

void DrawBreakTimerUI(Graphics& g, int w, int h) {
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
    float progress = (g_app.breakTimer.totalSec > 0) ? (static_cast<float>(g_app.breakTimer.remainingSec) / static_cast<float>(g_app.breakTimer.totalSec)) : 0.0f;
    progress = std::clamp(progress, 0.0f, 1.0f);

    float barX = cx - totalW / 2.0f;
    float barY = cy + 65.0f;

    SolidBrush trackBg(Color(255, 45, 45, 52));
    g.FillRectangle(&trackBg, barX, barY, totalW, barH);

    Color accentCol = (g_app.breakTimer.remainingSec <= 30 && !g_app.breakTimer.editing) ? Color(255, 235, 60, 60) : Color(255, 0, 140, 255);
    SolidBrush fillBrush(accentCol);
    g.FillRectangle(&fillBrush, barX, barY, totalW * progress, barH);

    RectF clockRect(cx - 300.0f, cy - 90.0f, 600.0f, 130.0f);

    if (g_app.breakTimer.editing) {
        Pen editBorder(Color(255, 0, 160, 255), 2.0f);
        SolidBrush editBg(Color(120, 20, 30, 45));
        g.FillRectangle(&editBg, cx - 220.0f, cy - 85.0f, 440.0f, 125.0f);
        g.DrawRectangle(&editBorder, cx - 220.0f, cy - 85.0f, 440.0f, 125.0f);

        std::wstring disp = g_app.breakTimer.inputStr.empty() ? L"__ : __" : (g_app.breakTimer.inputStr + L"|");
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
        int mins = g_app.breakTimer.remainingSec / 60;
        int secs = g_app.breakTimer.remainingSec % 60;
        wchar_t timeBuf[32];
        swprintf_s(timeBuf, L"%02d:%02d", mins, secs);

        SolidBrush textWhite(g_app.breakTimer.remainingSec <= 30 ? Color(255, 255, 100, 100) : Color(255, 245, 245, 250));
        g.DrawString(timeBuf, -1, &fontClock, clockRect, &fmtCenter, &textWhite);

        SolidBrush textAccent(accentCol);
        std::wstring titleStr = g_app.breakTimer.paused ? L"BREAK TIMER  *  [PAUSED]" : L"BREAK IN PROGRESS";
        RectF titleRect(cx - 300.0f, cy - 125.0f, 600.0f, 30.0f);
        g.DrawString(titleStr.c_str(), -1, &fontSub, titleRect, &fmtCenter, &textAccent);

        SolidBrush textHint(Color(255, 160, 160, 165));
        std::wstring hintStr = L"Wheel: min (+Shift: sec)  *  Click time: edit  *  Space: pause  *  Esc: exit";
        RectF hintRect(cx - 300.0f, cy + 90.0f, 600.0f, 25.0f);
        g.DrawString(hintStr.c_str(), -1, &fontHint, hintRect, &fmtCenter, &textHint);
    }
}