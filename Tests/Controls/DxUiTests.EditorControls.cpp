#include "DxUiTestHelpers.h"

#include "../../include/DxUi/ControlCatalog.h"

#include <limits>
#include <memory>
#include <vector>

// Editor consumer controls: Splitter, NumericStepper, ColorPicker. See UI_ControlsAndLayout.md.

namespace
{
using namespace DxUi;

struct SplitterEvents
{
    std::vector<SplitterChangePhase> phases;
    float lastPosition = -1.0f;

    [[nodiscard]] size_t Count(SplitterChangePhase phase) const noexcept
    {
        size_t count = 0u;
        for (const SplitterChangePhase seen : phases)
        {
            if (seen == phase)
            {
                ++count;
            }
        }
        return count;
    }
};

struct StepperEvents
{
    std::vector<NumericStepperChangePhase> phases;
    double lastValue = -12345.0;

    [[nodiscard]] size_t Count(NumericStepperChangePhase phase) const noexcept
    {
        size_t count = 0u;
        for (const NumericStepperChangePhase seen : phases)
        {
            if (seen == phase)
            {
                ++count;
            }
        }
        return count;
    }
};

struct PickerEvents
{
    std::vector<ColorPickerChangePhase> phases;
    uint32_t lastArgb = 0u;

    [[nodiscard]] size_t Count(ColorPickerChangePhase phase) const noexcept
    {
        size_t count = 0u;
        for (const ColorPickerChangePhase seen : phases)
        {
            if (seen == phase)
            {
                ++count;
            }
        }
        return count;
    }
};

[[nodiscard]] D2D1_POINT_2F Center(const D2D1_RECT_F& rect) noexcept
{
    return D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f);
}

[[nodiscard]] bool RectNear(const D2D1_RECT_F& actual, const D2D1_RECT_F& expected) noexcept
{
    constexpr float epsilon = 0.01f;
    return std::fabs(actual.left - expected.left) < epsilon && std::fabs(actual.top - expected.top) < epsilon &&
           std::fabs(actual.right - expected.right) < epsilon && std::fabs(actual.bottom - expected.bottom) < epsilon;
}

void Click(WindowHost& host, Button& button, const char* message)
{
    const D2D1_POINT_2F point = Center(button.GetBounds());
    Require(button.OnMouseDown(host, point, false, 0), message);
    Require(button.OnMouseUp(host, point, false, 0), message);
}

void TypeText(WindowHost& host, TextField& field, std::wstring_view text)
{
    host.SetFocusControl(&field);
    field.SetText({});
    for (const wchar_t ch : text)
    {
        static_cast<void>(field.OnChar(host, ch, 0));
    }
}

// ── Splitter ─────────────────────────────────────────────────────────────

void TestSplitterDefaultState()
{
    Splitter splitter;
    Require(splitter.GetOrientation() == SplitterOrientation::Vertical, "splitter defaults to a vertical separator");
    Require(splitter.GetThickness() == Splitter::kDefaultThicknessDip, "splitter defaults to the 6 DIP thickness");
    Require(splitter.GetMinimumFirstPane() == Splitter::kDefaultMinimumPaneDip, "splitter defaults the first pane minimum");
    Require(splitter.GetMinimumSecondPane() == Splitter::kDefaultMinimumPaneDip, "splitter defaults the second pane minimum");
    Require(splitter.GetPosition() == Splitter::kDefaultMinimumPaneDip, "splitter starts at the first pane minimum");
    Require(! splitter.IsDragging(), "splitter is not dragging initially");
}

void TestSplitterClampsToPaneMinimums()
{
    Splitter splitter;
    splitter.SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 100.0f));
    splitter.SetPosition(10.0f);
    RequireFloatNear(splitter.GetPosition(), 48.0f, 0.001f, "splitter clamps below the first pane minimum");
    splitter.SetPosition(1000.0f);
    RequireFloatNear(splitter.GetPosition(), 246.0f, 0.001f, "splitter clamps to extent minus thickness minus the second minimum");
    Require(RectNear(splitter.GetSeparatorBounds(), D2D1::RectF(246.0f, 0.0f, 252.0f, 100.0f)), "separator bounds follow the position");
    Require(RectNear(splitter.GetFirstPaneBounds(), D2D1::RectF(0.0f, 0.0f, 246.0f, 100.0f)), "first pane ends at the separator");
    Require(RectNear(splitter.GetSecondPaneBounds(), D2D1::RectF(252.0f, 0.0f, 300.0f, 100.0f)), "second pane starts after the separator");
    splitter.SetPosition(std::numeric_limits<float>::quiet_NaN());
    RequireFloatNear(splitter.GetPosition(), 246.0f, 0.001f, "splitter ignores a non-finite position");

    splitter.SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 100.0f));
    RequireFloatNear(splitter.GetPosition(), 146.0f, 0.001f, "shrinking the bounds re-clamps the position");

    Splitter persisted;
    persisted.SetPosition(200.0f);
    RequireFloatNear(persisted.GetPosition(), 200.0f, 0.001f, "a position set before layout keeps its value");
    persisted.SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 100.0f));
    RequireFloatNear(persisted.GetPosition(), 200.0f, 0.001f, "the persisted position survives the first layout");
    persisted.SetMinimumSecondPane(80.0f);
    RequireFloatNear(persisted.GetPosition(), 200.0f, 0.001f, "a larger second minimum keeps a valid position");
    persisted.SetMinimumSecondPane(120.0f);
    RequireFloatNear(persisted.GetPosition(), 174.0f, 0.001f, "a larger second minimum re-clamps the position");
}

void TestSplitterHitBoundsCoverOnlySeparator()
{
    Splitter splitter;
    splitter.SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 100.0f));
    splitter.SetPosition(100.0f);
    Require(RectNear(splitter.GetHitBounds(), D2D1::RectF(98.0f, 0.0f, 108.0f, 100.0f)), "hit bounds are the separator plus the slop");
    Require(! PointInRect(splitter.GetHitBounds(), D2D1::Point2F(50.0f, 50.0f)), "the first pane area is not hittable");
    Require(! PointInRect(splitter.GetHitBounds(), D2D1::Point2F(200.0f, 50.0f)), "the second pane area is not hittable");
    Require(PointInRect(splitter.GetHitBounds(), D2D1::Point2F(103.0f, 50.0f)), "the separator is hittable");
    Require(PointInRect(splitter.GetHitBounds(), D2D1::Point2F(99.0f, 50.0f)), "the slop beside the separator is hittable");
    splitter.SetBounds(D2D1::RectF());
    Require(RectNear(splitter.GetHitBounds(), D2D1::RectF()), "a zero-extent splitter reports empty hit bounds");
}

