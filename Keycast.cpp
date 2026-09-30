#include "Keycast.h"
#include "LensIt.h"
#include "Canvas.h"

static const ULONGLONG KEYCAST_FADE_IN_MS = 150;
static const ULONGLONG KEYCAST_FADE_OUT_MS = 200;
static const int KEYCAST_TOAST_GAP = 12;

static float s_keycastAlpha = 0.0f;
static ULONGLONG s_keycastShownTick = 0;
static bool s_prevKeycast = false;
static std::wstring s_prevKeycastValue;

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

bool IsKeycastActive() {
    return g_app.keycastText || s_keycastAlpha > 0.01f;
}

void UpdateKeycastState(bool& framePending) {
    const ULONGLONG now = GetTickCount64();

    if (g_app.keycastText != s_prevKeycast || g_app.keycastTextValue != s_prevKeycastValue) {
        if (g_app.keycastText && !s_prevKeycast) s_keycastShownTick = now;
        s_prevKeycast = g_app.keycastText;
        s_prevKeycastValue = g_app.keycastTextValue;
        framePending = true;
    }

    if (g_app.keycastText && now > g_app.keycastUntilTick) {
        g_app.keycastText = false;
        framePending = true;
    }

    if (g_app.keycastText) {
        const ULONGLONG elapsed = (now > s_keycastShownTick) ? (now - s_keycastShownTick) : 0;
        const ULONGLONG remaining = (g_app.keycastUntilTick > now) ? (g_app.keycastUntilTick - now) : 0;
        float ramp = std::min(1.0f, std::min(
            static_cast<float>(elapsed) / static_cast<float>(KEYCAST_FADE_IN_MS),
            static_cast<float>(remaining) / static_cast<float>(KEYCAST_FADE_OUT_MS)));

        if (ramp < 1.0f) framePending = true;
        s_keycastAlpha = ramp;
    }
    else {
        s_keycastAlpha = 0.0f;
    }
}

void DrawKeycastUI(Graphics& g, const POINT& toastAnchor, int toastW) {
    if (!g_app.keycastText && s_keycastAlpha <= 0.01f) return;

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

    const std::vector<std::wstring> parts = SplitKeycastCombo(g_app.keycastTextValue);

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
    const float slide = (1.0f - fade) * 8.0f;

    const float slotRight = static_cast<float>(toastAnchor.x + toastW - GetSystemMetrics(SM_XVIRTUALSCREEN));
    const float slotBottom = static_cast<float>(toastAnchor.y - GetSystemMetrics(SM_YVIRTUALSCREEN) - KEYCAST_TOAST_GAP);
    const float cardX = std::max(8.0f, slotRight - cardW);
    const float cardY = slotBottom - cardH + slide;

    GraphicsPath shadowPath;
    AddRoundedRect(shadowPath, cardX, cardY + 4.0f, cardW, cardH, cardRadius);
    SolidBrush shadowBrush(WithFade(Color(85, 0, 0, 0), fade));
    g.FillPath(&shadowBrush, &shadowPath);

    GraphicsPath cardPath;
    AddRoundedRect(cardPath, cardX, cardY, cardW, cardH, cardRadius);
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
        AddRoundedRect(capPath, cursorX, capY, capWidths[i], capHeight, capRadius);
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