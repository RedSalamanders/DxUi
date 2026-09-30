#include "DxUiTestHelpers.h"

#include "../../include/DxUi/ControlCatalog.h"

#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
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

void TestNumericStepperUnparseableEditRevertsAndDecimalsRound()
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

    TypeText(host, field, L"8");
    Require(stepper->IsEditing() && stepper->GetValue() == 8.0 && events.Count(NumericStepperChangePhase::Preview) == 1u, "typing previews");
    static_cast<void>(field.OnChar(host, L'x', 0));
    Require(field.GetText() == L"8x" && stepper->GetValue() == 8.0, "text that stops parsing keeps the last preview while editing");
    Require(field.OnKeyDown(host, VK_RETURN, 0), "Enter is handled");
    Require(stepper->GetValue() == 5.0 && field.GetText() == L"5" && ! stepper->IsEditing(),
            "an edit whose text no longer parses reverts to the committed value");
    Require(events.Count(NumericStepperChangePhase::Commit) == 0u && events.Count(NumericStepperChangePhase::Cancel) == 1u && events.lastValue == 5.0,
            "the reverted edit notifies cancel instead of committing the abandoned preview");

    stepper->SetDecimals(2);
    stepper->SetValue(1.2345);
    Require(stepper->GetValue() == 1.23 && field.GetText() == L"1.23", "two decimals round the value and the text");
    stepper->SetDecimals(0);
    Require(stepper->GetValue() == 1.0 && field.GetText() == L"1", "fewer decimals re-round the value to the text it shows");
    Require(events.Count(NumericStepperChangePhase::Commit) == 0u, "precision changes never notify");
}

void TestNumericStepperEditsEndWithCommitOrCancel()
{
    WindowHost host;
    auto root     = std::make_unique<Panel>();
    auto* stepper = root->AddChild<NumericStepper>();
    stepper->SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, NumericStepper::kDefaultHeightDip));
    stepper->SetMinimum(0.0);
    stepper->SetMaximum(100.0);
    stepper->SetValue(40.0);
    StepperEvents events;
    stepper->SetOnChange([&](NumericStepperChange change)
    {
        events.phases.push_back(change.phase);
        events.lastValue = change.value;
    });
    host.SetRoot(std::move(root));
    TextField& field = stepper->Field();

    // Every preview ends with Commit or Cancel, even when the edit returns to its start value.
    TypeText(host, field, L"41");
    Require(field.OnKeyDown(host, VK_BACK, 0), "Backspace is handled");
    static_cast<void>(field.OnChar(host, L'0', 0));
    Require(field.GetText() == L"40" && events.Count(NumericStepperChangePhase::Preview) >= 3u, "typing back to the start value previews each step");
    Require(field.OnKeyDown(host, VK_RETURN, 0), "Enter is handled");
    Require(events.Count(NumericStepperChangePhase::Commit) == 1u && events.lastValue == 40.0 && ! stepper->IsEditing(),
            "an edit that previewed and returned to its start still commits");

    // UI Automation's ValuePattern changes the text without focus: no Enter or focus loss follows, so it commits at once.
    host.SetFocusControl(nullptr);
    field.SetTextAndNotify(L"55");
    Require(stepper->GetValue() == 55.0 && ! stepper->IsEditing(), "an unfocused text change commits at once");
    Require(events.lastValue == 55.0 && events.phases.back() == NumericStepperChangePhase::Commit, "the unfocused change notifies commit");
    field.SetTextAndNotify(L"abc");
    Require(stepper->GetValue() == 55.0 && field.GetText() == L"55", "unfocused text that does not parse reverts at once");

    // The digits an IME or a locale keyboard types parse like ASCII.
    Require(NumericStepper::ParseValue(L"\xFF11\xFF12\xFF0E\xFF15") == 12.5, "full-width digits and point parse");
    Require(NumericStepper::ParseValue(L"\x2212\x0663") == -3.0, "the minus sign and Arabic-Indic digits parse");
    Require(NumericStepper::ParseValue(L"\x06F4\x066B\x06F5") == 4.5, "extended Arabic-Indic digits and the Arabic decimal separator parse");

    // A step finer than the shown precision moves one shown unit rather than refusing in one direction.
    stepper->SetStep(0.5);
    stepper->SetValue(2.0);
    Require(stepper->Nudge(host, -1, false) && stepper->GetValue() == 1.0, "a fine step below the shown precision still steps down");
    Require(stepper->Nudge(host, +1, false) && stepper->GetValue() == 2.0, "and steps up");
}

