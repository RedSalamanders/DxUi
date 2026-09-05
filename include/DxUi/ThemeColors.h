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
