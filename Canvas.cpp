#include "Canvas.h"
#include "WinHandles.h"

void AddRoundedRect(GraphicsPath& path, REAL x, REAL y, REAL w, REAL h, REAL radius) {
    const REAL r = std::min(radius, std::min(w, h) / 2.0f);
    const REAL d = r * 2.0f;
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

void DrawArrow(Graphics& g, Pen& pen, SolidBrush& brush, POINT p1, POINT p2, int width, int offX, int offY) {
    float x1 = static_cast<float>(p1.x - offX);
    float y1 = static_cast<float>(p1.y - offY);
    float x2 = static_cast<float>(p2.x - offX);
    float y2 = static_cast<float>(p2.y - offY);

    float dx = x2 - x1;
    float dy = y2 - y1;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 4.0f) return;

    float ux = dx / dist;
    float uy = dy / dist;
    float nx = -uy;
    float ny = ux;

    float arrowLen = static_cast<float>(width) * 3.2f + 10.0f;
    if (arrowLen > dist * 0.85f) arrowLen = dist * 0.85f;
    float arrowHalfWidth = arrowLen * 0.55f;

    float lineEndX = x2 - ux * (arrowLen - 2.0f);
    float lineEndY = y2 - uy * (arrowLen - 2.0f);

    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapFlat);
    g.DrawLine(&pen, x1, y1, lineEndX, lineEndY);

    PointF pts[3] = {
        PointF(x2, y2),
        PointF(x2 - ux * arrowLen + nx * arrowHalfWidth, y2 - uy * arrowLen + ny * arrowHalfWidth),
        PointF(x2 - ux * arrowLen - nx * arrowHalfWidth, y2 - uy * arrowLen - ny * arrowHalfWidth)
    };
    g.FillPolygon(&brush, pts, 3);
}

std::shared_ptr<Gdiplus::Bitmap> BakeBlurredBitmap(RECT rc) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 8 || h < 8) return nullptr;

    ScopedScreenDC screenDC;
    if (!screenDC) return nullptr;

    ScopedMemoryDC capDC(screenDC.get());
    UniqueBitmap capBmp(static_cast<HBITMAP>(CreateCompatibleBitmap(screenDC.get(), w, h)));
    if (!capDC || !capBmp) return nullptr;

    ScopedSelectedObject selected(capDC.get(), capBmp.get());
    BitBlt(capDC.get(), 0, 0, w, h, screenDC.get(), rc.left, rc.top, SRCCOPY);
    selected.restore();

    auto resultBmp = std::make_shared<Gdiplus::Bitmap>(w, h, PixelFormat32bppPARGB);
    {
        Gdiplus::Bitmap src(capBmp.get(), NULL);
        int sw = std::max(2, w / 14);
        int sh = std::max(2, h / 14);

        Gdiplus::Bitmap smallBmp(sw, sh, PixelFormat32bppPARGB);
        {
            Graphics gSmall(&smallBmp);
            gSmall.SetInterpolationMode(InterpolationModeBilinear);
            gSmall.DrawImage(&src, 0, 0, sw, sh);
        }

        Graphics gDest(resultBmp.get());
        gDest.SetInterpolationMode(InterpolationModeBilinear);
        gDest.DrawImage(&smallBmp, 0, 0, w, h);

        SolidBrush blackTint(Color(185, 12, 12, 14));
        gDest.FillRectangle(&blackTint, 0, 0, w, h);

        Pen framePen(Color(190, 45, 45, 50), 1.0f);
        gDest.DrawRectangle(&framePen, 0, 0, w - 1, h - 1);
    }
    return resultBmp;
}

static void ApplyStrokeAlpha(const Stroke& stroke, Color& col) {
    if (stroke.opacity >= 0.999f) return;
    col = Color(static_cast<BYTE>(col.GetAlpha() * stroke.opacity), col.GetRed(), col.GetGreen(), col.GetBlue());
}

