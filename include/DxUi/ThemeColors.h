#pragma once
#include <cstdint>
#include <windows.h>
namespace DxUi
{
struct ThemeColors
{
    uint32_t sizeBytes;

    unsigned int dpi;

    uint32_t backgroundArgb;
    uint32_t textArgb;
    uint32_t selectionBackgroundArgb;
    uint32_t selectionTextArgb;
    uint32_t accentArgb;

    // Zero alpha means none supplied: defaults keep distinct info/warning/error tones; high contrast uses windowBackground/text.
    // A missing partner is derived for readable contrast where possible; outside high contrast supplied pairs are used as given.
    // High contrast always preserves the user's window background/text pair, including when alert colors were supplied.
    uint32_t alertErrorBackgroundArgb;
    uint32_t alertErrorTextArgb;
    uint32_t alertWarningBackgroundArgb;
    uint32_t alertWarningTextArgb;
    uint32_t alertInfoBackgroundArgb;
    uint32_t alertInfoTextArgb;

    BOOL darkMode;
    BOOL highContrast;
    BOOL rainbowMode;
    BOOL darkBase;

    uint32_t diffAddedBackgroundArgb;
    uint32_t diffRemovedBackgroundArgb;
    uint32_t diffContextBackgroundArgb;
    uint32_t diffHeaderBackgroundArgb;
    uint32_t diffBannerBackgroundArgb;
    uint32_t diffPlaceholderBackgroundArgb;
    uint32_t diffDividerArgb;
};

} // namespace DxUi