void TestSplitterStaysInsideAnExtentBelowItsMinimums()
{
    Splitter splitter;
    splitter.SetBounds(D2D1::RectF(0.0f, 0.0f, 40.0f, 100.0f));
    splitter.SetPosition(10.0f);
    const D2D1_RECT_F separator = splitter.GetSeparatorBounds();
    Require(separator.left >= 0.0f && separator.right <= 40.0f && separator.left < separator.right,
            "an extent smaller than both minimums keeps the separator inside the control");
    Require(splitter.GetFirstPaneBounds().right <= 40.0f, "the first pane gives way instead of overflowing");
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
    Require(picker->RedField().GetBounds().right <= picker->GetHueStripRect().left, "a runtime flow change re-arranges the component fields");
    const D2D1_RECT_F rtlField = picker->GetFieldRect();
    Require(picker->OnMouseDown(host, D2D1::Point2F(rtlField.right - 1.0f, rtlField.top + 1.0f), false, 0), "a right-to-left field press starts a drag");
    Require(picker->GetHsv().saturation < 0.01f, "right-to-left flow mirrors the saturation axis the keys step (zero at the right edge)");
    Require(picker->OnMouseUp(host, D2D1::Point2F(rtlField.left + 1.0f, rtlField.top + 1.0f), false, 0), "the right-to-left drag ends");
    Require(picker->GetHsv().saturation > 0.99f, "full saturation is at the left edge in right-to-left flow");
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

// The step buttons show glyphs, which say nothing to a screen reader: they carry names, English by default and the
// consumer's localized ones once supplied. The picker's channel steppers name their channel.
void TestNumericStepperStepButtonsCarryAutomationNames()
{
    using namespace DxUi;
    NumericStepper stepper;
    Require(stepper.IncrementButton().GetAccessibleName() == L"Increase" && stepper.DecrementButton().GetAccessibleName() == L"Decrease",
            "step buttons have default names instead of their glyphs");
    stepper.SetStepButtonNames(L"Augmenter la largeur", L"Diminuer la largeur");
    Require(stepper.IncrementButton().GetAccessibleName() == L"Augmenter la largeur" && stepper.DecrementButton().GetAccessibleName() == L"Diminuer la largeur",
            "step buttons take the consumer's localized names");
    ColorPicker picker;
    Require(picker.RedField().IncrementButton().GetAccessibleName() == L"Increase red" &&
                picker.BlueField().DecrementButton().GetAccessibleName() == L"Decrease blue",
            "the picker's channel step buttons name their channel");
    ColorPicker::Labels labels = picker.GetLabels();
    labels.increaseRed         = L"Augmenter le rouge";
    labels.decreaseGreen       = L"Diminuer le vert";
    labels.increaseBlue        = L"Augmenter le bleu";
    picker.SetLabels(labels);
    Require(picker.RedField().IncrementButton().GetAccessibleName() == L"Augmenter le rouge" &&
                picker.GreenField().DecrementButton().GetAccessibleName() == L"Diminuer le vert" &&
                picker.BlueField().IncrementButton().GetAccessibleName() == L"Augmenter le bleu",
            "the picker's labels localize its channel step buttons");
}

// Translated captions need room: the channel and hex caption slots and the swatches follow the widths in Labels, so
// "Rot", "Hexadezimal" or "Couleur actuelle" are not cut to the English slot widths.
void TestColorPickerCaptionSlotsFollowTheLabelWidths()
{
    using namespace DxUi;
    ColorPicker picker;
    picker.SetBounds(D2D1::RectF(0.0f, 0.0f, 480.0f, ColorPicker::kDefaultHeightDip));
    const float englishField    = picker.RedField().Field().GetBounds().left;
    const float englishHex      = picker.HexField().GetBounds().left;
    ColorPicker::Labels labels  = picker.GetLabels();
    labels.red                  = L"Rot";
    labels.green                = L"Grün";
    labels.blue                 = L"Blau";
    labels.hex                  = L"Hexadezimal";
    labels.currentColor         = L"Couleur actuelle";
    labels.channelLabelWidthDip = 34.0f;
    labels.hexLabelWidthDip     = 82.0f;
    labels.swatchWidthDip       = 84.0f;
    picker.SetLabels(labels);
    Require(std::fabs(picker.RedField().Field().GetBounds().left - (englishField + 20.0f)) < 0.01f &&
                std::fabs(picker.BlueField().Field().GetBounds().left - (englishField + 20.0f)) < 0.01f,
            "a wider channel caption slot moves every channel field by the difference");
    Require(std::fabs(picker.HexField().GetBounds().left - (englishHex + 56.0f)) < 0.01f, "a wider hex caption slot moves the hex field by the difference");
    const D2D1_RECT_F fresh   = picker.GetNewSwatchRect();
    const D2D1_RECT_F current = picker.GetCurrentSwatchRect();
    Require(std::fabs((fresh.right - fresh.left) - 84.0f) < 0.01f && std::fabs((current.right - current.left) - 84.0f) < 0.01f && current.left > fresh.right,
            "both swatches, and the captions beneath them, take the supplied width side by side");

    // An absurd width is bounded: the hex field keeps the rest of its row (none), never a negative or huge extent.
    labels.channelLabelWidthDip = std::numeric_limits<float>::max();
    labels.hexLabelWidthDip     = 1.0e30f;
    picker.SetLabels(labels);
    Require(picker.GetLabels().channelLabelWidthDip == ColorPicker::kMaxLabelWidthDip && picker.GetLabels().hexLabelWidthDip == ColorPicker::kMaxLabelWidthDip,
            "caption widths are bounded");
    const D2D1_RECT_F hexField = picker.HexField().GetBounds();
    Require(hexField.right >= hexField.left && hexField.right <= picker.GetBounds().right,
            "an oversized hex caption leaves the field an empty, in-bounds rectangle");
}

void TestColorPickerTypingKeepsTheEditedField()
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

    // The previewed hex field is not rewritten under the caret ("abcdef" -> "#ABCDEF" would leave the caret
    // before the last digit); focus loss normalizes it.
    TextField& hex = picker->HexField();
    TypeText(host, hex, L"abcdef");
    Require(picker->GetColor() == 0xFFABCDEFu && picker->RedField().GetValue() == 171.0, "six typed hex digits preview and resync the components");
    Require(hex.GetText() == L"abcdef" && hex.GetCaretIndex() == 6u, "the hex field keeps the typed text and caret while editing");
    host.SetFocusControl(nullptr);
    Require(hex.GetText() == L"#ABCDEF", "focus loss normalizes the hex text");

    // A typed component keeps its own edit open; Escape restores it and the picker follows it back.
    NumericStepper& red = picker->RedField();
    TypeText(host, red.Field(), L"12");
    Require(picker->GetColor() == 0xFF0CCDEFu && hex.GetText() == L"#0CCDEF", "a typed component previews and resyncs the hex field");
    Require(red.IsEditing() && red.Field().GetText() == L"12", "the typed component keeps its edit open");
    Require(red.Field().OnKeyDown(host, VK_ESCAPE, 0), "Escape cancels the component edit");
    Require(red.GetValue() == 171.0 && picker->GetColor() == 0xFFABCDEFu, "the picker follows the canceled component back");
    Require(events.lastArgb == 0xFFABCDEFu && events.Count(ColorPickerChangePhase::Cancel) == 0u, "a component cancel previews the restored color");
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

// ── Moving a control ─────────────────────────────────────────────────────

// A control inherits its flow direction and density through its parents. A parent's own change is announced to its
// children, but a move announced nothing, so a control that arranges itself for them (steppers, pickers, tab headers,
// stack layouts, row heights) kept the arrangement of its old place. Control::Reparent announces what differs. A
// control moves by leaving its parent's slot (Panel::GetChildren hands out the owning pointers) and becoming a
// host's root or a page, while the old parent and host still exist.

using NamedRects = std::vector<std::pair<std::string, D2D1_RECT_F>>;

void AppendStepperRects(NamedRects& rects, const std::string& name, NumericStepper& stepper)
{
    rects.emplace_back(name, stepper.GetBounds());
    rects.emplace_back(name + " field", stepper.Field().GetBounds());
    rects.emplace_back(name + " increase", stepper.IncrementButton().GetBounds());
    rects.emplace_back(name + " decrease", stepper.DecrementButton().GetBounds());
}

// Every rectangle a picker arranges or computes, named for a failure message.
[[nodiscard]] NamedRects MeasurePicker(ColorPicker& picker)
{
    NamedRects rects;
    rects.emplace_back("picker", picker.GetBounds());
    rects.emplace_back("field", picker.GetFieldRect());
    rects.emplace_back("hue strip", picker.GetHueStripRect());
    rects.emplace_back("new swatch", picker.GetNewSwatchRect());
    rects.emplace_back("current swatch", picker.GetCurrentSwatchRect());
    AppendStepperRects(rects, "red", picker.RedField());
    AppendStepperRects(rects, "green", picker.GreenField());
    AppendStepperRects(rects, "blue", picker.BlueField());
    rects.emplace_back("hex", picker.HexField().GetBounds());
    rects.emplace_back("ok", picker.OkButton().GetBounds());
    rects.emplace_back("cancel", picker.CancelButton().GetBounds());
    return rects;
}

[[nodiscard]] NamedRects MeasureStepper(NumericStepper& stepper)
{
    NamedRects rects;
    AppendStepperRects(rects, "stepper", stepper);
    return rects;
}

[[nodiscard]] NamedRects MeasureTabs(TabControl& tabs)
{
    NamedRects rects;
    rects.emplace_back("tab control", tabs.GetBounds());
    for (size_t index = 0u; index < tabs.GetTabCount(); ++index)
    {
        rects.emplace_back(std::format("tab {}", index), tabs.DebugGetTabRect(index));
        if (tabs.IsTabClosable(index))
        {
            rects.emplace_back(std::format("close {}", index), tabs.DebugGetCloseButtonRect(index));
        }
    }
    rects.emplace_back("back button", tabs.DebugGetBackButtonRect());
    rects.emplace_back("forward button", tabs.DebugGetForwardButtonRect());
    rects.emplace_back("header divider", tabs.DebugGetHeaderDividerRect());
    rects.emplace_back("header scroll", D2D1::RectF(tabs.DebugGetHeaderScrollOffsetDip(), 0.0f, 0.0f, 0.0f));
    if (const Control* const page = tabs.GetSelectedPage())
    {
        rects.emplace_back("selected page", page->GetBounds());
    }
    return rects;
}

[[nodiscard]] NamedRects MeasureMenuBar(MenuBar& menuBar, ControlHost& host)
{
    NamedRects rects;
    for (size_t index = 0u; index < menuBar.GetItems().size(); ++index)
    {
        RECT rect{};
        if (menuBar.TryGetItemScreenRect(host, index, rect))
        {
            rects.emplace_back(
                std::format("item {}", index),
                D2D1::RectF(static_cast<float>(rect.left), static_cast<float>(rect.top), static_cast<float>(rect.right), static_cast<float>(rect.bottom)));
        }
    }
    return rects;
}

[[nodiscard]] NamedRects MeasureTextField(TextField& field, ControlHost& host)
{
    NamedRects rects;
    rects.emplace_back("text field", field.GetBounds());
    TextFieldDebugSingleLinePaintState state{};
    if (field.DebugGetSingleLinePaintState(host, state))
    {
        rects.emplace_back("text", state.textRect);
        rects.emplace_back("scroll", D2D1::RectF(state.horizontalScrollDip, 0.0f, 0.0f, 0.0f));
    }
    return rects;
}
// Empty when both are laid out alike; otherwise names every rectangle that differs.
[[nodiscard]] std::string DescribeGeometryDifference(const NamedRects& actual, const NamedRects& expected)
{
    std::string difference;
    if (actual.size() != expected.size())
    {
        return std::format("{} rectangles instead of {}; ", actual.size(), expected.size());
    }
    for (size_t index = 0u; index < actual.size(); ++index)
    {
        const D2D1_RECT_F& a = actual[index].second;
        const D2D1_RECT_F& e = expected[index].second;
        if (! RectNear(a, e))
        {
            difference += std::format("{} ({:.1f},{:.1f},{:.1f},{:.1f}) instead of ({:.1f},{:.1f},{:.1f},{:.1f}); ",
                                      actual[index].first,
                                      a.left,
                                      a.top,
                                      a.right,
                                      a.bottom,
                                      e.left,
                                      e.top,
                                      e.right,
                                      e.bottom);
        }
    }
    return difference;
}

// Empty when the captures are pixel-identical; otherwise the count and the box of the differing pixels.
[[nodiscard]] std::string DescribeBitmapDifference(const WindowHostBitmapCapture& actual, const WindowHostBitmapCapture& expected)
{
    if (actual.widthPx != expected.widthPx || actual.heightPx != expected.heightPx || actual.bgraPixels.size() != expected.bgraPixels.size())
    {
        return std::format("{}x{} px instead of {}x{}", actual.widthPx, actual.heightPx, expected.widthPx, expected.heightPx);
    }
    size_t differing = 0u;
    UINT left        = actual.widthPx;
    UINT top         = actual.heightPx;
    UINT right       = 0u;
    UINT bottom      = 0u;
    for (UINT y = 0u; y < actual.heightPx; ++y)
    {
        for (UINT x = 0u; x < actual.widthPx; ++x)
        {
            const size_t offset = (static_cast<size_t>(y) * actual.widthPx + x) * 4u;
            if (std::memcmp(&actual.bgraPixels[offset], &expected.bgraPixels[offset], 4u) != 0)
            {
                ++differing;
                left   = (std::min)(left, x);
                top    = (std::min)(top, y);
                right  = (std::max)(right, x);
                bottom = (std::max)(bottom, y);
            }
        }
    }
    return differing == 0u ? std::string{} : std::format("{} pixels differ inside ({},{})-({},{})", differing, left, top, right, bottom);
}

[[nodiscard]] size_t CountPixelsDifferingFromCorner(const WindowHostBitmapCapture& capture) noexcept
{
    size_t count = 0u;
    for (size_t offset = 4u; offset + 3u < capture.bgraPixels.size(); offset += 4u)
    {
        count += std::memcmp(&capture.bgraPixels[offset], capture.bgraPixels.data(), 4u) != 0 ? 1u : 0u;
    }
    return count;
}

WindowHostBitmapCapture CaptureWindow(AttachedHostWindow& window, const char* context)
{
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();
    WindowHostBitmapCapture capture;
    Require(window.Host().DebugCaptureBitmap(capture), context);
    return capture;
}

// A host with a theme, a density and a dpi the system did not choose, whose client area is exactly `sizeDip`.
void ConfigureHostPlace(AttachedHostWindow& window, UINT dpi, bool dark, Density density, D2D1_SIZE_F sizeDip)
{
    ThemePalette theme  = MakeDefaultThemePalette(dark);
    theme.reducedMotion = true;
    theme.density       = density;
    window.Host().SetTheme(theme);
    RECT outer{};
    GetWindowRect(window.Hwnd(), &outer);
    bool handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_DPICHANGED, MAKELONG(dpi, dpi), reinterpret_cast<LPARAM>(&outer), handled));
    Require(handled && window.Host().GetDpi() == static_cast<float>(dpi), "the host takes the requested dpi");
    RECT client{};
    GetWindowRect(window.Hwnd(), &outer);
    GetClientRect(window.Hwnd(), &client);
    const float scale = static_cast<float>(dpi) / 96.0f;
    SetWindowPos(window.Hwnd(),
                 nullptr,
                 0,
                 0,
                 (outer.right - outer.left) - (client.right - client.left) + static_cast<LONG>(std::lround(sizeDip.width * scale)),
                 (outer.bottom - outer.top) - (client.bottom - client.top) + static_cast<LONG>(std::lround(sizeDip.height * scale)),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    const D2D1_RECT_F clientDip = window.Host().GetClientBoundsDip();
    Require(std::fabs(clientDip.right - sizeDip.width) < 0.01f && std::fabs(clientDip.bottom - sizeDip.height) < 0.01f,
            "the host's client area is the requested size");
}

