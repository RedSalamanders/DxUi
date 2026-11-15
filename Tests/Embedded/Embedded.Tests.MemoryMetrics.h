#pragma once
// Memory metrics (performance and motion plan W0.3): control size probes, the retained-resource statistics of
// EmbeddedStatistics, and the teardown accounting of the ComplexUi fixture (FX5). Included once, from
// Embedded.Tests.Embedded.cpp, after the benchmark instrument whose counting operator new it reads.
#include <d3d11sdklayers.h>
#include <dxgi1_4.h>

namespace MemoryMetricsTests
{
// Size probes (W0.3, extended by W9.1): object sizes measured on 2026-11-14 by compiling x64 and ARM64 Release, Debug and
// ASan Debug, which agree per configuration. A probe is a ratchet, not a budget: it fails when a type grows; lower it when
// one shrinks. Debug STL containers carry a proxy pointer, so Debug and ASan Debug measure separately from Release.
// The controls serve the G13 line "<= 256 B per simple control all-in" (plan section 1), which today's sizes already
// exceed: W9.6 lowers them. ThroughputGraphSample is the graph's per-sample record (180 per graph), held at its size by
// the same line; no plan item lowers it yet. The accessibility snapshot record is probed beside its private definition
// (src/Controls/DxUi.Accessibility.cpp) and W9.7 lowers it.
// W6.2 (2026-11-14) raised every control probe by 8 bytes: Control::_measureCache, the pointer to the out-of-line measure
// cache that replaced the per-parent FlexPanel and FormField measurement records; a control never measured pays only it.
#if _ITERATOR_DEBUG_LEVEL != 0
inline constexpr size_t kControlBytes     = 344;
inline constexpr size_t kLabelBytes       = 504;
inline constexpr size_t kButtonBytes      = 888;
inline constexpr size_t kToggleBytes      = 992;
inline constexpr size_t kCheckboxBytes    = 1000;
inline constexpr size_t kRadioButtonBytes = 936;
#else
inline constexpr size_t kControlBytes     = 304;
inline constexpr size_t kLabelBytes       = 456;
inline constexpr size_t kButtonBytes      = 824;
inline constexpr size_t kToggleBytes      = 912;
inline constexpr size_t kCheckboxBytes    = 920;
inline constexpr size_t kRadioButtonBytes = 872;
#endif
inline constexpr size_t kThroughputGraphSampleBytes = 408; // no container: equal in every configuration
static_assert(sizeof(void*) == 8, "the size probes are measured for 64-bit targets");
static_assert(sizeof(DxUi::ThroughputGraphSample) <= kThroughputGraphSampleBytes,
              "ThroughputGraphSample grew: lower memory first, or re-measure with the reason");
static_assert(sizeof(DxUi::Control) <= kControlBytes,
              "Control grew: lower memory first, or re-measure with the reason (Specs/Core/Core_PerformanceAndResources.md)");
static_assert(sizeof(DxUi::Label) <= kLabelBytes, "Label grew: lower memory first, or re-measure with the reason");
static_assert(sizeof(DxUi::Button) <= kButtonBytes, "Button grew: lower memory first, or re-measure with the reason");
static_assert(sizeof(DxUi::Toggle) <= kToggleBytes, "Toggle grew: lower memory first, or re-measure with the reason");
static_assert(sizeof(DxUi::Checkbox) <= kCheckboxBytes, "Checkbox grew: lower memory first, or re-measure with the reason");
static_assert(sizeof(DxUi::RadioButton) <= kRadioButtonBytes, "RadioButton grew: lower memory first, or re-measure with the reason");

inline void PrintControlSizes()
{
    std::cout << "Control sizes: Control " << sizeof(DxUi::Control) << ", Label " << sizeof(DxUi::Label) << ", Button " << sizeof(DxUi::Button) << ", Toggle "
              << sizeof(DxUi::Toggle) << ", Checkbox " << sizeof(DxUi::Checkbox) << ", RadioButton " << sizeof(DxUi::RadioButton) << ", ThroughputGraphSample "
              << sizeof(DxUi::ThroughputGraphSample) << " bytes\n";
}

// Live objects the D3D11 debug layer reports for a device, its own ID3D11Device record excluded. Objects that only
// the runtime still references are ignored, as they leave with the device. nullopt: the device has no debug layer.
inline std::optional<size_t> CountLiveDeviceObjects(ID3D11Device* device, ID3D11DeviceContext* context)
{
    wil::com_ptr_nothrow<ID3D11Debug> debug;
    wil::com_ptr_nothrow<ID3D11InfoQueue> queue;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(debug.put()))) || FAILED(device->QueryInterface(IID_PPV_ARGS(queue.put()))))
        return std::nullopt;
    // Destruction a pending command still references completes at a flush.
    context->ClearState();
    context->Flush();
    queue->SetMessageCountLimit(static_cast<UINT64>(-1));
    queue->ClearStoredMessages();
    Hr(debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL), "report live device objects");
    size_t objects = 0u;
    std::vector<std::byte> storage;
    for (UINT64 index = 0u, count = queue->GetNumStoredMessages(); index < count; ++index)
    {
        SIZE_T bytes = 0u;
        Hr(queue->GetMessage(index, nullptr, &bytes), "live object message size");
        storage.resize(bytes);
        auto* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
        Hr(queue->GetMessage(index, message, &bytes), "live object message");
        std::string_view text(message->pDescription, message->DescriptionByteLength ? message->DescriptionByteLength - 1u : 0u);
        while (! text.empty() && (text.front() == '\t' || text.front() == ' '))
            text.remove_prefix(1u);
        if (text.starts_with("Live ") && ! text.starts_with("Live ID3D11Device at") && ! text.starts_with("Live Object"))
            ++objects;
    }
    queue->ClearStoredMessages();
    return objects;
}

