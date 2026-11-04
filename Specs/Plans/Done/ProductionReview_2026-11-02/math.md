# Native math typesetting: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: the native math parser, prepared geometry and
font-table reader (`src/Controls/DxUi.NativeMath.cpp`, `src/Controls/DxUi.PreparedMath.h`,
`src/Controls/DxUi.MathFontTable.h`, `src/Controls/DxUi.NativeMathSymbols.h`, `src/Controls/DxUi.NativeMathDimension.h`,
`include/DxUi/RichMath.h`), the symbol and test-data generators, and the math call sites in the semantic geometry and
document builders.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity. KaTeX behaviour is stated from
the reviewers' knowledge of KaTeX 0.16.11 (the pinned reference), not re-fetched.

Headline: no crash, out-of-bounds read, use-after-free or hang was found. Hostile input is bounded (see "Checked and
holds"). The real defects are typographic (display style lost in the `align` family), exception classification at extreme
font sizes (whole-document fallback instead of per-formula), a literal-`$` text policy, and a cluster of dead or shadowed
code and weak oracles.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| MATH-01 | P2 | Confirmed | `aligned`, `align`, `gather`, `equation`, `split` and `alignat` cells lose display style | `src/Controls/DxUi.PreparedMath.h:1075` | S |
| MATH-02 | P3 | Confirmed | Effective math size above 4096 DIP throws `std::invalid_argument` and drops the whole document to source | `src/Controls/DxUi.PreparedMath.h:2670` | S |
| MATH-03 | P3 | Confirmed | `\text{... $x$ ...}` renders literal dollar signs instead of nested math or source fallback | `src/Controls/DxUi.NativeMath.cpp:1418` | S |
| MATH-04 | P3 | Confirmed | `\bigl`, `\bigr` and `\big` take their atom class from the glyph, not the command | `src/Controls/DxUi.NativeMath.cpp:1081` | S |
| MATH-05 | P3 | Confirmed | Alphabet remapping rewrites `\text` content under `\mathbb`, `\mathsf`, `\mathtt`, `\mathcal`, `\mathfrak` | `src/Controls/DxUi.NativeMath.cpp:150` | S |
| MATH-06 | P3 | Confirmed | A row `\tag` inside `\substack` in an `align` row is accepted and attached to the outer row | `src/Controls/DxUi.NativeMath.cpp:1701` | S |
| MATH-07 | P3 | Confirmed | 91 of 127 hand-written symbol rows are shadowed; the `\ldots` fix in `3ecbd258` is dead code | `src/Controls/DxUi.NativeMath.cpp:1086` | S |
| MATH-08 | P3 | Confirmed | `\|`, `\Vert` and `\vert` atoms and delimiters use inconsistent code points | `src/Controls/DxUi.NativeMath.cpp:1250` | S |
| MATH-09 | P3 | Confirmed | `Finalize` does not reset `variants`; a retried `Advance` duplicates staged variant glyphs (latent) | `src/Controls/DxUi.PreparedMath.h:2558` | S |
| MATH-10 | P3 | Plausible | Matrix row height is measured and placed with different formulas | `src/Controls/DxUi.PreparedMath.h:1464` | S |
| MATH-11 | P3 | Confirmed | Fixed stack arrays grew 8x to 64-68 KiB; colored paint zero-fills 32 KiB and recomputes contrast per op | `src/Controls/DxUi.PreparedMath.h:717` | M |
| MATH-12 | P3 | Confirmed | The parser deep-copies subtrees on every structural wrapper | `src/Controls/DxUi.NativeMath.cpp:873` | S |
| MATH-13 | P3 | Confirmed | Equation `\tag` admits arbitrary atoms whose colors and backgrounds are silently dropped | `src/Controls/DxUi.NativeMath.cpp:1721` | S |
| MATH-14 | P3 | Confirmed | Dead parser branches: `StructuredCommand`, most of `StyledCommand`, the `structured[]` and `styled[]` lists | `src/Controls/DxUi.NativeMath.cpp:949` | S |
| MATH-15 | P3 | Confirmed | A comment claims MATH constants are read "without HRESULT propagation"; they do propagate | `src/Controls/DxUi.RichSemanticGeometry.cpp:342` | S |
| MATH-16 | P3 | Confirmed | Vector-fallback geometry keeps fixed-DIP offsets that do not scale with size or zoom | `src/Controls/DxUi.PreparedMath.h:2143` | S |

## P2 findings

### MATH-01 — `aligned`, `align`, `gather`, `equation`, `split` and `alignat` cells lose display style

- **Severity** P2 (reviewer P1, verifier P2) · **Category** conformance · **Verdict** Confirmed · **Confidence** high
  (code) and high (KaTeX behaviour, about 95 percent) · **Effort** S · **Introduced** the text-style rule for matrices
  predates the window; the `align`, `gather`, `equation` and `alignat` aliases came with `2c60c981` and `660b991f`
  (reviewer-reported)
- **Location**: `src/Controls/DxUi.PreparedMath.h:1073-1085` (`Flatten`, Matrix branch),
  `src/Controls/DxUi.NativeMath.cpp:536-575` (environment canonicalisation), `src/Controls/DxUi.NativeMath.cpp:800`
  (`Substack` emits `Matrix "gathered"`)
- **What**: `Flatten` sets `childDisplay` for Matrix cells only when the core name is `dcases`, `drcases` or starts with
  `darray:`. Every other core, including `aligned`, `alignat` and `gathered`, gets text style. The parser canonicalises
  `align`, `align*` and `split` to `aligned`, `alignat*` and `alignedat` to `alignat`, and `gather`, `gather*`, `equation`
  and `equation*` to `gathered` (`NativeMath.cpp:550-575`). `\substack` shares the `gathered` name (`NativeMath.cpp:800`),
  so a fix keyed on `gathered` would also change substack. Consequences in text style: fractions inside a cell use
  `scaledSize(nextLevel)` for numerator and denominator (`PreparedMath.h:1041-1051`), Scripts boxes lose above/below
  limits because `box.display` is false (`PreparedMath.h:1416-1418`, `:2321-2322`), and large operators are not enlarged
  (`PreparedMath.h:940`). KaTeX parses `aligned`, `alignat`, `split`, `gathered`, `gather` and `equation` with
  `parseArray(..., "display")` regardless of the enclosing style (the KaTeX source comment says gathered "contents are
  set in \displaystyle"); `array`, `matrix` and `cases` use text style; `smallmatrix` and `subarray` use script style.
  amsmath `\substack` is a `subarray`, so its rows are script style (about 90 percent sure). No doc sentence and no test
  pins text style for the `aligned` family; the author handled the explicit `d*` variants, which suggests an oversight.
- **Failure scenario**: `$$\begin{aligned} \sum_{i=1}^n i &= \frac{n(n+1)}{2} \end{aligned}$$` renders the sum with side
  scripts and a text-size fraction. `$$\begin{equation}\int_0^1 \frac{1}{x}\end{equation}$$` likewise. Workaround: a
  `\displaystyle` inside each cell works (`PreparedMath.h:1033-1039`).
- **Fix**: follow KaTeX, which is independent of the enclosing style. In the Matrix branch of `Flatten`
  (`PreparedMath.h:1075`) set `childDisplay` true when `MatrixCore(node.text)` is `aligned`, `alignat` or `gathered`, in
  addition to the existing `d*` names, with no `display &&` term, so inline `$\begin{aligned}...$` is also display style
  (document this in `docs/rich-text.md`). Give `\substack` its own core name first: emit `substack` at
  `NativeMath.cpp:800` and treat it exactly like `subarray` in `Flatten` (`PreparedMath.h:1077-1084`: script level at
  least 1, `boxes[index].display = false`). Add the name wherever `gathered` is enumerated: `MatrixName`
  (`PreparedMath.h:347-349`), the validation at `:832`, the unfenced-name lists at `:1469-1470`, `:2151` and
  `:2202-2203`, and the cell-offset helper that special-cases `aligned` and `alignat` (`:2157`). Without the separate
  name, substack would turn display style when it appears at the top level of display math. An equivalent alternative is
  to emit the already supported `subarray:c` for `\substack`; evaluate row-gap and centring parity before choosing it.
- **Regression test**: `Tests/Support/Support.Tests.MathCorpus.h` (needs the DirectWrite factory it already uses).
  1. `\begin{aligned}\frac{a}{b}\end{aligned}` prepared with `display=true` and again with `display=false` at 24 DIP: the
     numerator glyph `font` equals the root font (today it is `font * scriptScale`).
  2. `\begin{gathered}\sum_{i=1}^n i\end{gathered}` in display: the `n` glyph is horizontally centred over the sum
     (today it sits to the right).
  3. `\sum_{\substack{a\\b}}` and a top-level `$$\substack{a\\b}$$`: rows stay script size.
  4. `\begin{matrix}\frac{a}{b}\end{matrix}` and `\begin{cases}...\end{cases}` still use text style.
- **Verifier notes**: the code trace, the shared substack path and the absence of any pinning test or doc were confirmed.
  The original proposal `display && (...)` was corrected: it keeps text style inline (a deviation from KaTeX) and is only
  safe for substack because that normally sits in a Scripts child (`PreparedMath.h:1057`).
- **Lead note**: severity lowered to P2 because the output is legible and correct mathematics and a user workaround
  exists. P1 remains defensible if the release criteria require KaTeX style parity for the AMS display environments.

## P3 findings

### MATH-02 — Effective math size above 4096 DIP throws `std::invalid_argument` and drops the whole document to source

- **Severity** P3 (reviewer P2, verifier P3) · **Category** bug, contract · **Verdict** Confirmed · **Confidence** high
  (mechanism), reachability only at validator extremes · **Effort** S · **Introduced** the constructor check predates the
  window; the `MathTextFormats::Get` size check came with `a8ada883` (2026-11-01) · same channel as FLOW-01
  (`diagrams.md`) and ARCH-03 (`architecture.md`)
- **Location**: `src/Controls/DxUi.PreparedMath.h:2670-2672` (constructor), `:94-95` (`MathTextFormats::Get`, reached
  from `Advance` at `:2708`); catch sites `src/Controls/DxUi.RichSemanticGeometry.cpp:392` (constructor catches only
  `std::length_error`) and `:632-647` (`PrepareMath` catches only `wil::ResultException` and `std::length_error`);
  outcome at `src/Controls/DxUi.RichTextEditor.cpp:4102-4126`
- **What**: size validation throws `std::invalid_argument("Invalid mathematical preparation request")` and
  `("Invalid mathematical format size")`. `invalid_argument` is not a `length_error`, so it escapes `NativeImageGeometry`
  (constructed inside `Preparation`, `RichSemanticGeometry.cpp:797`) and `PrepareMath`. `PreparedSemanticGeometry::Advance`
  rethrows at `:2845`. The editor's `catch (const std::logic_error&)` (`RichTextEditor.cpp:4102`) matches because
  `invalid_argument` derives from `logic_error`; `richPreparationActive` is true at both `BeginPreparation` (`:3982`) and
  `AdvancePreparation` (`:4000`), so it records `preparationRejected` for the whole `Snapshot()` and shows exact source
  for every paragraph ("exact source is shown for this view key"). The key includes the source snapshot, so each edit
  retries and fails again until the formula or appearance changes. This contradicts `docs/rich-text.md:1686-1688`
  ("falls back only the affected formula ... Other semantic content stays rich").
- **Failure scenario** (valid public settings, exact numbers):
  1. `RichTextEditor::SetAppearance` accepts `bodyFontSizeDip` up to 512 and `headingScaleStep` up to 1.0
     (`RichTextEditor.cpp:1361-1371`); the editor clamps the zoomed body to 512 (`:3968`), and per-heading overrides are
     also capped at 512 (times 1.15 is 589, safe). Layout validation (`RichTextLayout.cpp:194`) has no upper font bound,
     and semantic math is only reachable through the editor.
  2. Heading size is `font * (1 + step * (7 - heading))` (`RichSemanticGeometry.cpp:335-339`), then `* 1.15f`
     (`:379`, `:385`). With body 512, step 1.0, source `# $x$`: 3584 * 1.15 = 4121.6 greater than 4096, so the constructor
     throws. Only level-1 headings qualify (level 2 gives 3532.8), and the window is narrow: body at least about 509 and
     step at least about 0.993.
  3. `\Huge` (`namedScales[10] = 2.488`, `PreparedMath.h:972`) multiplies the root font, not the current size
     (`scaledSize`, `:975-979`), so it does not compound. It needs a root size above 1646.3: a level-1 heading with body
     512 and step at least about 0.30 (or body at least about 205 at step 1.0), source `# $\Huge x$`. `Advance` then
     throws in `MathTextFormats::Get`. Default step 0.15 never reaches it.
  Result: every paragraph of the document shows as source.
- **Fix**: two parts.
  (a) Classify the size condition as capacity. In `PreparedMath.h:2670-2672` split the check so only `size > 4096.f`
  throws `std::length_error`; NaN, non-positive size, DPI and unit-scale failures stay `std::invalid_argument` (caller
  bugs that layout validation already rejects earlier). Make the same split in `MathTextFormats::Get`
  (`PreparedMath.h:94-95`). The existing catches then work: `RichSemanticGeometry.cpp:392` records the element in
  `rejected`, and `PrepareMath` (`:642-647`) converts to `NativeObjectCapacity`. This alone fixes MATH-02. Do not clamp
  derived sizes in `Flatten`; a silent geometry change is worse than a per-formula source fallback. Precedent: the
  diagram path rejects per placement when `state.options.fontSizeDip > 512` (`RichSemanticGeometry.cpp:2377-2383`).
  (b) Add one small shared helper in `RichSemanticGeometry.cpp` that converts a `std::invalid_argument` raised while
  preparing one native element into a per-element reject, and apply it at exactly three sites: the `NativeImageGeometry`
  constructor try block (`:347-396`, push the element into `rejected`), `PrepareMath` (`:578-648`, throw
  `NativeObjectCapacity` for the element), and the diagram `Begin` and `Advance` calls in `PreparedSemanticGeometry::Advance`
  (about `:2396-2410`, `state.Reject(placement.element)` and erase the placement; this is the FLOW-01 site). Do not widen
  the catch to every `std::logic_error`: real invariants such as "Invalid semantic table placement" or "Prepared image
  must produce one atomic SDK rectangle" must stay loud. This is the same escalation channel as FLOW-01 in `diagrams.md`
  and ARCH-03 in `architecture.md`; FLOW-01's root cause (the parser publishes an invalid model) still needs its own
  source fix, and the helper is the safety net for both.
- **Regression test**:
  1. Direct unit in `Tests/Controls/Controls.Tests.RichSemanticDocument.cpp`: `Detail::PreparedMath(factory,
     ParseMath(L"\\Huge x"), 1700.f)`; `Advance()` throws `std::length_error` (today `std::invalid_argument`). The
     existing 4096 case at `:695` must still pass.
  2. Geometry level in `Tests/Support/Support.Tests.RichSemanticGeometry.h`: prepare `# $x$\n\nplain` with
     `fontSizeDip=512`, `headingScaleStep=1`; preparation completes by throwing `NativeObjectsRejected` that names the
     math element (`RichSemanticGeometry.cpp:2832-2833`), not `invalid_argument`.
  3. Editor level in `Tests/Support/Support.Tests.RichSemanticEditor.h`: `SetAppearance` with body 512 and step 1.0
     (and a second case body 512, step 0.3, source `# $\Huge x$`); after preparation settles, the document is not in the
     whole-document `preparationRejected` state, the `plain` paragraph is rich, and the formula element has the
     source-fallback title `EquationViewFallback` with diagnostic `UnsupportedMath`. Do not assert `MathCapacity`: a
     geometry-level rejection rebuilds the formula as `NativeSourceFallback`, which sets `expression` to null, so
     `capacity` is false (`RichSemanticDocument.cpp:1208-1209`, `:1236-1244`).
  Both editor cases fail at `6df17007` (whole-document source) and pass after the fix.
- **Verifier notes**: all steps of the exception path were proved; the reachability arithmetic above is the verifier's.
  Severity lowered because the trigger needs a body font near 512 DIP (about 28 times the 18 DIP default) with
  step near 1, or a level-1 heading `\Huge` at body 512 with step at least 0.30. It is deterministic, self-healing by
  changing appearance or the formula, never crashes, and keeps exact source editable. The contract mismatch is real, so
  fix it opportunistically with MATH-01 or FLOW-01.

### MATH-03 — `\text{... $x$ ...}` renders literal dollar signs instead of nested math or source fallback

- **Severity** P3 (reviewer P2, verifier P3) · **Category** conformance · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S (reject) or M (nested math) · **Introduced** the `allowDollar` parameter came with `6d18fce0`
  (2026-11-01); the literal-`$` policy is pinned by a test
- **Location**: `src/Controls/DxUi.NativeMath.cpp:1298-1299` (`TextColorBody`, `allowDollar`), `:1369-1373` (generic
  append), `:1418` (`TextStyleCommand` passes `true`); pinned by `Tests/Support/Support.Tests.MathCorpus.h:1142`
- **What**: in math-mode `\text{}` the body is parsed with `allowDollar = true`, so an unescaped `$` is appended as an
  ordinary character. KaTeX treats `$...$` inside `\text` as a nested math shift (the `styling` function with
  `allowedInText`) and an unbalanced `$` as an error. `\$` already works (`NativeMath.cpp:1324-1327`). The pinned test
  only covers an unbalanced `\text{a$b}`.
- **Failure scenario**: block math `$$f(x)=\begin{cases}1&\text{if $x>0$}\\0&\text{otherwise}\end{cases}$$` renders
  `if $x>0$` with visible dollar signs and an upright `x` (a single `$` inside block `$$...$$` does not end the formula).
- **Fix**: decide per Question 2. Option 1 (small): pass `allowDollar=false` from `TextStyleCommand` (`:1418`) so the
  formula falls back to exact source, consistent with `RichMath.h:155` ("Unsupported/malformed input is all-or-nothing")
  and with the tag path (`GroupText(true, true)`, `:1715`); update `MathCorpus.h:1142`. Option 2 (medium): a bounded
  nested-math hop (`$` starts `Row(depth + 1)` until the matching `$`, counted against the depth and node budgets). Either
  way document the behaviour in `docs/rich-text.md`.
- **Regression test**: `Tests/Support/Support.Tests.MathCorpus.h`. Option 1: `! ParseMath(L"\\text{if $x$}")` (fails
  today, the formula parses to one Text leaf `if $x$`) and `ParseMath(L"\\text{cost \\$5}")` still parses. Option 2: the
  `x` leaf is italic and no leaf text contains `$`.
- **Verifier notes**: confirmed. Severity lowered because the output is the author's literal text, not wrong
  mathematics, and the literal policy is pinned deliberately. Rejecting converts a readable literal into a whole-formula
  source fallback, which is a UX trade-off, not a pure win. P2 is arguable if "no silently mis-typeset supported syntax"
  is the bar.

### MATH-04 — `\bigl`, `\bigr` and `\big` take their atom class from the glyph, not the command

- **Severity** P3 (reviewer P3, verifier P3) · **Category** conformance · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `f5a2cea9` (2026-10-24), before the window
- **Location**: `src/Controls/DxUi.NativeMath.cpp:1074-1083` (`SymbolCommand`), `:112-127` (`Classify`),
  `src/Controls/DxUi.PreparedMath.h:1335-1339` (`NormalizeRows`)
- **What**: after stripping a trailing `l` or `r`, the node class is `Classify(result.text)`, which looks at the glyph.
  KaTeX `delimsizing` (and TeX) classes `\bigl...` as Open, `\bigr...` as Close, `\bigm...` as Relation and bare
  `\big...` as Ordinary. Only non-bracket delimiters (`|`, `\|`, `\vert`, `\Vert`) and the bare `\big(` or `\big)` forms
  differ; `\bigl(` and `\bigr)` already agree.
- **Failure scenario**: `\bigl| -x \bigr|` keeps the minus Binary (left neighbour Ordinary), so medium spaces appear;
  KaTeX treats it as unary. `\big( -x \big)` is unary here but binary-spaced in TeX. `\bigl\| \sum x` gets an Ord-Op thin
  space where Open-Op has none.
- **Fix**: in `SymbolCommand` derive the class from the command suffix: trailing `l` gives Opening, trailing `r` gives
  Closing, none gives Ordinary. `\bigm` is not an admitted command, so skip the Relation case.
- **Regression test**: `Tests/Support/Support.Tests.MathCorpus.h`: prepare `\bigl|-x\bigr|` and `\bigl(-x\bigr)` at
  24 DIP; the gap between the left delimiter and the minus is equal (today the `|` case has medium spacing).
- **Verifier notes**: `Classify` and `NormalizeRows` traced; confirmed.

### MATH-05 — Alphabet remapping rewrites `\text` content under `\mathbb`, `\mathsf`, `\mathtt`, `\mathcal`, `\mathfrak`

- **Severity** P3 (reviewer P3, verifier P3) · **Category** conformance, consistency · **Verdict** Confirmed ·
  **Confidence** high (code), about 85 percent (KaTeX) · **Effort** S · **Introduced** remapping for `mathfrak` came
  with `7eb88380`; the other alphabets are older
- **Location**: `src/Controls/DxUi.NativeMath.cpp:150-210` (`Alphabet`, skip list `:157-161`), `:989-990`
  (`FinishMathStyle`), `:1778-1781` (`FontSwitchRow` rejects `\text`)
- **What**: `Alphabet` skips only the Style names in its list and rewrites every other `Text` node. `\text{...}` returns a
  bare Text node and named functions are Text too, so `\mathsf{\text{word}}` becomes U+1D5A0-block glyphs and
  `\mathbb{\sin x}` becomes double-struck. The legacy switches reject this case (`{\sf x \text{word}}` retains source,
  `MathCorpus.h:1942`), so behaviour depends on the spelling. `MathCorpus.h:196` asserts `\mathbb{\text{AAAA...}}` is
  admitted (its purpose is the expansion bound). `docs/rich-text.md:1635` ("Text-family commands nested under
  alphabet-remapping switches retain source") is only true for the legacy switches. KaTeX renders text-mode letters in
  the text font, so `\mathbb{\text{Real}}` is text roman.
- **Failure scenario**: `\mathbb{\text{Real}}` shows double-struck glyphs.
- **Fix**: the cheapest correct change is to reject `\text*` inside `FinishMathStyle` bodies for the five alphabets, as
  `FontSwitchRow` does (the check at `:1778` searches the source substring `\text`). A node flag marking text-origin
  leaves is a larger change to `MathNode` for little value. Rejecting `\sin` and other named functions under an alphabet
  is a separate decision; list it in the doc. Update the 8200-unit expansion test at `MathCorpus.h:196` to a non-text
  body (`\mathbb{AAAA...}`; adjacent ordinary letters merge into one leaf through `AppendRowAtom`, `:1818-1820`, so the
  bound is still exercised) and fix the doc sentence.
- **Regression test**: `Tests/Support/Support.Tests.MathCorpus.h`: `! ParseMath(L"\\mathsf{\\text{a}}")` (today it
  parses and the leaf text is U+1D5BA); the updated expansion test still passes.
- **Verifier notes**: asymmetry confirmed from the code and both tests.

### MATH-06 — A row `\tag` inside `\substack` in an `align` row is accepted and attached to the outer row

- **Severity** P3 (reviewer P3, verifier P3) · **Category** conformance, doc · **Verdict** Confirmed · **Confidence**
  high · **Effort** S · **Introduced** `0b6cfd31` (row tags) over `4ea6d134` (substack), both 2026-11-01
- **Location**: `src/Controls/DxUi.NativeMath.cpp:764-803` (`Substack`, rows parsed with `cell = true` at `:779`),
  `:1701` (`ParseTag`), `:505-511` (`Matrix` installs `RowTagScope`), `:1922-1926` (`Row` forwards `cell`)
- **What**: `Substack` installs no `RowTagScope`, and its rows are parsed with `cell = true`. `ParseTag` treats a tag as a
  row tag when `activeRowTags && cell && depth > activeRowTags->depth`; `activeRowTags` is still the root `align`
  context, the label `{7}` is a Text node, so the guard at `:1724` passes and the tag attaches to the outer row. Tags in
  ordinary script or fraction groups are correctly rejected (those rows pass `cell = false`), so only substack leaks.
  This contradicts `docs/rich-text.md:1635` ("nested row tags ... retain source").
- **Failure scenario**: `\begin{align} \sum_{\substack{i \tag{7}}} x \end{align}` renders `(7)` at the row's trailing
  edge instead of retaining source.
- **Fix**: at the top of `Substack` add `RowTagScope tagScope(*this, nullptr);`, the same pattern `Matrix` uses for
  nested environments. `ParseTag` then sees no active context, `row` is false, and `depth != 0` rejects.
- **Regression test**: `Tests/Support/Support.Tests.MathCorpus.h`:
  `! ParseMath(L"\\begin{align}\\sum_{\\substack{i\\tag{7}}}x\\end{align}")` (today it parses with a `rowtag` style).
- **Verifier notes**: confirmed.

### MATH-07 — 91 of 127 hand-written symbol rows are shadowed; the `\ldots` fix in `3ecbd258` is dead code

- **Severity** P3 (reviewer P3, verifier P3) · **Category** simplification, test gap · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S · **Introduced** generated table `ed233e0c` and the dead override `3ecbd258`, both
  2026-11-01
- **Location**: `src/Controls/DxUi.NativeMath.cpp:1086-1096` (generated table searched first), `:1097-1244` (hand table),
  `:1228-1231` (`ldots` override), `src/Controls/DxUi.NativeMathSymbols.h:192` (`ldots` is already `Inner`),
  `Tests/Support/Support.Tests.MathCorpus.h:195-210` (the added test)
- **What**: `SymbolCommand` returns on a generated-table match before the hand table is consulted. The hand table has 127
  rows; the generated table lacks exactly 36 of them (`sum prod coprod bigcup bigcap int iint iiint oint`, `ne neq`,
  `cdots vdots`, `notin`, `lbrace rbrace lvert rvert lVert rVert`, and the 16 names `sin cos tan cot sec csc sinh cosh
  log ln exp lim max min det gcd`), so 91 are unreachable. Two shadowed rows disagree with the live ones: `vert` (hand
  `|`, live U+2223) and `Vert` (hand U+2016, live U+2225). The `ldots` to Inner override never executes: `git show
  3ecbd258^:src/Controls/DxUi.NativeMathSymbols.h` already contains `{L"ldots", L'\x2026', MathAtomClass::Inner, false}`.
  The `3ecbd258` test (`a\ldots b` Inner, literal `a...b` Ordinary, width delta 8 DIP at 24 DIP) therefore passes without
  the source change, and its `Changes/` fragment describes a behaviour change that did not occur.
- **Failure scenario**: a maintainer edits a shadowed hand row (for example `\vert`) and nothing changes; reviewers
  believe a fix landed.
- **Fix**: delete the shadowed rows and the `ldots` branch; keep only the 36 live operator, function and delimiter-name
  rows. Hoist the table out of the function-local `constexpr` so a `static_assert` can prove it is disjoint from
  `NativeMathSymbols`. Optionally correct `Changes/2026-11-01-math-ldots-spacing.md`.
- **Regression test**: a `static_assert` or `check` loop that every hand key is absent from `NativeMathSymbols` (91
  collisions today).
- **Verifier notes**: counts recounted mechanically; the 36 live names match the reviewer exactly.

### MATH-08 — `\|`, `\Vert` and `\vert` atoms and delimiters use inconsistent code points

- **Severity** P3 (reviewer P3, verifier P3) · **Category** conformance, consistency · **Verdict** Confirmed ·
  **Confidence** high (inconsistency), about 70 percent (KaTeX maps `\|` to U+2225) · **Effort** S · **Introduced**
  `b5869cbc` (2026-11-01) for the `\|` atom; the delimiter table is older
- **Location**: `src/Controls/DxUi.NativeMath.cpp:640-643` and `:659-662` (`Delimiter`), `:1250-1251` (`\|` atom),
  `src/Controls/DxUi.NativeMathSymbols.h:50` and `:414`, `Tools/Generate-NativeMathSymbols.ps1:26-27`
- **What**: the `\|` atom is U+2016, the `\Vert` atom is U+2225 (generated from the pinned KaTeX table), the `\vert` atom
  is U+2223, while `Delimiter()` returns ASCII `|` for `\vert`, `\lvert` and `\rvert` and U+2016 for `\Vert` and `\|`.
  Docs describe `\|` and `\Vert` as equivalent double bars.
- **Failure scenario**: `\|v\| = \Vert v\Vert` shows two different glyph shapes in fonts where U+2016 and U+2225 differ.
- **Fix**: choose one code point per meaning for atoms and delimiters and align the generator override list (it already
  special-cases the four delimiter names) so `\|` and `\Vert` agree.
- **Regression test**: `Tests/Support/Support.Tests.MathCorpus.h`: `ParseMath(L"\\|")` and `ParseMath(L"\\Vert")` yield
  identical Text payloads (fails today).
- **Verifier notes**: the mismatch is proven from the code; the KaTeX code point is from memory, so verify against
  `symbols.js` before choosing the target.

### MATH-09 — `Finalize` does not reset `variants`; a retried `Advance` duplicates staged variant glyphs (latent)

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug (latent) · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `src/Controls/DxUi.PreparedMath.h:2558-2571` (`Finalize`), `:1750` (`variants.push_back`),
  `:2593-2599` (ink bounds loop over `variants`), `:2572-2573` (late `length_error`)
- **What**: `Finalize` clears `lines`, `paintOperations` and colour state but not `variants`. If it throws after `Place`
  (paint-capacity `length_error`, or an HRESULT from the glyph-metrics query), `finalized` stays false and the next
  `Advance()` re-enters `Finalize`, which appends a second copy of every variant, widens the ink bounds with stale
  entries and consumes the 512-per-formula assembly budget. `rowTags[i].baseline` is assigned in `Place` (`:2476`), not
  accumulated, so it needs no reset.
- **Failure scenario**: none today. `PreparedSemanticGeometry::Advance` rethrows and drops the preparation
  (`RichSemanticGeometry.cpp:2845-2848`), `NativeObjectCapacity` erases the placement, and reused objects come only from
  fully published geometry, so no owner retries the same object. The existing retry test
  (`Controls.Tests.RichSemanticDocument.cpp:695-717`) throws at `:2567-2568`, before any `Place`, so it cannot see
  the stale state.
- **Fix**: `variants.clear();` at the top of `Finalize`. Alternatively document "discard on throw" (Question 5).
- **Regression test**: `Tests/Controls/Controls.Tests.RichSemanticDocument.cpp`: prepare `\Big(` to the last leaf, set
  `testRichMathGlyphMetricsFailure = E_OUTOFMEMORY` so `Finalize` throws after variants were appended, catch, call
  `Advance()` again on the same object and assert `VariantLayouts().size()` equals a fresh preparation's (fails today).
  Guard on the font providing variants, and emit a skip count.
- **Verifier notes**: confirmed latent; the report's note about resetting row-tag baselines was unnecessary.

### MATH-10 — Matrix row height is measured and placed with different formulas

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug (pre-existing) · **Verdict** Plausible · **Confidence**
  medium (mechanism certain, visible size depends on font ascent) · **Effort** S
- **Location**: `src/Controls/DxUi.PreparedMath.h:1453-1465` (measure), `:2249-2270` (place)
- **What**: measurement uses `max(box.size, above + below)` per row; placement first raises `above` to
  `max(above, .8 * size)` and then uses `max(size, above + below)`. They differ when a row's tallest cell has
  `above < .8 * size` and `below > .2 * size`, by up to `.1 * size` per row. Fences and vertical rules use the measured
  extent, cells and the bottom `\hline` use the placed `y`, so later rows drift past the fence. Leaf extents come from
  DirectWrite line metrics (`PreparedMath.h:2772`); whether ordinary cells fall under `.8 * size` depends on the font, so
  the user-visible size is not statically provable. Script-only cells qualify clearly.
- **Failure scenario**: `\begin{pmatrix}{}_x\\{}_y\end{pmatrix}` (subscript-only cells) or `\texttt` cells with a short
  ascent: the last row overflows the measured box and the closing fence no longer encloses the cells.
- **Fix**: compute per-row heights once and use them in both `MeasureBoxes` and `PlaceMatrix`; the minimal fix is to
  apply the same `.8 * size` floor in the measurement.
- **Regression test**: `Tests/Support/Support.Tests.MathCorpus.h`: prepare the `{}_x` pmatrix above and assert the last
  cell glyph's bottom is within the formula box and the fence extent (fails today when the cell ascent is below
  `.8 * size`; use the subscript-only cells to make it deterministic).
- **Verifier notes**: divergence proven; impact depends on font metrics.

### MATH-11 — Fixed stack arrays grew 8x to 64-68 KiB; colored paint zero-fills 32 KiB and recomputes contrast per op

- **Severity** P3 (reviewer P3, verifier P3) · **Category** performance, resource · **Verdict** Confirmed ·
  **Confidence** high (sizes), medium (impact) · **Effort** M · **Introduced** `7c21b0d7` (2026-11-01, node cap 512 to
  4096)
- **Location**: `src/Controls/DxUi.PreparedMath.h:717` (`CountAndValidate`, `Pending[4096]` about 64 KiB, per preparation
  including each row-tag label), `:1303-1305` (`NormalizeRows` locals about 68 KiB), `:2844` (`DrawColored`
  `backgrounds`, 32 KiB value-initialised per paint), `:2885-2918` (6 `std::pow` per `contrast`)
- **What**: the base arrays were 512 entries at the same three sites. `DrawColored` runs in composition and is taken only
  for formulas with authored colors (`Draw`, `:2965-2978`). With up to 32,768 paint ops per formula, an adversarial
  colored formula costs hundreds of thousands of `pow` calls per frame. Not unbounded, no heap use, non-recursive, so
  stack risk is low (well under 1 MiB, ASan inflates it).
- **Failure scenario**: an adversarial colored formula inflates per-frame CPU; ordinary documents are unaffected.
- **Fix**: size scratch from `boxes.size()` into reusable members allocated at preparation; precompute authored-colour
  luminance at preparation and cache the theme-resolved contrast per `(context->text, context->fill)`; skip the
  zero-fill beyond `backgroundCount`.
- **Regression test**: add a paint-time CPU sample for a 4096-op colored formula to the performance fixture
  (`Tests/Embedded/Embedded.Tests.RichGeometryResources.h` already asserts no allocation in paint).
- **Verifier notes**: sizes and call frequency confirmed; low priority.

### MATH-12 — The parser deep-copies subtrees on every structural wrapper

- **Severity** P3 (reviewer P3, verifier P3) · **Category** performance · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `src/Controls/DxUi.NativeMath.cpp:128-131` (`Make`) and its 21 braced-list call sites (for example
  `:873`, `:915`, `:991`, `:1634`, `:1662`), `include/DxUi/RichMath.h:62-74`
- **What**: a braced list passed to `std::vector<MathNode>&&` goes through `std::initializer_list`, whose elements are
  `const`, so the vector copy-constructs every child subtree (`MathNode(const MathNode&) = default`). Also
  `MathNode(MathNode&&)` is not `noexcept`, so `vector<MathNode>` growth copies as well (`push_back` in `Row` and
  `AppendRowAtom`). Parsing therefore costs O(nodes x depth) node copies, bounded by 4096 nodes and depth 64, so an
  adversarial formula is about 10^5 to 10^6 small allocations per parse (milliseconds to tens of milliseconds); typical
  formulas are shallow. The 2026-10-24 disposition of `math-parser-06` ("move construction now defined") does not hold
  for these paths and has no copy-count test.
- **Failure scenario**: a 31-level `\sqrt{...}` nest around about 3,900 `a+` nodes copies the inner subtree 31 times.
- **Fix**: add a variadic `Make` (or a `MakeN(kind, text, MathNode&&...)` helper) that reserves and `push_back`s moved
  children, and convert the 21 sites. Do not mark the move operations `noexcept`: the delegated default constructor can
  allocate in MSVC Debug (the comment at `RichMath.h:64-65`).
- **Regression test**: `Tests/Embedded/Embedded.Tests.RichGeometryResources.h` allocation counter: parse the nest above
  and assert allocations at most `k * nodes`, with `k` taken from a measured pre/post pair.
- **Verifier notes**: deep copy and growth copy confirmed from the declarations.

### MATH-13 — Equation `\tag` admits arbitrary atoms whose colors and backgrounds are silently dropped

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug, conformance · **Verdict** Confirmed · **Confidence**
  high · **Effort** S · **Introduced** `18fa1698` (2026-11-01)
- **Location**: `src/Controls/DxUi.NativeMath.cpp:1719-1723` (`label = Atom(depth + 1u)` for an unbraced tag),
  `src/Controls/DxUi.RichSemanticGeometry.cpp:3446` (`image.tag->Draw(...)` with no palette), `:616-622` (palette staged
  from `math->Colors()` only)
- **What**: only the row-tag path requires a Text label (`:1724`). An unbraced equation tag accepts any atom, so
  `\tag\colorbox{red}{1}`, `\tag\textcolor{red}{1}` and `\tag\begin{matrix}a\end{matrix}` parse and prepare. The tag is
  drawn without a palette or color context, so `DrawColored` has `enabled == false` (`PreparedMath.h:2848`): text is
  drawn with the default brush and `Background` operations draw nothing (`:2936`).
- **Failure scenario**: `x\tag\colorbox{red}{1}` shows a plain `(1)` with no red fill.
- **Fix**: require a Text label for equation tags as for row tags (reject otherwise). Staging a second palette for the
  tag is more work for a pathological input.
- **Regression test**: `Tests/Support/Support.Tests.MathCorpus.h`: `! ParseMath(L"x\\tag\\colorbox{red}{1}")` (parses
  today).
- **Verifier notes**: confirmed.

### MATH-14 — Dead parser branches: `StructuredCommand`, most of `StyledCommand`, the `structured[]` and `styled[]` lists

- **Severity** P3 (reviewer P3, verifier P3) · **Category** simplification · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `145c9b5a` (2026-11-01, command-frame refactor)
- **Location**: `src/Controls/DxUi.NativeMath.cpp:949-957`, `:1038-1067`, `:1516-1529`
- **What**: every `structured[]` name is dispatched earlier in `Command` (`:1500-1515`), and `begin` is not in the list,
  so `StructuredCommand` is unreachable. All `styled[]` names except `operatorname` are caught earlier by
  `IsTextStyleCommand`, `MathStyleCommand`, `UnaryAccent` or `left`, so `StyledCommand`'s text branches are dead.
  Duplicate tables are where divergence bugs hide (compare MATH-07).
- **Failure scenario**: none; maintenance risk.
- **Fix**: dispatch `operatorname` directly, delete both tables and `StructuredCommand`.
- **Regression test**: none beyond the existing corpus; coverage should show zero hits for the removed branches.
- **Verifier notes**: every name traced to its earlier dispatch.

### MATH-15 — A comment claims MATH constants are read "without HRESULT propagation"; they do propagate

- **Severity** P3 (reviewer P3, verifier P3) · **Category** documentation · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `baff43d8` (2026-10-25, comment)
- **Location**: `src/Controls/DxUi.RichSemanticGeometry.cpp:342-346`, `src/Controls/DxUi.PreparedMath.h:220-226`
  (`OptionalMathFontOperation`), `:587-640` (`ReadMathConstants`), `:2686`
- **What**: `OptionalMathFontOperation` returns false only for `DWRITE_E_UNSUPPORTEDOPERATION` and otherwise calls
  `THROW_IF_FAILED`. `ReadMathConstants` runs inside the `PreparedMath` constructor, inside the `NativeImageGeometry`
  constructor, whose catch handles only `length_error`. An `E_OUTOFMEMORY` or device HRESULT therefore escapes as
  `wil::ResultException` to `RichTextEditor.cpp:4089-4100` and stays retryable, which matches `docs/rich-text.md:1637`.
  The comment is wrong.
- **Failure scenario**: a maintainer relies on the comment and adds a classification at the wrong site.
- **Fix**: correct the comment to say that only the unsupported-operation HRESULT selects fallback constants and all
  others propagate. Optionally move `ReadMathConstants` into the cooperative `PrepareMath` step.
- **Regression test**: none for the comment. Optional: a failure seam on `FindFamilyName` asserting that `Prepare`
  surfaces the HRESULT.
- **Verifier notes**: confirmed.

### MATH-16 — Vector-fallback geometry keeps fixed-DIP offsets that do not scale with size or zoom

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug (pre-existing) · **Verdict** Confirmed · **Confidence**
  high · **Effort** S
- **Location**: `src/Controls/DxUi.PreparedMath.h:2143-2144` (`\vec` head 4 x 3 DIP), `:1906` (`‖` offset 3 DIP),
  `:1880-1881` (brace `middle - 2`), `:2294` (fraction rule inset 2 DIP), `:2306` (root top `+ 2`), `:2020` (`underline`
  2 DIP)
- **What**: these constants are absolute DIPs. Editor zoom scales font size (and `pt` units since `7f7f88b7`), but they
  stay fixed, so fallback geometry looks thin and cramped at high zoom and overlaps at script sizes. The `\vec` and
  brace constants exist in `a0f67fd3`; none is new.
- **Failure scenario**: `\vec{v}` with the `missingTable` override at 96 DIP has a 4 x 3 DIP arrow head against a
  96 DIP font.
- **Fix**: express each constant as a fraction of `box.size`, with a minimum clamp for tiny script sizes.
- **Regression test**: `Tests/Support/Support.Tests.MathCorpus.h`: prepare `\vec{v}` at 24 and 96 DIP with the
  `missingTable` override and assert the arrow-head stroke length scales by 4.
- **Verifier notes**: constants confirmed; cosmetic.

## Refuted

None. All 16 findings survived verification; MATH-10 stays Plausible because its visible size depends on font ascent.

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| none | | |

## Area architecture and simplification proposals

1. **Shared per-element preparation-failure helper.** Convert `std::invalid_argument` raised while preparing one native
   element into a per-element reject at the `NativeImageGeometry` constructor, `PrepareMath` and the diagram `Begin` and
   `Advance` calls (details in MATH-02); not widened to every `logic_error`. Rationale: one channel serves MATH-02,
   FLOW-01 (`diagrams.md`) and ARCH-03 (`architecture.md`). Files: `src/Controls/DxUi.RichSemanticGeometry.cpp`.
   Risk: low. Effort: S to M. Before production: yes.
2. **Split `DxUi.PreparedMath.h` into a header and a `.cpp`.** About 3,000 lines of implementation, WRL, `dwrite_3.h` and
   WIL are compiled into `DxUi.RichSemanticGeometry.cpp` and five test translation units
   (`Controls.Tests.RichSemanticDocument.cpp`, `Embedded.Tests.RichGeometryResources.h`, `Support.Tests.MathCorpus.h`,
   `Support.Tests.RichSemanticEditor.h`, `Support.Tests.RichSemanticGeometry.h`). ODR is safe only because
   `DXUI_ENABLE_DIAGNOSTICS` is fixed to 1 (`include/DxUi/Configuration.h:4`, "not a consumer-selectable build option").
   A narrow `PreparedMath` interface plus a test-access header removes the risk and shortens compiles. Risk: low.
   Effort: M. Before production: no.
3. **Retain glyph runs instead of one `IDWriteTextLayout` per leaf** (`PreparedMath.h:2773`). Store face, indices,
   advances and offsets (the probe renderer already extracts them) and paint with `DrawGlyphRun`. Cuts memory for
   documents with many formulas. Risk: medium. Effort: M. Before production: no (measure first).
4. **Cache prepared math across document generations.** Reuse requires the identical document pointer and options
   (`RichSemanticGeometry.cpp:2178-2186`), so every edit re-prepares every formula and re-reads the MATH table once per
   generation. Key a cache by (source, size, display, DPI, family, locale, unit scale) and keep `MathFontData` per
   (factory, family). Risk: medium (invalidation). Effort: M. Before production: no.
5. **Use the font's top-accent attachment.** Accent skew uses half the italic correction (`PreparedMath.h:2000-2019`);
   OpenType `MathTopAccentAttachment` is unparsed in `DxUi.MathFontTable.h`. Display operators are enlarged by scaling
   the font (`:940-941`) instead of selecting the MATH display variant. Both are documented approximations. Risk:
   medium. Effort: M. Before production: no.
6. **One symbol source.** Fold the 36 live hand rows (operators, functions, `cdots`, `vdots`, `ne`, `neq`, `notin`,
   brace and bar names) into the generator input as explicit overrides so `SymbolCommand` has a single lookup (MATH-07,
   MATH-08). Files: `Tools/Generate-NativeMathSymbols.ps1`, `DxUi.NativeMath.cpp`. Risk: low. Effort: S to M. Before
   production: the MATH-07 cleanup yes, the generator fold optional.
7. **Assembly overlap policy.** `VerticalAssemblyAt` rejects a pair whose connector overlap is below the font's minimum
   (`DxUi.MathFontTable.h:332-337`, variable `overlap`); the reviewer says MathML Core clamps instead (unverified from
   the repository). Harmless today (vector fallback) but loses assemblies on some fonts. Risk: low. Effort: S. Before
   production: no.

## Test-quality observations

- `Tests/Support/Support.Tests.MathFontTable.h:41-43, 100-105, 147-149, 186-191, 232-234`: truncation tests pass
  sub-spans of a larger `std::array`, so an over-read past `length` stays inside the array, ASan cannot flag it, and the
  oracle only checks the return value. Copy each truncated prefix into an exact-size heap buffer.
- No randomized or fuzz input exists for the MATH reader or for `ParseMath`. A small seeded-random or libFuzzer harness
  asserting no crash, and `ParseMath` success implies `PreparedMath` success or `length_error`, is cheap.
- `PinnedSupportRows` (`Support.Tests.MathCorpus.h:26-54`, 1135 KaTeX rows) only checks that geometry is finite; it
  cannot detect any rendering defect (the generator says so). It is a smoke test, not support evidence, and
  capabilities and docs should not lean on it.
- The `\ldots` test added in `3ecbd258` (`Support.Tests.MathCorpus.h:195-210`) passes with or without the commit's code
  (MATH-07).
- `Tests/Embedded/Embedded.Tests.RichGeometryResources.h:96`: `retained->Size() == original` is vacuous because
  `retained` is an independent immutable object (`prepare()` builds a new one each time). Retry is only ever tested on
  fresh objects (also `Controls.Tests.RichSemanticDocument.cpp:343-457`), which is why MATH-09 is invisible.
- Font-dependent tests silently skip when Cambria Math lacks variants (`if (! isolatedFence->VariantLayouts().empty())`,
  `Controls.Tests.RichSemanticDocument.cpp:286, 340, 424`); on a Server Core or CI image they report success while
  testing nothing. Emit a skip count.
- `Support.Tests.MathCorpus.h:196` pins `\mathbb{\text{...}}` as admitted (MATH-05) although its purpose is the expansion
  bound; `:1142` pins the literal-`$` policy (MATH-03).
- Missing conformance and pathological tests: display style in `aligned` (MATH-01), extreme font sizes (MATH-02),
  `\bigl|` spacing (MATH-04), a tag in substack (MATH-06), parse copy cost at 4096 nodes and depth 64 (MATH-12). The
  128 KiB stack-headroom check prepares 64 nested arrays (about 129 AST levels), so `Place` recursion at the AST maximum
  is covered; the 64-68 KiB `CountAndValidate` and `NormalizeRows` frames are not observed by that probe.

## Questions for the developer

1. Is the text-style behaviour of `aligned`, `gathered` and `equation` cells intentional for the DxUiMath1 profile?
   Recommended answer: no; follow KaTeX (display style, independent of the enclosing style) and document it (MATH-01).
2. Is the literal-`$` policy in `\text{}` a product decision? Recommended answer: reject `$` in `\text` bodies until
   nested math is implemented, matching the all-or-nothing contract and the tag path; the verifier notes that this turns a
   readable literal into a source fallback, so the owner should choose knowingly (MATH-03).
3. Should the maximum editor heading size (512 x 7, then x 1.15 for math) be admissible for math at all, or should
   `RichTextEditorAppearance` reject combinations above the 4096 DIP math cap? Recommended answer: keep the validator and
   make math reject per formula (MATH-02); the diagram path already does for `fontSizeDip > 512`. The premise is true.
4. Ordinary-letter runs are merged into one DirectWrite leaf (`NativeMath.cpp:1818-1820`), so default OpenType features
   (`liga`, `kern`) apply across distinct variables (for example `fi`). Has this been checked against the configured math
   families? Recommended answer: check Cambria Math and any configured family; if one ligates, disable `liga` for math
   leaves or stop merging. The premise (the merge) is true; whether it ligates is open.
5. Is `Advance()` after an exception a supported retry on the same object (MATH-09)? Recommended answer: document
   "discard on throw" and add `variants.clear()` anyway as a one-line guard.

## Coverage

| File | Depth |
| --- | --- |
| `src/Controls/DxUi.PreparedMath.h` | full (all 2982 lines; base copy diffed for pre-existing classification) |
| `src/Controls/DxUi.NativeMath.cpp` | full (all 1989 lines) |
| `src/Controls/DxUi.MathFontTable.h` | full (every offset and count read checked against the OpenType MATH layout) |
| `src/Controls/DxUi.NativeMathDimension.h` | full |
| `include/DxUi/RichMath.h` | full plus diff |
| `src/Controls/DxUi.NativeMathSymbols.h` | generated; verified count (411), ordinal sort order for `lower_bound`, and shadowing against the hand table; rows spot-read |
| `Tools/Generate-NativeMathSymbols.ps1`, `Tools/Generate-NativeMathTestData.ps1` | full |
| `src/Controls/DxUi.RichSemanticGeometry.cpp` | diff plus touched functions (`NativeImagePlacement`, `NativeImageGeometry` constructor, `PrepareMath`, reuse at 2176-2197, math paint 3426-3465, NativeImages phase 2452-2489); the rest is owned by another reviewer |
| `src/Controls/DxUi.RichSemanticDocument.cpp` | touched functions only (math admission 1205-1249, 2011-2058) |
| `src/Controls/DxUi.RichTextEditor.cpp` | exception channels only (4045-4130) and options derivation (3925-3980), for MATH-02 |
| `src/Controls/DxUi.NativeColorPalette.h` | full (used by math paint) |
| `Tests/Support/Support.Tests.MathFontTable.h` | full |
| `Tests/Support/Support.Tests.MathCorpus.h` | partial (about 35 percent: harness, scenario and shape tables, stack and depth section 960-1660, targeted greps) |
| `Tests/Support/Support.Tests.NativeMathSupportRows.h` | generated data; header, tail and consuming harness read |
| `Tests/Controls/Controls.Tests.RichSemanticDocument.cpp` | math section 140-490 read |
| `Tests/Embedded/Embedded.Tests.RichGeometryResources.h` | math allocation test read (40-100) |
| `Tests/Support/Support.Tests.RichSemanticGeometry.h`, `Support.Tests.RichSemanticEditor.h` | grep level only (math reuse, palette and retry cases located, not audited) |
| `src/Controls/DxUi.NativeColor.h` (`ParseNativeColor`) | skipped: outside the math scope; assumed correct by callers |

### Checked and holds

The reviewer and verifier looked for each of these and found them sound:

- **Font-table bounds**: `DxUi.MathFontTable.h` bounds every offset, count and span read against the table length; the
  4 MiB snapshot cap (`PreparedMath.h:636`), the `size < 26` and version checks (`:629`), the MathConstants offset check
  (`:631-633`, `:647-649`) and the assembly part limits (32 distinct parts, 64 total) hold.
- **Nesting, AST and node caps**: parser depth at most 64 and AST depth at most 130 share one budget; node count is
  capped at 4096 (`MathMaximumNodes`) in `Make` and again in `CountAndValidate`; per-depth matrix scratch is sized
  65 x 65 and indexed only by validated depth; matrix cells (1024), rows and columns (64) and source units (16384) are
  capped; the 128 KiB stack-headroom test prepares 64 nested arrays.
- **Dimension caps**: explicit dimensions are limited to at most 64 pt or 4 em/ex, with no sign and no exponent
  (`DxUi.NativeMathDimension.h`).
- **Geometry guards**: `Finalize` rejects non-finite results and extents above 65536 before publication
  (`PreparedMath.h:2567-2568`, `:2609-2610`), and paint operations are capped at 32768 (`:2572-2573`).
- **No heap work in paint**: `Draw` and `DrawColored` allocate nothing (an Embedded allocation test asserts this);
  scratch is stack-only.
- **`noexcept` correctness**: no `noexcept` path can throw; `NormalizeRows`, `ApplyMathConstants`, `CopyMathConstants`
  and `DrawColored` do only bounded arithmetic.
- **All-or-nothing parser**: a failed parse yields no partial tree (`RichMath.h:155`); parse failure and per-formula
  `length_error` are handled as local source fallback (`RichSemanticGeometry.cpp:392`, `:642-647`).
