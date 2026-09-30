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

static wchar_t TranslateVkToChar(DWORD vkCode) {
    BYTE kbState[256] = { 0 };
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   kbState[VK_SHIFT] = 0x80;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) kbState[VK_CONTROL] = 0x80;
    if (GetAsyncKeyState(VK_MENU) & 0x8000)    kbState[VK_MENU] = 0x80;
    if (GetKeyState(VK_CAPITAL) & 0x0001)      kbState[VK_CAPITAL] = 0x01;

    HWND hFore = GetForegroundWindow();
    DWORD threadId = hFore ? GetWindowThreadProcessId(hFore, nullptr) : 0;
    HKL layout = threadId ? GetKeyboardLayout(threadId) : GetKeyboardLayout(0);

    wchar_t out[8] = { 0 };
    int count = ToUnicodeEx(vkCode, MapVirtualKeyW(vkCode, MAPVK_VK_TO_VSC), kbState, out, 8, 0, layout);
    return (count >= 1) ? out[0] : 0;
}

void CommitTextInput() {
    if (!g_app.textInputActive) return;
    g_app.textInputActive = false;
    if (!g_app.textDraft.text.empty()) {
        g_app.textDraft.pinned = true;
        g_app.strokes.push_back(g_app.textDraft);
        RedrawOverlay();
    }
    g_app.textDraft.text.clear();
    g_app.textDraft.cachedBitmap = nullptr;
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
    g_app.isTriggerHeld = false;
    g_app.isDrawingLine = false;
    g_app.isDrawingArrow = false;
    g_app.isDrawRectangle = false;
    g_app.isDrawingHighlight = false;
    g_app.isDrawingBlur = false;
    g_app.laserMode = false;
    g_app.persistentDrawingsActive = false;
    g_app.strokes.clear();
    g_app.currentStroke.points.clear();
    g_app.currentStroke.cachedBitmap = nullptr;
    g_app.stepCounter = 1;
    RedrawOverlay();
}

static COLORREF InkColorForNewStroke() {
    return g_app.inkOverrideSet ? g_app.inkOverride : 0;
}

void EnterFreezeMode() {
    if (g_app.freezeMode) return;
    if (g_hwndOverlay) {
        ShowWindow(g_hwndOverlay, SW_HIDE);
    }
    g_app.freezeBitmap = CaptureScreenBitmap();
    g_app.freezeMode = true;
    g_app.laserMode = false;
    if (!g_app.persistentDrawingsActive) {
        g_app.strokes.clear();
        g_app.stepCounter = 1;
    }
    g_app.currentStroke.points.clear();

    if (g_hwndOverlay) {
        ShowWindow(g_hwndOverlay, SW_SHOWNOACTIVATE);
        RedrawOverlay();
    }
    SetCursor(LoadCursor(NULL, IDC_CROSS));
    ShowNotification(L"Draw Mode", L"Screen frozen. Draw freely. Press Esc to exit.", RGB(46, 204, 113));
}

void ExitFreezeMode() {
    if (!g_app.freezeMode) return;
    g_app.freezeMode = false;
    g_app.freezeBitmap = nullptr;
    g_app.boardMode = BoardMode::None;
    g_app.cropMode = false;
    g_app.cropDragging = false;
    g_app.laserMode = false;
    if (!g_app.persistentDrawingsActive) {
        ResetDrawingState();
    }
    g_targetZoom = 1.0f;
    g_currentZoom = 1.0f;
    UpdateCamera();
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    RedrawOverlay();
    ShowNotification(L"Draw Mode", L"Screen unfrozen", RGB(220, 70, 70));
}

void ToggleFreezeMode() {
    if (g_app.freezeMode) ExitFreezeMode();
    else EnterFreezeMode();
}

