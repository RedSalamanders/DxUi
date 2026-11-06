#include "dxui_parser.h"
#include "dxui_allocator.h"
#include <setjmp.h>

enum dxui_fy_operation
{
    DXUI_FY_CREATE,
    DXUI_FY_SET_STRING,
    DXUI_FY_PARSE,
    DXUI_FY_EVENT_FREE,
    DXUI_FY_DESTROY,
    DXUI_FY_TOKEN_TEXT,
    DXUI_FY_TAG_NEXT,
    DXUI_FY_TAG_IS_DEFAULT,
    DXUI_FY_SCALAR_STYLE
};

typedef struct dxui_fy_command
{
    enum dxui_fy_operation operation;
    struct fy_parser* parser;
    struct fy_event* event;
    struct fy_token* token;
    struct fy_document_state* state;
    const struct fy_tag* tag;
    const char* text;
    size_t size;
    size_t* text_size;
    void** previous_tag;
    int status;
    enum fy_scalar_style style;
} dxui_fy_command;

static __declspec(noinline) void dxui_fy_execute(dxui_fy_command* command)
{
    dxui_fy_budget* const budget = dxui_fy_budget_current();
    void* const previous         = budget ? budget->trap : NULL;
    jmp_buf trap;
    if (! budget)
        dxui_fy_fatal_contract();
    if (budget->poisoned)
        return;
    // setjmp lives until the vendor call returns. All skipped frames are C;
    // no C++ owner, callback, lock, FILE, or OS handle is inside this fence.
    // A refused operation leaves a poisoned arena, never a reusable parser.
    if (setjmp(trap) != 0)
    {
        budget->trap = previous;
        return;
    }
    budget->trap = &trap;
    switch (command->operation)
    {
        case DXUI_FY_CREATE:
        {
            struct fy_parse_cfg config = {0};
            config.flags    = FYPCF_QUIET | FYPCF_DISABLE_MMAP_OPT | FYPCF_DISABLE_ACCELERATORS | FYPCF_DISABLE_RECYCLING | FYPCF_DISABLE_DEPTH_LIMIT;
            command->parser = fy_parser_create(&config);
            break;
        }
        case DXUI_FY_SET_STRING: command->status = fy_parser_set_string(command->parser, command->text, command->size); break;
        case DXUI_FY_PARSE: command->event = fy_parser_parse(command->parser); break;
        case DXUI_FY_EVENT_FREE: fy_parser_event_free(command->parser, command->event); break;
        case DXUI_FY_DESTROY: fy_parser_destroy(command->parser); break;
        case DXUI_FY_TOKEN_TEXT: command->text = fy_token_get_text(command->token, command->text_size); break;
        case DXUI_FY_TAG_NEXT: command->tag = fy_document_state_tag_directive_iterate(command->state, command->previous_tag); break;
        case DXUI_FY_TAG_IS_DEFAULT: command->status = fy_document_state_tag_is_default(command->state, command->tag); break;
        case DXUI_FY_SCALAR_STYLE: command->style = fy_token_scalar_style(command->token); break;
        default: dxui_fy_fatal_contract();
    }
    budget->trap = previous;
}

struct fy_parser* dxui_fy_guard_create(void)
{
    dxui_fy_command command = {0};
    command.operation       = DXUI_FY_CREATE;
    dxui_fy_execute(&command);
    return command.parser;
}
int dxui_fy_guard_set_string(struct fy_parser* parser, const char* text, size_t size)
{
    dxui_fy_command command = {0};
    command.operation       = DXUI_FY_SET_STRING;
    command.parser          = parser;
    command.text            = text;
    command.size            = size;
    command.status          = -1;
    dxui_fy_execute(&command);
    return command.status;
}
struct fy_event* dxui_fy_guard_parse(struct fy_parser* parser)
{
    dxui_fy_command command = {0};
    command.operation       = DXUI_FY_PARSE;
    command.parser          = parser;
    dxui_fy_execute(&command);
    return command.event;
}
void dxui_fy_guard_event_free(struct fy_parser* parser, struct fy_event* event)
{
    dxui_fy_command command = {0};
    command.operation       = DXUI_FY_EVENT_FREE;
    command.parser          = parser;
    command.event           = event;
    dxui_fy_execute(&command);
}
void dxui_fy_guard_destroy(struct fy_parser* parser)
{
    dxui_fy_command command = {0};
    command.operation       = DXUI_FY_DESTROY;
    command.parser          = parser;
    dxui_fy_execute(&command);
}
const char* dxui_fy_guard_token_text(struct fy_token* token, size_t* size)
{
    dxui_fy_command command = {0};
    command.operation       = DXUI_FY_TOKEN_TEXT;
    command.token           = token;
    command.text_size       = size;
    dxui_fy_execute(&command);
    return command.text;
}
const struct fy_tag* dxui_fy_guard_tag_next(struct fy_document_state* state, void** previous)
{
    dxui_fy_command command = {0};
    command.operation       = DXUI_FY_TAG_NEXT;
    command.state           = state;
    command.previous_tag    = previous;
    dxui_fy_execute(&command);
    return command.tag;
}
int dxui_fy_guard_tag_is_default(struct fy_document_state* state, const struct fy_tag* tag)
{
    dxui_fy_command command = {0};
    command.operation       = DXUI_FY_TAG_IS_DEFAULT;
    command.state           = state;
    command.tag             = tag;
    dxui_fy_execute(&command);
    return command.status;
}
enum fy_scalar_style dxui_fy_guard_scalar_style(struct fy_token* token)
{
    dxui_fy_command command = {0};
    command.operation       = DXUI_FY_SCALAR_STYLE;
    command.token           = token;
    dxui_fy_execute(&command);
    return command.style;
}
