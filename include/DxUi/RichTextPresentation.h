#pragma once
#include <array>
#include <cmath>
#include <initializer_list>

namespace DxUi::RichText
{
// Semantic presentation only. Source text, Markdown parsing and edit history are
// independent of this opt-in document style; Default retains the legacy layout.
enum class MarkdownPresentationStyle
{
    Default,
    Subtle
};

// Opt-in semantic margins. Real blank-source caret lines still fill these minimum gaps.
struct SemanticSpacing final
{
    std::array<float, 6> headingBeforeDip{};
    std::array<float, 6> headingAfterDip{8.f, 8.f, 8.f, 8.f, 8.f, 8.f};
    float containerIndentDip = 16.f;
    [[nodiscard]] bool IsValid() const noexcept
    {
        if (! std::isfinite(containerIndentDip) || containerIndentDip < 0.f || containerIndentDip > 64.f)
            return false;
        for (const auto& values : {headingBeforeDip, headingAfterDip})
            for (const float value : values)
                if (! std::isfinite(value) || value < 0.f || value > 256.f)
                    return false;
        return true;
    }
    bool operator==(const SemanticSpacing&) const noexcept = default;
};
} // namespace DxUi::RichText
