#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define GDIPVER 0x0110

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

#ifndef MW_FILTERMODE_EXCLUDE
#define MW_FILTERMODE_EXCLUDE 0
#endif

#include <windows.h>
#include "resource.h"
#include <magnification.h>
#include <gdiplus.h>
#include <gdipluseffects.h>
#include <shellapi.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <memory>

#pragma comment(lib, "magnification.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")

using namespace Gdiplus;

#define WM_APP_TRAYMSG        (WM_APP + 1)
#define WM_APP_SHOWSETTINGS   (WM_APP + 2)
#define WM_APP_TAKE_SCREENSHOT (WM_APP + 3)
#define ID_TRAY_SETTINGS      2001
#define ID_TRAY_EXIT          2002

enum class StrokeType { Line, Arrow, Rectangle, Badge, Highlight, Blur, Text };
enum class BindingMode { None, TriggerKey, RectKey };

struct Stroke {
    StrokeType type = StrokeType::Line;
    std::vector<POINT> points;
    int badgeNumber = 0;
    COLORREF color = 0;
    std::shared_ptr<Bitmap> cachedBitmap = nullptr;
    RECT cachedRect = { 0, 0, 0, 0 };
    float opacity = 1.0f;
    ULONGLONG birthTick = 0;
    bool pinned = false;
    std::wstring text;
};

struct AppConfig {
    DWORD triggerKey = VK_LMENU;
    DWORD rectKey = VK_SHIFT;
    COLORREF lineColor = RGB(255, 45, 45);
    int lineWidth = 4;
    COLORREF arrowColor = RGB(45, 200, 255);
    int arrowWidth = 6;
    COLORREF rectColor = RGB(46, 204, 113);
    int rectWidth = 4;
    COLORREF badgeColor = RGB(241, 196, 15);
    bool resetZoomOnRelease = false;
    bool keepDrawingsOnRelease = false;
    bool hideToastsFromCapture = true;
    bool isFirstRun = true;
};

enum class ActiveToolMode { None, Highlight, Blur };
enum class BoardMode { None, White, Dark };

struct AppState {
    bool isTriggerHeld = false;
    bool isDrawingLine = false;
    bool isDrawingArrow = false;
    bool isDrawRectangle = false;
    bool isDrawingHighlight = false;
    bool isDrawingBlur = false;
    bool persistentDrawingsActive = false;
    ActiveToolMode activeToolMode = ActiveToolMode::None;
    BoardMode boardMode = BoardMode::None;
    bool laserMode = false;
    bool spotlightMode = false;
    bool keycastEnabled = true;
    bool textInputActive = false;

    COLORREF inkOverride = 0;
    bool inkOverrideSet = false;
    Stroke textDraft;

    bool cropMode = false;
    bool cropDragging = false;
    POINT cropStart = { 0, 0 };
    POINT cropEnd = { 0, 0 };

    bool keycastText = false;
    std::wstring keycastTextValue;
    ULONGLONG keycastUntilTick = 0;

    int stepCounter = 1;
    std::vector<Stroke> strokes;
    Stroke currentStroke;

    struct {
        bool active = false;
        bool paused = false;
        bool editing = false;
        int totalSec = 300;
        int remainingSec = 300;
        std::wstring inputStr;
    } breakTimer;
};

extern AppState g_app;

extern AppConfig g_config;
extern HWND g_hwndOverlay;
extern HWND g_hwndSettings;
extern HWND g_hwndToast;
extern BindingMode g_bindingMode;
extern float g_currentZoom;
extern float g_targetZoom;
extern float g_camX;
extern float g_camY;
extern HICON g_appIcon;
extern NOTIFYICONDATA g_nid;

void StartBreakTimer(int minutes = 5);
void StopBreakTimer();
void ToggleBreakTimer(int minutes = 5);
void CommitBreakTimerInput();
void GetBreakTimerCenter(float& cx, float& cy);

LRESULT CALLBACK OverlayWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK SettingsWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ToastWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LowLevelKeyboardProc(int, WPARAM, LPARAM);
LRESULT CALLBACK LowLevelMouseProc(int, WPARAM, LPARAM);

extern UINT WM_TASKBARCREATED;

void UpdateCamera();
void RepositionOverlay();
void CreateOverlayBackbuffer();
void DestroyOverlayBackbuffer();
void PresentOverlayFrame();
void CopyScreenshotToClipboard();
void CopyRegionToClipboard(RECT rcScreen);
void StartCropSelection();
bool UndoLastStroke();
void RedrawOverlay();
void ResetDrawingState();
void SyncOverlayVisibility();
void ProcessOverlayFrame();
void RepositionToast();

void ShowNotification(const std::wstring& title, const std::wstring& message, COLORREF accentColor = RGB(0, 150, 255));
void InitToastWindow(HINSTANCE hInstance);
void ApplyToastCaptureAffinity();

void ShowSettingsWindow(HINSTANCE);
void InitTray(HWND, HINSTANCE);
std::wstring GetKeyNameStr(DWORD vkCode);
bool IsKeyMatching(DWORD vkCode, DWORD targetKey);
bool IsRectKeyPressed();
void StartZoomTimer();
void StopZoomTimer();
bool RequiresZoomTimer();

void LoadConfig();
void SaveConfig();

LRESULT CALLBACK WelcomeWndProc(HWND, UINT, WPARAM, LPARAM);
void ShowWelcomeWindow(HINSTANCE);
bool SetAutoStart(bool enable);
bool IsAutoStartEnabled();
bool CreateDesktopShortcut();