void TestSplitterDragPreviewsCommitsAndCancels()
{
    WindowHost host;
    auto root      = std::make_unique<Panel>();
    auto* splitter = root->AddChild<Splitter>();
    splitter->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 100.0f));
    splitter->SetPosition(100.0f);
    SplitterEvents events;
    splitter->SetOnChange([&](SplitterChange change)
    {
        events.phases.push_back(change.phase);
        events.lastPosition = change.position;
    });
    host.SetRoot(std::move(root));

    Require(! splitter->OnMouseDown(host, D2D1::Point2F(50.0f, 50.0f), false, 0), "a press outside the separator is ignored");
    Require(splitter->OnMouseDown(host, D2D1::Point2F(103.0f, 50.0f), false, 0), "a press on the separator starts a drag");
    Require(splitter->IsDragging(), "splitter reports the drag");
    Require(host.GetFocusControl() == splitter, "the drag focuses the splitter");
    Require(splitter->OnMouseMove(host, D2D1::Point2F(153.0f, 50.0f), 0), "moves during the drag are handled");
    RequireFloatNear(splitter->GetPosition(), 150.0f, 0.001f, "the drag keeps the pointer offset within the separator");
    Require(events.Count(SplitterChangePhase::Preview) == 1u && events.lastPosition == 150.0f, "a drag move previews");
    Require(splitter->OnMouseMove(host, D2D1::Point2F(153.0f, 60.0f), 0), "a move along the separator is still handled");
    Require(events.Count(SplitterChangePhase::Preview) == 1u, "an unchanged position does not preview again");
    Require(splitter->OnMouseUp(host, D2D1::Point2F(153.0f, 50.0f), false, 0), "release ends the drag");
    Require(! splitter->IsDragging(), "release clears the drag");
    Require(events.Count(SplitterChangePhase::Commit) == 1u && events.lastPosition == 150.0f, "release commits once");

    Require(splitter->OnMouseDown(host, D2D1::Point2F(153.0f, 50.0f), false, 0), "a second drag starts");
    Require(splitter->OnMouseMove(host, D2D1::Point2F(203.0f, 50.0f), 0), "the second drag moves");
    RequireFloatNear(splitter->GetPosition(), 200.0f, 0.001f, "the second drag previews the new position");
    splitter->OnCaptureLost(host);
    Require(! splitter->IsDragging(), "capture loss ends the drag");
    RequireFloatNear(splitter->GetPosition(), 150.0f, 0.001f, "capture loss restores the drag-start position");
    Require(events.Count(SplitterChangePhase::Cancel) == 1u && events.lastPosition == 150.0f, "capture loss notifies cancel");

    Require(splitter->OnMouseDown(host, D2D1::Point2F(153.0f, 50.0f), false, 0), "a third drag starts");
    Require(splitter->OnMouseMove(host, D2D1::Point2F(60.0f, 50.0f), 0), "the third drag moves toward the minimum");
    RequireFloatNear(splitter->GetPosition(), 57.0f, 0.001f, "the third drag previews");
    Require(splitter->OnKeyDown(host, VK_ESCAPE, 0), "Escape during a drag is handled");
    Require(! splitter->IsDragging(), "Escape ends the drag");
    RequireFloatNear(splitter->GetPosition(), 150.0f, 0.001f, "Escape restores the drag-start position");
    Require(events.Count(SplitterChangePhase::Cancel) == 2u, "Escape notifies cancel");
    Require(events.Count(SplitterChangePhase::Commit) == 1u, "cancelled drags never commit");

    Require(! splitter->OnMouseDown(host, D2D1::Point2F(153.0f, 50.0f), true, 0), "a right press does not start a drag");
}

void TestSplitterKeyboardStepsAndCommits()
{
    WindowHost host;
    auto root      = std::make_unique<Panel>();
    auto* splitter = root->AddChild<Splitter>();
    splitter->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 100.0f));
    splitter->SetPosition(100.0f);
    SplitterEvents events;
    splitter->SetOnChange([&](SplitterChange change)
    {
        events.phases.push_back(change.phase);
        events.lastPosition = change.position;
    });
    host.SetRoot(std::move(root));
    host.SetFocusControl(splitter);

    Require(splitter->OnKeyDown(host, VK_RIGHT, 0), "Right is handled by a vertical separator");
    RequireFloatNear(splitter->GetPosition(), 108.0f, 0.001f, "Right steps by the keyboard step");
    Require(events.Count(SplitterChangePhase::Commit) == 1u && events.lastPosition == 108.0f, "a keyboard step commits");
    Require(splitter->OnKeyDown(host, VK_RIGHT, MK_SHIFT), "Shift+Right is handled");
    RequireFloatNear(splitter->GetPosition(), 140.0f, 0.001f, "Shift+Right steps by the large step");
    Require(splitter->OnKeyDown(host, VK_LEFT, 0), "Left is handled");
    RequireFloatNear(splitter->GetPosition(), 132.0f, 0.001f, "Left steps back");
    Require(splitter->OnKeyDown(host, VK_HOME, 0), "Home is handled");
    RequireFloatNear(splitter->GetPosition(), 48.0f, 0.001f, "Home goes to the first pane minimum");
    Require(splitter->OnKeyDown(host, VK_LEFT, 0), "Left at the minimum is still handled");
    Require(events.Count(SplitterChangePhase::Commit) == 4u, "an unchanged position does not commit again");
    Require(splitter->OnKeyDown(host, VK_END, 0), "End is handled");
    RequireFloatNear(splitter->GetPosition(), 246.0f, 0.001f, "End goes to the far limit");
    Require(! splitter->OnKeyDown(host, VK_UP, 0), "Up is not handled by a vertical separator");
    Require(! splitter->OnKeyDown(host, VK_DOWN, 0), "Down is not handled by a vertical separator");
    Require(! splitter->OnKeyDown(host, VK_ESCAPE, 0), "Escape outside a drag is not handled");
    Require(events.Count(SplitterChangePhase::Preview) == 0u, "keyboard steps never preview");

    splitter->SetFlowDirection(FlowDirection::RightToLeft);
    splitter->SetPosition(100.0f);
    Require(RectNear(splitter->GetSeparatorBounds(), D2D1::RectF(194.0f, 0.0f, 200.0f, 100.0f)), "right-to-left measures the position from the right edge");
    Require(RectNear(splitter->GetFirstPaneBounds(), D2D1::RectF(200.0f, 0.0f, 300.0f, 100.0f)), "right-to-left places the first pane on the right");
    Require(splitter->OnKeyDown(host, VK_RIGHT, 0), "Right is handled in right-to-left flow");
    RequireFloatNear(splitter->GetPosition(), 92.0f, 0.001f, "Right shrinks the leading pane in right-to-left flow");
    Require(splitter->OnMouseDown(host, D2D1::Point2F(205.0f, 50.0f), false, 0), "the mirrored separator is hittable");
    Require(splitter->OnMouseMove(host, D2D1::Point2F(185.0f, 50.0f), 0), "the mirrored drag moves");
    RequireFloatNear(splitter->GetPosition(), 112.0f, 0.001f, "dragging left grows the leading pane in right-to-left flow");
    Require(splitter->OnMouseUp(host, D2D1::Point2F(185.0f, 50.0f), false, 0), "the mirrored drag commits");
}

