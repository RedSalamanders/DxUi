#pragma once
#include <DxUi/RichMath.h>
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <wil/com.h>
#include <wil/result.h>

namespace DxUi::RichText::Detail
{
// Immutable expression retained with the semantic publication. Only preparation
// creates text layouts; each Advance shapes at most one leaf. Draw uses caches.
class PreparedMath final
{
    struct Extent
    {
        float width{}, above{}, below{};
    };
    struct Glyph
    {
        const MathNode* node{};
        float font{};
        bool bold{};
        bool italic{};
        wil::com_ptr<IDWriteTextLayout> layout;
        Extent extent;
        D2D1_POINT_2F origin{};
    };
    struct Line
    {
        D2D1_POINT_2F first, last;
    };
    std::shared_ptr<const MathExpression> expression;
    wil::com_ptr<IDWriteFactory> factory;
    std::vector<Glyph> glyphs;
    std::vector<Line> lines;
    std::size_t cursor{};
    float font{};
    Extent extent;
    void Gather(const MathNode& node, float size, bool bold, bool italic = false)
    {
        if (node.kind == MathKind::Text)
            glyphs.push_back({&node, size, bold, italic, {}, {}, {}});
        else
            for (std::size_t index = 0; index < node.children.size(); ++index)
            {
                const bool reduced = node.kind == MathKind::Fraction || ((node.kind == MathKind::Scripts || node.kind == MathKind::Root) && index != 0);
                Gather(node.children[index],
                       size * (reduced ? node.kind == MathKind::Fraction ? 0.86f : 0.64f : 1.f),
                       bold || (node.kind == MathKind::Style && node.text == L"mathbf"),
                       node.kind == MathKind::Style ? node.text == L"mathit" : italic);
            }
    }
    Glyph& Leaf(const MathNode& node)
    {
        const auto found = std::ranges::find(glyphs, &node, &Glyph::node);
        if (found == glyphs.end())
            throw std::logic_error("Missing prepared mathematical leaf");
        return *found;
    }
    Extent Measure(const MathNode& node, float size)
    {
        using enum MathKind;
        if (node.kind == Text)
            return Leaf(node).extent;
        if (node.kind == Space)
            return {static_cast<float>(node.text.size()) * size * 0.25f, 0, 0};
        if (node.kind == Fraction)
        {
            const auto top = Measure(node.children[0], size * .86f), bottom = Measure(node.children[1], size * .86f);
            return {std::max(top.width, bottom.width) + size * .4f, top.above + top.below + size * .45f, bottom.above + bottom.below + size * .15f};
        }
        if (node.kind == Root)
        {
            const auto inner = Measure(node.children[0], size), index = Measure(node.children[1], size * .64f);
            return {inner.width + size * .7f + index.width, std::max(inner.above + size * .18f, index.above + index.below + size * .3f), inner.below};
        }
        if (node.kind == Scripts)
        {
            const auto base = Measure(node.children[0], size), sub = Measure(node.children[1], size * .64f), super = Measure(node.children[2], size * .64f);
            return {base.width + std::max(sub.width, super.width),
                    std::max(base.above, super.above + size * .55f),
                    std::max(base.below, sub.below + (sub.width ? size * .35f : 0.f))};
        }
        if (node.kind == Matrix)
        {
            float width{}, height{};
            for (unsigned column = 0; column < node.columns; ++column)
            {
                float widest{};
                for (std::size_t index = column; index < node.children.size(); index += node.columns)
                    widest = std::max(widest, Measure(node.children[index], size).width);
                width += widest + size * .65f;
            }
            for (std::size_t row = 0; row < node.children.size(); row += node.columns)
            {
                float above{}, below{};
                for (unsigned column = 0; column < node.columns; ++column)
                {
                    const auto cell = Measure(node.children[row + column], size);
                    above           = std::max(above, cell.above);
                    below           = std::max(below, cell.below);
                }
                height += std::max(size, above + below) + size * .3f;
            }
            return {width + size * .8f, height * .5f + size * .25f, std::max(0.f, height * .5f - size * .25f)};
        }
        if (node.kind == Accent)
        {
            auto result = Measure(node.children[0], size);
            if (node.text == L"underline")
                result.below += size * .2f;
            else
                result.above += size * .3f;
            return result;
        }
        if (node.kind == Delimited)
        {
            auto result = Measure(node.children[0], size);
            result.width += size * .8f;
            result.above += size * .1f;
            result.below += size * .1f;
            return result;
        }
        Extent result;
        for (const auto& child : node.children)
        {
            const auto value = Measure(child, size);
            result.width += value.width;
            result.above = std::max(result.above, value.above);
            result.below = std::max(result.below, value.below);
        }
        return result;
    }
    void Stroke(float x1, float y1, float x2, float y2)
    {
        lines.push_back({{x1, y1}, {x2, y2}});
    }
    void Fence(wchar_t shape, float x, float top, float bottom, float width)
    {
        if (shape == L'.')
            return;
        const bool right  = shape == L')' || shape == L']' || shape == L'}' || shape == L'⟩';
        const auto outer  = right ? x + width : x;
        const auto inner  = right ? x : x + width;
        const auto middle = (top + bottom) / 2;
        if (shape == L'(' || shape == L')')
        {
            Stroke(inner, top, outer, top + (bottom - top) * .2f);
            Stroke(outer, top + (bottom - top) * .2f, outer, bottom - (bottom - top) * .2f);
            Stroke(outer, bottom - (bottom - top) * .2f, inner, bottom);
        }
        else if (shape == L'{' || shape == L'}')
        {
            Stroke(inner, top, outer, top + (bottom - top) * .15f);
            Stroke(outer, top + (bottom - top) * .15f, outer, middle - 2);
            Stroke(outer, middle - 2, inner, middle);
            Stroke(inner, middle, outer, middle + 2);
            Stroke(outer, middle + 2, outer, bottom - (bottom - top) * .15f);
            Stroke(outer, bottom - (bottom - top) * .15f, inner, bottom);
        }
        else if (shape == L'⟨' || shape == L'⟩')
        {
            Stroke(inner, top, outer, middle);
            Stroke(outer, middle, inner, bottom);
        }
        else
        {
            Stroke(outer, top, outer, bottom);
            if (shape == L'‖')
                Stroke(outer + (right ? -3.f : 3.f), top, outer + (right ? -3.f : 3.f), bottom);
            else if (shape != L'|')
            {
                Stroke(outer, top, inner, top);
                Stroke(outer, bottom, inner, bottom);
            }
        }
    }
    void Place(const MathNode& node, float x, float baseline, float size)
    {
        using enum MathKind;
        if (node.kind == Text)
        {
            auto& leaf  = Leaf(node);
            leaf.origin = {x, baseline - leaf.extent.above};
            return;
        }
        if (node.kind == Space)
            return;
        const auto box = Measure(node, size);
        if (node.kind == Delimited)
        {
            Place(node.children[0], x + size * .4f, baseline, size);
            Fence(node.text[0], x, baseline - box.above, baseline + box.below, size * .25f);
            Fence(node.text[1], x + box.width - size * .25f, baseline - box.above, baseline + box.below, size * .25f);
            return;
        }
        if (node.kind == Fraction)
        {
            const auto a = Measure(node.children[0], size * .86f), b = Measure(node.children[1], size * .86f);
            const auto rule = baseline - size * .25f;
            Place(node.children[0], x + (box.width - a.width) / 2, rule - size * .2f - a.below, size * .86f);
            Place(node.children[1], x + (box.width - b.width) / 2, rule + size * .2f + b.above, size * .86f);
            Stroke(x + 2, rule, x + box.width - 2, rule);
            return;
        }
        if (node.kind == Root)
        {
            const auto index = Measure(node.children[1], size * .64f);
            const auto lead  = x + index.width;
            Place(node.children[0], lead + size * .7f, baseline, size);
            Place(node.children[1], x, baseline - size * .5f, size * .64f);
            const auto top = baseline - box.above + 2;
            Stroke(lead, baseline - size * .2f, lead + size * .18f, baseline - size * .3f);
            Stroke(lead + size * .18f, baseline - size * .3f, lead + size * .35f, baseline + box.below);
            Stroke(lead + size * .35f, baseline + box.below, lead + size * .62f, top);
            Stroke(lead + size * .62f, top, x + box.width, top);
            return;
        }
        if (node.kind == Scripts)
        {
            const auto base = Measure(node.children[0], size);
            Place(node.children[0], x, baseline, size);
            Place(node.children[1], x + base.width, baseline + size * .35f, size * .64f);
            Place(node.children[2], x + base.width, baseline - size * .55f, size * .64f);
            return;
        }
        if (node.kind == Accent)
        {
            Place(node.children[0], x, baseline, size);
            const float y = node.text == L"underline" ? baseline + box.below - 2 : baseline - box.above + size * .1f;
            if (node.text == L"hat" || node.text == L"widehat")
            {
                Stroke(x, y + size * .15f, x + box.width / 2, y);
                Stroke(x + box.width / 2, y, x + box.width, y + size * .15f);
            }
            else if (node.text == L"dot" || node.text == L"ddot")
            {
                Stroke(x + box.width / 2 - 1, y, x + box.width / 2 + 1, y);
                if (node.text == L"ddot")
                    Stroke(x + box.width / 2 + 4, y, x + box.width / 2 + 6, y);
            }
            else
            {
                Stroke(x, y, x + box.width, y);
                if (node.text == L"vec")
                {
                    Stroke(x + box.width - 4, y - 3, x + box.width, y);
                    Stroke(x + box.width - 4, y + 3, x + box.width, y);
                }
            }
            return;
        }
        if (node.kind == Matrix)
        {
            std::vector<float> widths(node.columns, 0.f);
            for (std::size_t index = 0; index < node.children.size(); ++index)
                widths[index % node.columns] = std::max(widths[index % node.columns], Measure(node.children[index], size).width);
            float y = baseline - box.above + size * .15f;
            for (std::size_t row = 0; row < node.children.size(); row += node.columns)
            {
                float above{}, below{}, cellX = x + size * .4f;
                for (unsigned col = 0; col < node.columns; ++col)
                {
                    const auto cell = Measure(node.children[row + col], size);
                    above           = std::max(above, cell.above);
                    below           = std::max(below, cell.below);
                }
                above = std::max(above, size * .8f);
                for (unsigned col = 0; col < node.columns; ++col)
                {
                    const float remaining = widths[col] - Measure(node.children[row + col], size).width;
                    const float offset    = node.text == L"aligned" ? (col % 2u == 0u ? remaining : 0.f) : node.text == L"cases" ? 0.f : remaining / 2.f;
                    Place(node.children[row + col], cellX + offset, y + above, size);
                    cellX += widths[col] + size * .65f;
                }
                y += std::max(size, above + below) + size * .3f;
            }
            const auto top = baseline - box.above + 1, bottom = baseline + box.below - 1;
            if (node.text != L"matrix" && node.text != L"smallmatrix" && node.text != L"aligned" && node.text != L"gathered")
            {
                const auto left  = node.text == L"pmatrix"                            ? L'('
                                   : node.text == L"Bmatrix" || node.text == L"cases" ? L'{'
                                   : node.text == L"vmatrix"                          ? L'|'
                                   : node.text == L"Vmatrix"                          ? L'‖'
                                                                                      : L'[';
                const auto right = left == L'(' ? L')' : left == L'{' ? L'}' : left == L'[' ? L']' : left;
                Fence(left, x + 2, top, bottom, size * .2f);
                if (node.text != L"cases")
                    Fence(right, x + box.width - size * .2f - 2, top, bottom, size * .2f);
            }
            return;
        }
        for (const auto& child : node.children)
        {
            Place(child, x, baseline, size);
            x += Measure(child, size).width;
        }
    }

public:
    PreparedMath(IDWriteFactory* write, std::shared_ptr<const MathExpression> value, float size)
        : expression(std::move(value)),
          factory(write),
          glyphs(0u),
          lines(0u),
          font(size)
    {
        Gather(expression->root, font, false);
    }
    bool Advance()
    {
        if (cursor < glyphs.size())
        {
            auto& leaf = glyphs[cursor++];
            wil::com_ptr<IDWriteTextFormat> format;
            THROW_IF_FAILED(factory->CreateTextFormat(L"Cambria Math",
                                                      nullptr,
                                                      leaf.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                                      leaf.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                                                      DWRITE_FONT_STRETCH_NORMAL,
                                                      leaf.font,
                                                      L"en-US",
                                                      format.put()));
            THROW_IF_FAILED(
                factory->CreateTextLayout(leaf.node->text.data(), static_cast<UINT32>(leaf.node->text.size()), format.get(), 65536, 65536, leaf.layout.put()));
            DWRITE_TEXT_METRICS metrics{};
            THROW_IF_FAILED(leaf.layout->GetMetrics(&metrics));
            DWRITE_LINE_METRICS line{};
            UINT32 count{};
            const auto hr = leaf.layout->GetLineMetrics(&line, 1, &count);
            if (FAILED(hr) || count != 1)
                throw std::length_error("Mathematical text leaf must be one line");
            leaf.extent = {metrics.widthIncludingTrailingWhitespace, line.baseline, metrics.height - line.baseline};
            return false;
        }
        extent = Measure(expression->root, font);
        if (extent.width > 65536 || extent.above + extent.below > 65536 || ! std::isfinite(extent.width + extent.above + extent.below))
            throw std::length_error("Mathematical geometry capacity exceeded");
        Place(expression->root, 0, extent.above, font);
        return true;
    }
    D2D1_SIZE_F Size() const noexcept
    {
        return {std::max(1.f, extent.width), std::max(1.f, extent.above + extent.below)};
    }
    float Baseline() const noexcept
    {
        return extent.above;
    }
    // Immutable measured geometry, also used by independent alignment oracles.
    const auto& GlyphLayouts() const noexcept
    {
        return glyphs;
    }
    void Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float scale) const noexcept
    {
        D2D1_MATRIX_3X2_F previous;
        target->GetTransform(&previous);
        target->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(origin.x, origin.y) * previous);
        for (const auto& glyph : glyphs)
            target->DrawTextLayout(glyph.origin, glyph.layout.get(), brush);
        for (const auto& line : lines)
            target->DrawLine(line.first, line.last, brush, std::max(1.f, font / 20));
        target->SetTransform(previous);
    }
};
} // namespace DxUi::RichText::Detail
