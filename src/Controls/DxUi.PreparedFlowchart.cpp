#include "DxUi.PreparedFlowchart.h"
#include "Support/Utilities.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <wil/resource.h>
#include <wil/result.h>

namespace DxUi::RichText::Detail
{
namespace
{
D2D1_COLOR_F DiagramColor(std::uint32_t color) noexcept
{
    return D2D1::ColorF(
        static_cast<float>((color >> 16u) & 255u) / 255.f, static_cast<float>((color >> 8u) & 255u) / 255.f, static_cast<float>(color & 255u) / 255.f);
}
double Contrast(D2D1_COLOR_F a, D2D1_COLOR_F b) noexcept
{
    return DxUi::Detail::ContrastRatioFromRelativeLuminance(DxUi::Detail::RelativeLuminanceFromSrgb(a.r, a.g, a.b),
                                                            DxUi::Detail::RelativeLuminanceFromSrgb(b.r, b.g, b.b));
}
D2D1_COLOR_F Composite(D2D1_COLOR_F foreground, D2D1_COLOR_F background) noexcept
{
    const auto alpha = std::clamp(foreground.a, 0.f, 1.f);
    return D2D1::ColorF(foreground.r * alpha + background.r * (1.f - alpha),
                        foreground.g * alpha + background.g * (1.f - alpha),
                        foreground.b * alpha + background.b * (1.f - alpha));
}
struct FlowchartLabel
{
    wil::com_ptr<IDWriteTextLayout> layout;
    D2D1_POINT_2F origin{};
    std::array<float, 16u> compartmentY{};
    std::size_t compartmentCount{};
    bool onNodeSurface{};
};
struct EdgeLabelKey
{
    std::wstring_view text;
    const FlowchartLabelStyles* styles{};
};
struct EdgeLabelOrder
{
    bool operator()(const EdgeLabelKey& a, const EdgeLabelKey& b) const noexcept
    {
        if (a.text != b.text)
            return a.text < b.text;
        const auto aCount = a.styles ? a.styles->count : 0u;
        const auto bCount = b.styles ? b.styles->count : 0u;
        if (aCount != bCount)
            return aCount < bCount;
        for (std::size_t index = 0u; index < aCount; ++index)
        {
            const auto& left  = a.styles->spans[index];
            const auto& right = b.styles->spans[index];
            if (left.first != right.first)
                return left.first < right.first;
            if (left.last != right.last)
                return left.last < right.last;
            if (left.flags != right.flags)
                return left.flags < right.flags;
        }
        return false;
    }
};
bool SameTypography(const LayoutOptions& a, const LayoutOptions& b) noexcept
{
    return a.fontFamily == b.fontFamily && a.fontSizeDip == b.fontSizeDip && a.locale == b.locale && a.readingDirection == b.readingDirection &&
           a.diagramLabelWidthDip == b.diagramLabelWidthDip;
}
D2D1_POINT_2F Point(FlowchartPoint point) noexcept
{
    return D2D1::Point2F(static_cast<float>(point.x), static_cast<float>(point.y));
}
D2D1_RECT_F Rect(FlowchartRect rect) noexcept
{
    return D2D1::RectF(
        static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.x + rect.width), static_cast<float>(rect.y + rect.height));
}
wil::com_ptr<ID2D1PathGeometry> Path(ID2D1Factory* factory, std::span<const FlowchartPoint> points, HRESULT& result)
{
    wil::com_ptr<ID2D1PathGeometry> path;
    if (FAILED(result = factory->CreatePathGeometry(path.put())))
        return {};
    wil::com_ptr<ID2D1GeometrySink> sink;
    if (FAILED(result = path->Open(sink.put())))
        return {};
    sink->BeginFigure(Point(points.front()), D2D1_FIGURE_BEGIN_FILLED);
    for (std::size_t index = 1u; index < points.size(); ++index)
        sink->AddLine(Point(points[index]));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(result = sink->Close()))
        return {};
    return path;
}
wil::com_ptr<ID2D1PathGeometry> CylinderPath(ID2D1Factory* factory, FlowchartRect rect, HRESULT& result)
{
    wil::com_ptr<ID2D1PathGeometry> path;
    if (FAILED(result = factory->CreatePathGeometry(path.put())))
        return {};
    wil::com_ptr<ID2D1GeometrySink> sink;
    if (FAILED(result = path->Open(sink.put())))
        return {};
    const auto left = static_cast<float>(rect.x), right = static_cast<float>(rect.x + rect.width);
    const auto top = static_cast<float>(rect.y), bottom = static_cast<float>(rect.y + rect.height);
    const auto radius  = 8.f;
    const auto arcSize = D2D1::SizeF((right - left) / 2.f, radius);
    sink->BeginFigure(D2D1::Point2F(left, top + radius), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddArc({D2D1::Point2F(right, top + radius), arcSize, 0.f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL});
    sink->AddLine(D2D1::Point2F(right, bottom - radius));
    sink->AddArc({D2D1::Point2F(left, bottom - radius), arcSize, 0.f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL});
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(result = sink->Close()))
        return {};
    return path;
}
wil::com_ptr<ID2D1PathGeometry> PiePath(ID2D1Factory* factory, FlowchartRect circle, double start, double end, HRESULT& result)
{
    wil::com_ptr<ID2D1PathGeometry> path;
    if (FAILED(result = factory->CreatePathGeometry(path.put())))
        return {};
    wil::com_ptr<ID2D1GeometrySink> sink;
    if (FAILED(result = path->Open(sink.put())))
        return {};
    const double radius = circle.width / 2.0;
    const auto point    = [&](double angle) noexcept
    {
        return D2D1::Point2F(static_cast<float>(circle.x + radius + radius * std::sin(angle)),
                             static_cast<float>(circle.y + radius - radius * std::cos(angle)));
    };
    sink->BeginFigure(D2D1::Point2F(static_cast<float>(circle.x + radius), static_cast<float>(circle.y + radius)), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(point(start));
    for (const auto angle : {(start + end) / 2.0, end})
    {
        const D2D1_ARC_SEGMENT arc{
            point(angle), D2D1::SizeF(static_cast<float>(radius), static_cast<float>(radius)), 0.f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL};
        sink->AddArc(arc);
    }
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(result = sink->Close()))
        return {};
    return path;
}
} // namespace
D2D1_COLOR_F ResolveDiagramLabelBackground(D2D1_COLOR_F ink, D2D1_COLOR_F surface) noexcept
{
    surface.a           = 1.f;
    const auto readable = [&](D2D1_COLOR_F background) noexcept { return Contrast(Composite(ink, background), background); };
    if (readable(surface) >= 4.5)
        return surface;
    const auto black = D2D1::ColorF(D2D1::ColorF::Black), white = D2D1::ColorF(D2D1::ColorF::White);
    // Some translucent custom ink cannot reach 4.5:1 on any neutral surface.
    // Preserve that caller's ink and choose its strongest available contrast.
    if (readable(black) < 4.5 && readable(white) < 4.5)
        return readable(black) >= readable(white) ? black : white;
    auto result   = surface;
    float closest = std::numeric_limits<float>::max();
    for (const auto endpoint : {black, white})
    {
        if (readable(endpoint) < 4.5)
            continue;
        const auto blend = [&](float amount) noexcept
        {
            return D2D1::ColorF(surface.r + (endpoint.r - surface.r) * amount,
                                surface.g + (endpoint.g - surface.g) * amount,
                                surface.b + (endpoint.b - surface.b) * amount,
                                1.f);
        };
        float low = 0.f, high = 1.f;
        for (unsigned step = 0u; step < 16u; ++step)
        {
            const auto middle = (low + high) * .5f;
            if (readable(blend(middle)) >= 4.5)
                high = middle;
            else
                low = middle;
        }
        const auto candidate = blend(high);
        const float r = candidate.r - surface.r, g = candidate.g - surface.g, b = candidate.b - surface.b;
        const float distance = r * r + g * g + b * b;
        if (distance < closest)
        {
            result  = candidate;
            closest = distance;
        }
    }
    return result;
}
DiagramPalette::DiagramPalette(std::shared_ptr<const FlowchartModel> input, ID2D1RenderTarget* raster)
    : graph(std::move(input)),
      target(raster),
      fills(graph && graph->nodes.size() <= NativeFlowchartParser::MaximumNodes ? graph->nodes.size() : 0u),
      strokes(graph && graph->nodes.size() <= NativeFlowchartParser::MaximumNodes ? graph->nodes.size() : 0u),
      texts(graph && graph->nodes.size() <= NativeFlowchartParser::MaximumNodes ? graph->nodes.size() : 0u),
      edgeStrokes(graph && graph->edges.size() <= NativeFlowchartParser::MaximumEdges ? graph->edges.size() : 0u)
{
    const Failure::Scope failureScope;
    if (! graph || ! raster || graph->nodes.size() > NativeFlowchartParser::MaximumNodes || graph->edges.size() > NativeFlowchartParser::MaximumEdges)
        return;
    for (const auto& node : graph->nodes)
        if (! ValidDiagramColor(node.fillColor) || ! ValidDiagramColor(node.strokeColor) || ! ValidDiagramColor(node.textColor))
            return;
    for (const auto& edge : graph->edges)
        if (! ValidDiagramColor(edge.strokeColor))
            return;
    std::vector<std::uint32_t> colors(0u);
    colors.reserve(graph->nodes.size() * 3u + graph->edges.size());
    for (const auto& node : graph->nodes)
        for (const auto color : {node.fillColor, node.strokeColor, node.textColor})
            if (color != NoDiagramColor)
                colors.push_back(color);
    for (const auto& edge : graph->edges)
        if (edge.strokeColor != NoDiagramColor)
            colors.push_back(edge.strokeColor);
    palette = std::make_unique<NativeColorPalette>(raster, colors);
}
bool DiagramPalette::Needed(const FlowchartModel& graph) noexcept
{
    return std::ranges::any_of(graph.nodes, [](const auto& node) {
        return node.fillColor != NoDiagramColor || node.strokeColor != NoDiagramColor || node.textColor != NoDiagramColor;
    }) || std::ranges::any_of(graph.edges, [](const auto& edge) { return edge.strokeColor != NoDiagramColor; });
}
bool DiagramPalette::Advance()
{
    if (! graph || ! palette)
        return false;
    const auto nodeSlots = graph->nodes.size() * 3u;
    const auto total     = nodeSlots + graph->edges.size();
    while (cursor < total)
    {
        const auto current = cursor;
        std::uint32_t color{};
        wil::com_ptr<ID2D1SolidColorBrush>* brush{};
        if (current < nodeSlots)
        {
            const auto node = current / 3u, kind = current % 3u;
            color = kind == 0u ? graph->nodes[node].fillColor : kind == 1u ? graph->nodes[node].strokeColor : graph->nodes[node].textColor;
            brush = kind == 0u ? std::addressof(fills[node]) : kind == 1u ? std::addressof(strokes[node]) : std::addressof(texts[node]);
        }
        else
        {
            const auto edge = current - nodeSlots;
            color           = graph->edges[edge].strokeColor;
            brush           = std::addressof(edgeStrokes[edge]);
        }
        if (color == NoDiagramColor)
        {
            ++cursor;
            continue;
        }
        *brush = palette->Stage(color);
        if (! *brush)
            return false;
        ++cursor;
        return false;
    }
    ready = true;
    return true;
}
struct PreparedFlowchart::State
{
    static bool HasCardinality(const FlowchartModel& value) noexcept
    {
        for (const auto& edge : value.edges)
            if (! edge.fromCardinality.empty() || ! edge.toCardinality.empty())
                return true;
        return false;
    }
    State(IDWriteFactory* inputWrite,
          ID2D1Factory* inputDrawing,
          std::shared_ptr<const FlowchartModel> inputGraph,
          const LayoutOptions& inputOptions,
          FlowchartResourceEpoch inputEpoch)
        : graph(std::move(inputGraph)),
          write(inputWrite),
          drawing(inputDrawing),
          options(inputOptions),
          epoch(inputEpoch),
          hasCardinality(HasCardinality(*graph)),
          nodeLabels(graph->nodes.size()),
          edgeLabels(graph->edges.size()),
          fromCardinalities(hasCardinality ? graph->edges.size() : 0u),
          toCardinalities(hasCardinality ? graph->edges.size() : 0u),
          groupLabels(graph->family == NativeDiagramFamily::Pie || graph->family == NativeDiagramFamily::Gantt ? 1u : graph->groups.size()),
          nodeSizes(graph->nodes.size()),
          edgeSizes(graph->edges.size()),
          fromCardinalitySizes(hasCardinality ? graph->edges.size() : 0u),
          toCardinalitySizes(hasCardinality ? graph->edges.size() : 0u),
          groupSizes(graph->family == NativeDiagramFamily::Pie || graph->family == NativeDiagramFamily::Gantt ? 1u : graph->groups.size()),
          nodePaths(graph->nodes.size()),
          arrowPaths(graph->edges.size())
    {
    }
    std::shared_ptr<const FlowchartModel> graph;
    wil::com_ptr<IDWriteFactory> write;
    wil::com_ptr<ID2D1Factory> drawing;
    LayoutOptions options;
    FlowchartResourceEpoch epoch{};
    bool hasCardinality{};
    wil::com_ptr<IDWriteTextFormat> format;
    wil::com_ptr<ID2D1StrokeStyle> dottedStyle;
    wil::com_ptr<ID2D1StrokeStyle> dashedStyle;
    std::vector<FlowchartLabel> nodeLabels, edgeLabels, fromCardinalities, toCardinalities, groupLabels;
    // Borrowed keys remain owned by the immutable graph for this generation.
    // At most one representative per admitted edge; no text copies or global cache.
    std::map<EdgeLabelKey, std::size_t, EdgeLabelOrder> edgeLabelCache;
    std::vector<FlowchartSize> nodeSizes, edgeSizes, fromCardinalitySizes, toCardinalitySizes, groupSizes;
    std::vector<wil::com_ptr<ID2D1PathGeometry>> nodePaths, arrowPaths;
    std::shared_ptr<const FlowchartLayoutPlan> geometry;
    bool labelCapacity{};
    bool Matches(IDWriteFactory* inputWrite,
                 ID2D1Factory* inputDrawing,
                 const std::shared_ptr<const FlowchartModel>& inputGraph,
                 const LayoutOptions& inputOptions,
                 FlowchartResourceEpoch inputEpoch) const noexcept
    {
        return write.get() == inputWrite && drawing.get() == inputDrawing && graph == inputGraph && epoch == inputEpoch &&
               SameTypography(options, inputOptions);
    }
    HRESULT Format()
    {
        RETURN_IF_FAILED(write->CreateTextFormat(options.fontFamily.c_str(),
                                                 nullptr,
                                                 DWRITE_FONT_WEIGHT_NORMAL,
                                                 DWRITE_FONT_STYLE_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL,
                                                 options.fontSizeDip,
                                                 options.locale.c_str(),
                                                 format.put()));
        RETURN_IF_FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
        RETURN_IF_FAILED(format->SetReadingDirection(options.readingDirection));
        RETURN_IF_FAILED(format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
        RETURN_IF_FAILED(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
        if (graph->family == NativeDiagramFamily::Sequence ||
            std::ranges::any_of(graph->edges, [](const auto& edge) { return edge.style == FlowchartLineStyle::Dotted; }))
        {
            const auto properties = D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 10.f, D2D1_DASH_STYLE_CUSTOM, 0.f);
            constexpr std::array<FLOAT, 2u> dashes{1.f, 3.f};
            RETURN_IF_FAILED(drawing->CreateStrokeStyle(properties, dashes.data(), static_cast<UINT32>(dashes.size()), dottedStyle.put()));
        }
        if (std::ranges::any_of(graph->nodes, [](const auto& node) { return node.borderDashed; }) ||
            std::ranges::any_of(graph->edges, [](const auto& edge) { return edge.dashed; }))
        {
            const auto properties = D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN_MITER, 10.f, D2D1_DASH_STYLE_CUSTOM, 0.f);
            constexpr std::array<FLOAT, 2u> dashes{5.f, 5.f};
            RETURN_IF_FAILED(drawing->CreateStrokeStyle(properties, dashes.data(), static_cast<UINT32>(dashes.size()), dashedStyle.put()));
        }
        return S_OK;
    }
    HRESULT Label(std::wstring_view text, FlowchartLabel& output, FlowchartSize& size, const FlowchartLabelStyles* styles = nullptr)
    {
        wil::com_ptr<IDWriteTextLayout> layout;
        constexpr auto maximum = static_cast<float>(NativeFlowchartLayout::MaximumExtent);
        const auto labelWidth =
            options.diagramLabelWidthDip > 0.f ? options.diagramLabelWidthDip : std::min(maximum, std::max(1.f, options.fontSizeDip * 12.5f));
        RETURN_IF_FAILED(write->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format.get(), labelWidth, maximum, layout.put()));
        if (styles)
            for (std::size_t index = 0u; index < styles->count; ++index)
            {
                const auto& style = styles->spans[index];
                const DWRITE_TEXT_RANGE range{static_cast<UINT32>(style.first), static_cast<UINT32>(style.last - style.first)};
                if (style.flags & LabelBold)
                    RETURN_IF_FAILED(layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range));
                if (style.flags & LabelItalic)
                    RETURN_IF_FAILED(layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range));
                if (style.flags & LabelUnderline)
                    RETURN_IF_FAILED(layout->SetUnderline(TRUE, range));
                if (style.flags & LabelStrike)
                    RETURN_IF_FAILED(layout->SetStrikethrough(TRUE, range));
            }
        DWRITE_TEXT_METRICS metrics{};
        DWRITE_OVERHANG_METRICS ink{};
        RETURN_IF_FAILED(layout->GetMetrics(&metrics));
        RETURN_IF_FAILED(layout->GetOverhangMetrics(&ink));
        for (const auto value : {metrics.left,
                                 metrics.top,
                                 metrics.widthIncludingTrailingWhitespace,
                                 metrics.height,
                                 metrics.layoutWidth,
                                 metrics.layoutHeight,
                                 ink.left,
                                 ink.top,
                                 ink.right,
                                 ink.bottom})
            if (! std::isfinite(value))
                return E_UNEXPECTED;
        // Logical whitespace/line extents and actual glyph ink both belong to
        // the measured label. RTL leading alignment may put ink near the far
        // layout edge; subtract its actual origin rather than rotating text.
        const auto left = std::min(metrics.left, -ink.left), top = std::min(metrics.top, -ink.top);
        const auto right  = std::max(metrics.left + metrics.widthIncludingTrailingWhitespace, metrics.layoutWidth + ink.right);
        const auto bottom = std::max(metrics.top + metrics.height, metrics.layoutHeight + ink.bottom);
        const auto width = static_cast<double>(right) - left, height = static_cast<double>(bottom) - top;
        if (! std::isfinite(width) || ! std::isfinite(height) || width < 0.0 || height < 0.0)
            return E_UNEXPECTED;
        // Large ink becomes an explicit capacity result; measured values are
        // never clamped into a seemingly renderable, clipped diagram.
        labelCapacity = labelCapacity || width > NativeFlowchartLayout::MaximumExtent || height > NativeFlowchartLayout::MaximumExtent;
        size          = {width, height};
        output.origin = D2D1::Point2F(-left, -top);
        output.layout = std::move(layout);
        return S_OK;
    }
};
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
struct PreparedFlowchart::Preparation
{
    enum class Stage
    {
        Format,
        Labels,
        GroupLabels,
        BeginGeometry,
        Geometry,
        NodePaths,
        ArrowPaths,
        Publish
    };
    Preparation(
        IDWriteFactory* write, ID2D1Factory* drawing, std::shared_ptr<const FlowchartModel> graph, const LayoutOptions& options, FlowchartResourceEpoch epoch)
        : state(std::make_unique<State>(write, drawing, std::move(graph), options, epoch))
    {
    }
    std::unique_ptr<State> state;
    NativeFlowchartLayout layout;
    Stage stage{};
    std::size_t cursor{};
};
#pragma warning(pop)
PreparedFlowchart::PreparedFlowchart() noexcept  = default;
PreparedFlowchart::~PreparedFlowchart() noexcept = default;
LayoutPreparationStatus PreparedFlowchart::Begin(IDWriteFactory* write,
                                                 ID2D1Factory* drawing,
                                                 std::shared_ptr<const FlowchartModel> graph,
                                                 const LayoutOptions& typography,
                                                 FlowchartResourceEpoch resourceEpoch)
{
    const Failure::Scope failureScope;
    const auto refuse = [this](HRESULT error)
    {
        result_ = error;
        return LayoutPreparationStatus::Failed;
    };
    if (! write || ! drawing || ! graph || graph->outcome != FlowchartOutcome::Prepared || graph->nodes.size() > NativeFlowchartParser::MaximumNodes ||
        graph->edges.size() > NativeFlowchartParser::MaximumEdges || graph->groups.size() > 16u ||
        graph->source.size() > NativeFlowchartParser::MaximumSourceUnits || ! std::isfinite(typography.fontSizeDip) || typography.fontSizeDip <= 0.0f ||
        typography.fontSizeDip > 512.0f || ! std::isfinite(typography.diagramLabelWidthDip) || typography.diagramLabelWidthDip < 0.f ||
        typography.diagramLabelWidthDip > 4096.f || (typography.diagramLabelWidthDip > 0.f && typography.diagramLabelWidthDip < 1.f) ||
        typography.fontFamily.size() > 1024u || typography.locale.size() > 1024u ||
        (typography.readingDirection != DWRITE_READING_DIRECTION_LEFT_TO_RIGHT && typography.readingDirection != DWRITE_READING_DIRECTION_RIGHT_TO_LEFT))
        return refuse(E_INVALIDARG);
    if (! ValidFlowchartModel(*graph))
        return refuse(E_INVALIDARG);
    result_ = S_OK;
    if (next_ && next_->state->Matches(write, drawing, graph, typography, resourceEpoch))
        return LayoutPreparationStatus::Pending;
    if (! next_ && valid_ && state_ && state_->Matches(write, drawing, graph, typography, resourceEpoch))
        return LayoutPreparationStatus::Complete;
    auto next = std::make_unique<Preparation>(write, drawing, std::move(graph), typography, resourceEpoch);
    next_     = std::move(next);
    return LayoutPreparationStatus::Pending;
}
LayoutAdvance PreparedFlowchart::Advance(LayoutWorkBudget budget)
{
    const Failure::Scope failureScope;
    LayoutAdvance result;
    const auto fail = [&](HRESULT error)
    {
        next_.reset();
        result_       = error;
        result.status = LayoutPreparationStatus::Failed;
        result.result = error;
        return result;
    };
    if (! next_)
        return result;
    // A refused Begin does not cancel the previous pending request. Its
    // admission error belongs to that call, not to this resumed stage.
    result_       = S_OK;
    result.status = LayoutPreparationStatus::Pending;
    if (! budget.maximumLayouts || ! budget.maximumBlockSteps)
        return result;
    {
        while (result.blockSteps < budget.maximumBlockSteps)
        {
            auto& work  = *next_;
            auto& state = *work.state;
            switch (work.stage)
            {
                case Preparation::Stage::Format:
                    if (const auto status = state.Format(); FAILED(status))
                        return fail(status);
                    work.stage = Preparation::Stage::Labels;
                    break;
                case Preparation::Stage::Labels:
                {
                    const auto nodes = state.graph->nodes.size();
                    const auto edges = state.graph->edges.size();
                    if (work.cursor == nodes + (state.hasCardinality ? 3u : 1u) * edges)
                    {
                        work.cursor = 0u;
                        work.stage  = Preparation::Stage::GroupLabels;
                        break;
                    }
                    const auto node   = work.cursor < nodes;
                    const auto group  = node ? 0u : (work.cursor - nodes) / edges;
                    const auto index  = node ? work.cursor : (work.cursor - nodes) % edges;
                    const auto* edge  = node ? nullptr : &state.graph->edges[index];
                    const auto& label = node          ? state.graph->nodes[index].label
                                        : group == 0u ? edge->label
                                        : group == 1u ? edge->fromCardinality
                                                      : edge->toCardinality;
                    if (node || ! label.empty())
                    {
                        auto& preparedLabel = node          ? state.nodeLabels[index]
                                              : group == 0u ? state.edgeLabels[index]
                                              : group == 1u ? state.fromCardinalities[index]
                                                            : state.toCardinalities[index];
                        auto& measuredSize  = node          ? state.nodeSizes[index]
                                              : group == 0u ? state.edgeSizes[index]
                                              : group == 1u ? state.fromCardinalitySizes[index]
                                                            : state.toCardinalitySizes[index];
                        // Node compartments and numbered messages have separate
                        // postprocessing/temporary text. Only immutable edge labels
                        // share layouts; every edge keeps its own source and placement.
                        const bool cacheable = ! node && group == 0u && ! edge->sequenceNumber;
                        const EdgeLabelKey key{label, cacheable ? &edge->styles : nullptr};
                        if (cacheable)
                        {
                            const auto found = state.edgeLabelCache.find(key);
                            if (found != state.edgeLabelCache.end())
                            {
                                preparedLabel = state.edgeLabels[found->second];
                                measuredSize  = state.edgeSizes[found->second];
                                ++work.cursor;
                                break;
                            }
                        }
                        if (result.layoutsPrepared == budget.maximumLayouts)
                            return result;
                        ++result.layoutsPrepared;
                        if (node && state.graph->family == NativeDiagramFamily::Gantt)
                        {
                            const auto& task  = state.graph->nodes[index];
                            const auto source = std::wstring_view(state.graph->source);
                            std::wstring legend(0u, L' ');
                            if (! task.ganttSection.empty())
                                legend.append(task.ganttSection).append(L" / ");
                            legend.append(label)
                                .append(L" (")
                                .append(source.substr(task.ganttStartSource.first, task.ganttStartSource.last - task.ganttStartSource.first))
                                .append(L", ")
                                .append(source.substr(task.ganttEndSource.first, task.ganttEndSource.last - task.ganttEndSource.first))
                                .append(L")");
                            if (const auto status = state.Label(legend, preparedLabel, measuredSize); FAILED(status))
                                return fail(status);
                        }
                        else if (node && state.graph->family == NativeDiagramFamily::Pie && state.graph->pieShowData)
                        {
                            const auto source = state.graph->nodes[index].valueSource;
                            std::wstring legend(0u, L' ');
                            legend.append(label).append(L": ").append(std::wstring_view(state.graph->source).substr(source.first, source.last - source.first));
                            if (const auto status = state.Label(legend, preparedLabel, measuredSize); FAILED(status))
                                return fail(status);
                        }
                        else if (! node && group == 0u && edge->sequenceNumber)
                        {
                            if (const auto status = state.Label(SequenceMessageText(*edge), preparedLabel, measuredSize); FAILED(status))
                                return fail(status);
                        }
                        else if (const auto status = state.Label(label,
                                                                 preparedLabel,
                                                                 measuredSize,
                                                                 node          ? &state.graph->nodes[index].styles
                                                                 : group == 0u ? &state.graph->edges[index].styles
                                                                               : nullptr);
                                 FAILED(status))
                            return fail(status);
                        if (node)
                        {
                            auto& prepared    = state.nodeLabels[index];
                            const auto& model = state.graph->nodes[index];
                            for (std::size_t compartment = 0u; compartment < model.compartmentCount; ++compartment)
                            {
                                FLOAT x{}, y{};
                                DWRITE_HIT_TEST_METRICS hit{};
                                if (const auto status =
                                        prepared.layout->HitTestTextPosition(static_cast<UINT32>(model.compartmentStarts[compartment]), FALSE, &x, &y, &hit);
                                    FAILED(status))
                                    return fail(status);
                                if (! std::isfinite(y))
                                    return fail(E_UNEXPECTED);
                                prepared.compartmentY[compartment] = y + prepared.origin.y;
                            }
                            prepared.compartmentCount = model.compartmentCount;
                        }
                        if (cacheable)
                            state.edgeLabelCache.emplace(key, index);
                    }
                    ++work.cursor;
                    break;
                }
                case Preparation::Stage::GroupLabels:
                    if (work.cursor == state.groupLabels.size())
                    {
                        work.stage = Preparation::Stage::BeginGeometry;
                        break;
                    }
                    if ((state.graph->family == NativeDiagramFamily::Pie && state.graph->pieTitle.empty()) ||
                        (state.graph->family == NativeDiagramFamily::Gantt && state.graph->ganttTitle.empty()))
                    {
                        ++work.cursor;
                        break;
                    }
                    if (result.layoutsPrepared == budget.maximumLayouts)
                        return result;
                    ++result.layoutsPrepared;
                    if (const auto status =
                            state.Label(state.graph->family == NativeDiagramFamily::Pie     ? std::wstring_view(state.graph->pieTitle)
                                        : state.graph->family == NativeDiagramFamily::Gantt ? std::wstring_view(state.graph->ganttTitle)
                                                                                            : std::wstring_view(state.graph->groups[work.cursor].title),
                                        state.groupLabels[work.cursor],
                                        state.groupSizes[work.cursor]);
                        FAILED(status))
                        return fail(status);
                    ++work.cursor;
                    break;
                case Preparation::Stage::BeginGeometry:
                    if (state.labelCapacity)
                    {
                        auto rejected   = std::make_shared<FlowchartLayoutPlan>();
                        rejected->issue = FlowchartLayoutIssue::DiagramExtent;
                        state.geometry  = std::move(rejected);
                        work.stage      = Preparation::Stage::Publish;
                        break;
                    }
                    if (work.layout.Begin(
                            state.graph, state.nodeSizes, state.edgeSizes, state.fromCardinalitySizes, state.toCardinalitySizes, state.groupSizes) !=
                        SourceResult::Success)
                        return fail(E_INVALIDARG);
                    work.stage = Preparation::Stage::Geometry;
                    break;
                case Preparation::Stage::Geometry:
                    if (work.layout.Advance(1u).status == MarkdownPreparationStatus::Complete)
                    {
                        state.geometry = work.layout.Published();
                        work.cursor    = 0u;
                        work.stage     = state.geometry->issue == FlowchartLayoutIssue::None ? Preparation::Stage::NodePaths : Preparation::Stage::Publish;
                    }
                    break;
                case Preparation::Stage::NodePaths:
                    if (work.cursor == state.geometry->nodes.size())
                    {
                        work.cursor = 0u;
                        work.stage  = Preparation::Stage::ArrowPaths;
                        break;
                    }
                    if (state.graph->family == NativeDiagramFamily::Pie)
                    {
                        const auto& slice = state.geometry->nodes[work.cursor];
                        if (slice.pieEnd > slice.pieStart)
                            state.nodePaths[work.cursor] = PiePath(state.drawing.get(), state.geometry->pieCircle, slice.pieStart, slice.pieEnd, result_);
                    }
                    else if (state.geometry->nodes[work.cursor].shape == FlowchartShape::Cylinder)
                        state.nodePaths[work.cursor] = CylinderPath(state.drawing.get(), state.geometry->nodes[work.cursor].bounds, result_);
                    else if (state.geometry->nodes[work.cursor].shape == FlowchartShape::Diamond)
                    {
                        const auto rect                             = state.geometry->nodes[work.cursor].bounds;
                        const std::array<FlowchartPoint, 4u> points = {{{rect.x + rect.width / 2.0, rect.y},
                                                                        {rect.x + rect.width, rect.y + rect.height / 2.0},
                                                                        {rect.x + rect.width / 2.0, rect.y + rect.height},
                                                                        {rect.x, rect.y + rect.height / 2.0}}};
                        state.nodePaths[work.cursor]                = Path(state.drawing.get(), points, result_);
                    }
                    else if (state.geometry->nodes[work.cursor].shape == FlowchartShape::Hexagon)
                    {
                        const auto rect                             = state.geometry->nodes[work.cursor].bounds;
                        const std::array<FlowchartPoint, 6u> points = {{{rect.x + rect.width * .25, rect.y},
                                                                        {rect.x + rect.width * .75, rect.y},
                                                                        {rect.x + rect.width, rect.y + rect.height * .5},
                                                                        {rect.x + rect.width * .75, rect.y + rect.height},
                                                                        {rect.x + rect.width * .25, rect.y + rect.height},
                                                                        {rect.x, rect.y + rect.height * .5}}};
                        state.nodePaths[work.cursor]                = Path(state.drawing.get(), points, result_);
                    }
                    else if (state.geometry->nodes[work.cursor].shape >= FlowchartShape::Asymmetric &&
                             state.geometry->nodes[work.cursor].shape <= FlowchartShape::ReverseTrapezoid)
                    {
                        const auto& node = state.geometry->nodes[work.cursor];
                        const auto r     = node.bounds;
                        const auto point = [&](double x, double y) { return FlowchartPoint{r.x + x * r.width, r.y + y * r.height}; };
                        if (node.shape == FlowchartShape::Asymmetric)
                        {
                            const std::array points{point(0.0, 0.0), point(.75, 0.0), point(1.0, .5), point(.75, 1.0), point(0.0, 1.0)};
                            state.nodePaths[work.cursor] = Path(state.drawing.get(), points, result_);
                        }
                        else
                        {
                            const auto topLeft  = node.shape == FlowchartShape::Parallelogram || node.shape == FlowchartShape::Trapezoid ? .25 : 0.0;
                            const auto topRight = node.shape == FlowchartShape::ReverseParallelogram || node.shape == FlowchartShape::Trapezoid ? .75 : 1.0;
                            const auto bottomLeft =
                                node.shape == FlowchartShape::ReverseParallelogram || node.shape == FlowchartShape::ReverseTrapezoid ? .25 : 0.0;
                            const auto bottomRight = node.shape == FlowchartShape::Parallelogram || node.shape == FlowchartShape::ReverseTrapezoid ? .75 : 1.0;
                            const std::array points{point(topLeft, 0.0), point(topRight, 0.0), point(bottomRight, 1.0), point(bottomLeft, 1.0)};
                            state.nodePaths[work.cursor] = Path(state.drawing.get(), points, result_);
                        }
                    }
                    ++work.cursor;
                    break;
                case Preparation::Stage::ArrowPaths:
                    if (work.cursor == state.geometry->edges.size())
                    {
                        work.stage = Preparation::Stage::Publish;
                        break;
                    }
                    if (state.geometry->edges[work.cursor].hasArrow && ! state.geometry->edges[work.cursor].openArrow)
                    {
                        const auto& edge = state.geometry->edges[work.cursor];
                        if (edge.relation == FlowchartRelation::Aggregation || edge.relation == FlowchartRelation::Composition)
                            state.arrowPaths[work.cursor] = Path(state.drawing.get(), edge.diamond, result_);
                        else
                            state.arrowPaths[work.cursor] = Path(state.drawing.get(), edge.arrow, result_);
                    }
                    {
                        const auto& edge        = state.geometry->edges[work.cursor];
                        const auto groupSurface = [&](FlowchartLabel& label, FlowchartRect bounds)
                        {
                            const auto x = bounds.x + bounds.width * .5, y = bounds.y + bounds.height * .5;
                            label.onNodeSurface = edge.sequenceNote || std::ranges::any_of(state.geometry->groups, [&](const auto& group) {
                                return x >= group.bounds.x && x <= group.bounds.x + group.bounds.width && y >= group.bounds.y &&
                                       y <= group.bounds.y + group.bounds.height;
                            });
                        };
                        groupSurface(state.edgeLabels[work.cursor], edge.label);
                        if (state.hasCardinality)
                        {
                            groupSurface(state.fromCardinalities[work.cursor], edge.fromCardinality);
                            groupSurface(state.toCardinalities[work.cursor], edge.toCardinality);
                        }
                    }
                    ++work.cursor;
                    break;
                case Preparation::Stage::Publish:
                    state_ = std::move(work.state);
                    next_.reset();
                    valid_ = true;
                    ++generation_;
                    result.status = LayoutPreparationStatus::Complete;
                    ++result.blockSteps;
                    return result;
            }
            if (FAILED(result_))
                return fail(result_);
            ++result.blockSteps;
        }
    }
    return result;
}
void PreparedFlowchart::Cancel() noexcept
{
    next_.reset();
}
void PreparedFlowchart::Invalidate() noexcept
{
    Cancel();
    valid_ = false;
}
void PreparedFlowchart::Clear() noexcept
{
    Invalidate();
    state_.reset();
}
bool PreparedFlowchart::Pending() const noexcept
{
    return ! ! next_;
}
bool PreparedFlowchart::Ready() const noexcept
{
    return valid_ && state_ && state_->geometry && state_->geometry->issue == FlowchartLayoutIssue::None;
}
std::uint64_t PreparedFlowchart::Generation() const noexcept
{
    return generation_;
}
std::uint64_t PreparedFlowchart::Revision() const noexcept
{
    return state_ ? state_->graph->revision : 0u;
}
std::shared_ptr<const FlowchartLayoutPlan> PreparedFlowchart::Geometry() const noexcept
{
    return state_ ? state_->geometry : nullptr;
}
bool PreparedFlowchart::Draw(ID2D1RenderTarget* target,
                             ID2D1Brush* text,
                             ID2D1Brush* stroke,
                             ID2D1Brush* fill,
                             D2D1_POINT_2F origin,
                             const DiagramPalette* palette,
                             const DiagramColorContext* colors,
                             ID2D1Brush* labelFill) const noexcept
{
    if (! Ready() || ! target || ! text || ! stroke || ! fill || ! std::isfinite(origin.x) || ! std::isfinite(origin.y))
        return false;
    wil::com_ptr<ID2D1Factory> drawing;
    target->GetFactory(drawing.put());
    if (drawing.get() != state_->drawing.get())
        return false;
    if (palette && (! palette->Ready() || ! palette->Matches(state_->graph.get(), target)))
        return false;
    // Contrast against a translucent surface depends on its unknown backdrop.
    // Keep theme brushes until the caller supplies an opaque surface.
    if (colors && colors->fill.a != 1.f)
        colors = nullptr;
    D2D1_MATRIX_3X2_F previous;
    target->GetTransform(&previous);
    const auto restore = wil::scope_exit([&]() noexcept { target->SetTransform(previous); });
    target->SetTransform(D2D1::Matrix3x2F::Translation(origin.x, origin.y) * previous);
    const auto& state = *state_;
    const auto& plan  = *state.geometry;
    const auto label  = [&](const FlowchartLabel& prepared, FlowchartRect bounds, ID2D1Brush* labelBrush = nullptr) noexcept
    {
        if (prepared.layout)
            target->DrawTextLayout(D2D1::Point2F(static_cast<float>(bounds.x) + prepared.origin.x, static_cast<float>(bounds.y) + prepared.origin.y),
                                   prepared.layout.get(),
                                   labelBrush ? labelBrush : text,
                                   D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    };
    if (state.graph->family == NativeDiagramFamily::Gantt)
    {
        label(state.groupLabels[0u], plan.ganttTitle);
        target->DrawLine(D2D1::Point2F(static_cast<float>(plan.ganttAxis.x), static_cast<float>(plan.ganttAxis.y)),
                         D2D1::Point2F(static_cast<float>(plan.ganttAxis.x + plan.ganttAxis.width), static_cast<float>(plan.ganttAxis.y)),
                         stroke);
        for (std::size_t index = 0u; index < plan.nodes.size(); ++index)
        {
            target->FillRectangle(Rect(plan.nodes[index].bounds), stroke);
            label(state.nodeLabels[index], plan.nodes[index].label);
        }
        return true;
    }
    if (state.graph->family == NativeDiagramFamily::Pie)
    {
        const auto originalOpacity = stroke->GetOpacity();
        const auto restoreOpacity  = wil::scope_exit([&]() noexcept { stroke->SetOpacity(originalOpacity); });
        label(state.groupLabels[0u], plan.pieTitle);
        for (std::size_t index = 0u; index < plan.nodes.size(); ++index)
        {
            const float opacity = originalOpacity * (.28f + .22f * static_cast<float>(index % 4u));
            stroke->SetOpacity(opacity);
            if (state.nodePaths[index])
                target->FillGeometry(state.nodePaths[index].get(), stroke);
            target->FillRectangle(Rect(plan.nodes[index].bounds), stroke);
            stroke->SetOpacity(originalOpacity);
            if (state.nodePaths[index])
                target->DrawGeometry(state.nodePaths[index].get(), stroke);
            target->DrawRectangle(Rect(plan.nodes[index].bounds), stroke);
            label(state.nodeLabels[index], plan.nodes[index].label);
        }
        return true;
    }
    for (const auto& group : plan.groups)
    {
        target->FillRectangle(Rect(group.bounds), fill);
        target->DrawRectangle(Rect(group.bounds), stroke);
    }
    if (state.graph->family == NativeDiagramFamily::Sequence)
        for (const auto& node : plan.nodes)
        {
            const auto x = static_cast<float>(node.bounds.x + node.bounds.width / 2.0);
            target->DrawLine(D2D1::Point2F(x, static_cast<float>(node.bounds.y + node.bounds.height)),
                             D2D1::Point2F(x, static_cast<float>(plan.extent.height - 8.0)),
                             stroke,
                             1.f,
                             state.dottedStyle.get());
        }
    for (std::size_t index = 0u; index < plan.edges.size(); ++index)
    {
        const auto& edge = plan.edges[index];
        if (edge.sequenceNote)
        {
            const auto first = Point(edge.path[0u]), last = Point(edge.path[1u]);
            const auto bounds = D2D1::RectF(first.x, first.y, last.x, last.y);
            target->FillRectangle(bounds, fill);
            target->DrawRectangle(bounds, stroke);
            continue;
        }
        if (edge.style == FlowchartLineStyle::Invisible)
            continue;
        const auto& authored = state.graph->edges[index];
        auto* edgeStroke     = stroke;
        if (palette && colors && authored.strokeColor != NoDiagramColor && Contrast(DiagramColor(authored.strokeColor), colors->fill) >= 3.0)
            edgeStroke = palette->edgeStrokes[index].get();
        const float lineWidth = authored.strokeWidth ? static_cast<float>(authored.strokeWidth) : edge.style == FlowchartLineStyle::Thick ? 3.f : 1.f;
        auto* lineStyle       = authored.dashSpecified                     ? (authored.dashed ? state.dashedStyle.get() : nullptr)
                                : edge.style == FlowchartLineStyle::Dotted ? state.dottedStyle.get()
                                                                           : nullptr;
        const bool triangle   = edge.relation == FlowchartRelation::Inheritance || edge.relation == FlowchartRelation::Realization;
        const bool diamond    = edge.relation == FlowchartRelation::Aggregation || edge.relation == FlowchartRelation::Composition;
        const bool shorten    = (triangle || diamond) && state.arrowPaths[index];
        const auto markerBase =
            triangle ? FlowchartPoint{(edge.arrow[1u].x + edge.arrow[2u].x) / 2.0, (edge.arrow[1u].y + edge.arrow[2u].y) / 2.0} : edge.diamond[2u];
        for (std::size_t point = 1u; point < edge.path.size(); ++point)
        {
            // Hollow markers shorten their own end of the connector. A
            // leading marker must not disconnect the opposite node.
            const auto start = shorten && authored.markerAtStart && point == 1u ? markerBase : edge.path[point - 1u];
            const auto end   = shorten && ! authored.markerAtStart && point + 1u == edge.path.size() ? markerBase : edge.path[point];
            target->DrawLine(Point(start), Point(end), edgeStroke, lineWidth, lineStyle);
        }
        if (state.graph->family == NativeDiagramFamily::EntityRelationship)
        {
            const auto endpoint = [&](bool last, ErMultiplicity cardinality) noexcept
            {
                const auto originPoint   = last ? edge.path.back() : edge.path.front();
                FlowchartPoint neighbour = originPoint;
                for (std::size_t step = 1u; step < edge.path.size(); ++step)
                {
                    neighbour = last ? edge.path[edge.path.size() - 1u - step] : edge.path[step];
                    if (neighbour.x != originPoint.x || neighbour.y != originPoint.y)
                        break;
                }
                const auto length = std::hypot(neighbour.x - originPoint.x, neighbour.y - originPoint.y);
                if (length <= 0.0)
                    return;
                const auto dx = (neighbour.x - originPoint.x) / length, dy = (neighbour.y - originPoint.y) / length;
                const auto p = [&](double distance, double across) noexcept
                { return Point({originPoint.x + dx * distance - dy * across, originPoint.y + dy * distance + dx * across}); };
                const auto bar      = [&](double distance) noexcept { target->DrawLine(p(distance, -4.0), p(distance, 4.0), edgeStroke); };
                const bool many     = cardinality == ErMultiplicity::OneOrMany || cardinality == ErMultiplicity::ZeroOrMany;
                const bool optional = cardinality == ErMultiplicity::ZeroOrOne || cardinality == ErMultiplicity::ZeroOrMany;
                if (many)
                {
                    target->DrawLine(p(9.0, 0.0), p(2.0, -4.0), edgeStroke);
                    target->DrawLine(p(9.0, 0.0), p(2.0, 4.0), edgeStroke);
                }
                else
                    bar(4.0);
                if (optional)
                {
                    const auto center = p(many ? 13.0 : 11.0, 0.0);
                    target->DrawEllipse(D2D1::Ellipse(center, 2.f, 2.f), edgeStroke);
                }
                else
                    bar(many ? 13.0 : 9.0);
            };
            endpoint(false, edge.erFrom);
            endpoint(true, edge.erTo);
        }
        if (edge.openArrow)
        {
            target->DrawLine(Point(edge.arrow[1u]), Point(edge.arrow[0u]), edgeStroke, lineWidth);
            target->DrawLine(Point(edge.arrow[0u]), Point(edge.arrow[2u]), edgeStroke, lineWidth);
        }
        else if (state.arrowPaths[index])
        {
            if (edge.relation == FlowchartRelation::Inheritance || edge.relation == FlowchartRelation::Realization ||
                edge.relation == FlowchartRelation::Aggregation)
                target->DrawGeometry(state.arrowPaths[index].get(), edgeStroke);
            else
                target->FillGeometry(state.arrowPaths[index].get(), edgeStroke);
        }
    }
    for (std::size_t index = 0u; index < plan.groups.size(); ++index)
    {
        target->FillRectangle(Rect(plan.groups[index].label), fill);
        label(state.groupLabels[index], plan.groups[index].label);
    }
    for (std::size_t index = 0u; index < plan.nodes.size(); ++index)
    {
        const auto& node = plan.nodes[index];
        auto* nodeStroke = stroke;
        auto* nodeFill   = fill;
        auto* nodeText   = text;
        if (palette && colors)
        {
            const auto& authoredNode   = state.graph->nodes[index];
            const auto candidateFill   = authoredNode.fillColor == NoDiagramColor ? colors->fill : DiagramColor(authoredNode.fillColor);
            const auto candidateText   = authoredNode.textColor == NoDiagramColor ? colors->text : DiagramColor(authoredNode.textColor);
            const auto candidateStroke = authoredNode.strokeColor == NoDiagramColor ? colors->stroke : DiagramColor(authoredNode.strokeColor);
            if (Contrast(candidateFill, Composite(candidateText, candidateFill)) >= 4.5 &&
                Contrast(candidateFill, Composite(candidateStroke, candidateFill)) >= 3.0)
            {
                if (authoredNode.fillColor != NoDiagramColor)
                    nodeFill = palette->fills[index].get();
                if (authoredNode.textColor != NoDiagramColor)
                    nodeText = palette->texts[index].get();
                if (authoredNode.strokeColor != NoDiagramColor)
                    nodeStroke = palette->strokes[index].get();
            }
            else if (authoredNode.strokeColor != NoDiagramColor && Contrast(DiagramColor(authoredNode.strokeColor), colors->fill) >= 3.0)
                nodeStroke = palette->strokes[index].get();
        }
        const auto bounds          = Rect(node.bounds);
        const auto nodeStrokeWidth = static_cast<float>(state.graph->nodes[index].strokeWidth);
        auto* nodeStrokeStyle      = state.graph->nodes[index].borderDashed ? state.dashedStyle.get() : nullptr;
        if (state.graph->nodes[index].sequenceActor)
        {
            const float x = (bounds.left + bounds.right) / 2.f, y = bounds.top + 4.f;
            const auto head = D2D1::Ellipse(D2D1::Point2F(x, y + 6.f), 6.f, 6.f);
            target->FillEllipse(head, nodeFill);
            target->DrawEllipse(head, nodeText);
            target->DrawLine(D2D1::Point2F(x, y + 12.f), D2D1::Point2F(x, y + 25.f), nodeText);
            target->DrawLine(D2D1::Point2F(x - 11.f, y + 17.f), D2D1::Point2F(x + 11.f, y + 17.f), nodeText);
            target->DrawLine(D2D1::Point2F(x, y + 25.f), D2D1::Point2F(x - 10.f, y + 34.f), nodeText);
            target->DrawLine(D2D1::Point2F(x, y + 25.f), D2D1::Point2F(x + 10.f, y + 34.f), nodeText);
        }
        else if (node.shape == FlowchartShape::Bar)
        {
            target->FillRectangle(bounds, nodeStroke);
        }
        else if (node.shape == FlowchartShape::Diamond || node.shape == FlowchartShape::Hexagon ||
                 (node.shape >= FlowchartShape::Asymmetric && node.shape <= FlowchartShape::ReverseTrapezoid))
        {
            target->FillGeometry(state.nodePaths[index].get(), nodeFill);
            target->DrawGeometry(state.nodePaths[index].get(), nodeStroke, nodeStrokeWidth, nodeStrokeStyle);
        }
        else if (node.shape == FlowchartShape::Circle || node.shape == FlowchartShape::DoubleCircle || node.shape == FlowchartShape::Ellipse)
        {
            const auto ellipse = D2D1::Ellipse(D2D1::Point2F((bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f),
                                               (bounds.right - bounds.left) / 2.0f,
                                               (bounds.bottom - bounds.top) / 2.0f);
            target->FillEllipse(ellipse, nodeFill);
            target->DrawEllipse(ellipse, nodeStroke, nodeStrokeWidth, nodeStrokeStyle);
            if (node.shape == FlowchartShape::DoubleCircle)
                target->DrawEllipse(D2D1::Ellipse(ellipse.point, ellipse.radiusX - 4.f, ellipse.radiusY - 4.f), nodeStroke, nodeStrokeWidth, nodeStrokeStyle);
        }
        else if (node.shape == FlowchartShape::Cylinder)
        {
            target->FillGeometry(state.nodePaths[index].get(), nodeFill);
            target->DrawGeometry(state.nodePaths[index].get(), nodeStroke, nodeStrokeWidth, nodeStrokeStyle);
            target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F((bounds.left + bounds.right) / 2.f, bounds.top + 8.f), (bounds.right - bounds.left) / 2.f, 8.f),
                                nodeStroke,
                                nodeStrokeWidth,
                                nodeStrokeStyle);
        }
        else if (node.shape == FlowchartShape::Rounded)
        {
            const auto rounded = D2D1::RoundedRect(bounds, 8.0f, 8.0f);
            target->FillRoundedRectangle(rounded, nodeFill);
            target->DrawRoundedRectangle(rounded, nodeStroke, nodeStrokeWidth, nodeStrokeStyle);
        }
        else if (node.shape == FlowchartShape::Stadium)
        {
            const auto radius  = (bounds.bottom - bounds.top) / 2.0f;
            const auto stadium = D2D1::RoundedRect(bounds, radius, radius);
            target->FillRoundedRectangle(stadium, nodeFill);
            target->DrawRoundedRectangle(stadium, nodeStroke, nodeStrokeWidth, nodeStrokeStyle);
        }
        else if (node.shape == FlowchartShape::Subroutine)
        {
            target->FillRectangle(bounds, nodeFill);
            target->DrawRectangle(bounds, nodeStroke, nodeStrokeWidth, nodeStrokeStyle);
            target->DrawLine(D2D1::Point2F(bounds.left + 8.f, bounds.top), D2D1::Point2F(bounds.left + 8.f, bounds.bottom), nodeStroke);
            target->DrawLine(D2D1::Point2F(bounds.right - 8.f, bounds.top), D2D1::Point2F(bounds.right - 8.f, bounds.bottom), nodeStroke);
        }
        else if (node.shape == FlowchartShape::Note)
        {
            target->FillRectangle(bounds, nodeFill);
            target->DrawRectangle(bounds, nodeStroke, nodeStrokeWidth, nodeStrokeStyle);
            target->DrawLine(D2D1::Point2F(bounds.right - 8.f, bounds.top), D2D1::Point2F(bounds.right - 8.f, bounds.top + 8.f), nodeStroke);
            target->DrawLine(D2D1::Point2F(bounds.right - 8.f, bounds.top + 8.f), D2D1::Point2F(bounds.right, bounds.top + 8.f), nodeStroke);
            target->DrawLine(D2D1::Point2F(bounds.right - 8.f, bounds.top), D2D1::Point2F(bounds.right, bounds.top + 8.f), nodeStroke);
        }
        else
        {
            target->FillRectangle(bounds, nodeFill);
            target->DrawRectangle(bounds, nodeStroke, nodeStrokeWidth, nodeStrokeStyle);
        }
        const auto& nodeLabel = state.nodeLabels[index];
        for (std::size_t compartment = 0u; compartment < nodeLabel.compartmentCount; ++compartment)
        {
            const auto y = static_cast<float>(node.label.y) + nodeLabel.compartmentY[compartment];
            target->DrawLine(D2D1::Point2F(bounds.left, y), D2D1::Point2F(bounds.right, y), nodeStroke);
        }
        label(nodeLabel, node.label, nodeText);
    }
    // Exterior corridors can cross other edges' label rectangles. Paint every
    // label last over its opaque backing, so no later route can obscure ink.
    for (std::size_t index = 0u; index < plan.edges.size(); ++index)
    {
        const auto drawCardinality = [&](const FlowchartLabel& prepared, FlowchartRect bounds) noexcept
        {
            if (! prepared.layout)
                return;
            target->FillRectangle(Rect(bounds), labelFill && ! prepared.onNodeSurface ? labelFill : fill);
            label(prepared, bounds);
        };
        if (state.hasCardinality)
        {
            drawCardinality(state.fromCardinalities[index], plan.edges[index].fromCardinality);
            drawCardinality(state.toCardinalities[index], plan.edges[index].toCardinality);
        }
        if (plan.edges[index].hasLabel)
        {
            auto background = Rect(plan.edges[index].label);
            background.left -= 2.0f;
            background.top -= 2.0f;
            background.right += 2.0f;
            background.bottom += 2.0f;
            target->FillRectangle(background, labelFill && ! state.edgeLabels[index].onNodeSurface ? labelFill : fill);
            label(state.edgeLabels[index], plan.edges[index].label);
        }
    }
    return true;
}
} // namespace DxUi::RichText::Detail