void TestSplitterHorizontalOrientationAndCursors()
{
    WindowHost host;
    auto root = std::make_unique<Panel>();
    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 400.0f, 300.0f));
    auto* vertical = root->AddChild<Splitter>();
    vertical->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 100.0f));
    vertical->SetPosition(100.0f);
    auto* horizontal = root->AddChild<Splitter>();
    horizontal->SetOrientation(SplitterOrientation::Horizontal);
    horizontal->SetBounds(D2D1::RectF(0.0f, 100.0f, 100.0f, 400.0f));
    horizontal->SetPosition(100.0f);
    SplitterEvents events;
    horizontal->SetOnChange([&](SplitterChange change)
    {
        events.phases.push_back(change.phase);
        events.lastPosition = change.position;
    });
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 400.0f, 400.0f));

    Require(horizontal->GetOrientation() == SplitterOrientation::Horizontal, "orientation round-trips");
    Require(RectNear(horizontal->GetSeparatorBounds(), D2D1::RectF(0.0f, 200.0f, 100.0f, 206.0f)), "a horizontal separator spans the width");
    Require(RectNear(horizontal->GetFirstPaneBounds(), D2D1::RectF(0.0f, 100.0f, 100.0f, 200.0f)), "the first pane is above");
    Require(RectNear(horizontal->GetSecondPaneBounds(), D2D1::RectF(0.0f, 206.0f, 100.0f, 400.0f)), "the second pane is below");

    host.SetFocusControl(horizontal);
    Require(horizontal->OnKeyDown(host, VK_DOWN, 0), "Down is handled by a horizontal separator");
    RequireFloatNear(horizontal->GetPosition(), 108.0f, 0.001f, "Down moves the separator down");
    Require(horizontal->OnKeyDown(host, VK_UP, MK_SHIFT), "Shift+Up is handled");
    RequireFloatNear(horizontal->GetPosition(), 76.0f, 0.001f, "Shift+Up moves by the large step");
    Require(! horizontal->OnKeyDown(host, VK_LEFT, 0), "Left is not handled by a horizontal separator");
    Require(events.Count(SplitterChangePhase::Commit) == 2u, "horizontal keyboard steps commit");

    Require(horizontal->OnMouseDown(host, D2D1::Point2F(50.0f, 179.0f), false, 0), "a press on the horizontal separator starts a drag");
    Require(horizontal->OnMouseMove(host, D2D1::Point2F(50.0f, 229.0f), 0), "the vertical drag moves");
    RequireFloatNear(horizontal->GetPosition(), 126.0f, 0.001f, "the vertical drag tracks the pointer");
    Require(horizontal->OnMouseUp(host, D2D1::Point2F(50.0f, 229.0f), false, 0), "the vertical drag commits");

    Require(host.DebugResolveCursorKindForPoint(D2D1::Point2F(50.0f, 50.0f)) == WindowHostCursorKind::Default, "pane areas use the default cursor");
    Require(host.DebugResolveCursorKindForPoint(D2D1::Point2F(103.0f, 50.0f)) == WindowHostCursorKind::HorizontalResize,
            "a vertical separator requests the horizontal resize cursor");
    Require(host.DebugResolveCursorKindForPoint(D2D1::Point2F(50.0f, 229.0f)) == WindowHostCursorKind::VerticalResize,
            "a horizontal separator requests the vertical resize cursor");
    Require(vertical->OnMouseDown(host, D2D1::Point2F(103.0f, 50.0f), false, 0), "the vertical separator starts a drag");
    host.CaptureMouse(vertical);
    Require(host.DebugResolveCursorKindForPoint(D2D1::Point2F(250.0f, 50.0f)) == WindowHostCursorKind::HorizontalResize,
            "an active drag keeps the resize cursor away from the separator");
    Require(vertical->OnMouseUp(host, D2D1::Point2F(103.0f, 50.0f), false, 0), "the vertical separator drag ends");
}

void TestSplitterDisabledAndRequestPosition()
{
    WindowHost host;
    auto root      = std::make_unique<Panel>();
    auto* splitter = root->AddChild<Splitter>();
    splitter->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 100.0f));
    splitter->SetPosition(100.0f);
    SplitterEvents events;
    splitter->SetOnChange([&](SplitterChange change)
    {
        events.phases.push_back(change.phase);
        events.lastPosition = change.position;
    });
    host.SetRoot(std::move(root));

    Require(splitter->RequestPosition(host, 120.0f), "RequestPosition accepts a finite position");
    RequireFloatNear(splitter->GetPosition(), 120.0f, 0.001f, "RequestPosition applies the position");
    Require(events.Count(SplitterChangePhase::Commit) == 1u && events.lastPosition == 120.0f, "RequestPosition commits once");
    Require(splitter->RequestPosition(host, 120.0f), "RequestPosition accepts an unchanged position");
    Require(events.phases.size() == 1u, "an unchanged RequestPosition does not notify");
    Require(! splitter->RequestPosition(host, std::numeric_limits<float>::infinity()), "RequestPosition rejects a non-finite position");
    Require(splitter->RequestPosition(host, 5000.0f), "RequestPosition clamps an out-of-range position");
    RequireFloatNear(splitter->GetPosition(), 246.0f, 0.001f, "RequestPosition clamps to the limit");
    Require(events.lastPosition == 246.0f, "RequestPosition reports the clamped position");

    Require(splitter->OnMouseDown(host, Center(splitter->GetSeparatorBounds()), false, 0), "a drag starts");
    Require(! splitter->RequestPosition(host, 100.0f), "RequestPosition is refused while dragging");
    Require(splitter->OnMouseUp(host, Center(splitter->GetSeparatorBounds()), false, 0), "the drag ends");

    splitter->SetEnabled(false);
    Require(! splitter->OnMouseDown(host, Center(splitter->GetSeparatorBounds()), false, 0), "a disabled splitter ignores presses");
    Require(! splitter->OnKeyDown(host, VK_RIGHT, 0), "a disabled splitter ignores keys");
    Require(! splitter->RequestPosition(host, 100.0f), "a disabled splitter refuses RequestPosition");
    Require(host.DebugResolveCursorKindForPoint(Center(splitter->GetSeparatorBounds())) == WindowHostCursorKind::Default,
            "a disabled splitter uses the default cursor");
    splitter->Paint(host);
    Require(true, "a disabled splitter paints without a device context");
}

void TestSplitterPaintHandlesMissingDeviceContext()
{
    WindowHost host;
    auto root      = std::make_unique<Panel>();
    auto* splitter = root->AddChild<Splitter>();
    splitter->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 100.0f));
    host.SetRoot(std::move(root));
    host.SetFocusControl(splitter);
    splitter->Paint(host);
    Require(splitter->OnMouseDown(host, Center(splitter->GetSeparatorBounds()), false, 0), "a drag starts for the pressed paint path");
    splitter->Paint(host);
    Require(splitter->OnMouseUp(host, Center(splitter->GetSeparatorBounds()), false, 0), "the drag ends");
    Require(true, "splitter paint paths tolerate a missing device context");
}

void TestSplitterChangeCallbackCanReplaceRootSafely()
{
    WindowHost host;
    auto root      = std::make_unique<Panel>();
    auto* splitter = root->AddChild<Splitter>();
    splitter->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 100.0f));
    splitter->SetPosition(100.0f);
    size_t commits = 0u;
    splitter->SetOnChange([&](SplitterChange change)
    {
        if (change.phase == SplitterChangePhase::Commit)
        {
            ++commits;
            host.SetRoot(std::make_unique<Panel>());
        }
    });
    host.SetRoot(std::move(root));

    Require(splitter->OnMouseDown(host, D2D1::Point2F(103.0f, 50.0f), false, 0), "the drag starts before root replacement");
    Require(splitter->OnMouseMove(host, D2D1::Point2F(153.0f, 50.0f), 0), "the drag moves before root replacement");
    Require(splitter->OnMouseUp(host, D2D1::Point2F(153.0f, 50.0f), false, 0), "the splitter survives root replacement during commit");
    Require(commits == 1u, "the commit callback ran once");
    Require(host.GetRoot() != nullptr, "the commit callback replaced the root safely");
}

// ── NumericStepper ───────────────────────────────────────────────────────

