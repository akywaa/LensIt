#pragma once
#include "LensIt.h"

void UpdateKeycastState(bool& framePending);
bool IsKeycastActive();
void DrawKeycastUI(Gdiplus::Graphics& g, const POINT& toastAnchor, int toastW);