void DrawStroke(Graphics& g, const Stroke& stroke, int offX, int offY) {
    if (stroke.type == StrokeType::Text) {
        if (stroke.points.empty()) return;
        COLORREF c = stroke.color ? stroke.color : g_config.lineColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, col);
        SolidBrush textBrush(col);
        Font font(L"Segoe UI", 18.0f, FontStyleBold);
        PointF origin(static_cast<REAL>(stroke.points[0].x - offX), static_cast<REAL>(stroke.points[0].y - offY));
        g.DrawString(stroke.text.c_str(), -1, &font, origin, &textBrush);
        return;
    }

    if (stroke.type == StrokeType::Badge) {
        if (stroke.points.empty()) return;
        int x = stroke.points[0].x - offX;
        int y = stroke.points[0].y - offY;
        int radius = 15;

        COLORREF bg = stroke.color ? stroke.color : g_config.badgeColor;
        Color bgCol(255, GetRValue(bg), GetGValue(bg), GetBValue(bg));
        ApplyStrokeAlpha(stroke, bgCol);
        SolidBrush bgBrush(bgCol);
        Pen borderPen(Color(255, 30, 30, 30), 2.0f);
        g.FillEllipse(&bgBrush, x - radius, y - radius, radius * 2, radius * 2);
        g.DrawEllipse(&borderPen, x - radius, y - radius, radius * 2, radius * 2);

        std::wstring numStr = std::to_wstring(stroke.badgeNumber);
        Font font(L"Segoe UI", 10.0f, FontStyleBold);
        SolidBrush textBrush(Color(255, 20, 20, 20));
        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentCenter);
        fmt.SetLineAlignment(StringAlignmentCenter);

        RectF rect(static_cast<REAL>(x - radius), static_cast<REAL>(y - radius + 1), static_cast<REAL>(radius * 2), static_cast<REAL>(radius * 2));
        g.DrawString(numStr.c_str(), -1, &font, rect, &fmt, &textBrush);
        return;
    }

    if (stroke.points.size() < 2) return;

    if (stroke.type == StrokeType::Blur) {
        if (stroke.cachedBitmap) {
            g.DrawImage(stroke.cachedBitmap.get(),
                static_cast<REAL>(stroke.cachedRect.left - offX),
                static_cast<REAL>(stroke.cachedRect.top - offY));
        }
        else {
            POINT a = stroke.points.front();
            POINT b = stroke.points.back();
            int left = std::min(a.x, b.x) - offX;
            int right = std::max(a.x, b.x) - offX;
            int top = std::min(a.y, b.y) - offY;
            int bottom = std::max(a.y, b.y) - offY;

            SolidBrush previewBrush(Color(140, 16, 16, 18));
            g.FillRectangle(&previewBrush, static_cast<REAL>(left), static_cast<REAL>(top), static_cast<REAL>(right - left), static_cast<REAL>(bottom - top));

            Pen previewPen(Color(200, 200, 200, 200), 1.0f);
            previewPen.SetDashStyle(DashStyleDash);
            g.DrawRectangle(&previewPen, static_cast<REAL>(left), static_cast<REAL>(top), static_cast<REAL>(right - left), static_cast<REAL>(bottom - top));
        }
        return;
    }

    if (stroke.type == StrokeType::Highlight) {
        COLORREF c = stroke.color ? stroke.color : RGB(250, 205, 40);
        POINT a = stroke.points.front();
        POINT b = stroke.points.back();
        int left = std::min(a.x, b.x) - offX;
        int right = std::max(a.x, b.x) - offX;
        int top = std::min(a.y, b.y) - offY;
        int bottom = std::max(a.y, b.y) - offY;
        Color fillCol(110, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, fillCol);
        SolidBrush fill(fillCol);
        g.FillRectangle(&fill, static_cast<REAL>(left), static_cast<REAL>(top), static_cast<REAL>(right - left), static_cast<REAL>(bottom - top));
        return;
    }

    if (stroke.type == StrokeType::Line) {
        COLORREF c = stroke.color ? stroke.color : g_config.lineColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, col);
        Pen pen(col, static_cast<REAL>(g_config.lineWidth));
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        pen.SetLineJoin(LineJoinRound);

        std::vector<Point> pts;
        pts.reserve(stroke.points.size());
        for (const auto& p : stroke.points) {
            pts.push_back(Point(p.x - offX, p.y - offY));
        }
        g.DrawLines(&pen, pts.data(), static_cast<INT>(pts.size()));
    }
    else if (stroke.type == StrokeType::Arrow) {
        COLORREF c = stroke.color ? stroke.color : g_config.arrowColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, col);
        Pen pen(col, static_cast<REAL>(g_config.arrowWidth));
        SolidBrush brush(col);
        DrawArrow(g, pen, brush, stroke.points.front(), stroke.points.back(), g_config.arrowWidth, offX, offY);
    }
    else if (stroke.type == StrokeType::Rectangle) {
        COLORREF c = stroke.color ? stroke.color : g_config.rectColor;
        Color col(255, GetRValue(c), GetGValue(c), GetBValue(c));
        ApplyStrokeAlpha(stroke, col);
        Pen pen(col, static_cast<REAL>(g_config.rectWidth));
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        pen.SetLineJoin(LineJoinMiter);

        POINT a = stroke.points.front();
        POINT b = stroke.points.back();
        int left = std::min(a.x, b.x) - offX;
        int right = std::max(a.x, b.x) - offX;
        int top = std::min(a.y, b.y) - offY;
        int bottom = std::max(a.y, b.y) - offY;
        g.DrawRectangle(&pen, static_cast<REAL>(left), static_cast<REAL>(top), static_cast<REAL>(right - left), static_cast<REAL>(bottom - top));
    }
}