void TestNumericStepperDefaultState()
{
    NumericStepper stepper;
    Require(stepper.GetValue() == 0.0, "stepper defaults to zero");
    Require(stepper.GetMinimum() == -1.0e9 && stepper.GetMaximum() == 1.0e9, "stepper defaults to a wide range");
    Require(stepper.GetStep() == 1.0 && stepper.GetLargeStep() == 10.0, "stepper defaults its steps");
    Require(stepper.GetDecimals() == 0u, "stepper defaults to integers");
    Require(stepper.Field().GetText() == L"0", "stepper text shows the default value");
    Require(! stepper.IsEditing(), "stepper is not editing initially");
    Require(stepper.IncrementButton().GetVariant() == ButtonVariant::IconOnly, "increment button is icon-only");
    Require(stepper.DecrementButton().GetVariant() == ButtonVariant::IconOnly, "decrement button is icon-only");
    Require(stepper.GetLabel().empty() && stepper.GetUnit().empty(), "stepper has no label or unit initially");
}

void TestNumericStepperParseAndFormat()
{
    Require(NumericStepper::ParseValue(L"12") == 12.0, "parse accepts an integer");
    Require(NumericStepper::ParseValue(L" -3.5 ") == -3.5, "parse trims whitespace and accepts a sign");
    Require(NumericStepper::ParseValue(L"1,25") == 1.25, "parse accepts a comma fraction");
    Require(NumericStepper::ParseValue(L"+7") == 7.0, "parse accepts a plus sign");
    Require(NumericStepper::ParseValue(L".5") == 0.5, "parse accepts a leading point");
    Require(! NumericStepper::ParseValue(L"").has_value(), "parse rejects empty text");
    Require(! NumericStepper::ParseValue(L"abc").has_value(), "parse rejects letters");
    Require(! NumericStepper::ParseValue(L"1.2.3").has_value(), "parse rejects two points");
    Require(! NumericStepper::ParseValue(L".").has_value(), "parse rejects a lone point");
    Require(! NumericStepper::ParseValue(L"-").has_value(), "parse rejects a lone sign");
    Require(! NumericStepper::ParseValue(L"1e5").has_value(), "parse rejects exponents");
    Require(! NumericStepper::ParseValue(L"1 2").has_value(), "parse rejects inner whitespace");

    NumericStepper stepper;
    Require(stepper.FormatValue(2.5) == L"3", "integer formatting rounds half away from zero");
    Require(stepper.FormatValue(-0.4) == L"0", "integer formatting never prints negative zero");
    stepper.SetDecimals(2);
    Require(stepper.FormatValue(1.25) == L"1.25", "formatting honors the decimals");
    Require(stepper.FormatValue(-0.001) == L"0.00", "fraction formatting never prints negative zero");
    Require(stepper.FormatValue(3.0) == L"3.00", "formatting pads the fraction");
    stepper.SetDecimals(9);
    Require(stepper.GetDecimals() == 6u, "decimals cap at six");
}

void TestNumericStepperSetValueClampsSilently()
{
    NumericStepper stepper;
    StepperEvents events;
    stepper.SetOnChange([&](NumericStepperChange change)
    {
        events.phases.push_back(change.phase);
        events.lastValue = change.value;
    });
    stepper.SetMinimum(0.0);
    stepper.SetMaximum(10.0);
    stepper.SetValue(15.0);
    Require(stepper.GetValue() == 10.0, "SetValue clamps to the maximum");
    Require(stepper.Field().GetText() == L"10", "SetValue rewrites the text");
    stepper.SetValue(-2.0);
    Require(stepper.GetValue() == 0.0, "SetValue clamps to the minimum");
    stepper.SetValue(std::numeric_limits<double>::quiet_NaN());
    Require(stepper.GetValue() == 0.0, "SetValue ignores a non-finite value");
    stepper.SetMinimum(20.0);
    Require(stepper.GetMaximum() == 20.0 && stepper.GetValue() == 20.0, "raising the minimum above the maximum lifts both");
    stepper.SetStep(0.0);
    stepper.SetLargeStep(-1.0);
    Require(stepper.GetStep() == 1.0 && stepper.GetLargeStep() == 10.0, "non-positive steps are ignored");
    Require(events.phases.empty(), "SetValue and range changes never notify");
}

void TestNumericStepperNudgeAndButtonsCommit()
{
    WindowHost host;
    auto root     = std::make_unique<Panel>();
    auto* stepper = root->AddChild<NumericStepper>();
    stepper->SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, NumericStepper::kDefaultHeightDip));
    stepper->SetMinimum(0.0);
    stepper->SetMaximum(10.0);
    StepperEvents events;
    stepper->SetOnChange([&](NumericStepperChange change)
    {
        events.phases.push_back(change.phase);
        events.lastValue = change.value;
    });
    host.SetRoot(std::move(root));

    Require(stepper->Nudge(host, +1, false), "Nudge steps up");
    Require(stepper->GetValue() == 1.0 && stepper->Field().GetText() == L"1", "Nudge applies the step and text");
    Require(events.Count(NumericStepperChangePhase::Commit) == 1u && events.lastValue == 1.0, "Nudge commits once");
    Require(stepper->Nudge(host, +1, true), "large Nudge steps up");
    Require(stepper->GetValue() == 10.0, "large Nudge clamps to the maximum");
    Require(! stepper->Nudge(host, +1, false), "Nudge at the maximum is refused");
    Require(events.Count(NumericStepperChangePhase::Commit) == 2u, "a refused Nudge does not notify");
    Require(stepper->Nudge(host, -1, false), "Nudge steps down");
    Require(stepper->GetValue() == 9.0, "Nudge down applies the step");

    Click(host, stepper->IncrementButton(), "the increment button handles a click");
    Require(stepper->GetValue() == 10.0, "the increment button steps up");
    Click(host, stepper->DecrementButton(), "the decrement button handles a click");
    Click(host, stepper->DecrementButton(), "the decrement button handles a second click");
    Require(stepper->GetValue() == 8.0, "the decrement button steps down");
    Require(events.Count(NumericStepperChangePhase::Commit) == 6u, "each button click commits once");
    Require(events.Count(NumericStepperChangePhase::Preview) == 0u, "buttons never preview");

    Require(stepper->RequestValue(host, 3.5), "RequestValue accepts a value");
    Require(stepper->GetValue() == 4.0 && stepper->Field().GetText() == L"4", "RequestValue applies the value and formats it");
    Require(! stepper->RequestValue(host, std::numeric_limits<double>::infinity()), "RequestValue rejects a non-finite value");
    Require(events.lastValue == 4.0 && events.Count(NumericStepperChangePhase::Commit) == 7u, "RequestValue commits once");
}

