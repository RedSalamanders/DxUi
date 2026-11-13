#pragma once
// Memory metrics (performance and motion plan W0.3): control size probes, the retained-resource statistics of
// EmbeddedStatistics, and the teardown accounting of the ComplexUi fixture (FX5). Included once, from
// Embedded.Tests.Embedded.cpp, after the benchmark instrument whose counting operator new it reads.
#include <d3d11sdklayers.h>
#include <dxgi1_4.h>

namespace MemoryMetricsTests
{
// Object sizes at 2026-11-13 (x64; ARM64 has the same layout). A probe fails when a control grows; lower it when one
// shrinks. Debug STL containers carry a proxy pointer, so Debug and ASan Debug measure separately from Release.
#if _ITERATOR_DEBUG_LEVEL != 0
inline constexpr size_t kControlBytes     = 336;
inline constexpr size_t kLabelBytes       = 496;
inline constexpr size_t kButtonBytes      = 880;
inline constexpr size_t kToggleBytes      = 984;
inline constexpr size_t kCheckboxBytes    = 992;
inline constexpr size_t kRadioButtonBytes = 928;
#else
inline constexpr size_t kControlBytes     = 296;
inline constexpr size_t kLabelBytes       = 448;
inline constexpr size_t kButtonBytes      = 816;
inline constexpr size_t kToggleBytes      = 904;
inline constexpr size_t kCheckboxBytes    = 912;
inline constexpr size_t kRadioButtonBytes = 864;
#endif
static_assert(sizeof(void*) == 8, "the size probes are measured for 64-bit targets");
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
              << sizeof(DxUi::Toggle) << ", Checkbox " << sizeof(DxUi::Checkbox) << ", RadioButton " << sizeof(DxUi::RadioButton) << " bytes\n";
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
// The FX5 memory receipt (`--memory-receipt <report.json>`, or `--memory-receipt-hardware <report.json>` on the default
// hardware adapter). It is written here, not by the timed benchmark, whose header a paired run compiles against older
// libraries. A hardware adapter reports the process's local (dedicated) and non-local (shared) video memory; WARP
// renders in system memory, which private bytes already count, so there video memory is not applicable.
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
inline void WriteRetained(std::ostream& output, const DxUi::EmbeddedStatistics& stats)
{
    output << "{\"surfaceBytes\":" << stats.surfaceBytes << ",\"cachedBrushes\":" << stats.cachedBrushes << ",\"cachedTextFormats\":" << stats.cachedTextFormats
           << ",\"retainedTextLayouts\":" << stats.retainedTextLayouts << ",\"retainedTextLayoutUnits\":" << stats.retainedTextLayoutUnits
           << ",\"retainedBitmaps\":" << stats.retainedBitmaps << ",\"retainedBitmapBytes\":" << stats.retainedBitmapBytes << '}';
}
inline int64_t Delta(uint64_t after, uint64_t before) noexcept
{
    return static_cast<int64_t>(after) - static_cast<int64_t>(before);
}
__declspec(noinline) inline void WriteComplexUiMemoryReceipt(const wchar_t* outputPath, bool hardware)
{
    const auto entry = ComplexUiBenchmark::Memory();
    GraphicsFixture gpu;
    gpu.width  = 1280;
    gpu.height = 720;
    Hr(gpu.Create(! hardware), "memory receipt device");
    wil::com_ptr_nothrow<IDXGIDevice> dxgi;
    wil::com_ptr_nothrow<IDXGIAdapter> adapter;
    wil::com_ptr_nothrow<IDXGIAdapter3> adapter3;
    Hr(gpu.device->QueryInterface(IID_PPV_ARGS(dxgi.put())), "memory receipt DXGI device");
    Hr(dxgi->GetAdapter(adapter.put()), "memory receipt adapter");
    Hr(adapter->QueryInterface(IID_PPV_ARGS(adapter3.put())), "memory receipt adapter 3");
    DXGI_ADAPTER_DESC1 desc{};
    Hr(adapter3->GetDesc1(&desc), "memory receipt adapter description");
    // Phases: device, scene, warm, scroll, shown, hidden.
    constexpr std::array phaseNames{"device", "scene", "warm", "scroll", "shown", "hidden"};
    std::array<VideoMemory, 6> video{};
    std::array<PROCESS_MEMORY_COUNTERS_EX, 6> process{};
    std::array<DxUi::EmbeddedStatistics, 6> stats{};
    const auto sample = [&](size_t phase, const ComplexUiScene* scene)
    {
        // A released texture leaves video memory once the immediate context submits its destruction.
        gpu.context->Flush();
        video[phase]   = QueryVideoMemory(adapter3.get());
        process[phase] = ComplexUiBenchmark::Memory();
        if (scene)
            stats[phase] = scene->view.GetStatistics();
    };
    sample(0u, nullptr);
    ComplexUiScene scene;
    // Live tracked C++ bytes (the counting operator new and the control heap) from the built model onwards.
    liveBytes      = 0;
    countLiveBytes = true;
    Hr(scene.Initialize(gpu.device.get()), "memory receipt scene");
    const ptrdiff_t trackedScene = liveBytes;
    sample(1u, &scene);
    const auto frames = [&](size_t first, size_t count)
    {
        for (size_t frame = first; frame < first + count; ++frame)
        {
            scene.Update(frame);
            Hr(scene.view.Prepare(1280, 720), "memory receipt preparation");
            gpu.Bind();
            Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "memory receipt composition");
        }
    };
    frames(0u, 20u);
    const ptrdiff_t trackedWarm = liveBytes;
    sample(2u, &scene);
    frames(20u, 200u);
    const ptrdiff_t trackedScroll = liveBytes;
    countLiveBytes                = false;
    sample(3u, &scene);
    sample(4u, &scene);
    scene.view.SetVisible(false);
    sample(5u, &scene);