static void FillBoardBackground(Graphics& g, int x, int y, int w, int h) {
    if (g_app.boardMode == BoardMode::White) {
        SolidBrush boardBrush(Color(255, 255, 255, 255));
        g.FillRectangle(&boardBrush, x, y, w, h);
    }
    else if (g_app.boardMode == BoardMode::Dark) {
        SolidBrush boardBrush(Color(255, 24, 24, 27));
        g.FillRectangle(&boardBrush, x, y, w, h);
    }
}

void CopyScreenshotToClipboard() {
    int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int scrW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int scrH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    ScopedScreenDC screenDC;
    if (!screenDC) return;

    ScopedMemoryDC captureDC(screenDC.get());
    UniqueBitmap captureBmp(static_cast<HBITMAP>(CreateCompatibleBitmap(screenDC.get(), scrW, scrH)));
    if (!captureDC || !captureBmp) return;

    ScopedSelectedObject selected(captureDC.get(), captureBmp.get());
    BitBlt(captureDC.get(), 0, 0, scrW, scrH, screenDC.get(), vScreenX, vScreenY, SRCCOPY);

    {
        Graphics g(captureDC.get());
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        FillBoardBackground(g, 0, 0, scrW, scrH);
        for (const auto& s : g_app.strokes) DrawStroke(g, s, vScreenX, vScreenY);
        if (!g_app.currentStroke.points.empty()) DrawStroke(g, g_app.currentStroke, vScreenX, vScreenY);
    }

    selected.restore();

    ScopedClipboard clipboard(g_hwndOverlay);
    if (!clipboard.ok()) return;

    EmptyClipboard();
    if (SetClipboardData(CF_BITMAP, captureBmp.get())) captureBmp.release();
}

void CopyRegionToClipboard(RECT rcScreen) {
    int vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);

    int left = std::max(static_cast<int>(rcScreen.left), vScreenX);
    int top = std::max(static_cast<int>(rcScreen.top), vScreenY);
    int right = std::min(static_cast<int>(rcScreen.right), vScreenX + GetSystemMetrics(SM_CXVIRTUALSCREEN));
    int bottom = std::min(static_cast<int>(rcScreen.bottom), vScreenY + GetSystemMetrics(SM_CYVIRTUALSCREEN));

    int w = right - left;
    int h = bottom - top;
    if (w < 1 || h < 1) return;

    ScopedScreenDC screenDC;
    if (!screenDC) return;

    ScopedMemoryDC captureDC(screenDC.get());
    UniqueBitmap captureBmp(static_cast<HBITMAP>(CreateCompatibleBitmap(screenDC.get(), w, h)));
    if (!captureDC || !captureBmp) return;

    ScopedSelectedObject selected(captureDC.get(), captureBmp.get());
    BitBlt(captureDC.get(), 0, 0, w, h, screenDC.get(), left, top, SRCCOPY);

    {
        Graphics g(captureDC.get());
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        FillBoardBackground(g, 0, 0, w, h);
        for (const auto& s : g_app.strokes) DrawStroke(g, s, left, top);
        if (!g_app.currentStroke.points.empty()) DrawStroke(g, g_app.currentStroke, left, top);
    }

    selected.restore();

    ScopedClipboard clipboard(g_hwndOverlay);
    if (!clipboard.ok()) return;

    EmptyClipboard();
    if (SetClipboardData(CF_BITMAP, captureBmp.get())) captureBmp.release();
}

void StartCropSelection() {
    if (!g_hwndOverlay) return;
    g_app.cropMode = true;
    ShowWindow(g_hwndOverlay, SW_SHOWNOACTIVATE);
    SetCursor(LoadCursor(NULL, IDC_CROSS));
    RedrawOverlay();
}

bool UndoLastStroke() {
    if (!g_app.strokes.empty()) {
        if (g_app.strokes.back().type == StrokeType::Badge && g_app.stepCounter > 1) {
            g_app.stepCounter--;
        }
        g_app.strokes.pop_back();
        if (g_app.strokes.empty()) {
            g_app.persistentDrawingsActive = false;
        }
        RedrawOverlay();
        return true;
    }
    return false;
}

void PruneVanishingStrokes() {
    ULONGLONG now = GetTickCount64();
    bool changed = false;
    for (size_t i = g_app.strokes.size(); i > 0; ) {
        --i;
        Stroke& s = g_app.strokes[i];
        if (s.birthTick == 0) continue;
        float age = static_cast<float>(now - s.birthTick);
        if (age >= 1200.0f) {
            g_app.strokes.erase(g_app.strokes.begin() + i);
            changed = true;
        }
        else if (age >= 600.0f) {
            float k = 1.0f - (age - 600.0f) / 600.0f;
            if (k < 0.0f) k = 0.0f;
            if (k < s.opacity) {
                s.opacity = k;
                changed = true;
            }
        }
    }
    if (changed) {
        if (g_app.strokes.empty()) {
            g_app.persistentDrawingsActive = false;
        }
        RedrawOverlay();
    }
}