void TestNumericStepperTypingPreviewsEnterCommitsEscapeCancels()
{
    WindowHost host;
    auto root     = std::make_unique<Panel>();
    auto* stepper = root->AddChild<NumericStepper>();
    stepper->SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, NumericStepper::kDefaultHeightDip));
    stepper->SetMinimum(0.0);
    stepper->SetMaximum(100.0);
    stepper->SetValue(5.0);
    StepperEvents events;
    stepper->SetOnChange([&](NumericStepperChange change)
    {
        events.phases.push_back(change.phase);
        events.lastValue = change.value;
    });
    host.SetRoot(std::move(root));
    TextField& field = stepper->Field();

    TypeText(host, field, L"7");
    Require(stepper->IsEditing(), "typing opens an edit");
    Require(stepper->GetValue() == 7.0, "typing previews the parsed value");
    Require(events.Count(NumericStepperChangePhase::Preview) == 1u && events.lastValue == 7.0, "typing notifies a preview");
    Require(field.OnKeyDown(host, VK_RETURN, 0), "Enter is handled by the field");
    Require(! stepper->IsEditing(), "Enter closes the edit");
    Require(events.Count(NumericStepperChangePhase::Commit) == 1u && events.lastValue == 7.0, "Enter commits once");

    TypeText(host, field, L"9");
    Require(stepper->GetValue() == 9.0, "a second edit previews");
    Require(field.OnKeyDown(host, VK_ESCAPE, 0), "Escape is handled while editing");
    Require(stepper->GetValue() == 7.0 && field.GetText() == L"7", "Escape restores the committed value and text");
    Require(events.Count(NumericStepperChangePhase::Cancel) == 1u && events.lastValue == 7.0, "Escape notifies cancel");
    Require(! field.OnKeyDown(host, VK_ESCAPE, 0), "Escape outside an edit is not consumed by the stepper");

    TypeText(host, field, L"x");
    Require(! stepper->IsEditing() && stepper->GetValue() == 7.0, "text that does not parse does not preview");
    host.SetFocusControl(nullptr);
    Require(field.GetText() == L"7", "focus loss reverts text that does not parse");
    Require(events.Count(NumericStepperChangePhase::Commit) == 1u, "focus loss without an edit does not commit");

    TypeText(host, field, L"250");
    Require(stepper->GetValue() == 100.0, "typing clamps the preview to the maximum");
    host.SetFocusControl(nullptr);
    Require(! stepper->IsEditing() && field.GetText() == L"100", "focus loss commits the clamped value and rewrites the text");
    Require(events.Count(NumericStepperChangePhase::Commit) == 2u && events.lastValue == 100.0, "focus loss commits once");

    TypeText(host, field, L"42");
    Require(field.OnKeyDown(host, VK_UP, 0), "Up is handled while editing");
    Require(stepper->GetValue() == 43.0 && ! stepper->IsEditing(), "Up nudges from the previewed value and commits");
    Require(field.OnKeyDown(host, VK_DOWN, MK_SHIFT), "Shift+Down is handled");
    Require(stepper->GetValue() == 33.0, "Shift+Down nudges by the large step");
    Require(events.Count(NumericStepperChangePhase::Commit) == 4u, "arrow nudges commit");
}

void TestNumericStepperDisabledAndLayout()
{
    WindowHost host;
    auto root     = std::make_unique<Panel>();
    auto* stepper = root->AddChild<NumericStepper>();
    stepper->SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 32.0f));
    stepper->SetLabel(L"X", 20.0f);
    stepper->SetUnit(L"px", 24.0f);
    host.SetRoot(std::move(root));

    Require(stepper->GetLabel() == L"X" && stepper->GetUnit() == L"px", "label and unit round-trip");
    const D2D1_RECT_F field     = stepper->Field().GetBounds();
    const D2D1_RECT_F increment = stepper->IncrementButton().GetBounds();
    const D2D1_RECT_F decrement = stepper->DecrementButton().GetBounds();
    Require(RectNear(field, D2D1::RectF(24.0f, 0.0f, 146.0f, 32.0f)), "the field sits after the label and before the unit");
    Require(RectNear(increment, D2D1::RectF(178.0f, 0.0f, 200.0f, 16.0f)), "the increment button is the top half of the trailing column");
    Require(RectNear(decrement, D2D1::RectF(178.0f, 16.0f, 200.0f, 32.0f)), "the decrement button is the bottom half of the trailing column");

    stepper->SetFlowDirection(FlowDirection::RightToLeft);
    Require(RectNear(stepper->Field().GetBounds(), D2D1::RectF(54.0f, 0.0f, 176.0f, 32.0f)), "right-to-left mirrors the field");
    Require(RectNear(stepper->IncrementButton().GetBounds(), D2D1::RectF(0.0f, 0.0f, 22.0f, 16.0f)), "right-to-left mirrors the buttons");

    stepper->SetEnabled(false);
    Require(! stepper->Field().IsEnabled(), "disabling the stepper disables the field");
    Require(! stepper->IncrementButton().IsEnabled() && ! stepper->DecrementButton().IsEnabled(), "disabling the stepper disables the buttons");
    Require(! stepper->Nudge(host, +1, false), "a disabled stepper refuses Nudge");
    Require(! stepper->RequestValue(host, 1.0), "a disabled stepper refuses RequestValue");
    stepper->Paint(host);
    stepper->SetEnabled(true);
    Require(stepper->Field().IsEnabled() && stepper->IncrementButton().IsEnabled(), "re-enabling the stepper re-enables the children");
    stepper->Paint(host);
    Require(true, "stepper paint paths tolerate a missing device context");
}

void TestNumericStepperChangeCallbackCanReplaceRootSafely()
{
    WindowHost host;
    auto root     = std::make_unique<Panel>();
    auto* stepper = root->AddChild<NumericStepper>();
    stepper->SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 32.0f));
    size_t commits = 0u;
    stepper->SetOnChange([&](NumericStepperChange change)
    {
        if (change.phase == NumericStepperChangePhase::Commit)
        {
            ++commits;
            host.SetRoot(std::make_unique<Panel>());
        }
    });
    host.SetRoot(std::move(root));
    Require(stepper->Nudge(host, +1, false), "the stepper survives root replacement during commit");
    Require(commits == 1u, "the commit callback ran once");
    Require(host.GetRoot() != nullptr, "the commit callback replaced the root safely");
}

// ── ColorPicker ──────────────────────────────────────────────────────────

