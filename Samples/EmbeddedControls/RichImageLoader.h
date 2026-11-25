#pragma once
#include <DxUi/Failure.h>
#include <DxUi/RichImage.h>
#include <algorithm>
#include <array>
#include <mutex>
#include <new>
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
        WaitForRetiredWork();
    }
    RichImageLoader(const RichImageLoader&)            = delete;
    RichImageLoader& operator=(const RichImageLoader&) = delete;
    HRESULT Initialize(HWND window, UINT message, WPARAM cookie, bool allowNetworkFiles = false) noexcept
    try
    {
        const DxUi::Failure::Scope failureScope;
        if (! window || message < WM_APP)
            return E_INVALIDARG;
        auto state = std::make_shared<State>();
        state->retired.reset(CreateEventW(nullptr, TRUE, TRUE, nullptr));
        if (! state->retired)
            return HRESULT_FROM_WIN32(GetLastError());
        state->window            = window;
        state->message           = message;
        state->cookie            = cookie;
        state->allowNetworkFiles = allowNetworkFiles;
        state->work.reset(CreateThreadpoolWork(&State::Worker, state.get(), nullptr));
        if (! state->work)
            return HRESULT_FROM_WIN32(GetLastError());
        Shutdown();
        WaitForRetiredWork();
        _state = std::move(state);
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        DxUi::Failure::FailAllocation();
    }
    void Shutdown() noexcept
    {
        if (! _state)
            return;
        {
            std::lock_guard lock(_state->mutex);
            _state->closing  = true;
            _state->window   = nullptr;
            _state->jobCount = 0u;
            for (auto& job : _state->jobs)
                job = {};
            for (auto& result : _state->results)
                result = {};
            _state->resultCount = _state->resultBytes = 0u;
            _state->hasResults.store(false, std::memory_order_release);
        }
        // An active callback retains only State and its owned job. It checks
        // cancellation and cannot call the view/editor or post after closure.
        // Closing the UI never joins a decoder or waits for a filesystem call.
    }
#if DXUI_ENABLE_DIAGNOSTICS
    // Borrowed handles must remain valid through final worker retirement.
    void SetDecoderGateForTests(HANDLE entered, HANDLE release) noexcept
    {
        std::lock_guard lock(_state->mutex);
        _state->decoderEntered = entered;
        _state->decoderRelease = release;
    }