[[nodiscard]] std::unique_ptr<Control> TakeChild(Panel& parent, size_t index)
{
    return std::move(parent.GetChildren()[index]);
}

[[nodiscard]] wil::com_ptr<ID2D1Device> DirectDeviceOf(ControlHost& host)
{
    wil::com_ptr<ID2D1Device> device;
    if (ID2D1DeviceContext* const context = host.GetDeviceContext())
    {
        context->GetDevice(device.put());
    }
    return device;
}

void PaintAndCachePicker(AttachedHostWindow& window, ColorPicker& picker, const char* context)
{
    static_cast<void>(CaptureWindow(window, context));
    Require(picker.DebugHasCachedBrushes(), "painting the picker caches its gradients");
}

struct MovePlace
{
    UINT dpi;
    bool dark;
    Density density;
};

struct MoveScenario
{
    const char* name;
    bool oldParentRightToLeft;
    bool newDevice; // The new host draws on a Direct2D device made after the control painted for the old one.
    MovePlace from;
    MovePlace to;
};

// Moves a control made by `configure` from one window host to a second, whose client area is the control's size (so
// setting the root changes no bounds: only the control itself can notice the move), and compares its arrangement
// (`measure`) and the painted window with a fresh control made the same way in the second host.
template <typename TControl, typename Configure, typename Measure>
void ExpectMovedControlMatchesAFreshOne(const char* control, D2D1_SIZE_F size, std::span<const MoveScenario> scenarios, Configure configure, Measure measure)
{
    for (const MoveScenario& scenario : scenarios)
    {
        AttachedHostWindow oldWindow;
        AttachedHostWindow newWindow;
        ConfigureHostPlace(oldWindow, scenario.from.dpi, scenario.from.dark, scenario.from.density, size);
        ConfigureHostPlace(newWindow, scenario.to.dpi, scenario.to.dark, scenario.to.density, size);

        auto oldRoot = std::make_unique<Panel>();
        if (scenario.oldParentRightToLeft)
        {
            oldRoot->SetFlowDirection(FlowDirection::RightToLeft);
        }
        Panel* const oldPanel = oldRoot.get();
        auto* moved           = oldRoot->AddChild<TControl>();
        configure(*moved);
        moved->SetBounds(newWindow.Host().GetClientBoundsDip());
        oldWindow.Host().SetRoot(std::move(oldRoot));
        static_cast<void>(CaptureWindow(oldWindow, "the control paints in its first host"));
        const wil::com_ptr<ID2D1Device> oldDevice = DirectDeviceOf(oldWindow.Host());
        Require(oldDevice != nullptr, "the first host draws on a Direct2D device");
        if (scenario.newDevice)
        {
            // Hidden, the first host stays on its device while the loss makes the next host build another.
            ShowWindow(oldWindow.Hwnd(), SW_HIDE);
            newWindow.Host().DebugSimulateDeviceLoss();
        }

        newWindow.Host().SetRoot(TakeChild(*oldPanel, 0u));
        Require(newWindow.Host().GetRoot() == moved, "the second host holds the moved control");
        const NamedRects movedRects              = measure(*moved, newWindow.Host());
        const WindowHostBitmapCapture movedImage = CaptureWindow(newWindow, "the moved control paints in its new host");
        Require(CountPixelsDifferingFromCorner(movedImage) > 200u, "the moved control painted something");
        Require(! scenario.newDevice || DirectDeviceOf(newWindow.Host()) != oldDevice, "the second host draws on another Direct2D device");

        auto created = std::make_unique<TControl>();
        configure(*created);
        TControl* const fresh = created.get();
        newWindow.Host().SetRoot(std::move(created));
        const NamedRects freshRects              = measure(*fresh, newWindow.Host());
        const WindowHostBitmapCapture freshImage = CaptureWindow(newWindow, "a fresh control paints in the new host");

        const std::string geometryDifference = DescribeGeometryDifference(movedRects, freshRects);
        const std::string imageDifference    = DescribeBitmapDifference(movedImage, freshImage);
        if (! geometryDifference.empty() || ! imageDifference.empty())
        {
            std::cerr << "moved " << control << ", " << scenario.name << ": rectangles: " << (geometryDifference.empty() ? "same" : geometryDifference)
                      << " | pixels: " << (imageDifference.empty() ? "same" : imageDifference) << '\n';
        }
        Require(geometryDifference.empty(), "a moved control arranges itself like a fresh one");
        Require(imageDifference.empty(), "a moved control paints like a fresh one");
    }
}