void TestColorHelpers()
{
    const HsvColor red = HsvFromArgb(0xFFFF0000u);
    RequireFloatNear(red.hue, 0.0f, 0.001f, "red has hue 0");
    RequireFloatNear(red.saturation, 1.0f, 0.001f, "red is fully saturated");
    RequireFloatNear(red.value, 1.0f, 0.001f, "red has full value");
    RequireFloatNear(HsvFromArgb(0xFF00FF00u).hue, 120.0f, 0.001f, "green has hue 120");
    RequireFloatNear(HsvFromArgb(0xFF0000FFu).hue, 240.0f, 0.001f, "blue has hue 240");
    RequireFloatNear(HsvFromArgb(0xFFFF00FFu).hue, 300.0f, 0.001f, "magenta has hue 300");
    Require(ArgbFromHsv(HsvColor{0.0f, 1.0f, 1.0f}) == 0xFFFF0000u, "hsv red converts back");
    Require(ArgbFromHsv(HsvColor{120.0f, 1.0f, 1.0f}) == 0xFF00FF00u, "hsv green converts back");
    Require(ArgbFromHsv(HsvColor{240.0f, 1.0f, 1.0f}) == 0xFF0000FFu, "hsv blue converts back");
    Require(ArgbFromHsv(HsvColor{0.0f, 0.0f, 1.0f}) == 0xFFFFFFFFu, "hsv white converts back");
    Require(ArgbFromHsv(HsvColor{0.0f, 0.0f, 0.0f}, 0x80u) == 0x80000000u, "hsv black keeps the requested alpha");
    Require(ArgbFromHsv(HsvColor{360.0f, 1.0f, 1.0f}) == 0xFFFF0000u, "hue wraps at 360");
    Require(ArgbFromHsv(HsvColor{-120.0f, 1.0f, 1.0f}) == 0xFF0000FFu, "negative hue wraps");

    for (const uint32_t argb : {0xFF123456u, 0xFFABCDEFu, 0xFF800000u, 0xFF0F0F10u, 0xFFFEDCBAu})
    {
        const uint32_t round = ArgbFromHsv(HsvFromArgb(argb));
        for (unsigned shift = 0u; shift < 24u; shift += 8u)
        {
            const int a = static_cast<int>((argb >> shift) & 0xFFu);
            const int b = static_cast<int>((round >> shift) & 0xFFu);
            Require(std::abs(a - b) <= 1, "argb survives an hsv round trip within one level");
        }
    }

    const HsvColor previous{200.0f, 0.75f, 0.5f};
    const HsvColor gray = HsvFromArgb(0xFF808080u, &previous);
    RequireFloatNear(gray.hue, 200.0f, 0.001f, "gray keeps the previous hue");
    RequireFloatNear(gray.saturation, 0.0f, 0.001f, "gray has no saturation");
    const HsvColor black = HsvFromArgb(0xFF000000u, &previous);
    RequireFloatNear(black.hue, 200.0f, 0.001f, "black keeps the previous hue");
    RequireFloatNear(black.saturation, 0.75f, 0.001f, "black keeps the previous saturation");
    RequireFloatNear(black.value, 0.0f, 0.001f, "black has no value");
    RequireFloatNear(HsvFromArgb(0xFF808080u).hue, 0.0f, 0.001f, "gray without history has hue 0");

    Require(ParseHexColor(L"#1A2b3C") == 0xFF1A2B3Cu, "hex parse accepts mixed case with a hash");
    Require(ParseHexColor(L"abc") == 0xFFAABBCCu, "hex parse expands the short form");
    Require(ParseHexColor(L" #ffffff ") == 0xFFFFFFFFu, "hex parse trims whitespace");
    Require(ParseHexColor(L"000000") == 0xFF000000u, "hex parse accepts six digits without a hash");
    Require(! ParseHexColor(L"#12345").has_value(), "hex parse rejects five digits");
    Require(! ParseHexColor(L"xyz").has_value(), "hex parse rejects non-hex letters");
    Require(! ParseHexColor(L"").has_value(), "hex parse rejects empty text");
    Require(! ParseHexColor(L"#").has_value(), "hex parse rejects a lone hash");
    Require(! ParseHexColor(L"#1234567").has_value(), "hex parse rejects seven digits");
    Require(FormatHexColor(0xFF1A2B3Cu) == L"#1A2B3C", "hex format is upper-case with a hash");
    Require(FormatHexColor(0x00000000u) == L"#000000", "hex format ignores alpha");
}

void TestColorPickerDefaultState()
{
    ColorPicker picker;
    Require(picker.GetColor() == 0xFF000000u && picker.GetCurrentColor() == 0xFF000000u, "picker defaults to black");
    Require(picker.GetLabels().ok == L"OK" && picker.GetLabels().cancel == L"Cancel", "picker ships neutral default captions");
    Require(picker.OkButton().GetText() == L"OK" && picker.CancelButton().GetText() == L"Cancel", "buttons show the default captions");
    Require(picker.HexField().GetText() == L"#000000", "hex field shows the default color");
    Require(picker.RedField().GetValue() == 0.0 && picker.RedField().GetMaximum() == 255.0, "component steppers are 8-bit");
    Require(! picker.IsDragging(), "picker is not dragging initially");

    ColorPicker::Labels labels;
    labels.ok     = L"Apply";
    labels.cancel = L"Discard";
    labels.red    = L"Rot";
    picker.SetLabels(labels);
    Require(picker.OkButton().GetText() == L"Apply" && picker.CancelButton().GetText() == L"Discard", "SetLabels recaptions the buttons");
    Require(picker.RedField().GetLabel() == L"Rot", "SetLabels relabels the component steppers");
}

void TestColorPickerSetColorSyncsChildrenSilently()
{
    ColorPicker picker;
    PickerEvents events;
    picker.SetOnChange([&](ColorPickerChange change)
    {
        events.phases.push_back(change.phase);
        events.lastArgb = change.argb;
    });
    picker.SetColor(0x00123456u);
    Require(picker.GetColor() == 0xFF123456u, "SetColor forces opaque alpha");
    Require(picker.GetCurrentColor() == 0xFF123456u, "SetColor also sets the current swatch");
    Require(picker.RedField().GetValue() == 0x12 && picker.GreenField().GetValue() == 0x34 && picker.BlueField().GetValue() == 0x56,
            "SetColor syncs the component steppers");
    Require(picker.HexField().GetText() == L"#123456", "SetColor syncs the hex field");
    picker.SetCurrentColor(0xFF654321u);
    Require(picker.GetCurrentColor() == 0xFF654321u && picker.GetColor() == 0xFF123456u, "SetCurrentColor changes only the reference swatch");
    Require(events.phases.empty(), "SetColor and SetCurrentColor never notify");
}

void TestColorPickerFieldDragPreviewsAndOkCommits()
{
    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* picker = root->AddChild<ColorPicker>();
    picker->SetBounds(D2D1::RectF(0.0f, 0.0f, ColorPicker::kDefaultWidthDip, ColorPicker::kDefaultHeightDip));
    picker->SetColor(0xFFFF0000u);
    PickerEvents events;
    picker->SetOnChange([&](ColorPickerChange change)
    {
        events.phases.push_back(change.phase);
        events.lastArgb = change.argb;
    });
    host.SetRoot(std::move(root));

    const D2D1_RECT_F field = picker->GetFieldRect();
    Require(RectNear(field, D2D1::RectF(8.0f, 8.0f, 168.0f, 168.0f)), "the field is a 160 DIP square inset by the gap");
    Require(RectNear(picker->GetHueStripRect(), D2D1::RectF(176.0f, 8.0f, 196.0f, 168.0f)), "the hue strip follows the field");
    Require(picker->GetNewSwatchRect().left > picker->GetHueStripRect().right, "the swatches follow the hue strip");
    Require(picker->GetCurrentSwatchRect().left > picker->GetNewSwatchRect().right, "the current swatch follows the new swatch");

    Require(picker->OnMouseDown(host, D2D1::Point2F(field.left, field.top), false, 0), "a press in the field starts a drag");
    Require(picker->IsDragging(), "the field drag is reported");
    Require(picker->GetColor() == 0xFFFFFFFFu, "the top-left field corner is white");
    Require(events.Count(ColorPickerChangePhase::Preview) == 1u && events.lastArgb == 0xFFFFFFFFu, "the field press previews");
    Require(picker->OnMouseMove(host, D2D1::Point2F(field.right, field.bottom), 0), "the field drag moves");
    Require(picker->GetColor() == 0xFF000000u, "the bottom-right field corner is black");
    RequireFloatNear(picker->GetHsv().hue, 0.0f, 0.001f, "black keeps the hue during the drag");
    RequireFloatNear(picker->GetHsv().saturation, 1.0f, 0.001f, "black keeps the saturation during the drag");
    Require(picker->OnMouseMove(host, D2D1::Point2F(field.right + 500.0f, field.top - 500.0f), 0), "the drag clamps outside the field");
    Require(picker->GetColor() == 0xFFFF0000u, "clamping outside the field reaches the pure hue corner");
    Require(picker->OnMouseUp(host, D2D1::Point2F(field.right, field.top), false, 0), "release ends the drag");
    Require(! picker->IsDragging(), "release clears the drag");
    Require(picker->GetCurrentColor() == 0xFFFF0000u && events.Count(ColorPickerChangePhase::Commit) == 0u, "release alone does not commit");
    Require(picker->RedField().GetValue() == 255.0 && picker->HexField().GetText() == L"#FF0000", "dragging syncs the children");

    Require(picker->OnMouseMove(host, D2D1::Point2F(field.left, field.top), 0) == false, "moves without a drag are not handled");
    Require(picker->OnMouseDown(host, D2D1::Point2F(field.left, field.top), false, 0), "a second drag starts");
    Require(picker->OnMouseUp(host, D2D1::Point2F(field.left, field.top), false, 0), "the second drag ends");
    Require(picker->GetColor() == 0xFFFFFFFFu, "the second drag previewed white");
    Click(host, picker->OkButton(), "the OK button handles a click");
    Require(picker->GetCurrentColor() == 0xFFFFFFFFu, "OK copies the editing color into the current swatch");
    Require(events.Count(ColorPickerChangePhase::Commit) == 1u && events.lastArgb == 0xFFFFFFFFu, "OK commits once");
    Require(! picker->OnMouseDown(host, D2D1::Point2F(field.left, field.top), true, 0), "a right press does not start a drag");
    Require(! picker->OnMouseDown(host, D2D1::Point2F(field.right + 4.0f, field.bottom + 40.0f), false, 0), "a press outside field and strip is ignored");
}

