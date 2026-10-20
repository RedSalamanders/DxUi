#pragma once
#include <DxUi/Embedded.h>
#include <array>
#include <exception>
#include <wil/result.h>

// This sample uses only installed public headers. The application supplies the device and presents the result.
struct EmbeddedScene
{
    DxUi::EmbeddedHost view;
    DxUi::Toggle* toggle       = nullptr;
    DxUi::Slider* slider       = nullptr;
    DxUi::TextField* text      = nullptr;
    DxUi::RichTextEditor* rich = nullptr;
    bool enabled               = true;
    double intensity           = 65;
    HRESULT Initialize(ID3D11Device* device, DxUi::EmbeddedCallbacks callbacks = {}, bool textInput = false, bool richText = false)
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        RETURN_IF_FAILED(DxUi::GraphicsDevice::Create(device, graphics));
        RETURN_IF_FAILED(view.Attach(std::move(graphics), callbacks));
        auto theme          = DxUi::MakeDefaultThemePalette(true);
        theme.reducedMotion = true;
        view.Controls().SetTheme(theme);
        auto root = std::make_unique<DxUi::Panel>();
        if (richText)
        {
            auto* caption = root->AddChild<DxUi::Label>(L"Unicode rich text - F2 edits code/diagram blocks; Esc finishes; Ctrl+Z/Y undo/redo");
            caption->SetBounds(D2D1::RectF(24, 8, 950, 32));
            auto* status = root->AddChild<DxUi::Label>(L"Click a diagram or code block and press F2. Select ordinary text for formatting actions.");
            status->SetBounds(D2D1::RectF(24, 34, 950, 58));
            rich = root->AddChild<DxUi::RichTextEditor>();
            rich->SetBounds(D2D1::RectF(24, 144, 950, 690));
            rich->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::RichMarkdown);
            rich->SetMarkdownRenderingEnabled(true);
            rich->SetText(L"# Owned Unicode rich text\r\n\r\n"
                          L"Copy an image file in Explorer, or copy its path, then paste here to display it.\r\n\r\n"
                          L"```mermaid\r\ngraph LR;A[Unicode \u03b2]-->|edit|B{Source};B-->A\r\n```\r\n\r\n"
                          L"Edit **bold**, *italic*, ~~deleted~~ and `code`.[^unicode] Try an [underlined link](https://example.invalid).\r\n\r\n"
                          L"<p>Native <b>HTML</b>, <u>underline</u>, <a href='#sample' title='Inert link'>a link</a> and &copy;.</p>\r\n\r\n"
                          L"- [x] A checked task\r\n- [ ] An unchecked task\r\n\r\n"
                          L"## Tables and Unicode\r\n\r\n"
                          L"| Feature | Example | Status |\r\n| :--- | :---: | ---: |\r\n"
                          L"| Unicode | \u6771\u4eac \u03b1\u03b2 \U0001f642 | Ready |\r\n"
                          L"| Empty cell | | Editable |\r\n| Padded cells |\r\n\r\n"
                          L"> A quote with **emphasis** and an [inert link](https://example.invalid).\r\n\r\n"
                          L"9. A long ordered item demonstrates a measured hanging marker column. Wrapped text stays aligned with the first text line.\r\n"
                          L"10. The two-digit marker shares the same text column.\r\n\r\n    A loose continuation keeps its text aligned.\r\n\r\n"
                          L"```cpp\r\n// Source spelling and Unicode are preserved.\r\nconst auto message = L\"Hello\";\r\n```\r\n\r\n"
                          L"An image alternative: ![small example](example.png). Source remains exact.[^unicode][^editing]\r\n\r\n"
                          L"Unsupported HTML stays literal: <span class='custom'>editable source</span>.\r\n\r\n"
                          L">\r\n\r\n- \r\n\r\n"
                          L"[^editing]: Selection and editing use original source positions, even when footers move.\r\n"
                          L"[^unicode]: **Unicode footnotes** retain raw labels, mixed formatting and \u03b2 \U0001f642.\r\n");
            using namespace DxUi::RichText;
            using Action = std::pair<std::wstring_view, MarkdownCommandRequest>;
            const std::array<Action, 18u> actions{{{L"Bold", {.command = MarkdownCommand::ToggleStyle}},
                                                   {L"Italic", {.command = MarkdownCommand::ToggleStyle, .style = MarkdownTextStyle::Emphasis}},
                                                   {L"Strike", {.command = MarkdownCommand::ToggleStyle, .style = MarkdownTextStyle::Strikethrough}},
                                                   {L"Code", {.command = MarkdownCommand::ToggleStyle, .style = MarkdownTextStyle::Code}},
                                                   {L"Heading 2", {.command = MarkdownCommand::SetHeading, .headingLevel = 2u}},
                                                   {L"Bullets", {.command = MarkdownCommand::SetList}},
                                                   {L"Numbered", {.command = MarkdownCommand::SetList, .listKind = MarkdownListKind::Ordered}},
                                                   {L"Task", {.command = MarkdownCommand::ToggleTask}},
                                                   {L"Table", {.command = MarkdownCommand::InsertTable}},
                                                   {L"Paragraph", {.command = MarkdownCommand::SetHeading, .headingLevel = 0u}},
                                                   {L"Indent", {.command = MarkdownCommand::IndentList}},
                                                   {L"Outdent", {.command = MarkdownCommand::OutdentList}},
                                                   {L"Link", {.command = MarkdownCommand::SetLink, .destination = L"#sample", .title = L"Sample link"}},
                                                   {L"Unlink", {.command = MarkdownCommand::RemoveLink}},
                                                   {L"Row +", {.command = MarkdownCommand::InsertTableRow}},
                                                   {L"Row -", {.command = MarkdownCommand::DeleteTableRow}},
                                                   {L"Column +", {.command = MarkdownCommand::InsertTableColumn}},
                                                   {L"Column -", {.command = MarkdownCommand::DeleteTableColumn}}}};
            for (std::size_t index = 0u; index < actions.size(); ++index)
            {
                const auto& [label, request] = actions[index];
                auto* button                 = root->AddChild<DxUi::Button>(std::wstring(label));
                const auto x = 24.f + static_cast<float>(index % 9u) * 103.f, y = 64.f + static_cast<float>(index / 9u) * 38.f;
                button->SetBounds(D2D1::RectF(x, y, x + 97.f, y + 32.f));
                button->SetOnClick([this, status, request]
                {
                    try
                    {
                        const auto result = rich->ExecuteMarkdownCommand(request);
                        status->SetText(result == MarkdownCommandStatus::Unavailable ? L"Choose compatible text, a list item or a table cell for this action."
                                        : result == MarkdownCommandStatus::Changed   ? L"Updated. Ctrl+Z restores the exact original text."
                                                                                     : L"Already in that state.");
                        view.Controls().SetFocusControl(rich, false);
                    }
                    catch (const std::exception&)
                    {
                        status->SetText(L"This edit could not be applied. The original text is preserved.");
                    }
                });
            }
            view.Controls().SetRoot(std::move(root));
            return S_OK;
        }
        auto* heading = root->AddChild<DxUi::Label>(L"Control playground");
        heading->SetBounds(D2D1::RectF(24, 16, 430, 50));
        toggle = root->AddChild<DxUi::Toggle>(L"Enable effect");
        toggle->SetChecked(true);
        toggle->SetBounds(D2D1::RectF(24, 64, 360, 104));
        toggle->SetOnToggled([this](bool checked) { enabled = checked; });
        auto* caption = root->AddChild<DxUi::Label>(L"Effect intensity");
        caption->SetBounds(D2D1::RectF(24, 128, 430, 154));
        slider = root->AddChild<DxUi::Slider>();
        slider->SetValue(intensity);
        slider->SetBounds(D2D1::RectF(24, 160, 440, 208));
        slider->SetOnChange([this](DxUi::SliderChange change) { intensity = change.value; });
        if (textInput)
        {
            auto* label = root->AddChild<DxUi::Label>(L"Profile name");
            label->SetBounds(D2D1::RectF(24, 224, 440, 250));
            text = root->AddChild<DxUi::TextField>();
            text->SetText(L"Desk profile");
            text->SetBounds(D2D1::RectF(24, 260, 440, 308));
        }
        view.Controls().SetRoot(std::move(root));
        return S_OK;
    }
};
