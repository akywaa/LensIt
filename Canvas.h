#pragma once
#include "LensIt.h"

void DrawStroke(Gdiplus::Graphics& g, const Stroke& stroke, int offX, int offY);
void DrawArrow(Gdiplus::Graphics& g, Gdiplus::Pen& pen, Gdiplus::SolidBrush& brush, POINT p1, POINT p2, int width, int offX, int offY);
std::shared_ptr<Gdiplus::Bitmap> BakeBlurredBitmap(RECT rc);

void CopyScreenshotToClipboard();
void CopyRegionToClipboard(RECT rcScreen);
void StartCropSelection();
bool UndoLastStroke();
void PruneVanishingStrokes();