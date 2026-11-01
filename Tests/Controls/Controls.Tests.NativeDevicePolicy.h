#pragma once
#include "Controls.Tests.DxUiTestHelpers.h"
#include <d2d1_3.h>
#include <dxgi1_6.h>
#include <exception>
#include <thread>

inline void TestWindowHostExplicitWarpPolicyRejectsConflictsAndSurvivesDeviceLoss()
{
    std::exception_ptr failure;
    std::jthread worker([&]
    {
        try
        {
            RequireSucceeded(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "initialize independent native policy thread");
            const auto com      = wil::scope_exit([] { CoUninitialize(); });
            const auto original = DxUi::DebugGetSharedWindowHostAttachmentCountForThread(GetCurrentThreadId());
            Require(original == 0u, "native policy fixture owns an initially unattached thread");
            AttachedHostWindow defaultOwner;
            AttachedHostWindow peer;
            peer.Host().Detach();
            const DxUi::ControlHost::AttachOptions warp{.forceWarp = true};
            Require(! peer.Host().Attach(peer.Hwnd(), warp) && GetLastError() == ERROR_INVALID_STATE,
                    "forced WARP cannot silently replace an attached default graphics pool");
            Require(! defaultOwner.Host().Attach(defaultOwner.Hwnd(), warp) && GetLastError() == ERROR_INVALID_STATE,
                    "idempotent attachment cannot silently promote the graphics policy");
            Require(DxUi::DebugGetSharedWindowHostAttachmentCountForThread(GetCurrentThreadId()) == 1u, "refused requests preserve attachment accounting");
            defaultOwner.Host().Detach();
            Require(peer.Host().Attach(peer.Hwnd(), warp), "first attachment can explicitly choose WARP");
            Require(peer.Host().Attach(peer.Hwnd(), warp), "matching explicit WARP attachment is idempotent");
            Require(defaultOwner.Host().Attach(defaultOwner.Hwnd()), "default peer inherits the established WARP policy");
            auto root    = std::make_unique<DxUi::Panel>();
            auto* editor = root->AddChild<DxUi::RichTextEditor>(L"Native $a_i^2$ and $\\frac{1}{2}$");
            editor->SetBounds(D2D1::RectF(8, 8, 250, 130));
            editor->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::RichMarkdown);
            editor->SetMarkdownRenderingEnabled(true);
            peer.Host().SetRoot(std::move(root));
            const auto isSoftware = [](DxUi::ControlHost& host)
            {
                Require(host.RenderInitialFrameForShow(), "render native WARP frame");
                auto* context = host.GetDeviceContext();
                Require(context != nullptr, "native WARP context exists");
                wil::com_ptr_nothrow<ID2D1Device> device;
                context->GetDevice(device.put());
                wil::com_ptr_nothrow<ID2D1Device2> device2;
                RequireSucceeded(device.query_to(device2.put()), "query native DXGI-capable D2D device");
                wil::com_ptr_nothrow<IDXGIDevice> dxgi;
                RequireSucceeded(device2->GetDxgiDevice(dxgi.put()), "read actual native DXGI device");
                wil::com_ptr_nothrow<IDXGIAdapter> adapter;
                RequireSucceeded(dxgi->GetAdapter(adapter.put()), "read actual native adapter");
                wil::com_ptr_nothrow<IDXGIAdapter1> adapter1;
                RequireSucceeded(adapter.query_to(adapter1.put()), "read native adapter description");
                DXGI_ADAPTER_DESC1 description{};
                RequireSucceeded(adapter1->GetDesc1(&description), "read native adapter flags");
                return (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
            };
            Require(isSoftware(peer.Host()) && isSoftware(defaultOwner.Host()), "explicit and inherited native hosts use actual software adapters");
            const auto generation = peer.Host().GetResourceGeneration();
            peer.Host().DebugSimulateDeviceLoss();
            Require(isSoftware(peer.Host()) && peer.Host().GetResourceGeneration() != generation,
                    "device replacement advances the resource generation and retains WARP");
            Require(isSoftware(defaultOwner.Host()), "peer recovers coherently on the same WARP pool");
            for (unsigned stage : {1u, 2u, 3u})
            {
                peer.Host().DebugSimulateDeviceLoss();
                peer.Host().DebugSetNextSizeResourceFailureForTest(stage, DXGI_ERROR_DEVICE_REMOVED);
                Require(! peer.Host().RenderInitialFrameForShow() && ! peer.Host().DebugHasD2DContext(),
                        "injected size-resource device removal discards the device instead of retaining a broken one");
                const auto failureGeneration = peer.Host().GetResourceGeneration();
                Require(isSoftware(peer.Host()) && peer.Host().GetResourceGeneration() != failureGeneration,
                        "a later frame recreates the failed stage on a fresh WARP generation");
                Require(isSoftware(defaultOwner.Host()), "the shared peer follows each replaced device generation");
            }
            RECT initial{};
            Require(GetWindowRect(peer.Hwnd(), &initial) != FALSE, "read the existing native swap-chain window extent");
            RECT initialClient{};
            Require(GetClientRect(peer.Hwnd(), &initialClient) != FALSE, "read the existing native client extent");
            const auto resizeFailures = peer.Host().DebugGetResizeFailureCount();
            peer.Host().DebugSetNextSizeResourceFailureForTest(4u, DXGI_ERROR_DEVICE_REMOVED);
            Require(SetWindowPos(peer.Hwnd(),
                                 nullptr,
                                 0,
                                 0,
                                 initial.right - initial.left + 37,
                                 initial.bottom - initial.top + 29,
                                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
                    "resize the owned hidden window without activation");
            Require(! peer.Host().RenderInitialFrameForShow() && peer.Host().DebugGetResizeFailureCount() == resizeFailures + 1u &&
                        ! peer.Host().DebugHasD2DContext(),
                    "ResizeBuffers device removal retires the device and rejects the failed frame");
            const auto resizeFailureGeneration = peer.Host().GetResourceGeneration();
            const auto rendersBeforeRecovery   = peer.Host().DebugGetRenderCount();
            Require(isSoftware(peer.Host()) && peer.Host().GetResourceGeneration() != resizeFailureGeneration &&
                        peer.Host().DebugGetRenderCount() > rendersBeforeRecovery,
                    "the next frame recreates the device and renders after ResizeBuffers device removal");
            DxUi::WindowHostBitmapCapture resizedFrame;
            Require(peer.Host().DebugCaptureBitmap(resizedFrame, true) && resizedFrame.widthPx > static_cast<UINT>(initialClient.right - initialClient.left) &&
                        resizedFrame.heightPx > static_cast<UINT>(initialClient.bottom - initialClient.top) && ! resizedFrame.bgraPixels.empty(),
                    "the recovered hidden host produces an actual captured frame");
            Require(isSoftware(defaultOwner.Host()), "the shared WARP peer recovers after resize device removal");
            peer.Host().Detach();
            defaultOwner.Host().Detach();
            Require(DxUi::DebugGetSharedWindowHostAttachmentCountForThread(GetCurrentThreadId()) == original,
                    "native WARP policy releases its last attachment");
        }
        catch (const std::exception&)
        {
            failure = std::current_exception(); // Test-only cross-thread result.
        }
    });
    worker.join();
    if (failure)
        std::rethrow_exception(failure);
}
