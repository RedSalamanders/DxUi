#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
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
    Delimited,
    Delimiter
};
enum class MathAtomClass
{
    Ordinary,
    Operator,
    Binary,
    Relation,
    Opening,
    Closing,
    Punctuation,
    Inner
};
enum class MathLimits
{
    Automatic,
    AboveBelow,
    Side
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
    // Empty-size construction is deliberately fallible in MSVC Debug, whose
    // containers may allocate iterator proxies. Transfer storage only afterward.
    MathNode(MathNode&& other) : MathNode()
    {
        Swap(other);
    }
    MathNode& operator=(MathNode&& other)
    {
        Swap(other);
        return *this;
    }
    MathKind kind = MathKind::Row;
    std::wstring text;
    std::vector<MathNode> children;
    unsigned columns{};
    unsigned delimiterSize{}; // 1..4 for big/Big/bigg/Bigg; zero otherwise.
    MathAtomClass atomClass = MathAtomClass::Ordinary;
    MathLimits limits       = MathLimits::Automatic;
    bool italic{}, displayLimits{}, largeOperator{};

private:
    void Swap(MathNode& other) noexcept
    {
        using std::swap;
        swap(kind, other.kind);
        text.swap(other.text);
        children.swap(other.children);
        swap(columns, other.columns);
        swap(delimiterSize, other.delimiterSize);
        swap(atomClass, other.atomClass);
        swap(limits, other.limits);
        swap(italic, other.italic);
        swap(displayLimits, other.displayLimits);
        swap(largeOperator, other.largeOperator);
    }
};
struct MathExpression
{
    MathExpression() : diagnostic(0u, L' ')
    {
    }
    MathExpression(const MathNode& value, const std::wstring& issue) : root(value), diagnostic(issue)
    {
    }
    MathExpression(MathNode&& value, std::wstring&& issue) : root(std::move(value)), diagnostic(0u, L' ')
    {
        diagnostic.swap(issue);
    }
    MathExpression(const MathExpression&)            = default;
    MathExpression& operator=(const MathExpression&) = default;
    MathExpression(MathExpression&& other) : root(std::move(other.root)), diagnostic(0u, L' ')
    {
        diagnostic.swap(other.diagnostic);
    }
    MathExpression& operator=(MathExpression&& other)
    {
        root = std::move(other.root);
        diagnostic.swap(other.diagnostic);
        return *this;
    }
    MathNode root;
    std::wstring diagnostic;
    explicit operator bool() const noexcept
    {
        return diagnostic.empty();
    }
};
// Limits: 4096 source units, 512 syntax nodes, 24 nesting levels, at most
// 16 rows/columns and 128 total matrix cells. Styles/limits are data, not TeX execution.
// Unsupported/malformed input is all-or-nothing; no partially rendered formula.
[[nodiscard]] MathExpression ParseMath(std::wstring_view source);
} // namespace DxUi::RichText
