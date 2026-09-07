#include "LensIt.h"

bool IsKeyMatching(DWORD vkCode, DWORD targetKey) {
    if (vkCode == targetKey) return true;
    if ((targetKey == VK_SHIFT || targetKey == VK_LSHIFT || targetKey == VK_RSHIFT) &&
        (vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT)) return true;
    if ((targetKey == VK_CONTROL || targetKey == VK_LCONTROL || targetKey == VK_RCONTROL) &&
        (vkCode == VK_CONTROL || vkCode == VK_LCONTROL || vkCode == VK_RCONTROL)) return true;
    if ((targetKey == VK_MENU || targetKey == VK_LMENU || targetKey == VK_RMENU) &&
        (vkCode == VK_MENU || vkCode == VK_LMENU || vkCode == VK_RMENU)) return true;
    return false;
}

bool IsRectKeyPressed() {
    if (g_config.rectKey == VK_SHIFT || g_config.rectKey == VK_LSHIFT || g_config.rectKey == VK_RSHIFT) {
        return (GetAsyncKeyState(VK_SHIFT) < 0);
    }
    if (g_config.rectKey == VK_CONTROL || g_config.rectKey == VK_LCONTROL || g_config.rectKey == VK_RCONTROL) {
        return (GetAsyncKeyState(VK_CONTROL) < 0);
    }
    if (g_config.rectKey == VK_MENU || g_config.rectKey == VK_LMENU || g_config.rectKey == VK_RMENU) {
        return (GetAsyncKeyState(VK_MENU) < 0);
    }
    return (GetAsyncKeyState(g_config.rectKey) < 0);
}

std::wstring GetKeyNameStr(DWORD vkCode) {
    if (vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT) return L"Shift";
    if (vkCode == VK_CONTROL || vkCode == VK_LCONTROL || vkCode == VK_RCONTROL) return L"Ctrl";
    if (vkCode == VK_MENU || vkCode == VK_LMENU || vkCode == VK_RMENU) return L"Alt";
    if (vkCode == VK_XBUTTON1) return L"Mouse 4 (X1)";
    if (vkCode == VK_XBUTTON2) return L"Mouse 5 (X2)";
    if (vkCode == VK_MBUTTON) return L"Middle Mouse";
    if (vkCode == VK_LBUTTON) return L"Left Mouse";
    if (vkCode == VK_RBUTTON) return L"Right Mouse";

    UINT scanCode = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
    switch (vkCode) {
    case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
    case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
    case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
        scanCode |= KF_EXTENDED; break;
    }
    wchar_t name[128] = { 0 };
    GetKeyNameTextW(scanCode << 16, name, 128);
    return (wcslen(name) == 0) ? (L"Key " + std::to_wstring(vkCode)) : name;
}

void ResetDrawingState() {
    g_isTriggerHeld = false;
    g_isDrawingLine = false;
    g_isDrawingArrow = false;
    g_isDrawRectangle = false;
    g_isDrawingHighlight = false;
    g_isDrawingBlur = false;
    g_persistentDrawingsActive = false;
    g_strokes.clear();
    g_currentStroke.points.clear();
    g_currentStroke.cachedBitmap = nullptr;
    g_stepCounter = 1;
    RedrawOverlay();
}

static COLORREF InkColorForNewStroke() {
    return g_inkOverrideSet ? g_inkOverride : 0;
}

