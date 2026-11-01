#pragma once
#include "DxUi.MathFontTable.h"
#include <DxUi/Configuration.h>
#include <DxUi/RichDocument.h>
#include <DxUi/RichMath.h>
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result.h>
#pragma warning(push)
#pragma warning(disable : 4746)
#include <wrl/implements.h>
#pragma warning(pop)

namespace DxUi::RichText::Detail
{
#if DXUI_ENABLE_DIAGNOSTICS
// Private test seam for preserving HRESULT classification from GetLineMetrics.
extern thread_local HRESULT testRichMathLineMetricsFailure;
extern thread_local HRESULT testRichMathGlyphMetricsFailure;
extern thread_local HRESULT testRichMathGlyphIndexFailure;
extern thread_local std::size_t testRichMathConstantReads;
extern thread_local std::size_t testRichMathPreparations;
extern thread_local std::size_t testRichMathTextFormats;
extern thread_local std::span<const std::uint8_t> testRichMathTableOverride;
#endif
// Shared only by one owner-thread staging generation. Published formula glyphs
// do not query or mutate this cache during paint. Fonts/formats survive through
// the formula's lifetime; no process-global cache or factory identity is used.
class MathTextFormats final
{
public:
    explicit MathTextFormats(IDWriteFactory* factory, std::wstring_view family = L"Cambria Math", std::wstring_view locale = L"en-us")
        : factory_(factory),
          family_(ValidateName(family, false)),
          locale_(ValidateName(locale, true)),
          formats_(0u)
    {
        if (! factory)
            throw std::invalid_argument("Mathematical text formats need a factory");
    }
    IDWriteFactory* Factory() const noexcept
    {
        return factory_.get();
    }
    const std::wstring& Family() const noexcept
    {
        return family_;
    }
    const std::wstring& Locale() const noexcept
    {
        return locale_;
    }
    std::size_t Size() const noexcept
    {
        return formats_.size();
    }
    IDWriteTextFormat* Get(float size, bool bold, bool italic)
    {
        if (! std::isfinite(size) || size <= 0.f || size > 4096.f)
            throw std::invalid_argument("Invalid mathematical format size");
        for (const auto& entry : formats_)
            if (entry.size == size && entry.bold == bold && entry.italic == italic)
                return entry.format.get();
        if (formats_.size() == 4096u)
            throw std::length_error("Mathematical text format capacity exceeded");
        wil::com_ptr<IDWriteTextFormat> format;
        THROW_IF_FAILED(factory_->CreateTextFormat(family_.c_str(),
                                                   nullptr,
                                                   bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                                   italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                                                   DWRITE_FONT_STRETCH_NORMAL,
                                                   size,
                                                   locale_.c_str(),
                                                   format.put()));
#if DXUI_ENABLE_DIAGNOSTICS
        ++testRichMathTextFormats;
#endif
        formats_.push_back({size, bold, italic, std::move(format)});
        return formats_.back().format.get();
    }

private:
    static std::wstring_view ValidateName(std::wstring_view value, bool emptyAllowed)
    {
        if ((! emptyAllowed && value.empty()) || value.size() > 255u || ! IsValidUtf16(value))
            throw std::invalid_argument("Invalid mathematical family or locale");
        return value;
    }
    struct Entry
    {
        float size;
        bool bold, italic;
        wil::com_ptr<IDWriteTextFormat> format;
    };
    wil::com_ptr<IDWriteFactory> factory_;
    std::wstring family_, locale_;
    std::vector<Entry> formats_;
};
// DWrite supplies the actual shaped face and glyph. Source-codepoint lookup is
// not sufficient for font fallback, substitutions or surrogate-pair shaping.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class MathGlyphProbe final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextRenderer>
{
public:
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* value) noexcept override
    {
        if (! value)
            return E_POINTER;
        *value = TRUE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* value) noexcept override
    {
        if (! value)
            return E_POINTER;
        *value = {1, 0, 0, 1, 0, 0};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* value) noexcept override
    {
        if (! value)
            return E_POINTER;
        *value = 1.f;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE
    DrawGlyphRun(void*, FLOAT, FLOAT, DWRITE_MEASURING_MODE, const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) noexcept override
    {
        ++runs;
        if (run && run->glyphCount == 1u && run->glyphIndices && run->fontFace && ! run->isSideways)
        {
            face  = run->fontFace;
            glyph = run->glyphIndices[0];
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) noexcept override
    {
        ++runs;
        return S_OK;
    }
    wil::com_ptr<IDWriteFontFace> face;
    UINT16 glyph{};
    unsigned runs{};
};
#pragma warning(pop)
inline bool SameMathFontFile(IDWriteFontFace* left, IDWriteFontFace* right) noexcept
{
    if (! left || ! right || left->GetType() != right->GetType() || left->GetIndex() != right->GetIndex())
        return false;
    UINT32 leftCount{}, rightCount{};
    if (FAILED(left->GetFiles(&leftCount, nullptr)) || FAILED(right->GetFiles(&rightCount, nullptr)) || leftCount != 1u || rightCount != 1u)
        return false;
    wil::com_ptr<IDWriteFontFile> a, b;
    if (FAILED(left->GetFiles(&leftCount, a.put())) || FAILED(right->GetFiles(&rightCount, b.put())))
        return false;
    wil::com_ptr<IDWriteFontFileLoader> aLoader, bLoader;
    const void *aKey{}, *bKey{};
    UINT32 aSize{}, bSize{};
    return SUCCEEDED(a->GetLoader(aLoader.put())) && SUCCEEDED(b->GetLoader(bLoader.put())) && aLoader.get() == bLoader.get() &&
           SUCCEEDED(a->GetReferenceKey(&aKey, &aSize)) && SUCCEEDED(b->GetReferenceKey(&bKey, &bSize)) && aSize == bSize &&
           (! aSize || (aKey && bKey && std::memcmp(aKey, bKey, aSize) == 0));
}
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
        UINT16 mathGlyph{};
        float inkAbove{}, inkBelow{};
        bool needsKern{};
    };
    struct MathFontData
    {
        static constexpr std::int32_t MissingCorrection = (std::numeric_limits<std::int32_t>::max)();
        MathFontData(IDWriteFontFace* source, std::span<const std::uint8_t> bytes, std::uint16_t units)
            : face(source),
              table(bytes.begin(), bytes.end()),
              corrections(source->GetGlyphCount(), MissingCorrection),
              correctionDevices(source->GetGlyphCount(), 0u),
              constructions(source->GetGlyphCount(), 0u),
              horizontalConstructions(source->GetGlyphCount(), 0u),
              designUnits(units)
        {
            const MathFontTable parsed(table);
            for (std::size_t offset = 8u; offset <= 152u; offset += 4u)
            {
                const auto value = parsed.MathConstant(offset);
                if (value)
                {
                    constants[offset / 4u]       = value.value();
                    constantPresent[offset / 4u] = true;
                }
            }
            for (std::size_t index = 0u; index < corrections.size(); ++index)
            {
                const auto glyph      = static_cast<std::uint16_t>(index);
                const auto correction = parsed.ItalicCorrectionRecord(glyph);
                if (correction)
                {
                    corrections[index]       = correction->value;
                    correctionDevices[index] = static_cast<std::uint32_t>(correction->device);
                }
                const auto construction = parsed.VerticalConstruction(glyph);
                if (construction)
                    constructions[index] = static_cast<std::uint32_t>(construction.value());
                const auto horizontal = parsed.HorizontalConstruction(glyph);
                if (horizontal)
                    horizontalConstructions[index] = static_cast<std::uint32_t>(horizontal.value());
            }
        }
        wil::com_ptr<IDWriteFontFace> face;
        std::vector<std::uint8_t> table;
        std::vector<std::int32_t> corrections;
        std::vector<std::uint32_t> correctionDevices;
        std::vector<std::uint32_t> constructions;
        std::vector<std::uint32_t> horizontalConstructions;
        std::array<MathFontTable::ValueRecord, 39u> constants{};
        std::array<bool, 39u> constantPresent{};
        std::uint16_t designUnits{};
    };
    struct VariantGlyph
    {
        std::uint16_t glyph{};
        float x{}, baseline{}, fontSize{}, scaleX{}, left{}, top{}, width{}, height{};
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
        float subKern{}, superKern{}, scriptLeftPad{};
        MathAtomClass atomClass{MathAtomClass::Ordinary};
        float size{};
        unsigned scriptLevel{};
        bool display{};
    };
    std::shared_ptr<const MathExpression> expression;
    wil::com_ptr<IDWriteFactory> factory;
    std::shared_ptr<MathTextFormats> textFormats;
    std::shared_ptr<const MathFontData> mathData;
    std::vector<Glyph> glyphs;
    std::vector<Box> boxes;
    std::vector<Line> lines;
    std::vector<VariantGlyph> variants;
    std::size_t cursor{};
    float font{}, axis{}, dpi{96.f};
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
    static bool OptionalFontOperation(HRESULT result)
    {
        if (result == DWRITE_E_UNSUPPORTEDOPERATION)
            return false;
        THROW_IF_FAILED(result);
        return true;
    }
    HRESULT GlyphMetrics(UINT16 glyph, DWRITE_GLYPH_METRICS& metrics) const noexcept
    {
#if DXUI_ENABLE_DIAGNOSTICS
        if (FAILED(testRichMathGlyphMetricsFailure))
        {
            const auto result               = testRichMathGlyphMetricsFailure;
            testRichMathGlyphMetricsFailure = S_OK;
            return result;
        }
#endif
        return mathData->face->GetDesignGlyphMetrics(&glyph, 1u, &metrics, FALSE);
    }
    HRESULT GlyphIndex(std::uint32_t scalar, UINT16& glyph) const noexcept
    {
#if DXUI_ENABLE_DIAGNOSTICS
        if (FAILED(testRichMathGlyphIndexFailure))
        {
            const auto result             = testRichMathGlyphIndexFailure;
            testRichMathGlyphIndexFailure = S_OK;
            return result;
        }
#endif
        return mathData->face->GetGlyphIndices(&scalar, 1u, &glyph);
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
    void ReadMathConstants(IDWriteFactory* write)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ++testRichMathConstantReads;
#endif
        // A missing, truncated, or implausible MATH table uses deterministic
        // em-relative constants. A valid table up to four MiB is copied and parsed once per geometry generation.
        wil::com_ptr<IDWriteFontCollection> collection;
        if (! OptionalFontOperation(write->GetSystemFontCollection(collection.put(), FALSE)))
            return;
        UINT32 familyIndex{};
        BOOL exists{};
        if (! OptionalFontOperation(collection->FindFamilyName(textFormats->Family().c_str(), &familyIndex, &exists)) || ! exists)
            return;
        wil::com_ptr<IDWriteFontFamily> family;
        if (! OptionalFontOperation(collection->GetFontFamily(familyIndex, family.put())))
            return;
        wil::com_ptr<IDWriteFont> fontObject;
        if (! OptionalFontOperation(
                family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, fontObject.put())))
            return;
        wil::com_ptr<IDWriteFontFace> face;
        if (! OptionalFontOperation(fontObject->CreateFontFace(face.put())))
            return;
        const void* table{};
        UINT32 size{};
        void* context{};
        BOOL existsTable{};
        if (! OptionalFontOperation(face->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('M', 'A', 'T', 'H'), &table, &size, &context, &existsTable)) ||
            ! existsTable)
            return;
        auto release      = wil::scope_exit([&] { face->ReleaseFontTable(context); });
        const auto* bytes = static_cast<const BYTE*>(table);
