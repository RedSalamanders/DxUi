#include "../../dxui_private.h"
#if defined(_MSC_VER)
#pragma warning(push, 0) /* Pinned upstream vendor body; DxUi adapter remains /Wall. */
#endif
/*
 * fy-types.c - types definition
 *
 * Copyright (c) 2019 Pantelis Antoniou <pantelis.antoniou@konsulko.com>
 *
 * SPDX-License-Identifier: MIT
 */
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>

#include "../../include/libfyaml.h"

#include "fy-parse.h"

/* parse only types */
FY_PARSE_TYPE_DEFINE_SIMPLE(indent);
FY_PARSE_TYPE_DEFINE_SIMPLE(simple_key);
FY_PARSE_TYPE_DEFINE_SIMPLE(parse_state_log);
FY_PARSE_TYPE_DEFINE_SIMPLE(flow);
FY_PARSE_TYPE_DEFINE_SIMPLE(streaming_alias);

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