// A picker that leaves a right-to-left parent is arranged for its new, left-to-right place, as a picker created there
// is: it kept its children where a right-to-left flow puts them, overlapping the field, strip and swatches its own
// rectangles (computed live) had already turned around.
void TestColorPickerMovedOutOfARightToLeftParentIsArrangedForItsNewPlace()
{
    const D2D1_RECT_F bounds = D2D1::RectF(0.0f, 0.0f, ColorPicker::kDefaultWidthDip, ColorPicker::kDefaultHeightDip);
    const auto expectFresh   = [&bounds](ColorPicker& moved, const char* context, FlowDirection freshFlow, bool explicitFlow)
    {
        PageHost freshPage;
        freshPage.SetBounds(bounds);
        auto created = std::make_unique<ColorPicker>();
        if (explicitFlow)
        {
            created->SetFlowDirection(freshFlow);
        }
        freshPage.SetPage(std::move(created));
        auto* fresh                  = static_cast<ColorPicker*>(freshPage.GetPage());
        const std::string difference = DescribeGeometryDifference(MeasurePicker(moved), MeasurePicker(*fresh));
        if (! difference.empty())
        {
            std::cerr << context << ": " << difference << '\n';
        }
        Require(difference.empty(), context);
    };

    auto oldRoot = std::make_unique<Panel>();
    oldRoot->SetFlowDirection(FlowDirection::RightToLeft);
    auto* picker = oldRoot->AddChild<ColorPicker>();
    picker->SetBounds(bounds);
    Require(picker->RedField().GetBounds().right <= picker->GetHueStripRect().left, "the picker inherits right-to-left flow: the steppers precede the strip");
    const NamedRects inherited = MeasurePicker(*picker);

    PageHost page;
    page.SetBounds(bounds);
    page.SetPage(TakeChild(*oldRoot, 0u));
    Require(page.GetPage() == picker && ! picker->IsRightToLeft(), "the page holds the picker, no longer in right-to-left flow");
    Require(picker->RedField().GetBounds().left >= picker->GetHueStripRect().right, "the steppers follow the strip again");
    expectFresh(*picker, "a picker moved out of a right-to-left parent is arranged like one created in its page", FlowDirection::LeftToRight, false);
    Require(! DescribeGeometryDifference(MeasurePicker(*picker), inherited).empty(), "the arrangement did change with the flow direction");

    // An explicit flow direction travels with the picker: moving it changes nothing it inherits.
    auto explicitRoot = std::make_unique<Panel>();
    auto* explicitRtl = explicitRoot->AddChild<ColorPicker>();
    explicitRtl->SetFlowDirection(FlowDirection::RightToLeft);
    explicitRtl->SetBounds(bounds);
    PageHost explicitPage;
    explicitPage.SetBounds(bounds);
    explicitPage.SetPage(TakeChild(*explicitRoot, 0u));
    Require(explicitRtl->IsRightToLeft(), "an explicit right-to-left flow survives the move");
    expectFresh(*explicitRtl, "a moved picker with an explicit flow is arranged like a fresh one with it", FlowDirection::RightToLeft, true);
}

