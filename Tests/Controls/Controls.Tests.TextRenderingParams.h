#pragma once

// Text rendering parameters (Specs/UI/UI_ThemeAndTypography.md, "Text antialiasing and rendering parameters"): the
// antialias mode Direct2D uses by default on each target kind, which rendering parameters each mode accepts, and the
// hosts' choice of mode and per-monitor parameters for the user's font-smoothing setting, replaced through the test seam.
void TestTextAntialiasDefaultsFollowTheTargetAlphaMode();
void TestTextRenderingParamsCompatibilityWithAntialiasModes();
void TestTextRenderingComplexUiCapturesForEachHost();
void TestTextRenderingWindowHostFollowsFontSmoothingOnComplexUi();
void TestTextRenderingDefaultSettingKeepsTodaysPixels();
void TestTextRenderingEmbeddedComplexUiUnderEachSetting();
void TestTextRenderingWindowHostRebindsOnDpiMonitorAndSettingChanges();
