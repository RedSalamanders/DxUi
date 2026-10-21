#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace DxUi::RichText
{
inline constexpr std::size_t MathMaximumSourceUnits = 4096u;
// Data-only, bounded native mathematical notation. This never evaluates TeX,
// reads files, expands user macros, invokes processes or fetches resources.
enum class MathKind
{
    Row,
    Text,
    Fraction,
    Root,
    Scripts,
    Matrix,
    Accent,
    Style,
    Space,
    Delimited
};
struct MathNode
{
    MathNode() : text(0u, L' '), children(0u)
    {
    }
    MathNode(MathKind value, std::wstring&& label, std::vector<MathNode>&& items, unsigned count) : kind(value), text(0u, L' '), children(0u), columns(count)
    {
        text.swap(label);
        children.swap(items);
    }
    MathNode(const MathNode&)            = default;
    MathNode& operator=(const MathNode&) = default;
    MathKind kind                        = MathKind::Row;
    std::wstring text;
    std::vector<MathNode> children;
    unsigned columns{};
};
struct MathExpression
{
    MathExpression() : diagnostic(0u, L' ')
    {
    }
    MathExpression(const MathNode& value, const std::wstring& issue) : root(value), diagnostic(issue)
    {
    }
    MathExpression(const MathExpression&)            = default;
    MathExpression& operator=(const MathExpression&) = default;
    MathNode root;
    std::wstring diagnostic;
    explicit operator bool() const noexcept
    {
        return diagnostic.empty();
    }
};
// Limits: 4096 source units, 512 syntax nodes, 24 nesting levels, 16x16 cells.
// Unsupported/malformed input is all-or-nothing; no partially rendered formula.
[[nodiscard]] MathExpression ParseMath(std::wstring_view source);
} // namespace DxUi::RichText
