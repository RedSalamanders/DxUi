#pragma once
#include <DxUi/RichImage.h>
#include <algorithm>
#include <array>
#include <mutex>
#include <wil/com.h>
#include <wil/resource.h>
#include <wincodec.h>

// Sample application service for one editor. The library/control owns no worker.
// One threadpool callback decodes at a time; fixed queues and a 64-MiB completion
// budget stop further work until the owner drains it. No timer or busy polling.
class RichImageLoader final
{
public:
    static constexpr std::size_t MaximumResults = 32u;
    struct Result
    {
        DxUi::RichText::RichImageRequest request;
        std::shared_ptr<const DxUi::RichText::RichImage> image;
        HRESULT status = S_OK;
    };
    RichImageLoader() noexcept = default;
    ~RichImageLoader()
    {
        Shutdown();
    }
    RichImageLoader(const RichImageLoader&)            = delete;
    RichImageLoader& operator=(const RichImageLoader&) = delete;
    HRESULT Initialize(HWND window, UINT message, WPARAM cookie, bool allowNetworkFiles = false) noexcept
    try
    {
        if (! window || message < WM_APP)
            return E_INVALIDARG;
        auto state               = std::make_shared<State>();
        state->window            = window;
        state->message           = message;
        state->cookie            = cookie;
        state->allowNetworkFiles = allowNetworkFiles;
        state->work.reset(CreateThreadpoolWork(&State::Worker, state.get(), nullptr));
        if (! state->work)
            return HRESULT_FROM_WIN32(GetLastError());
        Shutdown();
        state_ = std::move(state);
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    void Shutdown() noexcept
    {
        if (! state_)
            return;
        {
            std::lock_guard lock(state_->mutex);
            state_->closing  = true;
            state_->window   = nullptr;
            state_->jobCount = 0u;
            for (auto& job : state_->jobs)
                job = {};
            for (auto& result : state_->results)
                result = {};
            state_->resultCount = state_->resultBytes = 0u;
            state_->hasResults.store(false, std::memory_order_release);
        }
        // An active callback retains only State and its owned job. It checks
        // cancellation and cannot call the view/editor or post after closure.
        // Closing the UI never joins a decoder or waits for a filesystem call.
        state_.reset();
    }
    HRESULT Submit(std::span<const DxUi::RichText::RichImageRequest> requests) noexcept
    {
        if (! state_)
            return E_UNEXPECTED;
        std::lock_guard lock(state_->mutex);
        if (state_->closing)
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        std::array<DxUi::RichText::RichImageRequest, State::MaximumJobs> staged{};
        std::size_t count{};
        for (std::size_t index = 0u; index < state_->jobCount; ++index)
            if (! state_->jobs[index].Cancelled())
                staged[count++] = state_->jobs[index];
        for (const auto& request : requests)
        {
            if (request == state_->current || std::any_of(state_->results.begin(),
                                                          state_->results.begin() + static_cast<std::ptrdiff_t>(state_->resultCount),
                                                          [&](const auto& result) { return result.request == request; }))
                continue;
            if (request.Cancelled() ||
                std::find(staged.begin(), staged.begin() + static_cast<std::ptrdiff_t>(count), request) != staged.begin() + static_cast<std::ptrdiff_t>(count))
                continue;
            if (count == staged.size())
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            staged[count++] = request;
        }
        state_->jobs.swap(staged);
        state_->jobCount = count;
        state_->Schedule(state_);
        return S_OK;
    }
    bool HasResults() const noexcept
    {
        return state_ && state_->hasResults.load(std::memory_order_acquire);
    }
    void DiscardCancelled() noexcept
    {
        if (! state_)
            return;
        std::lock_guard lock(state_->mutex);
        std::size_t count{};
        for (std::size_t index = 0u; index < state_->jobCount; ++index)
            if (! state_->jobs[index].Cancelled())
                state_->jobs[count++] = state_->jobs[index];
        for (std::size_t index = count; index < state_->jobCount; ++index)
            state_->jobs[index] = {};
        state_->jobCount    = count;
        count               = 0u;
        state_->resultBytes = 0u;
        for (std::size_t index = 0u; index < state_->resultCount; ++index)
            if (! state_->results[index].request.Cancelled())
            {
                if (state_->results[index].image)
                    state_->resultBytes += state_->results[index].image->Pixels().size();
                state_->results[count++] = state_->results[index];
            }
        for (std::size_t index = count; index < state_->resultCount; ++index)
            state_->results[index] = {};
        state_->resultCount = count;
        state_->hasResults.store(count != 0u, std::memory_order_release);
        // Pruning can free the completion reservation that stopped the worker.
        // Still-valid queued requests need the same event-driven wake as Drain.
        state_->Schedule(state_);
    }
    void Drain(std::vector<Result>& output)
    {
        std::vector<Result> staged(MaximumResults);
        staged.resize(Drain(std::span(staged)));
        output.swap(staged);
    }
    std::size_t Drain(std::span<Result> output) noexcept
    {
        if (! state_)
            return 0u;
        std::lock_guard lock(state_->mutex);
        const auto count = std::min(output.size(), state_->resultCount);
        for (std::size_t index = 0u; index < count; ++index)
            output[index] = state_->results[index];
        for (std::size_t index = count; index < state_->resultCount; ++index)
            state_->results[index - count] = state_->results[index];
        const auto remaining = state_->resultCount - count;
        for (std::size_t index = remaining; index < state_->resultCount; ++index)
            state_->results[index] = {};
        state_->resultCount = remaining;
        state_->resultBytes = 0u;
        for (std::size_t index = 0u; index < remaining; ++index)
            if (state_->results[index].image)
                state_->resultBytes += state_->results[index].image->Pixels().size();
        state_->hasResults.store(remaining != 0u, std::memory_order_release);
        state_->Schedule(state_);
        return count;
    }

private:
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
    struct State
    {
        static constexpr std::size_t MaximumJobs = MaximumResults, MaximumResultBytes = 67108864u,
                                     MaximumImageBytes = static_cast<std::size_t>(DxUi::RichText::RichImage::MaximumPixels) * 4u;
        using Work                                     = wil::unique_any<PTP_WORK, decltype(&CloseThreadpoolWork), CloseThreadpoolWork>;
        Work work;
        std::mutex mutex;
        std::array<DxUi::RichText::RichImageRequest, MaximumJobs> jobs{};
        std::array<Result, MaximumJobs> results{};
        DxUi::RichText::RichImageRequest current;
        std::size_t jobCount{}, resultCount{}, resultBytes{};
        std::atomic_bool hasResults{false};
        bool active{}, closing{}, allowNetworkFiles{};
        HWND window{};
        UINT message{};
        WPARAM cookie{};
        std::shared_ptr<State> callbackLifetime;
        void Schedule(const std::shared_ptr<State>& lifetime) noexcept
        {
            if (active || closing || ! jobCount || resultCount == MaximumJobs || resultBytes > MaximumResultBytes - MaximumImageBytes)
                return;
            callbackLifetime = lifetime;
            active           = true;
            SubmitThreadpoolWork(work.get());
        }
        static void CALLBACK Worker(PTP_CALLBACK_INSTANCE, void* context, PTP_WORK) noexcept
        {
            auto& state = *static_cast<State*>(context);
            std::shared_ptr<State> lifetime;
            {
                std::lock_guard lock(state.mutex);
                lifetime = state.callbackLifetime;
            }
            const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            const auto apartment   = wil::scope_exit([=] noexcept
            {
                if (SUCCEEDED(initialized))
                    CoUninitialize();
            });
            wil::com_ptr_nothrow<IWICImagingFactory> factory;
            const auto factoryResult =
                SUCCEEDED(initialized) ? CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())) : initialized;
            for (;;)
            {
                DxUi::RichText::RichImageRequest request;
                {
                    std::lock_guard lock(state.mutex);
                    if (state.closing || ! state.jobCount || state.resultCount == MaximumJobs || state.resultBytes > MaximumResultBytes - MaximumImageBytes)
                    {
                        state.active = false;
                        state.callbackLifetime.reset();
                        return;
                    }
                    request       = state.jobs[0u];
                    state.current = request;
                    for (std::size_t index = 1u; index < state.jobCount; ++index)
                        state.jobs[index - 1u] = state.jobs[index];
                    state.jobs[--state.jobCount] = {};
                }
                if (request.Cancelled())
                {
                    std::lock_guard lock(state.mutex);
                    state.current = {};
                    continue;
                }
                std::shared_ptr<const DxUi::RichText::RichImage> image;
                const auto status = ! state.allowNetworkFiles && request.FilePath().starts_with(L"\\\\") ? E_ACCESSDENIED
                                    : SUCCEEDED(factoryResult)
                                        ? DxUi::RichText::RichImage::LoadFile(factory.get(), request.FilePath(), image, request.Cancellation())
                                        : factoryResult;
                {
                    std::lock_guard lock(state.mutex);
                    state.current = {};
                    if (! state.closing && ! request.Cancelled())
                    {
                        state.results[state.resultCount++] = {request, image, status};
                        if (image)
                            state.resultBytes += image->Pixels().size();
                        state.hasResults.store(true, std::memory_order_release);
                        // PostMessage never invokes the owner synchronously.
                        // Keep the nonblocking post in the same critical section
                        // as closure so a retired HWND cannot receive a late post.
                        if (state.window)
                            PostMessageW(state.window, state.message, state.cookie, 0);
                    }
                }
            }
        }
    };
#pragma warning(pop)
    std::shared_ptr<State> state_;
};
