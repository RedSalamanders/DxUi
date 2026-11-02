#include "dxui_allocator.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

/* Each allocation records its scope so retained parser storage cannot outlive it. */
typedef struct dxui_fy_header {
    dxui_fy_budget *owner;
    void *base;
    size_t size;
    size_t charge;
    struct dxui_fy_header *previous;
    struct dxui_fy_header *next;
} dxui_fy_header;
static __declspec(thread) dxui_fy_budget *dxui_fy_current;
static __declspec(thread) size_t dxui_fy_native_remaining = SIZE_MAX;

void dxui_fy_fail_native_after_for_test(size_t successful_allocations)
{
    dxui_fy_native_remaining = successful_allocations;
}
static void *dxui_fy_native_malloc(size_t bytes)
{
    if (dxui_fy_native_remaining != SIZE_MAX) {
        if (dxui_fy_native_remaining == 0) {
            dxui_fy_native_remaining = SIZE_MAX;
            return NULL;
        }
        --dxui_fy_native_remaining;
    }
    return malloc(bytes);
}

void dxui_fy_budget_begin(dxui_fy_budget *budget, size_t limit)
{
    budget->used = 0;
    budget->allocations = 0;
    budget->allocations_head = NULL;
    budget->limit = limit;
    budget->exceeded = 0;
    budget->poisoned = 0;
    budget->trap = NULL;
    budget->previous = dxui_fy_current;
    dxui_fy_current = budget;
}
void dxui_fy_budget_end(dxui_fy_budget *budget)
{
    if (dxui_fy_current != budget || budget->trap) dxui_fy_fatal_contract();
    /* A refused C operation poisons its parser. No vendor destructor may inspect
       that partial state; the arena owns every allocation and releases it here. */
    if (budget->exceeded) {
        dxui_fy_header *item = (dxui_fy_header *)budget->allocations_head;
        while (item) {
            dxui_fy_header *next = item->next;
            free(item->base);
            item = next;
        }
        budget->allocations_head = NULL;
        budget->allocations = 0;
        budget->used = 0;
    }
    if (budget->used != 0 || budget->allocations != 0 || budget->allocations_head)
        dxui_fy_fatal_contract();
    dxui_fy_current = budget->previous;
}
dxui_fy_budget *dxui_fy_budget_current(void) { return dxui_fy_current; }
static void *dxui_fy_refuse(dxui_fy_budget *owner)
{
    owner->exceeded = 1;
    if (owner->trap) {
        owner->poisoned = 1;
        /* The only trap is in dxui_parser.c, in a still-live C-only call frame.
           Native allocation failure never uses this capacity-refusal path. */
        longjmp(*(jmp_buf *)owner->trap, 1);
    }
    return NULL;
}
static void *dxui_fy_allocate(size_t size, size_t alignment)
{
    dxui_fy_budget *owner = dxui_fy_current;
    if (!owner || owner->used > owner->limit || alignment == 0 || (alignment & (alignment - 1)) != 0)
        dxui_fy_fatal_contract();
    if (alignment < 16u) alignment = 16u;
    if (alignment - 1 > SIZE_MAX - sizeof(dxui_fy_header) ||
        size > SIZE_MAX - sizeof(dxui_fy_header) - (alignment - 1)) {
        return dxui_fy_refuse(owner);
    }
    size_t total = sizeof(dxui_fy_header) + (alignment - 1) + size;
    if (total > owner->limit - owner->used) {
        return dxui_fy_refuse(owner);
    }
    void *base = dxui_fy_native_malloc(total);
    if (!base)
        dxui_fy_fatal_allocation(total);
    uintptr_t value = ((uintptr_t)base + sizeof(dxui_fy_header) + alignment - 1) & ~(uintptr_t)(alignment - 1);
    dxui_fy_header *header = (dxui_fy_header *)value - 1;
    header->owner = owner;
    header->base = base;
    header->size = size;
    header->charge = total;
    header->previous = NULL;
    header->next = (dxui_fy_header *)owner->allocations_head;
    if (header->next) header->next->previous = header;
    owner->allocations_head = header;
    owner->used += total;
    ++owner->allocations;
    return (void *)value;
}
void *dxui_fy_malloc(size_t size) { return dxui_fy_allocate(size, 16u); }
void *dxui_fy_calloc(size_t count, size_t size)
{
    if (size && count > SIZE_MAX / size) {
        if (!dxui_fy_current) dxui_fy_fatal_contract();
        return dxui_fy_refuse(dxui_fy_current);
    }
    size_t bytes = count * size;
    void *ptr = dxui_fy_malloc(bytes);
    if (ptr) memset(ptr, 0, bytes);
    return ptr;
}
void dxui_fy_free(void *ptr)
{
    if (!ptr) return;
    dxui_fy_header *header = (dxui_fy_header *)ptr - 1;
    if (!dxui_fy_current || header->owner != dxui_fy_current || header->charge > header->owner->used)
        dxui_fy_fatal_contract();
    if (header->previous) header->previous->next = header->next;
    else header->owner->allocations_head = header->next;
    if (header->next) header->next->previous = header->previous;
    header->owner->used -= header->charge;
    --header->owner->allocations;
    free(header->base);
}
void *dxui_fy_realloc(void *ptr, size_t size)
{
    if (!ptr) return dxui_fy_malloc(size);
    if (!size) { dxui_fy_free(ptr); return NULL; }
    dxui_fy_header *header = (dxui_fy_header *)ptr - 1;
    if (!dxui_fy_current || header->owner != dxui_fy_current) dxui_fy_fatal_contract();
    size_t old = header->size;
    /* Both native blocks remain live during the copy and count toward the limit. */
    void *replacement = dxui_fy_malloc(size);
    if (!replacement) return NULL;
    memcpy(replacement, ptr, old < size ? old : size);
    dxui_fy_free(ptr);
    return replacement;
}
char *dxui_fy_strdup(const char *str)
{
    size_t size = strlen(str) + 1;
    char *copy = dxui_fy_malloc(size);
    if (copy) memcpy(copy, str, size);
    return copy;
}
void *dxui_fy_aligned_malloc(size_t size, size_t alignment) { return dxui_fy_allocate(size, alignment); }
void dxui_fy_aligned_free(void *ptr) { dxui_fy_free(ptr); }




