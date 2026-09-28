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

static bool s_xbutton1Physical = false;
static bool s_xbutton2Physical = false;
static bool s_middlePhysical = false;
static bool s_swallowMiddleUp = false;
static bool s_swallowXButton1Up = false;
static bool s_swallowXButton2Up = false;

static bool IsTriggerPhysicallyPressed() {
    if (g_config.triggerKey == VK_XBUTTON1) return s_xbutton1Physical;
    if (g_config.triggerKey == VK_XBUTTON2) return s_xbutton2Physical;
    if (g_config.triggerKey == VK_MBUTTON)  return s_middlePhysical;

    if (g_config.triggerKey == VK_MENU || g_config.triggerKey == VK_LMENU || g_config.triggerKey == VK_RMENU) {
        return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    }
    if (g_config.triggerKey == VK_SHIFT || g_config.triggerKey == VK_LSHIFT || g_config.triggerKey == VK_RSHIFT) {
        return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    }
    if (g_config.triggerKey == VK_CONTROL || g_config.triggerKey == VK_LCONTROL || g_config.triggerKey == VK_RCONTROL) {
        return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    }
    return (GetAsyncKeyState(g_config.triggerKey) & 0x8000) != 0;
}

static bool IsModifierKey(DWORD vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
        vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
        vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
        vk == VK_LWIN || vk == VK_RWIN;
}

static bool IsTextInputBlocker(DWORD vk) {
    return vk == VK_TAB || vk == VK_INSERT || vk == VK_DELETE ||
        vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT ||
        vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT;
}

