#pragma once
#include <cmath>

// Independent retained scene: application policy is deliberately absent.
template <typename ConfigureAction, typename ArrangeActions>
static void TestLocalizedShortViewport(GraphicsFixture& gpu, ConfigureAction configureAction, ArrangeActions arrangeActions)
{
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "localized scene attach");
    auto& view = scene.view;
    ComplexUiModel model;
    model.names[0]  = L"Présentation détaillée — 東京 — élément sélectionné";
    auto root       = std::make_unique<DxUi::Panel>();
    auto* body      = root->AddChild<DxUi::ScrollPanel>();
    auto* paragraph = body->AddChild<DxUi::Label>(L"Vérifiez attentivement les informations affichées avant de poursuivre. "
                                                  L"Les détails restent disponibles lorsque la fenêtre est réduite.");
    paragraph->SetMultiline(true);
    auto* value = body->AddChild<DxUi::Label>(L"DocumentationTrèsLongueSansSéparateur_東京_éèàç_0123456789012345678901234567890123456789");
    value->SetMultiline(true);
    auto* checkbox = body->AddChild<DxUi::Checkbox>(L"Appliquer ce choix aux éléments similaires");
    checkbox->SetChecked(true);
    auto* graph = body->AddChild<DxUi::ThroughputGraph>();
    const std::array<DxUi::ThroughputGraphSample, 3> samples{{{12.0}, {18.0}, {15.0}}};
    graph->SetSamples(samples);
    auto* grid = body->AddChild<DxUi::Grid>();
    grid->SetModel(&model);
    grid->GetSelectionModel().SetSingle(0);
    auto* detail = body->AddChild<DxUi::Label>(model.names[0]);
    detail->SetMultiline(true);
    constexpr std::array<std::wstring_view, 4> captions{
        L"Conserver les deux versions", L"Remplacer la version existante", L"Ignorer cet élément", L"Annuler cette opération"};
    std::array<DxUi::Button*, 4> actions{};
    for (size_t index = 0; index < actions.size(); ++index)
    {
        actions[index] = root->AddChild<DxUi::Button>(std::wstring(captions[index]));
        configureAction(*actions[index]);
    }
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(480, 240, 96), "initialize localized typography");
    view.Controls().SetFocusControl(actions.back());
    for (const float dpi : {96.0f, 144.0f, 192.0f, 96.0f})
    {
        for (const float width : {480.0f, 640.0f, 760.0f})
        {
            constexpr float height = 240.0f;
            const UINT widthPx     = static_cast<UINT>(width * dpi / 96.0f);
            const UINT heightPx    = static_cast<UINT>(height * dpi / 96.0f);
            Hr(view.Prepare(widthPx, heightPx, dpi), "refresh localized DPI");
            auto* factory = view.Controls().GetWriteFactory();
            auto* format  = view.Controls().GetTextFormat(DxUi::FontRole::Body, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, true);
            Check(factory && format, "localized scene has current font");
            const auto measure = [&](std::wstring_view text, float available)
            {
                wil::com_ptr<IDWriteTextLayout> layout;
                Hr(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format, available, 4096, layout.put()), "measure French text");
                DWRITE_TEXT_METRICS metrics{};
                Hr(layout->GetMetrics(&metrics), "French text metrics");
                return D2D1::SizeF(std::ceil(metrics.widthIncludingTrailingWhitespace), std::ceil(metrics.height));
            };
            std::array<D2D1_SIZE_F, 4> sizes{};
            std::array<D2D1_RECT_F, 4> bounds{};
            for (size_t index = 0; index < sizes.size(); ++index)
            {
                const auto text = measure(captions[index], width - 48.0f);
                sizes[index]    = D2D1::SizeF((std::min)(width - 24.0f, text.width + 24.0f), (std::max)(32.0f, text.height + 16.0f));
            }
            float actionHeight = 0.0f;
            Hr(arrangeActions(sizes, width - 24.0f, bounds, actionHeight), "localized footer layout");
            for (size_t index = 0; index < sizes.size(); ++index)
            {
                const auto rect = bounds[index];
                Check(rect.left >= 0 && rect.right <= width - 24.0f && rect.right - rect.left >= sizes[index].width &&
                          rect.bottom - rect.top >= sizes[index].height,
                      "localized action bounds contain measured labels inside the viewport");
            }
            const float actionTop = height - 12.0f - actionHeight;
            Check(actionTop > 40.0f, "short viewport retains body space");
            body->SetBounds(D2D1::RectF(12, 12, width - 12, actionTop - 8));
            const float bodyRight = width - 24.0f - body->GetScrollbarThickness();
            float y               = 12.0f;
            const auto place      = [&](DxUi::Control* control, float extent)
            {
                control->SetBounds(D2D1::RectF(12, y, bodyRight, y + extent));
                y += extent + 8.0f;
            };
            place(paragraph, measure(paragraph->GetText(), bodyRight - 12).height);
            place(value, measure(value->GetText(), bodyRight - 12).height);
            place(checkbox, 36);
            place(graph, 80);
            place(grid, 96);
            place(detail, measure(detail->GetText(), bodyRight - 12).height);
            body->SetContentHeight(y - 12);
            body->SetScrollOffset(10000);
            Check(body->NeedsScrollbar() && body->GetScrollOffset() > 0, "short body scrolls independently");
            for (size_t index = 0; index < actions.size(); ++index)
            {
                const auto rect = bounds[index];
                actions[index]->SetBounds(D2D1::RectF(rect.left + 12, rect.top + actionTop, rect.right + 12, rect.bottom + actionTop));
            }
            Hr(view.Prepare(widthPx, heightPx, dpi), "prepare coherent localized layout");
            Check(actions.back()->HasFocus() && checkbox->IsChecked(), "DPI/width reflow preserves focus and checked state");
            for (auto* action : actions)
            {
                const auto rect   = action->GetBounds();
                const auto center = D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f);
                Check(view.Controls().DebugHitTestControl(center) == action, "fixed localized actions retain their hit targets after scrolling");
            }
            const auto previousAllocations = allocations;
            countAllocations               = true;
            const HRESULT composite        = view.Composite(gpu.context.get(), gpu.Viewport());
            countAllocations               = false;
            Hr(composite, "localized scene clean composition");
            Check(allocations == previousAllocations, "localized clean composition allocates nothing");
        }
    }
    graph->SetVisible(false);
    // The caller services one discovery tick after tree/visibility changes; an
    // idle hidden graph must not request subsequent animation work.
    static_cast<void>(view.AdvanceAnimation(GetTickCount64()));
    Hr(view.Prepare(480, 240, 96), "prepare collapsed graph");
    Check(! graph->IsVisible() && actions.back()->HasFocus() && ! view.NeedsAnimation(), "graph hiding leaves action focus intact without visual ticks");
    view.Detach();
}
