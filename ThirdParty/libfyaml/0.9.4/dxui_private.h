/* Included only by libfyaml C files. Keep CRT declarations before macros. */
#pragma once
#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H 1
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "dxui_prefix.h"
#include "dxui_allocator.h"
#define malloc(n) dxui_fy_malloc(n)
#define calloc(n, s) dxui_fy_calloc(n, s)
#define realloc(p, n) dxui_fy_realloc(p, n)
#define free(p) dxui_fy_free(p)
#define _strdup(s) dxui_fy_strdup(s)
#define _aligned_malloc(n, a) dxui_fy_aligned_malloc(n, a)
#define _aligned_free(p) dxui_fy_aligned_free(p)

