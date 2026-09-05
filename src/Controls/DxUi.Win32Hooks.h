#pragma once
#include <windows.h>
namespace DxUi
{
inline WNDPROC GetStoredWndProc(HWND hwnd, const wchar_t* name) noexcept
{
    return reinterpret_cast<WNDPROC>(GetPropW(hwnd, name));
}
inline bool InstallWndProcHook(HWND hwnd, const wchar_t* name, WNDPROC hook) noexcept
{
    if (! hwnd || ! name || ! hook || GetPropW(hwnd, name))
        return false;
    auto previous = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
    if (! previous || ! SetPropW(hwnd, name, reinterpret_cast<HANDLE>(previous)))
        return false;
    SetLastError(0);
    auto result = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hook));
    if (! result && GetLastError())
    {
        RemovePropW(hwnd, name);
        return false;
    }
    return true;
}
inline void RestoreWndProcHook(HWND hwnd, const wchar_t* name) noexcept
{
    if (auto proc = GetStoredWndProc(hwnd, name))
    {
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(proc));
        RemovePropW(hwnd, name);
    }
}
inline LRESULT CallStoredWndProc(HWND hwnd, const wchar_t* name, UINT msg, WPARAM wp, LPARAM lp) noexcept
{
    auto proc = GetStoredWndProc(hwnd, name);
    return proc ? CallWindowProcW(proc, hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}
} // namespace DxUi
