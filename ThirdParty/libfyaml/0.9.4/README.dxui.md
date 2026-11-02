# libfyaml 0.9.4 private metadata backend

Upstream: https://github.com/pantoniou/libfyaml, tag v0.9.4, commit
`cd5f869cb937d0e9db11073d64e8758a7a7c978a`. `UPSTREAM.sha256` records SHA-256
for every unmodified file extracted from that commit. `LICENSE` is the upstream
MIT license; `LICENSE-xxHash.txt` preserves the bundled xxHash BSD-2-Clause terms.

DxUi uses only the event parser, with borrowed input, quiet diagnostics, mmap,
recycling and accelerators disabled. Normal event objects and the parser are destroyed
before the synchronous thread-local allocation arena ends. Arena allocations
are bounded to 4 MiB; budget refusal is distinct from a native allocation failure,
which enters the SDK fatal bridge. No backend pointers escape the arena. A refusal
may interrupt a partial vendor operation: `dxui_parser.c` catches that refusal
with `setjmp` in a still-live C frame and returns status to its C++ caller. Only
C vendor frames are skipped; no C++ callbacks or owners are inside the fence.
The parser is poisoned, no further vendor operation or destructor may inspect
it, and the arena frees every remaining allocation. The borrowed-memory event
path owns no FILE, OS handle or lock. The fence must never be widened to the
backend's stream, emission, comment-mutation or diagnostic callback utilities,
which can own external resources. Native allocation failure still enters the
fatal bridge directly. A non-refusal with outstanding allocations is
a fatal contract violation. Allocation interposition applies only to this vendor
code and never replaces application allocation functions.

Local changes: private symbol prefixing and allocator interposition; MSVC-only
warning scopes surrounding upstream C bodies; quiet status-only parser diagnostics;
bounded heap storage instead of input-sized stack arrays for version directives
and block scalar trailing breaks; an ARM64 MSVC yield intrinsic; initialization of
optional locals and removal of dead returns for MSVC whole-program analysis; and
renaming the allocator's private `free` operation to avoid macro collision. The
first-party adapter, `dxui_parser.c` and `dxui_allocator.c` retain normal first-party warnings.

The independently tested event-parser source closure is:
`src/lib/fy-atom.c`, `fy-diag.c`, `fy-docstate.c`, `fy-event.c`, `fy-input.c`,
`fy-parse.c`, `fy-token.c`, `fy-types.c`, `fy-input-diag.c`, `fy-parse-diag.c`,
`fy-composer.c`, `fy-doc.c`, `fy-docbuilder.c`, `fy-emit.c`, `fy-path.c`,
`fy-walk.c`, `fy-accel.c`, `fy-dump.c`, `fy-composer-diag.c`, `fy-doc-diag.c`,
`fy-docbuilder-diag.c`; `src/util/fy-blob.c`, `fy-ctype.c`, `fy-utf8.c`, `fy-utils.c`;
and `src/xxhash/xxhash.c`. These paths are relative to this directory. Compile
with the MSVC default C language mode; each vendor translation unit explicitly includes
`dxui_private.h` to define the vendor configuration macros locally. Compile
`dxui_allocator.c` and `dxui_parser.c` as C using their own local includes. Vendor-local includes
are relative to their source files; no project-wide vendor include path is needed. Consumer distribution must include both license files. This
closure is diagnostic evidence, not a substitute for each native profile build.



