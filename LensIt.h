#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "resource.h"
#include <magnification.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <vector>
#include <string>
#include <cmath>

#pragma comment(lib, "magnification.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")

using namespace Gdiplus;

#define WM_APP_TRAYMSG        (WM_APP + 1)
#define ID_TRAY_SETTINGS      2001
#define ID_TRAY_EXIT          2002

enum class StrokeType { Line, Arrow, Rectangle };
enum class BindingMode { None, TriggerKey, RectKey };

struct Stroke {
    StrokeType type;
    std::vector<POINT> points;
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
    bool resetZoomOnRelease = false;
};

extern AppConfig g_config;
extern HWND g_hwndOverlay;
extern HWND g_hwndSettings;
extern HHOOK g_kbdHook;
extern HHOOK g_mouseHook;
extern bool g_isTriggerHeld;
extern bool g_isDrawingLine;
extern bool g_isDrawingArrow;
extern bool g_isDrawRectangle;
extern BindingMode g_bindingMode;
extern float g_currentZoom;
extern float g_camX;
extern float g_camY;
extern std::vector<Stroke> g_strokes;
extern Stroke g_currentStroke;
extern HICON g_appIcon;

LRESULT CALLBACK OverlayWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK SettingsWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LowLevelKeyboardProc(int, WPARAM, LPARAM);
LRESULT CALLBACK LowLevelMouseProc(int, WPARAM, LPARAM);

void UpdateCamera();
void RepositionOverlay();
void ShowSettingsWindow(HINSTANCE);
void InitTray(HWND, HINSTANCE);
std::wstring GetKeyNameStr(DWORD vkCode);
bool IsKeyMatching(DWORD vkCode, DWORD targetKey);
bool IsRectKeyPressed();

void LoadConfig();
void SaveConfig();