#if DXUI_ENABLE_DIAGNOSTICS
        if (! testRichMathTableOverride.empty())
        {
            if (testRichMathTableOverride.size() > 4u * 1024u * 1024u)
                return;
            bytes = testRichMathTableOverride.data();
            size  = static_cast<UINT32>(testRichMathTableOverride.size());
        }
#endif
        if (! bytes || size < 26 || bytes[0] != 0 || bytes[1] != 1)
            return;
        const auto offset = static_cast<unsigned>((bytes[4] << 8u) | bytes[5]);
        if (offset == 0 || offset > size || size - offset < 16u)
            return;
        DWRITE_FONT_METRICS metrics{};
        face->GetMetrics(&metrics);
        if (metrics.designUnitsPerEm == 0u || size > 4u * 1024u * 1024u)
            return;
        mathData = std::make_shared<MathFontData>(face.get(), std::span(bytes, static_cast<std::size_t>(size)), metrics.designUnitsPerEm);
        ApplyMathConstants();
    }
    void ApplyMathConstants() noexcept
    {
        if (! mathData || mathData->table.size() < 16u || mathData->designUnits == 0u)
            return;
        const auto* bytes = mathData->table.data();
        const auto size   = mathData->table.size();
        const auto offset = static_cast<unsigned>((bytes[4] << 8u) | bytes[5]);
        if (offset == 0u || offset > size || size - offset < 16u)
            return;
        // MathConstants starts with the two percentage scales, two unsigned
        // heights, then MathValueRecords. AxisHeight follows MathLeading.
        const auto* constants     = bytes + offset;
        const auto first          = Signed16(constants);
        const auto second         = Signed16(constants + 2);
        const auto delimitedUnits = static_cast<unsigned>((constants[4] << 8u) | constants[5]);
        const auto operatorUnits  = static_cast<unsigned>((constants[6] << 8u) | constants[7]);
        if (first >= 30 && first <= 100)
            scriptScale = static_cast<float>(first) / 100.f;
        if (second >= 20 && second <= 100)
            scriptScriptScale = static_cast<float>(second) / 100.f;
        const auto delimitedScale = static_cast<float>(delimitedUnits) / static_cast<float>(mathData->designUnits);
        const auto operatorScale  = static_cast<float>(operatorUnits) / static_cast<float>(mathData->designUnits);
        if (std::isfinite(delimitedScale) && delimitedScale >= 1.f && delimitedScale <= 4.f)
            delimitedMinimum = delimitedScale;
        if (std::isfinite(operatorScale) && operatorScale >= 1.f && operatorScale <= 4.f)
            displayOperatorMinimum = operatorScale;
        const MathFontTable table(mathData->table);
        const auto readDesignValue = [&](unsigned valueOffset, float fallback)
        {
            const auto index = valueOffset / 4u;
            if (index >= mathData->constants.size() || ! mathData->constantPresent[index])
                return fallback;
            const auto scaled = table.AdjustedValue(mathData->constants[index], font, dpi, mathData->designUnits);
            return scaled && scaled.value() >= 0.f && scaled.value() <= font * 2.f ? scaled.value() : fallback;
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
        const auto candidateAxis        = readDesignValue(12u, axis);
        if (std::isfinite(candidateAxis) && candidateAxis > 0 && candidateAxis < font)
            axis = candidateAxis;
    }
    // Share owned font bytes, but recalculate ppem-specific values for this formula's size and DPI.
    void CopyMathConstants(const PreparedMath& source) noexcept
    {
        mathData = source.mathData;
        ApplyMathConstants();
    }
    [[nodiscard]] float MathConstantAt(unsigned valueOffset, float size, float rootValue) const noexcept
    {
        const auto fallback = rootValue * size / font;
        const auto index    = valueOffset / 4u;
        if (! mathData || size == font || index >= mathData->constants.size() || ! mathData->constantPresent[index] || ! mathData->constants[index].device)
            return fallback;
        const auto adjusted = MathFontTable(mathData->table).AdjustedValue(mathData->constants[index], size, dpi, mathData->designUnits);
        return adjusted && adjusted.value() >= 0.f && adjusted.value() <= size * 2.f ? adjusted.value() : fallback;
    }
    static std::size_t CountAndValidate(const MathNode& root)
    {
        struct Pending
        {
            const MathNode* node;
            unsigned depth;
        };
        std::array<Pending, MathMaximumNodes> pending{};
        std::size_t top{}, count{};
        pending[top++] = {&root, 0u};
        while (top != 0u)
        {
            const auto current = pending[--top];
            const auto& node   = *current.node;
            if (++count > MathMaximumNodes || current.depth > MathMaximumNodeDepth || node.children.size() > MathMaximumNodes - top ||
                node.text.size() > MathMaximumSourceUnits || ! IsValidUtf16(node.text))
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
                    if (node.columns == 0u || node.columns > MathMaximumColumns || node.children.empty() || node.children.size() > MathMaximumMatrixCells ||
                        node.children.size() % node.columns != 0u || node.children.size() / node.columns > MathMaximumRows ||
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
                    if (node.text != L"mathrm" && node.text != L"mathbf" && node.text != L"mathit" && node.text != L"mathnormal" &&
                        node.text != L"boldsymbol" && node.text != L"bm" && node.text != L"mathbb" && node.text != L"mathcal" && node.text != L"mathfrak" &&
                        node.text != L"mathsf" && node.text != L"mathtt" && node.text != L"mathord" && node.text != L"mathop" && node.text != L"mathbin" &&
                        node.text != L"mathrel" && node.text != L"mathopen" && node.text != L"mathclose" && node.text != L"mathpunct" &&
                        node.text != L"mathinner" && node.text != L"displaystyle" && node.text != L"textstyle" && node.text != L"scriptstyle" &&
                        node.text != L"scriptscriptstyle")
                        throw std::length_error("Invalid mathematical style");
                    if (node.children.size() != 1u)
                        throw std::length_error("Invalid mathematical unary node");
                    break;
                case MathKind::Delimited:
                    if (node.kind == MathKind::Delimited &&
                        (node.text.size() != 2u || std::wstring_view(L"()[]{}|.⟨⟩‖⌊⌋⌈⌉/<>\\↑↓↕⇑⇓⇕").find(node.text[0]) == std::wstring_view::npos ||
                         std::wstring_view(L"()[]{}|.⟨⟩‖⌊⌋⌈⌉/<>\\↑↓↕⇑⇓⇕").find(node.text[1]) == std::wstring_view::npos))
                        throw std::length_error("Invalid mathematical fence pair");
                    if (node.children.size() != 1u)
                        throw std::length_error("Invalid mathematical unary node");
                    break;
                case MathKind::Delimiter:
                    if (! node.children.empty() || node.text.size() != 1u ||
                        std::wstring_view(L"()[]{}|.⟨⟩‖⌊⌋⌈⌉/<>\\↑↓↕⇑⇓⇕").find(node.text[0]) == std::wstring_view::npos || node.delimiterSize < 1u ||
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
                else if (node.text == L"boldsymbol" || node.text == L"bm")
                    childBold = true;
                else if (node.text == L"mathnormal")
                {
                    childBold        = false;
                    childItalic      = false;
                    childForceItalic = false;
                }
                if (node.text == L"mathrm" || node.text == L"mathbb" || node.text == L"mathcal" || node.text == L"mathfrak" || node.text == L"mathsf" ||
                    node.text == L"mathtt")
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
    [[nodiscard]] std::optional<float> FontItalicCorrection(std::wstring_view text, float size) const
    {
        if (! mathData || text.empty())
            return std::nullopt;
        std::uint32_t scalar = static_cast<std::uint16_t>(text.back());
        if (scalar >= 0xdc00u && scalar <= 0xdfffu && text.size() >= 2u)
        {
            const auto lead = static_cast<std::uint16_t>(text[text.size() - 2u]);
            if (lead < 0xd800u || lead > 0xdbffu)
                return std::nullopt;
            scalar = 0x10000u + ((lead - 0xd800u) << 10u) + (scalar - 0xdc00u);
        }
        UINT16 glyph{};
        if (! OptionalFontOperation(GlyphIndex(scalar, glyph)) || glyph == 0u || glyph >= mathData->corrections.size())
            return std::nullopt;
        const auto correction = mathData->corrections[glyph];
        if (correction == MathFontData::MissingCorrection)
            return std::nullopt;
        const MathFontTable::ValueRecord record{static_cast<std::int16_t>(correction), mathData->correctionDevices[glyph]};
        const auto scaled = MathFontTable(mathData->table).AdjustedValue(record, size, dpi, mathData->designUnits);
        return scaled && std::abs(scaled.value()) <= size * 2.f ? scaled : std::nullopt;
    }
    float ScriptKern(std::size_t baseIndex, std::size_t scriptIndex, float shift, bool superscript) const noexcept
    {
        if (! mathData || boxes[baseIndex].glyph == std::numeric_limits<std::size_t>::max() ||
            boxes[scriptIndex].glyph == std::numeric_limits<std::size_t>::max())
            return 0.f;
        const auto& base   = glyphs[boxes[baseIndex].glyph];
        const auto& script = glyphs[boxes[scriptIndex].glyph];
        if (! base.mathGlyph || ! script.mathGlyph)
            return 0.f;
        const MathFontTable table(mathData->table);
        using Corner            = MathFontTable::KernCorner;
        const auto baseCorner   = superscript ? Corner::TopRight : Corner::BottomRight;
        const auto scriptCorner = superscript ? Corner::BottomLeft : Corner::TopLeft;
        const auto value        = [&](const Glyph& glyph, Corner corner, float height)
        { return table.MathKern(glyph.mathGlyph, corner, height, glyph.font, dpi, mathData->designUnits).value_or(0.f); };
        const float baseHeight1   = superscript ? shift - script.inkBelow : script.inkAbove - shift;
        const float scriptHeight1 = superscript ? -script.inkBelow : script.inkAbove;
        const float baseHeight2   = superscript ? base.inkAbove : -base.inkBelow;
        const float scriptHeight2 = superscript ? base.inkAbove - shift : shift - base.inkBelow;
        const float first         = value(base, baseCorner, baseHeight1) + value(script, scriptCorner, scriptHeight1);
        const float second        = value(base, baseCorner, baseHeight2) + value(script, scriptCorner, scriptHeight2);
        const float result        = std::min(first, second);
        return std::isfinite(result) && std::abs(result) <= base.font * 2.f ? result : 0.f;
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
        std::array<std::size_t, MathMaximumNodes> children{};
        std::array<MathAtomClass, MathMaximumNodes> original{}, next{};
        std::array<bool, MathMaximumNodes> hasNext{};
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
                const float axisAtSize = MathConstantAt(12u, box.size, axis);
                const float rule       = std::max(1.f, MathConstantAt(144u, box.size, fractionRuleThickness));
                const float topGap     = MathConstantAt(box.display ? 140u : 136u, box.size, box.display ? fractionNumeratorDisplayGap : fractionNumeratorGap);
                const float bottomGap =
                    MathConstantAt(box.display ? 152u : 148u, box.size, box.display ? fractionDenominatorDisplayGap : fractionDenominatorGap);
                const float topShift =
                    std::max(MathConstantAt(box.display ? 124u : 120u, box.size, box.display ? fractionNumeratorDisplayShift : fractionNumeratorShift),
                             axisAtSize + top.below + topGap + rule * .5f);
                const float bottomShift =
                    std::max(MathConstantAt(box.display ? 132u : 128u, box.size, box.display ? fractionDenominatorDisplayShift : fractionDenominatorShift),
                             bottom.above + bottomGap + rule * .5f - axisAtSize);
                box.extent = {std::max(top.width, bottom.width) + box.size * .4f, topShift + top.above, bottomShift + bottom.below};
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
                if (limitsAboveBelow)
                {
                    box.extent = {std::max({base.width, sub.width, super.width}) + box.size * .1f,
                                  base.above + (super.width > 0.f ? super.below + super.above + MathConstantAt(64u, box.size, upperLimitGap) : 0.f),
                                  base.below + (sub.width > 0.f ? sub.above + sub.below + MathConstantAt(72u, box.size, lowerLimitGap) : 0.f)};
                    continue;
                }
                const float scriptGap    = MathConstantAt(52u, box.size, scriptGapMinimum);
                const float subShift     = std::max(MathConstantAt(24u, box.size, subscriptShift), base.below + box.size * .12f);
                const float superShift   = std::max(MathConstantAt(36u, box.size, superscriptShift), base.above - box.size * .08f);
                const float availableGap = superShift + subShift - super.below - sub.above;
                const float extra        = std::max(0.f, (scriptGap - availableGap) / 2.f);
                const auto baseIndex     = ChildAt(reverse, 0u);
                const auto subIndex = ChildAt(reverse, 1u), superIndex = ChildAt(reverse, 2u);
                box.subKern        = sub.width > 0.f ? ScriptKern(baseIndex, subIndex, subShift + extra, false) : 0.f;
                box.superKern      = super.width > 0.f ? ScriptKern(baseIndex, superIndex, superShift + extra, true) : 0.f;
                const float subX   = ScriptBaseAdvance(baseIndex) + box.subKern;
                const float superX = ScriptAnchor(baseIndex) + box.superKern;
                const float left   = std::min({0.f, sub.width > 0.f ? subX : 0.f, super.width > 0.f ? superX : 0.f});
                box.scriptLeftPad  = -left;
                box.extent         = {std::max({base.width, sub.width > 0.f ? subX + sub.width : 0.f, super.width > 0.f ? superX + super.width : 0.f}) - left,
                                      std::max(base.above, super.above + superShift + extra),
                                      std::max(base.below, sub.below + subShift + extra)};
                continue;
            }
            if (node.kind == Matrix && node.columns > 0u)
            {
                std::array<float, MathMaximumColumns> widths{};
                float totalHeight{};
                std::size_t ci{};
                for (auto cellIndex = box.firstChild; cellIndex < box.endChild; cellIndex = NextSibling(cellIndex), ++ci)
                    widths[ci % node.columns] = std::max(widths[ci % node.columns], boxes[cellIndex].extent.width);
                auto rowCell = box.firstChild;
                for (std::size_t row = 0u; row < node.children.size(); row += node.columns)
                {
                    float above{}, below{};
                    for (unsigned col = 0u; col < node.columns; ++col)
                    {
                        const auto cell = boxes[rowCell].extent;
                        above           = std::max(above, cell.above);
                        below           = std::max(below, cell.below);
                        rowCell         = NextSibling(rowCell);
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
                    const float axisOffset    = MathConstantAt(12u, box.size, axis);
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
                const float axisOffset = MathConstantAt(12u, box.size, axis);
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
    bool AddVerticalAssembly(
        const MathFontTable& table, std::size_t construction, std::uint32_t minimum, float x, float top, float bottom, float width, float size)
    {
        const auto plan                                      = table.VerticalAssemblyAt(construction, minimum);
        constexpr std::size_t MaximumAssemblyPartsPerFormula = 512u;
        if (! plan || plan->count == 0u || variants.size() >= MaximumAssemblyPartsPerFormula || plan->count > MaximumAssemblyPartsPerFormula - variants.size())
            return false;
        const float units = size / static_cast<float>(mathData->designUnits);
        std::array<DWRITE_GLYPH_METRICS, 64u> metrics{};
        float left = (std::numeric_limits<float>::max)(), right = -(std::numeric_limits<float>::max)();
        for (std::size_t index = 0u; index < plan->count; ++index)
        {
            const auto glyph = plan->placements[index].part.glyph;
            if (glyph >= mathData->face->GetGlyphCount())
                return false;
            std::size_t previous{};
            while (previous < index && plan->placements[previous].part.glyph != glyph)
                ++previous;
            if (previous < index)
                metrics[index] = metrics[previous];
            else if (! OptionalFontOperation(GlyphMetrics(glyph, metrics[index])))
                return false;
            left  = std::min(left, static_cast<float>(metrics[index].leftSideBearing) * units);
            right = std::max(right, (static_cast<float>(metrics[index].advanceWidth) - static_cast<float>(metrics[index].rightSideBearing)) * units);
        }
        const auto inkWidth = right - left;
        if (! std::isfinite(inkWidth) || inkWidth <= 0.f)
            return false;
        const auto scaleX         = (std::min)(1.f, width / inkWidth);
        const auto originX        = x + (width - inkWidth * scaleX) / 2.f - left * scaleX;
        const auto assemblyHeight = static_cast<float>(plan->advance) * units;
        const auto assemblyBottom = (top + bottom + assemblyHeight) / 2.f;
        std::array<VariantGlyph, 64u> staged{};
        for (std::size_t index = 0u; index < plan->count; ++index)
        {
            const auto& part    = plan->placements[index];
            const auto& metric  = metrics[index];
            const auto inkUnits = static_cast<std::int64_t>(metric.advanceHeight) - metric.topSideBearing - metric.bottomSideBearing;
            if (std::abs(inkUnits - static_cast<std::int64_t>(part.part.advance)) > 2)
                return false; // Unknown part origin convention: retain the vector fallback.
            const auto partTop = assemblyBottom - static_cast<float>(part.offset + part.part.advance) * units;
            // For the admitted ink-aligned subset, MATH fullAdvance covers
            // the part ink; DirectWrite's vertical origin includes its top bearing.
            const auto baseline = partTop + (static_cast<float>(metric.verticalOriginY) - static_cast<float>(metric.topSideBearing)) * units;
            const auto inkLeft  = originX + static_cast<float>(metric.leftSideBearing) * units * scaleX;
            const auto partWidth =
                (static_cast<float>(metric.advanceWidth) - static_cast<float>(metric.leftSideBearing) - static_cast<float>(metric.rightSideBearing)) * units *
                scaleX;
            const auto inkTop = baseline + (static_cast<float>(metric.topSideBearing) - static_cast<float>(metric.verticalOriginY)) * units;
            const auto inkHeight =
                (static_cast<float>(metric.advanceHeight) - static_cast<float>(metric.topSideBearing) - static_cast<float>(metric.bottomSideBearing)) * units;
            if (! std::isfinite(originX + baseline + inkLeft + partWidth + inkTop + inkHeight) || partWidth < 0.f || inkHeight < 0.f || inkHeight > 65536.f)
                return false;
            staged[index] = {part.part.glyph, originX, baseline, size, scaleX, inkLeft, inkTop, partWidth, inkHeight};
        }
        variants.insert(variants.end(), staged.begin(), staged.begin() + static_cast<std::ptrdiff_t>(plan->count));
        return true;
    }
    bool AddVerticalVariant(wchar_t shape, float x, float top, float bottom, float width, float size)
    {
        if (! mathData || width <= 0.f || size <= 0.f || bottom <= top)
            return false;
        const auto required = (bottom - top) * static_cast<float>(mathData->designUnits) / size;
        if (! std::isfinite(required) || required <= 0.f || required > static_cast<float>((std::numeric_limits<std::uint32_t>::max)() / 2u))
            return false;
        const std::uint32_t scalar = static_cast<std::uint16_t>(shape);
        UINT16 baseGlyph{};
        if (! OptionalFontOperation(GlyphIndex(scalar, baseGlyph)) || baseGlyph == 0u)
            return false;
        if (baseGlyph >= mathData->constructions.size() || mathData->constructions[baseGlyph] == 0u)
            return false;
        const MathFontTable table(mathData->table);
        const auto minimum = static_cast<std::uint32_t>(std::ceil(required));
        const auto choice = minimum <= 65535u ? table.VerticalVariantAt(mathData->constructions[baseGlyph], static_cast<std::uint16_t>(minimum)) : std::nullopt;
        if (! choice)
            return AddVerticalAssembly(table, mathData->constructions[baseGlyph], minimum, x, top, bottom, width, size);
        const auto glyph = choice.value().glyph;
        if (glyph >= mathData->face->GetGlyphCount())
            return false;
        DWRITE_GLYPH_METRICS metrics{};
        if (! OptionalFontOperation(GlyphMetrics(glyph, metrics)))
            return false;
        const float units = size / static_cast<float>(mathData->designUnits);
        const float inkWidth =
            (static_cast<float>(metrics.advanceWidth) - static_cast<float>(metrics.leftSideBearing) - static_cast<float>(metrics.rightSideBearing)) * units;
        const float inkAbove = (static_cast<float>(metrics.verticalOriginY) - static_cast<float>(metrics.topSideBearing)) * units;
        const float inkBelow =
            (static_cast<float>(metrics.advanceHeight) - static_cast<float>(metrics.verticalOriginY) - static_cast<float>(metrics.bottomSideBearing)) * units;
        const float height = inkAbove + inkBelow;
        if (! std::isfinite(inkWidth + height + inkAbove) || inkWidth <= 0.f || height < bottom - top || height > 65536.f)
            return false;
        const float scaleX   = (std::min)(1.f, width / inkWidth);
        const float left     = x + (width - inkWidth * scaleX) / 2.f;
        const float baseline = (top + bottom - height) / 2.f + inkAbove;
        const float originX  = left - static_cast<float>(metrics.leftSideBearing) * units * scaleX;
        variants.push_back({glyph, originX, baseline, size, scaleX, left, baseline - inkAbove, inkWidth * scaleX, height});
        return true;
    }
    bool AddHorizontalAccent(wchar_t shape, float x, float top, float bottom, float width, float size)
    {
        if (! mathData || width <= 0.f || size <= 0.f || bottom <= top)
            return false;
        const auto required = width * static_cast<float>(mathData->designUnits) / size;
        if (! std::isfinite(required) || required <= 0.f || required > static_cast<float>((std::numeric_limits<std::uint32_t>::max)() / 2u))
            return false;
        const std::uint32_t scalar = static_cast<std::uint16_t>(shape);
        UINT16 baseGlyph{};
        if (! OptionalFontOperation(GlyphIndex(scalar, baseGlyph)) || baseGlyph == 0u || baseGlyph >= mathData->horizontalConstructions.size())
            return false;
        const auto construction = mathData->horizontalConstructions[baseGlyph];
        if (! construction)
            return false;
        const MathFontTable table(mathData->table);
        const auto minimum = static_cast<std::uint32_t>(std::ceil(required));
        const auto choice  = minimum <= 65535u ? table.HorizontalVariantAt(construction, static_cast<std::uint16_t>(minimum)) : std::nullopt;
        const auto plan    = choice ? std::optional<MathFontTable::AssemblyPlan>{} : table.HorizontalAssemblyAt(construction, minimum);
        const auto count   = choice ? 1u : plan ? plan->count : 0u;
        constexpr std::size_t MaximumAssemblyPartsPerFormula = 512u;
        if (! count || variants.size() >= MaximumAssemblyPartsPerFormula || count > MaximumAssemblyPartsPerFormula - variants.size())
            return false;
        const auto totalAdvance = choice ? static_cast<std::uint32_t>(choice->advance) : plan->advance;
        const float units       = size / static_cast<float>(mathData->designUnits);
        const float span        = static_cast<float>(totalAdvance) * units;
        if (! std::isfinite(span) || span <= 0.f)
            return false;
        const float scaleX = (std::min)(1.f, width / span);
        const float start  = x + (width - span * scaleX) / 2.f;
        std::array<DWRITE_GLYPH_METRICS, 64u> metrics{};
        float inkTop = (std::numeric_limits<float>::max)(), inkBottom = -(std::numeric_limits<float>::max)();
        for (std::size_t index = 0u; index < count; ++index)
        {
            const auto glyph = choice ? choice->glyph : plan->placements[index].part.glyph;
            if (glyph >= mathData->face->GetGlyphCount())
                return false;
            std::size_t previous{};
            while (previous < index && plan->placements[previous].part.glyph != glyph)
                ++previous;
            if (previous < index)
                metrics[index] = metrics[previous];
            else if (! OptionalFontOperation(GlyphMetrics(glyph, metrics[index])))
                return false;
            if (! choice &&
                std::abs(static_cast<std::int64_t>(metrics[index].advanceWidth) - static_cast<std::int64_t>(plan->placements[index].part.advance)) > 2)
                return false;
            inkTop    = std::min(inkTop, (static_cast<float>(metrics[index].topSideBearing) - static_cast<float>(metrics[index].verticalOriginY)) * units);
            inkBottom = std::max(inkBottom,
                                 (static_cast<float>(metrics[index].advanceHeight) - static_cast<float>(metrics[index].verticalOriginY) -
                                  static_cast<float>(metrics[index].bottomSideBearing)) *
                                     units);
        }
        const auto height = inkBottom - inkTop;
        if (! std::isfinite(height) || height < 0.f || height > bottom - top)
            return false;
        const auto baseline = (top + bottom - height) / 2.f - inkTop;
        std::array<VariantGlyph, 64u> staged{};
        for (std::size_t index = 0u; index < count; ++index)
        {
            const auto glyph   = choice ? choice->glyph : plan->placements[index].part.glyph;
            const auto offset  = choice ? 0u : plan->placements[index].offset;
            const auto origin  = start + static_cast<float>(offset) * units * scaleX;
            const auto& metric = metrics[index];
            const auto left    = origin + static_cast<float>(metric.leftSideBearing) * units * scaleX;
            const auto partWidth =
                (static_cast<float>(metric.advanceWidth) - static_cast<float>(metric.leftSideBearing) - static_cast<float>(metric.rightSideBearing)) * units *
                scaleX;
            const auto partTop = baseline + (static_cast<float>(metric.topSideBearing) - static_cast<float>(metric.verticalOriginY)) * units;
            const auto partHeight =
                (static_cast<float>(metric.advanceHeight) - static_cast<float>(metric.topSideBearing) - static_cast<float>(metric.bottomSideBearing)) * units;
            if (! std::isfinite(origin + left + partWidth + partTop + partHeight) || partWidth < 0.f || partHeight < 0.f || left < x - .01f ||
                left + partWidth > x + width + .01f)
                return false;
            staged[index] = {glyph, origin, baseline, size, scaleX, left, partTop, partWidth, partHeight};
        }
        variants.insert(variants.end(), staged.begin(), staged.begin() + static_cast<std::ptrdiff_t>(count));
        return true;
    }
    void Fence(wchar_t shape, float x, float top, float bottom, float width, float thickness, float size)
    {
        if (shape == L'.')
            return;
        if (AddVerticalVariant(shape, x, top, bottom, width, size))
            return;
        const bool right  = shape == L')' || shape == L']' || shape == L'}' || shape == L'⟩' || shape == L'⌋' || shape == L'⌉' || shape == L'>';
        const auto outer  = right ? x + width : x;
        const auto inner  = right ? x : x + width;
        const auto middle = (top + bottom) / 2.f;
        if (shape == L'↑' || shape == L'↓' || shape == L'↕' || shape == L'⇑' || shape == L'⇓' || shape == L'⇕')
        {
            const bool doubled = shape == L'⇑' || shape == L'⇓' || shape == L'⇕';
            const bool up      = shape == L'↑' || shape == L'↕' || shape == L'⇑' || shape == L'⇕';
            const bool down    = shape == L'↓' || shape == L'↕' || shape == L'⇓' || shape == L'⇕';
            const float center = x + width * .5f;
            const float head   = std::min((bottom - top) * .25f, width * .7f);
            if (doubled)
            {
                Stroke(x + width / 3.f, top + (up ? head * .35f : 0.f), x + width / 3.f, bottom - (down ? head * .35f : 0.f), thickness);
                Stroke(x + 2.f * width / 3.f, top + (up ? head * .35f : 0.f), x + 2.f * width / 3.f, bottom - (down ? head * .35f : 0.f), thickness);
            }
            else
                Stroke(center, top, center, bottom, thickness);
            if (up)
            {
                Stroke(x, top + head, center, top, thickness);
                Stroke(center, top, x + width, top + head, thickness);
            }
            if (down)
            {
                Stroke(x, bottom - head, center, bottom, thickness);
                Stroke(center, bottom, x + width, bottom - head, thickness);
            }
        }
        else if (shape == L'(' || shape == L')')
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
        else if (shape == L'⟨' || shape == L'⟩' || shape == L'<' || shape == L'>')
        {
            Stroke(inner, top, outer, middle, thickness);
            Stroke(outer, middle, inner, bottom, thickness);
        }
        else if (shape == L'/' || shape == L'\\')
            Stroke(shape == L'/' ? x + width : x, top, shape == L'/' ? x : x + width, bottom, thickness);
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
            const float axisAtSize = MathConstantAt(12u, box.size, axis);
            const float scaledRule = std::max(1.f, MathConstantAt(144u, box.size, fractionRuleThickness));
            const float topGap     = MathConstantAt(box.display ? 140u : 136u, box.size, box.display ? fractionNumeratorDisplayGap : fractionNumeratorGap);
            const float bottomGap  = MathConstantAt(box.display ? 152u : 148u, box.size, box.display ? fractionDenominatorDisplayGap : fractionDenominatorGap);
            const float topShift =
                std::max(MathConstantAt(box.display ? 124u : 120u, box.size, box.display ? fractionNumeratorDisplayShift : fractionNumeratorShift),
                         axisAtSize + top.below + topGap + scaledRule * .5f);
            const float bottomShift =
                std::max(MathConstantAt(box.display ? 132u : 128u, box.size, box.display ? fractionDenominatorDisplayShift : fractionDenominatorShift),
                         bottom.above + bottomGap + scaledRule * .5f - axisAtSize);
            const float ruleY = baseline - axisAtSize;
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
            if (limitsAboveBelow)
            {
                const float center           = x + box.extent.width / 2.f;
                const float baseX            = center - base.width / 2.f;
                const float baseAboveShift   = base.above + MathConstantAt(64u, box.size, upperLimitGap);
                const float baseBelowShift   = base.below + MathConstantAt(72u, box.size, lowerLimitGap);
                const float italicCorrection = ItalicCorrection(baseIndex) * .75f;
                Place(baseIndex, baseX, baseline);
                if (boxes[super].extent.width > 0.f)
                    Place(super, center - boxes[super].extent.width / 2.f + italicCorrection / 2.f, baseline - baseAboveShift - boxes[super].extent.below);
                if (boxes[sub].extent.width > 0.f)
                    Place(sub, center - boxes[sub].extent.width / 2.f - italicCorrection / 2.f, baseline + baseBelowShift + boxes[sub].extent.above);
                return;
            }
            const float subShift   = std::max(MathConstantAt(24u, box.size, subscriptShift), base.below + box.size * .12f);
            const float superShift = std::max(MathConstantAt(36u, box.size, superscriptShift), base.above - box.size * .08f);
            const float gapNeed =
                MathConstantAt(52u, box.size, scriptGapMinimum) - (superShift + subShift - boxes[super].extent.below - boxes[sub].extent.above);
            const float correction = std::max(0.f, gapNeed / 2.f);
            const auto baseX       = x + box.scriptLeftPad;
            Place(baseIndex, baseX, baseline);
            Place(sub, baseX + ScriptBaseAdvance(baseIndex) + box.subKern, baseline + subShift + correction);
            Place(super, baseX + ScriptAnchor(baseIndex) + box.superKern, baseline - superShift - correction);
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
                Fence(node.text[0u], x, top, bottom, width, stroke, box.size);
            if (rightVisible)
                Fence(node.text[1u], x + box.extent.width - rightAdvance, top, bottom, width, stroke, box.size);
            return;
        }
        if (node.kind == Delimiter)
        {
            const auto width = box.extent.width;
            Fence(node.text.empty() ? L'.' : node.text[0], x, baseline - box.extent.above, baseline + box.extent.below, width, stroke, box.size);
            return;
        }
        if (node.kind == Accent && node.children.size() == 1u)
        {
            Place(ChildAt(index, 0u), x, baseline);
            const float y    = node.text == L"underline" ? baseline + box.extent.below - 2.f : baseline - box.extent.above + box.size * .1f;
            const auto shape = node.text == L"widehat" ? L'\u0302' : node.text == L"widetilde" ? L'\u0303' : node.text == L"vec" ? L'\u20d7' : L'\0';
            if (shape && AddHorizontalAccent(shape, x, baseline - box.extent.above, baseline - box.extent.above + box.size * .3f, box.extent.width, box.size))
                return;
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
            std::array<float, MathMaximumColumns> widths{};
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
                Fence(left, x + 2.f, top, bottom, box.size * .2f, stroke, box.size);
                if (node.text != L"cases")
                    Fence(right, x + box.extent.width - box.size * .2f - 2.f, top, bottom, box.size * .2f, stroke, box.size);
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
        variants.reserve(boxes.size() * 2u);
        Place(0u, 0.f, extent.above);
        float left{}, right = extent.width, top{}, bottom = extent.above + extent.below;
        for (const auto& glyph : glyphs)
        {
            left   = std::min(left, glyph.origin.x - glyph.leftOverhang);
            right  = std::max(right, glyph.origin.x + glyph.extent.width + glyph.rightOverhang);
            top    = std::min(top, glyph.origin.y - glyph.topOverhang);
            bottom = std::max(bottom, glyph.origin.y + glyph.extent.above + glyph.extent.below + glyph.bottomOverhang);
        }
        for (const auto& variant : variants)
        {
            left   = std::min(left, variant.left);
            right  = std::max(right, variant.left + variant.width);
            top    = std::min(top, variant.top);
            bottom = std::max(bottom, variant.top + variant.height);
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
        for (auto& variant : variants)
        {
            variant.x += shiftX;
            variant.baseline += shiftY;
            variant.left += shiftX;
            variant.top += shiftY;
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
    PreparedMath(IDWriteFactory* write,
                 std::shared_ptr<const MathExpression> value,
                 float size,
                 bool display                             = false,
                 const PreparedMath* constants            = nullptr,
                 float deviceDpi                          = 96.f,
                 std::shared_ptr<MathTextFormats> formats = {})
        : expression(std::move(value)),
          factory(write),
          textFormats(std::move(formats)),
          glyphs(0u),
          boxes(0u),
          lines(0u),
          variants(0u),
          font(size),
          dpi(deviceDpi),
          displayStyle(display)
    {
        if (! expression || ! *expression || ! std::isfinite(size) || size <= 0.f || size > 4096.f || ! std::isfinite(deviceDpi) || deviceDpi <= 0.f ||
            deviceDpi > 4096.f || ! write)
            throw std::invalid_argument("Invalid mathematical preparation request");
#if DXUI_ENABLE_DIAGNOSTICS
        ++testRichMathPreparations;
#endif
        const auto count = CountAndValidate(expression->root);
        if (! textFormats)
            textFormats = std::make_shared<MathTextFormats>(write);
        if (textFormats->Factory() != write)
            throw std::invalid_argument("Mathematical formats belong to another factory");
        boxes.reserve(count);
        glyphs.reserve(count);
        axis = font * .25f;
        InitializeFallbackConstants();
        if (constants && constants->factory.get() == write && constants->textFormats->Family() == textFormats->Family())
            CopyMathConstants(*constants);
        else
            ReadMathConstants(write);
        Flatten(expression->root, font, false, false, false, displayStyle, 0u);
        // Only direct single-glyph script operands can use MATH corner data.
        // Compound boxes use the OpenType zero-kern fallback.
        for (std::size_t i = 0u; i < boxes.size(); ++i)
            if (boxes[i].node->kind == MathKind::Scripts && boxes[i].node->children.size() == 3u)
                for (std::size_t child = 0u; child < 3u; ++child)
                {
                    const auto index = ChildAt(i, child);
                    if (boxes[index].glyph != std::numeric_limits<std::size_t>::max())
                        glyphs[boxes[index].glyph].needsKern = true;
                }
    }
    bool Advance()
    {
        if (finalized)
            return true;
        if (cursor < glyphs.size())
        {
            auto& leaf   = glyphs[cursor];
            auto* format = textFormats->Get(leaf.font, leaf.bold, leaf.italic);
            wil::com_ptr<IDWriteTextLayout> layout;
            THROW_IF_FAILED(
                factory->CreateTextLayout(leaf.node->text.data(), static_cast<UINT32>(leaf.node->text.size()), format, 65536.f, 65536.f, layout.put()));
            DWRITE_TEXT_METRICS metrics{};
            THROW_IF_FAILED(layout->GetMetrics(&metrics));
            DWRITE_LINE_METRICS line{};
            UINT32 lineCount{};
            HRESULT hr{};
#if DXUI_ENABLE_DIAGNOSTICS
            if (FAILED(testRichMathLineMetricsFailure))
            {
                hr                             = testRichMathLineMetricsFailure;
                testRichMathLineMetricsFailure = S_OK;
            }
            else
#endif
            {
                hr = layout->GetLineMetrics(&line, 1u, &lineCount);
            }
            // DirectWrite reports a too-small line array with this HRESULT
            // while still returning the required line count. That is the
            // expected local capacity signal for our one-line leaf.
            if (hr == E_NOT_SUFFICIENT_BUFFER && lineCount > 1u)
                throw std::length_error("Mathematical text leaf must occupy one line");
            THROW_IF_FAILED(hr);
            if (lineCount != 1u)
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
            const float italicCorrection = leaf.italic ? FontItalicCorrection(leaf.node->text, leaf.font).value_or(rightOverhang) : 0.f;
            if (leaf.needsKern && mathData)
            {
                auto probe = Microsoft::WRL::Make<MathGlyphProbe>();
                if (! probe)
                    throw std::bad_alloc();
                THROW_IF_FAILED(layout->Draw(nullptr, probe.Get(), 0.f, 0.f));
                if (probe->runs == 1u && probe->glyph && SameMathFontFile(probe->face.get(), mathData->face.get()))
                {
                    DWRITE_GLYPH_METRICS ink{};
                    if (OptionalFontOperation(probe->face->GetDesignGlyphMetrics(&probe->glyph, 1u, &ink, FALSE)))
                    {
                        const float scale = leaf.font / static_cast<float>(mathData->designUnits);
                        leaf.mathGlyph    = probe->glyph;
                        leaf.inkAbove     = static_cast<float>(static_cast<std::int32_t>(ink.verticalOriginY) - ink.topSideBearing) * scale;
                        leaf.inkBelow     = static_cast<float>(static_cast<std::int32_t>(ink.advanceHeight) - static_cast<std::int32_t>(ink.verticalOriginY) -
                                                               ink.bottomSideBearing) *
                                            scale;
                    }
                }
            }
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
    const auto& VariantLayouts() const noexcept
    {
        return variants;
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
        for (const auto& variant : variants)
        {
            D2D1_MATRIX_3X2_F current;
            target->GetTransform(&current);
            target->SetTransform(D2D1::Matrix3x2F::Scale(variant.scaleX, 1.f, D2D1::Point2F(variant.x, variant.baseline)) * current);
            const DWRITE_GLYPH_RUN run{mathData->face.get(), variant.fontSize, 1u, &variant.glyph, nullptr, nullptr, FALSE, 0u};
            target->DrawGlyphRun(D2D1::Point2F(variant.x, variant.baseline), &run, brush, DWRITE_MEASURING_MODE_NATURAL);
            target->SetTransform(current);
        }
        target->SetTransform(previous);
    }
};
} // namespace DxUi::RichText::Detail
