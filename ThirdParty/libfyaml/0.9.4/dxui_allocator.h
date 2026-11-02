#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct dxui_fy_budget
    {
        size_t used;
        size_t allocations;
        void* allocations_head;
        size_t limit;
        struct dxui_fy_budget* previous;
        void* trap;
        int exceeded;
        int poisoned;
    } dxui_fy_budget;

    void dxui_fy_budget_begin(dxui_fy_budget* budget, size_t limit);
    void dxui_fy_budget_end(dxui_fy_budget* budget);
    dxui_fy_budget* dxui_fy_budget_current(void);
    /* Deterministic native-NULL seam used only by an owned fatal-test child. */
    void dxui_fy_fail_native_after_for_test(size_t successful_allocations);
    void* dxui_fy_malloc(size_t bytes);
    void* dxui_fy_calloc(size_t count, size_t bytes);
    void* dxui_fy_realloc(void* ptr, size_t bytes);
    void dxui_fy_free(void* ptr);
    char* dxui_fy_strdup(const char* str);
    void* dxui_fy_aligned_malloc(size_t bytes, size_t alignment);
    void dxui_fy_aligned_free(void* ptr);
    __declspec(noreturn) void dxui_fy_fatal_allocation(size_t bytes);
    __declspec(noreturn) void dxui_fy_fatal_contract(void);

#ifdef __cplusplus
}
#endif
