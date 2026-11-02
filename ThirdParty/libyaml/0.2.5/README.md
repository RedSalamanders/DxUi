# Private libyaml 0.2.5

Source: <https://github.com/yaml/libyaml/tree/2c891fc7a770e8ba2fec34fc6b545c672beb37e6> (the upstream 0.2.5 tag). The MIT license is in `LICENSE`.

Original upstream SHA-256 (before the local changes below):

| File | SHA-256 |
| --- | --- |
| include/yaml.h | 5501d24d7b625db5f839481a8ed07069e96ccbd2ab851fb1de748afb4d94972e |
| src/yaml_private.h | 36c02cd7b2ba64b1f2b1bcd7185429eef9e51f2d3831a34379728408cca4d482 |
| src/api.c | ed799eed2f7426534cc89635dcb08d391789a702aee9eee2260d75850fd8ec3d |
| src/parser.c | 01f81aa7730a6e8804983b572b4b3320a01d340683cdde9d9056633b02a87b80 |
| src/reader.c | 6d60e24f41f487ff8b2c678c2b694adb010f00d33d28d27f7a1b6d789e9acc63 |
| src/scanner.c | 9c2a5f3c99ee435ac8276ec2028c1f8263563649a9c6e26885c1dc033a97a934 |
| License | 6c2ea37323f6191cb829c3ae906a878f5c5fafa03b53d4155c0ce8053a5c8944 |

The vendored public header also has trailing whitespace and its final blank line normalized.

All `yaml_` identifiers are mechanically prefixed `dxui_yaml_` to isolate the static implementation. The private header uses a local include and defines the version constants. `api.c` replaces the upstream malloc/realloc/free/strdup wrappers with thread-local scoped allocation budgeting and checked native allocation failure; no longjmp or C++ unwinding is used. Only parser, reader, scanner and common API sources are included. The owning C++ graph separately implements the YAML 1.2 core key and alias rules.

The upstream parser, reader, scanner, and common API body use a local MSVC warning-level push/pop at level 0; the project retains `/Wall` for first-party code. The budgeted allocator and duplication adapter in `api.c` precede that vendor-only scope and remain under `/Wall`. The configuration-header test uses a defined guard so an absent optional config does not produce a diagnostic. No source-specific compiler options are applied.

The parser's optional tag mark is defensively initialized before its existing tag-token assignments, avoiding an MSVC link-time uninitialized-value diagnostic without changing tag locations.
