#pragma once

#include "../Support/PreparedTreeSemanticFixture.h"

struct PreparedTreeRootLifetimeAccessibilitySite final : DxUi::EmbeddedAccessibilitySite
{
    IRawElementProviderFragmentRoot* root = nullptr; // Borrowed fixture reference; cleared before teardown.

    void ActionCompleted() noexcept override
    {
    }
    HRESULT Navigate(NavigateDirection, IRawElementProviderFragment** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result = nullptr;
        return S_OK;
    }
    HRESULT FragmentRoot(IRawElementProviderFragmentRoot** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result = root;
        if (root)
            root->AddRef();
        return root ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
    }
    HRESULT RequestFocus() noexcept override
    {
        return S_OK;
    }
};

static std::wstring ReadPreparedTreeRootLifetimeName(const wil::com_ptr_nothrow<IRawElementProviderFragment>& provider, const char* context)
{
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    Hr(provider.query_to(simple.put()), context);
    Check(bool(simple), context);
    wil::unique_variant name;
    Hr(simple->GetPropertyValue(UIA_NamePropertyId, &name), context);
    Check(name.vt == VT_BSTR, context);
    return name.bstrVal ? std::wstring(name.bstrVal, SysStringLen(name.bstrVal)) : std::wstring{};
}

static void TestPreparedTreeRootDoesNotRetainSupersededSource(GraphicsFixture& gpu)
{
    using namespace DxUi::Tests;

    auto oldWitness = std::make_shared<PreparedTreeSourceWitness>();
    auto oldSource  = std::make_shared<ThreePreparedTreeRows>(
        ThreePreparedTreeRows::Names{L"Ancienne ligne α", L"Ancienne ligne 東京", L"Ancienne dernière ligne Ω"}, oldWitness);
    {
        const auto oldFirst  = oldSource->GetItem(0u);
        const auto oldMiddle = oldSource->GetItem(1u);
        const auto oldLast   = oldSource->GetItem(2u);
        Check(oldFirst && oldFirst->itemId == 101u && oldFirst->text == L"Ancienne ligne α" && oldMiddle && oldMiddle->itemId == 709u &&
                  oldMiddle->text == L"Ancienne ligne 東京" && oldLast && oldLast->itemId == 9001u && oldLast->text == L"Ancienne dernière ligne Ω",
              "root lifetime fixture starts with literal Unicode names and stable ids");
    }
    ThreePreparedTreeModel model(oldSource);

    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "prepared tree root-lifetime supplied-device scene");
    auto& view    = scene.view;
    auto controls = std::make_unique<DxUi::Panel>();
    auto* tree    = controls->AddChild<DxUi::Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 120.0f));
    tree->SetModel(&model);
    view.Controls().SetRoot(std::move(controls));
    Hr(view.Prepare(320u, 180u, 96.0f), "prepare root-lifetime tree");

    auto site = std::make_shared<PreparedTreeRootLifetimeAccessibilitySite>();
    DxUi::EmbeddedAccessibilityPlacement placement{{0, 0, 320, 180}, true};
    Hr(view.AttachAccessibility(site, 0x524F4F544C494645ull, placement), "attach root-lifetime tree accessibility");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    Hr(view.GetAccessibilityProvider(root.put()), "root-lifetime canonical root");
    Check(bool(root), "root-lifetime canonical root exists");
    site->root = root.get();
    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
    Hr(root.query_to(rootFragment.put()), "root-lifetime canonical root fragment");
    Check(bool(rootFragment), "root-lifetime canonical root fragment exists");

    oldSource.reset();
    auto replacementWitness = std::make_shared<PreparedTreeSourceWitness>();
    auto replacementSource  = std::make_shared<ThreePreparedTreeRows>(
        ThreePreparedTreeRows::Names{L"Nouvelle ligne α", L"Nouvelle ligne 東京", L"Nouvelle dernière ligne Ω et 東京"}, replacementWitness);
    {
        const auto replacementFirst  = replacementSource->GetItem(0u);
        const auto replacementMiddle = replacementSource->GetItem(1u);
        const auto replacementLast   = replacementSource->GetItem(2u);
        Check(replacementFirst && replacementFirst->itemId == 101u && replacementFirst->text == L"Nouvelle ligne α" && replacementMiddle &&
                  replacementMiddle->itemId == 709u && replacementMiddle->text == L"Nouvelle ligne 東京" && replacementLast &&
                  replacementLast->itemId == 9001u && replacementLast->text == L"Nouvelle dernière ligne Ω et 東京",
              "replacement source keeps the same literal ids and publishes new Unicode names");
    }
    model.Replace(replacementSource);
    tree->NotifyDataChanged();
    view.MarkDirty();
    Check(view.Prepare(320u, 180u, 96.0f) == S_OK, "prepare root-lifetime replacement changes the displayed revision");
    Check(view.UpdateAccessibility(placement) == S_OK, "publish root-lifetime replacement creates a new snapshot");
    Check(oldWitness->destructions.load() == 1u && root && rootFragment, "superseded prepared rows retire while the canonical root remains retained");

    wil::com_ptr_nothrow<IRawElementProviderFragment> treeProvider;
    Hr(rootFragment->Navigate(NavigateDirection_FirstChild, treeProvider.put()), "navigate retained root to replacement tree");
    Check(bool(treeProvider), "replacement tree provider exists");
    wil::com_ptr_nothrow<IRawElementProviderFragment> last;
    Hr(treeProvider->Navigate(NavigateDirection_LastChild, last.put()), "navigate replacement tree to last row");
    Check(last && ReadPreparedTreeRootLifetimeName(last, "read replacement last row name") == L"Nouvelle dernière ligne Ω et 東京",
          "retained root resolves the full literal name from the replacement source");

    last.reset();
    treeProvider.reset();
    site->root = nullptr;
    view.Detach();
    wil::com_ptr_nothrow<IRawElementProviderFragment> detachedChild;
    Check(rootFragment->Navigate(NavigateDirection_FirstChild, detachedChild.put()) == UIA_E_ELEMENTNOTAVAILABLE && ! detachedChild,
          "retained canonical root is safely stale after detach");
}
