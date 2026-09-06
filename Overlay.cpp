#include "LensIt.h"

static char g_configBuf[MAX_PATH];
static bool g_configPathReady = false;

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
    g_config.resetZoomOnRelease = GetPrivateProfileIntA("Config", "ResetZoomOnRelease", g_config.resetZoomOnRelease ? 1 : 0, g_configBuf) != 0;
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
    snprintf(buf, sizeof(buf), "%d", g_config.resetZoomOnRelease ? 1 : 0);
    WritePrivateProfileStringA("Config", "ResetZoomOnRelease", buf, g_configBuf);

    WritePrivateProfileStringA(NULL, NULL, NULL, g_configBuf);
}

void UpdateCamera() {
    if (g_currentZoom <= 1.0f) {
        MagSetFullscreenTransform(1.0f, 0, 0);
        g_camX = g_camY = 0; return;
    }
    POINT pt; GetCursorPos(&pt);
    float targetX = pt.x - (pt.x / g_currentZoom);
    float targetY = pt.y - (pt.y / g_currentZoom);
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
    InvalidateRect(g_hwndOverlay, NULL, FALSE);
}

void DrawArrow(Graphics& g, Pen& pen, SolidBrush& brush, POINT p1, POINT p2, int width) {
    float dx = (float)(p2.x - p1.x);
    float dy = (float)(p2.y - p1.y);
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 3.0f) return;

    float ux = dx / dist;
    float uy = dy / dist;
    float nx = -uy;
    float ny = ux;

    float arrowLen = (float)width * 3.2f + 10.0f;
    if (arrowLen > dist * 0.85f) arrowLen = dist * 0.85f;
    float arrowHalfWidth = arrowLen * 0.55f;

    float overlap = 2.0f;
    float lineEndX = p2.x - ux * (arrowLen - overlap);
    float lineEndY = p2.y - uy * (arrowLen - overlap);

    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapFlat);
    g.DrawLine(&pen, (REAL)p1.x, (REAL)p1.y, lineEndX, lineEndY);

    PointF pts[3];
    pts[0] = PointF((REAL)p2.x, (REAL)p2.y);
    pts[1] = PointF((REAL)(p2.x - ux * arrowLen + nx * arrowHalfWidth), (REAL)(p2.y - uy * arrowLen + ny * arrowHalfWidth));
    pts[2] = PointF((REAL)(p2.x - ux * arrowLen - nx * arrowHalfWidth), (REAL)(p2.y - uy * arrowLen - ny * arrowHalfWidth));

    g.FillPolygon(&brush, pts, 3);
}

void DrawStroke(Graphics& g, const Stroke& stroke) {
    if (stroke.points.size() < 2) return;

    if (stroke.type == StrokeType::Line) {
        Color c(255, GetRValue(g_config.lineColor), GetGValue(g_config.lineColor), GetBValue(g_config.lineColor));
        Pen pen(c, (REAL)g_config.lineWidth);
        pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound);
        g.DrawLines(&pen, (const Point*)stroke.points.data(), (INT)stroke.points.size());
    }
    else if (stroke.type == StrokeType::Arrow) {
        Color c(255, GetRValue(g_config.arrowColor), GetGValue(g_config.arrowColor), GetBValue(g_config.arrowColor));
        Pen pen(c, (REAL)g_config.arrowWidth);
        SolidBrush brush(c);
        DrawArrow(g, pen, brush, stroke.points.front(), stroke.points.back(), g_config.arrowWidth);
    }
    else if (stroke.type == StrokeType::Rectangle) {
        Color c(255, GetRValue(g_config.rectColor), GetGValue(g_config.rectColor), GetBValue(g_config.rectColor));
        Pen pen(c, (REAL)g_config.rectWidth);
        pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinMiter);
        POINT a = stroke.points.front();
        POINT b = stroke.points.back();
        int left = min(a.x, b.x), right = max(a.x, b.x);
        int top = min(a.y, b.y), bottom = max(a.y, b.y);
        g.DrawRectangle(&pen, (REAL)left, (REAL)top, (REAL)(right - left), (REAL)(bottom - top));
    }
}

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
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

    case WM_TIMER: UpdateCamera(); return 0;

    case WM_DISPLAYCHANGE:
        g_currentZoom = 1.0f;
        g_strokes.clear();
        g_currentStroke.points.clear();
        RepositionOverlay();
        UpdateCamera();
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int scrW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int scrH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, scrW, scrH);
        SelectObject(memDC, memBitmap);

        RECT rc = { 0, 0, scrW, scrH };
        HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(memDC, &rc, bg); DeleteObject(bg);

        Graphics graphics(memDC);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);

        for (const auto& s : g_strokes) DrawStroke(graphics, s);
        if (!g_currentStroke.points.empty()) DrawStroke(graphics, g_currentStroke);

        BitBlt(hdc, vScreenX, vScreenY, scrW, scrH, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBitmap); DeleteDC(memDC); EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}