// One FX5 lifetime: build the 83-control scene, 20 warm frames, 20 changing frames, hide, then destroy it.
inline void RunComplexUiLifetime(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11RenderTargetView* target)
{
    auto scene = std::make_unique<ComplexUiScene>();
    Hr(scene->Initialize(device), "FX5 lifetime scene");
    const D3D11_VIEWPORT viewport{0, 0, 1280, 720, 0, 1};
    for (size_t frame = 0u; frame < 40u; ++frame)
    {
        scene->Update(frame);
        Hr(scene->view.Prepare(1280, 720), "FX5 lifetime preparation");
        context->OMSetRenderTargets(1, &target, nullptr);
        Hr(scene->view.Composite(context, viewport), "FX5 lifetime composition");
    }
    scene->view.SetVisible(false);
    Check(scene->view.GetStatistics().surfaceBytes == 0u, "hidden FX5 view holds no surface");
    scene.reset();
}

// Teardown accounting: after one lifetime has created the process's first-use state (DirectWrite font collections,
// thread-local dispatchers), a second FX5 lifetime returns every byte the counting allocator tracked and, in Debug
// with the D3D11 debug layer, every device object DxUi created.
__declspec(noinline) inline void TestComplexUiTeardownReturnsTrackedMemory()
{
    PrintControlSizes();
    wil::com_ptr_nothrow<ID3D11Device> device;
    wil::com_ptr_nothrow<ID3D11DeviceContext> context;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HRESULT created = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, context.put());
    const bool debugLayer = SUCCEEDED(created) && (flags & D3D11_CREATE_DEVICE_DEBUG) != 0u;
    if (created == DXGI_ERROR_SDK_COMPONENT_MISSING)
        created = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, context.put());
    Hr(created, "teardown WARP device");
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 1280, desc.Height = 720, desc.MipLevels = 1, desc.ArraySize = 1, desc.SampleDesc.Count = 1;
    desc.Format    = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    wil::com_ptr_nothrow<ID3D11Texture2D> texture;
    wil::com_ptr_nothrow<ID3D11RenderTargetView> target;
    Hr(device->CreateTexture2D(&desc, nullptr, texture.put()), "teardown target");
    Hr(device->CreateRenderTargetView(texture.get(), nullptr, target.put()), "teardown target view");

    const auto trackedLifetime = [&]
    {
        liveBytes      = 0;
        countLiveBytes = true;
        RunComplexUiLifetime(device.get(), context.get(), target.get());
        countLiveBytes = false;
        return liveBytes;
    };
    const ptrdiff_t firstUse = trackedLifetime();
    const auto baseline      = debugLayer ? CountLiveDeviceObjects(device.get(), context.get()) : std::nullopt;
    const ptrdiff_t retained = trackedLifetime();
    std::cout << "FX5 teardown: " << retained << " live tracked bytes (first lifetime " << firstUse << " bytes of first-use state)\n";
    Check(retained == 0, "an FX5 lifetime returns every tracked C++ byte at teardown");
    {
        // The tracked delta sees a block left alive, from operator new and from the control heap.
        liveBytes      = 0;
        countLiveBytes = true;
        auto block     = std::make_unique<std::array<std::byte, 64>>();
        auto control   = std::make_unique<DxUi::Label>();
        countLiveBytes = false;
        Check(liveBytes >= static_cast<ptrdiff_t>(64u + sizeof(DxUi::Label)), "the tracked delta counts seeded blocks");
    }
    if (! debugLayer)
    {
        std::cout << "FX5 teardown: live device objects not reported ("
#if defined(_DEBUG)
                  << "D3D11 SDK layers are not installed"
#else
                  << "Debug only"
#endif
                  << ")\n";
        return;
    }
    const auto after = CountLiveDeviceObjects(device.get(), context.get());
    Check(baseline.has_value() && after.has_value(), "debug-layer live object report");
    std::cout << "FX5 teardown: " << (*after - *baseline) << " DxUi-created live device objects (" << *baseline << " fixture objects)\n";
    Check(*baseline >= 3u, "the report lists the fixture's context, target and view");
    Check(*after == *baseline, "an FX5 lifetime releases every device object DxUi created");
    // The report sees an object left alive.
    wil::com_ptr_nothrow<ID3D11Texture2D> seeded;
    Hr(device->CreateTexture2D(&desc, nullptr, seeded.put()), "seeded live texture");
    const auto withSeeded = CountLiveDeviceObjects(device.get(), context.get());
    Check(withSeeded.has_value() && *withSeeded == *baseline + 1u, "the live object report counts a seeded texture");
}