void CommitTextInput() {
    if (!g_isTextInputActive) return;
    g_isTextInputActive = false;
    if (!g_textDraft.text.empty()) {
        g_textDraft.pinned = true;
        g_strokes.push_back(g_textDraft);
        RedrawOverlay();
    }
    g_textDraft.text.clear();
    g_textDraft.cachedBitmap = nullptr;
    StartZoomTimer();
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

    if (g_isTextInputActive) {
        return;
    }

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
        bool hasVanishing = false;
        bool hasPinnedText = false;
        for (const auto& s : g_strokes) {
            if (s.birthTick != 0) hasVanishing = true;
            if (s.pinned) hasPinnedText = true;
        }

        if (g_boardMode != BoardMode::None) {
            g_currentStroke.points.clear();
            g_currentStroke.cachedBitmap = nullptr;
            if (g_config.resetZoomOnRelease) {
                g_targetZoom = 1.0f;
                g_currentZoom = 1.0f;
                UpdateCamera();
            }
            RedrawOverlay();
        }
        else if (hasVanishing || hasPinnedText) {
            g_strokes.erase(
                std::remove_if(g_strokes.begin(), g_strokes.end(),
                    [](const Stroke& s) { return !s.pinned && s.birthTick == 0; }),
                g_strokes.end());
            g_currentStroke.points.clear();
            g_currentStroke.cachedBitmap = nullptr;
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
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

        if (p->vkCode == VK_TAB) {
            if (g_isTriggerHeld) {
                HandleTriggerRelease();
            }
        }
        else if (p->vkCode == VK_LWIN || p->vkCode == VK_RWIN) {
            if (g_isTriggerHeld && !IsTriggerPhysicallyPressed()) {
                HandleTriggerRelease();
            }
        }

        if (!isDown && !IsKeyMatching(p->vkCode, g_config.triggerKey)) {
            if (IsModifierKey(p->vkCode)) {
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }
            bool blockAllKeyups = g_isTextInputActive || g_bindingMode != BindingMode::None || g_isBreakTimerEditing;
            bool consumedByTimer = g_isBreakTimerActive && !g_isBreakTimerEditing &&
                (p->vkCode == VK_ESCAPE || p->vkCode == VK_SPACE ||
                    p->vkCode == VK_UP || p->vkCode == VK_DOWN ||
                    p->vkCode == VK_LEFT || p->vkCode == VK_RIGHT);
            if (blockAllKeyups || consumedByTimer) {
                return 1;
            }
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        if (g_keycastEnabled && isDown && !g_isBreakTimerActive && !g_isTextInputActive && g_bindingMode == BindingMode::None) {
            bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool win = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;

            if (ctrl || alt || win) {
                bool isModKey = (p->vkCode == VK_CONTROL || p->vkCode == VK_LCONTROL || p->vkCode == VK_RCONTROL ||
                    p->vkCode == VK_MENU || p->vkCode == VK_LMENU || p->vkCode == VK_RMENU ||
                    p->vkCode == VK_SHIFT || p->vkCode == VK_LSHIFT || p->vkCode == VK_RSHIFT ||
                    p->vkCode == VK_LWIN || p->vkCode == VK_RWIN);

                std::vector<std::wstring> parts;
                if (win) parts.push_back(L"Win");
                if (ctrl) parts.push_back(L"Ctrl");
                if (alt) parts.push_back(L"Alt");
                if (shift) parts.push_back(L"Shift");

                if (!isModKey) {
                    UINT scan = MapVirtualKeyW(p->vkCode, MAPVK_VK_TO_VSC);
                    switch (p->vkCode) {
                    case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
                    case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
                    case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
                        scan |= KF_EXTENDED; break;
                    }
                    wchar_t keyName[32] = { 0 };
                    GetKeyNameTextW(static_cast<LONG>(scan << 16), keyName, 32);
                    if (keyName[0]) parts.push_back(keyName);
                    else if (p->vkCode >= '0' && p->vkCode <= '9') parts.push_back(std::wstring(1, static_cast<wchar_t>(p->vkCode)));
                    else if (p->vkCode >= 'A' && p->vkCode <= 'Z') parts.push_back(std::wstring(1, static_cast<wchar_t>(p->vkCode)));
                    else parts.push_back(L"Key " + std::to_wstring(p->vkCode));
                }

                if (!isModKey && parts.size() >= 2) {
                    std::wstring combo;
                    for (size_t i = 0; i < parts.size(); ++i) {
                        if (i) combo += L" + ";
                        combo += parts[i];
                    }
                    g_keycastTextValue = combo;
                    g_keycastText = true;
                    g_keycastUntilTick = GetTickCount64() + 1500;
                    RedrawOverlay();
                }
            }
        }

        if (g_isBreakTimerActive) {
            if (g_isBreakTimerEditing) {
                if (!isDown) return 1;
                if (p->vkCode == VK_ESCAPE) {
                    g_isBreakTimerEditing = false;
                    g_breakTimerInputStr.clear();
                    RedrawOverlay();
                    return 1;
                }
                if (p->vkCode == VK_RETURN) {
                    CommitBreakTimerInput();
                    return 1;
                }
                if (p->vkCode == VK_BACK) {
                    if (!g_breakTimerInputStr.empty()) {
                        g_breakTimerInputStr.pop_back();
                        RedrawOverlay();
                    }
                    return 1;
                }

                wchar_t ch = 0;
                if (p->vkCode >= VK_NUMPAD0 && p->vkCode <= VK_NUMPAD9) {
                    ch = static_cast<wchar_t>(L'0' + (p->vkCode - VK_NUMPAD0));
                }
                else {
                    BYTE kbState[256] = { 0 };
                    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) kbState[VK_SHIFT] = 0x80;

                    HWND hFore = GetForegroundWindow();
                    DWORD foreThread = hFore ? GetWindowThreadProcessId(hFore, NULL) : 0;
                    HKL hkl = foreThread ? GetKeyboardLayout(foreThread) : GetKeyboardLayout(0);

                    wchar_t out[8] = { 0 };
                    int produced = ToUnicodeEx(p->vkCode, MapVirtualKeyW(p->vkCode, MAPVK_VK_TO_VSC), kbState, out, 8, 0, hkl);
                    if (produced == 1) {
                        wchar_t c = out[0];
                        if (c >= L'0' && c <= L'9') ch = c;
                        else if (c == L':' || c == L'.' || c == L',' || c == L';' || c == L' ') ch = L':';
                    }
                }

                if (ch != 0 && g_breakTimerInputStr.length() < 6) {
                    g_breakTimerInputStr.push_back(ch);
                    RedrawOverlay();
                    return 1;
                }
                return 1;
            }

            if (!isDown) {
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }

            if (p->vkCode == VK_ESCAPE) {
                StopBreakTimer();
                ShowNotification(L"Break Timer", L"Timer cancelled", RGB(220, 70, 70));
                return 1;
            }
            if (p->vkCode == VK_SPACE) {
                g_isBreakTimerPaused = !g_isBreakTimerPaused;
                RedrawOverlay();
                return 1;
            }

            bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) < 0);
            int step = shiftPressed ? 5 : 60;

            if (p->vkCode == VK_UP || p->vkCode == VK_RIGHT) {
                g_breakTimerRemainingSec += step;
                if (g_breakTimerRemainingSec > 5999) g_breakTimerRemainingSec = 5999;
                g_breakTimerTotalSec = std::max(g_breakTimerTotalSec, g_breakTimerRemainingSec);
                RedrawOverlay();
                return 1;
            }
            if (p->vkCode == VK_DOWN || p->vkCode == VK_LEFT) {
                g_breakTimerRemainingSec = std::max(5, g_breakTimerRemainingSec - step);
                RedrawOverlay();
                return 1;
            }
        }

        if (g_isTextInputActive && isDown && !IsKeyMatching(p->vkCode, g_config.triggerKey)) {
            if (IsModifierKey(p->vkCode)) {
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }
            if (p->vkCode == VK_RETURN) {
                CommitTextInput();
                return 1;
            }
            if (p->vkCode == VK_ESCAPE) {
                g_isTextInputActive = false;
                g_textDraft.text.clear();
                g_textDraft.cachedBitmap = nullptr;
                RedrawOverlay();
                return 1;
            }
            if (p->vkCode == VK_BACK) {
                if (!g_textDraft.text.empty()) {
                    g_textDraft.text.pop_back();
                    RedrawOverlay();
                }
                return 1;
            }
            if (IsTextInputBlocker(p->vkCode)) {
                return 1;
            }

            BYTE kbState[256] = { 0 };
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) kbState[VK_SHIFT] = 0x80;
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) kbState[VK_CONTROL] = 0x80;
            if (GetAsyncKeyState(VK_MENU) & 0x8000) kbState[VK_MENU] = 0x80;
            if (GetKeyState(VK_CAPITAL) & 0x0001) kbState[VK_CAPITAL] = 0x01;

            HWND hFore = GetForegroundWindow();
            DWORD foreThread = hFore ? GetWindowThreadProcessId(hFore, NULL) : 0;
            HKL hkl = foreThread ? GetKeyboardLayout(foreThread) : GetKeyboardLayout(0);

            wchar_t out[8] = { 0 };
            int produced = ToUnicodeEx(p->vkCode, MapVirtualKeyW(p->vkCode, MAPVK_VK_TO_VSC), kbState, out, 8, 0, hkl);
            if (produced >= 1 && out[0] >= 0x20 && g_textDraft.text.length() < 256) {
                g_textDraft.text.push_back(out[0]);
                RedrawOverlay();
            }
            return 1;
        }

        if (g_bindingMode != BindingMode::None) {
            if (!isDown) return 1;
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
                if (g_isTextInputActive) {
                    CommitTextInput();
                }
            }
            else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                HandleTriggerRelease();
            }
        }
        else if (g_isTriggerHeld && isDown) {
            if (p->vkCode == 'T') {
                ToggleBreakTimer(5);
                return 1;
            }
            if (p->vkCode == 'S') {
                g_spotlightMode = !g_spotlightMode;
                StartZoomTimer();
                return 1;
            }
            if (p->vkCode == 'V') {
                g_laserMode = !g_laserMode;
                ShowNotification(L"Laser Ink", g_laserMode ? L"Mode enabled" : L"Mode disabled", RGB(255, 90, 90));
                return 1;
            }
            if (p->vkCode == 'X') {
                g_isTextInputActive = true;
                g_textDraft = Stroke();
                g_textDraft.type = StrokeType::Text;
                g_textDraft.color = InkColorForNewStroke();
                g_textDraft.points.emplace_back();
                GetCursorPos(&g_textDraft.points[0]);
                StartZoomTimer();
                return 1;
            }
            if (p->vkCode == 'K') {
                g_keycastEnabled = !g_keycastEnabled;
                g_keycastUntilTick = 0;
                ShowNotification(L"Keystroke HUD", g_keycastEnabled ? L"Enabled" : L"Disabled", RGB(150, 150, 255));
                return 1;
            }
            if (p->vkCode == VK_ESCAPE) {
                if (g_cropMode) {
                    g_cropMode = false;
                    g_cropDragging = false;
                    SetCursor(LoadCursor(NULL, IDC_ARROW));
                    RedrawOverlay();
                    return 1;
                }
                if (g_boardMode != BoardMode::None) {
                    g_boardMode = BoardMode::None;
                    RedrawOverlay();
                    return 1;
                }
                if (g_persistentDrawingsActive) {
                    ShowNotification(L"Pin Mode", L"Drawings are pinned. Unpin with P.", RGB(250, 205, 40));
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
                if (UndoLastStroke()) {
                    ShowNotification(L"Undo", L"Last drawing undone", RGB(120, 160, 255));
                }
                return 1;
            }
            if (p->vkCode == 'C') {
                if (IsRectKeyPressed()) {
                    StartCropSelection();
                }
                else if (g_hwndOverlay) {
                    PostMessageW(g_hwndOverlay, WM_APP_TAKE_SCREENSHOT, 0, 0);
                }
                return 1;
            }
            if (p->vkCode == 'W') {
                if (g_boardMode == BoardMode::None) g_boardMode = BoardMode::White;
                else if (g_boardMode == BoardMode::White) g_boardMode = BoardMode::Dark;
                else g_boardMode = BoardMode::None;
                RedrawOverlay();
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
            if (g_cropMode) {
                g_cropMode = false;
                g_cropDragging = false;
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                RedrawOverlay();
                return 1;
            }
            if (g_boardMode != BoardMode::None) {
                g_boardMode = BoardMode::None;
                RedrawOverlay();
                return 1;
            }
            if (!g_persistentDrawingsActive) {
                bool hadActiveState = (g_currentZoom > 1.01f || !g_strokes.empty());
                g_targetZoom = 1.0f;
                g_currentZoom = 1.0f;
                g_inkOverrideSet = false;
                ResetDrawingState();
                UpdateCamera();
                if (hadActiveState) return 1;
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        MSLLHOOKSTRUCT* pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        if (wParam == WM_MOUSEMOVE) {
            static bool s_haveLastMovePt = false;
            static POINT s_lastMovePt = { 0, 0 };
            if (s_haveLastMovePt && pMouse->pt.x == s_lastMovePt.x && pMouse->pt.y == s_lastMovePt.y) {
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }
            s_haveLastMovePt = true;
            s_lastMovePt = pMouse->pt;
        }

        if (wParam == WM_XBUTTONDOWN || wParam == WM_NCXBUTTONDOWN) {
            const WORD button = HIWORD(pMouse->mouseData);
            if (button == XBUTTON1) s_xbutton1Physical = true;
            else if (button == XBUTTON2) s_xbutton2Physical = true;
        }
        else if (wParam == WM_XBUTTONUP || wParam == WM_NCXBUTTONUP) {
            const WORD button = HIWORD(pMouse->mouseData);
            if (button == XBUTTON1) s_xbutton1Physical = false;
            else if (button == XBUTTON2) s_xbutton2Physical = false;
        }
        else if (wParam == WM_MBUTTONDOWN) {
            s_middlePhysical = true;
        }
        else if (wParam == WM_MBUTTONUP) {
            s_middlePhysical = false;
        }

        if (wParam == WM_MBUTTONUP && s_swallowMiddleUp) {
            s_swallowMiddleUp = false;
            return 1;
        }
        if (wParam == WM_XBUTTONUP || wParam == WM_NCXBUTTONUP) {
            const WORD button = HIWORD(pMouse->mouseData);
            if (button == XBUTTON1 && s_swallowXButton1Up) {
                s_swallowXButton1Up = false;
                return 1;
            }
            if (button == XBUTTON2 && s_swallowXButton2Up) {
                s_swallowXButton2Up = false;
                return 1;
            }
        }

        if (g_isTriggerHeld && !IsTriggerPhysicallyPressed()) {
            HandleTriggerRelease();
        }

        if (g_spotlightMode || g_isTextInputActive) {
            if (wParam == WM_MOUSEMOVE || wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
                wParam == WM_MBUTTONDOWN || wParam == WM_LBUTTONUP || wParam == WM_RBUTTONUP) {
                StartZoomTimer();
            }
        }

        if (g_isBreakTimerActive) {
            if (wParam == WM_LBUTTONDOWN) {
                float cx = 0.0f;
                float cy = 0.0f;
                GetBreakTimerCenter(cx, cy);

                int localX = pMouse->pt.x - GetSystemMetrics(SM_XVIRTUALSCREEN);
                int localY = pMouse->pt.y - GetSystemMetrics(SM_YVIRTUALSCREEN);

                // Clicked inside clock hit-box
                if (localX >= (cx - 220) && localX <= (cx + 220) && localY >= (cy - 90) && localY <= (cy + 50)) {
                    g_isBreakTimerEditing = true;
                    g_isBreakTimerPaused = true;
                    g_breakTimerInputStr.clear();
                    RedrawOverlay();
                    return 1;
                }
                else if (g_isBreakTimerEditing) {
                    CommitBreakTimerInput();
                    return 1;
                }
            }

            if (wParam == WM_MOUSEWHEEL && !g_isBreakTimerEditing) {
                short delta = GET_WHEEL_DELTA_WPARAM(pMouse->mouseData);
                bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) < 0);
                int step = shiftPressed ? ((delta > 0) ? 5 : -5) : ((delta > 0) ? 60 : -60);

                g_breakTimerRemainingSec += step;
                if (g_breakTimerRemainingSec < 5) g_breakTimerRemainingSec = 5;
                if (g_breakTimerRemainingSec > 5999) g_breakTimerRemainingSec = 5999;
                g_breakTimerTotalSec = std::max(g_breakTimerTotalSec, g_breakTimerRemainingSec);
                RedrawOverlay();
                return 1;
            }
        }

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
                if (pressedVk == VK_MBUTTON) s_swallowMiddleUp = true;
                else if (pressedVk == VK_XBUTTON1) s_swallowXButton1Up = true;
                else if (pressedVk == VK_XBUTTON2) s_swallowXButton2Up = true;
                g_bindingMode = BindingMode::None;
                SaveConfig();
                if (g_hwndSettings) InvalidateRect(g_hwndSettings, NULL, FALSE);
                return 1;
            }
        }

        if (g_cropMode) {
            if (wParam == WM_MOUSEMOVE) {
                SetCursor(LoadCursor(NULL, IDC_CROSS));
                if (g_cropDragging) {
                    g_cropEnd = pMouse->pt;
                    RedrawOverlay();
                    return 1;
                }
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }
            if (wParam == WM_LBUTTONDOWN) {
                SetCursor(LoadCursor(NULL, IDC_CROSS));
                g_cropStart = pMouse->pt;
                g_cropEnd = pMouse->pt;
                g_cropDragging = true;
                RedrawOverlay();
                return 1;
            }
            if (wParam == WM_LBUTTONUP && g_cropDragging) {
                g_cropDragging = false;
                g_cropMode = false;
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                int l = std::min(g_cropStart.x, g_cropEnd.x);
                int t = std::min(g_cropStart.y, g_cropEnd.y);
                int r = std::max(g_cropStart.x, g_cropEnd.x);
                int b = std::max(g_cropStart.y, g_cropEnd.y);
                if (r - l >= 4 && b - t >= 4) {
                    RECT rc = { l, t, r, b };
                    CopyRegionToClipboard(rc);
                    ShowNotification(L"Crop Screenshot", L"Copied to clipboard!", RGB(46, 204, 113));
                }
                RedrawOverlay();
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
                if (g_laserMode) badge.birthTick = GetTickCount64();
                g_strokes.push_back(badge);
                s_swallowMiddleUp = true;
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
                        return CallNextHookEx(nullptr, nCode, wParam, lParam);
                    }
                }

                const bool rectKeyPressed = IsRectKeyPressed();
                POINT np = pMouse->pt;
                if ((g_isDrawingLine || g_isDrawingArrow) && !g_currentStroke.points.empty() && rectKeyPressed) {
                    POINT first = g_currentStroke.points.front();
                    float fdx = static_cast<float>(np.x - first.x);
                    float fdy = static_cast<float>(np.y - first.y);
                    float dist = sqrtf(fdx * fdx + fdy * fdy);
                    if (dist > 1.0f) {
                        float angle = atan2f(fdy, fdx);
                        float snap = roundf(angle / (3.14159265f / 4.0f)) * (3.14159265f / 4.0f);
                        np.x = first.x + static_cast<LONG>(dist * cosf(snap));
                        np.y = first.y + static_cast<LONG>(dist * sinf(snap));
                    }
                }

                const bool snapLine = rectKeyPressed && (g_isDrawingLine || g_isDrawingArrow);
                if (snapLine) {
                    if (g_currentStroke.points.size() >= 2) {
                        g_currentStroke.points.resize(2);
                        g_currentStroke.points[1] = np;
                    }
                    else {
                        g_currentStroke.points.push_back(np);
                    }
                }
                else if (g_currentStroke.points.size() > 1 &&
                         (g_isDrawRectangle || g_isDrawingBlur || g_isDrawingHighlight)) {
                    g_currentStroke.points.back() = np;
                }
                else {
                    g_currentStroke.points.push_back(np);
                }
                StartZoomTimer();
            }

            bool lmbRelease = (wParam == WM_LBUTTONUP) &&
                (g_isDrawingLine || g_isDrawRectangle || g_isDrawingHighlight || g_isDrawingBlur);
            if (lmbRelease || (wParam == WM_RBUTTONUP && g_isDrawingArrow)) {
                bool wasLine = (wParam == WM_LBUTTONUP && g_isDrawingLine);
                bool wasRect = (wParam == WM_LBUTTONUP && g_isDrawRectangle);
                bool wasBlur = (wParam == WM_LBUTTONUP && g_isDrawingBlur);

                g_isDrawingLine = g_isDrawRectangle = g_isDrawingArrow = g_isDrawingHighlight = g_isDrawingBlur = false;

                if (g_currentStroke.points.size() > 1) {
                    if (g_laserMode) g_currentStroke.birthTick = GetTickCount64();
                    if (wasBlur) {
                        POINT a = g_currentStroke.points.front();
                        POINT b = g_currentStroke.points.back();
                        RECT rc = { std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y) };
                        if ((rc.right - rc.left) >= 8 && (rc.bottom - rc.top) >= 8) {
                            g_currentStroke.cachedRect = rc;
                            g_currentStroke.cachedBitmap = nullptr;
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
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}