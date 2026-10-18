#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

// Pure shared UTF-16 services. No host, font, device, allocation or scheduling.
namespace DxUi
{
struct Utf16CodePoint
{
    std::uint32_t value{};
    std::size_t end{};
};
[[nodiscard]] bool IsUtf16LeadSurrogate(wchar_t value) noexcept;
[[nodiscard]] bool IsUtf16TrailSurrogate(wchar_t value) noexcept;
[[nodiscard]] std::size_t StepToPreviousCodePoint(std::wstring_view text, std::size_t caret) noexcept;
[[nodiscard]] std::size_t StepToNextCodePoint(std::wstring_view text, std::size_t caret) noexcept;
[[nodiscard]] Utf16CodePoint ReadCodePointAt(std::wstring_view text, std::size_t position) noexcept;
// The start is an established element boundary. Arbitrary-caret wrappers scan
// from the beginning to retain RI/Indic/emoji context. Unpaired surrogates are
// preserved as individual code units for legacy callers; the rich model rejects them.
[[nodiscard]] std::size_t FindNextTextElementBoundary(std::wstring_view text, std::size_t start) noexcept;
} // namespace DxUi