static void HandleTriggerRelease() {
    g_isTriggerHeld = false;
    g_isDrawingLine = false;
    g_isDrawingArrow = false;
    g_isDrawRectangle = false;
    g_isDrawingHighlight = false;
    g_isDrawingBlur = false;

    if (g_config.keepDrawingsOnRelease && !g_strokes.empty()) {
        g_persistentDrawingsActive = true;
        if (g_config.resetZoomOnRelease) {
            g_targetZoom = 1.0f;
            g_currentZoom = 1.0f;
            UpdateCamera();
        }
        RedrawOverlay();
    }
    else {
        ResetDrawingState();
        if (g_config.resetZoomOnRelease) {
            g_targetZoom = 1.0f;
            g_currentZoom = 1.0f;
            UpdateCamera();
        }
    }
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

        if (g_bindingMode != BindingMode::None && isDown) {
            if (p->vkCode != VK_ESCAPE) {
                if (g_bindingMode == BindingMode::TriggerKey) {
                    g_config.triggerKey = p->vkCode;
                }
                else if (g_bindingMode == BindingMode::RectKey) {
                    g_config.rectKey = p->vkCode;
                }
            }
            g_bindingMode = BindingMode::None;
            SaveConfig();
            if (g_hwndSettings) InvalidateRect(g_hwndSettings, NULL, FALSE);
            return 1;
        }

        if (IsKeyMatching(p->vkCode, g_config.triggerKey)) {
            if (isDown) {
                g_isTriggerHeld = true;
            }
            else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                HandleTriggerRelease();
            }
        }
        else if (g_isTriggerHeld && isDown) {
            if (p->vkCode == VK_ESCAPE) {
                if (g_persistentDrawingsActive) {
                    return 1;
                }
                g_targetZoom = 1.0f;
                g_currentZoom = 1.0f;
                g_inkOverrideSet = false;
                ResetDrawingState();
                UpdateCamera();
                ShowNotification(L"Reset", L"Zoom and drawings cleared", RGB(220, 70, 70));
                return 1;
            }
            if (p->vkCode == 'P') {
                g_config.keepDrawingsOnRelease = !g_config.keepDrawingsOnRelease;
                SaveConfig();
                if (g_hwndSettings) InvalidateRect(g_hwndSettings, NULL, FALSE);

                if (g_config.keepDrawingsOnRelease) {
                    g_persistentDrawingsActive = true;
                    ShowNotification(L"Pin Mode", L"Drawings pinned on screen", RGB(46, 204, 113));
                }
                else {
                    ResetDrawingState();
                    ShowNotification(L"Pin Mode", L"Drawings unpinned & cleared", RGB(235, 60, 60));
                }
                return 1;
            }
            if (p->vkCode == 'Z') {
                UndoLastStroke();
                ShowNotification(L"Undo", L"Last drawing undone", RGB(120, 160, 255));
                return 1;
            }
            if (p->vkCode == 'C') {
                CopyScreenshotToClipboard();
                ShowNotification(L"Screenshot", L"Copied to clipboard!", RGB(46, 204, 113));
                return 1;
            }
            if (p->vkCode == 'H') {
                if (g_activeToolMode == ActiveToolMode::Highlight) {
                    g_activeToolMode = ActiveToolMode::None;
                    ShowNotification(L"Highlighter", L"Mode disabled", RGB(180, 180, 180));
                }
                else {
                    g_activeToolMode = ActiveToolMode::Highlight;
                    ShowNotification(L"Highlighter", L"Mode enabled", RGB(250, 205, 40));
                }
                return 1;
            }
            if (p->vkCode == 'O') {
                if (g_activeToolMode == ActiveToolMode::Blur) {
                    g_activeToolMode = ActiveToolMode::None;
                    ShowNotification(L"Blackout Blur", L"Mode disabled", RGB(180, 180, 180));
                }
                else {
                    g_activeToolMode = ActiveToolMode::Blur;
                    ShowNotification(L"Blackout Blur", L"Mode enabled", RGB(140, 140, 255));
                }
                return 1;
            }

            COLORREF ink = 0;
            std::wstring colorName;
            if (p->vkCode == 'R') { ink = RGB(235, 60, 60); colorName = L"Red"; }
            else if (p->vkCode == 'G') { ink = RGB(60, 190, 90); colorName = L"Green"; }
            else if (p->vkCode == 'B') { ink = RGB(70, 130, 250); colorName = L"Blue"; }
            else if (p->vkCode == 'Y') { ink = RGB(250, 205, 40); colorName = L"Yellow"; }

            if (ink) {
                g_inkOverride = ink;
                g_inkOverrideSet = true;
                if (!g_currentStroke.points.empty()) g_currentStroke.color = ink;
                RedrawOverlay();
                ShowNotification(L"Color Switched", colorName, ink);
                return 1;
            }
        }
        else if (p->vkCode == VK_ESCAPE && isDown) {
            if (!g_persistentDrawingsActive) {
                g_targetZoom = 1.0f;
                g_currentZoom = 1.0f;
                g_inkOverrideSet = false;
                ResetDrawingState();
                UpdateCamera();
            }
        }
    }
    return CallNextHookEx(g_kbdHook, nCode, wParam, lParam);
}

