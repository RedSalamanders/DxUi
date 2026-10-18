#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
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
struct TextElementRange
{
    std::size_t first{}, last{};
};
// At an interior ACP, either collapsed delete removes the containing element.
// Explicit noncollapsed source selections are a separate caller policy.
// An optional trusted index contains every ascending boundary, from 0 to size;
// prepared layouts use it for logarithmic queries. Other callers walk once.
[[nodiscard]] TextElementRange GetTextElementDeletionRange(std::wstring_view text,
                                                           std::size_t caret,
                                                           bool backwards,
                                                           std::span<const std::size_t> preparedBoundaries = {}) noexcept;
} // namespace DxUi
