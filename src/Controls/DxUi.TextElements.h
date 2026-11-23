#pragma once
#include <DxUi/Configuration.h>
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
// The start is an established element boundary. Arbitrary-caret wrappers start
// from FindTextElementSearchAnchor to retain RI/Indic/emoji context. Unpaired surrogates are
// preserved as individual code units for legacy callers; the rich model rejects them.
[[nodiscard]] std::size_t FindNextTextElementBoundary(std::wstring_view text, std::size_t start) noexcept;
// Returns an element boundary at or before position: 0, the end, or the nearest
// code-point position whose neighbor pair breaks under every possible preceding
// context (UAX #29 GB4/GB5 or GB999 with GB3 and GB6-GB13 excluded by the pair).
// A forward walk from it equals a walk from 0. The backward search is bounded by
// the containing element, except across regional-indicator runs (GB12/GB13),
// pictographic ZWJ chains (GB11) and Indic linker chains (GB9c).
[[nodiscard]] std::size_t FindTextElementSearchAnchor(std::wstring_view text, std::size_t position) noexcept;
// Returns the preceding boundary, or the containing element's start for an
// interior UTF-16 position. Contextual RI/Indic/emoji runs remain unbounded.
[[nodiscard]] std::size_t FindPreviousTextElementBoundary(std::wstring_view text, std::size_t position) noexcept;
#if DXUI_ENABLE_DIAGNOSTICS
extern thread_local std::size_t testTextElementPropertyLookups;
// UTF-16 code units read by the grapheme walkers, counted once per code point
// read. A local hook until the shared work counters own it.
extern thread_local std::size_t testTextElementCodeUnitsVisited;
#endif
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
