#pragma once

#include <DxUi/Embedded.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <psapi.h>
#include <random>
#include <string_view>
#include <typeinfo>
#include <vector>
#pragma comment(lib, "psapi.lib")

// Opt-in, within-candidate diagnostic, never part of the implicit functional suite:
// DxUi.EmbeddedTests.exe --benchmark-form-layout <report.json> [seed] [blocks]
// The manual variant performs real leaf measurement with bounded numeric caches; the form variant delegates it
// to FormPanel/FormField. The same semantic leaf hierarchy, metadata and populated pixels must agree before timing.
// Check, Hr and the allocation hook are supplied by Embedded.Tests.Embedded.cpp. Its signed live-byte counter
// covers plain C++ allocations on this thread, not aligned blocks, COM/driver heaps or the whole process.
namespace FormLayoutBenchmark
{
using Clock                              = std::chrono::steady_clock;
inline constexpr size_t kFieldCount      = 8u;
inline constexpr size_t kWarmupFrames    = 12u;
inline constexpr size_t kFramesPerPhase  = 40u;
inline constexpr size_t kBlockCount      = 12u;
inline constexpr size_t kMaxBlockCount   = 64u;
inline constexpr uint32_t kDefaultSeed   = 20'261'010u;
inline constexpr float kPaddingDip       = 12.0f;
inline constexpr float kColumnGapDip     = 16.0f;
inline constexpr float kRowGapDip        = 16.0f;
inline constexpr float kPartGapDip       = 6.0f;
inline constexpr float kLabelGapDip      = 12.0f;
inline constexpr float kMinimumEditorDip = 48.0f;
inline constexpr std::array<std::wstring_view, kFieldCount> kIds{
    L"form.name", L"form.language", L"form.width", L"form.enabled", L"form.path", L"form.method", L"form.height", L"form.preview"};
inline constexpr std::array<std::wstring_view, kFieldCount> kLabels{L"Nom de la présentation",
                                                                    L"Langue — العربية",
                                                                    L"Largeur en pixels",
                                                                    L"Activer la fonction",
                                                                    L"Chemin de destination — 東京",
                                                                    L"Méthode de synchronisation",
                                                                    L"Hauteur disponible",
                                                                    L"Aperçu de l’élément é 📷"};
inline constexpr std::array<std::wstring_view, kFieldCount> kHelp{L"Ce nom reste disponible pour la prochaine ouverture.",
                                                                  L"Le texte conserve les caractères français, العربية et 東京.",
                                                                  L"Choisissez une dimension compatible avec le contenu.",
                                                                  L"La valeur appartient à l’application et reste inchangée.",
                                                                  L"Le chemin complet reste accessible après le redimensionnement.",
                                                                  L"La méthode sélectionnée s’applique aux prochaines opérations.",
                                                                  L"Cette dimension ne modifie pas les autres paramètres.",
                                                                  L"L’aperçu utilise la même police, le même thème et les mêmes valeurs."};
inline constexpr std::wstring_view kError = L"Indiquez une valeur valide pour continuer — é 東京 📷.";

enum class Variant : uint8_t
{
    Manual,
    Form
};
enum class Phase : uint8_t
{
    Unchanged,
    Width,
    Validation
};
struct Profile
{
    const char* name;
    UINT width;
    UINT narrowWidth;
    UINT height;
    float dpi;
    DxUi::Density density;
    DxUi::FlowDirection flow;
    size_t columns;
    DxUi::LabelPlacement labels;
};
inline constexpr std::array<Profile, 2> kProfiles{{
    {"standard-ltr-above", 640u, 520u, 720u, 96.0f, DxUi::Density::Standard, DxUi::FlowDirection::LeftToRight, 2u, DxUi::LabelPlacement::Above},
    {"compact-rtl-beside", 720u, 600u, 1440u, 144.0f, DxUi::Density::Compact, DxUi::FlowDirection::RightToLeft, 1u, DxUi::LabelPlacement::Beside},
}};

[[nodiscard]] inline double ElapsedMs(Clock::time_point start) noexcept
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
[[nodiscard]] inline PROCESS_MEMORY_COUNTERS_EX Memory()
{
    PROCESS_MEMORY_COUNTERS_EX result{};
    result.cb = sizeof(result);
    Check(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&result), sizeof(result)) != FALSE,
          "form layout process memory");
    return result;
}
[[nodiscard]] inline uint32_t ParseSeed(const wchar_t* text)
{
    Check(text && *text, "form layout seed is populated");
    errno            = 0;
    wchar_t* end     = nullptr;
    const auto value = std::wcstoull(text, &end, 10);
    Check(end && *end == L'\0' && errno == 0 && value <= (std::numeric_limits<uint32_t>::max)(), "form layout seed is uint32");
    return static_cast<uint32_t>(value);
}
[[nodiscard]] inline size_t ParseBlocks(const wchar_t* text)
{
    const auto value = ParseSeed(text);
    Check(value > 0u && value <= kMaxBlockCount, "form layout blocks are bounded to 1..64");
    return value;
}

