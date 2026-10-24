#pragma once
#include <DxUi/RichDocument.h>
#include <DxUi/RichMath.h>
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result.h>

namespace DxUi::RichText::Detail
{
// PreparedMath owns all shaping and geometry. The syntax tree is admitted and
// indexed once; Advance shapes one text leaf per call and finalizes from cached
// child boxes. Draw is read-only and performs no layout or allocation.
class PreparedMath final
{
    struct Extent
    {
        float width{}, above{}, below{};
    };
    struct Glyph
    {
        const MathNode* node{};
        std::size_t boxIndex{};
        float font{};
        bool bold{};
        bool italic{};
        wil::com_ptr<IDWriteTextLayout> layout;
        Extent extent;
        D2D1_POINT_2F origin{};
        float italicCorrection{};
        float leftOverhang{}, topOverhang{}, rightOverhang{}, bottomOverhang{};
    };
    struct Line
    {
        D2D1_POINT_2F first, last;
        float thickness{};
    };
    struct Box
    {
        const MathNode* node{};
        std::size_t firstChild{}, endChild{}, glyph{};
        Extent extent;
        float leftOffset{};
        MathAtomClass atomClass{MathAtomClass::Ordinary};
        float size{};
        unsigned scriptLevel{};
        bool display{};
    };
    std::shared_ptr<const MathExpression> expression;
    wil::com_ptr<IDWriteFactory> factory;
    std::vector<Glyph> glyphs;
    std::vector<Box> boxes;
    std::vector<Line> lines;
    std::size_t cursor{};
    float font{}, axis{};
    float scriptScale{.7f}, scriptScriptScale{.5f};
    float delimitedMinimum{1.5f}, displayOperatorMinimum{1.4f};
    float subscriptShift{.35f}, superscriptShift{.55f}, scriptGapMinimum{.18f};
    float fractionNumeratorShift{.45f}, fractionDenominatorShift{.45f}, fractionNumeratorGap{.12f}, fractionDenominatorGap{.12f};
    float fractionNumeratorDisplayShift{.6f}, fractionDenominatorDisplayShift{.6f}, fractionNumeratorDisplayGap{.2f}, fractionDenominatorDisplayGap{.2f};
    float fractionRuleThickness{1.f / 24.f}, upperLimitGap{.12f}, lowerLimitGap{.12f};
    Extent extent;
    bool displayStyle{}, finalized{};

