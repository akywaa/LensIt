#include "LensIt.h"
#include "WinHandles.h"
#include "Canvas.h"
#include "BreakTimer.h"
#include "Keycast.h"

#include <filesystem>
#include <fstream>
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

static bool s_framePending = false;
static POINT s_cursorPos = { 0, 0 };

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

void AddRoundedRect(GraphicsPath& path, REAL x, REAL y, REAL w, REAL h, REAL radius) {
    const REAL r = std::min(radius, std::min(w, h) / 2.0f);
    const REAL d = r * 2.0f;
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

}

void LoadConfig() {
    const std::wstring path = ConfigPath().wstring();

    g_config.triggerKey = GetPrivateProfileIntW(L"Settings", L"TriggerKey", g_config.triggerKey, path.c_str());
    g_config.rectKey = GetPrivateProfileIntW(L"Settings", L"RectKey", g_config.rectKey, path.c_str());
    g_config.lineColor = static_cast<COLORREF>(GetPrivateProfileIntW(L"Settings", L"LineColor", g_config.lineColor, path.c_str()));
    g_config.lineWidth = static_cast<int>(GetPrivateProfileIntW(L"Settings", L"LineWidth", g_config.lineWidth, path.c_str()));
    g_config.arrowColor = static_cast<COLORREF>(GetPrivateProfileIntW(L"Settings", L"ArrowColor", g_config.arrowColor, path.c_str()));
    g_config.arrowWidth = static_cast<int>(GetPrivateProfileIntW(L"Settings", L"ArrowWidth", g_config.arrowWidth, path.c_str()));
    g_config.rectColor = static_cast<COLORREF>(GetPrivateProfileIntW(L"Settings", L"RectColor", g_config.rectColor, path.c_str()));
    g_config.rectWidth = static_cast<int>(GetPrivateProfileIntW(L"Settings", L"RectWidth", g_config.rectWidth, path.c_str()));
    g_config.badgeColor = static_cast<COLORREF>(GetPrivateProfileIntW(L"Settings", L"BadgeColor", g_config.badgeColor, path.c_str()));
    g_config.resetZoomOnRelease = GetPrivateProfileIntW(L"Settings", L"ResetZoomOnRelease", g_config.resetZoomOnRelease ? 1 : 0, path.c_str()) != 0;
    g_config.keepDrawingsOnRelease = GetPrivateProfileIntW(L"Settings", L"KeepDrawingsOnRelease", g_config.keepDrawingsOnRelease ? 1 : 0, path.c_str()) != 0;
    g_config.hideToastsFromCapture = GetPrivateProfileIntW(L"Settings", L"HideToastsFromCapture", g_config.hideToastsFromCapture ? 1 : 0, path.c_str()) != 0;
    g_config.isFirstRun = GetPrivateProfileIntW(L"Settings", L"FirstRun", 1, path.c_str()) != 0;
}

