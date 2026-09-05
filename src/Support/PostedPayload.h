#pragma once
#include <array>
#include <limits>
#include <memory>
#include <mutex>
#include <windows.h>
namespace DxUi::Detail
{
struct PayloadEntry
{
    LPARAM token                    = 0;
    HWND window                     = nullptr;
    void* value                     = nullptr;
    void (*destroy)(void*) noexcept = nullptr;
};
struct PayloadRegistry
{
    std::mutex mutex;
    std::array<PayloadEntry, 128> entries{};
    std::array<HWND, 128> windows{};
    LPARAM next = 1;
    ~PayloadRegistry()
    {
        for (auto& e : entries)
            if (e.destroy)
                e.destroy(e.value);
    }
};
inline PayloadRegistry& Payloads()
{
    static PayloadRegistry registry;
    return registry;
}
template <class T> void DestroyPayload(void* value) noexcept
{
    delete static_cast<T*>(value);
}
} // namespace DxUi::Detail
inline void InitPostedPayloadWindow(HWND hwnd) noexcept
{
    if (! hwnd)
        return;
    auto& r = DxUi::Detail::Payloads();
    std::lock_guard lock(r.mutex);
    for (auto w : r.windows)
        if (w == hwnd)
            return;
    for (auto& w : r.windows)
        if (! w)
        {
            w = hwnd;
            return;
        }
}
inline size_t DrainPostedPayloadsForWindow(HWND hwnd) noexcept
{
    std::array<DxUi::Detail::PayloadEntry, 128> removed{};
    size_t count = 0;
    {
        auto& r = DxUi::Detail::Payloads();
        std::lock_guard lock(r.mutex);
        for (auto& w : r.windows)
            if (w == hwnd)
                w = nullptr;
        for (auto& e : r.entries)
            if (e.token && e.window == hwnd)
            {
                removed[count++] = e;
                e                = {};
            }
    }
    for (size_t i = 0; i < count; ++i)
        removed[i].destroy(removed[i].value);
    return count;
}
template <class T> bool PostMessagePayload(HWND hwnd, UINT msg, WPARAM wp, std::unique_ptr<T> value) noexcept
{
    if (! value)
        return PostMessageW(hwnd, msg, wp, 0) != FALSE;
    auto& r = DxUi::Detail::Payloads();
    std::unique_lock lock(r.mutex);
    bool registered = false;
    for (auto w : r.windows)
        registered = registered || (w && w == hwnd);
    if (! registered || r.next == (std::numeric_limits<LPARAM>::max)())
        return false;
    for (auto& e : r.entries)
        if (! e.token)
        {
            e = {r.next++, hwnd, value.get(), &DxUi::Detail::DestroyPayload<T>};
            if (! PostMessageW(hwnd, msg, wp, e.token))
            {
                e = {};
                return false;
            }
            value.release();
            return true;
        }
    return false;
}
template <class T> std::unique_ptr<T> TakeMessagePayload(LPARAM token) noexcept
{
    DxUi::Detail::PayloadEntry taken{};
    {
        auto& r = DxUi::Detail::Payloads();
        std::lock_guard lock(r.mutex);
        for (auto& e : r.entries)
            if (token && e.token == token)
            {
                taken = e;
                e     = {};
                break;
            }
    }
    if (taken.destroy != &DxUi::Detail::DestroyPayload<T>)
    {
        if (taken.destroy)
            taken.destroy(taken.value);
        return {};
    }
    return std::unique_ptr<T>(static_cast<T*>(taken.value));
}