static void HandleTriggerRelease() {
    g_app.isTriggerHeld = false;
    g_app.isDrawingLine = false;
    g_app.isDrawingArrow = false;
    g_app.isDrawRectangle = false;
    g_app.isDrawingHighlight = false;
    g_app.isDrawingBlur = false;

    if (g_app.freezeMode || g_app.textInputActive) {
        return;
    }

    if (g_config.keepDrawingsOnRelease && !g_app.strokes.empty()) {
        g_app.persistentDrawingsActive = true;
        if (g_config.resetZoomOnRelease) {
            g_targetZoom = 1.0f;
            g_currentZoom = 1.0f;
            UpdateCamera();
        }
        RedrawOverlay();
    }
    else {
        if (g_config.resetZoomOnRelease) {
            g_targetZoom = 1.0f;
            g_currentZoom = 1.0f;
            UpdateCamera();
        }
        ResetDrawingState();
    }
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

        if (g_bindingMode != BindingMode::None) {
            if (!isDown) return 1;
            if (p->vkCode != VK_ESCAPE) {
                if (g_bindingMode == BindingMode::TriggerKey) g_config.triggerKey = p->vkCode;
                else if (g_bindingMode == BindingMode::RectKey) g_config.rectKey = p->vkCode;
                else if (g_bindingMode == BindingMode::FreezeKey) g_config.freezeKey = p->vkCode;
            }
            g_bindingMode = BindingMode::None;
            SaveConfig();
            UpdateSettingsUI();
            return 1;
        }

        if (isDown && !g_app.textInputActive) {
            bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            if (ctrlPressed && !altPressed && p->vkCode == g_config.freezeKey) {
                ToggleFreezeMode();
                return 1;
            }
        }

        if (g_app.freezeMode && isDown && !g_app.textInputActive) {
            bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

            if (p->vkCode == VK_ESCAPE) {
                ExitFreezeMode();
                return 1;
            }
            if ((ctrlPressed && p->vkCode == 'Z') || p->vkCode == 'Z') {
                if (UndoLastStroke()) {
                    ShowNotification(L"Undo", L"Last stroke removed", RGB(120, 160, 255));
                }
                return 1;
            }
            if ((ctrlPressed && p->vkCode == 'C') || p->vkCode == 'C') {
                if (IsRectKeyPressed()) {
                    StartCropSelection();
                }
                else {
                    CopyScreenshotToClipboard();
                    ShowNotification(L"Screenshot", L"Copied to clipboard!", RGB(46, 204, 113));
                }
                return 1;
            }
            if (p->vkCode == 'W') {
                if (g_app.boardMode == BoardMode::None) g_app.boardMode = BoardMode::White;
                else if (g_app.boardMode == BoardMode::White) g_app.boardMode = BoardMode::Dark;
                else g_app.boardMode = BoardMode::None;
                RedrawOverlay();
                return 1;
            }
            if (p->vkCode == 'H') {
                g_app.activeToolMode = (g_app.activeToolMode == ActiveToolMode::Highlight) ? ActiveToolMode::None : ActiveToolMode::Highlight;
                ShowNotification(L"Highlighter", g_app.activeToolMode == ActiveToolMode::Highlight ? L"Enabled" : L"Disabled", RGB(250, 205, 40));
                return 1;
            }
            if (p->vkCode == 'O') {
                g_app.activeToolMode = (g_app.activeToolMode == ActiveToolMode::Blur) ? ActiveToolMode::None : ActiveToolMode::Blur;
                ShowNotification(L"Blur Redaction", g_app.activeToolMode == ActiveToolMode::Blur ? L"Enabled" : L"Disabled", RGB(140, 140, 255));
                return 1;
            }
            if (p->vkCode == 'V') {
                g_app.laserMode = !g_app.laserMode;
                ShowNotification(L"Laser Ink", g_app.laserMode ? L"Enabled (Draw Mode)" : L"Disabled (Draw Mode)", RGB(255, 90, 90));
                return 1;
            }
            if (p->vkCode == 'X') {
                g_app.textInputActive = true;
                g_app.textDraft = Stroke();
                g_app.textDraft.type = StrokeType::Text;
                g_app.textDraft.color = InkColorForNewStroke();
                g_app.textDraft.points.emplace_back();
                GetCursorPos(&g_app.textDraft.points[0]);
                StartZoomTimer();
                return 1;
            }
            if (p->vkCode == 'P') {
                g_app.persistentDrawingsActive = !g_app.persistentDrawingsActive;
                ShowNotification(L"Pin Mode", g_app.persistentDrawingsActive ? L"Drawings pinned to screen" : L"Strokes will clear on exit", RGB(46, 204, 113));
                return 1;
            }
            if (p->vkCode == 'T') {
                ToggleBreakTimer(5);
                return 1;
            }

            COLORREF ink = 0;
            std::wstring colorName;
            if (p->vkCode == 'R') { ink = RGB(235, 60, 60); colorName = L"Red"; }
            else if (p->vkCode == 'G') { ink = RGB(60, 190, 90); colorName = L"Green"; }
            else if (p->vkCode == 'B') { ink = RGB(70, 130, 250); colorName = L"Blue"; }
            else if (p->vkCode == 'Y') { ink = RGB(250, 205, 40); colorName = L"Yellow"; }

            if (ink) {
                g_app.inkOverride = ink;
                g_app.inkOverrideSet = true;
                RedrawOverlay();
                ShowNotification(L"Pen Color", colorName, ink);
                return 1;
            }
        }

        if (p->vkCode == VK_TAB && g_app.isTriggerHeld) {
            HandleTriggerRelease();
        }
        else if ((p->vkCode == VK_LWIN || p->vkCode == VK_RWIN) && g_app.isTriggerHeld && !IsTriggerPhysicallyPressed()) {
            HandleTriggerRelease();
        }

        if (!isDown && !IsKeyMatching(p->vkCode, g_config.triggerKey)) {
            if (IsModifierKey(p->vkCode)) {
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }
            bool blockAllKeyups = g_app.textInputActive || g_bindingMode != BindingMode::None || g_app.breakTimer.editing || g_app.freezeMode;
            if (blockAllKeyups) return 1;
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        if (g_app.keycastEnabled && isDown && !g_app.breakTimer.active && !g_app.textInputActive && g_bindingMode == BindingMode::None) {
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
                    g_app.keycastTextValue = combo;
                    g_app.keycastText = true;
                    g_app.keycastUntilTick = GetTickCount64() + 1500;
                    RedrawOverlay();
                }
            }
        }

        if (g_app.breakTimer.active) {
            if (g_app.breakTimer.editing) {
                if (!isDown) return 1;
                if (p->vkCode == VK_ESCAPE) {
                    g_app.breakTimer.editing = false;
                    g_app.breakTimer.inputStr.clear();
                    RedrawOverlay();
                    return 1;
                }
                if (p->vkCode == VK_RETURN) {
                    CommitBreakTimerInput();
                    return 1;
                }
                if (p->vkCode == VK_BACK) {
                    if (!g_app.breakTimer.inputStr.empty()) {
                        g_app.breakTimer.inputStr.pop_back();
                        RedrawOverlay();
                    }
                    return 1;
                }

                wchar_t ch = 0;
                if (p->vkCode >= VK_NUMPAD0 && p->vkCode <= VK_NUMPAD9) {
                    ch = static_cast<wchar_t>(L'0' + (p->vkCode - VK_NUMPAD0));
                }
                else {
                    wchar_t c = TranslateVkToChar(p->vkCode);
                    if (c >= L'0' && c <= L'9') ch = c;
                    else if (c == L':' || c == L'.' || c == L',' || c == L';' || c == L' ') ch = L':';
                }

                if (ch != 0 && g_app.breakTimer.inputStr.length() < 6) {
                    g_app.breakTimer.inputStr.push_back(ch);
                    RedrawOverlay();
                    return 1;
                }
                return 1;
            }

            if (!isDown) return CallNextHookEx(nullptr, nCode, wParam, lParam);

            if (p->vkCode == VK_ESCAPE) {
                StopBreakTimer();
                ShowNotification(L"Break Timer", L"Timer cancelled", RGB(220, 70, 70));
                return 1;
            }
            if (p->vkCode == VK_SPACE) {
                g_app.breakTimer.paused = !g_app.breakTimer.paused;
                RedrawOverlay();
                return 1;
            }

            bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) < 0);
            int step = shiftPressed ? 5 : 60;

            if (p->vkCode == VK_UP || p->vkCode == VK_RIGHT) {
                g_app.breakTimer.remainingSec += step;
                if (g_app.breakTimer.remainingSec > 5999) g_app.breakTimer.remainingSec = 5999;
                g_app.breakTimer.totalSec = std::max(g_app.breakTimer.totalSec, g_app.breakTimer.remainingSec);
                RedrawOverlay();
                return 1;
            }
            if (p->vkCode == VK_DOWN || p->vkCode == VK_LEFT) {
                g_app.breakTimer.remainingSec = std::max(5, g_app.breakTimer.remainingSec - step);
                RedrawOverlay();
                return 1;
            }
        }

        if (g_app.textInputActive && isDown && !IsKeyMatching(p->vkCode, g_config.triggerKey)) {
            if (IsModifierKey(p->vkCode)) return CallNextHookEx(nullptr, nCode, wParam, lParam);
            if (p->vkCode == VK_RETURN) { CommitTextInput(); return 1; }
            if (p->vkCode == VK_ESCAPE) {
                g_app.textInputActive = false;
                g_app.textDraft.text.clear();
                g_app.textDraft.cachedBitmap = nullptr;
                RedrawOverlay();
                return 1;
            }
            if (p->vkCode == VK_BACK) {
                if (!g_app.textDraft.text.empty()) {
                    g_app.textDraft.text.pop_back();
                    RedrawOverlay();
                }
                return 1;
            }
            if (IsTextInputBlocker(p->vkCode)) return 1;

            wchar_t c = TranslateVkToChar(p->vkCode);
            if (c >= 0x20 && g_app.textDraft.text.length() < 256) {
                g_app.textDraft.text.push_back(c);
                RedrawOverlay();
            }
            return 1;
        }

        if (IsKeyMatching(p->vkCode, g_config.triggerKey)) {
            if (isDown) {
                g_app.isTriggerHeld = true;
                if (g_app.textInputActive) CommitTextInput();
            }
            else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                HandleTriggerRelease();
            }
        }
        else if (g_app.isTriggerHeld && isDown) {
            if (p->vkCode == 'T') { ToggleBreakTimer(5); return 1; }
            if (p->vkCode == 'S') { g_app.spotlightMode = !g_app.spotlightMode; StartZoomTimer(); return 1; }
            if (p->vkCode == 'V') {
                g_config.holdUsesLaser = !g_config.holdUsesLaser;
                SaveConfig();
                UpdateSettingsUI();
                ShowNotification(L"Laser Ink", g_config.holdUsesLaser ? L"Enabled (Hold)" : L"Disabled (Hold)", RGB(255, 90, 90));
                return 1;
            }
            if (p->vkCode == 'X') {
                g_app.textInputActive = true;
                g_app.textDraft = Stroke();
                g_app.textDraft.type = StrokeType::Text;
                g_app.textDraft.color = InkColorForNewStroke();
                g_app.textDraft.points.emplace_back();
                GetCursorPos(&g_app.textDraft.points[0]);
                StartZoomTimer();
                return 1;
            }
            if (p->vkCode == 'K') {
                g_app.keycastEnabled = !g_app.keycastEnabled;
                g_app.keycastUntilTick = 0;
                ShowNotification(L"Keystroke HUD", g_app.keycastEnabled ? L"Enabled" : L"Disabled", RGB(150, 150, 255));
                return 1;
            }
            if (p->vkCode == VK_ESCAPE) {
                if (g_app.cropMode) {
                    g_app.cropMode = false;
                    g_app.cropDragging = false;
                    SetCursor(LoadCursor(NULL, IDC_ARROW));
                    RedrawOverlay();
                    return 1;
                }
                if (g_app.boardMode != BoardMode::None) {
                    g_app.boardMode = BoardMode::None;
                    RedrawOverlay();
                    return 1;
                }
                g_targetZoom = 1.0f;
                g_currentZoom = 1.0f;
                g_app.inkOverrideSet = false;
                ResetDrawingState();
                UpdateCamera();
                return 1;
            }
            if (p->vkCode == 'P') {
                g_config.keepDrawingsOnRelease = !g_config.keepDrawingsOnRelease;
                SaveConfig();
                UpdateSettingsUI();
                if (g_config.keepDrawingsOnRelease) {
                    g_app.persistentDrawingsActive = true;
                    ShowNotification(L"Pin Mode", L"Drawings pinned to screen", RGB(46, 204, 113));
                }
                else {
                    ResetDrawingState();
                    ShowNotification(L"Pin Mode", L"Drawings unpinned and cleared", RGB(235, 60, 60));
                }
                return 1;
            }
            if (p->vkCode == 'Z') {
                if (UndoLastStroke()) ShowNotification(L"Undo", L"Last stroke removed", RGB(120, 160, 255));
                return 1;
            }
            if (p->vkCode == 'C') {
                if (IsRectKeyPressed()) StartCropSelection();
                else if (g_hwndOverlay) PostMessageW(g_hwndOverlay, WM_APP_TAKE_SCREENSHOT, 0, 0);
                return 1;
            }
            if (p->vkCode == 'W') {
                if (g_app.boardMode == BoardMode::None) g_app.boardMode = BoardMode::White;
                else if (g_app.boardMode == BoardMode::White) g_app.boardMode = BoardMode::Dark;
                else g_app.boardMode = BoardMode::None;
                RedrawOverlay();
                return 1;
            }
            if (p->vkCode == 'H') {
                g_app.activeToolMode = (g_app.activeToolMode == ActiveToolMode::Highlight) ? ActiveToolMode::None : ActiveToolMode::Highlight;
                ShowNotification(L"Highlighter", g_app.activeToolMode == ActiveToolMode::Highlight ? L"Enabled" : L"Disabled", RGB(250, 205, 40));
                return 1;
            }
            if (p->vkCode == 'O') {
                g_app.activeToolMode = (g_app.activeToolMode == ActiveToolMode::Blur) ? ActiveToolMode::None : ActiveToolMode::Blur;
                ShowNotification(L"Blur Redaction", g_app.activeToolMode == ActiveToolMode::Blur ? L"Enabled" : L"Disabled", RGB(140, 140, 255));
                return 1;
            }

            COLORREF ink = 0;
            std::wstring colorName;
            if (p->vkCode == 'R') { ink = RGB(235, 60, 60); colorName = L"Red"; }
            else if (p->vkCode == 'G') { ink = RGB(60, 190, 90); colorName = L"Green"; }
            else if (p->vkCode == 'B') { ink = RGB(70, 130, 250); colorName = L"Blue"; }
            else if (p->vkCode == 'Y') { ink = RGB(250, 205, 40); colorName = L"Yellow"; }

            if (ink) {
                g_app.inkOverride = ink;
                g_app.inkOverrideSet = true;
                if (!g_app.currentStroke.points.empty()) g_app.currentStroke.color = ink;
                RedrawOverlay();
                ShowNotification(L"Pen Color", colorName, ink);
                return 1;
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
        else if (wParam == WM_MBUTTONDOWN) s_middlePhysical = true;
        else if (wParam == WM_MBUTTONUP)   s_middlePhysical = false;

        if (wParam == WM_MBUTTONUP && s_swallowMiddleUp) {
            s_swallowMiddleUp = false;
            return 1;
        }
        if (wParam == WM_XBUTTONUP || wParam == WM_NCXBUTTONUP) {
            const WORD button = HIWORD(pMouse->mouseData);
            if (button == XBUTTON1 && s_swallowXButton1Up) { s_swallowXButton1Up = false; return 1; }
            if (button == XBUTTON2 && s_swallowXButton2Up) { s_swallowXButton2Up = false; return 1; }
        }

        if (g_app.isTriggerHeld && !IsTriggerPhysicallyPressed()) {
            HandleTriggerRelease();
        }

        if (g_app.spotlightMode || g_app.textInputActive) {
            if (wParam == WM_MOUSEMOVE || wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
                wParam == WM_MBUTTONDOWN || wParam == WM_LBUTTONUP || wParam == WM_RBUTTONUP) {
                StartZoomTimer();
            }
        }

        if (g_app.breakTimer.active) {
            if (wParam == WM_LBUTTONDOWN) {
                float cx = 0.0f, cy = 0.0f;
                GetBreakTimerCenter(cx, cy);
                int localX = pMouse->pt.x - GetSystemMetrics(SM_XVIRTUALSCREEN);
                int localY = pMouse->pt.y - GetSystemMetrics(SM_YVIRTUALSCREEN);

                if (localX >= (cx - 220) && localX <= (cx + 220) && localY >= (cy - 90) && localY <= (cy + 50)) {
                    g_app.breakTimer.editing = true;
                    g_app.breakTimer.paused = true;
                    g_app.breakTimer.inputStr.clear();
                    RedrawOverlay();
                    return 1;
                }
                else if (g_app.breakTimer.editing) {
                    CommitBreakTimerInput();
                    return 1;
                }
            }

            if (wParam == WM_MOUSEWHEEL && !g_app.breakTimer.editing) {
                short delta = GET_WHEEL_DELTA_WPARAM(pMouse->mouseData);
                bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) < 0);
                int step = shiftPressed ? ((delta > 0) ? 5 : -5) : ((delta > 0) ? 60 : -60);

                g_app.breakTimer.remainingSec += step;
                if (g_app.breakTimer.remainingSec < 5) g_app.breakTimer.remainingSec = 5;
                if (g_app.breakTimer.remainingSec > 5999) g_app.breakTimer.remainingSec = 5999;
                g_app.breakTimer.totalSec = std::max(g_app.breakTimer.totalSec, g_app.breakTimer.remainingSec);
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
            else if (wParam == WM_MBUTTONDOWN) pressedVk = VK_MBUTTON;

            if (pressedVk != 0) {
                if (g_bindingMode == BindingMode::TriggerKey) g_config.triggerKey = pressedVk;
                else if (g_bindingMode == BindingMode::RectKey) g_config.rectKey = pressedVk;
                else if (g_bindingMode == BindingMode::FreezeKey) g_config.freezeKey = pressedVk;

                if (pressedVk == VK_MBUTTON) s_swallowMiddleUp = true;
                else if (pressedVk == VK_XBUTTON1) s_swallowXButton1Up = true;
                else if (pressedVk == VK_XBUTTON2) s_swallowXButton2Up = true;
                g_bindingMode = BindingMode::None;
                SaveConfig();
                UpdateSettingsUI();
                return 1;
            }
        }

        if (g_app.cropMode) {
            if (wParam == WM_MOUSEMOVE) {
                SetCursor(LoadCursor(NULL, IDC_CROSS));
                if (g_app.cropDragging) {
                    g_app.cropEnd = pMouse->pt;
                    RedrawOverlay();
                    return 1;
                }
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }
            if (wParam == WM_LBUTTONDOWN) {
                SetCursor(LoadCursor(NULL, IDC_CROSS));
                g_app.cropStart = pMouse->pt;
                g_app.cropEnd = pMouse->pt;
                g_app.cropDragging = true;
                RedrawOverlay();
                return 1;
            }
            if (wParam == WM_LBUTTONUP && g_app.cropDragging) {
                g_app.cropDragging = false;
                g_app.cropMode = false;
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                int l = std::min(g_app.cropStart.x, g_app.cropEnd.x);
                int t = std::min(g_app.cropStart.y, g_app.cropEnd.y);
                int r = std::max(g_app.cropStart.x, g_app.cropEnd.x);
                int b = std::max(g_app.cropStart.y, g_app.cropEnd.y);
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
            bool isDown = false, isUp = false;
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

            if (isDown) { g_app.isTriggerHeld = true; return 1; }
            else if (isUp) { HandleTriggerRelease(); return 1; }
        }

        const bool activeForDrawing = g_app.isTriggerHeld || g_app.freezeMode;

        if (activeForDrawing) {
            if (wParam == WM_MOUSEWHEEL) {
                short delta = GET_WHEEL_DELTA_WPARAM(pMouse->mouseData);
                g_targetZoom += (delta > 0) ? 0.25f : -0.25f;
                if (g_targetZoom > 5.0f) g_targetZoom = 5.0f;
                if (g_targetZoom < 1.0f) g_targetZoom = 1.0f;
                StartZoomTimer();
                return 1;
            }

            if (wParam == WM_MBUTTONDOWN && (g_app.freezeMode || g_config.triggerKey != VK_MBUTTON)) {
                Stroke badge;
                badge.type = StrokeType::Badge;
                badge.points = { pMouse->pt };
                badge.badgeNumber = g_app.stepCounter++;
                const bool isLaser = g_app.freezeMode ? g_app.laserMode : g_config.holdUsesLaser;
                if (isLaser) {
                    badge.birthTick = GetTickCount64();
                }
                g_app.strokes.push_back(badge);
                s_swallowMiddleUp = true;
                RedrawOverlay();
                return 1;
            }

            if (wParam == WM_LBUTTONDOWN) {
                g_app.currentStroke = Stroke();
                g_app.currentStroke.color = InkColorForNewStroke();
                if (g_app.activeToolMode == ActiveToolMode::Highlight) {
                    g_app.currentStroke.type = StrokeType::Highlight;
                    g_app.isDrawingHighlight = true;
                }
                else if (g_app.activeToolMode == ActiveToolMode::Blur) {
                    g_app.currentStroke.type = StrokeType::Blur;
                    g_app.isDrawingBlur = true;
                }
                else if (IsRectKeyPressed()) {
                    g_app.currentStroke.type = StrokeType::Rectangle;
                    g_app.isDrawRectangle = true;
                }
                else {
                    g_app.currentStroke.type = StrokeType::Line;
                    g_app.isDrawingLine = true;
                }
                g_app.currentStroke.points.push_back(pMouse->pt);
                StartZoomTimer();
                return 1;
            }
            else if (wParam == WM_RBUTTONDOWN) {
                g_app.currentStroke = Stroke();
                g_app.currentStroke.type = StrokeType::Arrow;
                g_app.currentStroke.color = InkColorForNewStroke();
                g_app.currentStroke.points.push_back(pMouse->pt);
                g_app.isDrawingArrow = true;
                StartZoomTimer();
                return 1;
            }

            if (wParam == WM_MOUSEMOVE &&
                (g_app.isDrawingLine || g_app.isDrawingArrow || g_app.isDrawRectangle || g_app.isDrawingHighlight || g_app.isDrawingBlur)) {
                if (!g_app.currentStroke.points.empty()) {
                    POINT last = g_app.currentStroke.points.back();
                    int dx = pMouse->pt.x - last.x;
                    int dy = pMouse->pt.y - last.y;
                    if (dx * dx + dy * dy < 9) {
                        return CallNextHookEx(nullptr, nCode, wParam, lParam);
                    }
                }

                const bool rectKeyPressed = IsRectKeyPressed();
                POINT np = pMouse->pt;
                if ((g_app.isDrawingLine || g_app.isDrawingArrow) && !g_app.currentStroke.points.empty() && rectKeyPressed) {
                    POINT first = g_app.currentStroke.points.front();
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

                const bool snapLine = rectKeyPressed && (g_app.isDrawingLine || g_app.isDrawingArrow);
                if (snapLine) {
                    if (g_app.currentStroke.points.size() >= 2) {
                        g_app.currentStroke.points.resize(2);
                        g_app.currentStroke.points[1] = np;
                    }
                    else {
                        g_app.currentStroke.points.push_back(np);
                    }
                }
                else if (g_app.currentStroke.points.size() > 1 &&
                    (g_app.isDrawRectangle || g_app.isDrawingBlur || g_app.isDrawingHighlight)) {
                    g_app.currentStroke.points.back() = np;
                }
                else {
                    g_app.currentStroke.points.push_back(np);
                }
                StartZoomTimer();
            }

            bool lmbRelease = (wParam == WM_LBUTTONUP) &&
                (g_app.isDrawingLine || g_app.isDrawRectangle || g_app.isDrawingHighlight || g_app.isDrawingBlur);
            if (lmbRelease || (wParam == WM_RBUTTONUP && g_app.isDrawingArrow)) {
                bool wasLine = (wParam == WM_LBUTTONUP && g_app.isDrawingLine);
                bool wasRect = (wParam == WM_LBUTTONUP && g_app.isDrawRectangle);

                g_app.isDrawingLine = g_app.isDrawRectangle = g_app.isDrawingArrow = g_app.isDrawingHighlight = g_app.isDrawingBlur = false;

                if (g_app.currentStroke.points.size() > 1) {
                    const bool isLaser = g_app.freezeMode ? g_app.laserMode : g_config.holdUsesLaser;
                    if (isLaser) {
                        g_app.currentStroke.birthTick = GetTickCount64();
                    }
                    if (wParam == WM_LBUTTONUP && g_app.currentStroke.type == StrokeType::Blur) {
                        POINT a = g_app.currentStroke.points.front();
                        POINT b = g_app.currentStroke.points.back();
                        RECT rc = { std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y) };
                        if ((rc.right - rc.left) >= 8 && (rc.bottom - rc.top) >= 8) {
                            g_app.currentStroke.cachedRect = rc;
                            g_app.currentStroke.cachedBitmap = nullptr;
                            g_app.strokes.push_back(g_app.currentStroke);
                        }
                    }
                    else {
                        g_app.strokes.push_back(g_app.currentStroke);
                    }
                }
                g_app.currentStroke.points.clear();
                g_app.currentStroke.cachedBitmap = nullptr;
                RedrawOverlay();
                if ((wasLine || wasRect) && g_currentZoom <= 1.0f) UpdateCamera();
                return 1;
            }

            if (g_app.freezeMode) {
                if (wParam == WM_LBUTTONDOWN || wParam == WM_LBUTTONUP ||
                    wParam == WM_RBUTTONDOWN || wParam == WM_RBUTTONUP ||
                    wParam == WM_MBUTTONDOWN || wParam == WM_MBUTTONUP) {
                    return 1;
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}