#endif
    HRESULT Submit(std::span<const DxUi::RichText::RichImageRequest> requests) noexcept
    {
        if (! _state)
            return E_UNEXPECTED;
        std::lock_guard lock(_state->mutex);
        if (_state->closing)
            return HRESULT_FROM_WIN32(ERROR_CANCELLED);
        std::array<DxUi::RichText::RichImageRequest, State::MaximumJobs> staged{};
        std::size_t count{};
        for (std::size_t index = 0u; index < _state->jobCount; ++index)
            if (! _state->jobs[index].Cancelled())
                staged[count++] = _state->jobs[index];
        for (const auto& request : requests)
        {
            if (request == _state->current || std::any_of(_state->results.begin(),
                                                          _state->results.begin() + static_cast<std::ptrdiff_t>(_state->resultCount),
                                                          [&](const auto& result) { return result.request == request; }))
                continue;
            if (request.Cancelled() ||
                std::find(staged.begin(), staged.begin() + static_cast<std::ptrdiff_t>(count), request) != staged.begin() + static_cast<std::ptrdiff_t>(count))
                continue;
            if (count == staged.size())
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
            staged[count++] = request;
        }
        _state->jobs.swap(staged);
        _state->jobCount = count;
        _state->Schedule(_state);
        return S_OK;
    }
    bool HasResults() const noexcept
    {
        return _state && _state->hasResults.load(std::memory_order_acquire);
    }
    void DiscardCancelled() noexcept
    {
        if (! _state)
            return;
        std::lock_guard lock(_state->mutex);
        std::size_t count{};
        for (std::size_t index = 0u; index < _state->jobCount; ++index)
            if (! _state->jobs[index].Cancelled())
                _state->jobs[count++] = _state->jobs[index];
        for (std::size_t index = count; index < _state->jobCount; ++index)
            _state->jobs[index] = {};
        _state->jobCount    = count;
        count               = 0u;
        _state->resultBytes = 0u;
        for (std::size_t index = 0u; index < _state->resultCount; ++index)
            if (! _state->results[index].request.Cancelled())
            {
                if (_state->results[index].image)
                    _state->resultBytes += _state->results[index].image->Pixels().size();
                _state->results[count++] = _state->results[index];
            }
        for (std::size_t index = count; index < _state->resultCount; ++index)
            _state->results[index] = {};
        _state->resultCount = count;
        _state->hasResults.store(count != 0u, std::memory_order_release);
        // Pruning can free the completion reservation that stopped the worker.
        // Still-valid queued requests need the same event-driven wake as Drain.
        _state->Schedule(_state);
    }
    void Drain(std::vector<Result>& output)
    {
        std::vector<Result> staged(MaximumResults);
        staged.resize(Drain(std::span(staged)));
        output.swap(staged);
    }
    std::size_t Drain(std::span<Result> output) noexcept
    {
        if (! _state)
            return 0u;
        std::lock_guard lock(_state->mutex);
        const auto count = std::min(output.size(), _state->resultCount);
        for (std::size_t index = 0u; index < count; ++index)
            output[index] = _state->results[index];
        for (std::size_t index = count; index < _state->resultCount; ++index)
            _state->results[index - count] = _state->results[index];
        const auto remaining = _state->resultCount - count;
        for (std::size_t index = remaining; index < _state->resultCount; ++index)
            _state->results[index] = {};
        _state->resultCount = remaining;
        _state->resultBytes = 0u;
        for (std::size_t index = 0u; index < remaining; ++index)
            if (_state->results[index].image)
                _state->resultBytes += _state->results[index].image->Pixels().size();
        _state->hasResults.store(remaining != 0u, std::memory_order_release);
        _state->Schedule(_state);
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
        wil::unique_handle retired;
        std::mutex mutex;
        std::array<DxUi::RichText::RichImageRequest, MaximumJobs> jobs{};
        std::array<Result, MaximumJobs> results{};
        DxUi::RichText::RichImageRequest current;
        std::size_t jobCount{}, resultCount{}, resultBytes{};
        std::size_t callbacks{};
        std::atomic_bool hasResults{false};
        bool active{}, closing{}, allowNetworkFiles{};
#if DXUI_ENABLE_DIAGNOSTICS
        HANDLE decoderEntered{}, decoderRelease{};
#endif
        HWND window{};
        UINT message{};
        WPARAM cookie{};
        std::shared_ptr<State> callbackLifetime;
        void Schedule(const std::shared_ptr<State>& lifetime) noexcept
        {
            if (active || closing || ! jobCount || resultCount == MaximumJobs || resultBytes > MaximumResultBytes - MaximumImageBytes)
                return;
            if (callbacks == 0u && ! ResetEvent(retired.get()))
                DxUi::Failure::FailContract(HRESULT_FROM_WIN32(GetLastError()));
            ++callbacks;
            callbackLifetime = lifetime;
            active           = true;
            SubmitThreadpoolWork(work.get());
        }
        static void CALLBACK Worker(PTP_CALLBACK_INSTANCE, void* context, PTP_WORK) noexcept
        {
            auto& state = *static_cast<State*>(context);
            // Declared first so retirement is signaled after the failure scope exits.
            const auto retireCallback = wil::scope_exit([&state] noexcept
            {
                std::lock_guard lock(state.mutex);
                if (--state.callbacks == 0u && ! SetEvent(state.retired.get()))
                    DxUi::Failure::FailContract(HRESULT_FROM_WIN32(GetLastError()));
            });
            const DxUi::Failure::Scope failureScope;
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
#if DXUI_ENABLE_DIAGNOSTICS
                HANDLE decoderEntered{}, decoderRelease{};
#endif
                {
                    std::lock_guard lock(state.mutex);
                    if (state.closing || ! state.jobCount || state.resultCount == MaximumJobs || state.resultBytes > MaximumResultBytes - MaximumImageBytes)
                    {
                        state.active = false;
                        state.callbackLifetime.reset();
                        return;
                    }
#if DXUI_ENABLE_DIAGNOSTICS
                    decoderEntered = state.decoderEntered;
                    decoderRelease = state.decoderRelease;
#endif
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
#if DXUI_ENABLE_DIAGNOSTICS
                // Test gate is outside the queue mutex, directly before real decode.
                if (decoderEntered && decoderRelease && SUCCEEDED(factoryResult) && (state.allowNetworkFiles || ! request.FilePath().starts_with(L"\\\\")))
                {
                    if (! SetEvent(decoderEntered))
                        DxUi::Failure::FailContract(HRESULT_FROM_WIN32(GetLastError()));
                    const auto wait = WaitForSingleObject(decoderRelease, INFINITE);
                    if (wait != WAIT_OBJECT_0)
                        DxUi::Failure::FailContract(wait == WAIT_FAILED ? HRESULT_FROM_WIN32(GetLastError()) : E_UNEXPECTED);
                }
#endif
                std::shared_ptr<const DxUi::RichText::RichImage> image;
                const auto kind    = request.SourceKind();
                const auto path    = kind == DxUi::RichText::RichImageSourceKind::NetworkFile ? request.Source() : request.FilePath();
                const bool file    = kind == DxUi::RichText::RichImageSourceKind::LocalFile || kind == DxUi::RichText::RichImageSourceKind::NetworkFile;
                const bool network = kind == DxUi::RichText::RichImageSourceKind::NetworkFile || path.starts_with(L"\\\\");
                const auto status  = ! file                                 ? HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)
                                     : network && ! state.allowNetworkFiles ? E_ACCESSDENIED
                                     : SUCCEEDED(factoryResult) ? DxUi::RichText::RichImage::LoadFile(factory.get(), path, image, request.Cancellation())
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
    // UI closure remains nonblocking; only final teardown or reinitialization
    // waits so the process failure host outlives every worker scope.
    void WaitForRetiredWork() noexcept
    {
        if (_state)
        {
            const auto wait = WaitForSingleObject(_state->retired.get(), 10000u);
            if (wait == WAIT_TIMEOUT)
                DxUi::Failure::FailContract(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
            if (wait != WAIT_OBJECT_0)
                DxUi::Failure::FailContract(wait == WAIT_FAILED ? HRESULT_FROM_WIN32(GetLastError()) : E_UNEXPECTED);
            WaitForThreadpoolWorkCallbacks(_state->work.get(), FALSE);
            _state.reset();
        }
    }
    std::shared_ptr<State> _state;
};
