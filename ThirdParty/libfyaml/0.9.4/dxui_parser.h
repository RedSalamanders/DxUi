#pragma once

/* The supported C interface is deliberately limited to borrowed-memory events.
   No stream, diagnostic callback, emitter, or application callback is accepted. */
#include "dxui_prefix.h"
#pragma warning(push, 0)
#include "include/libfyaml.h"
#pragma warning(pop)

#ifdef __cplusplus
extern "C" {
#endif

struct fy_parser *dxui_fy_guard_create(void);
int dxui_fy_guard_set_string(struct fy_parser *parser, const char *text, size_t size);
struct fy_event *dxui_fy_guard_parse(struct fy_parser *parser);
void dxui_fy_guard_event_free(struct fy_parser *parser, struct fy_event *event);
void dxui_fy_guard_destroy(struct fy_parser *parser);
const char *dxui_fy_guard_token_text(struct fy_token *token, size_t *size);
const struct fy_tag *dxui_fy_guard_tag_next(struct fy_document_state *state, void **previous);
int dxui_fy_guard_tag_is_default(struct fy_document_state *state, const struct fy_tag *tag);
enum fy_scalar_style dxui_fy_guard_scalar_style(struct fy_token *token);

#ifdef __cplusplus
}
#endif