// EmbeddedStatistics counts the layouts and bitmaps controls keep, on query only and without allocating.
__declspec(noinline) inline void TestRetainedResourceStatistics()
{
    // A device of its own: the suite's shared fixture keeps its size for later pixel tests.
    GraphicsFixture gpu;
    gpu.width  = 1280;
    gpu.height = 720;
    Hr(gpu.Create(), "retained statistics WARP device");
    ComplexUiScene scene;
    Hr(scene.Initialize(gpu.device.get()), "retained statistics scene");
    const auto empty = scene.view.GetStatistics();
    Check(empty.retainedTextLayouts == 0u && empty.retainedTextLayoutUnits == 0u && empty.retainedBitmaps == 0u && empty.retainedBitmapBytes == 0u,
          "an unprepared view retains nothing");
    for (size_t frame = 0u; frame < 3u; ++frame)
    {
        scene.Update(frame);
        Hr(scene.view.Prepare(1280, 720), "retained statistics preparation");
    }
    countAllocations       = true;
    const auto allocated   = allocations;
    const auto prepared    = scene.view.GetStatistics();
    const auto queryAllocs = allocations - allocated;
    countAllocations       = false;
    Check(queryAllocs == 0u, "a statistics query allocates nothing");
    const auto grid = scene.grid->DebugGetTextLayoutStatistics();
    // In FX5 only the Grid keeps layouts: its single-line cells, which share no omitted tails. Plain Labels draw
    // without a retained layout, and Toggle, Slider, ProgressBar and Tree keep none.
    std::cout << "FX5 retained: " << prepared.retainedTextLayouts << " text layouts of " << prepared.retainedTextLayoutUnits << " units ("
              << grid.retainedLayouts << " Grid cells), " << prepared.retainedBitmaps << " bitmaps\n";
    Check(grid.retainedLayouts > 0u && grid.displayLayouts == 0u, "FX5 grid retains single-line cell layouts");
    Check(prepared.retainedTextLayouts == grid.retainedLayouts, "retained layouts count every Grid cell layout");
    Check(prepared.retainedTextLayoutUnits >= prepared.retainedTextLayouts * std::wstring_view(L"Item 0").size(), "retained layout units cover the cell text");
    Check(prepared.retainedBitmaps == 0u && prepared.retainedBitmapBytes == 0u, "FX5 retains no bitmaps besides its surface");
    gpu.Bind();
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "retained statistics composition");
    const auto composed = scene.view.GetStatistics();
    Check(composed.retainedTextLayouts == prepared.retainedTextLayouts && composed.retainedTextLayoutUnits == prepared.retainedTextLayoutUnits,
          "composition changes no retained resource");
    scene.view.SetVisible(false);
    const auto hidden = scene.view.GetStatistics();
    Check(hidden.surfaceBytes == 0u && hidden.retainedTextLayouts == 0u && hidden.retainedTextLayoutUnits == 0u,
          "hiding releases the surface and the Grid's layouts");
    scene.view.SetVisible(true);
    Hr(scene.view.Prepare(1280, 720), "retained statistics preparation after show");
    Check(scene.view.GetStatistics().retainedTextLayouts == prepared.retainedTextLayouts, "showing rebuilds the same retained layouts");

    // Controls outside FX5: a TextField keeps its single-line layout, a Label with a typography override keeps one and a
    // plain Label keeps none.
    auto root    = std::make_unique<DxUi::Panel>();
    auto* field  = root->AddChild<DxUi::TextField>();
    auto* styled = root->AddChild<DxUi::Label>(L"Name");
    auto* plain  = root->AddChild<DxUi::Label>(L"Plain");
    field->SetText(L"abcdef");
    field->SetBounds(D2D1::RectF(8, 8, 208, 40));
    Hr(styled->SetTypographyOverride(DxUi::LabelTypography{.fontSizeDip = 15.0f}), "label typography override");
    styled->SetBounds(D2D1::RectF(8, 48, 208, 72));
    plain->SetBounds(D2D1::RectF(8, 80, 208, 104));
    scene.view.Controls().SetRoot(std::move(root));
    Hr(PrepareRichEmbeddedContent(scene.view, 1280, 720), "retained statistics form preparation");
    const auto form = scene.view.GetStatistics();
    std::cout << "Form retained: " << form.retainedTextLayouts << " text layouts of " << form.retainedTextLayoutUnits << " units\n";
    Check(form.retainedTextLayouts == 2u && form.retainedTextLayoutUnits == 10u, "a TextField and a styled Label each retain one layout of their text");
}
// W9.1: largestTextHistoryBytes reports the undo history of the editor that holds the most, and the registry totals count
// a TextField's retained layout; both on query only, without allocating.
__declspec(noinline) inline void TestTextHistoryAndCacheStatistics()
{
    GraphicsFixture gpu;
    gpu.width  = 320;
    gpu.height = 120;
    Hr(gpu.Create(), "text history statistics WARP device");
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "text history graphics device");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(std::move(graphics)), "text history view");
    auto root        = std::make_unique<DxUi::Panel>();
    auto* shortField = root->AddChild<DxUi::TextField>();
    auto* longField  = root->AddChild<DxUi::TextField>();
    shortField->SetBounds(D2D1::RectF(8, 8, 300, 40));
    longField->SetBounds(D2D1::RectF(8, 48, 300, 80));
    longField->SetText(std::wstring(4096u, L'x'));
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(320, 120), "text history preparation");
    const auto before = view.GetStatistics();
    Check(before.largestTextHistoryBytes == 0u, "an unedited field holds no history");
    Check(before.retainedCacheEntries > 0u, "the registry totals count the fields' retained layouts");
    const auto type = [&](DxUi::TextField* field, size_t characters)
    {
        view.Controls().SetFocusControl(field);
        Hr(view.Prepare(320, 120), "text history focus preparation");
        for (size_t index = 0u; index < characters; ++index)
        {
            Check(view.DispatchCharacter(index % 2u ? L' ' : L'a'), "type into the field");
            Hr(view.Prepare(320, 120), "text history typing preparation");
        }
    };
    type(shortField, 4u);
    const auto smallEdited = view.GetStatistics().largestTextHistoryBytes;
    type(longField, 4u);
    countAllocations     = true;
    const auto allocated = allocations;
    const auto edited    = view.GetStatistics();
    const auto queried   = allocations - allocated;
    countAllocations     = false;
    Check(queried == 0u, "a statistics query with text history allocates nothing");
    Check(smallEdited > 0u, "typing records history");
    // The 4,096-unit field keeps at least one earlier text that its current text does not share: 8 KiB.
    Check(edited.largestTextHistoryBytes >= 4096u * sizeof(wchar_t) && edited.largestTextHistoryBytes > smallEdited,
          "the largest history is the field that keeps the larger texts");
    std::cout << "Text history: " << smallEdited << " bytes after 4 edits of a short field, " << edited.largestTextHistoryBytes
              << " bytes after 4 edits of a 4,096-unit field; cache entries " << edited.retainedCacheEntries << ", resident bytes " << edited.retainedCacheBytes
              << '\n';
}