extern std::shared_ptr<Bitmap> BakeBlurredBitmap(RECT rc);

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;

        if (g_bindingMode != BindingMode::None) {
            DWORD pressedVk = 0;
            if (wParam == WM_XBUTTONDOWN || wParam == WM_NCXBUTTONDOWN) {
                WORD xbtn = HIWORD(pMouse->mouseData);
                pressedVk = (xbtn == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
            }
            else if (wParam == WM_MBUTTONDOWN) {
                pressedVk = VK_MBUTTON;
            }

            if (pressedVk != 0) {
                if (g_bindingMode == BindingMode::TriggerKey) {
                    g_config.triggerKey = pressedVk;
                }
                else if (g_bindingMode == BindingMode::RectKey) {
                    g_config.rectKey = pressedVk;
                }
                g_bindingMode = BindingMode::None;
                SaveConfig();
                if (g_hwndSettings) InvalidateRect(g_hwndSettings, NULL, FALSE);
                return 1;
            }
        }

        if (g_config.triggerKey == VK_XBUTTON1 || g_config.triggerKey == VK_XBUTTON2 || g_config.triggerKey == VK_MBUTTON) {
            bool isDown = false;
            bool isUp = false;

            if (g_config.triggerKey == VK_MBUTTON) {
                if (wParam == WM_MBUTTONDOWN) isDown = true;
                if (wParam == WM_MBUTTONUP) isUp = true;
            }
            else {
                WORD xbtn = HIWORD(pMouse->mouseData);
                DWORD targetX = (xbtn == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
                if (targetX == g_config.triggerKey) {
                    if (wParam == WM_XBUTTONDOWN || wParam == WM_NCXBUTTONDOWN) isDown = true;
                    if (wParam == WM_XBUTTONUP || wParam == WM_NCXBUTTONUP) isUp = true;
                }
            }

            if (isDown) {
                g_isTriggerHeld = true;
                return 1;
            }
            else if (isUp) {
                HandleTriggerRelease();
                return 1;
            }
        }

        if (g_isTriggerHeld) {
            if (wParam == WM_MOUSEWHEEL) {
                short delta = GET_WHEEL_DELTA_WPARAM(pMouse->mouseData);
                g_targetZoom += (delta > 0) ? 0.25f : -0.25f;
                if (g_targetZoom > 5.0f) g_targetZoom = 5.0f;
                if (g_targetZoom < 1.0f) g_targetZoom = 1.0f;
                StartZoomTimer();
                return 1;
            }

            if (wParam == WM_MBUTTONDOWN && g_config.triggerKey != VK_MBUTTON) {
                Stroke badge;
                badge.type = StrokeType::Badge;
                badge.points = { pMouse->pt };
                badge.badgeNumber = g_stepCounter++;
                g_strokes.push_back(badge);
                RedrawOverlay();
                return 1;
            }

            if (wParam == WM_LBUTTONDOWN) {
                g_currentStroke = Stroke();
                g_currentStroke.color = InkColorForNewStroke();
                if (g_activeToolMode == ActiveToolMode::Highlight) {
                    g_currentStroke.type = StrokeType::Highlight;
                    g_isDrawingHighlight = true;
                }
                else if (g_activeToolMode == ActiveToolMode::Blur) {
                    g_currentStroke.type = StrokeType::Blur;
                    g_isDrawingBlur = true;
                }
                else if (IsRectKeyPressed()) {
                    g_currentStroke.type = StrokeType::Rectangle;
                    g_isDrawRectangle = true;
                }
                else {
                    g_currentStroke.type = StrokeType::Line;
                    g_isDrawingLine = true;
                }
                g_currentStroke.points.push_back(pMouse->pt);
                StartZoomTimer();
                return 1;
            }
            else if (wParam == WM_RBUTTONDOWN) {
                g_currentStroke = Stroke();
                g_currentStroke.type = StrokeType::Arrow;
                g_currentStroke.color = InkColorForNewStroke();
                g_currentStroke.points.push_back(pMouse->pt);
                g_isDrawingArrow = true;
                StartZoomTimer();
                return 1;
            }

            if (wParam == WM_MOUSEMOVE &&
                (g_isDrawingLine || g_isDrawingArrow || g_isDrawRectangle || g_isDrawingHighlight || g_isDrawingBlur)) {
                if (!g_currentStroke.points.empty()) {
                    POINT last = g_currentStroke.points.back();
                    int dx = pMouse->pt.x - last.x;
                    int dy = pMouse->pt.y - last.y;
                    if (dx * dx + dy * dy < 9) {
                        return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
                    }
                }
                g_currentStroke.points.push_back(pMouse->pt);
                RedrawOverlay();
            }

            bool lmbRelease = (wParam == WM_LBUTTONUP) &&
                (g_isDrawingLine || g_isDrawRectangle || g_isDrawingHighlight || g_isDrawingBlur);
            if (lmbRelease || (wParam == WM_RBUTTONUP && g_isDrawingArrow)) {
                bool wasLine = (wParam == WM_LBUTTONUP && g_isDrawingLine);
                bool wasRect = (wParam == WM_LBUTTONUP && g_isDrawRectangle);
                bool wasBlur = (wParam == WM_LBUTTONUP && g_isDrawingBlur);

                g_isDrawingLine = g_isDrawRectangle = g_isDrawingArrow = g_isDrawingHighlight = g_isDrawingBlur = false;

                if (g_currentStroke.points.size() > 1) {
                    if (wasBlur) {
                        POINT a = g_currentStroke.points.front();
                        POINT b = g_currentStroke.points.back();
                        RECT rc = { min(a.x, b.x), min(a.y, b.y), max(a.x, b.x), max(a.y, b.y) };
                        if ((rc.right - rc.left) >= 8 && (rc.bottom - rc.top) >= 8) {
                            g_currentStroke.cachedRect = rc;
                            g_currentStroke.cachedBitmap = BakeBlurredBitmap(rc);
                            g_strokes.push_back(g_currentStroke);
                        }
                    }
                    else {
                        g_strokes.push_back(g_currentStroke);
                    }
                }
                g_currentStroke.points.clear();
                g_currentStroke.cachedBitmap = nullptr;
                RedrawOverlay();
                if ((wasLine || wasRect) && g_currentZoom <= 1.0f) UpdateCamera();
                return 1;
            }
        }
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}