constexpr MovePlace kBasePlace{96u, false, Density::Standard};
constexpr MoveScenario kEveryPlace[] = {
    {"same metrics", false, false, kBasePlace, kBasePlace},
    {"a larger dpi", false, false, kBasePlace, {192u, false, Density::Standard}},
    {"a smaller dpi", false, false, {144u, false, Density::Standard}, kBasePlace},
    {"another theme", false, false, kBasePlace, {96u, true, Density::Standard}},
    {"compact density", false, false, kBasePlace, {96u, false, Density::Compact}},
    {"standard density", false, false, {96u, false, Density::Compact}, kBasePlace},
    {"another Direct2D device", false, true, kBasePlace, kBasePlace},
    {"a right-to-left parent", true, false, kBasePlace, kBasePlace},
    {"a right-to-left parent, another device and every metric", true, true, {96u, false, Density::Compact}, {144u, true, Density::Standard}},
};
constexpr MoveScenario kRightToLeftPlaces[] = {
    {"same metrics", false, false, kBasePlace, kBasePlace},
    {"a right-to-left parent", true, false, kBasePlace, kBasePlace},
    {"a right-to-left parent, another device and every metric", true, true, {96u, false, Density::Compact}, {144u, true, Density::Standard}},
};

// A picker across window hosts that differ in dpi, theme, density and Direct2D device, and out of a right-to-left
// parent: what a host shows a control must not stay behind from the previous host.
void TestColorPickerMovedBetweenHostsMatchesAFreshOne()
{
    const auto configure = [](ColorPicker& picker)
    {
        picker.SetColor(0xFF3A7BD5u);
        picker.SetCurrentColor(0xFFC04030u);
    };
    const auto measure = [](ColorPicker& picker, ControlHost&) { return MeasurePicker(picker); };
    ExpectMovedControlMatchesAFreshOne<ColorPicker>(
        "color picker", D2D1::SizeF(ColorPicker::kDefaultWidthDip, ColorPicker::kDefaultHeightDip), kEveryPlace, configure, measure);
}

// A stepper keeps its field and buttons where its flow direction put them (its label and unit follow the field).
void TestNumericStepperMovedBetweenHostsMatchesAFreshOne()
{
    const auto configure = [](NumericStepper& stepper)
    {
        stepper.SetLabel(L"X", 20.0f);
        stepper.SetUnit(L"px", 24.0f);
        stepper.SetValue(42.0);
    };
    const auto measure = [](NumericStepper& stepper, ControlHost&) { return MeasureStepper(stepper); };
    ExpectMovedControlMatchesAFreshOne<NumericStepper>("numeric stepper", D2D1::SizeF(240.0f, 40.0f), kRightToLeftPlaces, configure, measure);
}

// A tab control across window hosts. Its own host change already invalidates its header layout, so this held before
// the announcement; the hostless move below is the one that needs it.
void TestTabControlMovedBetweenHostsMatchesAFreshOne()
{
    const auto configure = [](TabControl& tabs)
    {
        tabs.AddTab<Label>(L"General", L"General page");
        tabs.AddTab<Label>(L"Advanced settings", L"Advanced page");
        tabs.AddTab<Label>(L"About", L"About page");
        tabs.SetTabClosable(1u, true);
    };
    const auto measure = [](TabControl& tabs, ControlHost&) { return MeasureTabs(tabs); };
    ExpectMovedControlMatchesAFreshOne<TabControl>("tab control", D2D1::SizeF(360.0f, 120.0f), kRightToLeftPlaces, configure, measure);
}

// The menu bar and the text field key their layouts on the flow direction (and the host, bounds and metrics), so they
// never went stale; they are here to show that the announcement leaves such controls right.
void TestMenuBarMovedBetweenHostsMatchesAFreshOne()
{
    const auto configure = [](MenuBar& menuBar)
    {
        menuBar.SetItems({
            MenuBarItem{.text = L"File", .mnemonic = L'F'},
            MenuBarItem{.text = L"Edit", .mnemonic = L'E'},
            MenuBarItem{.text = L"Help", .mnemonic = L'H', .rightJustified = true},
        });
    };
    const auto measure = [](MenuBar& menuBar, ControlHost& host) { return MeasureMenuBar(menuBar, host); };
    ExpectMovedControlMatchesAFreshOne<MenuBar>("menu bar", D2D1::SizeF(320.0f, 32.0f), kRightToLeftPlaces, configure, measure);
}

void TestTextFieldMovedBetweenHostsMatchesAFreshOne()
{
    const auto configure = [](TextField& field) { field.SetText(L"Move me \u0645\u0631\u062D\u0628\u0627 123"); };
    const auto measure   = [](TextField& field, ControlHost& host) { return MeasureTextField(field, host); };
    ExpectMovedControlMatchesAFreshOne<TextField>("text field", D2D1::SizeF(240.0f, 40.0f), kRightToLeftPlaces, configure, measure);
}

// The gradients (and the Direct2D device reference that keys them) belong to the host painted for. A picker that leaves
// its host used to keep them, and so the old host's device, until it next painted somewhere.
void TestColorPickerReleasesItsGradientsWhenItsHostChanges()
{
    const D2D1_SIZE_F size = D2D1::SizeF(ColorPicker::kDefaultWidthDip, ColorPicker::kDefaultHeightDip);
    AttachedHostWindow oldWindow;
    AttachedHostWindow newWindow;
    ConfigureHostPlace(oldWindow, 96u, false, Density::Standard, size);
    ConfigureHostPlace(newWindow, 96u, false, Density::Standard, size);

    auto oldRoot          = std::make_unique<Panel>();
    Panel* const oldPanel = oldRoot.get();
    auto* picker          = oldRoot->AddChild<ColorPicker>();
    picker->SetBounds(D2D1::RectF(0.0f, 0.0f, size.width, size.height));
    Require(! picker->DebugHasCachedBrushes(), "a picker that never painted holds no gradients");
    oldWindow.Host().SetRoot(std::move(oldRoot));
    PaintAndCachePicker(oldWindow, *picker, "the picker paints in its first host");

    // A change of the parent's flow keeps them: the device is still the host's.
    oldPanel->SetFlowDirection(FlowDirection::RightToLeft);
    static_cast<void>(CaptureWindow(oldWindow, "the picker repaints after its parent's flow changed"));
    Require(picker->DebugHasCachedBrushes(), "a flow change keeps the gradients");

    newWindow.Host().SetRoot(TakeChild(*oldPanel, 0u));
    Require(! picker->DebugHasCachedBrushes(), "leaving the first host releases the gradients and its device reference");
    static_cast<void>(CaptureWindow(newWindow, "the picker paints in the second host"));
    Require(picker->DebugHasCachedBrushes(), "the second host's first paint makes the gradients again");

    // A picker parked in a page that has no host holds no device either.
    auto parkedRoot          = std::make_unique<Panel>();
    Panel* const parkedPanel = parkedRoot.get();
    auto* parked             = parkedRoot->AddChild<ColorPicker>();
    parked->SetBounds(D2D1::RectF(0.0f, 0.0f, size.width, size.height));
    newWindow.Host().SetRoot(std::move(parkedRoot));
    PaintAndCachePicker(newWindow, *parked, "the picker paints under a panel root");
    PageHost parking;
    parking.SetBounds(D2D1::RectF(0.0f, 0.0f, size.width, size.height));
    parking.SetPage(TakeChild(*parkedPanel, 0u));
    Require(parking.GetPage() == parked && ! parked->DebugHasCachedBrushes(), "a picker parked without a host releases the gradients");
}

