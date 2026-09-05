#pragma once
#include <DxUi/Embedded.h>
#include <wil/result.h>

// This sample uses only installed public headers. The application supplies the device and presents the result.
struct EmbeddedScene
{
    DxUi::EmbeddedHost view;
    DxUi::Toggle* toggle = nullptr;
    DxUi::Slider* slider = nullptr;
    bool enabled         = true;
    double intensity     = 65;
    HRESULT Initialize(ID3D11Device* device, DxUi::EmbeddedCallbacks callbacks = {})
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        RETURN_IF_FAILED(DxUi::GraphicsDevice::Create(device, graphics));
        RETURN_IF_FAILED(view.Attach(std::move(graphics), callbacks));
        auto theme          = DxUi::MakeDefaultThemePalette(true);
        theme.reducedMotion = true;
        view.Controls().SetTheme(theme);
        auto root     = std::make_unique<DxUi::Panel>();
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
        view.Controls().SetRoot(std::move(root));
        return S_OK;
    }
};
