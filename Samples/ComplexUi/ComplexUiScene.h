#pragma once
#include <DxUi/Embedded.h>
#include <array>
#include <wil/result.h>

// Synthetic library-owned workload: no application settings, services, plugins or device APIs.
struct ComplexUiModel final : DxUi::IDxGridModel, DxUi::IDxTreeModel
{
    std::array<std::wstring, 1000> names;
    ComplexUiModel()
    {
        for (size_t i = 0; i < names.size(); ++i)
            names[i] = L"Item " + std::to_wstring(i);
    }
    size_t GetRowCount() const noexcept override
    {
        return names.size();
    }
    size_t GetColumnCount() const noexcept override
    {
        return 4;
    }
    DxUi::GridColumnDesc GetColumn(size_t column) const override
    {
        return {std::to_wstring(column), L"Column " + std::to_wstring(column), 170};
    }
    void GetCellData(size_t row, size_t, DxUi::GridCellData& cell) const override
    {
        cell.text = names[row];
    }
    std::optional<size_t> FindRowByStableId(uint64_t id) const noexcept override
    {
        return id < names.size() ? std::optional<size_t>(static_cast<size_t>(id)) : std::nullopt;
    }
    uint64_t GetStableRowId(size_t row) const noexcept override
    {
        return row;
    }
    size_t GetVisibleItemCount() const noexcept override
    {
        return names.size();
    }
    void GetVisibleItem(size_t row, DxUi::TreeItemData& item) const override
    {
        item.id    = row;
        item.text  = names[row];
        item.depth = static_cast<uint32_t>(row % 3);
    }
};

struct ComplexUiScene
{
    ComplexUiModel model;
    std::array<DxUi::Slider*, 16> sliders{};
    std::array<DxUi::ProgressBar*, 16> progress{};
    DxUi::Grid* grid = nullptr;
    // Destroy the view before its borrowed model.
    DxUi::EmbeddedHost view;
    HRESULT Initialize(ID3D11Device* device, DxUi::EmbeddedCallbacks callbacks = {})
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        RETURN_IF_FAILED(DxUi::GraphicsDevice::Create(device, graphics));
        RETURN_IF_FAILED(view.Attach(std::move(graphics), callbacks));
        auto theme          = DxUi::MakeDefaultThemePalette(true);
        theme.reducedMotion = true;
        view.Controls().SetTheme(theme);
        auto root = std::make_unique<DxUi::Panel>();
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            auto* card    = root->AddChild<DxUi::CardPanel>();
            const float x = 8 + static_cast<float>(i % 4) * 316;
            const float y = 8 + static_cast<float>(i / 4) * 86;
            card->SetBounds(D2D1::RectF(x, y, x + 304, y + 78));
            card->AddChild<DxUi::Label>(L"Metric " + std::to_wstring(i))->SetBounds(D2D1::RectF(x + 8, y + 2, x + 155, y + 26));
            auto* toggle = card->AddChild<DxUi::Toggle>(L"Enabled");
            toggle->SetBounds(D2D1::RectF(x + 166, y + 2, x + 296, y + 30));
            toggle->SetChecked(i % 2 == 0);
            sliders[i] = card->AddChild<DxUi::Slider>();
            sliders[i]->SetBounds(D2D1::RectF(x + 8, y + 30, x + 296, y + 58));
            progress[i] = card->AddChild<DxUi::ProgressBar>();
            progress[i]->SetBounds(D2D1::RectF(x + 8, y + 62, x + 296, y + 74));
        }
        auto* tree = root->AddChild<DxUi::Tree>();
        tree->SetBounds(D2D1::RectF(8, 360, 380, 712));
        tree->SetModel(&model);
        grid = root->AddChild<DxUi::Grid>();
        grid->SetBounds(D2D1::RectF(392, 360, 1272, 712));
        grid->SetModel(&model);
        view.Controls().SetRoot(std::move(root));

        for (size_t i = 0; i < sliders.size(); ++i)
            sliders[i]->SetOnChange([this, i](DxUi::SliderChange change) { progress[i]->SetValue(change.value); });
        Update(0);
        return S_OK;
    }
    void Update(size_t frame)
    {
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            const double value = static_cast<double>((frame + i) % 100);
            sliders[i]->SetValue(value);
            progress[i]->SetValue(value);
        }
        grid->EnsureRowVisible((frame * 7) % model.names.size());
        view.MarkDirty();
    }
};