// The busy bytes of every process heap: the sum Support.Tests.HeapDiagnostic.h reports per heap, which that header (an
// input of the paired benchmark) does not total. One heap is locked at a time; nothing is allocated. W9.1 uses it for the
// G13 Grid cell probe (Embedded.Tests.PerformanceFixtures.h).
inline uint64_t LiveHeapBytes() noexcept
{
    std::array<HANDLE, 128> heaps{};
    const DWORD count = GetProcessHeaps(static_cast<DWORD>(heaps.size()), heaps.data());
    uint64_t busy     = 0u;
    for (DWORD index = 0u; index < count && index < heaps.size(); ++index)
    {
        if (! HeapLock(heaps[index]))
            continue;
        PROCESS_HEAP_ENTRY entry{};
        while (HeapWalk(heaps[index], &entry))
            if ((entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0u)
                busy += entry.cbData;
        HeapUnlock(heaps[index]);
    }
    return busy;
}

// Video memory for the memory fields of every performance fixture receipt (Embedded.Tests.PerformanceFixtures.h, which also
// writes the FX5 memory receipt for --memory-receipt and --memory-receipt-hardware). A hardware adapter reports the process's local (dedicated) and non-local
// (shared) video memory; WARP renders in system memory, which private bytes already count, so there video memory is not applicable.
struct VideoMemory
{
    bool applicable = false;
    DXGI_QUERY_VIDEO_MEMORY_INFO local{}, nonLocal{};
};
inline VideoMemory QueryVideoMemory(IDXGIAdapter3* adapter)
{
    VideoMemory memory{};
    DXGI_ADAPTER_DESC1 desc{};
    Hr(adapter->GetDesc1(&desc), "memory receipt adapter description");
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        return memory;
    memory.applicable = true;
    Hr(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory.local), "memory receipt local video memory");
    Hr(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &memory.nonLocal), "memory receipt non-local video memory");
    return memory;
}
} // namespace MemoryMetricsTests
