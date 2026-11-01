#include "DxUi.RichSemanticGeometry.h"
#include "DxUi.PreparedLayoutAccess.h"
#include "DxUi.PaintIndex.h"
#include "DxUi.PreparedMath.h"
#include "DxUi.RichImageResources.h"
#include "DxUi.RichLayoutOptions.h"
#include "DxUi.SemanticExtensions.h"
#include "Support/Diagnostics.h"
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <wil/resource.h>
#include <wil/result.h>
// Windows SDK WRL owns its interlocked/volatile implementation and target semantics.
// Scope is this system header only. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4746)
#include <wrl/implements.h>
#pragma warning(pop)

namespace DxUi::RichText::Detail
{
#if DXUI_ENABLE_DIAGNOSTICS
thread_local const RichImageRasterCalls* testRichImageRasterCalls{};
thread_local IDWriteFactory* testRichSemanticWriteFactory{};
thread_local HRESULT testRichMathLineMetricsFailure{S_OK};
thread_local unsigned testRichPreparationFailure{};
thread_local std::size_t testRichPreparationFailureAttempts{};
thread_local HRESULT testRichMathGlyphMetricsFailure{S_OK};
thread_local HRESULT testRichMathGlyphIndexFailure{S_OK};
thread_local std::size_t testRichMathConstantReads{};
thread_local std::size_t testRichMathPreparations{};
thread_local std::size_t testRichMathTextFormats{};
thread_local std::span<const std::uint8_t> testRichMathTableOverride{};
#endif
namespace
{
HRESULT UploadImageBitmap(
    ID2D1RenderTarget* target, D2D1_SIZE_U size, const void* pixels, UINT32 stride, const D2D1_BITMAP_PROPERTIES& properties, ID2D1Bitmap** bitmap) noexcept
{
#if DXUI_ENABLE_DIAGNOSTICS
    if (testRichImageRasterCalls && testRichImageRasterCalls->createBitmap)
        return testRichImageRasterCalls->createBitmap(target, size, pixels, stride, properties, bitmap);
#endif
    return target->CreateBitmap(size, pixels, stride, properties, bitmap);
}
HRESULT CopyImageBitmap(ID2D1Bitmap* bitmap, const D2D1_RECT_U& rectangle, const void* pixels, UINT32 stride) noexcept
{
#if DXUI_ENABLE_DIAGNOSTICS
    if (testRichImageRasterCalls && testRichImageRasterCalls->copyBitmap)
        return testRichImageRasterCalls->copyBitmap(bitmap, rectangle, pixels, stride);
#endif
    return bitmap->CopyFromMemory(&rectangle, pixels, stride);
}
// DirectWrite reserves and hit-tests one atomic graphic. Graphics are painted
// later on the known owner target; the opaque drawingContext is never cast.
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class SemanticObjectPlaceholder final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteInlineObject>
{
public:
    explicit SemanticObjectPlaceholder(DWRITE_INLINE_OBJECT_METRICS metrics) noexcept : metrics_(metrics)
    {
    }
    HRESULT STDMETHODCALLTYPE Draw(void*, IDWriteTextRenderer*, FLOAT, FLOAT, BOOL, BOOL, IUnknown*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetMetrics(DWRITE_INLINE_OBJECT_METRICS* metrics) noexcept override
    {
        if (! metrics)
            return E_POINTER;
        *metrics = metrics_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetOverhangMetrics(DWRITE_OVERHANG_METRICS* metrics) noexcept override
    {
        if (! metrics)
            return E_POINTER;
        *metrics = {};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetBreakConditions(DWRITE_BREAK_CONDITION* before, DWRITE_BREAK_CONDITION* after) noexcept override
    {
        if (! before || ! after)
            return E_POINTER;
        *before = *after = DWRITE_BREAK_CONDITION_NEUTRAL;
        return S_OK;
    }

private:
    DWRITE_INLINE_OBJECT_METRICS metrics_;
};
#pragma warning(pop)
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626)
struct NativeDiagramPlacement
{
    std::size_t element{}, display{};
    std::shared_ptr<PreparedFlowchart> paint;
    D2D1_RECT_F bounds{}; // Relative to the containing DirectWrite paragraph.
    float caretHeight{};
    bool rtl{};
};
#pragma warning(pop)
struct NativeDiagramGeometry
{
    NativeDiagramGeometry(std::size_t count, ID2D1Factory* factory, FlowchartResourceEpoch inputEpochs)
        : drawing(factory),
          epochs(inputEpochs),
          placements(count)
    {
    }
    wil::com_ptr<ID2D1Factory> drawing;
    FlowchartResourceEpoch epochs;
    std::vector<NativeDiagramPlacement> placements;
    NativeDiagramPlacement* Find(std::size_t element) noexcept
    {
        const auto found = std::lower_bound(placements.begin(), placements.end(), element, [](const auto& value, auto index) { return value.element < index; });
        return found != placements.end() && found->element == element ? &*found : nullptr;
    }
};
struct SemanticBox;
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626)
struct NativeImagePlacement
{
    std::size_t element{};
    MarkdownRange display;
    std::shared_ptr<const RichImage> pixels;
    wil::com_ptr<ID2D1Bitmap> bitmap;
    std::shared_ptr<PreparedMath> math;
    D2D1_SIZE_F Size() const noexcept
    {
        return math ? math->Size() : D2D1::SizeF(static_cast<float>(pixels->Width()), static_cast<float>(pixels->Height()));
    }
    float Baseline() const noexcept
    {
        return math ? math->Baseline() : Size().height;
    }
    D2D1_RECT_F bounds{};
    const SemanticBox* owner{};
    float caretHeight{}, mathCaretTop{};
    bool hasMathCaret{};
    UINT32 uploadedX{}, uploadedY{}; // Staging only; never published partially.
};
#pragma warning(pop)
struct NativeImageGeometry
{
    NativeImageGeometry(const RichImageResourceSet& resources,
                        const SemanticDocument& document,
                        ID2D1RenderTarget* target,
                        IDWriteFactory* write,
                        const LayoutOptions& options,
                        const NativeImageGeometry* previous,
                        std::vector<std::size_t>& rejected)
        : placements(resources.placements.size() + SemanticExtensionAccess::Maths(document).size()),
          unsupportedPixels(0u),
          targetIdentity(target)
    {
        if (! target)
            throw std::invalid_argument("Image raster preparation requires the owner's target");
        if (placements.size() > RichImageResources::MaximumPlacements)
            throw NativeObjectCapacity("Too many prepared image placements");
        std::size_t bytes{};
        for (std::size_t index = 0u; index < resources.placements.size(); ++index)
        {
            const auto& resource = resources.placements[index];
            if (! resource.image || resource.element >= document.Elements().size() || document.Elements()[resource.element].role != SemanticRole::Image)
                throw std::invalid_argument("Invalid prepared image resource binding");
            auto& placement   = placements[index];
            placement.element = resource.element;
            placement.display = document.Elements()[resource.element].display;
            placement.pixels  = resource.image;
            const auto prior  = std::find_if(resources.placements.begin(),
                                             resources.placements.begin() + static_cast<std::ptrdiff_t>(index),
                                             [&](const auto& value) { return value.image == resource.image; });
            if (prior == resources.placements.begin() + static_cast<std::ptrdiff_t>(index))
            {
                if (resource.image->Pixels().size() > RichImageResources::MaximumDecodedBytes - bytes)
                    throw NativeObjectCapacity("Prepared image pixel capacity exceeded");
                bytes += resource.image->Pixels().size();
            }
        }
        const auto maths              = SemanticExtensionAccess::Maths(document);
        const PreparedMath* constants = nullptr;
        std::shared_ptr<MathTextFormats> textFormats;
        for (std::size_t index = 0; index < maths.size(); ++index)
        {
            auto& placement   = placements[resources.placements.size() + index];
            placement.element = maths[index].element;
            placement.display = document.Elements()[placement.element].display;
            float font        = options.fontSizeDip;
            const auto runs   = document.Runs();
            const auto run    = std::upper_bound(
                runs.begin(), runs.end(), placement.display.first, [](std::size_t offset, const StyleRun& candidate) { return offset < candidate.start; });
            if (run != runs.begin())
            {
                const auto& candidate = *std::prev(run);
                if (placement.display.first - candidate.start < candidate.length)
                {
                    if ((candidate.flags & Code) && options.codeFontSizeDip > 0.0f)
                        font = options.codeFontSizeDip;
                    if (candidate.heading > 0u && candidate.heading <= options.headingFontSizeDip.size())
                    {
                        const float overrideSize = options.headingFontSizeDip[candidate.heading - 1u];
                        font = overrideSize > 0.0f ? overrideSize : font * (1.0f + options.headingScaleStep * static_cast<float>(7u - candidate.heading));
                    }
                }
            }
            // PreparedMath construction validates bounded AST storage and reads
            // optional MATH constants without HRESULT propagation. Font-local
            // SDK operations occur only in PrepareMath below, where the exact
            // affected formula is known. Bounded AST validation can still
            // reject this formula as a local geometry-capacity failure.
            try
            {
                if (previous)
                {
                    const auto prior =
                        std::lower_bound(previous->placements.begin(), previous->placements.end(), placement.element, [](const auto& value, auto element) {
                        return value.element < element;
                    });
                    if (prior != previous->placements.end() && prior->element == placement.element)
                        placement.math = prior->math;
                }
                if (! placement.math)
                {
                    if (! textFormats)
                        textFormats = std::make_shared<MathTextFormats>(write, options.mathFontFamily, options.locale);
                    placement.math =
                        std::make_shared<PreparedMath>(write, maths[index].expression, font * 1.15f, maths[index].display, constants, options.dpi, textFormats);
                }
                if (! constants)
                    constants = placement.math.get();
            }
            catch (const std::length_error&)
            {
                rejected.push_back(placement.element);
            }
        }
        std::erase_if(placements, [](const auto& placement) { return ! placement.math && ! placement.pixels; });
        std::ranges::sort(placements, {}, &NativeImagePlacement::element);
    }
    std::vector<NativeImagePlacement> placements;
    std::vector<const RichImage*> unsupportedPixels;
    ID2D1RenderTarget* targetIdentity; // Identity only after staging; never dereferenced here.
    struct UploadProgress
    {
        bool complete{}, attempted{}, unsupported{};
        std::size_t bytes{};
    };
    UploadProgress PrepareOne(std::size_t index, ID2D1RenderTarget* target, std::size_t maximumBytes)
    {
        auto& placement = placements[index];
        if (std::ranges::find(unsupportedPixels, placement.pixels.get()) != unsupportedPixels.end())
            return {true, false, true, 0u};
        const auto reject = [&]
        {
            unsupportedPixels.push_back(placement.pixels.get());
            Debug::Warning(L"DxUi: image upload is unsupported by this target; the image alternative is retained.");
            return UploadProgress{true, true, true, 0u};
        };
        const auto width = placement.pixels->Width(), height = placement.pixels->Height();
        if (! placement.bitmap)
        {
            const auto found = std::find_if(placements.begin(), placements.begin() + static_cast<std::ptrdiff_t>(index), [&](const auto& prior) {
                return prior.pixels == placement.pixels;
            });
            if (found != placements.begin() + static_cast<std::ptrdiff_t>(index))
            {
                placement.bitmap = found->bitmap;
                return {true, false, false, 0u};
            }
            const auto bytes      = static_cast<std::size_t>(width) * height * 4u;
            const bool whole      = bytes <= maximumBytes;
            const auto properties = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
            const auto result     = UploadImageBitmap(target,
                                                      D2D1::SizeU(width, height),
                                                      whole ? placement.pixels->Pixels().data() : nullptr,
                                                      whole ? placement.pixels->Stride() : 0u,
                                                      properties,
                                                      placement.bitmap.put());
            if (result == D2DERR_UNSUPPORTED_OPERATION)
                return reject();
            THROW_IF_FAILED(result);
            if (whole)
                return {true, true, false, bytes};
        }
        const auto columns = static_cast<UINT32>(std::min(static_cast<std::size_t>(width - placement.uploadedX), maximumBytes / 4u));
        const auto rows =
            placement.uploadedX == 0u && columns == width
                ? static_cast<UINT32>(std::min(static_cast<std::size_t>(height - placement.uploadedY), maximumBytes / (static_cast<std::size_t>(width) * 4u)))
                : 1u;
        const D2D1_RECT_U rectangle{placement.uploadedX, placement.uploadedY, placement.uploadedX + columns, placement.uploadedY + rows};
        const auto* pixels =
            placement.pixels->Pixels().data() + static_cast<std::size_t>(placement.uploadedY) * placement.pixels->Stride() + placement.uploadedX * 4u;
        const auto result = CopyImageBitmap(placement.bitmap.get(), rectangle, pixels, placement.pixels->Stride());
        if (result == D2DERR_UNSUPPORTED_OPERATION)
            return reject();
        THROW_IF_FAILED(result);
        placement.uploadedX += columns;
        if (placement.uploadedX == width)
        {
            placement.uploadedX = 0u;
            placement.uploadedY += rows;
        }
        return {placement.uploadedY == height, true, false, static_cast<std::size_t>(columns) * rows * 4u};
    }
    bool PrepareMath(std::size_t index)
    {
        try
        {
            return placements[index].math->Advance();
        }
        catch (const wil::ResultException& failure)
        {
            // This DirectWrite result is scoped to the current formula/font:
            // the operation is unsupported by the selected font type. Other
            // HRESULTs, including device loss and memory exhaustion, stay
            // retryable and retain their original error channel.
            if (failure.GetErrorCode() == DWRITE_E_UNSUPPORTEDOPERATION)
                throw NativeObjectUnsupported("Mathematical operation is unsupported by the selected font", placements[index].element, failure.GetErrorCode());
            throw;
        }
        catch (const std::length_error&)
        {
            // Admission of one native formula is a local presentation failure.
            // Driver/HRESULT and allocation failures retain their own channels.
            throw NativeObjectCapacity("Mathematical object exceeds its font/geometry capacity", placements[index].element);
        }
    }
    bool Placed(std::size_t element) const noexcept
    {
        const auto found = std::lower_bound(placements.begin(), placements.end(), element, [](const auto& value, auto index) { return value.element < index; });
        return found != placements.end() && found->element == element && found->owner;
    }
};
struct SemanticBox
{
    PreparedLayout layout;
    std::vector<D2D1_RECT_F> codeChips{0u};
    std::vector<D2D1_RECT_F> markChips{0u};
    SemanticParagraph paragraph;
    float x{}, y{}, width{}, height{}, codeGutter{};
    D2D1_RECT_F frame{};
    std::optional<MarkdownEmptyLine> sourceCaret;
    bool caretOnly{}, markerOnly{}, markerLayout{};
    const NativeDiagramPlacement* native{};
    const NativeImageGeometry* images{};
    CaretGeometry Caret(std::size_t local) const
    {
        if (! native)
        {
            auto caret = layout.Caret(local);
            if (images)
                for (const auto& image : images->placements)
                    if (image.owner == this &&
                        (paragraph.display.first + local == image.display.first || paragraph.display.first + local == image.display.last))
                    {
                        if (image.math && image.hasMathCaret &&
                            (layout.LineBoundary(local, false) == layout.LineBoundary(image.display.first - paragraph.display.first, false) ||
                             (local == layout.Content().text.size() && image.display.last == paragraph.display.last)))
                        {
                            caret.y      = image.mathCaretTop;
                            caret.height = image.caretHeight;
                        }
                        else if (! image.math)
                        {
                            caret.height = std::min(image.caretHeight, image.bounds.bottom - image.bounds.top);
                            caret.y      = image.bounds.bottom - caret.height;
                        }
                        break;
                    }
            return caret;
        }
        const auto object = native->display - paragraph.display.first;
        if (local < object)
        {
            auto caret   = layout.Caret(local);
            caret.height = std::min(caret.height, native->caretHeight);
            return caret;
        }
        const auto& bounds     = native->bounds;
        const auto caretHeight = std::min(native->caretHeight, bounds.bottom - bounds.top);
        const bool after       = local > object;
        return {after != native->rtl ? bounds.right : bounds.left, after ? bounds.bottom - caretHeight : bounds.top, caretHeight};
    }
};
struct MarkdownDecorations
{
    // Debug iterator proxies allocate: choose fallible size constructors.
    MarkdownDecorations() : tableRows(0u), headingRules(0u)
    {
    }
    std::vector<D2D1_RECT_F> tableRows, headingRules;
};
bool SameOptions(const LayoutOptions& a, const LayoutOptions& b) noexcept
{
    return SameLayoutOptions(a, b, LayoutOptionsUse::Semantic);
}
void Union(std::optional<D2D1_RECT_F>& bounds, const D2D1_RECT_F& rectangle) noexcept
{
    if (! bounds)
        bounds = rectangle;
    else
    {
        bounds->left   = std::min(bounds->left, rectangle.left);
        bounds->top    = std::min(bounds->top, rectangle.top);
        bounds->right  = std::max(bounds->right, rectangle.right);
        bounds->bottom = std::max(bounds->bottom, rectangle.bottom);
    }
}
// Stack-only rounding/observation barrier; no inter-thread synchronization or memory-mapped I/O.
// Both target volatile models preserve these accesses. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4746)
float AddHeight(float y, float height)
{
    volatile float next = y + height;
    if (! std::isfinite(next) || next < y)
        throw std::overflow_error("Nonfinite semantic geometry extent");
    return next;
}
#pragma warning(pop)
void SemanticDisplayInput(const SemanticDocument& document, MarkdownRange range, PreparedLayoutAccess::SemanticInput& projection)
{
    const auto text     = document.Map().Display().substr(range.first, range.last - range.first);
    projection.revision = document.Syntax()->Revision();
    projection.text.assign(text);
    const auto runs  = document.Runs();
    const auto first = std::lower_bound(runs.begin(), runs.end(), range.first, [](const auto& run, auto offset) { return run.start + run.length <= offset; });
    for (auto run = first; run != runs.end() && run->start < range.last; ++run)
    {
        const auto begin = std::max(run->start, range.first), end = std::min(run->start + run->length, range.last);
        projection.runs.push_back({begin - range.first, end - begin, run->flags, run->heading});
    }
}
} // namespace
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626)
struct PreparedSemanticGeometry::State
{
    State(std::shared_ptr<const SemanticDocument> input,
          IDWriteFactory* writeFactory,
          const LayoutOptions& inputOptions,
          ID2D1Factory* drawing,
          FlowchartResourceEpoch epochs,
          std::shared_ptr<const RichImageResourceSet> inputImages,
          ID2D1RenderTarget* raster,
          const NativeImageGeometry* previousImages)
        : document(std::move(input)),
          factory(writeFactory),
          options(inputOptions),
          imageResources(std::move(inputImages)),
          imageEpochs(epochs),
          rejected(0u),
          boxes(0u),
          codeNumbers(0u),
          glyphBoxes(0u),
          nodeElements(document->Syntax()->Nodes().size(), MarkdownNoNode),
          bounds(document->Elements().size()),
          sourceLines(0u) // Fallible size constructor: MSVC Debug's noexcept default allocates an iterator proxy.
    {
        width = options.widthDip;
        if (options.markdownPresentation == MarkdownPresentationStyle::Subtle)
        {
            decorations = std::make_unique<MarkdownDecorations>();
            if (! document->Syntax()->SourceRegion())
                codeLabels = std::make_unique<std::vector<std::unique_ptr<PreparedLayout>>>(document->Elements().size());
        }
        if ((imageResources && ! imageResources->placements.empty()) || ! SemanticExtensionAccess::Maths(*document).empty())
        {
            const RichImageResourceSet empty;
            images = std::make_unique<NativeImageGeometry>(
                imageResources ? *imageResources : empty, *document, raster, factory.get(), options, previousImages, rejected);
        }
        const auto diagrams = SemanticExtensionAccess::Diagrams(*document);
        if (! diagrams.empty())
        {
            native = std::make_unique<NativeDiagramGeometry>(diagrams.size(), drawing, epochs);
            for (std::size_t index = 0u; index < diagrams.size(); ++index)
            {
                native->placements[index].element = diagrams[index].element;
                native->placements[index].display = diagrams[index].display.first;
            }
        }
    }
    std::shared_ptr<const SemanticDocument> document;
    wil::com_ptr<IDWriteFactory> factory;
    LayoutOptions options;
    std::shared_ptr<const RichImageResourceSet> imageResources;
    FlowchartResourceEpoch imageEpochs;
    std::vector<std::size_t> rejected;
    void Reject(std::size_t element)
    {
        if (std::ranges::find(rejected, element) == rejected.end())
        {
            if (rejected.size() == SemanticExtensions::MaximumObjects)
                throw NativeObjectCapacity("Native rejection set exceeds object capacity");
            rejected.push_back(element);
        }
    }
    std::unique_ptr<NativeImageGeometry> images;
    std::unique_ptr<NativeDiagramGeometry> native;
    std::vector<std::unique_ptr<SemanticBox>> boxes;
    std::unique_ptr<MarkdownDecorations> decorations;
    std::unique_ptr<std::vector<std::unique_ptr<PreparedLayout>>> codeLabels;
    struct CodeNumber
    {
        std::unique_ptr<PreparedLayout> layout;
        D2D1_POINT_2F origin{};
        D2D1_RECT_F clip{};
    };
    std::vector<CodeNumber> codeNumbers;
    PaintIndex paintBoxes, paintElements, paintNumbers, paintHeadingRules, paintTableRows;
    // Source caret boxes are in physical source order, while their zero-width
    // display anchors may coincide with or follow generated marker text. Keep
    // the glyph search index in display order instead of assuming both orders
    // are interchangeable.
    std::vector<std::size_t> glyphBoxes;
    std::vector<std::size_t> nodeElements;
    std::vector<std::optional<D2D1_RECT_F>> bounds;
    std::vector<std::pair<std::size_t, std::size_t>> sourceLines;
    float height{}, width{};
    std::size_t codeChipCount{};
    bool codeChipsAvailable = true;
    bool markedText{};
    std::size_t reusedBoxes{};
    const SemanticBox& BoxAt(std::size_t position) const
    {
        if (position > document->Map().Display().size() || boxes.empty())
            throw std::out_of_range("Invalid semantic display offset");
        if (glyphBoxes.empty())
            return *boxes.front();
        auto found = std::lower_bound(
            glyphBoxes.begin(), glyphBoxes.end(), position, [&](auto index, auto offset) { return boxes[index]->paragraph.display.last <= offset; });
        if (found == glyphBoxes.end())
            found = std::prev(glyphBoxes.end());
        else if (boxes[*found]->paragraph.display.first > position && found != glyphBoxes.begin())
            --found;
        return *boxes[*found];
    }
    bool Belongs(const SemanticBox& box, std::size_t source) const noexcept
    {
        if (box.sourceCaret)
        {
            const auto& line = *box.sourceCaret;
            return source == line.insertion || (source >= line.source.first && source < line.source.last);
        }
        const auto range = document->Syntax()->Nodes()[box.paragraph.syntaxNode].block.source;
        return source >= range.first && source <= range.last;
    }
    std::size_t SourceBox(std::size_t source, std::size_t site) const
    {
        if (! IsEditBoundary(document->Map().Source(), source))
            throw std::out_of_range("Invalid semantic source caret offset");
        if (site < boxes.size() && Belongs(*boxes[site], source))
            return site;
        for (std::size_t i = 0u; i < boxes.size(); ++i)
            if (boxes[i]->sourceCaret && Belongs(*boxes[i], source))
            {
                const auto& owner = document->Elements()[boxes[i]->paragraph.element];
                if (owner.role != SemanticRole::Paragraph || owner.source.first != owner.source.last)
                    return i; // Generated backlinks cannot steal a shared source seam.
            }
        const auto* chosen = &BoxAt(document->Map().ToDisplay(source));
        const auto found   = std::find_if(boxes.begin(), boxes.end(), [&](const auto& box) { return box.get() == chosen; });
        return static_cast<std::size_t>(found - boxes.begin());
    }
    std::size_t NearestBox(float x, float y) const
    {
        if (! std::isfinite(x) || ! std::isfinite(y))
            throw std::invalid_argument("Invalid semantic hit coordinates");
        std::size_t chosen = 0u;
        double best        = std::numeric_limits<double>::max();
        for (std::size_t i = 0u; i < boxes.size(); ++i)
        {
            const auto& frame     = boxes[i]->frame;
            const float dy        = y < frame.top ? frame.top - y : y > frame.bottom ? y - frame.bottom : 0.0f;
            const float dx        = x < frame.left ? frame.left - x : x > frame.right ? x - frame.right : 0.0f;
            const double distance = static_cast<double>(dy) * 100000.0 + static_cast<double>(dx);
            if (distance < best || (distance == best && boxes[chosen]->markerOnly && ! boxes[i]->markerOnly))
            {
                chosen = i;
                best   = distance;
            }
        }
        return chosen;
    }
    SourceCaretHit HitSourceBox(std::size_t index, float x, float y) const
    {
        const auto& box = *boxes[index];
        if (box.sourceCaret)
            return {box.sourceCaret->insertion, index};
        const auto hit      = box.layout.HitTest(x - box.x, y - box.y);
        const auto affinity = hit.trailing ? MappingAffinity::Trailing : MappingAffinity::Leading;
        const auto display  = document->Map().SnapDisplayBoundary(box.paragraph.display.first + hit.position, affinity);
        return {document->Map().ToSource(display, affinity), index};
    }
};
#pragma warning(pop)
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626)
struct PreparedSemanticGeometry::Preparation
{
    enum class Phase
    {
        IndexReuse,
        IndexElements,
        IndexLiteralDefinitions,
        IndexCaretAnchors,
        IndexCaretSites,
        CountSites,
        BodyLine,
        NativeObjects,
        NativeImages,
        CodeLabels,
        MarkerLayouts,
        TableAllocate,
        Layouts,
        CodeNumbers,
        RowPlacement,
        IndexSourceBoxes,
        SourceLines,
        InlineBounds,
        ParentBounds,
        PaintIndex,
        Publish
    };
    explicit Preparation(std::shared_ptr<const SemanticDocument> document,
                         IDWriteFactory* factory,
                         const LayoutOptions& options,
                         ID2D1Factory* drawing,
                         FlowchartResourceEpoch epochs,
                         std::shared_ptr<const RichImageResourceSet> images,
                         ID2D1RenderTarget* target,
                         const NativeImageGeometry* previousImages,
                         const State* previous,
                         const State* previousNative)
        : state(std::make_unique<State>(std::move(document), factory, options, drawing, epochs, std::move(images), target, previousImages)),
          raster(state->images ? target : nullptr),
          previousState(previous),
          previousNativeState(previousNative),
          reusable(std::less<std::uint64_t>{}),
          emptyLayouts(0u),
          markers(state->document->Paragraphs().size()),
          gutters(state->document->Syntax()->Nodes().size(), 0.0f),
          literalDefinitionNodes(0u),
          footerRanges(0u),
          nodeSections(0u),
          caretAnchors(0u),
          caretSites(0u),
          sourceBoxes(0u)
    {
        if (options.markdownPresentation == MarkdownPresentationStyle::Subtle)
            tableSizing = std::make_unique<std::vector<std::unique_ptr<TableSizing>>>(0u);
        if (options.listItemSpacingDip)
            spacingItems = std::make_unique<std::vector<std::size_t>>(state->document->Syntax()->Nodes().size(), MarkdownNoNode);
        if (options.paragraphSpacingAfterLines && options.lineHeightMultiplier > 0.0f)
            SetBodyLineHeight(options.fontSizeDip * options.lineHeightMultiplier);
    }
    std::unique_ptr<State> state;
    wil::com_ptr<ID2D1RenderTarget> raster; // Retained only across cooperative staging; released before publication.
    const State* previousState{};
    const State* previousNativeState{};
    // MSVC's unordered hash-vector allocator constructor is noexcept even
    // though Debug iterator-proxy construction allocates. The ordered index
    // has fallible construction and bounded logarithmic per-item operations.
    std::multimap<std::uint64_t, std::size_t> reusable;
    std::size_t reuseIndex{};
    std::size_t paintGroup{}, paintCursor{}, paintCount{};
    unsigned paintPhase{};
    struct EmptyLayout
    {
        float width{};
        DWRITE_TEXT_ALIGNMENT alignment{};
        PreparedLayout layout;
    };
    std::vector<std::unique_ptr<EmptyLayout>> emptyLayouts;
    std::size_t nativeIndex{}, codeLabelIndex{}, codeNumberBox{}, codeNumberLine{}, codeNumberOffset{}, codeNumberValue = 1u;
    float codeNumberY{};
    bool codeNumberNeeded = true;
    std::size_t imageIndex{};
    std::vector<std::unique_ptr<SemanticBox>> markers;
    std::vector<float> gutters;
    std::vector<bool> literalDefinitionNodes;
    struct FooterRange
    {
        MarkdownRange source, display;
        std::size_t section{};
    };
    struct CaretAnchor
    {
        MarkdownRange source, display;
        std::size_t section{};
    };
    struct CaretSite
    {
        std::size_t index{}, display{};
    };
    std::vector<FooterRange> footerRanges;
    std::vector<std::size_t> nodeSections;
    std::vector<CaretAnchor> caretAnchors;
    std::vector<CaretSite> caretSites;
    std::vector<std::size_t> sourceBoxes;
    struct TableSizing
    {
        TableSizing() : preferred(0u), minimum(0u), widths(0u), probeWidths(0u), probeParagraphs(0u), probes(0u)
        {
        }
        std::size_t table{}, columns{};
        std::size_t widthIndex{};
        float indent{}, preferredTotal{}, minimumTotal{}, scale{};
        bool compressMinimum{};
        SemanticParagraph firstContent;
        std::vector<float> preferred, minimum, widths, probeWidths;
        std::vector<std::size_t> probeParagraphs;
        std::vector<std::unique_ptr<SemanticBox>> probes;
    };
    // Stable owners avoid MSVC Debug's allocating noexcept vector moves when
    // another table grows this collection during an allocation sweep.
    std::unique_ptr<std::vector<std::unique_ptr<TableSizing>>> tableSizing;
    // Only an opted-in list policy needs this bounded parent-first metadata.
    // A nullable owner avoids even Debug STL proxy allocation for legacy calls.
    std::unique_ptr<std::vector<std::size_t>> spacingItems;
    Phase phase = Phase::IndexReuse;
    std::size_t paragraph{}, rowStart{}, rowPlacement{}, rowTable = MarkdownNoNode, rowIndex{}, boundsIndex{}, tableAllocateIndex{};
    float y{}, rowHeight{};
    float rowLeft = std::numeric_limits<float>::max(), rowRight{};
    float flowBottom{}, emptyHeight{}, gapAfter{}, paragraphGap{};
    std::size_t flowItem = MarkdownNoNode;
    bool rowActive{};
    std::size_t emptyLine{}, plannedBoxes{}, markerIndex{}, lastLayoutCalls{};
    bool markerTableMeasured{};
    std::size_t lineBox{}, lineOffset{}, sourceLineStart{};
    bool HasSpacingPolicy() const noexcept
    {
        return state->options.paragraphSpacingAfterLines.has_value() || state->options.listItemSpacingDip.has_value() ||
               state->options.semanticSpacing.has_value();
    }
    void SetBodyLineHeight(float height)
    {
        paragraphGap = height * state->options.paragraphSpacingAfterLines.value();
        if (! std::isfinite(height) || height <= 0.0f || ! std::isfinite(paragraphGap))
            throw std::overflow_error("Nonfinite semantic paragraph spacing");
    }
    struct MathCaretFontMetric
    {
        unsigned flags{}, heading{};
        float size{}, ascent{}, gap{}, height{};
    };
    std::array<MathCaretFontMetric, 128u> mathCaretFonts{};
    std::size_t mathCaretFontCount{};
    wil::com_ptr<IDWriteFontCollection> mathCaretCollection;
    CaretGeometry StandaloneMathCaret(const SemanticBox& box, std::size_t position, const LayoutOptions& options, float lineTop, float baseline)
    {
        const auto& runs   = box.layout.Content().runs;
        const auto found   = std::ranges::find_if(runs, [&](const auto& run) { return run.start <= position && position - run.start < run.length; });
        const auto flags   = found == runs.end() ? 0u : found->flags;
        const auto heading = found == runs.end() ? 0u : found->heading;
        const auto& family = (flags & Code) ? options.codeFontFamily : options.fontFamily;
        float size         = (flags & Code) && options.codeFontSizeDip > 0.f ? options.codeFontSizeDip : options.fontSizeDip;
        if (heading)
            size = options.headingFontSizeDip[heading - 1u] > 0.f ? options.headingFontSizeDip[heading - 1u]
                                                                  : size * (1.f + options.headingScaleStep * static_cast<float>(7u - heading));
        const auto styleFlags = flags & (Bold | Italic | Code);
        for (std::size_t i = 0u; i < mathCaretFontCount; ++i)
        {
            const auto& metric = mathCaretFonts[i];
            if (metric.flags == styleFlags && metric.heading == heading && metric.size == size)
                return {0.f, lineTop + baseline - metric.ascent - metric.gap * .5f, metric.height};
        }
        const auto weight = heading ? ((flags & Bold) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_SEMI_BOLD)
                                    : ((flags & Bold) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL);
        const auto style  = (flags & Italic) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
        if (! mathCaretCollection)
            THROW_IF_FAILED(state->factory->GetSystemFontCollection(mathCaretCollection.put()));
        UINT32 index{};
        BOOL exists{};
        THROW_IF_FAILED(mathCaretCollection->FindFamilyName(family.c_str(), &index, &exists));
        if (! exists)
            THROW_IF_FAILED(mathCaretCollection->FindFamilyName(L"Segoe UI", &index, &exists));
        if (! exists)
            index = 0u;
        wil::com_ptr<IDWriteFontFamily> selected;
        THROW_IF_FAILED(mathCaretCollection->GetFontFamily(index, selected.put()));
        wil::com_ptr<IDWriteFont> font;
        THROW_IF_FAILED(selected->GetFirstMatchingFont(weight, DWRITE_FONT_STRETCH_NORMAL, style, font.put()));
        DWRITE_FONT_METRICS metrics{};
        font->GetMetrics(&metrics);
        if (! metrics.designUnitsPerEm)
            throw std::logic_error("Mathematical caret font has no design units");
        const float scale  = size / static_cast<float>(metrics.designUnitsPerEm);
        const float gap    = static_cast<float>(metrics.lineGap) * scale;
        const float ascent = static_cast<float>(metrics.ascent) * scale;
        const float height = (static_cast<float>(metrics.ascent) + static_cast<float>(metrics.descent) + static_cast<float>(metrics.lineGap)) * scale;
        if (! std::isfinite(height) || height <= 0.f)
            throw std::logic_error("Mathematical caret font has invalid line metrics");
        if (mathCaretFontCount < mathCaretFonts.size())
            mathCaretFonts[mathCaretFontCount++] = {styleFlags, heading, size, ascent, gap, height};
        return {0.f, lineTop + baseline - ascent - gap * .5f, height};
    }
    void MeasureBodyLine()
    {
        // Only native-height opt-in calls need an SDK measurement. Uniform
        // body heights are known from the same public size/multiplier policy.
        PreparedLayoutAccess::SemanticInput projection;
        PreparedLayout line;
        PreparedLayoutAccess::PrepareSemantic(line, state->factory.get(), projection, state->options);
        const auto metrics = PreparedLayoutAccess::LineMetrics(line);
        if (metrics.empty())
            throw std::logic_error("Empty body layout has no native line metrics");
        SetBodyLineHeight(metrics.front().height);
    }
    float IncomingGap(const SemanticParagraph& content) const noexcept
    {
        if (spacingItems && flowItem != MarkdownNoNode && content.markerOwner != MarkdownNoNode && content.markerOwner != flowItem)
            return state->options.listItemSpacingDip.value();
        if (state->options.semanticSpacing && content.heading > 0 && content.heading <= 6)
            return std::max(gapAfter, state->options.semanticSpacing->headingBeforeDip[content.heading - 1]);
        return gapAfter;
    }
    std::size_t ItemOwner(const SemanticParagraph& content) const noexcept
    {
        return spacingItems ? (*spacingItems)[content.syntaxNode] : MarkdownNoNode;
    }
    std::size_t EmptyIndex(std::size_t index) const noexcept
    {
        return caretSites.empty() ? index : caretSites[index].index;
    }
    const MarkdownEmptyLine& EmptyLine(std::size_t index) const noexcept
    {
        return state->document->Syntax()->EmptyLines()[EmptyIndex(index)];
    }
    std::size_t SectionAt(std::size_t source) const noexcept
    {
        const auto next =
            std::upper_bound(footerRanges.begin(), footerRanges.end(), source, [](auto offset, const auto& row) { return offset < row.source.first; });
        if (next == footerRanges.begin())
            return 0u;
        const auto& row = *std::prev(next);
        return source < row.source.last ? row.section : 0u;
    }
    std::size_t CaretSection(const MarkdownEmptyLine& line) const noexcept
    {
        auto section    = nodeSections[line.owner];
        const auto size = state->document->Map().Source().size();
        if (! section && line.source.first == size && size)
            section = SectionAt(size - 1u);
        return section;
    }
    std::size_t CaretDisplay(const MarkdownEmptyLine& line) const
    {
        const auto section = CaretSection(line);
        const auto found   = std::lower_bound(caretAnchors.begin(), caretAnchors.end(), std::pair{section, line.insertion}, [](const auto& anchor, auto key) {
            return anchor.section < key.first || (anchor.section == key.first && anchor.source.last <= key.second);
        });
        if (found != caretAnchors.end() && found->section == section)
        {
            if (found->source.first <= line.insertion)
                return state->document->Map().ToDisplay(line.insertion);
            const auto paragraphs     = state->document->Paragraphs();
            const auto foundParagraph = std::lower_bound(
                paragraphs.begin(), paragraphs.end(), found->display.first, [](const auto& content, auto display) { return content.display.last <= display; });
            // Generated number/marker prefixes precede their first raw glyph.
            // A preceding empty site belongs before the complete paragraph/row,
            // never between that prefix and its table cells or body glyphs.
            return foundParagraph == paragraphs.end() ? found->display.first : foundParagraph->display.first;
        }
        if (found != caretAnchors.begin() && std::prev(found)->section == section)
            return std::prev(found)->display.last;
        if (section)
        {
            const auto owner = state->nodeElements[section - 1u];
            return state->document->Elements()[owner].display.first;
        }
        // An all-hidden main document has a source insertion before its footer.
        return 0u;
    }
    bool EmptyBefore(std::size_t index, const SemanticParagraph& content) const
    {
        if (caretSites.empty())
            return EmptyLine(index).source.first <= SourceFirst(content);
        auto first = content.display.first;
        if (content.table != MarkdownNoNode)
        {
            const auto row = state->document->Syntax()->Nodes()[content.syntaxNode].parent;
            first          = state->document->Elements()[state->nodeElements[row]].display.first;
        }
        return caretSites[index].display <= first;
    }
    bool EmptyOwnsParagraph(std::size_t index, const SemanticParagraph& content) const noexcept
    {
        const auto& owner = state->document->Elements()[content.element];
        return content.syntaxNode == EmptyLine(index).owner && (owner.role != SemanticRole::Paragraph || owner.source.first != owner.source.last);
    }
    bool LiteralDefinitionLine(std::size_t index) const noexcept
    {
        const auto& syntax = *state->document->Syntax();
        // This membership is prepared once per node, not an ancestor walk per
        // blank line. Deep containers and repeated prefix-free blank lines must
        // not multiply geometry work. No table is allocated without a fallback.
        return ! literalDefinitionNodes.empty() && literalDefinitionNodes[syntax.EmptyLines()[EmptyIndex(index)].owner];
    }
    static bool SplitMarker(const SemanticParagraph& paragraph) noexcept
    {
        return paragraph.markerOwner != MarkdownNoNode && paragraph.markerEnd > paragraph.display.first && paragraph.markerEnd < paragraph.display.last;
    }
    float Gutter(const SemanticParagraph& content) const
    {
        float result      = 0.0f;
        const auto& nodes = state->document->Syntax()->Nodes();
        auto node         = content.syntaxNode;
        // Container indentation already caps at 64 levels. Marker columns use
        // the same bounded ancestor walk and are clamped again to the viewport.
        for (unsigned depth = 0u; node != MarkdownNoNode && depth < 64u; ++depth, node = nodes[node].parent)
        {
            const auto element = state->nodeElements[node];
            if (element != MarkdownNoNode && state->document->Elements()[element].role == SemanticRole::Footnote)
                break;
            if (nodes[node].block.kind == MarkdownBlockKind::ListItem)
                result += gutters[nodes[node].parent];
        }
        if (! std::isfinite(result))
            throw std::overflow_error("Nonfinite semantic marker gutter");
        return result;
    }
    bool MeasureMarkerOne()
    {
        const auto paragraphs = state->document->Paragraphs();
        if (markerIndex == paragraphs.size())
        {
            phase = state->options.markdownPresentation == MarkdownPresentationStyle::Subtle ? Phase::TableAllocate : Phase::Layouts;
            return false;
        }
        const auto index    = markerIndex;
        const auto& content = paragraphs[index];
        if (state->options.markdownPresentation == MarkdownPresentationStyle::Subtle && content.table != MarkdownNoNode && ! markerTableMeasured)
        {
            MeasureTableOne(index, content);
            markerTableMeasured = true;
            return true;
        }
        markerTableMeasured = false;
        ++markerIndex;
        if (content.markerOwner == MarkdownNoNode)
            return false;
        if (content.markerEnd <= content.display.first || content.markerEnd > content.display.last)
            throw std::logic_error("Invalid semantic marker prefix");
        auto box                    = std::make_unique<SemanticBox>();
        box->paragraph              = content;
        box->markerLayout           = true;
        box->paragraph.display.last = content.markerEnd;
        auto options                = state->options;
        // Native no-wrap measurement is separate from the body's width. Its
        // cached metrics retain the natural advance even for an unusual font.
        options.widthDip      = std::max(options.widthDip, options.fontSizeDip * 32.0f);
        options.textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        PreparedLayoutAccess::SemanticInput projection;
        SemanticDisplayInput(*state->document, box->paragraph.display, projection);
        bool reused      = false;
        const auto& node = state->document->Syntax()->Nodes()[content.syntaxNode];
        if (previousState && node.identity != 0u)
        {
            const auto range = reusable.equal_range(node.identity);
            for (auto found = range.first; found != range.second; ++found)
            {
                const auto& old      = *previousState->boxes[found->second];
                const auto& oldNodes = previousState->document->Syntax()->Nodes();
                if (! old.markerLayout || old.paragraph.markerOwner == MarkdownNoNode || content.markerOwner == MarkdownNoNode ||
                    old.paragraph.markerOwner >= oldNodes.size() || content.markerOwner >= state->document->Syntax()->Nodes().size() ||
                    oldNodes[old.paragraph.markerOwner].identity != state->document->Syntax()->Nodes()[content.markerOwner].identity)
                    continue;
                const auto& prior   = old.layout.Content();
                const bool sameRuns = prior.runs.size() == projection.runs.size() &&
                                      std::equal(prior.runs.begin(), prior.runs.end(), projection.runs.begin(), [](const auto& left, const auto& right) {
                    return left.start == right.start && left.length == right.length && left.flags == right.flags && left.heading == right.heading;
                });
                if (prior.text == projection.text && sameRuns &&
                    previousState->document->Syntax()->Slice(oldNodes[old.paragraph.syntaxNode].block.source) ==
                        state->document->Syntax()->Slice(node.block.source))
                {
                    box->layout = old.layout;
                    ++state->reusedBoxes;
                    reused = true;
                    break;
                }
            }
        }
        if (! reused)
            PreparedLayoutAccess::PrepareSemantic(box->layout, state->factory.get(), projection, options, true);
        const auto width = PreparedLayoutAccess::HorizontalTextBounds(box->layout).second;
        if (! std::isfinite(width) || width < 0.0f)
            throw std::overflow_error("Nonfinite semantic marker width");
        const auto list = state->document->Syntax()->Nodes()[content.markerOwner].parent;
        gutters[list]   = std::max(gutters[list], width);
        box->height     = box->layout.Height();
        markers[index]  = std::move(box);
        return ! reused;
    }
    std::size_t SourceFirst(const SemanticParagraph& content) const
    {
        const auto& nodes = state->document->Syntax()->Nodes();
        const auto& node  = nodes[content.syntaxNode];
        // Padded cells may be anchored after their row's EOL. Order every cell
        // by the physical row start, so an EOF/blank caret can never split a
        // table row between its real and generated cells.
        return content.table == MarkdownNoNode ? node.block.source.first : nodes[node.parent].block.source.first;
    }
    void FinishRow()
    {
        rowPlacement = rowStart;
        phase        = Phase::RowPlacement;
    }
    TableSizing* FindTableSizing(std::size_t table) noexcept
    {
        if (! tableSizing)
            return nullptr;
        const auto found = std::ranges::find_if(*tableSizing, [table](const auto& sizing) { return sizing->table == table; });
        return found == tableSizing->end() ? nullptr : found->get();
    }
    const TableSizing* FindTableSizing(std::size_t table) const noexcept
    {
        if (! tableSizing)
            return nullptr;
        const auto& tables = *tableSizing;
        const auto found   = std::ranges::find_if(tables, [table](const auto& sizing) { return sizing->table == table; });
        return found == tables.end() ? nullptr : found->get();
    }
    void MeasureTableOne(std::size_t paragraphIndex, const SemanticParagraph& content)
    {
        if (! content.columns || content.column >= content.columns || content.columns > SemanticDocument::MaximumParagraphs)
            throw std::logic_error("Invalid semantic table measurement");
        auto* sizing = FindTableSizing(content.table);
        if (! sizing)
        {
            if (! tableSizing)
                throw std::logic_error("Missing subtle table staging");
            tableSizing->push_back(std::make_unique<TableSizing>());
            sizing               = tableSizing->back().get();
            sizing->table        = content.table;
            sizing->columns      = content.columns;
            sizing->firstContent = content;
            sizing->preferred.assign(content.columns, 24.0f);
            sizing->preferredTotal = 24.0f * static_cast<float>(content.columns);
            sizing->minimum.assign(content.columns, 24.0f);
            sizing->minimumTotal = sizing->preferredTotal;
            sizing->probeWidths.assign(content.columns, -1.0f);
            sizing->probeParagraphs.assign(content.columns, MarkdownNoNode);
            sizing->probes.resize(content.columns);
        }
        if (sizing->columns != content.columns)
            throw std::logic_error("Inconsistent semantic table column count");
        LayoutOptions options = state->options;
        options.widthDip      = std::max(state->options.widthDip, 1048576.0f);
        options.textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        PreparedLayoutAccess::SemanticInput projection;
        SemanticDisplayInput(*state->document, content.display, projection);
        PreparedLayout measured;
        PreparedLayoutAccess::PrepareSemantic(measured, state->factory.get(), projection, options, true);
        const auto textBounds         = PreparedLayoutAccess::HorizontalTextBounds(measured);
        float textWidth               = std::max(0.0f, textBounds.second);
        float minimumWidth            = PreparedLayoutAccess::MinimumTextWidth(measured);
        const auto includeObjectWidth = [&](MarkdownRange object, float objectWidth)
        {
            if (object.first < content.display.first || object.last > content.display.last || object.first >= object.last)
                return;
            const auto local    = object.first - content.display.first;
            const auto leading  = measured.Caret(local);
            const auto trailing = measured.Caret(local + (object.last - object.first));
            textWidth += std::max(0.0f, std::min(objectWidth, options.widthDip) - std::abs(trailing.x - leading.x));
            minimumWidth = std::max(minimumWidth, std::min(objectWidth, options.widthDip));
        };
        if (state->images)
            for (const auto& image : state->images->placements)
                if (image.display.first >= content.display.first && image.display.last <= content.display.last)
                    includeObjectWidth(image.display, image.Size().width);
        if (const auto* diagram = state->native ? state->native->Find(content.element) : nullptr)
        {
            const auto extent = diagram->paint->Geometry()->extent;
            includeObjectWidth(state->document->Elements()[diagram->element].display, static_cast<float>(extent.width));
        }
        const auto minimum = minimumWidth + 12.0f;
        const auto width   = std::max(textWidth + 12.0f, minimum);
        if (! std::isfinite(width) || ! std::isfinite(minimum) || width <= 0.0f)
            throw std::overflow_error("Nonfinite intrinsic table column width");
        const auto previous               = sizing->preferred[content.column];
        sizing->preferred[content.column] = std::max(previous, width);
        sizing->preferredTotal += sizing->preferred[content.column] - previous;
        const auto previousMinimum      = sizing->minimum[content.column];
        sizing->minimum[content.column] = std::max(previousMinimum, minimum);
        sizing->minimumTotal += sizing->minimum[content.column] - previousMinimum;
        const bool hasInlineObjects =
            (state->native && state->native->Find(content.element)) || (state->images && std::ranges::any_of(state->images->placements, [&](const auto& image) {
            return image.display.first >= content.display.first && image.display.last <= content.display.last;
        }));
        const bool leadingAligned = state->options.readingDirection == DWRITE_READING_DIRECTION_LEFT_TO_RIGHT &&
                                    state->options.textAlignment == DWRITE_TEXT_ALIGNMENT_LEADING &&
                                    (content.alignment == MarkdownAlignment::None || content.alignment == MarkdownAlignment::Left);
        if (! hasInlineObjects && leadingAligned && textWidth > sizing->probeWidths[content.column])
        {
            auto probe                              = std::make_unique<SemanticBox>();
            probe->paragraph                        = content;
            probe->width                            = textWidth;
            probe->layout                           = std::move(measured);
            sizing->probeWidths[content.column]     = textWidth;
            sizing->probeParagraphs[content.column] = paragraphIndex;
            sizing->probes[content.column]          = std::move(probe);
        }
    }
    void AllocateTableOne()
    {
        if (! tableSizing || tableAllocateIndex == tableSizing->size())
        {
            phase = Phase::Layouts;
            return;
        }
        auto& sizing = *(*tableSizing)[tableAllocateIndex];
        if (sizing.widthIndex == 0u)
        {
            const auto baseIndent = static_cast<float>(std::min(sizing.firstContent.indent, 64u)) *
                                    (state->options.semanticSpacing ? state->options.semanticSpacing->containerIndentDip : 16.0f);
            sizing.indent         = std::min(state->options.widthDip - 1.0f, baseIndent + Gutter(sizing.firstContent));
            const auto available  = std::max(1.0f, state->options.widthDip - sizing.indent);
            if (! std::isfinite(sizing.preferredTotal) || sizing.preferredTotal <= 0.0f || ! std::isfinite(sizing.minimumTotal) || sizing.minimumTotal <= 0.0f)
                throw std::overflow_error("Nonfinite intrinsic semantic table width");
            // First preserve each column's widest nonbreaking word/object.
            // Remaining width is shared by its capacity to wrap. Only when
            // even the summed minima do not fit do we permit emergency breaks.
            sizing.compressMinimum = sizing.minimumTotal > available && ! state->options.tableOverflow;
            const auto flexible    = sizing.preferredTotal - sizing.minimumTotal;
            sizing.scale           = sizing.compressMinimum ? available / sizing.minimumTotal
                                     : flexible > 0.0f      ? std::clamp((available - sizing.minimumTotal) / flexible, 0.0f, 1.0f)
                                                            : 0.0f;
            sizing.widths.reserve(sizing.columns);
        }
        const auto column  = sizing.widthIndex++;
        const auto minimum = sizing.minimum[column];
        const auto width   = sizing.compressMinimum ? minimum * sizing.scale : minimum + (sizing.preferred[column] - minimum) * sizing.scale;
        sizing.widths.push_back(std::max(0.01f, width));
        if (sizing.widthIndex == sizing.columns)
        {
            ++tableAllocateIndex;
            if (tableAllocateIndex < tableSizing->size())
                (*tableSizing)[tableAllocateIndex]->widthIndex = 0u;
        }
    }
    void LayoutOne()
    {
        lastLayoutCalls       = 0u;
        auto& next            = *state;
        const auto paragraphs = next.document->Paragraphs();
        const auto emptyLines = next.document->Syntax()->EmptyLines();
        if (emptyLine < emptyLines.size() && LiteralDefinitionLine(emptyLine))
        {
            ++emptyLine;
            return;
        }
        const bool atEnd = paragraph == paragraphs.size() && emptyLine == emptyLines.size();
        if (atEnd)
        {
            if (rowActive)
            {
                FinishRow();
                return;
            }
            if (! next.boxes.empty())
            {
                next.height = std::max(next.options.fontSizeDip, y);
                phase       = Phase::CodeNumbers;
                return;
            }
        }
        SemanticParagraph content;
        std::optional<MarkdownEmptyLine> caretLine;
        bool caretOnly = false, consumeParagraph = false, consumeEmptyLine = false;
        if (atEnd)
        {
            // A definitions-only document still has a real source insertion
            // site even though it contributes neither a paragraph nor glyphs.
            caretLine = MarkdownEmptyLine{{0u, next.document->Map().Source().size()}, next.document->Map().Source().size(), 0u, 0u};
            caretOnly = true;
        }
        if (paragraph < paragraphs.size())
        {
            content          = paragraphs[paragraph];
            consumeParagraph = true;
        }
        if (emptyLine < emptyLines.size() && (paragraph == paragraphs.size() || EmptyBefore(emptyLine, content)))
        {
            caretLine        = EmptyLine(emptyLine);
            consumeEmptyLine = true;
            // An empty list item's generated marker already owns this visual
            // line. Attach its insertion site instead of making a second row.
            consumeParagraph = paragraph < paragraphs.size() && EmptyOwnsParagraph(emptyLine, content);
            if (! consumeParagraph)
            {
                content            = {};
                content.syntaxNode = caretLine->owner;
                content.element    = next.nodeElements[caretLine->owner];
                content.indent     = caretLine->indent + (! nodeSections.empty() && CaretSection(*caretLine) ? 1u : 0u);
                const auto display = caretSites.empty() ? next.document->Map().ToDisplay(caretLine->insertion) : caretSites[emptyLine].display;
                content.display    = {display, display};
                caretOnly          = true;
            }
        }
        const bool table = content.table != MarkdownNoNode;
        if (rowActive && (! table || content.table != rowTable || content.row != rowIndex))
        {
            FinishRow();
            return;
        }
        if (next.boxes.size() == SemanticDocument::MaximumParagraphs)
            throw std::length_error("Semantic native paragraph/caret-site capacity exceeded");
        auto marker      = consumeParagraph ? std::move(markers[paragraph]) : nullptr;
        const bool split = marker && SplitMarker(content);
        if (split)
            content.display.first = content.markerEnd;
        auto box         = marker && ! split ? std::move(marker) : std::make_unique<SemanticBox>();
        box->paragraph   = content;
        box->caretOnly   = caretOnly;
        box->sourceCaret = caretLine;
        const auto& node = next.document->Syntax()->Nodes()[content.syntaxNode];
        if (content.emptyInsertion != MarkdownNoNode)
        {
            const auto& owner = next.document->Elements()[content.element];
            if (! box->sourceCaret || owner.role != SemanticRole::Footnote)
                box->sourceCaret = MarkdownEmptyLine{owner.source, content.emptyInsertion, content.syntaxNode, content.indent};
        }
        if (! box->sourceCaret && node.block.content.first == node.block.content.last)
        {
            box->sourceCaret = MarkdownEmptyLine{table ? node.block.content : node.block.source, node.block.content.first, content.syntaxNode, content.indent};
            if (table)
            {
                const auto row = next.document->Syntax()->Nodes()[node.parent].block.content;
                auto insertion = node.block.content.first;
                // M3's padded token is anchored at the full source row end,
                // which can include CRLF. Its visual insertion site is before
                // that EOL, never on the following physical source line.
                if (insertion == row.last)
                    while (insertion > row.first &&
                           (next.document->Map().Source()[insertion - 1u] == L'\r' || next.document->Map().Source()[insertion - 1u] == L'\n'))
                        --insertion;
                box->sourceCaret->source    = {insertion, insertion};
                box->sourceCaret->insertion = insertion;
            }
        }
        const bool rtl = next.options.readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT;
        if (HasSpacingPolicy() && ! rowActive)
        {
            // Empty source sites fill the requested margin. A second source
            // blank adds another real line; no extra synthetic ACP is needed.
            y = AddHeight(flowBottom, caretOnly ? emptyHeight : std::max(IncomingGap(content), emptyHeight));
        }
        const float baseIndent =
            static_cast<float>(std::min(content.indent, 64u)) * (next.options.semanticSpacing ? next.options.semanticSpacing->containerIndentDip : 16.0f);
        const float indent    = std::min(next.options.widthDip - 1.0f, baseIndent + Gutter(content));
        box->x                = rtl ? 0.0f : indent;
        box->y                = y;
        box->width            = next.options.widthDip - indent;
        LayoutOptions options = next.options;
        if (table)
        {
            if (! content.columns || content.column >= content.columns)
                throw std::logic_error("Invalid semantic table placement");
            if (! rowActive)
            {
                rowActive = true;
                rowStart  = next.boxes.size();
                rowTable  = content.table;
                rowIndex  = content.row;
                rowHeight = 0.0f;
                rowLeft   = std::numeric_limits<float>::max();
                rowRight  = 0.0f;
            }
            const auto column = rtl ? content.columns - content.column - 1u : content.column;
            float cellWidth   = box->width / static_cast<float>(content.columns);
            if (next.options.markdownPresentation == MarkdownPresentationStyle::Subtle)
            {
                const auto* sizing = FindTableSizing(content.table);
                if (! sizing || sizing->widths.size() != content.columns)
                    throw std::logic_error("Missing intrinsic semantic table widths");
                cellWidth = sizing->widths[content.column];
                float preceding{};
                for (std::size_t index = 0u; index < column; ++index)
                    preceding += sizing->widths[rtl ? content.columns - index - 1u : index];
                if (rtl)
                {
                    float tableWidth{};
                    for (const float width : sizing->widths)
                        tableWidth += width;
                    box->x += (next.options.tableOverflow ? std::max(0.0f, box->width - tableWidth) : box->width - tableWidth) + preceding;
                }
                else
                    box->x += preceding;
            }
            else
                box->x += cellWidth * static_cast<float>(column);
            box->x += 6.0f;
            box->y += 6.0f;
            box->width = std::max(1.0f, cellWidth - 12.0f);
            if (content.alignment == MarkdownAlignment::Center)
                options.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
            else if (content.alignment == MarkdownAlignment::Left)
                options.textAlignment = rtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING;
            else if (content.alignment == MarkdownAlignment::Right)
                options.textAlignment = rtl ? DWRITE_TEXT_ALIGNMENT_LEADING : DWRITE_TEXT_ALIGNMENT_TRAILING;
        }
        else if (next.options.markdownPresentation == MarkdownPresentationStyle::Subtle)
        {
            const auto& element = next.document->Elements()[content.element];
            if (element.role == SemanticRole::Diagram || std::ranges::any_of(SemanticExtensionAccess::Maths(*next.document), [&](const auto& formula) {
                return formula.display && next.document->Elements()[formula.element].display == content.display;
            }))
                options.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        }
        const bool paddedCode = ! table && ! caretOnly && next.options.markdownPresentation == MarkdownPresentationStyle::Subtle &&
                                ! next.document->Syntax()->SourceRegion() &&
                                (node.block.kind == MarkdownBlockKind::FencedCode || node.block.kind == MarkdownBlockKind::IndentedCode) &&
                                next.document->Elements()[content.element].role == SemanticRole::Code;
        const auto* label     = paddedCode && next.codeLabels ? (*next.codeLabels)[content.element].get() : nullptr;
        const float codeHeader =
            paddedCode ? std::max(label ? label->Height() + 3.0f : 0.0f, next.options.codeCopyAction && box->width >= 96.0f ? 18.0f : 0.0f) : 0.0f;
        const float codeInset = paddedCode ? std::min(6.0f, std::max(0.0f, (box->width - 1.0f) * 0.5f)) : 0.0f;
        if (paddedCode)
        {
            auto remainingLines = node.lines.size() + 1u;
            unsigned digits     = 1u;
            while (remainingLines >= 10u)
            {
                remainingLines /= 10u;
                ++digits;
            }
            const float codeSize     = options.codeFontSizeDip > 0.0f ? options.codeFontSizeDip : options.fontSizeDip;
            const float wantedGutter = std::max(40.0f, static_cast<float>(digits) * codeSize * 0.75f + 8.0f);
            box->codeGutter          = box->width >= wantedGutter + 56.0f ? wantedGutter : 0.0f;
            box->x += codeInset + (rtl ? 0.0f : box->codeGutter);
            box->y += 6.0f + codeHeader;
            box->width -= 2.0f * codeInset + box->codeGutter;
        }
        options.widthDip = std::max(1.0f, box->width);
        auto* native     = next.native ? next.native->Find(content.element) : nullptr;
        if (native && (native->display < content.display.first || native->display >= content.display.last))
            native = nullptr;
        if (table && next.options.markdownPresentation == MarkdownPresentationStyle::Subtle)
        {
            auto* sizing = FindTableSizing(content.table);
            if (! sizing || content.column >= sizing->probes.size())
                throw std::logic_error("Missing intrinsic semantic table probe");
            auto& measured              = sizing->probes[content.column];
            const bool hasInlineObjects = native || (next.images && std::ranges::any_of(next.images->placements, [&](const auto& image) {
                return image.display.first >= content.display.first && image.display.last <= content.display.last;
            }));
            const bool ownsProbe        = sizing->probeParagraphs[content.column] == paragraph;
            if (ownsProbe && measured && measured->paragraph.display == content.display && ! hasInlineObjects && ! rtl &&
                options.textAlignment == DWRITE_TEXT_ALIGNMENT_LEADING && measured->width <= options.widthDip)
            {
                box->layout = std::move(measured->layout);
                box->height = box->layout.Height();
            }
            if (ownsProbe)
                measured.reset();
        }
        if (! box->layout.Ready())
        {
            PreparedLayoutAccess::SemanticInput projection;
            SemanticDisplayInput(*next.document, content.display, projection);
            wil::com_ptr<IDWriteInlineObject> placeholder;
            if (native)
            {
                const auto extent = native->paint->Geometry()->extent;
                if (extent.width > options.widthDip)
                {
                    next.Reject(native->element);
                    native = nullptr;
                }
                if (native)
                {
                    const auto height = static_cast<float>(extent.height);
                    const auto object = Microsoft::WRL::Make<SemanticObjectPlaceholder>(
                        DWRITE_INLINE_OBJECT_METRICS{static_cast<float>(extent.width), height, std::min(options.fontSizeDip, height), FALSE});
                    if (! object)
                        throw std::bad_alloc();
                    placeholder = object.Get();
                }
            }
            const auto objectOffset = native ? static_cast<UINT32>(native->display - content.display.first) : 0u;
            struct ImageInlines
            {
                std::array<PreparedLayoutAccess::InlineObject, RichImageResources::MaximumPlacements> ranges{};
                std::array<wil::com_ptr<IDWriteInlineObject>, RichImageResources::MaximumPlacements> objects{};
                std::size_t count{};
            };
            std::optional<ImageInlines> imageInlines;
            if (next.images)
            {
                imageInlines.emplace();
                for (auto& image : next.images->placements)
                    if (image.display.first >= content.display.first && image.display.last <= content.display.last && image.display.first < image.display.last)
                    {
                        const auto size  = image.Size();
                        const auto scale = std::min(1.0f, options.widthDip / size.width);
                        const auto width = size.width * scale, height = size.height * scale;
                        const auto object =
                            Microsoft::WRL::Make<SemanticObjectPlaceholder>(DWRITE_INLINE_OBJECT_METRICS{width, height, image.Baseline() * scale, FALSE});
                        if (! object)
                            throw std::bad_alloc();
                        const auto index             = imageInlines->count++;
                        imageInlines->objects[index] = object.Get();
                        imageInlines->ranges[index]  = {object.Get(),
                                                        static_cast<UINT32>(image.display.first - content.display.first),
                                                        static_cast<UINT32>(image.display.last - image.display.first)};
                    }
                std::sort(imageInlines->ranges.begin(),
                          imageInlines->ranges.begin() + static_cast<std::ptrdiff_t>(imageInlines->count),
                          [](const auto& a, const auto& b) { return a.first < b.first; });
            }
            const auto imageRanges =
                imageInlines ? std::span(imageInlines->ranges.data(), imageInlines->count) : std::span<PreparedLayoutAccess::InlineObject>{};
            if (previousState && node.identity != 0u && ! caretOnly && ! marker)
            {
                const auto range = reusable.equal_range(node.identity);
                for (auto found = range.first; found != range.second; ++found)
                {
                    const auto& old       = *previousState->boxes[found->second];
                    const auto& priorNode = previousState->document->Syntax()->Nodes()[old.paragraph.syntaxNode];
                    const auto& prior     = old.layout.Content();
                    const auto& oldNodes  = previousState->document->Syntax()->Nodes();
                    const auto& newNodes  = next.document->Syntax()->Nodes();
                    const auto sameOwner  = [&](std::size_t left, std::size_t right)
                    {
                        return left == MarkdownNoNode ? right == MarkdownNoNode
                                                      : right != MarkdownNoNode && left < oldNodes.size() && right < newNodes.size() &&
                                                            oldNodes[left].identity != 0u && oldNodes[left].identity == newNodes[right].identity;
                    };
                    bool sameTable = old.paragraph.table == MarkdownNoNode && content.table == MarkdownNoNode;
                    if (old.paragraph.table != MarkdownNoNode && content.table != MarkdownNoNode &&
                        old.paragraph.table < previousState->document->Elements().size() && content.table < next.document->Elements().size())
                    {
                        const auto& left  = previousState->document->Elements()[old.paragraph.table];
                        const auto& right = next.document->Elements()[content.table];
                        sameTable         = left.role == SemanticRole::Table && right.role == SemanticRole::Table && left.blockIdentity != 0u &&
                                            left.blockIdentity == right.blockIdentity && left.rows == right.rows && left.columns == right.columns &&
                                            previousState->document->Syntax()->Slice(left.source) == next.document->Syntax()->Slice(right.source);
                    }
                    bool sameNative = (! old.native && ! native);
                    if (old.native && native)
                    {
                        const auto oldExtent = old.native->paint->Geometry()->extent;
                        const auto newExtent = native->paint->Geometry()->extent;
                        sameNative           = oldExtent.width == newExtent.width && oldExtent.height == newExtent.height &&
                                               old.native->display - old.paragraph.display.first == native->display - content.display.first;
                    }
                    bool sameImages = ! old.images && imageRanges.empty();
                    if (old.images && ! imageRanges.empty() && next.images)
                    {
                        std::size_t matched = 0u, priorCount = 0u;
                        for (const auto& item : old.images->placements)
                            if (item.owner == &old)
                                ++priorCount;
                        for (const auto& item : next.images->placements)
                        {
                            if (item.display.first < content.display.first || item.display.last > content.display.last ||
                                item.display.first == item.display.last)
                                continue;
                            const auto first = item.display.first - content.display.first, last = item.display.last - content.display.first;
                            const auto foundImage = std::ranges::find_if(old.images->placements,
                                                                         [&](const auto& candidate)
                            {
                                if (candidate.owner != &old || candidate.display.first - old.paragraph.display.first != first ||
                                    candidate.display.last - old.paragraph.display.first != last || bool(candidate.math) != bool(item.math))
                                    return false;
                                const auto a = candidate.Size(), b = item.Size();
                                return a.width == b.width && a.height == b.height && candidate.Baseline() == item.Baseline();
                            });
                            if (foundImage != old.images->placements.end())
                                ++matched;
                        }
                        sameImages = matched == imageRanges.size() && matched == priorCount;
                    }
                    const bool sameRuns = prior.runs.size() == projection.runs.size() &&
                                          std::equal(prior.runs.begin(), prior.runs.end(), projection.runs.begin(), [](const auto& left, const auto& right) {
                        return left.start == right.start && left.length == right.length && left.flags == right.flags && left.heading == right.heading;
                    });
                    if (! old.markerLayout && sameNative && sameImages && sameTable && sameOwner(old.paragraph.markerOwner, content.markerOwner) &&
                        old.paragraph.row == content.row && old.paragraph.column == content.column && old.paragraph.columns == content.columns &&
                        old.width == box->width && old.paragraph.indent == content.indent && old.paragraph.flags == content.flags &&
                        old.paragraph.heading == content.heading && old.paragraph.alignment == content.alignment &&
                        previousState->document->Syntax()->Slice(priorNode.block.source) == next.document->Syntax()->Slice(node.block.source) &&
                        prior.text == projection.text && sameRuns)
                    {
                        box->layout = old.layout;
                        ++next.reusedBoxes;
                        break;
                    }
                }
            }
            const bool emptySite = caretOnly && ! table && ! native && imageRanges.empty() && projection.text.empty() && projection.runs.empty();
            if (! box->layout.Ready() && emptySite)
            {
                const auto found = std::ranges::find_if(
                    emptyLayouts, [&](const auto& entry) { return entry->width == options.widthDip && entry->alignment == options.textAlignment; });
                if (found != emptyLayouts.end())
                {
                    box->layout = (*found)->layout;
                }
            }
            if (! box->layout.Ready())
            {
                PreparedLayoutAccess::PrepareSemantic(
                    box->layout, next.factory.get(), projection, options, false, placeholder.get(), objectOffset, imageRanges);
                lastLayoutCalls = 1u;
                if (emptySite)
                {
                    auto cached       = std::make_unique<EmptyLayout>();
                    cached->width     = options.widthDip;
                    cached->alignment = options.textAlignment;
                    cached->layout    = box->layout;
                    emptyLayouts.push_back(std::move(cached));
                }
            }
            if (imageInlines && imageInlines->count)
                for (auto& image : next.images->placements)
                    if (image.display.first >= content.display.first && image.display.last <= content.display.last && image.display.first < image.display.last)
                    {
                        const auto rectangles = box->layout.Range(image.display.first - content.display.first, image.display.last - content.display.first);
                        if (rectangles.size() != 1u)
                            throw std::logic_error("Prepared image must produce one atomic SDK rectangle");
                        image.bounds     = rectangles.front();
                        const auto local = image.display.first - content.display.first;
                        std::size_t lineStart{};
                        float lineTop{};
                        bool located{};
                        for (const auto& line : PreparedLayoutAccess::LineMetrics(box->layout))
                        {
                            if (local < lineStart + line.length)
                            {
                                // HitTestTextRange reports the complete line
                                // cell, including neighboring font descent.
                                // Paint the image at its actual inline baseline
                                // and pixel aspect ratio, never stretched to
                                // that larger SDK selection rectangle.
                                if (image.math)
                                {
                                    const auto lineEnd  = lineStart + line.length;
                                    const auto inObject = [&](std::size_t position)
                                    {
                                        const auto global = content.display.first + position;
                                        return std::ranges::any_of(next.images->placements, [&](const auto& item) {
                                            return item.display.first <= global && global < item.display.last;
                                        });
                                    };
                                    const auto usable = [&](std::size_t position)
                                    {
                                        return position >= lineStart && position < lineEnd && position < box->layout.Content().text.size() &&
                                               box->layout.Content().text[position] != L'\n' && ! inObject(position);
                                    };
                                    const auto after    = image.display.last - content.display.first;
                                    const auto before   = local > lineStart ? box->layout.PreviousCluster(local) : lineEnd;
                                    const auto neighbor = usable(before) ? before : usable(after) ? after : lineEnd;
                                    const auto textCaret =
                                        neighbor < lineEnd ? box->layout.Caret(neighbor) : StandaloneMathCaret(*box, local, options, lineTop, line.baseline);
                                    image.mathCaretTop = textCaret.y;
                                    image.caretHeight  = textCaret.height;
                                    image.hasMathCaret = true;
                                }
                                const auto size     = image.Size();
                                const auto scale    = std::min(1.0f, options.widthDip / size.width);
                                image.bounds.top    = lineTop + line.baseline - image.Baseline() * scale;
                                image.bounds.bottom = image.bounds.top + size.height * scale;
                                image.bounds.right  = image.bounds.left + size.width * scale;
                                located             = true;
                                break;
                            }
                            lineStart += line.length;
                            lineTop += line.height;
                        }
                        if (! located)
                            throw std::logic_error("Prepared image has no SDK line baseline");
                        image.owner = box.get();
                        if (! image.hasMathCaret)
                            image.caretHeight = options.fontSizeDip;
                        box->images = next.images.get();
                    }
            if (native)
            {
                const auto textBounds = PreparedLayoutAccess::HorizontalTextBounds(box->layout);
                if (textBounds.second > options.widthDip + 0.01f)
                    next.Reject(native->element);
                const auto rectangles = box->layout.Range(objectOffset, objectOffset + 1u);
                if (rectangles.size() != 1u)
                    throw std::logic_error("Native diagram did not produce one atomic SDK object");
                native->bounds      = rectangles.front();
                native->caretHeight = options.fontSizeDip;
                native->rtl         = rtl;
                box->native         = native;
            }
        }
        if (next.codeChipsAvailable && next.document->Elements()[content.element].role != SemanticRole::Code && ! caretOnly)
            for (const auto& run : box->layout.Content().runs)
            {
                const bool marked = (run.flags & Marked) != 0u;
                next.markedText   = next.markedText || marked;
                if ((! marked && ((run.flags & Code) == 0u || next.options.markdownPresentation != MarkdownPresentationStyle::Subtle)) || run.length == 0u)
                    continue;
                const auto chips = box->layout.Range(run.start, run.start + run.length);
                // Cosmetic chip storage is independent of source/geometry
                // admission. Retain at most this many rectangles and omit all
                // chips consistently if a pathological document exceeds it.
                constexpr std::size_t maximumCodeChips = 16384u;
                if (chips.size() > maximumCodeChips - next.codeChipCount)
                {
                    next.codeChipsAvailable = false;
                    break;
                }
                next.codeChipCount += chips.size();
                auto& destination = marked ? box->markChips : box->codeChips;
                destination.insert(destination.end(), chips.begin(), chips.end());
            }
        box->height = box->layout.Height();
        box->frame  = D2D1::RectF(box->x, box->y, box->x + box->width, box->y + box->height);
        if (paddedCode)
            box->frame = D2D1::RectF(box->x - codeInset - (rtl ? 0.0f : box->codeGutter),
                                     box->y - 6.0f - codeHeader,
                                     box->x + box->width + codeInset + (rtl ? box->codeGutter : 0.0f),
                                     box->y + box->height + 6.0f);
        if (native)
            box->frame = D2D1::RectF(box->x + native->bounds.left, box->y + native->bounds.top, box->x + native->bounds.right, box->y + native->bounds.bottom);
        float bottom = box->frame.bottom;
        if (content.markerOwner != MarkdownNoNode && ! split && consumeParagraph)
        {
            const auto [left, width] = PreparedLayoutAccess::HorizontalTextBounds(box->layout);
            const auto actualLeft    = rtl ? next.options.widthDip - indent : indent - width;
            box->x                   = actualLeft - left;
            box->width               = width;
            box->frame               = D2D1::RectF(actualLeft, box->y, actualLeft + width, box->y + box->height);
        }
        if (marker)
        {
            // Two real layouts share one first baseline. The marker column is
            // measured once per list, so 9./10., tasks and continuation leaves
            // agree on the same text edge in either reading direction.
            const auto [left, width] = PreparedLayoutAccess::HorizontalTextBounds(marker->layout);
            const auto markerLines = PreparedLayoutAccess::LineMetrics(marker->layout), bodyLines = PreparedLayoutAccess::LineMetrics(box->layout);
            const float markerBaseline = markerLines.empty() ? 0.0f : markerLines.front().baseline;
            const float bodyBaseline   = bodyLines.empty() ? 0.0f : bodyLines.front().baseline;
            const float baseline       = std::max(markerBaseline, bodyBaseline);
            marker->markerOnly         = true;
            marker->y                  = y + baseline - markerBaseline;
            box->y                     = y + baseline - bodyBaseline;
            box->frame.top             = box->y;
            box->frame.bottom          = box->y + box->height;
            const auto actualLeft      = rtl ? next.options.widthDip - indent : indent - width;
            marker->x                  = actualLeft - left;
            marker->width              = width;
            marker->frame              = D2D1::RectF(actualLeft, marker->y, actualLeft + width, marker->y + marker->height);
            bottom                     = std::max(marker->frame.bottom, box->frame.bottom);
            Union(next.bounds[content.element], marker->frame);
            next.glyphBoxes.push_back(next.boxes.size());
            next.boxes.push_back(std::move(marker));
        }
        if (table)
        {
            rowHeight = std::max(rowHeight, box->height);
            flowItem  = ItemOwner(content);
        }
        else if (HasSpacingPolicy())
        {
            if (caretOnly)
                emptyHeight = AddHeight(emptyHeight, box->height);
            else
            {
                flowBottom  = bottom;
                emptyHeight = 0.0f;
                gapAfter =
                    next.options.paragraphSpacingAfterLines && next.document->Elements()[content.element].role == SemanticRole::Paragraph ? paragraphGap : 8.0f;
                if (next.options.semanticSpacing && content.heading > 0 && content.heading <= 6)
                    gapAfter = next.options.semanticSpacing->headingAfterDip[content.heading - 1];
                if (next.options.markdownPresentation == MarkdownPresentationStyle::Subtle && content.heading > 0u && content.heading <= 2u)
                    gapAfter = std::max(gapAfter, 14.0f);
                flowItem = ItemOwner(content);
            }
            y = AddHeight(flowBottom, std::max(gapAfter, emptyHeight));
        }
        else
            y = AddHeight(
                bottom, next.options.markdownPresentation == MarkdownPresentationStyle::Subtle && content.heading > 0u && content.heading <= 2u ? 14.0f : 8.0f);
        if (next.options.markdownPresentation == MarkdownPresentationStyle::Subtle && content.heading > 0u && content.heading <= 2u && ! caretOnly)
        {
            const auto lineY = bottom + 3.0f;
            next.decorations->headingRules.push_back(D2D1::RectF(box->x, lineY, box->x + box->width, lineY + 1.0f));
            Union(next.bounds[content.element], next.decorations->headingRules.back());
        }
        if (consumeParagraph)
            ++paragraph;
        if (consumeEmptyLine)
            ++emptyLine;
        Union(next.bounds[content.element], box->frame);
        if (! caretOnly)
            next.glyphBoxes.push_back(next.boxes.size());
        next.boxes.push_back(std::move(box));
    }
    void PlaceRowOne()
    {
        auto& next = *state;
        if (rowPlacement < next.boxes.size())
        {
            auto& box = *next.boxes[rowPlacement++];
            float cellWidth =
                (next.options.widthDip - std::min(next.options.widthDip - 1.0f,
                                                  static_cast<float>(std::min(box.paragraph.indent, 64u)) *
                                                          (next.options.semanticSpacing ? next.options.semanticSpacing->containerIndentDip : 16.0f) +
                                                      Gutter(box.paragraph))) /
                static_cast<float>(box.paragraph.columns);
            if (next.options.markdownPresentation == MarkdownPresentationStyle::Subtle)
            {
                const auto* sizing = FindTableSizing(box.paragraph.table);
                if (! sizing || sizing->widths.size() != box.paragraph.columns)
                    throw std::logic_error("Missing intrinsic semantic table row widths");
                cellWidth = sizing->widths[box.paragraph.column];
            }
            const auto left                    = box.x - 6.0f;
            rowLeft                            = std::min(rowLeft, left);
            rowRight                           = std::max(rowRight, left + cellWidth);
            box.frame                          = D2D1::RectF(left, y, left + cellWidth, y + rowHeight + 12.0f);
            next.width                         = std::max(next.width, box.frame.right);
            next.bounds[box.paragraph.element] = box.frame;
            return;
        }
        if (next.options.markdownPresentation == MarkdownPresentationStyle::Subtle && rowLeft < rowRight)
            next.decorations->tableRows.push_back(D2D1::RectF(rowLeft, y, rowRight, y + rowHeight + 12.0f));
        y = AddHeight(y, rowHeight + 12.0f);
        if (HasSpacingPolicy())
        {
            flowBottom  = y; // Row frame already includes its original padding.
            gapAfter    = 0.0f;
            emptyHeight = 0.0f;
        }
        rowActive = false;
        phase     = Phase::Layouts;
    }
};
#pragma warning(pop)
PreparedSemanticGeometry::PreparedSemanticGeometry() noexcept  = default;
PreparedSemanticGeometry::~PreparedSemanticGeometry() noexcept = default;
LayoutPreparationStatus PreparedSemanticGeometry::Begin(IDWriteFactory* factory,
                                                        std::shared_ptr<const SemanticDocument> document,
                                                        const LayoutOptions& options,
                                                        ID2D1Factory* drawing,
                                                        FlowchartResourceEpoch epochs,
                                                        std::shared_ptr<const RichImageResourceSet> images,
                                                        ID2D1RenderTarget* raster)
{
    const auto validSpacing = [](const std::optional<float>& value, float maximum) noexcept
    { return ! value || (std::isfinite(value.value()) && value.value() >= 0.0f && value.value() <= maximum); };
    if (! factory || ! document || ! std::isfinite(options.widthDip) || options.widthDip <= 0.0f || ! std::isfinite(options.fontSizeDip) ||
        ! std::isfinite(options.dpi) || options.dpi <= 0.0f || options.dpi > 4096.0f || options.fontSizeDip <= 0.0f ||
        ! validSpacing(options.paragraphSpacingAfterLines, 4.0f) || ! validSpacing(options.listItemSpacingDip, 256.0f) ||
        (options.semanticSpacing && ! options.semanticSpacing->IsValid()) ||
        (options.markdownPresentation != MarkdownPresentationStyle::Default && options.markdownPresentation != MarkdownPresentationStyle::Subtle))
        throw std::invalid_argument("Invalid semantic geometry request");
    const auto diagrams    = SemanticExtensionAccess::Diagrams(*document);
    const auto nativeCount = diagrams.size() + SemanticExtensionAccess::Maths(*document).size();
    if (nativeCount > RichImageResources::MaximumPlacements || (images && images->placements.size() > RichImageResources::MaximumPlacements - nativeCount))
        throw NativeObjectCapacity("Combined native image/diagram capacity exceeded");
    // Hosting supplies its raster target, not knowledge of extension storage.
    // CPU-only callers may still supply a factory without a raster target.
    wil::com_ptr<ID2D1Factory> targetFactory;
    if (! diagrams.empty() && ! drawing && raster)
    {
        raster->GetFactory(targetFactory.put());
        drawing = targetFactory.get();
    }
    if (! diagrams.empty() && ! drawing)
        throw std::invalid_argument("Native semantic objects require a drawing factory");
    const auto same = [&](const State& state)
    {
        return state.factory.get() == factory && state.document == document && SameOptions(state.options, options) && state.imageResources == images &&
               (! state.images || (state.images->targetIdentity == raster && state.imageEpochs == epochs)) &&
               (! state.native || (state.native->drawing.get() == drawing && state.native->epochs == epochs));
    };
    if (next_ && same(*next_->state))
        return LayoutPreparationStatus::Pending;
    if (state_ && same(*state_))
    {
        next_.reset();
        valid_ = true;
        return LayoutPreparationStatus::Complete;
    }
    // Reuse only fully published math for a width-only reflow. Prepared math owns
    // DirectWrite layouts, not raster-target resources; placement geometry is rebuilt.
    const NativeImageGeometry* previousImages = nullptr;
    if (state_ && state_->images && state_->factory.get() == factory && state_->document == document && state_->imageResources == images &&
        state_->images->targetIdentity == raster && state_->imageEpochs == epochs)
    {
        LayoutOptions priorOptions = state_->options;
        priorOptions.widthDip      = options.widthDip;
        if (SameOptions(priorOptions, options))
            previousImages = state_->images.get();
    }
    const auto* previous =
        state_ && state_->factory.get() == factory && state_->imageEpochs == epochs && SameOptions(state_->options, options) ? state_.get() : nullptr;
    const auto* previousNative = state_ && state_->document == document && state_->factory.get() == factory && state_->native &&
                                         state_->native->drawing.get() == drawing && state_->native->epochs == epochs &&
                                         state_->options.fontFamily == options.fontFamily && state_->options.fontSizeDip == options.fontSizeDip &&
                                         state_->options.locale == options.locale && state_->options.readingDirection == options.readingDirection
                                     ? state_.get()
                                     : nullptr;
    auto next                  = std::make_unique<Preparation>(
        std::move(document), factory, options, drawing, epochs, std::move(images), raster, previousImages, previous, previousNative);
    next_.swap(next);
    return LayoutPreparationStatus::Pending;
}
LayoutAdvance PreparedSemanticGeometry::Advance(LayoutWorkBudget budget)
{
    if (! budget.maximumLayouts || ! budget.maximumBlockSteps || budget.maximumUploadBytes < 4u)
        throw std::invalid_argument("Semantic layout budgets must be positive and admit one BGRA pixel");
    if (! next_)
        return {};
    LayoutAdvance result{LayoutPreparationStatus::Pending};
    try
    {
        while (result.blockSteps < budget.maximumBlockSteps)
        {
            auto& work = *next_;
            if (work.phase == Preparation::Phase::IndexReuse)
            {
                if (! work.previousState || work.reuseIndex == work.previousState->boxes.size())
                {
                    work.phase = Preparation::Phase::IndexElements;
                    continue;
                }
                const auto index  = work.reuseIndex++;
                const auto& box   = *work.previousState->boxes[index];
                const auto& nodes = work.previousState->document->Syntax()->Nodes();
                if (! box.caretOnly && (! box.markerOnly || box.markerLayout) && box.paragraph.syntaxNode < nodes.size() &&
                    nodes[box.paragraph.syntaxNode].identity != 0u && box.layout.Ready())
                    work.reusable.emplace(nodes[box.paragraph.syntaxNode].identity, index);
            }
            else if (work.phase == Preparation::Phase::IndexElements)
            {
                const auto elements = work.state->document->Elements();
                if (work.boundsIndex == elements.size())
                {
                    work.boundsIndex = 0u;
                    std::ranges::sort(work.footerRanges, {}, [](const auto& row) { return row.source.first; });
                    work.phase = work.literalDefinitionNodes.empty() && work.footerRanges.empty() && ! work.spacingItems
                                     ? Preparation::Phase::CountSites
                                     : Preparation::Phase::IndexLiteralDefinitions;
                    continue;
                }
                const auto index = work.boundsIndex++;
                if (elements[index].syntaxNode != MarkdownNoNode)
                {
                    work.state->nodeElements[elements[index].syntaxNode] = index;
                    if (elements[index].role == SemanticRole::Footnote)
                    {
                        if (work.nodeSections.empty())
                            work.nodeSections.assign(work.state->nodeElements.size(), 0u);
                        work.footerRanges.push_back({elements[index].source, elements[index].display, elements[index].syntaxNode + 1u});
                    }
                    if (elements[index].role == SemanticRole::Code &&
                        work.state->document->Syntax()->Nodes()[elements[index].syntaxNode].block.kind == MarkdownBlockKind::FootnoteDefinition &&
                        work.literalDefinitionNodes.empty())
                        work.literalDefinitionNodes.assign(work.state->nodeElements.size(), false);
                }
            }
            else if (work.phase == Preparation::Phase::IndexLiteralDefinitions)
            {
                const auto& nodes = work.state->document->Syntax()->Nodes();
                if (work.boundsIndex == nodes.size())
                {
                    work.boundsIndex = 0u;
                    work.phase       = work.footerRanges.empty() ? Preparation::Phase::CountSites : Preparation::Phase::IndexCaretAnchors;
                    continue;
                }
                const auto index  = work.boundsIndex++;
                const auto parent = nodes[index].parent;
                if (parent != MarkdownNoNode && parent >= index)
                    throw std::logic_error("Invalid block ancestor order");
                const auto element = work.state->nodeElements[index];
                if (work.spacingItems)
                    (*work.spacingItems)[index] = nodes[index].block.kind == MarkdownBlockKind::ListItem ? index
                                                  : parent == MarkdownNoNode                             ? MarkdownNoNode
                                                                                                         : (*work.spacingItems)[parent];
                if (! work.literalDefinitionNodes.empty())
                    work.literalDefinitionNodes[index] = (parent != MarkdownNoNode && work.literalDefinitionNodes[parent]) ||
                                                         (nodes[index].block.kind == MarkdownBlockKind::FootnoteDefinition && element != MarkdownNoNode &&
                                                          work.state->document->Elements()[element].role == SemanticRole::Code);
                if (! work.nodeSections.empty())
                    work.nodeSections[index] = element != MarkdownNoNode && work.state->document->Elements()[element].role == SemanticRole::Footnote
                                                   ? index + 1u
                                               : parent != MarkdownNoNode ? work.nodeSections[parent]
                                                                          : 0u;
            }
            else if (work.phase == Preparation::Phase::IndexCaretAnchors)
            {
                const auto spans = work.state->document->Map().Spans();
                if (work.boundsIndex == spans.size())
                {
                    std::ranges::sort(work.caretAnchors, {}, [](const auto& anchor) { return std::pair{anchor.section, anchor.source.first}; });
                    work.boundsIndex = 0u;
                    work.phase       = Preparation::Phase::IndexCaretSites;
                    continue;
                }
                const auto& span = spans[work.boundsIndex++];
                if (span.primary && span.source.first != span.source.last)
                    work.caretAnchors.push_back({span.source, span.display, work.SectionAt(span.source.first)});
            }
            else if (work.phase == Preparation::Phase::IndexCaretSites)
            {
                const auto lines = work.state->document->Syntax()->EmptyLines();
                if (work.boundsIndex == lines.size())
                {
                    std::ranges::sort(
                        work.caretSites, {}, [&](const auto& site) { return std::tuple{site.display, lines[site.index].source.first, site.index}; });
                    work.boundsIndex = 0u;
                    work.phase       = Preparation::Phase::CountSites;
                    continue;
                }
                const auto index = work.boundsIndex++;
                work.caretSites.push_back({index, work.CaretDisplay(lines[index])});
            }
            else if (work.phase == Preparation::Phase::CountSites)
            {
                // Preflight the same display-order merge, one metadata event at
                // a time. Overflow never spends thousands of SDK layout calls
                // before discovering that source fallback was required.
                const auto paragraphs = work.state->document->Paragraphs();
                const auto lines      = work.state->document->Syntax()->EmptyLines();
                if (work.emptyLine < lines.size() && work.LiteralDefinitionLine(work.emptyLine))
                {
                    ++work.emptyLine;
                    ++result.blockSteps;
                    continue;
                }
                if (work.paragraph == paragraphs.size() && work.emptyLine == lines.size())
                {
                    work.paragraph = work.emptyLine = 0u;
                    work.phase = work.state->options.paragraphSpacingAfterLines.value_or(0.0f) > 0.0f && work.state->options.lineHeightMultiplier == 0.0f
                                     ? Preparation::Phase::BodyLine
                                     : Preparation::Phase::NativeObjects;
                    continue;
                }
                std::size_t additional = 1u;
                if (work.emptyLine < lines.size() && (work.paragraph == paragraphs.size() || work.EmptyBefore(work.emptyLine, paragraphs[work.paragraph])))
                {
                    if (work.paragraph < paragraphs.size() && work.EmptyOwnsParagraph(work.emptyLine, paragraphs[work.paragraph]))
                    {
                        if (Preparation::SplitMarker(paragraphs[work.paragraph]))
                            ++additional;
                        ++work.paragraph;
                    }
                    ++work.emptyLine;
                }
                else
                {
                    if (Preparation::SplitMarker(paragraphs[work.paragraph]))
                        ++additional;
                    ++work.paragraph;
                }
                work.plannedBoxes += additional;
                if (work.plannedBoxes > SemanticDocument::MaximumParagraphs)
                    throw std::length_error("Semantic native paragraph/caret-site admission exceeded");
            }
            else if (work.phase == Preparation::Phase::BodyLine)
            {
                if (result.layoutsPrepared == budget.maximumLayouts)
                    break;
                work.MeasureBodyLine();
                ++result.layoutsPrepared;
                work.phase = Preparation::Phase::NativeObjects;
            }
            else if (work.phase == Preparation::Phase::NativeObjects)
            {
                auto& state         = *work.state;
                const auto diagrams = SemanticExtensionAccess::Diagrams(*state.document);
                if (work.nativeIndex == diagrams.size())
                {
                    if (state.native)
                        std::erase_if(state.native->placements, [](const auto& item) { return ! item.paint; });
                    work.phase = Preparation::Phase::NativeImages;
                    continue;
                }
                if (result.layoutsPrepared == budget.maximumLayouts)
                    break;
                auto& placement = state.native->placements[work.nativeIndex];
                if (state.options.fontSizeDip > 512.0f)
                {
                    state.Reject(placement.element);
                    ++work.nativeIndex;
                    ++result.blockSteps;
                    continue;
                }
                if (! placement.paint && work.previousNativeState && work.previousNativeState->native)
                {
                    auto* retained = work.previousNativeState->native->Find(placement.element);
                    if (retained && retained->paint && retained->paint->Ready())
                    {
                        placement.paint = retained->paint;
                        ++work.nativeIndex;
                        ++result.blockSteps;
                        continue;
                    }
                }
                if (! placement.paint)
                {
                    placement.paint = std::make_shared<PreparedFlowchart>();
                    placement.paint->Begin(
                        state.factory.get(), state.native->drawing.get(), diagrams[work.nativeIndex].graph, state.options, state.native->epochs);
                    ++result.blockSteps;
                    continue;
                }
                const auto advance = placement.paint->Advance({budget.maximumLayouts - result.layoutsPrepared, budget.maximumBlockSteps - result.blockSteps});
                result.layoutsPrepared += advance.layoutsPrepared;
                result.blockSteps += advance.blockSteps;
                if (advance.status == LayoutPreparationStatus::Complete)
                {
                    if (! placement.paint->Ready())
                    {
                        state.Reject(placement.element);
                        placement.paint.reset();
                    }
                    ++work.nativeIndex;
                }
                if (! advance.blockSteps && ! advance.layoutsPrepared)
                    break;
                continue;
            }
            else if (work.phase == Preparation::Phase::NativeImages)
            {
                if (! work.state->images || work.imageIndex == work.state->images->placements.size())
                {
                    work.raster.reset();
                    work.phase = Preparation::Phase::CodeLabels;
                    continue;
                }
                if (work.state->images->placements[work.imageIndex].math)
                {
                    if (result.layoutsPrepared == budget.maximumLayouts)
                        break;
                    try
                    {
                        if (work.state->images->PrepareMath(work.imageIndex))
                            ++work.imageIndex;
                        else
                            ++result.layoutsPrepared;
                    }
                    catch (const NativeObjectUnsupported& failure)
                    {
                        if (failure.Error() != DWRITE_E_UNSUPPORTEDOPERATION)
                            throw;
                        ++result.layoutsPrepared;
                        work.state->Reject(failure.Element());
                        work.state->images->placements.erase(work.state->images->placements.begin() + static_cast<std::ptrdiff_t>(work.imageIndex));
                    }
                    catch (const NativeObjectCapacity& failure)
                    {
                        ++result.layoutsPrepared;
                        work.state->Reject(failure.Element());
                        work.state->images->placements.erase(work.state->images->placements.begin() + static_cast<std::ptrdiff_t>(work.imageIndex));
                    }
                    ++result.blockSteps;
                    continue;
                }
                if (budget.maximumUploadBytes - result.uploadedBytes < 4u)
                    break;
                const auto upload = work.state->images->PrepareOne(work.imageIndex, work.raster.get(), budget.maximumUploadBytes - result.uploadedBytes);
                if (upload.unsupported)
                    work.state->images->placements.erase(work.state->images->placements.begin() + static_cast<std::ptrdiff_t>(work.imageIndex));
                else if (upload.complete)
                    ++work.imageIndex;
                result.uploadedBytes += upload.bytes;
                ++result.blockSteps;
                // One bounded data upload per turn. Large bitmaps remain
                // private until every tile is ready; duplicates share pixels.
                if (upload.attempted)
                    break;
                // A reused placement already consumed its one event above.
                // Do not count it again in the common phase-step tail.
                continue;
            }
            else if (work.phase == Preparation::Phase::CodeLabels)
            {
                const auto elements = work.state->document->Elements();
                if (! work.state->codeLabels || work.codeLabelIndex == elements.size())
                {
                    work.phase = Preparation::Phase::MarkerLayouts;
                    continue;
                }
                if (result.layoutsPrepared == budget.maximumLayouts)
                    break;
                const auto index    = work.codeLabelIndex++;
                const auto& element = elements[index];
                const auto& nodes   = work.state->document->Syntax()->Nodes();
                if (element.role == SemanticRole::Code && element.syntaxNode < nodes.size() &&
                    nodes[element.syntaxNode].block.kind == MarkdownBlockKind::FencedCode)
                {
                    auto language = work.state->document->Syntax()->Slice(nodes[element.syntaxNode].block.info);
                    while (! language.empty() && (language.front() == L' ' || language.front() == L'\t'))
                        language.remove_prefix(1u);
                    const auto end = language.find_first_of(L" \t\r\n");
                    if (end != language.npos)
                        language = language.substr(0u, end);
                    if (! language.empty() && language.size() <= 128u)
                    {
                        ::DxUi::RichText::Document labelSource{std::wstring(language)};
                        auto projection              = Project(labelSource.Snapshot(), labelSource.Revision(), ViewMode::Source);
                        auto options                 = work.state->options;
                        options.widthDip             = std::max(1.0f, options.widthDip - 12.0f);
                        options.fontSizeDip          = std::max(1.0f, options.fontSizeDip * 0.75f);
                        options.lineHeightMultiplier = 0.0f;
                        auto label                   = std::make_unique<PreparedLayout>();
                        label->Prepare(work.state->factory.get(), std::move(projection), options);
                        (*work.state->codeLabels)[index] = std::move(label);
                        ++result.layoutsPrepared;
                    }
                }
            }
            else if (work.phase == Preparation::Phase::MarkerLayouts)
            {
                if (result.layoutsPrepared == budget.maximumLayouts)
                    break;
                if (work.MeasureMarkerOne())
                    ++result.layoutsPrepared;
            }
            else if (work.phase == Preparation::Phase::TableAllocate)
                work.AllocateTableOne();
            else if (work.phase == Preparation::Phase::Layouts)
            {
                if (result.layoutsPrepared == budget.maximumLayouts)
                    break;
                work.LayoutOne();
                result.layoutsPrepared += work.lastLayoutCalls;
            }
            else if (work.phase == Preparation::Phase::CodeNumbers)
            {
                const auto& boxes = work.state->boxes;
                if (work.codeNumberBox == boxes.size())
                {
                    work.phase = work.footerRanges.empty() ? Preparation::Phase::SourceLines : Preparation::Phase::IndexSourceBoxes;
                    continue;
                }
                const auto& box  = *boxes[work.codeNumberBox];
                const auto lines = PreparedLayoutAccess::LineMetrics(box.layout);
                if (box.codeGutter == 0.0f || lines.size() > 4096u || work.codeNumberLine == lines.size() || work.state->codeNumbers.size() == 4096u)
                {
                    ++work.codeNumberBox;
                    work.codeNumberLine = work.codeNumberOffset = 0u;
                    work.codeNumberValue                        = 1u;
                    work.codeNumberY                            = 0.0f;
                    work.codeNumberNeeded                       = true;
                    continue;
                }
                const auto line  = lines[work.codeNumberLine];
                const auto& text = box.layout.Content().text;
                if (work.codeNumberNeeded && line.length > 0u)
                {
                    if (result.layoutsPrepared == budget.maximumLayouts)
                        break;
                    ::DxUi::RichText::Document labelSource{std::to_wstring(work.codeNumberValue)};
                    auto projection       = Project(labelSource.Snapshot(), labelSource.Revision(), ViewMode::Source);
                    auto options          = work.state->options;
                    options.widthDip      = box.codeGutter - 8.0f;
                    options.fontFamily    = options.codeFontFamily;
                    options.fontSizeDip   = options.codeFontSizeDip > 0.0f ? options.codeFontSizeDip : options.fontSizeDip;
                    options.textAlignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
                    auto layout           = std::make_unique<PreparedLayout>();
                    layout->Prepare(work.state->factory.get(), std::move(projection), options);
                    const bool rtl = options.readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT;
                    const float x  = rtl ? box.x + box.width + 4.0f : box.x - box.codeGutter + 4.0f;
                    work.state->codeNumbers.push_back({std::move(layout), D2D1::Point2F(x, box.y + work.codeNumberY), box.frame});
                    ++result.layoutsPrepared;
                }
                const auto end        = std::min(text.size(), work.codeNumberOffset + static_cast<std::size_t>(line.length));
                work.codeNumberNeeded = end > work.codeNumberOffset && (text[end - 1u] == L'\r' || text[end - 1u] == L'\n');
                if (work.codeNumberNeeded)
                    ++work.codeNumberValue;
                work.codeNumberOffset = end;
                work.codeNumberY += line.height;
                ++work.codeNumberLine;
            }
            else if (work.phase == Preparation::Phase::RowPlacement)
                work.PlaceRowOne();
            else if (work.phase == Preparation::Phase::IndexSourceBoxes)
            {
                const auto& boxes = work.state->boxes;
                if (work.lineBox == boxes.size())
                {
                    // UIA Line units remain an ordered raw-source partition,
                    // independently of footer display order. Each native line
                    // is still measured once, never duplicated for backlinks.
                    const auto first = [&](std::size_t index)
                    {
                        const auto& box = *boxes[index];
                        if (box.sourceCaret && box.paragraph.table == MarkdownNoNode)
                            return box.sourceCaret->source.first;
                        return work.SourceFirst(box.paragraph);
                    };
                    std::ranges::sort(work.sourceBoxes, {}, [&](auto index) { return std::pair{first(index), index}; });
                    work.lineBox = 0u;
                    work.phase   = Preparation::Phase::SourceLines;
                    continue;
                }
                work.sourceBoxes.push_back(work.lineBox++);
            }
            else if (work.phase == Preparation::Phase::SourceLines)
            {
                auto& prepared  = *work.state;
                const auto& map = prepared.document->Map();
                if (work.lineBox == prepared.boxes.size())
                {
                    if (prepared.sourceLines.empty())
                        prepared.sourceLines.emplace_back(0u, map.Source().size());
                    else
                        prepared.sourceLines.back().second = map.Source().size();
                    if (! map.Source().empty() && (map.Source().back() == L'\n' || map.Source().back() == L'\r') &&
                        prepared.sourceLines.back() != std::pair{map.Source().size(), map.Source().size()})
                        prepared.sourceLines.emplace_back(map.Source().size(), map.Source().size());
                    work.phase = Preparation::Phase::InlineBounds;
                    continue;
                }
                const auto boxIndex = work.sourceBoxes.empty() ? work.lineBox : work.sourceBoxes[work.lineBox];
                const auto& box     = *prepared.boxes[boxIndex];
                const auto& owner   = prepared.document->Elements()[box.paragraph.element];
                if (box.markerOnly || (owner.role == SemanticRole::Paragraph && owner.source.first == owner.source.last))
                {
                    ++work.lineBox;
                    work.lineOffset = 0u;
                    ++result.blockSteps;
                    continue; // Its body's first line owns the same raw prefix.
                }
                if (box.sourceCaret && box.paragraph.table == MarkdownNoNode)
                {
                    const auto& line = *box.sourceCaret;
                    // Keep hidden closing syntax and EOL with the preceding
                    // mapped line, rather than creating an EOL-only line unit.
                    if (! prepared.sourceLines.empty() && line.source.first > work.sourceLineStart)
                    {
                        prepared.sourceLines.back().second = line.source.first;
                        work.sourceLineStart               = line.source.first;
                    }
                    if (line.source.first == map.Source().size() && line.source.last == map.Source().size())
                    {
                        if (prepared.sourceLines.empty())
                            prepared.sourceLines.emplace_back(0u, map.Source().size());
                        else
                            prepared.sourceLines.back().second = map.Source().size();
                        // Finalization adds the single empty EOF unit below.
                    }
                    else if (line.source.last > work.sourceLineStart)
                        prepared.sourceLines.emplace_back(work.sourceLineStart, line.source.last);
                    work.sourceLineStart = std::max(work.sourceLineStart, line.source.last);
                    ++work.lineBox;
                    work.lineOffset = 0u;
                    ++result.blockSteps;
                    continue;
                }
                const auto length = box.paragraph.display.last - box.paragraph.display.first;
                auto end          = box.layout.LineBoundary(work.lineOffset, true);
                if (end < length && map.Display()[box.paragraph.display.first + end] == L'\n')
                    ++end;
                const auto snapped = map.SnapDisplayBoundary(box.paragraph.display.first + end, MappingAffinity::Trailing);
                auto sourceEnd     = map.ToSource(snapped, MappingAffinity::Trailing);
                if (snapped >= box.paragraph.display.last && box.paragraph.table == MarkdownNoNode &&
                    (owner.role == SemanticRole::Paragraph || owner.role == SemanticRole::Heading || owner.role == SemanticRole::Code ||
                     owner.role == SemanticRole::Html || owner.role == SemanticRole::Metadata))
                {
                    // The final raw line owns its hidden closing syntax/EOL,
                    // even when the next source leaf is displayed elsewhere.
                    // An atomic value can snap to this end on its first SDK
                    // fragment; absorb the EOL there rather than inventing an
                    // EOL-only source line when that object wraps visually.
                    sourceEnd = std::max(sourceEnd, owner.source.last);
                }
                // Atomic objects can wrap over several SDK lines. One source
                // line unit owns the whole object; later overlapping fragments
                // must not create reversed or duplicate raw ACP intervals.
                if (sourceEnd > work.sourceLineStart)
                {
                    prepared.sourceLines.emplace_back(work.sourceLineStart, sourceEnd);
                    work.sourceLineStart = sourceEnd;
                }
                if (end >= length || end <= work.lineOffset)
                {
                    ++work.lineBox;
                    work.lineOffset = 0u;
                }
                else
                    work.lineOffset = end;
            }
            else if (work.phase == Preparation::Phase::InlineBounds)
            {
                const auto elements = work.state->document->Elements();
                if (work.boundsIndex == elements.size())
                {
                    work.phase = Preparation::Phase::ParentBounds;
                    continue;
                }
                const auto index    = work.boundsIndex++;
                const auto& element = elements[index];
                if (element.role == SemanticRole::Link || element.role == SemanticRole::Image || element.role == SemanticRole::Math ||
                    element.role == SemanticRole::Task || element.role == SemanticRole::FootnoteReference || element.role == SemanticRole::FootnoteBacklink)
                    for (const auto& rectangle : RangeFromState(*work.state, element.display.first, element.display.last))
                        Union(work.state->bounds[index], rectangle);
            }
            else if (work.phase == Preparation::Phase::ParentBounds)
            {
                if (! work.boundsIndex)
                {
                    work.phase = Preparation::Phase::PaintIndex;
                    continue;
                }
                const auto index  = --work.boundsIndex;
                const auto parent = work.state->document->Elements()[index].parent;
                if (parent != MarkdownNoNode && work.state->bounds[index])
                    Union(work.state->bounds[parent], *work.state->bounds[index]);
            }
            else if (work.phase == Preparation::Phase::PaintIndex)
            {
                auto& state = *work.state;
                if (work.paintGroup == 5u)
                {
                    work.phase = Preparation::Phase::Publish;
                    continue;
                }
                PaintIndex* index = work.paintGroup == 0u   ? &state.paintBoxes
                                    : work.paintGroup == 1u ? &state.paintElements
                                    : work.paintGroup == 2u ? &state.paintNumbers
                                    : work.paintGroup == 3u ? &state.paintHeadingRules
                                                            : &state.paintTableRows;
                if (work.paintPhase == 0u)
                {
                    work.paintCount = work.paintGroup == 0u   ? state.boxes.size()
                                      : work.paintGroup == 1u ? state.bounds.size()
                                      : work.paintGroup == 2u ? state.codeNumbers.size()
                                      : ! state.decorations   ? 0u
                                      : work.paintGroup == 3u ? state.decorations->headingRules.size()
                                                              : state.decorations->tableRows.size();
                    if (! work.paintCount)
                    {
                        ++work.paintGroup;
                        continue;
                    }
                    index->leaves = 1u;
                    while (index->leaves < work.paintCount)
                        index->leaves *= 2u;
                    index->nodes.reserve(index->leaves * 2u);
                    work.paintCursor = 0u;
                    work.paintPhase  = 1u;
                }
                else if (work.paintPhase == 1u)
                {
                    index->nodes.emplace_back();
                    if (index->nodes.size() == index->leaves * 2u)
                    {
                        work.paintCursor = 0u;
                        work.paintPhase  = 2u;
                    }
                }
                else if (work.paintPhase == 2u)
                {
                    if (work.paintCursor == work.paintCount)
                    {
                        work.paintCursor = index->leaves;
                        work.paintPhase  = 3u;
                        continue;
                    }
                    const auto i = work.paintCursor++;
                    std::optional<D2D1_RECT_F> rect;
                    if (work.paintGroup == 0u)
                        rect = state.boxes[i]->frame;
                    else if (work.paintGroup == 1u)
                        rect = state.bounds[i];
                    else if (work.paintGroup == 2u)
                        rect = state.codeNumbers[i].clip;
                    else if (work.paintGroup == 3u)
                        rect = state.decorations->headingRules[i];
                    else
                        rect = state.decorations->tableRows[i];
                    if (rect)
                        index->nodes[index->leaves + i] = {rect->top, rect->bottom};
                }
                else
                {
                    if (work.paintCursor <= 1u)
                    {
                        ++work.paintGroup;
                        work.paintPhase = 0u;
                        continue;
                    }
                    const auto i    = --work.paintCursor;
                    const auto& a   = index->nodes[i * 2u];
                    const auto& b   = index->nodes[i * 2u + 1u];
                    index->nodes[i] = {std::min(a.top, b.top), std::max(a.bottom, b.bottom)};
                }
            }
            else
            {
                if (! work.state->rejected.empty())
                    throw NativeObjectsRejected(work.state->rejected);
                state_ = std::move(work.state);
                next_.reset();
                valid_ = true;
                ++generation_;
                result.status = LayoutPreparationStatus::Complete;
                ++result.blockSteps;
                return result;
            }
            ++result.blockSteps;
        }
    }
    catch (const std::exception&)
    {
        next_.reset();
        throw;
    }
    return result;
}
void PreparedSemanticGeometry::Cancel() noexcept
{
    next_.reset();
}
void PreparedSemanticGeometry::Invalidate() noexcept
{
    Cancel();
    valid_ = false;
}
void PreparedSemanticGeometry::Clear() noexcept
{
    Cancel();
    state_.reset();
    valid_ = false;
}
bool PreparedSemanticGeometry::Ready() const noexcept
{
    return valid_ && static_cast<bool>(state_);
}
bool PreparedSemanticGeometry::Pending() const noexcept
{
    return static_cast<bool>(next_);
}
std::uint64_t PreparedSemanticGeometry::Generation() const noexcept
{
    return generation_;
}
std::uint64_t PreparedSemanticGeometry::Revision() const noexcept
{
    return state_ ? state_->document->Syntax()->Revision() : 0u;
}
float PreparedSemanticGeometry::Height() const noexcept
{
    return state_ ? state_->height : 0.0f;
}
float PreparedSemanticGeometry::Width() const noexcept
{
    return state_ ? state_->width : 0.0f;
}
bool PreparedSemanticGeometry::HasMarkedText() const noexcept
{
    return state_ && state_->markedText;
}
std::size_t PreparedSemanticGeometry::BoxCount() const noexcept
{
    return state_ ? state_->boxes.size() : 0u;
}
std::size_t PreparedSemanticGeometry::ReusedBoxCount() const noexcept
{
    return state_ ? state_->reusedBoxes : 0u;
}
bool PreparedSemanticGeometry::HasNativeObjects() const noexcept
{
    return state_ && (state_->native || state_->images);
}
const PreparedSemanticGeometry::State& PreparedSemanticGeometry::Current() const
{
    if (! Ready())
        throw std::logic_error("Semantic geometry is not current");
    return *state_;
}
const SemanticDocument& PreparedSemanticGeometry::Document() const
{
    return *Current().document;
}
CaretGeometry PreparedSemanticGeometry::Caret(std::size_t offset) const
{
    const auto& state = Current();
    const auto& box   = state.BoxAt(offset);
    auto result       = box.Caret(std::clamp(offset, box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first);
    result.x += box.x;
    result.y += box.y;
    return result;
}
Hit PreparedSemanticGeometry::HitTest(float x, float y) const
{
    const auto& state  = Current();
    const auto& chosen = *state.boxes[state.NearestBox(x, y)];
    auto hit           = chosen.layout.HitTest(x - chosen.x, y - chosen.y);
    hit.position += chosen.paragraph.display.first;
    hit.position = state.document->Map().SnapDisplayBoundary(hit.position, hit.trailing ? MappingAffinity::Trailing : MappingAffinity::Leading);
    return hit;
}
CaretGeometry PreparedSemanticGeometry::SourceCaret(std::size_t source, std::size_t site) const
{
    const auto& state  = Current();
    const auto& box    = *state.boxes[state.SourceBox(source, site)];
    const auto display = box.sourceCaret ? (box.caretOnly ? box.paragraph.display.first
                                            : state.document->Syntax()->Nodes()[box.paragraph.syntaxNode].block.kind == MarkdownBlockKind::ListItem
                                                ? box.paragraph.display.last
                                                : box.paragraph.display.first)
                                         : state.document->Map().ToDisplay(source);
    auto result        = box.Caret(std::clamp(display, box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first);
    result.x += box.x;
    result.y += box.y;
    return result;
}
SourceCaretHit PreparedSemanticGeometry::HitTestSource(float x, float y) const
{
    const auto& state = Current();
    return state.HitSourceBox(state.NearestBox(x, y), x, y);
}
SourceCaretHit PreparedSemanticGeometry::AdjacentSourceLine(std::size_t source, std::size_t site, float x, bool down) const
{
    const auto& state = Current();
    if (! std::isfinite(x))
        throw std::invalid_argument("Invalid semantic preferred caret x");
    const auto index = state.SourceBox(source, site);
    const auto caret = SourceCaret(source, index);
    SourceCaretHit result{source, index};
    float best = std::numeric_limits<float>::max(), bestX = best;
    // Boxes are bounded and placed in visual flow order; tables can share a
    // row. Scan toward the requested direction so ordinary nearest lines win
    // before more distant SDK hits. Span reads and scalar queries allocate no
    // storage; atomic objects retain their existing source hit/caret policy.
    for (std::size_t offset = 0u; offset < state.boxes.size(); ++offset)
    {
        const auto boxIndex = down ? offset : state.boxes.size() - 1u - offset;
        const auto& box     = *state.boxes[boxIndex];
        if (box.markerOnly)
            continue;
        const auto lines = PreparedLayoutAccess::LineMetrics(box.layout);
        float lineY      = box.y;
        for (const auto& line : lines)
        {
            const auto bottom    = lineY + line.height;
            const auto potential = down ? std::max(0.0f, lineY - caret.y) : std::max(0.0f, caret.y - bottom);
            if (potential <= best + .01f && (down ? bottom > caret.y + .01f : lineY < caret.y - .01f))
            {
                const auto hit       = state.HitSourceBox(boxIndex, x, lineY + line.height * .5f);
                const auto candidate = SourceCaret(hit.position, hit.site);
                const auto distance  = down ? candidate.y - caret.y : caret.y - candidate.y;
                const auto dx        = std::abs(candidate.x - x);
                if (distance > .01f && (distance < best - .01f || (std::abs(distance - best) <= .01f && dx < bestX)))
                {
                    best   = distance;
                    bestX  = dx;
                    result = hit;
                }
            }
            lineY = bottom;
        }
    }
    return result;
}
SourceCaretHit PreparedSemanticGeometry::StepSource(std::size_t source, std::size_t site, bool forwards) const
{
    const auto& state = Current();
    const auto index  = state.SourceBox(source, site);
    const auto& box   = *state.boxes[index];
    const auto& map   = state.document->Map();
    auto display      = std::clamp(map.ToDisplay(source), box.paragraph.display.first, box.paragraph.display.last);
    if (! box.sourceCaret && ! forwards)
    {
        const auto spans = map.Spans();
        const auto token = std::upper_bound(spans.begin(), spans.end(), display, [](auto offset, const auto& span) { return offset < span.display.last; });
        // A legal raw ACP can be inside an atomic token whose leading projection
        // is already its first display stop. Left still reaches its leading raw
        // source boundary instead of stalling at that projected display stop.
        if (token != spans.end() && (token->kind == DisplaySpanKind::Transformed || token->kind == DisplaySpanKind::Object) && token->source.first < source &&
            source < token->source.last)
            return {token->source.first, index};
    }
    if (! box.sourceCaret)
        while (forwards ? display < box.paragraph.display.last : display > box.paragraph.display.first)
        {
            const auto local = display - box.paragraph.display.first;
            const auto next  = box.paragraph.display.first + (forwards ? box.layout.NextCluster(local) : box.layout.PreviousCluster(local));
            if (next == display)
                break;
            display           = map.SnapDisplayBoundary(next, forwards ? MappingAffinity::Trailing : MappingAffinity::Leading);
            const auto result = map.ToSource(display, forwards ? MappingAffinity::Trailing : MappingAffinity::Leading);
            // Generated marker anchors can lie ahead of a raw caret inside
            // opening syntax. Left must not follow them forward, nor Right
            // backward. Those anchors are not owned logical source stops.
            if (forwards ? result > source : result < source)
                return {result, index};
        }
    auto neighbor            = index;
    const auto footnoteOwner = [&](const SemanticBox& candidate) noexcept
    {
        const auto& nodes = state.document->Syntax()->Nodes();
        for (auto node = candidate.paragraph.syntaxNode; node < nodes.size(); node = nodes[node].parent)
            if (nodes[node].block.kind == MarkdownBlockKind::FootnoteDefinition)
                return node;
        return MarkdownNoNode;
    };
    const auto currentFootnote = footnoteOwner(box);
    while (forwards ? neighbor + 1u < state.boxes.size() : neighbor > 0u)
    {
        neighbor           = forwards ? neighbor + 1u : neighbor - 1u;
        const auto& target = *state.boxes[neighbor];
        if (target.markerOnly)
            continue;
        const auto targetFootnote = footnoteOwner(target);
        const bool reordered      = currentFootnote != targetFootnote && (currentFootnote != MarkdownNoNode || targetFootnote != MarkdownNoNode);
        if (target.sourceCaret)
        {
            const auto insertion = target.sourceCaret->insertion;
            // Equal ACPs are meaningful only for explicit visual source sites,
            // such as padded cells. Generated marker anchors must not reverse
            // direction or add an invisible extra arrow stop between layouts.
            if (reordered || (forwards ? insertion >= source : insertion <= source))
                return {insertion, neighbor};
            continue;
        }
        auto position = forwards ? target.paragraph.display.first : target.paragraph.display.last;
        auto raw      = map.ToSource(position, forwards ? MappingAffinity::Leading : MappingAffinity::Trailing);
        if (reordered ? raw != source && state.Belongs(target, raw) : (forwards ? raw > source : raw < source))
            return {raw, neighbor};
        while (forwards ? position < target.paragraph.display.last : position > target.paragraph.display.first)
        {
            const auto local = position - target.paragraph.display.first;
            const auto next  = target.paragraph.display.first + (forwards ? target.layout.NextCluster(local) : target.layout.PreviousCluster(local));
            if (next == position)
                break;
            position = map.SnapDisplayBoundary(next, forwards ? MappingAffinity::Trailing : MappingAffinity::Leading);
            raw      = map.ToSource(position, forwards ? MappingAffinity::Trailing : MappingAffinity::Leading);
            if (reordered ? raw != source && state.Belongs(target, raw) : (forwards ? raw > source : raw < source))
                return {raw, neighbor};
        }
    }
    return {source, index};
}
SourceCaretHit PreparedSemanticGeometry::SourceLineBoundary(std::size_t source, std::size_t site, bool end) const
{
    const auto& state = Current();
    const auto index  = state.SourceBox(source, site);
    const auto& box   = *state.boxes[index];
    if (box.sourceCaret)
        return {box.sourceCaret->insertion, index};
    const auto& map  = state.document->Map();
    const auto local = std::clamp(map.ToDisplay(source), box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first;
    const auto boundary =
        map.SnapDisplayBoundary(box.paragraph.display.first + box.layout.LineBoundary(local, end), end ? MappingAffinity::Trailing : MappingAffinity::Leading);
    return {map.ToSource(boundary, end ? MappingAffinity::Trailing : MappingAffinity::Leading), index};
}
bool PreparedSemanticGeometry::IsEmptySourceSite(std::size_t source, std::size_t site) const
{
    const auto& state = Current();
    return state.boxes[state.SourceBox(source, site)]->sourceCaret.has_value();
}
bool PreparedSemanticGeometry::IsCellSourceSite(std::size_t source, std::size_t site) const
{
    const auto& state = Current();
    const auto& box   = *state.boxes[state.SourceBox(source, site)];
    return box.sourceCaret && box.paragraph.table != MarkdownNoNode;
}
std::size_t PreparedSemanticGeometry::SourceSyntaxNode(std::size_t source, std::size_t site) const
{
    const auto& state = Current();
    return state.boxes[state.SourceBox(source, site)]->paragraph.syntaxNode;
}
std::size_t PreparedSemanticGeometry::SourceSiteForSyntaxNode(std::size_t node) const
{
    const auto& state = Current();
    for (std::size_t i = 0u; i < state.boxes.size(); ++i)
        if (! state.boxes[i]->markerOnly && state.boxes[i]->paragraph.syntaxNode == node)
            return i;
    return MarkdownNoNode;
}
std::pair<std::size_t, std::size_t> PreparedSemanticGeometry::VisibleSourceRange(float top, float bottom) const
{
    const auto& state = Current();
    if (! std::isfinite(top) || ! std::isfinite(bottom) || top > bottom)
        throw std::invalid_argument("Invalid semantic source viewport");
    const auto& map   = state.document->Map();
    std::size_t first = map.Source().size(), last = 0u;
    for (const auto& box : state.boxes)
        if (box->frame.bottom > top && box->frame.top < bottom)
        {
            if (box->sourceCaret)
            {
                first = std::min(first, box->sourceCaret->source.first);
                last  = std::max(last, box->sourceCaret->source.last);
            }
            else
            {
                const auto range = box->layout.VisibleRange(top - box->y, bottom - box->y);
                first            = std::min(first, map.ToSource(box->paragraph.display.first + range.first));
                last             = std::max(last, map.ToSource(box->paragraph.display.first + range.second, MappingAffinity::Trailing));
            }
        }
    return first <= last ? std::pair{first, last} : std::pair{std::size_t{0u}, std::size_t{0u}};
}
std::vector<D2D1_RECT_F> PreparedSemanticGeometry::RangeFromState(const State& state, std::size_t first, std::size_t last)
{
    if (first > last || last > state.document->Map().Display().size())
        throw std::out_of_range("Invalid semantic display range");
    std::vector<D2D1_RECT_F> result(0u);
    const auto initial = std::lower_bound(
        state.glyphBoxes.begin(), state.glyphBoxes.end(), first, [&](auto index, auto offset) { return state.boxes[index]->paragraph.display.last <= offset; });
    if (first != last)
        for (auto entry = initial; entry != state.glyphBoxes.end() && state.boxes[*entry]->paragraph.display.first < last; ++entry)
        {
            const auto& box  = *state.boxes[*entry];
            const auto begin = std::max(first, box.paragraph.display.first), end = std::min(last, box.paragraph.display.last);
            if (begin >= end)
                continue;
            const auto append = [&](D2D1_RECT_F rectangle)
            {
                rectangle.left += box.x;
                rectangle.right += box.x;
                rectangle.top += box.y;
                rectangle.bottom += box.y;
                if (box.paragraph.table != MarkdownNoNode)
                {
                    rectangle.left   = std::max(rectangle.left, box.frame.left);
                    rectangle.right  = std::min(rectangle.right, box.frame.right);
                    rectangle.top    = std::max(rectangle.top, box.frame.top);
                    rectangle.bottom = std::min(rectangle.bottom, box.frame.bottom);
                }
                if (rectangle.right > rectangle.left && rectangle.bottom > rectangle.top)
                    result.push_back(rectangle);
            };
            const NativeImagePlacement* atomic{};
            if (box.images)
                for (const auto& image : box.images->placements)
                    if (image.owner == &box && begin >= image.display.first && end <= image.display.last)
                    {
                        atomic = &image;
                        break;
                    }
            if (atomic)
                append(atomic->bounds);
            else
                for (const auto rectangle : box.layout.Range(begin - box.paragraph.display.first, end - box.paragraph.display.first))
                    append(rectangle);
        }
    return std::vector<D2D1_RECT_F>(result);
}
std::vector<D2D1_RECT_F> PreparedSemanticGeometry::Range(std::size_t first, std::size_t last) const
{
    return RangeFromState(Current(), first, last);
}
std::vector<D2D1_RECT_F> PreparedSemanticGeometry::SourceRange(MarkdownRange source) const
{
    const auto& state = Current();
    std::vector<D2D1_RECT_F> result(0u);
    for (const auto fragment : state.document->Map().DisplayRanges(source))
        for (const auto& rectangle : RangeFromState(state, fragment.first, fragment.last))
            result.push_back(rectangle);
    return std::vector<D2D1_RECT_F>(result);
}
std::span<const std::pair<std::size_t, std::size_t>> PreparedSemanticGeometry::SourceLines() const
{
    return Current().sourceLines;
}
std::optional<D2D1_RECT_F> PreparedSemanticGeometry::ElementBounds(std::size_t element) const
{
    const auto& state = Current();
    if (element >= state.bounds.size())
        throw std::out_of_range("Invalid semantic element");
    return state.bounds[element];
}
std::size_t PreparedSemanticGeometry::PreviousCluster(std::size_t offset) const
{
    const auto& state   = Current();
    const auto& box     = state.BoxAt(offset);
    const auto local    = std::clamp(offset, box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first;
    const auto previous = local ? box.paragraph.display.first + box.layout.PreviousCluster(local) : offset ? offset - 1u : 0u;
    return state.document->Map().SnapDisplayBoundary(previous, MappingAffinity::Leading);
}
std::size_t PreparedSemanticGeometry::NextCluster(std::size_t offset) const
{
    const auto& state = Current();
    const auto& box   = state.BoxAt(offset);
    const auto local  = std::clamp(offset, box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first;
    const auto next   = local < box.paragraph.display.last - box.paragraph.display.first ? box.paragraph.display.first + box.layout.NextCluster(local)
                                                                                         : std::min(state.document->Map().Display().size(), offset + 1u);
    return state.document->Map().SnapDisplayBoundary(next, MappingAffinity::Trailing);
}
std::pair<std::size_t, std::size_t> PreparedSemanticGeometry::DeletionRange(std::size_t offset, bool backwards) const
{
    const auto& state   = Current();
    const auto leading  = state.document->Map().SnapDisplayBoundary(offset, MappingAffinity::Leading);
    const auto trailing = state.document->Map().SnapDisplayBoundary(offset, MappingAffinity::Trailing);
    if (leading != trailing)
        return {leading, trailing};
    return backwards ? std::pair{PreviousCluster(offset), offset} : std::pair{offset, NextCluster(offset)};
}
std::size_t PreparedSemanticGeometry::LineBoundary(std::size_t offset, bool end) const
{
    const auto& box  = Current().BoxAt(offset);
    const auto local = std::clamp(offset, box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first;
    return box.paragraph.display.first + box.layout.LineBoundary(local, end);
}
std::pair<std::size_t, std::size_t> PreparedSemanticGeometry::VisibleRange(float top, float bottom) const
{
    const auto& state = Current();
    if (! std::isfinite(top) || ! std::isfinite(bottom) || top > bottom)
        throw std::invalid_argument("Invalid semantic viewport");
    std::size_t first = state.document->Map().Display().size(), last = 0u;
    for (const auto& box : state.boxes)
        if (box->frame.bottom > top && box->frame.top < bottom)
        {
            const auto range = box->layout.VisibleRange(top - box->y, bottom - box->y);
            first            = std::min(first, box->paragraph.display.first + range.first);
            last             = std::max(last, box->paragraph.display.first + range.second);
        }
    return first <= last ? std::pair{first, last} : std::pair{std::size_t{0u}, std::size_t{0u}};
}
void PreparedSemanticGeometry::Draw(ID2D1RenderTarget* target,
                                    ID2D1Brush* text,
                                    ID2D1Brush* decoration,
                                    D2D1_POINT_2F origin,
                                    float top,
                                    float bottom,
                                    ID2D1Brush* fill,
                                    float imageOpacity,
                                    const LinkDrawing* links,
                                    ID2D1Brush* quoteFill) const
{
    const auto& state = Current();
    if (! target || ! text || ! decoration || ! std::isfinite(top) || ! std::isfinite(bottom) || top > bottom || ! std::isfinite(imageOpacity) ||
        imageOpacity < 0.0f || imageOpacity > 1.0f)
        throw std::invalid_argument("Invalid semantic raster request");
    if (state.images && state.images->targetIdentity != target)
        throw std::logic_error("Prepared image target/resources are incompatible");
    if (state.native && ! fill)
        throw std::invalid_argument("Native diagram drawing needs an opaque fill brush");
    const auto elements = state.document->Elements();
    state.paintElements.ForEach(top,
                                bottom,
                                [&](std::size_t i)
    {
        const auto& bounds = state.bounds[i];
        if (! bounds)
            return;
        const auto rectangle  = D2D1::RectF(bounds->left + origin.x, bounds->top + origin.y, bounds->right + origin.x, bounds->bottom + origin.y);
        const auto nodeIndex  = elements[i].syntaxNode;
        const bool styledCode = elements[i].role == SemanticRole::Code && state.options.markdownPresentation == MarkdownPresentationStyle::Subtle &&
                                ! state.document->Syntax()->SourceRegion() && nodeIndex < state.document->Syntax()->Nodes().size() &&
                                (state.document->Syntax()->Nodes()[nodeIndex].block.kind == MarkdownBlockKind::FencedCode ||
                                 state.document->Syntax()->Nodes()[nodeIndex].block.kind == MarkdownBlockKind::IndentedCode);
        const bool tableHeader =
            elements[i].role == SemanticRole::Cell && elements[i].tableHeader && state.options.markdownPresentation == MarkdownPresentationStyle::Subtle;
        if ((elements[i].role == SemanticRole::Quote || styledCode || tableHeader) && quoteFill)
            target->FillRectangle(rectangle, quoteFill);
        const auto copy = styledCode && state.options.codeCopyAction
                              ? CodeCopyBounds(rectangle, state.options.readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT)
                              : std::nullopt;
        if (styledCode && state.codeLabels && (*state.codeLabels)[i])
        {
            auto clip = rectangle;
            if (copy)
            {
                if (state.options.readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT)
                    clip.left = copy->right + 2.0f;
                else
                    clip.right = copy->left - 2.0f;
            }
            target->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            const auto restore = wil::scope_exit([&]() noexcept { target->PopAxisAlignedClip(); });
            (*state.codeLabels)[i]->Draw(target, decoration, D2D1::Point2F(clip.left + 6.0f, clip.top + 3.0f));
        }
        if (copy)
        {
            target->DrawRectangle(*copy, decoration);
            target->DrawRectangle(D2D1::RectF(copy->left + 5.0f, copy->top + 4.0f, copy->left + 12.0f, copy->top + 13.0f), decoration);
            target->DrawLine(D2D1::Point2F(copy->left + 7.0f, copy->top + 15.0f), D2D1::Point2F(copy->left + 14.0f, copy->top + 15.0f), decoration);
            target->DrawLine(D2D1::Point2F(copy->left + 14.0f, copy->top + 15.0f), D2D1::Point2F(copy->left + 14.0f, copy->top + 6.0f), decoration);
        }
        if (elements[i].role == SemanticRole::Image && state.images && state.images->Placed(i))
            return;
        if (elements[i].role == SemanticRole::Cell && state.options.markdownPresentation == MarkdownPresentationStyle::Subtle)
            return;
        if (elements[i].role == SemanticRole::Cell || elements[i].role == SemanticRole::Code || elements[i].role == SemanticRole::Image)
            target->DrawRectangle(rectangle, decoration);
        else if (elements[i].role == SemanticRole::Quote)
        {
            const float x = state.options.readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT ? rectangle.right + 8.0f : rectangle.left - 8.0f;
            target->DrawLine(D2D1::Point2F(x, rectangle.top), D2D1::Point2F(x, rectangle.bottom), decoration, 2.0f);
        }
    });
    state.paintNumbers.ForEach(top,
                               bottom,
                               [&](std::size_t i)
    {
        const auto& number = state.codeNumbers[i];
        const auto clip    = D2D1::RectF(number.clip.left + origin.x, number.clip.top + origin.y, number.clip.right + origin.x, number.clip.bottom + origin.y);
        target->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        const auto restore = wil::scope_exit([&]() noexcept { target->PopAxisAlignedClip(); });
        number.layout->Draw(target, decoration, D2D1::Point2F(number.origin.x + origin.x, number.origin.y + origin.y));
    });
    if (const auto* decorations = state.decorations.get())
    {
        state.paintHeadingRules.ForEach(top,
                                        bottom,
                                        [&](std::size_t i)
        {
            const auto& rule = decorations->headingRules[i];
            target->DrawLine(D2D1::Point2F(rule.left + origin.x, rule.top + origin.y), D2D1::Point2F(rule.right + origin.x, rule.top + origin.y), decoration);
        });
        state.paintTableRows.ForEach(top,
                                     bottom,
                                     [&](std::size_t i)
        {
            const auto& row = decorations->tableRows[i];
            target->DrawLine(D2D1::Point2F(row.left + origin.x, row.bottom - 1.0f + origin.y),
                             D2D1::Point2F(row.right + origin.x, row.bottom - 1.0f + origin.y),
                             decoration);
        });
    }
    state.paintBoxes.ForEach(top,
                             bottom,
                             [&](std::size_t i)
    {
        const auto& box       = state.boxes[i];
        const bool emphasized = links && links->emphasis && links->emphasisBrush && links->emphasis->first < box->paragraph.display.last &&
                                box->paragraph.display.first < links->emphasis->last;
        auto* boxText         = emphasized ? links->emphasisBrush : text;
        if (elements[box->paragraph.element].role == SemanticRole::Rule)
            target->DrawLine(D2D1::Point2F(origin.x + box->x, origin.y + box->y + box->height * .5f),
                             D2D1::Point2F(origin.x + box->x + box->width, origin.y + box->y + box->height * .5f),
                             decoration);
        else
        {
            if (box->paragraph.table != MarkdownNoNode)
                target->PushAxisAlignedClip(
                    D2D1::RectF(origin.x + box->frame.left, origin.y + box->frame.top, origin.x + box->frame.right, origin.y + box->frame.bottom),
                    D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            const auto restoreClip = wil::scope_exit([&]() noexcept
            {
                if (box->paragraph.table != MarkdownNoNode)
                    target->PopAxisAlignedClip();
            });
            if (quoteFill && state.codeChipsAvailable)
                for (const auto& chip : box->codeChips)
                {
                    const auto rectangle = D2D1::RectF(
                        origin.x + box->x + chip.left, origin.y + box->y + chip.top, origin.x + box->x + chip.right, origin.y + box->y + chip.bottom);
                    if (rectangle.bottom > origin.y + top && rectangle.top < origin.y + bottom)
                        target->FillRoundedRectangle(D2D1::RoundedRect(rectangle, 2.0f, 2.0f), quoteFill);
                }
            if (links && links->markedTextFill && state.codeChipsAvailable)
                for (const auto& chip : box->markChips)
                {
                    const auto rectangle = D2D1::RectF(
                        origin.x + box->x + chip.left, origin.y + box->y + chip.top, origin.x + box->x + chip.right, origin.y + box->y + chip.bottom);
                    if (rectangle.bottom <= origin.y + top || rectangle.top >= origin.y + bottom)
                        continue;
                    if (links->outlineMarkedText)
                        target->DrawRectangle(rectangle, text);
                    else
                        target->FillRectangle(rectangle, links->markedTextFill);
                }
            auto localLinks = links ? *links : LinkDrawing{};
            localLinks.displayOrigin += box->paragraph.display.first;
            box->layout.Draw(target, boxText, D2D1::Point2F(origin.x + box->x, origin.y + box->y), links ? &localLinks : nullptr);
            if (box->native &&
                ! box->native->paint->Draw(target,
                                           boxText,
                                           decoration,
                                           fill,
                                           D2D1::Point2F(origin.x + box->x + box->native->bounds.left, origin.y + box->y + box->native->bounds.top)))
                throw std::logic_error("Native diagram target/resources are incompatible");
            if (box->images)
                for (const auto& image : box->images->placements)
                    if (image.owner == box.get() && image.bounds.bottom + box->y > top && image.bounds.top + box->y < bottom)
                    {
                        if (image.math)
                        {
                            image.math->Draw(target,
                                             boxText,
                                             D2D1::Point2F(origin.x + box->x + image.bounds.left, origin.y + box->y + image.bounds.top),
                                             (image.bounds.right - image.bounds.left) / image.Size().width);
                            continue;
                        }
                        target->DrawBitmap(image.bitmap.get(),
                                           D2D1::RectF(origin.x + box->x + image.bounds.left,
                                                       origin.y + box->y + image.bounds.top,
                                                       origin.x + box->x + image.bounds.right,
                                                       origin.y + box->y + image.bounds.bottom),
                                           imageOpacity,
                                           D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                    }
        }
    });
}
} // namespace DxUi::RichText::Detail