    if (const std::filesystem::path parent = std::filesystem::path(outputPath).parent_path(); ! parent.empty())
        std::filesystem::create_directories(parent);
    std::ofstream output{std::filesystem::path(outputPath)};
    Check(bool(output), "memory receipt output file");
    output << "{\"compiler\":" << _MSC_FULL_VER << ",\"fixture\":\"dxui-complex-ui-memory-v1\",\"workloadOwner\":\"DxUi\",\"renderer\":\""
           << (hardware ? "Hardware" : "WARP") << "\",\"width\":1280,\"height\":720,\"dpi\":96,\"controls\":83,\"modelRows\":1000,"
           << "\"warmFrames\":20,\"scrollFrames\":200,\"memory\":{\"adapter\":{\"description\":\"";
    for (const wchar_t* c = desc.Description; *c; ++c)
        output << (*c >= 0x20 && *c < 0x7F && *c != L'"' && *c != L'\\' ? static_cast<char>(*c) : '?');
    output << "\",\"software\":" << ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? "true" : "false")
           << ",\"dedicatedVideoMemoryBytes\":" << desc.DedicatedVideoMemory << ",\"sharedSystemMemoryBytes\":" << desc.SharedSystemMemory
           << "},\"videoMemory\":{\"applicable\":" << (video[0].applicable ? "true" : "false");
    if (! video[0].applicable)
        output << ",\"reason\":\"software adapter: rendering uses system memory, which privateBytes counts\"";
    output << "},\"trackedLiveBytes\":{\"scene\":" << trackedScene << ",\"warm\":" << trackedWarm << ",\"scroll\":" << trackedScroll << "},\"phases\":[";
    for (size_t phase = 0u; phase < phaseNames.size(); ++phase)
    {
        if (phase)
            output << ',';
        output << "{\"name\":\"" << phaseNames[phase] << "\",\"privateBytes\":" << process[phase].PrivateUsage
               << ",\"workingSetBytes\":" << process[phase].WorkingSetSize;
        if (video[phase].applicable)
            output << ",\"localVideoUsageBytes\":" << video[phase].local.CurrentUsage << ",\"localVideoBudgetBytes\":" << video[phase].local.Budget
                   << ",\"nonLocalVideoUsageBytes\":" << video[phase].nonLocal.CurrentUsage << ",\"nonLocalVideoBudgetBytes\":" << video[phase].nonLocal.Budget;
        if (phase)
        {
            output << ",\"retained\":";
            WriteRetained(output, stats[phase]);
        }
        output << '}';
    }
    const auto& shown  = stats[4];
    const auto& hidden = stats[5];
    output << "],\"hiddenResidencyDelta\":{\"surfaceBytes\":" << Delta(hidden.surfaceBytes, shown.surfaceBytes)
           << ",\"retainedTextLayouts\":" << Delta(hidden.retainedTextLayouts, shown.retainedTextLayouts)
           << ",\"retainedBitmapBytes\":" << Delta(hidden.retainedBitmapBytes, shown.retainedBitmapBytes)
           << ",\"privateBytes\":" << Delta(process[5].PrivateUsage, process[4].PrivateUsage)
           << ",\"workingSetBytes\":" << Delta(process[5].WorkingSetSize, process[4].WorkingSetSize);
    if (video[4].applicable)
        output << ",\"localVideoBytes\":" << Delta(video[5].local.CurrentUsage, video[4].local.CurrentUsage)
               << ",\"nonLocalVideoBytes\":" << Delta(video[5].nonLocal.CurrentUsage, video[4].nonLocal.CurrentUsage);
    // An embedded view is minimized by hiding it (SetVisible(false)), so its minimized residency is the hidden one.
    output << "},\"minimizedResidency\":\"hidden\",\"entryPrivateBytes\":" << entry.PrivateUsage << ",\"controlBytes\":{\"Control\":" << sizeof(DxUi::Control)
           << ",\"Label\":" << sizeof(DxUi::Label) << ",\"Button\":" << sizeof(DxUi::Button) << ",\"Toggle\":" << sizeof(DxUi::Toggle)
           << ",\"Checkbox\":" << sizeof(DxUi::Checkbox) << ",\"RadioButton\":" << sizeof(DxUi::RadioButton) << "}}}\n";
    output.close();
    Check(bool(output), "memory receipt written");
    // Sanity, not machine values: the fields are present and coherent.
    Check(trackedScene > 0 && trackedWarm > 0 && stats[2].surfaceBytes == 1280ull * 720u * 4u && hidden.surfaceBytes == 0u,
          "memory receipt tracks the scene and its surface");
    Check(video[0].applicable == ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0u), "video memory is applicable exactly on a hardware adapter");
    Check(! video[2].applicable || video[2].local.CurrentUsage > 0u, "a hardware adapter reports local video usage");
    std::cout << "FX5 memory receipt (" << (hardware ? "hardware" : "WARP") << "): tracked " << trackedScene << '/' << trackedWarm << '/' << trackedScroll
              << " bytes; written to " << std::filesystem::path(outputPath).string() << '\n';
}
} // namespace MemoryMetricsTests