struct PartCache
{
    const DxUi::Control* control = nullptr;
    std::weak_ptr<int> lifetime;
    uint64_t revision             = 0u;
    uint64_t typography           = 0u;
    const DxUi::ControlHost* host = nullptr;
    float width                   = -1.0f;
    DxUi::DesiredSize desired;
};
struct PartColumns
{
    float labelLeft   = 0.0f;
    float labelRight  = 0.0f;
    float editorLeft  = 0.0f;
    float editorRight = 0.0f;
};
[[nodiscard]] inline PartColumns ResolveManualPartColumns(const D2D1_RECT_F& bounds, bool beside, bool rtl, float labelWidth, float editorMinimum)
{
    if (! beside)
        labelWidth = bounds.right - bounds.left;
    const float gap = beside && labelWidth > 0.0f ? kLabelGapDip : 0.0f;
    PartColumns columns;
    columns.labelLeft     = beside && rtl ? bounds.right - labelWidth : bounds.left;
    columns.labelRight    = beside && ! rtl ? bounds.left + labelWidth : bounds.right;
    const float laneLeft  = beside && ! rtl ? columns.labelRight + gap : bounds.left;
    const float laneRight = beside && rtl ? columns.labelLeft - gap : bounds.right;
    const float width     = (std::max)({kMinimumEditorDip, editorMinimum, laneRight - laneLeft});
    columns.editorLeft    = rtl ? laneRight - width : laneLeft;
    columns.editorRight   = rtl ? laneRight : laneLeft + width;
    // Measure and arrange the same represented endpoints, including inward rounding at the minimum.
    if (columns.editorRight - columns.editorLeft < width)
    {
        if (rtl)
            columns.editorLeft = std::nextafter(columns.editorLeft, -(std::numeric_limits<float>::infinity)());
        else
            columns.editorRight = std::nextafter(columns.editorRight, (std::numeric_limits<float>::infinity)());
    }
    Check(std::isfinite(columns.labelLeft) && std::isfinite(columns.labelRight) && columns.labelRight >= columns.labelLeft &&
              std::isfinite(columns.editorLeft) && std::isfinite(columns.editorRight) && columns.editorRight - columns.editorLeft >= width,
          "manual form represented columns preserve the minimum");
    return columns;
}
struct Row
{
    DxUi::Panel* owner     = nullptr;
    DxUi::FormField* field = nullptr;
    // Label, editor, description, validation. Child ownership order matches FormField's public logical hierarchy.
    std::array<DxUi::Control*, 4> parts{};
    std::array<PartCache, 4> caches;
    PartCache sharedLabel;
    DxUi::Control* semantic = nullptr;
    bool required           = false;
};
struct Frame
{
    UINT width                         = 0u;
    bool validation                    = false;
    double layoutAndPrepareMs          = 0;
    double prepareMs                   = 0;
    double compositeMs                 = 0;
    double completedFrameMs            = 0;
    uint64_t layoutAndPrepareCycles    = 0u;
    uint64_t compositeCycles           = 0u;
    uint64_t measures                  = 0u;
    uint64_t preparations              = 0u;
    uint64_t surfaceAllocations        = 0u;
    uint64_t surfaceBytes              = 0u;
    size_t cppAllocations              = 0u;
    size_t cppAllocationBytes          = 0u;
    ptrdiff_t plainCppLiveDeltaBytes   = 0;
    size_t compositeCppAllocations     = 0u;
    size_t compositeCppAllocationBytes = 0u;
};

// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class Fixture final
{
public:
    Fixture(GraphicsFixture& gpu, const std::shared_ptr<DxUi::GraphicsDevice>& graphics, Profile profile, Variant variant)
        : _gpu(&gpu),
          _profile(profile),
          _variant(variant),
          _width(profile.width)
    {
        Hr(_view.Attach(graphics), "form layout fixture attach");
        auto theme          = DxUi::MakeDefaultThemePalette(true);
        theme.reducedMotion = true;
        _view.Controls().SetTheme(theme);
        // Initialize DPI and device/surface state equally before the leaf hierarchy is installed.
        Hr(_view.Prepare(_width, _profile.height, _profile.dpi), "form layout blank surface setup");
        std::unique_ptr<DxUi::Panel> root;
        if (_variant == Variant::Form)
        {
            auto form = std::make_unique<DxUi::FormPanel>();
            _form     = form.get();
            Hr(form->SetColumns(_profile.columns), "form layout columns");
            Hr(form->SetLabelPlacement(_profile.labels), "form layout label placement");
            Hr(form->SetGap(kColumnGapDip, kRowGapDip), "form layout gaps");
            Hr(form->SetPadding(kPaddingDip), "form layout padding");
            root = std::move(form);
        }
        else
            root = std::make_unique<DxUi::Panel>();
        _root = root.get();
        root->SetDensity(_profile.density);
        root->SetFlowDirection(_profile.flow);
        for (size_t index = 0u; index < _rows.size(); ++index)
        {
            auto& row = _rows[index];
            if (_form)
            {
                row.field = _form->AddField(std::wstring(kIds[index]), std::wstring(kLabels[index]));
                Check(row.field != nullptr, "form layout field created");
                row.owner    = row.field;
                row.parts[0] = row.field->GetLabelControl();
                row.parts[2] = row.field->GetLogicalChild(1u);
                row.parts[3] = row.field->GetLogicalChild(2u);
            }
            else
            {
                row.owner    = root->AddChild<DxUi::Panel>();
                row.parts[0] = row.owner->AddChild<DxUi::Label>(std::wstring(kLabels[index]));
                row.parts[2] = row.owner->AddChild<DxUi::Label>();
                row.parts[3] = row.owner->AddChild<DxUi::Label>();
                for (const size_t part : {0u, 2u, 3u})
                    static_cast<DxUi::Label*>(row.parts[part])->SetMultiline(true);
                static_cast<DxUi::Label*>(row.parts[2])->SetFontRole(DxUi::FontRole::Small);
                static_cast<DxUi::Label*>(row.parts[3])->SetFontRole(DxUi::FontRole::Small);
                row.parts[2]->SetVisible(false);
                row.parts[3]->SetVisible(false);
            }
            row.parts[1] = AddEditor(row, index % 4u);
            Check(row.parts[1] != nullptr, "form layout editor created");
            if (auto* stepper = dynamic_cast<DxUi::NumericStepper*>(row.parts[1]))
                row.semantic = &stepper->Field();
            else
                row.semantic = row.parts[1];
            row.required = index % 2u == 0u;
            if (row.field)
            {
                row.field->SetDescription(std::wstring(kHelp[index]));
                Hr(row.field->SetRequired(row.required), "form layout required metadata");
                Hr(row.field->SetEditorPreferredSize(D2D1::SizeF(180.0f, PreferredEditorHeight())), "form layout preferred editor height");
            }
            else
            {
                static_cast<DxUi::Label*>(row.parts[2])->SetText(std::wstring(kHelp[index]));
                row.parts[2]->SetVisible(true);
                SyncManualMetadata(index);
            }
        }
        _view.Controls().SetRoot(std::move(root));
        CreateCompletion();
    }

    [[nodiscard]] const std::array<Row, kFieldCount>& Rows() const noexcept
    {
        return _rows;
    }
    [[nodiscard]] DxUi::EmbeddedStatistics Statistics() const noexcept
    {
        return _view.GetStatistics();
    }
    [[nodiscard]] uint64_t MeasureCount() const noexcept
    {
        return _form ? _form->DebugGetMeasureCount() : _manualMeasures;
    }
    [[nodiscard]] UINT Width() const noexcept
    {
        return _width;
    }
    [[nodiscard]] bool Validation() const noexcept
    {
        return _validation;
    }
    void RestoreInitialState()
    {
        if (_width != _profile.width)
        {
            _width       = _profile.width;
            _manualDirty = true;
        }
        SetValidation(false);
    }
    void PrepareOnly()
    {
        ApplyManualLayout();
        Hr(_view.Prepare(_width, _profile.height, _profile.dpi), "form layout preparation");
    }
    void CheckCachedLayout()
    {
        const auto measures     = MeasureCount();
        const auto preparations = Statistics().preparations;
        if (_variant == Variant::Manual)
            _manualDirty = true;
        PrepareOnly();
        Check(MeasureCount() == measures && Statistics().preparations == preparations,
              "form layout preflight repeats exact translated widths without measuring or preparing");
        if (_variant == Variant::Manual)
            for (const auto& row : _rows)
                for (size_t part = 0u; part < row.parts.size(); ++part)
                    if (row.parts[part]->IsVisible())
                    {
                        const auto bounds = row.parts[part]->GetBounds();
                        Check(row.caches[part].width == bounds.right - bounds.left, "manual form measures the exact arranged leaf width");
                    }
    }
    void CompositeOnly()
    {
        _gpu->Bind();
        Hr(_view.Composite(_gpu->context.get(), Viewport()), "form layout composition");
        Complete();
    }
    void Pixels(std::vector<uint8_t>& pixels)
    {
        CompositeOnly();
        Hr(_gpu->Read(pixels), "form layout populated pixels");
    }

    [[nodiscard]] Frame MeasureFrame(Phase phase, size_t frame)
    {
        Frame sample;
        const auto measuresBefore   = MeasureCount();
        const auto statisticsBefore = Statistics();
        const auto liveBefore       = liveBytes;
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        const auto counting           = wil::scope_exit([]() noexcept { countAllocations = false; });
        ULONG64 cyclesBefore = 0u, cyclesAfter = 0u, compositeCyclesBefore = 0u, compositeCyclesAfter = 0u;
        Check(QueryThreadCycleTime(GetCurrentThread(), &cyclesBefore) != FALSE, "form layout preparation cycles");
        const auto started = Clock::now();
        if (phase == Phase::Width)
        {
            const UINT next = frame % 2u ? _profile.width : _profile.narrowWidth;
            if (next != _width)
            {
                _width       = next;
                _manualDirty = true;
            }
        }
        else if (phase == Phase::Validation)
            SetValidation(frame % 2u == 0u);
        ApplyManualLayout();
        const auto prepareStarted = Clock::now();
        Hr(_view.Prepare(_width, _profile.height, _profile.dpi), "form layout timed preparation");
        sample.prepareMs          = ElapsedMs(prepareStarted);
        sample.layoutAndPrepareMs = ElapsedMs(started);
        Check(QueryThreadCycleTime(GetCurrentThread(), &cyclesAfter) != FALSE, "form layout preparation cycles complete");
        sample.layoutAndPrepareCycles = cyclesAfter - cyclesBefore;
        _gpu->Bind();
        const auto compositeMeasures     = MeasureCount();
        const auto compositePreparations = Statistics().preparations;
        const auto compositeAllocations  = allocations;
        const auto compositeBytes        = allocationBytes;
        Check(QueryThreadCycleTime(GetCurrentThread(), &compositeCyclesBefore) != FALSE, "form layout composition cycles");
        const auto compositeStarted = Clock::now();
        Hr(_view.Composite(_gpu->context.get(), Viewport()), "form layout timed composition");
        sample.compositeMs = ElapsedMs(compositeStarted);
        Check(QueryThreadCycleTime(GetCurrentThread(), &compositeCyclesAfter) != FALSE, "form layout composition cycles complete");
        sample.compositeCycles             = compositeCyclesAfter - compositeCyclesBefore;
        sample.compositeCppAllocations     = allocations - compositeAllocations;
        sample.compositeCppAllocationBytes = allocationBytes - compositeBytes;
        Check(sample.compositeCppAllocations == 0u && sample.compositeCppAllocationBytes == 0u && MeasureCount() == compositeMeasures &&
                  Statistics().preparations == compositePreparations,
              "form layout composition allocates, measures and prepares nothing");
        Complete();
        sample.completedFrameMs       = ElapsedMs(started);
        sample.cppAllocations         = allocations;
        sample.cppAllocationBytes     = allocationBytes;
        sample.plainCppLiveDeltaBytes = liveBytes - liveBefore;
        const auto statisticsAfter    = Statistics();
        sample.width                  = _width;
        sample.validation             = _validation;
        sample.measures               = MeasureCount() - measuresBefore;
        sample.preparations           = statisticsAfter.preparations - statisticsBefore.preparations;
        sample.surfaceAllocations     = statisticsAfter.surfaceAllocations - statisticsBefore.surfaceAllocations;
        sample.surfaceBytes           = statisticsAfter.surfaceBytes;
        return sample;
    }
    void Narrow()
    {
        _width       = _profile.narrowWidth;
        _manualDirty = true;
    }
    void SetValidation(bool value)
    {
        if (_validation == value)
            return;
        _validation  = value;
        _manualDirty = true;
        auto& row    = _rows[0u];
        if (row.field)
            Hr(row.field->SetValidation(value ? DxUi::ValidationState::Error : DxUi::ValidationState::None, value ? std::wstring(kError) : std::wstring{}),
               "form layout validation");
        else
        {
            static_cast<DxUi::Label*>(row.parts[3])->SetText(value ? std::wstring(kError) : std::wstring{});
            row.parts[3]->SetVisible(value);
            SyncManualMetadata(0u);
        }
    }
    void SetVisible(bool visible)
    {
        _view.SetVisible(visible);
        if (visible)
            _manualDirty = true;
    }
    [[nodiscard]] HRESULT HiddenPrepare()
    {
        return _view.Prepare(_width, _profile.height, _profile.dpi);
    }
    [[nodiscard]] HRESULT PendingComposite()
    {
        _gpu->Bind();
        return _view.Composite(_gpu->context.get(), Viewport());
    }
    void ReplaceDevice(GraphicsFixture& gpu, const std::shared_ptr<DxUi::GraphicsDevice>& graphics)
    {
        Hr(_view.ReplaceDevice(graphics), "form layout device generation replacement");
        _gpu         = &gpu;
        _manualDirty = true;
        CreateCompletion();
    }
    void RetireRoot()
    {
        const auto lifetime = GetControlLifetimeToken(*_root);
        const auto count    = MeasureCount();
        _view.Controls().SetRoot({});
        Check(lifetime.expired(), "form layout root retirement releases ownership");
        _form           = nullptr;
        _root           = nullptr;
        _manualMeasures = count;
        _rows           = {};
        _manualDirty    = false;
    }
    void Detach()
    {
        _view.Detach();
    }

private:
    [[nodiscard]] float PreferredEditorHeight() const noexcept
    {
        return _profile.density == DxUi::Density::Compact ? 24.0f : 32.0f;
    }
    [[nodiscard]] D3D11_VIEWPORT Viewport() const noexcept
    {
        return {0, 0, static_cast<float>(_width), static_cast<float>(_profile.height), 0, 1};
    }
    template <typename T, typename... Args> [[nodiscard]] T* Add(Row& row, Args&&... args)
    {
        return row.field ? row.field->AddEditor<T>(std::forward<Args>(args)...) : row.owner->AddChild<T>(std::forward<Args>(args)...);
    }
    [[nodiscard]] DxUi::Control* AddEditor(Row& row, size_t type)
    {
        if (type == 0u)
        {
            auto* value = Add<DxUi::TextField>(row);
            value->SetText(L"Présentation é — 東京 📷");
            return value;
        }
        if (type == 1u)
        {
            auto* value = Add<DxUi::ComboBox>(row);
            value->SetText(L"Français — العربية");
            return value;
        }
        if (type == 2u)
        {
            auto* value = Add<DxUi::NumericStepper>(row);
            value->SetMinimum(0.0);
            value->SetMaximum(4096.0);
            value->SetDecimals(2u);
            value->SetValue(123.45);
            return value;
        }
        auto* value = Add<DxUi::Checkbox>(row, L"Conserver cet état");
        value->SetChecked(true);
        return value;
    }
    void SyncManualMetadata(size_t index)
    {
        auto& row = _rows[index];
        std::wstring help(kHelp[index]);
        std::wstring status(index == 0u && _validation ? kError : std::wstring_view{});
        std::wstring id(kIds[index]);
        Hr(row.semantic->SetAccessibleFormProperties(row.parts[0], row.required, index != 0u || ! _validation, std::move(status)), "manual form metadata");
        static_cast<DxUi::Label*>(row.parts[0])->SetMnemonicTarget(row.semantic);
        row.semantic->SetAccessibleHelpText(std::move(help));
        row.semantic->SetAccessibleAutomationId(std::move(id));
    }
    [[nodiscard]] DxUi::DesiredSize MeasurePart(DxUi::Control* control, float width, PartCache& cache, const DxUi::MeasureContext& context)
    {
        if (! control || ! control->IsVisible())
            return {};
        static_cast<void>(context.host.GetWriteFactory());
        const auto revision   = control->GetMeasureRevision();
        const auto typography = context.host.GetTypographyRevision();
        if (cache.control == control && ! cache.lifetime.expired() && cache.revision == revision && cache.typography == typography &&
            cache.host == &context.host && cache.width == width)
            return cache.desired;
        DxUi::DesiredSize desired;
        Hr(DxUi::MeasureControl(*control, context, DxUi::LayoutConstraints::AtMost(width), desired), "manual bounded leaf measure");
        cache = {control, GetControlLifetimeToken(*control), revision, typography, &context.host, width, desired};
        ++_manualMeasures;
        return desired;
    }
    void ApplyManualLayout()
    {
        if (_variant != Variant::Manual || ! _manualDirty || ! _root)
            return;
        const float width  = static_cast<float>(_width) * 96.0f / _profile.dpi;
        const float height = static_cast<float>(_profile.height) * 96.0f / _profile.dpi;
        _root->SetBounds(D2D1::RectF(0, 0, width, height));
        const bool beside        = _profile.labels == DxUi::LabelPlacement::Beside;
        const bool rtl           = _profile.flow == DxUi::FlowDirection::RightToLeft;
        const float columns      = static_cast<float>(_profile.columns);
        const float minimumWidth = beside ? 2.0f * kMinimumEditorDip + kLabelGapDip : kMinimumEditorDip;
        const float columnWidth =
            (std::max)(minimumWidth, ((std::max)(0.0f, width - 2.0f * kPaddingDip) - static_cast<float>(_profile.columns - 1u) * kColumnGapDip) / columns);
        size_t budget = DxUi::kMaxIntrinsicTextUnits;
        const DxUi::MeasureContext context{_view.Controls(), &budget};
        float labelWidth = 0.0f;
        if (beside)
        {
            const float cap = (std::min)(columnWidth * 0.45f, columnWidth - kMinimumEditorDip - kLabelGapDip);
            for (auto& row : _rows)
                labelWidth = (std::max)(labelWidth, (std::min)(cap, MeasurePart(row.parts[0], cap, row.sharedLabel, context).sizeDip.width));
        }
        std::array<std::array<DxUi::DesiredSize, 4>, kFieldCount> measured;
        std::array<D2D1_RECT_F, kFieldCount> fieldBounds{};
        std::array<PartColumns, kFieldCount> partColumns{};
        std::array<float, kFieldCount> fieldHeights{};
        for (size_t index = 0u; index < _rows.size(); ++index)
        {
            const size_t column = index % _profile.columns;
            const float offset  = static_cast<float>(column) * (columnWidth + kColumnGapDip);
            const float left    = rtl ? width - kPaddingDip - columnWidth - offset : kPaddingDip + offset;
            fieldBounds[index]  = D2D1::RectF(left, 0.0f, left + columnWidth, 0.0f);
            auto& columnsForRow = partColumns[index];
            columnsForRow       = ResolveManualPartColumns(fieldBounds[index], beside, rtl, labelWidth, kMinimumEditorDip);
            for (size_t part = 0u; part < measured[index].size(); ++part)
            {
                const float partWidth = part == 0u ? columnsForRow.labelRight - columnsForRow.labelLeft : columnsForRow.editorRight - columnsForRow.editorLeft;
                measured[index][part] = MeasurePart(_rows[index].parts[part], partWidth, _rows[index].caches[part], context);
                if (part == 1u)
                {
                    auto& editor  = measured[index][part].sizeDip;
                    editor.width  = (std::max)(editor.width, (std::max)(kMinimumEditorDip, (std::min)(180.0f, partWidth)));
                    editor.height = (std::max)(editor.height, PreferredEditorHeight());
                    columnsForRow = ResolveManualPartColumns(fieldBounds[index], beside, rtl, labelWidth, editor.width);
                    Check(columnsForRow.editorRight - columnsForRow.editorLeft == partWidth,
                          "manual diagnostic profile has no editor minimum requiring column expansion");
                }
            }
            auto& parts = measured[index];
            fieldHeights[index] =
                beside ? (std::max)(parts[0].sizeDip.height, parts[1].sizeDip.height) : parts[0].sizeDip.height + kPartGapDip + parts[1].sizeDip.height;
            for (size_t part = 2u; part < parts.size(); ++part)
                if (parts[part].sizeDip.height > 0.0f)
                    fieldHeights[index] += kPartGapDip + parts[part].sizeDip.height;
        }
        float rowTop = kPaddingDip, rowHeight = 0.0f;
        for (size_t index = 0u; index < _rows.size(); ++index)
        {
            const size_t column = index % _profile.columns;
            if (column == 0u && index)
            {
                rowTop += rowHeight + kRowGapDip;
                rowHeight = 0.0f;
            }
            const auto rectangle = D2D1::RectF(fieldBounds[index].left, rowTop, fieldBounds[index].right, rowTop + fieldHeights[index]);
            auto& row            = _rows[index];
            row.owner->SetBounds(rectangle);
            const auto& parts         = measured[index];
            const auto& columnsForRow = partColumns[index];
            const float editorTop     = rectangle.top + (beside ? 0.0f : parts[0].sizeDip.height + kPartGapDip);
            float nextTop = beside ? rectangle.top + (std::max)(parts[0].sizeDip.height, parts[1].sizeDip.height) : editorTop + parts[1].sizeDip.height;
            const std::array<D2D1_RECT_F, 2> first{
                D2D1::RectF(columnsForRow.labelLeft, rectangle.top, columnsForRow.labelRight, rectangle.top + parts[0].sizeDip.height),
                D2D1::RectF(columnsForRow.editorLeft, editorTop, columnsForRow.editorRight, editorTop + parts[1].sizeDip.height)};
            row.parts[0]->SetBounds(first[0]);
            row.parts[1]->SetBounds(first[1]);
            for (size_t part = 2u; part < parts.size(); ++part)
            {
                if (parts[part].sizeDip.height > 0.0f)
                    nextTop += kPartGapDip;
                if (row.parts[part]->IsVisible())
                    row.parts[part]->SetBounds(D2D1::RectF(columnsForRow.editorLeft, nextTop, columnsForRow.editorRight, nextTop + parts[part].sizeDip.height));
                nextTop += parts[part].sizeDip.height;
            }
            rowHeight = (std::max)(rowHeight, fieldHeights[index]);
        }
        _manualDirty = false;
    }
    void CreateCompletion()
    {
        D3D11_TEXTURE2D_DESC descriptor{};
        descriptor.Width = descriptor.Height = descriptor.MipLevels = descriptor.ArraySize = descriptor.SampleDesc.Count = 1u;
        descriptor.Format                                                                                                = DXGI_FORMAT_B8G8R8A8_UNORM;
        descriptor.Usage                                                                                                 = D3D11_USAGE_STAGING;
        descriptor.CPUAccessFlags                                                                                        = D3D11_CPU_ACCESS_READ;
        _completion.reset();
        Hr(_gpu->device->CreateTexture2D(&descriptor, nullptr, _completion.put()), "form layout completion texture");
    }
    void Complete()
    {
        const D3D11_BOX pixel{0, 0, 0, 1, 1, 1};
        _gpu->context->CopySubresourceRegion(_completion.get(), 0u, 0u, 0u, 0u, _gpu->target.get(), 0u, &pixel);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Hr(_gpu->context->Map(_completion.get(), 0u, D3D11_MAP_READ, 0u, &mapped), "form layout GPU completion");
        _gpu->context->Unmap(_completion.get(), 0u);
    }

    GraphicsFixture* _gpu;
    Profile _profile;
    Variant _variant;
    DxUi::EmbeddedHost _view;
    wil::com_ptr_nothrow<ID3D11Texture2D> _completion;
    DxUi::Panel* _root     = nullptr;
    DxUi::FormPanel* _form = nullptr;
    std::array<Row, kFieldCount> _rows;
    UINT _width;
    bool _validation         = false;
    bool _manualDirty        = true;
    uint64_t _manualMeasures = 0u;
};
#pragma warning(pop)