// A control moved from a right-to-left panel into a page that has no host, so neither its host nor its bounds change:
// only the announcement can tell it where it now is. It is compared with one made the same way in another page.
template <typename TControl, typename Configure, typename Measure>
void ExpectControlMovedToAPageMatchesAFreshOne(const char* control, const D2D1_RECT_F& bounds, Configure configure, Measure measure)
{
    auto oldRoot = std::make_unique<Panel>();
    oldRoot->SetFlowDirection(FlowDirection::RightToLeft);
    auto* moved = oldRoot->AddChild<TControl>();
    moved->SetBounds(bounds);
    configure(*moved);
    const NamedRects inherited = measure(*moved);

    PageHost page;
    page.SetBounds(bounds);
    page.SetPage(TakeChild(*oldRoot, 0u));
    Require(page.GetPage() == moved && ! moved->IsRightToLeft(), "the page holds the control, no longer in right-to-left flow");

    auto created = std::make_unique<TControl>();
    created->SetBounds(bounds);
    configure(*created);
    PageHost freshPage;
    freshPage.SetBounds(bounds);
    freshPage.SetPage(std::move(created));
    const std::string difference = DescribeGeometryDifference(measure(*moved), measure(*static_cast<TControl*>(freshPage.GetPage())));
    if (! difference.empty())
    {
        std::cerr << "moved " << control << " (page): " << difference << '\n';
    }
    Require(difference.empty(), "a control moved out of a right-to-left parent is arranged like one created in its page");
    Require(! DescribeGeometryDifference(measure(*moved), inherited).empty(), "the arrangement did change with the flow direction");
}

// A horizontal stack lays its children out from the right in right-to-left flow, and only when told to. It is told
// when its flow direction changes, and a move changes it.
void TestStackPanelMovedOutOfARightToLeftParentIsLaidOutForItsNewPlace()
{
    const auto configure = [](StackPanel& stack)
    {
        stack.SetOrientation(StackOrientation::Horizontal);
        stack.SetGap(8.0f);
        auto* first  = stack.AddChild<Button>(L"One");
        auto* second = stack.AddChild<Button>(L"Two");
        stack.SetChildExtent(first, 70.0f);
        stack.SetChildExtent(second, 90.0f);
        stack.ApplyLayout();
    };
    const auto measure = [](StackPanel& stack)
    {
        NamedRects rects;
        for (size_t index = 0u; index < stack.GetChildren().size(); ++index)
        {
            rects.emplace_back(std::format("child {}", index), stack.GetChildren()[index]->GetBounds());
        }
        return rects;
    };
    ExpectControlMovedToAPageMatchesAFreshOne<StackPanel>("stack panel", D2D1::RectF(0.0f, 0.0f, 240.0f, 40.0f), configure, measure);
}

// A tab control's header layout (tab and close-button rectangles, overflow buttons) is cached and invalidated by name,
// not keyed on the flow direction. It repairs itself when its host changes (the moves across windows above never
// went stale), but not when a move keeps it hostless.
void TestTabControlMovedOutOfARightToLeftParentIsLaidOutForItsNewPlace()
{
    const auto configure = [](TabControl& tabs)
    {
        tabs.AddTab<Label>(L"General", L"General page");
        tabs.AddTab<Label>(L"Advanced settings", L"Advanced page");
        tabs.AddTab<Label>(L"About", L"About page");
        tabs.SetTabClosable(1u, true);
    };
    const auto measure = [](TabControl& tabs) { return MeasureTabs(tabs); };
    ExpectControlMovedToAPageMatchesAFreshOne<TabControl>("tab control", D2D1::RectF(0.0f, 0.0f, 360.0f, 120.0f), configure, measure);
}

// Row heights depend on density and are made when it is announced (and when the row height is set). A tree or grid moved
// from a compact parent to a standard place, or from a standard one into a compact host, must have the row metrics a
// tree or grid given its row height in that place has. The oracle is configured after it stands in its place, so it
// does not depend on the announcement under test.
template <typename TControl, typename Configure, typename Measure>
void ExpectMovedRowMetricsMatchAFreshOne(const char* control, Configure configure, Measure measure)
{
    struct DensityScenario
    {
        const char* name;
        bool oldParentCompact;
        Density oldHost;
        Density newHost;
    };
    const DensityScenario scenarios[] = {
        {"a compact parent into a standard host", true, Density::Standard, Density::Standard},
        {"a standard parent into a compact host", false, Density::Standard, Density::Compact},
        {"a compact parent into a compact host", true, Density::Standard, Density::Compact},
        {"a compact host into a standard host", false, Density::Compact, Density::Standard},
        {"the same standard density", false, Density::Standard, Density::Standard},
    };
    const D2D1_RECT_F bounds = D2D1::RectF(0.0f, 0.0f, 300.0f, 200.0f);
    const auto themeOf       = [](Density density)
    {
        ThemePalette theme = MakeDefaultThemePalette(false);
        theme.density      = density;
        return theme;
    };
    std::optional<NamedRects> standardRows;
    std::optional<NamedRects> compactRows;
    for (const DensityScenario& scenario : scenarios)
    {
        WindowHost oldHost;
        WindowHost newHost;
        oldHost.SetTheme(themeOf(scenario.oldHost));
        newHost.SetTheme(themeOf(scenario.newHost));

        auto oldRoot = std::make_unique<Panel>();
        if (scenario.oldParentCompact)
        {
            oldRoot->SetDensity(Density::Compact);
        }
        Panel* const oldPanel = oldRoot.get();
        auto* moved           = oldRoot->AddChild<TControl>();
        configure(*moved);
        moved->SetBounds(bounds);
        oldHost.SetRoot(std::move(oldRoot));

        newHost.SetRoot(TakeChild(*oldPanel, 0u));
        moved->SetBounds(bounds);
        const NamedRects movedRows = measure(*moved, newHost);

        auto created          = std::make_unique<TControl>();
        TControl* const fresh = created.get();
        newHost.SetRoot(std::move(created));
        configure(*fresh);
        fresh->SetBounds(bounds);
        const NamedRects freshRows        = measure(*fresh, newHost);
        std::optional<NamedRects>& oracle = scenario.newHost == Density::Compact ? compactRows : standardRows;
        oracle                            = freshRows;

        const std::string difference = DescribeGeometryDifference(movedRows, freshRows);
        if (! difference.empty())
        {
            std::cerr << "moved " << control << ", " << scenario.name << ": " << difference << '\n';
        }
        Require(difference.empty(), "a moved control has the row metrics of one configured in its new place");
    }
    Require(compactRows.has_value() && standardRows.has_value() && ! DescribeGeometryDifference(*compactRows, *standardRows).empty(),
            "the densities this test moves between give different row metrics");
}