void SaveConfig() {
    const std::wstring path = ConfigPath().wstring();

    auto writeInt = [&path](const wchar_t* key, LONG value) {
        WritePrivateProfileStringW(L"Settings", key, std::to_wstring(value).c_str(), path.c_str());
    };
    auto writeBool = [&path](const wchar_t* key, bool value) {
        WritePrivateProfileStringW(L"Settings", key, value ? L"1" : L"0", path.c_str());
    };

    writeInt(L"TriggerKey", g_config.triggerKey);
    writeInt(L"RectKey", g_config.rectKey);
    writeInt(L"LineColor", g_config.lineColor);
    writeInt(L"LineWidth", g_config.lineWidth);
    writeInt(L"ArrowColor", g_config.arrowColor);
    writeInt(L"ArrowWidth", g_config.arrowWidth);
    writeInt(L"RectColor", g_config.rectColor);
    writeInt(L"RectWidth", g_config.rectWidth);
    writeInt(L"BadgeColor", g_config.badgeColor);
    writeBool(L"ResetZoomOnRelease", g_config.resetZoomOnRelease);
    writeBool(L"KeepDrawingsOnRelease", g_config.keepDrawingsOnRelease);
    writeBool(L"HideToastsFromCapture", g_config.hideToastsFromCapture);
    writeBool(L"FirstRun", g_config.isFirstRun);
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

void PresentOverlayFrame() {
    if (!g_hwndOverlay) return;
    if (!g_memDC || !g_dibBits || g_backWidth <= 0 || g_backHeight <= 0) {
        CreateOverlayBackbuffer();
        if (!g_memDC || !g_dibBits) return;
    }

    memset(g_dibBits, 0, static_cast<size_t>(g_backStride) * static_cast<size_t>(g_backHeight));

    POINT curPt = s_cursorPos;
    POINT pt;
    if (GetCursorPos(&pt)) {
        s_cursorPos = pt;
        curPt = pt;
    }

    {
        Bitmap surface(g_backWidth, g_backHeight, g_backStride, PixelFormat32bppPARGB, static_cast<BYTE*>(g_dibBits));
        Graphics g(&surface);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

        if (g_app.spotlightMode) {
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

        if (g_app.breakTimer.active) {
            DrawBreakTimerUI(g, g_backWidth, g_backHeight);
        }
        else {
            int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
            int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);

            if (g_app.boardMode == BoardMode::White) {
                SolidBrush boardBrush(Color(255, 255, 255, 255));
                g.FillRectangle(&boardBrush, 0, 0, g_backWidth, g_backHeight);
            }
            else if (g_app.boardMode == BoardMode::Dark) {
                SolidBrush boardBrush(Color(255, 24, 24, 27));
                g.FillRectangle(&boardBrush, 0, 0, g_backWidth, g_backHeight);
            }

            for (const auto& s : g_app.strokes) DrawStroke(g, s, vScreenX, vScreenY);
            if (!g_app.currentStroke.points.empty()) DrawStroke(g, g_app.currentStroke, vScreenX, vScreenY);
            if (g_app.textInputActive && !g_app.textDraft.points.empty()) {
                Stroke caretDraft = g_app.textDraft;
                caretDraft.text = g_app.textDraft.text + (((GetTickCount64() / 500) % 2) ? L" " : L"|");
                DrawStroke(g, caretDraft, vScreenX, vScreenY);
            }

            if (g_app.cropMode) {
                SolidBrush dimCrop(Color(140, 0, 0, 0));
                if (g_app.cropDragging) {
                    int l = std::min(g_app.cropStart.x, g_app.cropEnd.x) - vScreenX;
                    int t = std::min(g_app.cropStart.y, g_app.cropEnd.y) - vScreenY;
                    int r = std::max(g_app.cropStart.x, g_app.cropEnd.x) - vScreenX;
                    int b = std::max(g_app.cropStart.y, g_app.cropEnd.y) - vScreenY;
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

        DrawKeycastUI(g, ToastAnchor(), TOAST_W);
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

void RedrawOverlay() {
    s_framePending = true;
    if (g_hwndOverlay) SetTimer(g_hwndOverlay, 3, 14, NULL);
}

void ProcessOverlayFrame() {
    PruneVanishingStrokes();

    bool bakedBlur = false;
    for (auto& s : g_app.strokes) {
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
    if (GetCursorPos(&pt)) {
        if (pt.x != s_cursorPos.x || pt.y != s_cursorPos.y) {
            s_cursorPos = pt;
            if (g_app.spotlightMode) s_framePending = true;
        }
    }

    static bool s_prevSpotlight = false;
    if (g_app.spotlightMode != s_prevSpotlight) {
        s_prevSpotlight = g_app.spotlightMode;
        s_framePending = true;
    }

    UpdateKeycastState(s_framePending);

    bool isDrawingStroke = g_app.isDrawingLine || g_app.isDrawingArrow || g_app.isDrawRectangle ||
                           g_app.isDrawingHighlight || g_app.isDrawingBlur || !g_app.currentStroke.points.empty();
    if (isDrawingStroke) {
        s_framePending = true;
    }

    static bool s_prevTextInputActive = false;
    if (g_app.textInputActive != s_prevTextInputActive) {
        s_prevTextInputActive = g_app.textInputActive;
        if (g_hwndOverlay) {
            if (g_app.textInputActive) SetTimer(g_hwndOverlay, 4, 500, NULL);
            else KillTimer(g_hwndOverlay, 4);
        }
        s_framePending = true;
    }

    bool animating = IsKeycastActive();
    if (!animating) {
        for (const auto& s : g_app.strokes) {
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

    bool shouldBeVisible = g_app.breakTimer.active ||
                           g_app.isTriggerHeld ||
                           g_app.spotlightMode ||
                           g_app.textInputActive ||
                           g_app.keycastText ||
                           g_app.cropMode ||
                           (g_app.boardMode != BoardMode::None) ||
                           g_app.persistentDrawingsActive ||
                           !g_app.strokes.empty() ||
                           !g_app.currentStroke.points.empty() ||
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

    SetWindowPos(g_hwndOverlay, HWND_TOPMOST, vScreenX, vScreenY, screenW, screenH, SWP_NOACTIVATE);
    CreateOverlayBackbuffer();
    PresentOverlayFrame();
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
        AddRoundedRect(cardPath, 0.0f, 0.0f, static_cast<REAL>(w), static_cast<REAL>(h), 12.0f);
        LinearGradientBrush bgCard(RectF(0.0f, 0.0f, static_cast<REAL>(w), static_cast<REAL>(h)), Color(242, 34, 36, 47), Color(242, 15, 16, 23), LinearGradientModeVertical);
        g.FillPath(&bgCard, &cardPath);

        GraphicsPath borderPath;
        AddRoundedRect(borderPath, 0.5f, 0.5f, static_cast<REAL>(w) - 1.0f, static_cast<REAL>(h) - 1.0f, 12.0f);
        Pen borderPen(Color(190, 120, 122, 148), 1.0f);
        g.DrawPath(&borderPen, &borderPath);

        const Color accentCol(255, GetRValue(s_toastAccent), GetGValue(s_toastAccent), GetBValue(s_toastAccent));

        GraphicsPath glowPath;
        AddRoundedRect(glowPath, 6.0f, 6.0f, 16.0f, static_cast<REAL>(h) - 12.0f, 8.0f);
        SolidBrush glowBrush(Color(45, accentCol.GetR(), accentCol.GetG(), accentCol.GetB()));
        g.FillPath(&glowBrush, &glowPath);

        GraphicsPath barPath;
        AddRoundedRect(barPath, 11.0f, 11.0f, 4.0f, static_cast<REAL>(h) - 22.0f, 2.0f);
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
    case WM_TIMER:
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
    case WM_DESTROY:
        MagSetFullscreenTransform(1.0f, 0, 0);
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
        mis->itemWidth = 140;
        mis->itemHeight = 32;
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
        StringFormat format;
        format.SetAlignment(StringAlignmentNear);
        format.SetLineAlignment(StringAlignmentCenter);
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
            if (g_app.breakTimer.active && !g_app.breakTimer.paused && !g_app.breakTimer.editing) {
                if (g_app.breakTimer.remainingSec > 0) {
                    g_app.breakTimer.remainingSec--;
                    RedrawOverlay();
                    if (g_app.breakTimer.remainingSec == 0) {
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