inline void CheckControlParity(const DxUi::Control& manual, const DxUi::Control& form)
{
    Check(typeid(manual) == typeid(form), "form layout semantic leaf types agree");
    Check(manual.IsVisible() == form.IsVisible() && manual.IsEnabled() == form.IsEnabled() && manual.GetDensity() == form.GetDensity() &&
              manual.GetFlowDirection() == form.GetFlowDirection(),
          "form layout semantic leaf presentation agrees");
    const auto a = manual.GetBounds(), b = form.GetBounds();
    Check(std::abs(a.left - b.left) < 0.01f && std::abs(a.top - b.top) < 0.01f && std::abs(a.right - b.right) < 0.01f && std::abs(a.bottom - b.bottom) < 0.01f,
          "form layout leaf geometry agrees");
    if (const auto* label = dynamic_cast<const DxUi::Label*>(&manual))
        Check(label->GetText() == static_cast<const DxUi::Label&>(form).GetText(), "form layout label content agrees");
    if (const auto* field = dynamic_cast<const DxUi::TextField*>(&manual))
        Check(field->GetText() == static_cast<const DxUi::TextField&>(form).GetText(), "form layout text draft agrees");
    if (const auto* combo = dynamic_cast<const DxUi::ComboBox*>(&manual))
        Check(combo->GetText() == static_cast<const DxUi::ComboBox&>(form).GetText(), "form layout combo value agrees");
    if (const auto* button = dynamic_cast<const DxUi::Button*>(&manual))
        Check(button->GetText() == static_cast<const DxUi::Button&>(form).GetText(), "form layout button caption agrees");
    Check(manual.GetAccessibleAutomationId() == form.GetAccessibleAutomationId() && manual.GetAccessibleHelpText() == form.GetAccessibleHelpText() &&
              manual.GetAccessibleItemStatus() == form.GetAccessibleItemStatus() &&
              manual.IsAccessibleRequiredForForm() == form.IsAccessibleRequiredForForm() &&
              manual.IsAccessibleDataValidForForm() == form.IsAccessibleDataValidForForm(),
          "form layout accessibility metadata agrees");
    Check(manual.GetLogicalChildCount() == form.GetLogicalChildCount(), "form layout editor child hierarchy agrees");
    for (size_t index = 0u; index < manual.GetLogicalChildCount(); ++index)
        CheckControlParity(*manual.GetLogicalChild(index), *form.GetLogicalChild(index));
}
[[nodiscard]] inline bool HasPopulatedPixels(const std::vector<uint8_t>& pixels, const std::vector<uint8_t>& background) noexcept
{
    if (pixels.size() < 8u || pixels.size() % 4u != 0u || pixels.size() != background.size() || pixels == background)
        return false;
    // A uniformly cleared or uniformly painted empty canvas is insufficient evidence of populated controls.
    for (size_t offset = 4u; offset < pixels.size(); offset += 4u)
        if (! std::equal(pixels.data(), pixels.data() + 4u, pixels.data() + offset))
            return true;
    return false;
}
inline void CheckPreflightGuards(const std::vector<uint8_t>& background)
{
    Check(! HasPopulatedPixels({}, background) && ! HasPopulatedPixels(background, background), "form pixel preflight rejects empty and cleared captures");
    auto uniform = background;
    for (size_t offset = 0u; offset < uniform.size(); offset += 4u)
        uniform[offset] ^= 1u;
    Check(! HasPopulatedPixels(uniform, background), "form pixel preflight rejects a different uniform empty canvas");
    uniform[0] ^= 1u;
    Check(HasPopulatedPixels(uniform, background), "form pixel preflight distinguishes painted content from a uniform canvas");
    constexpr float minimum = 48.001f;
    const auto bounds       = D2D1::RectF(1000.0f, 0.0f, 1020.0f, 0.0f);
    for (const bool rtl : {false, true})
    {
        const auto columns = ResolveManualPartColumns(bounds, false, rtl, 0.0f, minimum);
        Check(columns.labelLeft == bounds.left && columns.labelRight == bounds.right && columns.editorRight - columns.editorLeft >= minimum &&
                  (rtl ? columns.editorRight == bounds.right : columns.editorLeft == bounds.left),
              "manual form preflight preserves exact anchored endpoints and fractional minima");
    }
}
inline void CheckParity(GraphicsFixture& gpu, const std::shared_ptr<DxUi::GraphicsDevice>& graphics, const Profile& profile)
{
    std::vector<uint8_t> background;
    gpu.Bind();
    Hr(gpu.Read(background), "form layout empty background reference");
    Check(background.size() == static_cast<size_t>(profile.width) * profile.height * 4u, "form layout complete background capture");
    CheckPreflightGuards(background);
    Fixture manual(gpu, graphics, profile, Variant::Manual), form(gpu, graphics, profile, Variant::Form);
    std::vector<uint8_t> manualPixels, formPixels;
    for (size_t state = 0u; state < 5u; ++state)
    {
        if (state == 1u)
        {
            manual.Narrow();
            form.Narrow();
        }
        if (state == 2u)
        {
            manual.SetValidation(true);
            form.SetValidation(true);
        }
        if (state == 3u)
        {
            manual.RestoreInitialState();
            form.RestoreInitialState();
            manual.SetValidation(true);
            form.SetValidation(true);
        }
        if (state == 4u)
        {
            manual.RestoreInitialState();
            form.RestoreInitialState();
        }
        manual.PrepareOnly();
        form.PrepareOnly();
        manual.CheckCachedLayout();
        form.CheckCachedLayout();
        for (size_t index = 0u; index < kFieldCount; ++index)
        {
            const auto& a = manual.Rows()[index];
            const auto& b = form.Rows()[index];
            for (size_t part = 0u; part < a.parts.size(); ++part)
                CheckControlParity(*a.parts[part], *b.parts[part]);
            Check(a.semantic->GetAccessibleLabelledBy() == a.parts[0] && b.semantic->GetAccessibleLabelledBy() == b.parts[0],
                  "form layout label relationships point to their own labels");
        }
        manual.Pixels(manualPixels);
        form.Pixels(formPixels);
        Check(HasPopulatedPixels(manualPixels, background) && HasPopulatedPixels(formPixels, background),
              "form layout both variants contain populated control pixels before timing");
        Check(manualPixels == formPixels, "form layout populated pixels agree before timing");
    }
}
inline void WriteFrame(std::ostream& output, const Frame& frame)
{
    output << "{\"width\":" << frame.width << ",\"validation\":" << (frame.validation ? "true" : "false")
           << ",\"layoutAndPrepareMs\":" << frame.layoutAndPrepareMs << ",\"prepareMs\":" << frame.prepareMs << ",\"compositeMs\":" << frame.compositeMs
           << ",\"completedFrameMs\":" << frame.completedFrameMs << ",\"layoutAndPrepareThreadCycles\":" << frame.layoutAndPrepareCycles
           << ",\"compositeThreadCycles\":" << frame.compositeCycles << ",\"measures\":" << frame.measures << ",\"preparations\":" << frame.preparations
           << ",\"surfaceAllocations\":" << frame.surfaceAllocations << ",\"surfaceBytes\":" << frame.surfaceBytes
           << ",\"cppAllocations\":" << frame.cppAllocations << ",\"cppAllocationBytes\":" << frame.cppAllocationBytes
           << ",\"plainCppLiveDeltaBytes\":" << frame.plainCppLiveDeltaBytes << ",\"compositeCppAllocations\":" << frame.compositeCppAllocations
           << ",\"compositeCppAllocationBytes\":" << frame.compositeCppAllocationBytes << '}';
}
inline void WriteMemory(std::ostream& output, const PROCESS_MEMORY_COUNTERS_EX& memory)
{
    output << "{\"privateBytes\":" << memory.PrivateUsage << ",\"workingSetBytes\":" << memory.WorkingSetSize
           << ",\"peakWorkingSetBytes\":" << memory.PeakWorkingSetSize << '}';
}
inline void WriteStatistics(std::ostream& output, const DxUi::EmbeddedStatistics& statistics)
{
    output << "{\"preparations\":" << statistics.preparations << ",\"composites\":" << statistics.composites << ",\"surfaceBytes\":" << statistics.surfaceBytes
           << ",\"surfaceAllocations\":" << statistics.surfaceAllocations << ",\"replacementPeakBytes\":" << statistics.replacementPeakBytes
           << ",\"cachedBrushes\":" << statistics.cachedBrushes << ",\"cachedTextFormats\":" << statistics.cachedTextFormats << '}';
}
inline void MeasureRun(GraphicsFixture& gpu,
                       const std::shared_ptr<DxUi::GraphicsDevice>& graphics,
                       const Profile& profile,
                       Variant variant,
                       size_t block,
                       size_t slot,
                       std::ostream& output)
{
    std::array<std::array<Frame, kFramesPerPhase>, 3> samples{};
    std::array<PROCESS_MEMORY_COUNTERS_EX, 5> memory{};
    std::array<ptrdiff_t, 5> retained{};
    Frame initial;
    size_t constructionAllocations = 0u, constructionBytes = 0u;
    DxUi::EmbeddedStatistics finalStatistics;
    memory[0]      = Memory();
    liveBytes      = 0;
    countLiveBytes = countAllocations = true;
    allocations = allocationBytes = 0u;
    {
        Fixture fixture(gpu, graphics, profile, variant);
        constructionAllocations = allocations;
        constructionBytes       = allocationBytes;
        countAllocations        = false;
        retained[0]             = liveBytes;
        initial                 = fixture.MeasureFrame(Phase::Unchanged, 0u);
        for (size_t frame = 0u; frame < kWarmupFrames; ++frame)
            static_cast<void>(fixture.MeasureFrame(Phase::Unchanged, frame));
        memory[1]   = Memory();
        retained[1] = liveBytes;
        for (size_t phase = 0u; phase < samples.size(); ++phase)
        {
            fixture.RestoreInitialState();
            static_cast<void>(fixture.MeasureFrame(Phase::Unchanged, 0u));
            for (size_t frame = 0u; frame < kFramesPerPhase; ++frame)
            {
                samples[phase][frame] = fixture.MeasureFrame(static_cast<Phase>(phase), frame);
                if (phase == 0u)
                    Check(samples[phase][frame].measures == 0u && samples[phase][frame].preparations == 0u && samples[phase][frame].cppAllocations == 0u,
                          "unchanged form layout frames reuse geometry, preparation and C++ storage");
            }
            memory[phase + 2u]   = Memory();
            retained[phase + 2u] = liveBytes;
        }
        finalStatistics = fixture.Statistics();
    }
    countLiveBytes = countAllocations = false;
    const auto retiredBytes           = liveBytes;
    const auto afterRetirement        = Memory();
    output << "{\"block\":" << block << ",\"slot\":" << slot << ",\"variant\":\"" << (variant == Variant::Manual ? "manual" : "form")
           << "\",\"constructionCppAllocations\":" << constructionAllocations << ",\"constructionCppAllocationBytes\":" << constructionBytes
           << ",\"constructionPlainCppLiveDeltaBytes\":" << retained[0] << ",\"initial\":";
    WriteFrame(output, initial);
    output << ",\"stages\":[";
    constexpr std::array<const char*, 5> stages{"beforeConstruction", "afterWarmup", "afterUnchanged", "afterWidth", "afterValidation"};
    for (size_t stage = 0u; stage < stages.size(); ++stage)
    {
        if (stage)
            output << ',';
        output << "{\"stage\":\"" << stages[stage] << "\",\"plainCppLiveDeltaBytes\":" << (stage == 0u ? 0 : retained[stage]) << ",\"process\":";
        WriteMemory(output, memory[stage]);
        output << '}';
    }
    output << "],\"afterRetirement\":{\"plainCppLiveDeltaBytes\":" << retiredBytes << ",\"process\":";
    WriteMemory(output, afterRetirement);
    output << "},\"finalStatistics\":";
    WriteStatistics(output, finalStatistics);
    output << ",\"phases\":[";
    constexpr std::array<const char*, 3> names{"unchanged", "width", "validation"};
    for (size_t phase = 0u; phase < samples.size(); ++phase)
    {
        if (phase)
            output << ',';
        output << "{\"phase\":\"" << names[phase] << "\",\"frames\":[";
        for (size_t frame = 0u; frame < kFramesPerPhase; ++frame)
        {
            if (frame)
                output << ',';
            WriteFrame(output, samples[phase][frame]);
        }
        output << "]}";
    }
    output << "]}";
}
inline void Lifecycle(
    GraphicsFixture& gpu, const std::shared_ptr<DxUi::GraphicsDevice>& graphics, const Profile& profile, Variant variant, std::ostream& output)
{
    // Capture buffers and both caller-owned pools are allocated before the C++ live window and remain fixed.
    const size_t pixelBytes = static_cast<size_t>(profile.width) * profile.height * 4u;
    std::vector<uint8_t> before(pixelBytes), after(pixelBytes);
    GraphicsFixture replacement;
    replacement.width  = profile.width;
    replacement.height = profile.height;
    Hr(replacement.Create(), "form layout replacement WARP generation");
    std::shared_ptr<DxUi::GraphicsDevice> next;
    Hr(DxUi::GraphicsDevice::Create(replacement.device.get(), next), "form layout replacement graphics pool");
    struct Snapshot
    {
        const char* name  = nullptr;
        uint64_t measures = 0u;
        DxUi::EmbeddedStatistics statistics;
        PROCESS_MEMORY_COUNTERS_EX process;
        size_t cppAllocations    = 0u;
        size_t cppBytes          = 0u;
        ptrdiff_t plainLiveBytes = 0;
    };
    std::array<Snapshot, 10> stages{};
    size_t stageIndex  = 0u;
    const auto capture = [&](const char* name, uint64_t measures, DxUi::EmbeddedStatistics statistics)
    {
        countAllocations     = false;
        stages[stageIndex++] = {name, measures, statistics, Memory(), allocations, allocationBytes, liveBytes};
        allocations = allocationBytes = 0u;
        countAllocations              = true;
    };
    liveBytes   = 0;
    allocations = allocationBytes = 0u;
    countLiveBytes = countAllocations = true;
    uint64_t finalMeasures            = 0u;
    {
        Fixture fixture(gpu, graphics, profile, variant);
        fixture.PrepareOnly();
        fixture.Pixels(before);
        capture("visible", fixture.MeasureCount(), fixture.Statistics());
        const auto visible         = fixture.Statistics();
        const auto visibleMeasures = fixture.MeasureCount();
        fixture.SetVisible(false);
        Check(fixture.Statistics().surfaceBytes == 0u && fixture.HiddenPrepare() == S_FALSE && fixture.PendingComposite() == S_FALSE &&
                  fixture.MeasureCount() == visibleMeasures,
              "hidden form layout retains no surface and performs no layout work");
        Check(fixture.Statistics().surfaceAllocations == visible.surfaceAllocations, "hiding form layout allocates no surface");
        capture("hidden", fixture.MeasureCount(), fixture.Statistics());
        fixture.SetVisible(true);
        fixture.PrepareOnly();
        fixture.Pixels(after);
        Check(before == after && fixture.Statistics().surfaceAllocations == visible.surfaceAllocations + 1u, "shown form layout recovers the same pixels once");
        capture("shown", fixture.MeasureCount(), fixture.Statistics());
        fixture.ReplaceDevice(replacement, next);
        Check(fixture.Statistics().surfaceBytes == 0u && fixture.PendingComposite() == S_FALSE,
              "device generation replacement cannot compose stale form pixels");
        capture("generationInvalidated", fixture.MeasureCount(), fixture.Statistics());
        fixture.PrepareOnly();
        fixture.Pixels(after);
        Check(before == after, "device generation recovery preserves form pixels and values");
        capture("generationRecovered", fixture.MeasureCount(), fixture.Statistics());
        fixture.SetVisible(false);
        fixture.ReplaceDevice(gpu, graphics);
        Check(fixture.Statistics().surfaceBytes == 0u && fixture.HiddenPrepare() == S_FALSE, "hidden generation replacement retains no form surface");
        capture("hiddenGenerationReplaced", fixture.MeasureCount(), fixture.Statistics());
        fixture.SetVisible(true);
        fixture.PrepareOnly();
        fixture.Pixels(after);
        Check(before == after, "hidden generation recovery preserves form pixels and values");
        capture("hiddenGenerationRecovered", fixture.MeasureCount(), fixture.Statistics());
        fixture.RetireRoot();
        fixture.PrepareOnly();
        fixture.CompositeOnly();
        capture("rootRetired", fixture.MeasureCount(), fixture.Statistics());
        fixture.Detach();
        Check(fixture.Statistics().surfaceBytes == 0u, "detached form layout retains no surface");
        finalMeasures = fixture.MeasureCount();
        capture("detached", finalMeasures, fixture.Statistics());
    }
    capture("destroyed", finalMeasures, {});
    countLiveBytes = countAllocations = false;
    Check(stageIndex == stages.size(), "form layout complete lifecycle snapshots");
    output << "{\"variant\":\"" << (variant == Variant::Manual ? "manual" : "form") << "\",\"stages\":[";
    for (size_t index = 0u; index < stages.size(); ++index)
    {
        if (index)
            output << ',';
        const auto& stage = stages[index];
        output << "{\"stage\":\"" << stage.name << "\",\"measures\":" << stage.measures << ",\"cppAllocations\":" << stage.cppAllocations
               << ",\"cppAllocationBytes\":" << stage.cppBytes << ",\"plainCppLiveDeltaBytes\":" << stage.plainLiveBytes << ",\"statistics\":";
        WriteStatistics(output, stage.statistics);
        output << ",\"process\":";
        WriteMemory(output, stage.process);
        output << '}';
    }
    output << "]}";
}

