#pragma once
#include <DxUi/DxUi.h>
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace UniformSourceTests
{
inline std::wstring Fixture()
{
    std::wstring source;
    for (unsigned line = 0u; line < 1000u; ++line)
        source += L"A source paragraph with stable widths and ordinary words.\r\n";
    return source;
}
template <typename Check> inline void EditReuse(IDWriteFactory* factory, Check&& check)
{
    using namespace DxUi::RichText;
    Document document(Fixture());
    LayoutOptions options;
    options.widthDip             = 720.f;
    options.lineHeightMultiplier = 1.6f;
    PreparedDocumentLayout prepared;
    prepared.Prepare(factory, document.Snapshot(), document.Revision(), ViewMode::Source, options);
    document.Replace(document.Length() / 2u, document.Length() / 2u + 1u, L"x", {0u, 0u});
    prepared.Prepare(factory, document.Snapshot(), document.Revision(), ViewMode::Source, options);
    const auto work = prepared.LastPreparation();
    std::cout << "uniform edit source=" << document.Length() << " prepared=" << work.preparedSourceUnits << " reused=" << work.reusedChunks
              << " whole=" << work.wholeLayoutSourceUnits << '\n';
    check(work.reusedChunks > 0u && work.preparedSourceUnits <= 2048u && work.wholeLayoutSourceUnits == 0u && ! prepared.UsesWholeGeometry(),
          "custom Source line spacing retains bounded unchanged chunks after one edit");
    PreparedLayout flat;
    flat.Prepare(factory, Project(document.Snapshot(), document.Revision(), ViewMode::Source), options);
    for (std::size_t position = 0u; position <= flat.Content().text.size(); position = std::min(flat.Content().text.size(), position + 127u))
    {
        const auto actual = prepared.Caret(position), expected = flat.Caret(position);
        check(std::abs(actual.y - expected.y) <= .125f && std::abs(actual.height - expected.height) <= .125f,
              "uniform chunk carets retain the whole-layout geometry");
        if (position == flat.Content().text.size())
            break;
    }
}
inline void Measure(IDWriteFactory* factory)
{
    using namespace DxUi::RichText;
    for (unsigned pair = 0u; pair < 12u; ++pair)
        for (unsigned order = 0u; order < 2u; ++order)
        {
            const auto multiplier = ((pair + order) % 2u) ? 1.6f : 0.f;
            Document document(Fixture());
            LayoutOptions options;
            options.widthDip             = 720.f;
            options.lineHeightMultiplier = multiplier;
            PreparedDocumentLayout prepared;
            prepared.Prepare(factory, document.Snapshot(), document.Revision(), ViewMode::Source, options);
            document.Replace(document.Length() / 2u, document.Length() / 2u + 1u, L"x", {0u, 0u});
            const auto start = std::chrono::steady_clock::now();
            auto status      = prepared.BeginPreparation(factory, document.Snapshot(), document.Revision(), ViewMode::Source, options);
            std::size_t advances{};
            while (status == LayoutPreparationStatus::Pending)
            {
                ++advances;
                status = prepared.AdvancePreparation({2u, 256u}).status;
                if (advances > 20000u)
                    throw std::runtime_error("uniform Source measurement exceeded its bounded turn count");
            }
            if (status != LayoutPreparationStatus::Complete)
                throw std::runtime_error("uniform Source measurement did not complete");
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
            const auto work    = prepared.LastPreparation();
            std::cout << "{\"pair\":" << pair << ",\"multiplier\":" << multiplier << ",\"source_units\":" << document.Length()
                      << ",\"prepared_source_units\":" << work.preparedSourceUnits << ",\"reused_chunks\":" << work.reusedChunks
                      << ",\"whole_layout_source_units\":" << work.wholeLayoutSourceUnits << ",\"edit_layout_advances\":" << advances
                      << ",\"elapsed_us\":" << elapsed << "}\n";
        }
}
template <typename Prepare, typename Check> inline void ControlEditReuse(DxUi::ControlHost& host, Prepare&& prepare, Check&& check)
{
    const auto source = Fixture();
    auto root         = std::make_unique<DxUi::Panel>();
    auto* editor      = root->AddChild<DxUi::RichTextEditor>(source);
    editor->SetBounds(D2D1::RectF(0.f, 0.f, 480.f, 240.f));
    editor->SetViewMode(DxUi::RichText::ViewMode::Source);
    DxUi::RichTextEditorAppearance appearance;
    appearance.lineHeightMultiplier = 1.6f;
    editor->SetAppearance(appearance);
    unsigned changes{};
    editor->SetOnTextChanged([&] { ++changes; });
    host.SetRoot(std::move(root));
    prepare();
    editor->SetSelection({source.size() / 2u, source.size() / 2u + 1u});
    check(editor->ReplaceSelection(L"x"), "embedded uniform Source edit is admitted");
    prepare();
    const auto after = editor->DebugGetState();
    check(after.layoutReady && after.reusedGeometryChunks > 0u && after.preparedSourceUnits <= 2048u && after.wholeLayoutSourceUnits == 0u &&
              ! after.wholeGeometryFallback && ! after.wholeRasterFallback,
          "embedded Source edit retains bounded chunk preparation with custom spacing");
    prepare();
    check(editor->DebugGetState().layoutGeneration == after.layoutGeneration && changes == 1u,
          "unchanged embedded preparation retains geometry and emits no source notification");
    check(editor->Undo(), "uniform Source edit preserves undo");
    prepare();
    check(editor->GetText() == source && changes == 2u, "uniform Source undo restores exact source including CRLF");
    host.SetRoot(nullptr);
}
} // namespace UniformSourceTests
