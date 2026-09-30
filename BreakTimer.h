#pragma once
#include "LensIt.h"

void StartBreakTimer(int minutes = 5);
void StopBreakTimer();
void ToggleBreakTimer(int minutes = 5);
void CommitBreakTimerInput();
void GetBreakTimerCenter(float& cx, float& cy);
void DrawBreakTimerUI(Gdiplus::Graphics& g, int w, int h);