void TestColorPickerHueStripAndKeyboard()
{
    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* picker = root->AddChild<ColorPicker>();
    picker->SetBounds(D2D1::RectF(0.0f, 0.0f, ColorPicker::kDefaultWidthDip, ColorPicker::kDefaultHeightDip));
    picker->SetColor(0xFFFF0000u);
    PickerEvents events;
    picker->SetOnChange([&](ColorPickerChange change)
    {
        events.phases.push_back(change.phase);
        events.lastArgb = change.argb;
    });
    host.SetRoot(std::move(root));

    const D2D1_RECT_F strip = picker->GetHueStripRect();
    const float height      = strip.bottom - strip.top;
    Require(picker->OnMouseDown(host, D2D1::Point2F(strip.left + 5.0f, strip.top + height / 3.0f), false, 0), "a press on the strip starts a hue drag");
    RequireFloatNear(picker->GetHsv().hue, 120.0f, 0.01f, "one third down the strip is hue 120");
    Require(picker->GetColor() == 0xFF00FF00u, "hue 120 at full saturation and value is green");
    Require(picker->OnMouseMove(host, D2D1::Point2F(strip.left, strip.top + height * 2.0f / 3.0f), 0), "the hue drag moves");
    RequireFloatNear(picker->GetHsv().hue, 240.0f, 0.01f, "two thirds down the strip is hue 240");
    Require(picker->OnMouseMove(host, D2D1::Point2F(strip.left, strip.bottom + 50.0f), 0), "the hue drag clamps below the strip");
    Require(picker->GetHsv().hue < 360.0f && picker->GetHsv().hue > 359.0f, "the strip bottom stays below 360");
    Require(picker->OnMouseUp(host, D2D1::Point2F(strip.left, strip.bottom + 50.0f), false, 0), "the hue drag ends");
    Require(events.Count(ColorPickerChangePhase::Preview) >= 3u, "hue drags preview");

    picker->SetColor(0xFFFF0000u); // hue 0, then gray keeps it
    picker->SetColor(0xFF808080u);
    host.SetFocusControl(picker);
    Require(picker->OnKeyDown(host, VK_UP, 0), "Up is handled");
    Require(picker->GetColor() == 0xFF818181u, "Up raises the value by one level");
    Require(picker->OnKeyDown(host, VK_DOWN, MK_SHIFT), "Shift+Down is handled");
    Require(picker->GetColor() == 0xFF777777u, "Shift+Down lowers the value by ten levels");
    Require(picker->OnKeyDown(host, VK_NEXT, 0), "Page Down is handled");
    RequireFloatNear(picker->GetHsv().hue, 1.0f, 0.001f, "Page Down steps the hue by one degree");
    Require(picker->OnKeyDown(host, VK_PRIOR, MK_SHIFT), "Shift+Page Up is handled");
    RequireFloatNear(picker->GetHsv().hue, 351.0f, 0.001f, "Shift+Page Up steps the hue by ten degrees and wraps");
    Require(picker->OnKeyDown(host, VK_RIGHT, MK_SHIFT), "Shift+Right is handled");
    RequireFloatNear(picker->GetHsv().saturation, 10.0f / 255.0f, 0.001f, "Shift+Right raises the saturation by ten levels");
    Require(picker->OnKeyDown(host, VK_LEFT, 0), "Left is handled");
    RequireFloatNear(picker->GetHsv().saturation, 9.0f / 255.0f, 0.001f, "Left lowers the saturation by one level");
    picker->SetFlowDirection(FlowDirection::RightToLeft);
    Require(picker->OnKeyDown(host, VK_LEFT, 0), "Left is handled in right-to-left flow");
    RequireFloatNear(picker->GetHsv().saturation, 10.0f / 255.0f, 0.001f, "Left raises the saturation in right-to-left flow");
    Require(picker->GetFieldRect().right > picker->GetHueStripRect().right, "right-to-left places the hue strip before the field");
    Require(! picker->OnKeyDown(host, VK_TAB, 0), "Tab is not handled by the picker");
    Require(picker->OnKeyDown(host, VK_RETURN, 0), "Enter is handled");
    Require(picker->GetCurrentColor() == picker->GetColor() && events.Count(ColorPickerChangePhase::Commit) == 1u, "Enter commits");
}

void TestColorPickerCancelPaths()
{
    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* picker = root->AddChild<ColorPicker>();
    picker->SetBounds(D2D1::RectF(0.0f, 0.0f, ColorPicker::kDefaultWidthDip, ColorPicker::kDefaultHeightDip));
    picker->SetColor(0xFF112233u);
    PickerEvents events;
    picker->SetOnChange([&](ColorPickerChange change)
    {
        events.phases.push_back(change.phase);
        events.lastArgb = change.argb;
    });
    host.SetRoot(std::move(root));
    const D2D1_RECT_F field = picker->GetFieldRect();

    Require(picker->OnMouseDown(host, D2D1::Point2F(field.left, field.top), false, 0), "a drag starts");
    Require(picker->GetColor() == 0xFFFFFFFFu, "the drag previewed white");
    picker->OnCaptureLost(host);
    Require(! picker->IsDragging(), "capture loss ends the drag");
    Require(picker->GetColor() == 0xFF112233u, "capture loss restores the current color");
    Require(picker->HexField().GetText() == L"#112233" && picker->RedField().GetValue() == 0x11, "capture loss resyncs the children");
    Require(events.Count(ColorPickerChangePhase::Cancel) == 1u && events.lastArgb == 0xFF112233u, "capture loss notifies cancel");

    Require(picker->OnMouseDown(host, D2D1::Point2F(field.left, field.top), false, 0), "a second drag starts");
    Require(picker->OnKeyDown(host, VK_ESCAPE, 0), "Escape is handled during a drag");
    Require(! picker->IsDragging() && picker->GetColor() == 0xFF112233u, "Escape ends the drag and restores the current color");
    Require(events.Count(ColorPickerChangePhase::Cancel) == 2u, "Escape notifies cancel");

    picker->SampleColor(host, 0x00ABCDEFu);
    Require(picker->GetColor() == 0xFFABCDEFu, "SampleColor previews an opaque color");
    Require(events.Count(ColorPickerChangePhase::Preview) >= 2u && events.lastArgb == 0xFFABCDEFu, "SampleColor notifies a preview");
    Click(host, picker->CancelButton(), "the Cancel button handles a click");
    Require(picker->GetColor() == 0xFF112233u, "Cancel restores the current color");
    Require(events.Count(ColorPickerChangePhase::Cancel) == 3u, "Cancel notifies once");
    Require(events.Count(ColorPickerChangePhase::Commit) == 0u, "cancel paths never commit");
}