void TestTreeMovedBetweenDensitiesMatchesAFreshOne()
{
    MutableTreeModel treeModel;
    treeModel.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Viewers"},
    });
    const auto configure = [&treeModel](Tree& tree)
    {
        tree.SetRowHeightDip(30.0f);
        tree.SetModel(&treeModel);
    };
    const auto measure = [](Tree& tree, ControlHost& host)
    {
        NamedRects rows;
        rows.emplace_back("row 0", tree.GetItemLayoutMetrics(host, 0u).rowRect);
        rows.emplace_back("row 1", tree.GetItemLayoutMetrics(host, 1u).rowRect);
        return rows;
    };
    ExpectMovedRowMetricsMatchAFreshOne<Tree>("tree", configure, measure);
}

void TestGridMovedBetweenDensitiesMatchesAFreshOne()
{
    MultiRowGridModel gridModel(4u);
    const auto configure = [&gridModel](Grid& grid)
    {
        grid.SetRowHeightDip(46.0f);
        grid.SetHeaderHeightDip(30.0f);
        grid.SetModel(&gridModel);
    };
    const auto measure = [](Grid& grid, ControlHost& host)
    {
        NamedRects rows;
        rows.emplace_back("cell 0", grid.GetCellLayoutMetrics(host, 0u, 0u).cellRect);
        rows.emplace_back("cell 1", grid.GetCellLayoutMetrics(host, 1u, 0u).cellRect);
        return rows;
    };
    ExpectMovedRowMetricsMatchAFreshOne<Grid>("grid", configure, measure);
}

// Counts the announcements a control hears in counters the test owns (a move may destroy the control), and where its
// host stood when it heard them.
class AnnouncementProbe final : public Panel
{
public:
    struct Heard
    {
        size_t flowDirection     = 0u;
        size_t density           = 0u;
        ControlHost* flowHost    = nullptr;
        ControlHost* densityHost = nullptr;
    };

    explicit AnnouncementProbe(Heard* heard) noexcept : _heard(heard)
    {
    }

protected:
    void OnFlowDirectionChanged() noexcept override
    {
        ++_heard->flowDirection;
        _heard->flowHost = GetHost();
        Panel::OnFlowDirectionChanged();
    }

    void OnDensityChanged() noexcept override
    {
        ++_heard->density;
        _heard->densityHost = GetHost();
        Panel::OnDensityChanged();
    }

private:
    Heard* _heard;
};

// A move announces the flow direction and density the control now inherits, as a parent's own change is announced to
// its children: once, once it stands in its final place, and only when a value differs.
void TestMovingAControlAnnouncesWhatItNowInheritsOnce()
{
    using Heard             = AnnouncementProbe::Heard;
    const auto densityTheme = [](Density density)
    {
        ThemePalette theme = MakeDefaultThemePalette(false);
        theme.density      = density;
        return theme;
    };
    const auto heardNothing = [](const Heard& heard) noexcept { return heard.flowDirection == 0u && heard.density == 0u; };

    // A child that inherits what its parent has hears nothing, however deep it is added.
    {
        Panel parent;
        Heard child;
        Heard grandchild;
        auto* probe = parent.AddChild<AnnouncementProbe>(&child);
        probe->AddChild<AnnouncementProbe>(&grandchild);
        Require(heardNothing(child) && heardNothing(grandchild), "adding children that inherit what their parents have announces nothing");
    }

    // Under a right-to-left, compact parent a new child, and a child of that child, hear both once.
    {
        Panel parent;
        parent.SetFlowDirection(FlowDirection::RightToLeft);
        parent.SetDensity(Density::Compact);
        Heard child;
        Heard grandchild;
        auto* probe = parent.AddChild<AnnouncementProbe>(&child);
        probe->AddChild<AnnouncementProbe>(&grandchild);
        Require(child.flowDirection == 1u && child.density == 1u, "a child added under a right-to-left, compact parent hears both once");
        Require(grandchild.flowDirection == 1u && grandchild.density == 1u, "and so does its own child");
    }

    // Moving out of such a parent: the moved control hears both once, and its child hears each once, through it.
    {
        Panel oldParent;
        oldParent.SetFlowDirection(FlowDirection::RightToLeft);
        oldParent.SetDensity(Density::Compact);
        Heard moved;
        Heard inner;
        auto* probe = oldParent.AddChild<AnnouncementProbe>(&moved);
        probe->AddChild<AnnouncementProbe>(&inner);
        moved = Heard{};
        inner = Heard{};
        PageHost page;
        page.SetPage(TakeChild(oldParent, 0u));
        Require(moved.flowDirection == 1u && moved.density == 1u, "a control moved out of a right-to-left, compact parent hears both once");
        Require(inner.flowDirection == 1u && inner.density == 1u, "and its child hears each once, through it");
    }

    // A child with its own density is not told about its parent's: it inherits nothing.
    {
        Panel oldParent;
        oldParent.SetDensity(Density::Compact);
        Heard moved;
        Heard own;
        auto* probe = oldParent.AddChild<AnnouncementProbe>(&moved);
        auto* inner = probe->AddChild<AnnouncementProbe>(&own);
        inner->SetDensity(Density::Compact);
        moved = Heard{};
        own   = Heard{};
        PageHost page;
        page.SetPage(TakeChild(oldParent, 0u));
        Require(moved.density == 1u && own.density == 0u, "a child with its own density hears nothing of its parent's move");
    }

    // A control with its own flow direction and density inherits neither, so a move changes nothing for it.
    {
        Panel oldParent;
        oldParent.SetFlowDirection(FlowDirection::RightToLeft);
        oldParent.SetDensity(Density::Compact);
        Heard moved;
        auto* probe = oldParent.AddChild<AnnouncementProbe>(&moved);
        probe->SetFlowDirection(FlowDirection::RightToLeft);
        probe->SetDensity(Density::Compact);
        moved = Heard{};
        PageHost page;
        page.SetPage(TakeChild(oldParent, 0u));
        Require(heardNothing(moved), "a control with its own flow direction and density hears nothing when it moves");
    }

    // Places that give the same values announce nothing, into a page and into a host's root alike.
    {
        Panel oldParent;
        Heard moved;
        oldParent.AddChild<AnnouncementProbe>(&moved);
        WindowHost host;
        host.SetTheme(densityTheme(Density::Standard));
        host.SetRoot(TakeChild(oldParent, 0u));
        Require(heardNothing(moved), "moving between places with the same flow direction and density announces nothing");
    }

    // A root takes its host's density: a compact host announces it when the root arrives, once, with the host in place.
    {
        WindowHost host;
        host.SetTheme(densityTheme(Density::Compact));
        Heard root;
        host.SetRoot(std::make_unique<AnnouncementProbe>(&root));
        Require(root.flowDirection == 0u && root.density == 1u, "a root arriving in a compact host hears its density once");
        Require(root.densityHost == &host, "and hears it standing in that host");
    }

    // Out of a right-to-left parent into a host: the direction is heard once, standing in the new host.
    {
        WindowHost oldHost;
        WindowHost newHost;
        auto oldRoot = std::make_unique<Panel>();
        oldRoot->SetFlowDirection(FlowDirection::RightToLeft);
        Panel* const oldPanel = oldRoot.get();
        Heard moved;
        oldRoot->AddChild<AnnouncementProbe>(&moved);
        oldHost.SetRoot(std::move(oldRoot));
        moved = Heard{};
        newHost.SetRoot(TakeChild(*oldPanel, 0u));
        Require(moved.flowDirection == 1u && moved.density == 0u && moved.flowHost == &newHost,
                "a control moved into another host hears the direction it now inherits once, in place");
    }

    // The intermediate places do not count: from a compact parent into a compact host the density never differs, though
    // the control passes through a place with no host on the way.
    {
        WindowHost oldHost;
        WindowHost newHost;
        newHost.SetTheme(densityTheme(Density::Compact));
        auto oldRoot = std::make_unique<Panel>();
        oldRoot->SetDensity(Density::Compact);
        Panel* const oldPanel = oldRoot.get();
        Heard moved;
        oldRoot->AddChild<AnnouncementProbe>(&moved);
        oldHost.SetRoot(std::move(oldRoot));
        moved = Heard{};
        newHost.SetRoot(TakeChild(*oldPanel, 0u));
        Require(heardNothing(moved), "a control whose density is compact before and after hears nothing on the way");
    }

    // Tearing down announces nothing, though a root leaving a compact host stops inheriting its density.
    {
        WindowHost host;
        host.SetTheme(densityTheme(Density::Compact));
        Heard root;
        Heard child;
        auto rootProbe = std::make_unique<AnnouncementProbe>(&root);
        rootProbe->AddChild<AnnouncementProbe>(&child);
        host.SetRoot(std::move(rootProbe));
        root  = Heard{};
        child = Heard{};
        host.SetRoot(nullptr);
        Require(heardNothing(root) && heardNothing(child), "replacing the root announces nothing to the tree it drops");

        Panel panel;
        Heard cleared;
        panel.SetFlowDirection(FlowDirection::RightToLeft);
        panel.AddChild<AnnouncementProbe>(&cleared);
        cleared = Heard{};
        panel.ClearChildren();
        Require(heardNothing(cleared), "clearing a panel announces nothing to its children");
    }
}