    static std::int16_t Signed16(const BYTE* bytes) noexcept
    {
        return static_cast<std::int16_t>((static_cast<unsigned>(bytes[0]) << 8u) | bytes[1]);
    }
    void InitializeFallbackConstants() noexcept
    {
        subscriptShift *= font;
        superscriptShift *= font;
        scriptGapMinimum *= font;
        fractionNumeratorShift *= font;
        fractionDenominatorShift *= font;
        fractionNumeratorGap *= font;
        fractionDenominatorGap *= font;
        fractionNumeratorDisplayShift *= font;
        fractionDenominatorDisplayShift *= font;
        fractionNumeratorDisplayGap *= font;
        fractionDenominatorDisplayGap *= font;
        fractionRuleThickness *= font;
        upperLimitGap *= font;
        lowerLimitGap *= font;
    }
    void ReadMathConstants(IDWriteFactory* write) noexcept
    {
        // A missing, truncated, or implausible MATH table uses deterministic
        // em-relative constants. Only the bounded header/constants prefix is read.
        wil::com_ptr<IDWriteFontCollection> collection;
        if (FAILED(write->GetSystemFontCollection(collection.put(), FALSE)))
            return;
        UINT32 familyIndex{};
        BOOL exists{};
        if (FAILED(collection->FindFamilyName(L"Cambria Math", &familyIndex, &exists)) || ! exists)
            return;
        wil::com_ptr<IDWriteFontFamily> family;
        if (FAILED(collection->GetFontFamily(familyIndex, family.put())))
            return;
        wil::com_ptr<IDWriteFont> fontObject;
        if (FAILED(family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, fontObject.put())))
            return;
        wil::com_ptr<IDWriteFontFace> face;
        if (FAILED(fontObject->CreateFontFace(face.put())))
            return;
        const void* table{};
        UINT32 size{};
        void* context{};
        BOOL existsTable{};
        if (FAILED(face->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('M', 'A', 'T', 'H'), &table, &size, &context, &existsTable)) || ! existsTable)
            return;
        auto release      = wil::scope_exit([&] { face->ReleaseFontTable(context); });
        const auto* bytes = static_cast<const BYTE*>(table);
        if (! bytes || size < 26 || bytes[0] != 0 || bytes[1] != 1)
            return;
        const auto offset = static_cast<unsigned>((bytes[4] << 8u) | bytes[5]);
        if (offset == 0 || offset > size || size - offset < 16u)
            return;
        // MathConstants starts with the two percentage scales, two unsigned
        // heights, then MathValueRecords. AxisHeight follows MathLeading.
        const auto* constants     = bytes + offset;
        const auto constantsSize  = size - offset;
        const auto first          = Signed16(constants);
        const auto second         = Signed16(constants + 2);
        const auto delimitedUnits = static_cast<unsigned>((constants[4] << 8u) | constants[5]);
        const auto operatorUnits  = static_cast<unsigned>((constants[6] << 8u) | constants[7]);
        const auto axisUnits      = Signed16(constants + 12);
        DWRITE_FONT_METRICS metrics{};
        face->GetMetrics(&metrics);
        if (metrics.designUnitsPerEm == 0)
            return;
        if (first >= 30 && first <= 100)
            scriptScale = static_cast<float>(first) / 100.f;
        if (second >= 20 && second <= 100)
            scriptScriptScale = static_cast<float>(second) / 100.f;
        const auto delimitedScale = static_cast<float>(delimitedUnits) / static_cast<float>(metrics.designUnitsPerEm);
        const auto operatorScale  = static_cast<float>(operatorUnits) / static_cast<float>(metrics.designUnitsPerEm);
        if (std::isfinite(delimitedScale) && delimitedScale >= 1.f && delimitedScale <= 4.f)
            delimitedMinimum = delimitedScale;
        if (std::isfinite(operatorScale) && operatorScale >= 1.f && operatorScale <= 4.f)
            displayOperatorMinimum = operatorScale;
        const auto readDesignValue = [&](unsigned valueOffset, float fallback)
        {
            if (valueOffset + 2u > constantsSize)
                return fallback;
            const auto value  = Signed16(constants + valueOffset);
            const auto scaled = font * static_cast<float>(value) / static_cast<float>(metrics.designUnitsPerEm);
            return std::isfinite(scaled) && scaled >= 0.f && scaled <= font * 2.f ? scaled : fallback;
        };
        subscriptShift                  = readDesignValue(24u, subscriptShift);
        superscriptShift                = readDesignValue(36u, superscriptShift);
        scriptGapMinimum                = readDesignValue(52u, scriptGapMinimum);
        upperLimitGap                   = readDesignValue(64u, upperLimitGap);
        lowerLimitGap                   = readDesignValue(72u, lowerLimitGap);
        fractionNumeratorShift          = readDesignValue(120u, fractionNumeratorShift);
        fractionNumeratorDisplayShift   = readDesignValue(124u, fractionNumeratorDisplayShift);
        fractionDenominatorShift        = readDesignValue(128u, fractionDenominatorShift);
        fractionDenominatorDisplayShift = readDesignValue(132u, fractionDenominatorDisplayShift);
        fractionNumeratorGap            = readDesignValue(136u, fractionNumeratorGap);
        fractionNumeratorDisplayGap     = readDesignValue(140u, fractionNumeratorDisplayGap);
        fractionRuleThickness           = readDesignValue(144u, fractionRuleThickness);
        fractionDenominatorGap          = readDesignValue(148u, fractionDenominatorGap);
        fractionDenominatorDisplayGap   = readDesignValue(152u, fractionDenominatorDisplayGap);
        const auto candidateAxis        = font * static_cast<float>(axisUnits) / static_cast<float>(metrics.designUnitsPerEm);
        if (std::isfinite(candidateAxis) && candidateAxis > 0 && candidateAxis < font)
            axis = candidateAxis;
    }
    static std::size_t CountAndValidate(const MathNode& root)
    {
        struct Pending
        {
            const MathNode* node;
            unsigned depth;
        };
        std::array<Pending, 512u> pending{};
        std::size_t top{}, count{};
        pending[top++] = {&root, 0u};
        while (top != 0u)
        {
            const auto current = pending[--top];
            const auto& node   = *current.node;
            if (++count > 512u || current.depth > 24u || node.children.size() > 512u - top || node.text.size() > MathMaximumSourceUnits ||
                ! IsValidUtf16(node.text))
                throw std::length_error("Mathematical syntax capacity exceeded");
            switch (node.kind)
            {
                case MathKind::Row: break;
                case MathKind::Text:
                    if (node.children.size() != 0u)
                        throw std::length_error("Invalid mathematical text node");
                    break;
                case MathKind::Space:
                    if (! node.children.empty() || (node.text != L"" && node.text != L"quad" && node.text != L"qquad" && node.text != L"," &&
                                                    node.text != L":" && node.text != L";" && node.text != L"!" && node.text != L" "))
                        throw std::length_error("Invalid mathematical space node");
                    break;
                case MathKind::Fraction:
                    if (node.text != L"frac" && node.text != L"dfrac" && node.text != L"tfrac")
                        throw std::length_error("Invalid mathematical fraction style");
                    if (node.children.size() != 2u)
                        throw std::length_error("Invalid mathematical pair node");
                    break;
                case MathKind::Root:
                    if (node.children.size() != 2u)
                        throw std::length_error("Invalid mathematical pair node");
                    break;
                case MathKind::Scripts:
                    if (node.children.size() != 3u)
                        throw std::length_error("Invalid mathematical scripts node");
                    break;
                case MathKind::Matrix:
                    if (node.columns == 0u || node.columns > 16u || node.children.empty() || node.children.size() > 128u ||
                        node.children.size() % node.columns != 0u || node.children.size() / node.columns > 16u ||
                        (node.text != L"matrix" && node.text != L"smallmatrix" && node.text != L"pmatrix" && node.text != L"bmatrix" &&
                         node.text != L"Bmatrix" && node.text != L"vmatrix" && node.text != L"Vmatrix" && node.text != L"cases" && node.text != L"aligned" &&
                         node.text != L"gathered"))
                        throw std::length_error("Mathematical matrix capacity exceeded");
                    break;
                case MathKind::Accent:
                    if (node.text != L"overline" && node.text != L"underline" && node.text != L"bar" && node.text != L"hat" && node.text != L"widehat" &&
                        node.text != L"tilde" && node.text != L"widetilde" && node.text != L"vec" && node.text != L"dot" && node.text != L"ddot")
                        throw std::length_error("Invalid mathematical accent");
                    if (node.children.size() != 1u)
                        throw std::length_error("Invalid mathematical unary node");
                    break;
                case MathKind::Style:
                    if (node.text != L"mathrm" && node.text != L"mathbf" && node.text != L"mathit" && node.text != L"mathbb" && node.text != L"mathcal" &&
                        node.text != L"mathord" && node.text != L"mathop" && node.text != L"mathbin" && node.text != L"mathrel" && node.text != L"mathopen" &&
                        node.text != L"mathclose" && node.text != L"mathpunct" && node.text != L"mathinner" && node.text != L"displaystyle" &&
                        node.text != L"textstyle" && node.text != L"scriptstyle" && node.text != L"scriptscriptstyle")
                        throw std::length_error("Invalid mathematical style");
                    if (node.children.size() != 1u)
                        throw std::length_error("Invalid mathematical unary node");
                    break;
                case MathKind::Delimited:
                    if (node.kind == MathKind::Delimited &&
                        (node.text.size() != 2u || std::wstring_view(L"()[]{}|.⟨⟩‖⌊⌋⌈⌉").find(node.text[0]) == std::wstring_view::npos ||
                         std::wstring_view(L"()[]{}|.⟨⟩‖⌊⌋⌈⌉").find(node.text[1]) == std::wstring_view::npos))
                        throw std::length_error("Invalid mathematical fence pair");
                    if (node.children.size() != 1u)
                        throw std::length_error("Invalid mathematical unary node");
                    break;
                case MathKind::Delimiter:
                    if (! node.children.empty() || node.text.size() != 1u ||
                        std::wstring_view(L"()[]{}|.⟨⟩‖⌊⌋⌈⌉").find(node.text[0]) == std::wstring_view::npos || node.delimiterSize < 1u ||
                        node.delimiterSize > 4u)
                        throw std::length_error("Invalid sized mathematical delimiter");
                    break;
                default: throw std::length_error("Unknown mathematical node kind");
            }
            switch (node.atomClass)
            {
                case MathAtomClass::Ordinary:
                case MathAtomClass::Operator:
                case MathAtomClass::Binary:
                case MathAtomClass::Relation:
                case MathAtomClass::Opening:
                case MathAtomClass::Closing:
                case MathAtomClass::Punctuation:
                case MathAtomClass::Inner: break;
                default: throw std::length_error("Unknown mathematical atom class");
            }
            if (node.limits != MathLimits::Automatic && node.limits != MathLimits::AboveBelow && node.limits != MathLimits::Side)
                throw std::length_error("Unknown mathematical limits mode");
            for (const auto& child : node.children)
                pending[top++] = {&child, current.depth + 1u};
        }
        return count;
    }
    float StyleScale(unsigned level) const noexcept
    {
        if (level == 0u)
            return 1.f;
        return level == 1u ? scriptScale : scriptScriptScale;
    }
    static bool VisibleDelimiter(wchar_t shape) noexcept
    {
        return shape != L'.';
    }
    std::size_t Flatten(const MathNode& node, float size, bool bold, bool italic, bool forceItalic, bool display, unsigned scriptLevel)
    {
        const auto index = boxes.size();
        boxes.push_back({&node, 0u, 0u, std::numeric_limits<std::size_t>::max(), {}, 0.f, node.atomClass, size, scriptLevel, display});
        if (node.kind == MathKind::Text)
        {
            if (node.text.empty())
            {
                boxes[index].endChild = boxes.size();
                return index;
            }
            if (node.largeOperator && display)
                size = std::max(size, font * displayOperatorMinimum);
            const auto glyphIndex = glyphs.size();
            glyphs.push_back({&node, index, size, bold, forceItalic ? italic : italic || node.italic, {}, {}, {}});
            boxes[index].glyph    = glyphIndex;
            boxes[index].endChild = boxes.size();
            return index;
        }
        boxes[index].firstChild = boxes.size();
        const auto firstChild   = boxes[index].firstChild;
        const auto nextLevel    = scriptLevel < 2u ? scriptLevel + 1u : 2u;
        for (std::size_t childIndex = 0; childIndex < node.children.size(); ++childIndex)
        {
            const auto& child = node.children[childIndex];
            float childSize   = size;
            bool childBold = bold, childItalic = italic, childDisplay = display;
            bool childForceItalic = forceItalic;
            auto childLevel       = scriptLevel;
            if (node.kind == MathKind::Style)
            {
                if (node.text == L"mathbf")
                {
                    childBold        = true;
                    childItalic      = false;
                    childForceItalic = true;
                }
                if (node.text == L"mathrm" || node.text == L"mathbb" || node.text == L"mathcal")
                {
                    childItalic      = false;
                    childForceItalic = true;
                }
                else if (node.text == L"mathit")
                {
                    childItalic      = true;
                    childForceItalic = true;
                }
                if (node.text == L"displaystyle" || node.text == L"textstyle" || node.text == L"scriptstyle" || node.text == L"scriptscriptstyle")
                {
                    const unsigned level = node.text == L"displaystyle" || node.text == L"textstyle" ? 0u : node.text == L"scriptstyle" ? 1u : 2u;
                    childSize            = font * StyleScale(level);
                    childLevel           = level;
                    childDisplay         = node.text == L"displaystyle";
                }
            }
            else if (node.kind == MathKind::Fraction)
            {
                auto fractionDisplay = display;
                if (node.text == L"dfrac")
                    fractionDisplay = true;
                else if (node.text == L"tfrac")
                    fractionDisplay = false;
                boxes[index].display = fractionDisplay;
                childDisplay         = false; // Fraction contents use text or script style, not display style.
                childLevel           = fractionDisplay ? 0u : nextLevel;
                childSize            = font * StyleScale(childLevel);
            }
            else if (node.kind == MathKind::Scripts && childIndex != 0u)
            {
                childLevel   = nextLevel;
                childSize    = font * StyleScale(childLevel);
                childDisplay = false;
            }
            else if (node.kind == MathKind::Root)
            {
                if (childIndex == 1u)
                {
                    childLevel = 2u;
                    childSize  = font * StyleScale(childLevel);
                }
            }
            else if (node.kind == MathKind::Matrix)
            {
                childDisplay         = false;
                boxes[index].display = false;
                if (node.text == L"smallmatrix")
                {
                    childLevel               = std::max(1u, scriptLevel);
                    childSize                = font * StyleScale(childLevel);
                    boxes[index].size        = childSize;
                    boxes[index].scriptLevel = childLevel;
                    boxes[index].display     = false;
                }
            }
            else if (node.kind == MathKind::Delimiter)
                childSize = size;
            Flatten(child, childSize, childBold, childItalic, childForceItalic, childDisplay, childLevel);
        }
        boxes[index].firstChild = firstChild;
        boxes[index].endChild   = boxes.size();
        return index;
    }
    std::size_t NextSibling(std::size_t index) const noexcept
    {
        return boxes[index].endChild;
    }
    std::size_t ChildAt(std::size_t parent, std::size_t offset) const noexcept
    {
        auto child = boxes[parent].firstChild;
        while (offset-- != 0u)
            child = NextSibling(child);
        return child;
    }
    float SpaceWidth(const MathNode& node, float size) const noexcept
    {
        if (node.text.empty())
            return 0.f;
        if (node.text == L"quad")
            return size;
        if (node.text == L"qquad")
            return size * 2.f;
        if (node.text == L"!")
            return -size * 3.f / 18.f;
        if (node.text == L",")
            return size * 3.f / 18.f;
        if (node.text == L":")
            return size * 4.f / 18.f;
        if (node.text == L";")
            return size * 5.f / 18.f;
        return size / 3.f;
    }
    float ItalicCorrection(std::size_t index) const noexcept
    {
        const auto& box = boxes[index];
        if (box.glyph != std::numeric_limits<std::size_t>::max())
            return glyphs[box.glyph].italic ? glyphs[box.glyph].italicCorrection : 0.f;
        for (auto leaf = box.endChild; leaf-- > index;)
            if (boxes[leaf].node->kind == MathKind::Text && boxes[leaf].glyph != std::numeric_limits<std::size_t>::max())
            {
                const auto& glyph = glyphs[boxes[leaf].glyph];
                return glyph.italic ? glyph.italicCorrection : 0.f;
            }
        return 0.f;
    }
    float ScriptBaseAdvance(std::size_t index) const noexcept
    {
        const auto& box = boxes[index];
        if (box.glyph != std::numeric_limits<std::size_t>::max())
        {
            const auto& glyph = glyphs[box.glyph];
            // Ink overhang reserves bounds, but is not part of the shaped advance.
            // Only the superscript receives the base's italic correction.
            return box.leftOffset + glyph.extent.width;
        }
        return box.extent.width;
    }
    float ScriptAnchor(std::size_t index) const noexcept
    {
        return ScriptBaseAdvance(index) + ItalicCorrection(index);
    }
    float AtomSpace(MathAtomClass left, MathAtomClass right, float size, unsigned styleLevel) const noexcept
    {
        // Class pairs and tight-script entries follow KaTeX v0.16.11's TeX
        // spacing data, expressed here as a bounded native lookup.
        using enum MathAtomClass;
        unsigned mu{};
        if (left == Ordinary)
            mu = right == Operator ? 3u : right == Binary ? 4u : right == Relation ? 5u : right == Inner ? 3u : 0u;
        else if (left == Operator)
            mu = right == Ordinary || right == Operator ? 3u : right == Relation ? 5u : right == Inner ? 3u : 0u;
        else if (left == Binary)
            mu = right == Ordinary || right == Operator || right == Opening || right == Inner ? 4u : 0u;
        else if (left == Relation)
            mu = right == Ordinary || right == Operator || right == Opening || right == Inner ? 5u : 0u;
        else if (left == Closing)
            mu = right == Operator ? 3u : right == Binary ? 4u : right == Relation ? 5u : right == Inner ? 3u : 0u;
        else if (left == Punctuation)
            mu = right == Ordinary || right == Operator || right == Opening || right == Closing || right == Punctuation || right == Inner ? 3u
                 : right == Relation                                                                                                      ? 5u
                                                                                                                                          : 0u;
        else if (left == Inner)
            mu = right == Ordinary || right == Operator || right == Opening || right == Punctuation || right == Inner ? 3u
                 : right == Binary                                                                                    ? 4u
                 : right == Relation                                                                                  ? 5u
                                                                                                                      : 0u;
        if (styleLevel != 0u)
        {
            const bool thin = (left == Ordinary && right == Operator) || (left == Operator && (right == Ordinary || right == Operator)) ||
                              (left == Closing && right == Operator) || (left == Inner && right == Operator);
            if (! thin)
                return 0.f;
        }
        return size * static_cast<float>(mu) / 18.f;
    }
    void NormalizeRows() noexcept
    {
        std::array<std::size_t, 512u> children{};
        std::array<MathAtomClass, 512u> original{}, next{};
        std::array<bool, 512u> hasNext{};
        for (std::size_t rowIndex = 0u; rowIndex < boxes.size(); ++rowIndex)
        {
            const auto& row = boxes[rowIndex];
            if (row.node->kind != MathKind::Row)
                continue;
            std::size_t count{};
            for (auto child = row.firstChild; child < row.endChild; child = NextSibling(child))
            {
                children[count] = child;
                original[count] = boxes[child].atomClass;
                ++count;
            }
            bool hasRight{};
            MathAtomClass right{};
            for (std::size_t index = count; index-- > 0u;)
            {
                hasNext[index] = hasRight;
                next[index]    = right;
                if (boxes[children[index]].node->kind != MathKind::Space)
                {
                    right    = original[index];
                    hasRight = true;
                }
            }
            bool hasLeft{};
            MathAtomClass left{};
            for (std::size_t index = 0u; index < count; ++index)
            {
                auto value = original[index];
                if (value == MathAtomClass::Binary &&
                    (! hasLeft || ! hasNext[index] || left == MathAtomClass::Binary || left == MathAtomClass::Relation || left == MathAtomClass::Operator ||
                     left == MathAtomClass::Opening || left == MathAtomClass::Punctuation || next[index] == MathAtomClass::Relation ||
                     next[index] == MathAtomClass::Closing || next[index] == MathAtomClass::Punctuation))
                    value = MathAtomClass::Ordinary;
                if (boxes[children[index]].node->kind != MathKind::Space)
                {
                    boxes[children[index]].atomClass = value;
                    left                             = value;
                    hasLeft                          = true;
                }
            }
        }
    }
    void MeasureBoxes()
    {
        NormalizeRows();
        for (std::size_t reverse = boxes.size(); reverse-- > 0u;)
        {
            auto& box        = boxes[reverse];
            box.extent       = {};
            const auto& node = *box.node;
            using enum MathKind;
            if (node.kind == Text)
            {
                if (node.text.empty())
                {
                    box.extent = {};
                    continue;
                }
                const auto& glyph = glyphs[box.glyph];
                box.extent        = glyph.extent;
                box.extent.width += box.leftOffset + glyph.rightOverhang;
                box.extent.above += glyph.topOverhang;
                box.extent.below += glyph.bottomOverhang;
                continue;
            }
            if (node.kind == Space)
            {
                box.extent.width = SpaceWidth(node, box.size);
                continue;
            }
            const auto child = [&](std::size_t offset) { return boxes[ChildAt(reverse, offset)].extent; };
            if (node.kind == Fraction && node.children.size() == 2u)
            {
                const auto top = child(0u), bottom = child(1u);
                const float scale     = box.size / font;
                const float rule      = std::max(1.f, fractionRuleThickness * scale);
                const float topGap    = (box.display ? fractionNumeratorDisplayGap : fractionNumeratorGap) * scale;
                const float bottomGap = (box.display ? fractionDenominatorDisplayGap : fractionDenominatorGap) * scale;
                const float topShift =
                    std::max((box.display ? fractionNumeratorDisplayShift : fractionNumeratorShift) * scale, axis * scale + top.below + topGap + rule * .5f);
                const float bottomShift = std::max((box.display ? fractionDenominatorDisplayShift : fractionDenominatorShift) * scale,
                                                   bottom.above + bottomGap + rule * .5f - axis * scale);
                box.extent              = {std::max(top.width, bottom.width) + box.size * .4f, topShift + top.above, bottomShift + bottom.below};
                continue;
            }
            if (node.kind == Root && node.children.size() == 2u)
            {
                const auto inner = child(0u), index = child(1u);
                box.extent = {inner.width + box.size * .7f + index.width,
                              std::max(inner.above + box.size * .22f, index.above + index.below + box.size * .3f),
                              inner.below};
                continue;
            }
            if (node.kind == Scripts && node.children.size() == 3u)
            {
                const auto base = child(0u), sub = child(1u), super = child(2u);
                const auto& baseNode = node.children[0u];
                const bool limitsAboveBelow =
                    (baseNode.limits == MathLimits::AboveBelow) || (baseNode.limits == MathLimits::Automatic && baseNode.displayLimits && box.display);
                if (limitsAboveBelow && baseNode.atomClass == MathAtomClass::Operator)
                {
                    const float scale = box.size / font;
                    box.extent        = {std::max({base.width, sub.width, super.width}) + box.size * .1f,
                                         base.above + (super.width > 0.f ? super.below + super.above + upperLimitGap * scale : 0.f),
                                         base.below + (sub.width > 0.f ? sub.above + sub.below + lowerLimitGap * scale : 0.f)};
                    continue;
                }
                const float scale        = box.size / font;
                const float scriptGap    = scriptGapMinimum * scale;
                const float subShift     = std::max(subscriptShift * scale, base.below + box.size * .12f);
                const float superShift   = std::max(superscriptShift * scale, base.above - box.size * .08f);
                const float availableGap = superShift + subShift - super.below - sub.above;
                const float extra        = std::max(0.f, (scriptGap - availableGap) / 2.f);
                const auto baseIndex     = ChildAt(reverse, 0u);
                const auto scriptAnchor  = ScriptAnchor(baseIndex);
                box.extent               = {std::max({base.width, ScriptBaseAdvance(baseIndex) + sub.width, scriptAnchor + super.width}),
                                            std::max(base.above, super.above + superShift + extra),
                                            std::max(base.below, sub.below + subShift + extra)};
                continue;
            }
            if (node.kind == Matrix && node.columns > 0u)
            {
                std::array<float, 16u> widths{};
                float totalHeight{};
                std::size_t ci{};
                for (auto cellIndex = box.firstChild; cellIndex < box.endChild; cellIndex = NextSibling(cellIndex), ++ci)
                    widths[ci % node.columns] = std::max(widths[ci % node.columns], boxes[cellIndex].extent.width);
                for (std::size_t row = 0u; row < node.children.size(); row += node.columns)
                {
                    float above{}, below{};
                    for (unsigned col = 0u; col < node.columns; ++col)
                    {
                        const auto cell = child(row + col);
                        above           = std::max(above, cell.above);
                        below           = std::max(below, cell.below);
                    }
                    totalHeight += std::max(box.size, above + below) + box.size * .3f;
                }
                const bool fenced        = node.text != L"matrix" && node.text != L"smallmatrix" && node.text != L"aligned" && node.text != L"gathered";
                const float leftPadding  = fenced ? box.size * .4f : 0.f;
                const float rightPadding = fenced && node.text != L"cases" ? box.size * .4f : 0.f;
                const auto width         = std::accumulate(widths.begin(), widths.begin() + node.columns, 0.f) +
                                           box.size * .65f * static_cast<float>(node.columns - 1u) + leftPadding + rightPadding;
                box.extent               = {width, totalHeight * .5f + box.size * .25f, std::max(0.f, totalHeight * .5f - box.size * .25f)};
                continue;
            }
            if (node.kind == Accent && node.children.size() == 1u)
            {
                box.extent = child(0u);
                if (node.text == L"underline")
                    box.extent.below += box.size * .2f;
                else
                    box.extent.above += box.size * .3f;
                continue;
            }
            if (node.kind == Delimited && node.children.size() == 1u)
            {
                box.extent             = child(0u);
                const bool leftVisible = VisibleDelimiter(node.text[0u]), rightVisible = VisibleDelimiter(node.text[1u]);
                box.extent.width += box.size * .4f * static_cast<float>(leftVisible + rightVisible);
                if (leftVisible || rightVisible)
                {
                    const float minimumHeight = box.size * delimitedMinimum;
                    const float axisOffset    = axis * box.size / font;
                    box.extent.above          = std::max(box.extent.above, minimumHeight * .5f + axisOffset);
                    box.extent.below          = std::max(box.extent.below, std::max(0.f, minimumHeight * .5f - axisOffset));
                }
                continue;
            }
            if (node.kind == Delimiter)
            {
                if (! VisibleDelimiter(node.text[0u]))
                {
                    box.extent = {};
                    continue;
                }
                const float height     = box.size * (0.9f + 0.3f * static_cast<float>(node.delimiterSize));
                const float axisOffset = axis * box.size / font;
                box.extent             = {box.size * (node.delimiterSize == 1u   ? .45f
                                                      : node.delimiterSize == 2u ? .65f
                                                      : node.delimiterSize == 3u ? .8f
                                                                                 : 1.f),
                                          height * .5f + axisOffset,
                                          std::max(0.f, height * .5f - axisOffset)};
                continue;
            }
            if (node.kind == Row)
            {
                float advance{}, minAdvance{}, maxRight{};
                auto previous = box.endChild;
                for (auto index = box.firstChild; index < box.endChild; index = NextSibling(index))
                {
                    if (previous < box.endChild && boxes[index].node->kind != MathKind::Space)
                        advance += AtomSpace(boxes[previous].atomClass, boxes[index].atomClass, box.size, box.scriptLevel);
                    minAdvance = std::min(minAdvance, advance);
                    if (boxes[index].node->kind != MathKind::Space)
                        maxRight = std::max(maxRight, advance + boxes[index].extent.width);
                    advance += boxes[index].extent.width;
                    minAdvance       = std::min(minAdvance, advance);
                    box.extent.above = std::max(box.extent.above, boxes[index].extent.above);
                    box.extent.below = std::max(box.extent.below, boxes[index].extent.below);
                    if (boxes[index].node->kind != MathKind::Space)
                        previous = index;
                }
                box.leftOffset   = -minAdvance;
                box.extent.width = std::max(advance, maxRight) - minAdvance;
                continue;
            }
            for (auto index = box.firstChild; index < box.endChild; index = NextSibling(index))
            {
                auto childExtent = boxes[index].extent;
                box.extent.width += childExtent.width;
                box.extent.above = std::max(box.extent.above, childExtent.above);
                box.extent.below = std::max(box.extent.below, childExtent.below);
            }
            if (node.kind == Style && node.children.size() == 1u)
                box.extent = child(0u);
        }
    }
    void Stroke(float x1, float y1, float x2, float y2, float thickness)
    {
        lines.push_back({{x1, y1}, {x2, y2}, thickness});
    }
    void Fence(wchar_t shape, float x, float top, float bottom, float width, float thickness)
    {
        if (shape == L'.')
            return;
        const bool right  = shape == L')' || shape == L']' || shape == L'}' || shape == L'⟩' || shape == L'⌋' || shape == L'⌉';
        const auto outer  = right ? x + width : x;
        const auto inner  = right ? x : x + width;
        const auto middle = (top + bottom) / 2.f;
        if (shape == L'(' || shape == L')')
        {
            Stroke(inner, top, outer, top + (bottom - top) * .2f, thickness);
            Stroke(outer, top + (bottom - top) * .2f, outer, bottom - (bottom - top) * .2f, thickness);
            Stroke(outer, bottom - (bottom - top) * .2f, inner, bottom, thickness);
        }
        else if (shape == L'{' || shape == L'}')
        {
            Stroke(inner, top, outer, top + (bottom - top) * .15f, thickness);
            Stroke(outer, top + (bottom - top) * .15f, outer, middle - 2.f, thickness);
            Stroke(outer, middle - 2.f, inner, middle, thickness);
            Stroke(inner, middle, outer, middle, thickness);
            Stroke(outer, middle, outer, bottom - (bottom - top) * .15f, thickness);
            Stroke(outer, bottom - (bottom - top) * .15f, inner, bottom, thickness);
        }
        else if (shape == L'⟨' || shape == L'⟩')
        {
            Stroke(inner, top, outer, middle, thickness);
            Stroke(outer, middle, inner, bottom, thickness);
        }
        else if (shape == L'⌊' || shape == L'⌋' || shape == L'⌈' || shape == L'⌉')
        {
            Stroke(outer, top, outer, bottom, thickness);
            if (shape == L'⌊' || shape == L'⌋')
                Stroke(outer, bottom, inner, bottom, thickness);
            else
                Stroke(outer, top, inner, top, thickness);
        }
        else
        {
            Stroke(outer, top, outer, bottom, thickness);
            if (shape == L'‖')
                Stroke(outer + (right ? -3.f : 3.f), top, outer + (right ? -3.f : 3.f), bottom, thickness);
            else if (shape != L'|')
            {
                Stroke(outer, top, inner, top, thickness);
                Stroke(outer, bottom, inner, bottom, thickness);
            }
        }
    }
    void Place(std::size_t index, float x, float baseline)
    {
        const auto& box  = boxes[index];
        const auto& node = *box.node;
        x += box.leftOffset;
        const float stroke = std::max(.5f, box.size / 20.f);
        using enum MathKind;
        if (node.kind == Text)
        {
            if (node.text.empty())
                return;
            auto& glyph  = glyphs[box.glyph];
            glyph.origin = {x, baseline - glyph.extent.above};
            return;
        }
        if (node.kind == Space)
            return;
        if (node.kind == Fraction && node.children.size() == 2u)
        {
            const auto top = boxes[ChildAt(index, 0u)].extent, bottom = boxes[ChildAt(index, 1u)].extent;
            const float scale      = box.size / font;
            const float scaledRule = std::max(1.f, fractionRuleThickness * scale);
            const float topGap     = (box.display ? fractionNumeratorDisplayGap : fractionNumeratorGap) * scale;
            const float bottomGap  = (box.display ? fractionDenominatorDisplayGap : fractionDenominatorGap) * scale;
            const float topShift =
                std::max((box.display ? fractionNumeratorDisplayShift : fractionNumeratorShift) * scale, axis * scale + top.below + topGap + scaledRule * .5f);
            const float bottomShift = std::max((box.display ? fractionDenominatorDisplayShift : fractionDenominatorShift) * scale,
                                               bottom.above + bottomGap + scaledRule * .5f - axis * scale);
            const float ruleY       = baseline - axis * scale;
            Place(ChildAt(index, 0u), x + (box.extent.width - top.width) / 2.f, baseline - topShift);
            Place(ChildAt(index, 1u), x + (box.extent.width - bottom.width) / 2.f, baseline + bottomShift);
            Stroke(x + 2.f, ruleY, x + box.extent.width - 2.f, ruleY, scaledRule);
            return;
        }
        if (node.kind == Root && node.children.size() == 2u)
        {
            const auto indexBox = ChildAt(index, 1u);
            const auto lead     = x + boxes[indexBox].extent.width;
            Place(ChildAt(index, 0u), lead + box.size * .7f, baseline);
            Place(indexBox, x, baseline - box.size * .5f);
            const auto top = baseline - box.extent.above + 2.f;
            Stroke(lead, baseline - box.size * .2f, lead + box.size * .18f, baseline - box.size * .3f, stroke);
            Stroke(lead + box.size * .18f, baseline - box.size * .3f, lead + box.size * .35f, baseline + box.extent.below, stroke);
            Stroke(lead + box.size * .35f, baseline + box.extent.below, lead + box.size * .62f, top, stroke);
            Stroke(lead + box.size * .62f, top, x + box.extent.width, top, stroke);
            return;
        }
        if (node.kind == Scripts && node.children.size() == 3u)
        {
            const auto baseIndex = ChildAt(index, 0u), sub = ChildAt(index, 1u), super = ChildAt(index, 2u);
            const auto base      = boxes[baseIndex].extent;
            const auto& baseNode = node.children[0u];
            const bool limitsAboveBelow =
                (baseNode.limits == MathLimits::AboveBelow) || (baseNode.limits == MathLimits::Automatic && baseNode.displayLimits && box.display);
            if (limitsAboveBelow && baseNode.atomClass == MathAtomClass::Operator)
            {
                const float center           = x + box.extent.width / 2.f;
                const float baseX            = center - base.width / 2.f;
                const float scale            = box.size / font;
                const float baseAboveShift   = base.above + upperLimitGap * scale;
                const float baseBelowShift   = base.below + lowerLimitGap * scale;
                const float italicCorrection = ItalicCorrection(baseIndex) * .75f;
                Place(baseIndex, baseX, baseline);
                if (boxes[super].extent.width > 0.f)
                    Place(super, center - boxes[super].extent.width / 2.f + italicCorrection / 2.f, baseline - baseAboveShift - boxes[super].extent.below);
                if (boxes[sub].extent.width > 0.f)
                    Place(sub, center - boxes[sub].extent.width / 2.f - italicCorrection / 2.f, baseline + baseBelowShift + boxes[sub].extent.above);
                return;
            }
            const float scale      = box.size / font;
            const float subShift   = std::max(subscriptShift * scale, base.below + box.size * .12f);
            const float superShift = std::max(superscriptShift * scale, base.above - box.size * .08f);
            const float gapNeed    = scriptGapMinimum * scale - (superShift + subShift - boxes[super].extent.below - boxes[sub].extent.above);
            const float correction = std::max(0.f, gapNeed / 2.f);
            Place(baseIndex, x, baseline);
            Place(sub, x + ScriptBaseAdvance(baseIndex), baseline + subShift + correction);
            Place(super, x + ScriptAnchor(baseIndex), baseline - superShift - correction);
            return;
        }
        if (node.kind == Delimited && node.children.size() == 1u)
        {
            const float width      = box.size * .25f;
            const bool leftVisible = VisibleDelimiter(node.text[0u]), rightVisible = VisibleDelimiter(node.text[1u]);
            const float leftAdvance  = leftVisible ? box.size * .4f : 0.f;
            const float rightAdvance = rightVisible ? box.size * .4f : 0.f;
            const float top = baseline - box.extent.above, bottom = baseline + box.extent.below;
            Place(ChildAt(index, 0u), x + leftAdvance, baseline);
            if (leftVisible)
                Fence(node.text[0u], x, top, bottom, width, stroke);
            if (rightVisible)
                Fence(node.text[1u], x + box.extent.width - rightAdvance, top, bottom, width, stroke);
            return;
        }
        if (node.kind == Delimiter)
        {
            const auto width = box.extent.width;
            Fence(node.text.empty() ? L'.' : node.text[0], x, baseline - box.extent.above, baseline + box.extent.below, width, stroke);
            return;
        }
        if (node.kind == Accent && node.children.size() == 1u)
        {
            Place(ChildAt(index, 0u), x, baseline);
            const float y = node.text == L"underline" ? baseline + box.extent.below - 2.f : baseline - box.extent.above + box.size * .1f;
            if (node.text == L"hat" || node.text == L"widehat")
            {
                Stroke(x, y + box.size * .15f, x + box.extent.width / 2.f, y, stroke);
                Stroke(x + box.extent.width / 2.f, y, x + box.extent.width, y + box.size * .15f, stroke);
            }
            else if (node.text == L"tilde" || node.text == L"widetilde")
            {
                constexpr unsigned segments = 6u;
                const float amplitude       = box.size * .055f;
                auto previousX              = x;
                auto previousY              = y + amplitude;
                for (unsigned segment = 1u; segment <= segments; ++segment)
                {
                    const float nextX = x + box.extent.width * static_cast<float>(segment) / static_cast<float>(segments);
                    const float nextY = y + ((segment & 1u) ? -amplitude : amplitude);
                    Stroke(previousX, previousY, nextX, nextY, stroke);
                    previousX = nextX;
                    previousY = nextY;
                }
            }
            else if (node.text == L"dot" || node.text == L"ddot")
            {
                const float radius = std::max(stroke * .5f, box.size * .035f);
                const float center = x + box.extent.width / 2.f;
                if (node.text == L"ddot")
                {
                    Stroke(center - box.size * .14f - radius, y, center - box.size * .14f + radius, y, stroke);
                    Stroke(center + box.size * .14f - radius, y, center + box.size * .14f + radius, y, stroke);
                }
                else
                    Stroke(center - radius, y, center + radius, y, stroke);
            }
            else
            {
                Stroke(x, y, x + box.extent.width, y, stroke);
                if (node.text == L"vec")
                {
                    Stroke(x + box.extent.width - 4.f, y - 3.f, x + box.extent.width, y, stroke);
                    Stroke(x + box.extent.width - 4.f, y + 3.f, x + box.extent.width, y, stroke);
                }
            }
            return;
        }
        if (node.kind == Matrix && node.columns > 0u)
        {
            std::array<float, 16u> widths{};
            unsigned childColumn{};
            for (auto child = box.firstChild; child < box.endChild; child = NextSibling(child))
            {
                widths[childColumn % node.columns] = std::max(widths[childColumn % node.columns], boxes[child].extent.width);
                ++childColumn;
            }
            const auto rows = static_cast<unsigned>(node.children.size() / node.columns);
            const float top = baseline - box.extent.above, bottom = baseline + box.extent.below;
            float y       = top + box.size * .15f;
            auto rowFirst = box.firstChild;
            for (unsigned row = 0; row < rows; ++row)
            {
                float above{}, below{};
                auto cell = rowFirst;
                for (unsigned col = 0; col < node.columns; ++col)
                {
                    above = std::max(above, boxes[cell].extent.above);
                    below = std::max(below, boxes[cell].extent.below);
                    cell  = NextSibling(cell);
                }
                above             = std::max(above, box.size * .8f);
                const bool fenced = node.text != L"matrix" && node.text != L"smallmatrix" && node.text != L"aligned" && node.text != L"gathered";
                float cellX       = x + (fenced ? box.size * .4f : 0.f);
                cell              = rowFirst;
                for (unsigned col = 0; col < node.columns; ++col)
                {
                    const float remaining = widths[col] - boxes[cell].extent.width;
                    const float offset    = node.text == L"aligned" ? (col % 2u == 0u ? remaining : 0.f) : node.text == L"cases" ? 0.f : remaining / 2.f;
                    Place(cell, cellX + offset, y + above);
                    cellX += widths[col] + box.size * .65f;
                    cell = NextSibling(cell);
                }
                y += std::max(box.size, above + below) + box.size * .3f;
                rowFirst = cell;
            }
            if (node.text != L"matrix" && node.text != L"smallmatrix" && node.text != L"aligned" && node.text != L"gathered")
            {
                const auto left  = node.text == L"pmatrix"                            ? L'('
                                   : node.text == L"Bmatrix" || node.text == L"cases" ? L'{'
                                   : node.text == L"vmatrix"                          ? L'|'
                                   : node.text == L"Vmatrix"                          ? L'‖'
                                                                                      : L'[';
                const auto right = left == L'(' ? L')' : left == L'{' ? L'}' : left == L'[' ? L']' : left;
                Fence(left, x + 2.f, top, bottom, box.size * .2f, stroke);
                if (node.text != L"cases")
                    Fence(right, x + box.extent.width - box.size * .2f - 2.f, top, bottom, box.size * .2f, stroke);
            }
            return;
        }
        if (node.kind == Row)
        {
            auto previous = box.endChild;
            for (auto child = box.firstChild; child < box.endChild; child = NextSibling(child))
            {
                if (previous < box.endChild && boxes[child].node->kind != MathKind::Space)
                {
                    const auto left = boxes[previous].atomClass, right = boxes[child].atomClass;
                    x += AtomSpace(left, right, box.size, box.scriptLevel);
                }
                Place(child, x, baseline);
                x += boxes[child].extent.width;
                if (boxes[child].node->kind != MathKind::Space)
                    previous = child;
            }
            return;
        }
        auto child = box.firstChild;
        while (child < box.endChild)
        {
            Place(child, x, baseline);
            x += boxes[child].extent.width;
            child = NextSibling(child);
        }
    }
    void Finalize()
    {
        lines.clear();
        MeasureBoxes();
        extent = boxes.empty() ? Extent{} : boxes[0u].extent;
        if (extent.width > 65536.f || extent.above + extent.below > 65536.f || ! std::isfinite(extent.width + extent.above + extent.below))
            throw std::length_error("Mathematical geometry capacity exceeded");
        lines.reserve(boxes.size() * 6u);
        Place(0u, 0.f, extent.above);
        float left{}, right = extent.width, top{}, bottom = extent.above + extent.below;
        for (const auto& glyph : glyphs)
        {
            left   = std::min(left, glyph.origin.x - glyph.leftOverhang);
            right  = std::max(right, glyph.origin.x + glyph.extent.width + glyph.rightOverhang);
            top    = std::min(top, glyph.origin.y - glyph.topOverhang);
            bottom = std::max(bottom, glyph.origin.y + glyph.extent.above + glyph.extent.below + glyph.bottomOverhang);
        }
        for (const auto& line : lines)
        {
            const float half = line.thickness * .5f;
            left             = std::min({left, line.first.x - half, line.last.x - half});
            right            = std::max({right, line.first.x + half, line.last.x + half});
            top              = std::min({top, line.first.y - half, line.last.y - half});
            bottom           = std::max({bottom, line.first.y + half, line.last.y + half});
        }
        const float width = right - left, above = extent.above - top, below = bottom - extent.above;
        if (! std::isfinite(width + above + below) || width > 65536.f || above + below > 65536.f)
            throw std::length_error("Mathematical ink geometry capacity exceeded");
        const float shiftX = -left, shiftY = -top;
        for (auto& glyph : glyphs)
        {
            glyph.origin.x += shiftX;
            glyph.origin.y += shiftY;
        }
        for (auto& line : lines)
        {
            line.first.x += shiftX;
            line.first.y += shiftY;
            line.last.x += shiftX;
            line.last.y += shiftY;
        }
        extent    = {width, above, below};
        finalized = true;
    }

