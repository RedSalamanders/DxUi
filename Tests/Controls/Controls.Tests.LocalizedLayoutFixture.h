#pragma once

// Consumer-independent workload. Keep this fixture identical across the baseline and candidate;
// the caller supplies the layout strategy, not the text, measurements or acceptance assertions.
template <typename Arrange> void RunLocalizedActionFixture(Arrange&& arrange)
{
    constexpr std::array<std::wstring_view, 4> labels{L"Conserver les deux versions du document",
                                                      L"Remplacer la version existante uniquement après avoir vérifié toutes les informations affichées",
                                                      L"Ignorer cet élément et poursuivre",
                                                      L"Annuler cette opération"};
    constexpr std::array widths{480.0f, 640.0f, 760.0f};
    constexpr std::array scales{1.0f, 1.5f, 2.0f, 1.0f};
    constexpr float gap = 8.0f;
    struct Scenario
    {
        float width = 0.0f;
        std::array<D2D1_SIZE_F, labels.size()> sizes{};
    };
    std::array<Scenario, widths.size() * scales.size()> scenarios{};
    wil::com_ptr<IDWriteFactory> factory;
    Require(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put()))),
            "localized fixture factory");
    wil::com_ptr<IDWriteTextFormat> format;
    Require(SUCCEEDED(factory->CreateTextFormat(
                L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"fr-FR", format.put())),
            "localized fixture font");
    Require(SUCCEEDED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP)), "localized fixture wrapping");
    size_t scenarioIndex = 0;
    for (const float scale : scales)
    {
        for (const float width : widths)
        {
            auto& scenario = scenarios[scenarioIndex++];
            scenario.width = width;
            for (size_t index = 0; index < labels.size(); ++index)
            {
                wil::com_ptr<IDWriteTextLayout> layout;
                Require(SUCCEEDED(factory->CreateTextLayout(
                            labels[index].data(), static_cast<UINT32>(labels[index].size()), format.get(), width - 24.0f, 4096.0f, layout.put())),
                        "localized fixture text layout");
                DWRITE_TEXT_METRICS metrics{};
                Require(SUCCEEDED(layout->GetMetrics(&metrics)), "localized fixture metrics");
                scenario.sizes[index] = D2D1::SizeF((std::min)(width, std::ceil((metrics.widthIncludingTrailingWhitespace + 24.0f) * scale) / scale),
                                                    (std::max)(32.0f, std::ceil((metrics.height + 16.0f) * scale) / scale));
            }
        }
    }
    std::array<D2D1_RECT_F, labels.size()> bounds{};
    const auto workload = [&]()
    {
        double checksum = 0.0;
        for (size_t repeat = 0; repeat < 2000; ++repeat)
        {
            for (const auto& scenario : scenarios)
            {
                checksum += arrange(scenario.sizes, scenario.width, gap, bounds);
                checksum += bounds.back().bottom;
            }
        }
        return checksum;
    };
    const double warmup = workload();
    for (size_t round = 0; round < 5; ++round)
    {
        const auto begin      = std::chrono::steady_clock::now();
        const double checksum = workload() + warmup;
        const auto elapsed    = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count();
        std::cout << "LOCALIZED_LAYOUT round=" << round << " layouts=24000 us=" << elapsed << " checksum=" << checksum << '\n';
    }
    bool fits = true;
    for (const auto& scenario : scenarios)
    {
        const float height = arrange(scenario.sizes, scenario.width, gap, bounds);
        for (size_t index = 0; index < bounds.size(); ++index)
        {
            const auto rect = bounds[index];
            fits            = fits && rect.left >= 0.0f && rect.top >= 0.0f && rect.right <= scenario.width && rect.bottom <= height &&
                              rect.right - rect.left + 0.01f >= scenario.sizes[index].width && rect.bottom - rect.top + 0.01f >= scenario.sizes[index].height;
            if (index > 0)
            {
                const auto previous = bounds[index - 1];
                fits                = fits && ((rect.top == previous.top && rect.left >= previous.right + gap) || rect.top >= previous.bottom + gap);
            }
        }
    }
    Require(fits, "French actions retain measured text, stay within width, preserve order and never overlap");
}
