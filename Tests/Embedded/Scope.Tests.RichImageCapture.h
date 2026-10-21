#pragma once
// Deterministic application-harness capture. The sample's actual retained
// editor paints caller-supplied synthetic pixels, including a missing-image
// alternative. File decoding is independently covered by the async loader test.
static void CaptureRichImageSample(const wchar_t* output, bool dark)
{
    GraphicsFixture gpu;
    gpu.width  = 980u;
    gpu.height = 720u;
    Hr(gpu.Create(), "image sample capture WARP device");
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get(), {}, false, true), "actual rich sample scene");
    auto theme          = DxUi::MakeDefaultThemePalette(dark);
    theme.reducedMotion = true;
    scene.view.Controls().SetTheme(theme);
    const std::wstring source = L"# Local image preview\n\n"
                                L"Rich text keeps **Unicode alternatives** and relative Markdown destinations.\n\n"
                                L"![Colour card - \u6771\u4eac \U0001f600](images/colour%20card.png)\n\n"
                                L"- [x] Prepared native pixels; source stays editable\n"
                                L"- [ ] Missing files retain a descriptive alternative\n\n"
                                L"| Clipboard input | Markdown destination |\n| :--- | :--- |\n"
                                L"| Image file or copied path | Relative to the document |\n\n"
                                L"Missing image: ![Descriptive fallback](images/missing.png)\n";
    scene.rich->SetDocumentFilePath(L"C:\\DxUiCapture\\note.md");
    scene.rich->SetText(source);
    const auto prepare = [&] { Hr(PrepareRichEmbeddedContent(scene.view, gpu.width, gpu.height), "image sample cooperative preparation"); };
    prepare();
    constexpr unsigned width = 288u, height = 112u;
    std::vector<std::byte> pixels(width * height * 4u);
    for (unsigned y = 0u; y < height; ++y)
        for (unsigned x = 0u; x < width; ++x)
        {
            const auto at = (static_cast<std::size_t>(y) * width + x) * 4u;
            const auto dx = static_cast<int>(x) - 216, dy = static_cast<int>(y) - 56;
            const bool circle = dx * dx + dy * dy < 32 * 32;
            pixels[at]        = std::byte(circle ? 190u : 40u + x * 120u / width);
            pixels[at + 1u]   = std::byte(circle ? 230u : 50u + y * 150u / height);
            pixels[at + 2u]   = std::byte(circle ? 255u : 220u - x * 120u / width);
            pixels[at + 3u]   = std::byte(255u);
        }
    std::shared_ptr<const DxUi::RichText::RichImage> image;
    Hr(DxUi::RichText::RichImage::CreatePbgra(width, height, pixels, image), "owned synthetic image pixels");
    std::vector<DxUi::RichText::RichImageRequest> requests(0u);
    scene.rich->TakePendingImageRequests(requests);
    Check(requests.size() == 2u, "capture admits real and missing local image destinations");
    for (const auto& request : requests)
        Check(scene.rich->CompleteImageRequest(request, request.FilePath().ends_with(L"colour card.png") ? image : nullptr),
              "current capture image completion");
    prepare();
    Check(scene.rich->GetText() == source && ! scene.rich->CanUndo() && scene.rich->GetImageResourceStatistics().readySources == 1u &&
              scene.rich->GetImageResourceStatistics().failedSources == 1u,
          "capture preserves source/history and explicit missing fallback");
    gpu.Bind();
    if (! dark)
    {
        constexpr float background[]{0.96f, 0.96f, 0.97f, 1.f};
        gpu.context->ClearRenderTargetView(gpu.rtv.get(), background);
    }
    Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "actual image sample composition");
    Hr(gpu.Save(output), "image sample PNG capture");
    std::cout << "captured actual rich-image sample, synthetic caller pixels, theme=" << (dark ? "Dark" : "Light") << '\n';
}
