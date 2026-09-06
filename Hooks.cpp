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

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

        if (g_bindingMode != BindingMode::None && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
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
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                g_isTriggerHeld = true;
            }
            else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                g_isTriggerHeld = g_isDrawingLine = g_isDrawingArrow = g_isDrawRectangle = false;
                g_strokes.clear();
                g_currentStroke.points.clear();
                InvalidateRect(g_hwndOverlay, NULL, FALSE);
                if (g_config.resetZoomOnRelease) {
                    g_currentZoom = 1.0f;
                    UpdateCamera();
                }
            }
        }
        else if (p->vkCode == VK_ESCAPE && wParam == WM_KEYDOWN) {
            g_currentZoom = 1.0f;
            g_strokes.clear();
            g_currentStroke.points.clear();
            InvalidateRect(g_hwndOverlay, NULL, FALSE);
            UpdateCamera();
        }
    }
    return CallNextHookEx(g_kbdHook, nCode, wParam, lParam);
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;

        if (g_isTriggerHeld) {
            if (wParam == WM_MOUSEWHEEL) {
                short delta = GET_WHEEL_DELTA_WPARAM(pMouse->mouseData);
                g_currentZoom += (delta > 0) ? 0.25f : -0.25f;
                if (g_currentZoom > 5.0f) g_currentZoom = 5.0f;
                if (g_currentZoom < 1.0f) g_currentZoom = 1.0f;
                return 1;
            }

            if (wParam == WM_LBUTTONDOWN) {
                if (IsRectKeyPressed()) {
                    g_isDrawRectangle = true;
                    g_currentStroke = { StrokeType::Rectangle, { pMouse->pt } };
                }
                else {
                    g_isDrawingLine = true;
                    g_currentStroke = { StrokeType::Line, { pMouse->pt } };
                }
                return 1;
            }
            else if (wParam == WM_RBUTTONDOWN) {
                g_isDrawingArrow = true;
                g_currentStroke = { StrokeType::Arrow, { pMouse->pt } };
                return 1;
            }

            if (wParam == WM_MOUSEMOVE && (g_isDrawingLine || g_isDrawingArrow || g_isDrawRectangle)) {
                if (!g_currentStroke.points.empty()) {
                    POINT last = g_currentStroke.points.back();
                    int dx = pMouse->pt.x - last.x;
                    int dy = pMouse->pt.y - last.y;
                    if (dx * dx + dy * dy < 4) {
                        return 0;
                    }
                }
                g_currentStroke.points.push_back(pMouse->pt);
                InvalidateRect(g_hwndOverlay, NULL, FALSE);
            }

            if ((wParam == WM_LBUTTONUP && (g_isDrawingLine || g_isDrawRectangle)) || (wParam == WM_RBUTTONUP && g_isDrawingArrow)) {
                bool finishedLine = (wParam == WM_LBUTTONUP && g_isDrawingLine);
                bool finishedRect = (wParam == WM_LBUTTONUP && g_isDrawRectangle);
                g_isDrawingLine = g_isDrawRectangle = g_isDrawingArrow = false;
                if (g_currentStroke.points.size() > 1) {
                    g_strokes.push_back(g_currentStroke);
                }
                g_currentStroke.points.clear();
                InvalidateRect(g_hwndOverlay, NULL, FALSE);
                if ((finishedLine || finishedRect) && g_currentZoom <= 1.0f) UpdateCamera();
                return 1;
            }
        }
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}