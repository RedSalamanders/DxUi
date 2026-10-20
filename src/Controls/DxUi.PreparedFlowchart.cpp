#include "DxUi.PreparedFlowchart.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <wil/resource.h>
#include <wil/result.h>

namespace DxUi::RichText::Detail
{
namespace
{
struct FlowchartLabel
{
    wil::com_ptr<IDWriteTextLayout> layout;
    D2D1_POINT_2F origin{};
};
bool SameTypography(const LayoutOptions& a, const LayoutOptions& b) noexcept
{
    return a.fontFamily == b.fontFamily && a.fontSizeDip == b.fontSizeDip && a.locale == b.locale && a.readingDirection == b.readingDirection;
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
wil::com_ptr<ID2D1PathGeometry> Path(ID2D1Factory* factory, std::span<const FlowchartPoint> points)
{
    wil::com_ptr<ID2D1PathGeometry> path;
    THROW_IF_FAILED(factory->CreatePathGeometry(path.put()));
    wil::com_ptr<ID2D1GeometrySink> sink;
    THROW_IF_FAILED(path->Open(sink.put()));
    sink->BeginFigure(Point(points.front()), D2D1_FIGURE_BEGIN_FILLED);
    for (std::size_t index = 1u; index < points.size(); ++index)
        sink->AddLine(Point(points[index]));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    THROW_IF_FAILED(sink->Close());
    return path;
}
} // namespace
struct PreparedFlowchart::State
{
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
          nodeLabels(graph->nodes.size()),
          edgeLabels(graph->edges.size()),
          nodeSizes(graph->nodes.size()),
          edgeSizes(graph->edges.size()),
          nodePaths(graph->nodes.size()),
          arrowPaths(graph->edges.size())
    {
    }
    std::shared_ptr<const FlowchartModel> graph;
    wil::com_ptr<IDWriteFactory> write;
    wil::com_ptr<ID2D1Factory> drawing;
    LayoutOptions options;
    FlowchartResourceEpoch epoch{};
    wil::com_ptr<IDWriteTextFormat> format;
    std::vector<FlowchartLabel> nodeLabels, edgeLabels;
    std::vector<FlowchartSize> nodeSizes, edgeSizes;
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
    void Format()
    {
        THROW_IF_FAILED(write->CreateTextFormat(options.fontFamily.c_str(),
                                                nullptr,
                                                DWRITE_FONT_WEIGHT_NORMAL,
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL,
                                                options.fontSizeDip,
                                                options.locale.c_str(),
                                                format.put()));
        THROW_IF_FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
        THROW_IF_FAILED(format->SetReadingDirection(options.readingDirection));
        THROW_IF_FAILED(format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
        THROW_IF_FAILED(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    }
    void Label(std::wstring_view text, FlowchartLabel& output, FlowchartSize& size)
    {
        wil::com_ptr<IDWriteTextLayout> layout;
        constexpr auto maximum = static_cast<float>(NativeFlowchartLayout::MaximumExtent);
        THROW_IF_FAILED(write->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format.get(), maximum, maximum, layout.put()));
        DWRITE_TEXT_METRICS metrics{};
        DWRITE_OVERHANG_METRICS ink{};
        THROW_IF_FAILED(layout->GetMetrics(&metrics));
        THROW_IF_FAILED(layout->GetOverhangMetrics(&ink));
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
                throw std::overflow_error("Nonfinite native flowchart ink metrics");
        // Logical whitespace/line extents and actual glyph ink both belong to
        // the measured label. RTL leading alignment may put ink near the far
        // layout edge; subtract its actual origin rather than rotating text.
        const auto left = std::min(metrics.left, -ink.left), top = std::min(metrics.top, -ink.top);
        const auto right  = std::max(metrics.left + metrics.widthIncludingTrailingWhitespace, metrics.layoutWidth + ink.right);
        const auto bottom = std::max(metrics.top + metrics.height, metrics.layoutHeight + ink.bottom);
        const auto width = static_cast<double>(right) - left, height = static_cast<double>(bottom) - top;
        if (! std::isfinite(width) || ! std::isfinite(height) || width < 0.0 || height < 0.0)
            throw std::overflow_error("Nonfinite native flowchart label metrics");
        // Large ink becomes an explicit capacity result; measured values are
        // never clamped into a seemingly renderable, clipped diagram.
        labelCapacity = labelCapacity || width > NativeFlowchartLayout::MaximumExtent || height > NativeFlowchartLayout::MaximumExtent;
        size          = {width, height};
        output.origin = D2D1::Point2F(-left, -top);
        output.layout = std::move(layout);
    }
};
struct PreparedFlowchart::Preparation
{
    enum class Stage
    {
        Format,
        Labels,
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
PreparedFlowchart::PreparedFlowchart() noexcept  = default;
PreparedFlowchart::~PreparedFlowchart() noexcept = default;
LayoutPreparationStatus PreparedFlowchart::Begin(IDWriteFactory* write,
                                                 ID2D1Factory* drawing,
                                                 std::shared_ptr<const FlowchartModel> graph,
                                                 const LayoutOptions& typography,
                                                 FlowchartResourceEpoch resourceEpoch)
{
    if (! write || ! drawing || ! graph || graph->outcome != FlowchartOutcome::Prepared || graph->nodes.size() > NativeFlowchartParser::MaximumNodes ||
        graph->edges.size() > NativeFlowchartParser::MaximumEdges || graph->source.size() > NativeFlowchartParser::MaximumSourceUnits ||
        ! std::isfinite(typography.fontSizeDip) || typography.fontSizeDip <= 0.0f || typography.fontSizeDip > 512.0f || typography.fontFamily.size() > 1024u ||
        typography.locale.size() > 1024u ||
        (typography.readingDirection != DWRITE_READING_DIRECTION_LEFT_TO_RIGHT && typography.readingDirection != DWRITE_READING_DIRECTION_RIGHT_TO_LEFT))
        throw std::invalid_argument("Invalid native flowchart preparation admission");
    if (graph->direction < FlowchartDirection::TopBottom || graph->direction > FlowchartDirection::RightLeft)
        throw std::invalid_argument("Invalid native flowchart direction");
    for (const auto& node : graph->nodes)
        if (node.label.size() > NativeFlowchartParser::MaximumLabelUnits || node.shape < FlowchartShape::Rectangle || node.shape > FlowchartShape::Diamond)
            throw std::invalid_argument("Invalid flowchart node label/shape");
    for (const auto& edge : graph->edges)
        if (edge.label.size() > NativeFlowchartParser::MaximumLabelUnits || edge.from >= graph->nodes.size() || edge.to >= graph->nodes.size())
            throw std::invalid_argument("Invalid native flowchart edge");
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
    LayoutAdvance result;
    if (! next_)
        return result;
    result.status = LayoutPreparationStatus::Pending;
    if (! budget.maximumLayouts || ! budget.maximumBlockSteps)
        return result;
    try
    {
        while (result.blockSteps < budget.maximumBlockSteps)
        {
            auto& work  = *next_;
            auto& state = *work.state;
            switch (work.stage)
            {
                case Preparation::Stage::Format:
                    state.Format();
                    work.stage = Preparation::Stage::Labels;
                    break;
                case Preparation::Stage::Labels:
                {
                    const auto nodes = state.graph->nodes.size();
                    if (work.cursor == nodes + state.graph->edges.size())
                    {
                        work.stage = Preparation::Stage::BeginGeometry;
                        break;
                    }
                    const auto node   = work.cursor < nodes;
                    const auto index  = node ? work.cursor : work.cursor - nodes;
                    const auto& label = node ? state.graph->nodes[index].label : state.graph->edges[index].label;
                    if (node || ! label.empty())
                    {
                        if (result.layoutsPrepared == budget.maximumLayouts)
                            return result;
                        ++result.layoutsPrepared;
                        state.Label(label, node ? state.nodeLabels[index] : state.edgeLabels[index], node ? state.nodeSizes[index] : state.edgeSizes[index]);
                    }
                    ++work.cursor;
                    break;
                }
                case Preparation::Stage::BeginGeometry:
                    if (state.labelCapacity)
                    {
                        auto rejected   = std::make_shared<FlowchartLayoutPlan>();
                        rejected->issue = FlowchartLayoutIssue::DiagramExtent;
                        state.geometry  = std::move(rejected);
                        work.stage      = Preparation::Stage::Publish;
                        break;
                    }
                    work.layout.Begin(state.graph, state.nodeSizes, state.edgeSizes);
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
                    if (state.geometry->nodes[work.cursor].shape == FlowchartShape::Diamond)
                    {
                        const auto rect                             = state.geometry->nodes[work.cursor].bounds;
                        const std::array<FlowchartPoint, 4u> points = {{{rect.x + rect.width / 2.0, rect.y},
                                                                        {rect.x + rect.width, rect.y + rect.height / 2.0},
                                                                        {rect.x + rect.width / 2.0, rect.y + rect.height},
                                                                        {rect.x, rect.y + rect.height / 2.0}}};
                        state.nodePaths[work.cursor]                = Path(state.drawing.get(), points);
                    }
                    ++work.cursor;
                    break;
                case Preparation::Stage::ArrowPaths:
                    if (work.cursor == state.geometry->edges.size())
                    {
                        work.stage = Preparation::Stage::Publish;
                        break;
                    }
                    if (state.geometry->edges[work.cursor].hasArrow)
                        state.arrowPaths[work.cursor] = Path(state.drawing.get(), state.geometry->edges[work.cursor].arrow);
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
bool PreparedFlowchart::Draw(ID2D1RenderTarget* target, ID2D1Brush* text, ID2D1Brush* stroke, ID2D1Brush* fill, D2D1_POINT_2F origin) const noexcept
{
    if (! Ready() || ! target || ! text || ! stroke || ! fill || ! std::isfinite(origin.x) || ! std::isfinite(origin.y))
        return false;
    wil::com_ptr<ID2D1Factory> drawing;
    target->GetFactory(drawing.put());
    if (drawing.get() != state_->drawing.get())
        return false;
    D2D1_MATRIX_3X2_F previous;
    target->GetTransform(&previous);
    const auto restore = wil::scope_exit([&]() noexcept { target->SetTransform(previous); });
    target->SetTransform(D2D1::Matrix3x2F::Translation(origin.x, origin.y) * previous);
    const auto& state = *state_;
    const auto& plan  = *state.geometry;
    const auto label  = [&](const FlowchartLabel& prepared, FlowchartRect bounds) noexcept
    {
        if (prepared.layout)
            target->DrawTextLayout(D2D1::Point2F(static_cast<float>(bounds.x) + prepared.origin.x, static_cast<float>(bounds.y) + prepared.origin.y),
                                   prepared.layout.get(),
                                   text,
                                   D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    };
    for (std::size_t index = 0u; index < plan.edges.size(); ++index)
    {
        const auto& edge = plan.edges[index];
        for (std::size_t point = 1u; point < edge.path.size(); ++point)
            target->DrawLine(Point(edge.path[point - 1u]), Point(edge.path[point]), stroke);
        if (state.arrowPaths[index])
            target->FillGeometry(state.arrowPaths[index].get(), stroke);
    }
    for (std::size_t index = 0u; index < plan.nodes.size(); ++index)
    {
        const auto& node  = plan.nodes[index];
        const auto bounds = Rect(node.bounds);
        if (node.shape == FlowchartShape::Diamond)
        {
            target->FillGeometry(state.nodePaths[index].get(), fill);
            target->DrawGeometry(state.nodePaths[index].get(), stroke);
        }
        else if (node.shape == FlowchartShape::Circle)
        {
            const auto ellipse = D2D1::Ellipse(D2D1::Point2F((bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f),
                                               (bounds.right - bounds.left) / 2.0f,
                                               (bounds.bottom - bounds.top) / 2.0f);
            target->FillEllipse(ellipse, fill);
            target->DrawEllipse(ellipse, stroke);
        }
        else if (node.shape == FlowchartShape::Rounded)
        {
            const auto rounded = D2D1::RoundedRect(bounds, 8.0f, 8.0f);
            target->FillRoundedRectangle(rounded, fill);
            target->DrawRoundedRectangle(rounded, stroke);
        }
        else
        {
            target->FillRectangle(bounds, fill);
            target->DrawRectangle(bounds, stroke);
        }
        label(state.nodeLabels[index], node.label);
    }
    // Exterior corridors can cross other edges' label rectangles. Paint every
    // label last over its opaque backing, so no later route can obscure ink.
    for (std::size_t index = 0u; index < plan.edges.size(); ++index)
        if (plan.edges[index].hasLabel)
        {
            auto background = Rect(plan.edges[index].label);
            background.left -= 2.0f;
            background.top -= 2.0f;
            background.right += 2.0f;
            background.bottom += 2.0f;
            target->FillRectangle(background, fill);
            label(state.edgeLabels[index], plan.edges[index].label);
        }
    return true;
}
} // namespace DxUi::RichText::Detail