// A null slot dereference is a hardware fault, not an exception: report it as a failure instead of ending the run.
[[nodiscard]] bool ClearsWithoutFaulting(Panel& panel) noexcept
{
    __try
    {
        panel.ClearChildren();
        return true;
    }
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return false;
    }
}

// Panel::GetChildren hands out the owning pointers, so a child can be moved out and leaves a null slot. Everything a
// panel does with its children skips it: announcements, painting, hit testing, ticking, accessibility, focus and
// clearing (which dereferenced it).
void TestPanelSkipsTheSlotOfAChildMovedOutThroughGetChildren()
{
    AttachedHostWindow window;
    auto root          = std::make_unique<Panel>();
    Panel* const panel = root.get();
    auto* first        = root->AddChild<Button>(L"First");
    auto* second       = root->AddChild<Button>(L"Second");
    auto* third        = root->AddChild<Button>(L"Third");
    first->SetBounds(D2D1::RectF(10.0f, 10.0f, 110.0f, 40.0f));
    second->SetBounds(D2D1::RectF(10.0f, 50.0f, 110.0f, 80.0f));
    third->SetBounds(D2D1::RectF(10.0f, 90.0f, 110.0f, 120.0f));
    window.Host().SetRoot(std::move(root));
    static_cast<void>(CaptureWindow(window, "the panel paints with all its children"));

    PageHost page;
    page.SetBounds(D2D1::RectF(10.0f, 50.0f, 110.0f, 80.0f));
    page.SetPage(TakeChild(*panel, 1u));
    Require(page.GetPage() == second && panel->DebugChildCount() == 3u && ! panel->GetChildren()[1] && panel->GetLogicalChild(1u) == nullptr,
            "the second button left an empty slot behind");

    panel->SetFlowDirection(FlowDirection::RightToLeft);
    panel->SetDensity(Density::Compact);
    RECT windowRect{};
    GetWindowRect(window.Hwnd(), &windowRect);
    bool handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_DPICHANGED, MAKELONG(144u, 144u), reinterpret_cast<LPARAM>(&windowRect), handled));
    Require(handled, "the host announces its dpi to the panel around the empty slot");
    static_cast<void>(CaptureWindow(window, "the panel paints around the empty slot"));
    Require(window.Host().DebugHitTestControl(D2D1::Point2F(60.0f, 65.0f)) == panel, "where the second button was, a hit test finds the panel");
    Require(window.Host().DebugHitTestControl(D2D1::Point2F(60.0f, 25.0f)) == first, "and still finds the first button");
    static_cast<void>(window.Host().DebugAnimationTickForTest(GetTickCount64()));
    window.Host().RefreshAccessibilitySnapshot();
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled && window.Host().GetFocusControl() != nullptr, "tabbing skips the empty slot");

    Require(ClearsWithoutFaulting(*panel), "clearing the panel skips the empty slot");
    Require(panel->DebugChildCount() == 0u, "clearing the panel removes every slot");
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
    TestNumericStepperUnparseableEditRevertsAndDecimalsRound();
    TestNumericStepperEditsEndWithCommitOrCancel();
    TestSplitterStaysInsideAnExtentBelowItsMinimums();
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
    TestColorPickerTypingKeepsTheEditedField();
    TestNumericStepperStepButtonsCarryAutomationNames();
    TestColorPickerCaptionSlotsFollowTheLabelWidths();
    TestColorPickerDisabledAndPaint();
    TestColorPickerCommitCallbackCanReplaceRootSafely();
    TestColorPickerMovedOutOfARightToLeftParentIsArrangedForItsNewPlace();
    TestColorPickerReleasesItsGradientsWhenItsHostChanges();

    // Moving controls (Control::Reparent, Panel slots)
    TestColorPickerMovedBetweenHostsMatchesAFreshOne();
    TestNumericStepperMovedBetweenHostsMatchesAFreshOne();
    TestTabControlMovedBetweenHostsMatchesAFreshOne();
    TestMenuBarMovedBetweenHostsMatchesAFreshOne();
    TestTextFieldMovedBetweenHostsMatchesAFreshOne();
    TestStackPanelMovedOutOfARightToLeftParentIsLaidOutForItsNewPlace();
    TestTabControlMovedOutOfARightToLeftParentIsLaidOutForItsNewPlace();
    TestTreeMovedBetweenDensitiesMatchesAFreshOne();
    TestGridMovedBetweenDensitiesMatchesAFreshOne();
    TestMovingAControlAnnouncesWhatItNowInheritsOnce();
    TestPanelSkipsTheSlotOfAChildMovedOutThroughGetChildren();
    TestEditorControlsAreCatalogued();
}