inline void Run(const wchar_t* path, uint32_t seed = kDefaultSeed, size_t blocks = kBlockCount)
{
    Check(blocks > 0u && blocks <= kMaxBlockCount, "form layout block count is bounded");
    std::ofstream output{std::filesystem::path(path)};
    Check(bool(output), "form layout report file");
    std::mt19937 random(seed);
    std::vector<uint8_t> formFirst(blocks);
    for (auto& value : formFirst)
        value = static_cast<uint8_t>(random() & 1u);
    output << std::setprecision(17)
           << "{\"schema\":1,\"fixture\":\"dxui-form-layout-cost-v1\",\"classification\":\"diagnostic-within-candidate\","
              "\"renderer\":\"WARP\",\"compiler\":"
           << _MSC_FULL_VER << ",\"pointerBits\":" << sizeof(void*) * 8u << ",\"iteratorDebugLevel\":" << _ITERATOR_DEBUG_LEVEL << ",\"fields\":" << kFieldCount
           << ",\"warmupFrames\":" << kWarmupFrames << ",\"framesPerPhase\":" << kFramesPerPhase << ",\"seed\":" << seed << ",\"blocks\":" << blocks
           << ",\"scheduleProtocol\":\"mt19937-independent-order-v1\",\"schedule\":[";
    for (size_t block = 0u; block < blocks; ++block)
    {
        if (block)
            output << ',';
        output << '"' << (formFirst[block] ? "BAAB" : "ABBA") << '"';
    }
    output << "],\"allocationScope\":\"UI-thread C++ calls; live delta excludes aligned blocks and COM/driver heaps\","
              "\"timingScope\":\"layoutAndPrepareMs/threadCycles include phase mutation setters and all layout/preparation; prepareMs excludes manual layout "
              "but includes Form intrinsic layout and is variant-dependent; compositeMs/threadCycles measure submission; completedFrameMs also includes "
              "target bind/clear and blocking one-pixel GPU completion\","
              "\"identityScope\":\"source, fixture, executable and complete build identity must accompany this raw report in the PowerShell packet\","
              "\"deviceRecoveryScope\":\"public ReplaceDevice generation recovery; no hardware removed-device qualification\",\"profiles\":[";
    for (size_t profileIndex = 0u; profileIndex < kProfiles.size(); ++profileIndex)
    {
        const auto& profile = kProfiles[profileIndex];
        GraphicsFixture gpu;
        gpu.width  = profile.width;
        gpu.height = profile.height;
        Hr(gpu.Create(), "form layout WARP fixture");
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "form layout graphics pool");
        CheckParity(gpu, graphics, profile);
        if (profileIndex)
            output << ',';
        output << "{\"name\":\"" << profile.name << "\",\"width\":" << profile.width << ",\"narrowWidth\":" << profile.narrowWidth
               << ",\"height\":" << profile.height << ",\"dpi\":" << profile.dpi << ",\"columns\":" << profile.columns << ",\"density\":\""
               << (profile.density == DxUi::Density::Compact ? "compact" : "standard") << "\",\"flow\":\""
               << (profile.flow == DxUi::FlowDirection::RightToLeft ? "rtl" : "ltr") << "\",\"labels\":\""
               << (profile.labels == DxUi::LabelPlacement::Beside ? "beside" : "above")
               << "\",\"preTimingParity\":{\"states\":5,\"leafHierarchy\":true,\"metadata\":true,\"bounds\":true,\"pixels\":true},\"runs\":[";
        for (size_t block = 0u; block < blocks; ++block)
        {
            for (size_t slot = 0u; slot < 4u; ++slot)
            {
                if (block || slot)
                    output << ',';
                const bool outer      = slot == 0u || slot == 3u;
                const Variant variant = (formFirst[block] != 0u) == outer ? Variant::Form : Variant::Manual;
                MeasureRun(gpu, graphics, profile, variant, block, slot, output);
            }
        }
        output << "],\"lifecycle\":[";
        Lifecycle(gpu, graphics, profile, Variant::Manual, output);
        output << ',';
        Lifecycle(gpu, graphics, profile, Variant::Form, output);
        output << "]}";
        std::cout << "Form layout diagnostic " << profile.name << ": parity + " << blocks << " independent-order blocks completed\n";
    }
    output << "]}";
    output.flush();
    Check(bool(output), "form layout report completed");
}
} // namespace FormLayoutBenchmark