void TestColorPickerTypedComponentsAndHex()
{
    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* picker = root->AddChild<ColorPicker>();
    picker->SetBounds(D2D1::RectF(0.0f, 0.0f, ColorPicker::kDefaultWidthDip, ColorPicker::kDefaultHeightDip));
    picker->SetColor(0xFF112233u);
    PickerEvents events;
    picker->SetOnChange([&](ColorPickerChange change)
    {
        events.phases.push_back(change.phase);
        events.lastArgb = change.argb;
    });
    host.SetRoot(std::move(root));

    Require(picker->RedField().RequestValue(host, 200.0), "the red stepper accepts a value");
    Require(picker->GetColor() == 0xFFC82233u, "a committed component previews the color");
    Require(picker->HexField().GetText() == L"#C82233", "a component change resyncs the hex field");
    Require(events.Count(ColorPickerChangePhase::Preview) == 1u && events.lastArgb == 0xFFC82233u, "a component change notifies a preview");
    Require(picker->GreenField().Nudge(host, +1, true), "the green stepper nudges");
    Require(picker->GetColor() == 0xFFC82C33u, "a nudged component previews the color");

    TextField& hex = picker->HexField();
    TypeText(host, hex, L"#00F");
    Require(picker->GetColor() == 0xFFC82C33u, "three typed digits do not preview mid-typing");
    TypeText(host, hex, L"#00FF00");
    Require(picker->GetColor() == 0xFF00FF00u, "six typed digits preview");
    Require(picker->RedField().GetValue() == 0.0 && picker->GreenField().GetValue() == 255.0, "a hex change resyncs the component steppers");
    TypeText(host, hex, L"zz");
    Require(picker->GetColor() == 0xFF00FF00u, "text that is not a color does not preview");
    host.SetFocusControl(nullptr);
    Require(hex.GetText() == L"#00FF00", "focus loss restores the hex text");
    TypeText(host, hex, L"abc");
    host.SetFocusControl(nullptr);
    Require(picker->GetColor() == 0xFFAABBCCu && hex.GetText() == L"#AABBCC", "focus loss applies and normalizes a short form");
    TypeText(host, hex, L"123");
    Require(hex.OnKeyDown(host, VK_RETURN, 0), "Enter in the hex field is handled");
    Require(picker->GetColor() == 0xFF112233u && picker->GetCurrentColor() == 0xFF112233u, "Enter in the hex field applies the short form and commits");
    Require(events.Count(ColorPickerChangePhase::Commit) == 1u, "Enter in the hex field commits once");
}

void TestColorPickerDisabledAndPaint()
{
    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* picker = root->AddChild<ColorPicker>();
    picker->SetBounds(D2D1::RectF(0.0f, 0.0f, ColorPicker::kDefaultWidthDip, ColorPicker::kDefaultHeightDip));
    picker->SetColor(0xFF3A7BD5u);
    host.SetRoot(std::move(root));
    host.SetFocusControl(picker);
    picker->Paint(host);
    Require(picker->OnMouseDown(host, Center(picker->GetHueStripRect()), false, 0), "a hue drag starts for the pressed paint path");
    picker->Paint(host);
    Require(picker->OnMouseUp(host, Center(picker->GetHueStripRect()), false, 0), "the hue drag ends");

    picker->SetEnabled(false);
    Require(! picker->RedField().IsEnabled() && ! picker->HexField().IsEnabled(), "disabling the picker disables the fields");
    Require(! picker->OkButton().IsEnabled() && ! picker->CancelButton().IsEnabled(), "disabling the picker disables the buttons");
    Require(! picker->OnMouseDown(host, Center(picker->GetFieldRect()), false, 0), "a disabled picker ignores presses");
    Require(! picker->OnKeyDown(host, VK_UP, 0), "a disabled picker ignores keys");
    picker->Paint(host);
    picker->SetEnabled(true);
    Require(picker->RedField().IsEnabled() && picker->OkButton().IsEnabled(), "re-enabling the picker re-enables the children");
    Require(true, "picker paint paths tolerate a missing device context");
}

void TestColorPickerCommitCallbackCanReplaceRootSafely()
{
    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* picker = root->AddChild<ColorPicker>();
    picker->SetBounds(D2D1::RectF(0.0f, 0.0f, ColorPicker::kDefaultWidthDip, ColorPicker::kDefaultHeightDip));
    size_t commits = 0u;
    picker->SetOnChange([&](ColorPickerChange change)
    {
        if (change.phase == ColorPickerChangePhase::Commit)
        {
            ++commits;
            host.SetRoot(std::make_unique<Panel>());
        }
    });
    host.SetRoot(std::move(root));
    Click(host, picker->OkButton(), "the OK button survives root replacement during commit");
    Require(commits == 1u, "the commit callback ran once");
    Require(host.GetRoot() != nullptr, "the commit callback replaced the root safely");
}

void TestEditorControlsAreCatalogued()
{
    Require(GetControlCatalog().size() == 30u, "the catalog lists 30 controls");
    for (const ControlKind kind : {ControlKind::Splitter, ControlKind::NumericStepper, ControlKind::ColorPicker})
    {
        std::unique_ptr<Control> control;
        Require(SUCCEEDED(CreateControl(kind, control)) && control != nullptr, "the factory creates each editor control");
    }
    bool found = false;
    for (const ControlDescriptor& descriptor : GetControlCatalog())
    {
        found = found || (descriptor.kind == ControlKind::ColorPicker && descriptor.name == L"ColorPicker");
    }
    Require(found, "the catalog names the color picker");
}
} // namespace

void RunEditorControlTests()
{
    // Splitter
    TestSplitterDefaultState();
    TestSplitterClampsToPaneMinimums();
    TestSplitterHitBoundsCoverOnlySeparator();
    TestSplitterDragPreviewsCommitsAndCancels();
    TestSplitterKeyboardStepsAndCommits();
    TestSplitterHorizontalOrientationAndCursors();
    TestSplitterDisabledAndRequestPosition();
    TestSplitterPaintHandlesMissingDeviceContext();
    TestSplitterChangeCallbackCanReplaceRootSafely();

    // NumericStepper
    TestNumericStepperDefaultState();
    TestNumericStepperParseAndFormat();
    TestNumericStepperSetValueClampsSilently();
    TestNumericStepperNudgeAndButtonsCommit();
    TestNumericStepperTypingPreviewsEnterCommitsEscapeCancels();
    TestNumericStepperDisabledAndLayout();
    TestNumericStepperChangeCallbackCanReplaceRootSafely();

    // ColorPicker
    TestColorHelpers();
    TestColorPickerDefaultState();
    TestColorPickerSetColorSyncsChildrenSilently();
    TestColorPickerFieldDragPreviewsAndOkCommits();
    TestColorPickerHueStripAndKeyboard();
    TestColorPickerCancelPaths();
    TestColorPickerTypedComponentsAndHex();
    TestColorPickerDisabledAndPaint();
    TestColorPickerCommitCallbackCanReplaceRootSafely();
    TestEditorControlsAreCatalogued();
}