public:
    PreparedMath(IDWriteFactory* write, std::shared_ptr<const MathExpression> value, float size, bool display = false)
        : expression(std::move(value)),
          factory(write),
          glyphs(0u),
          boxes(0u),
          lines(0u),
          font(size),
          displayStyle(display)
    {
        if (! expression || ! *expression || ! std::isfinite(size) || size <= 0.f || size > 4096.f || ! write)
            throw std::invalid_argument("Invalid mathematical preparation request");
        const auto count = CountAndValidate(expression->root);
        boxes.reserve(count);
        glyphs.reserve(count);
        axis = font * .25f;
        InitializeFallbackConstants();
        ReadMathConstants(write);
        Flatten(expression->root, font, false, false, false, displayStyle, 0u);
    }
    bool Advance()
    {
        if (finalized)
            return true;
        if (cursor < glyphs.size())
        {
            auto& leaf = glyphs[cursor];
            wil::com_ptr<IDWriteTextFormat> format;
            THROW_IF_FAILED(factory->CreateTextFormat(L"Cambria Math",
                                                      nullptr,
                                                      leaf.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                                      leaf.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                                                      DWRITE_FONT_STRETCH_NORMAL,
                                                      leaf.font,
                                                      L"en-US",
                                                      format.put()));
            wil::com_ptr<IDWriteTextLayout> layout;
            THROW_IF_FAILED(
                factory->CreateTextLayout(leaf.node->text.data(), static_cast<UINT32>(leaf.node->text.size()), format.get(), 65536.f, 65536.f, layout.put()));
            DWRITE_TEXT_METRICS metrics{};
            THROW_IF_FAILED(layout->GetMetrics(&metrics));
            DWRITE_LINE_METRICS line{};
            UINT32 lineCount{};
            const auto hr = layout->GetLineMetrics(&line, 1u, &lineCount);
            if (FAILED(hr) || lineCount != 1u)
                throw std::length_error("Mathematical text leaf must occupy one line");
            const float layoutWidth  = std::min(65536.f, metrics.widthIncludingTrailingWhitespace + leaf.font);
            const float layoutHeight = std::min(65536.f, metrics.height + leaf.font);
            THROW_IF_FAILED(layout->SetMaxWidth(std::max(1.f, layoutWidth)));
            THROW_IF_FAILED(layout->SetMaxHeight(std::max(1.f, layoutHeight)));
            DWRITE_OVERHANG_METRICS overhang{};
            THROW_IF_FAILED(layout->GetOverhangMetrics(&overhang));
            const float leftInk = std::isfinite(overhang.left) ? std::max(0.f, overhang.left) : 0.f;
            const float rightOverhang =
                std::isfinite(overhang.right) ? std::max(0.f, layoutWidth + overhang.right - metrics.widthIncludingTrailingWhitespace) : 0.f;
            const float italicCorrection = leaf.italic ? rightOverhang : 0.f;
            const Extent shapedExtent{metrics.widthIncludingTrailingWhitespace, line.baseline, metrics.height - line.baseline};
            leaf.layout                       = std::move(layout);
            leaf.extent                       = shapedExtent;
            leaf.italicCorrection             = italicCorrection;
            leaf.leftOverhang                 = leftInk;
            leaf.topOverhang                  = std::isfinite(overhang.top) ? std::max(0.f, overhang.top) : 0.f;
            leaf.rightOverhang                = rightOverhang;
            leaf.bottomOverhang               = std::isfinite(overhang.bottom) ? std::max(0.f, layoutHeight + overhang.bottom - metrics.height) : 0.f;
            boxes[leaf.boxIndex].leftOffset   = leftInk;
            boxes[leaf.boxIndex].extent.width = leaf.extent.width + leftInk + rightOverhang;
            boxes[leaf.boxIndex].extent.above = leaf.extent.above + leaf.topOverhang;
            boxes[leaf.boxIndex].extent.below = leaf.extent.below + leaf.bottomOverhang;
            ++cursor;
            return false;
        }
        Finalize();
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
    const auto& GlyphLayouts() const noexcept
    {
        return glyphs;
    }
    const auto& StrokeLayouts() const noexcept
    {
        return lines;
    }
    void Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float scale) const noexcept
    {
        D2D1_MATRIX_3X2_F previous;
        target->GetTransform(&previous);
        target->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(origin.x, origin.y) * previous);
        for (const auto& glyph : glyphs)
            target->DrawTextLayout(glyph.origin, glyph.layout.get(), brush);
        for (const auto& line : lines)
            target->DrawLine(line.first, line.last, brush, line.thickness);
        target->SetTransform(previous);
    }
};
} // namespace DxUi::RichText::Detail
