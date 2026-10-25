# Input and accessibility

Lossless Markdown analysis is preparation metadata. Enabling or changing its
grammar target must preserve the existing RichTextEditor source ACP contract,
Unicode source transactions, history, composition and source UIA text. Parser
queries and UIA/TSF geometry queries never advance parsing. While source analysis
or geometry is pending, current glyph bounds remain unavailable; a previous
acknowledged embedded text snapshot must never acquire unrelated current glyph
bounds. The opt-in semantic accessibility contract below adds source-backed
structure/style after coherent preparation. The raw parser accepting NUL does not change
the editor/text-store admission contract.
Opt-in native HTML and hanging marker presentation preserve that same source
contract. Markdown/HTML links retain inert destination/title metadata;
clicking places a caret and double-clicking selects source text. Neither action
navigates or creates a text/history transaction. Validated tag/attribute ranges
have no source glyph rectangles but remain searchable/selectable raw TextPattern
text. HTML references and br tags are atomic visual source tokens; TSF imports
still use exact raw ACPs. An empty balanced HTML leaf has a real insertion between
its opening/closing tags, with no generated text or virtual ACP in the model.
Unicode preview/cancel/commit, one undo/redo, hide/reflow and stale-result rejection
apply identically in native and embedded hosts. Equal-ACP affinity remains valid
for padded cells; ordinary marker boundaries must not add invisible arrow stops
or move a logical Left forward/Right backward. Structured link/HTML children,
style attributes and task actions follow the semantic accessibility contract below.
Document/node identities are syntax handles, not UTF-16 ACPs or semantic editing
anchors. The retained editor supplies SourceSnapshot ancestry when preparing a
new block tree, including edits among equal paragraphs. UIA/TSF continues to expose
exact source; block analysis never silently substitutes native rich-text ranges.

Rich editor link decoration is presentation state. UIA `UnderlineStyle` MUST
report the currently rendered underline for every source-format run, including
hover/caret opt-in, explicit `<u>` style and high-contrast override. Changing
decoration MUST retain Hyperlink children, names, destinations and inert action
semantics. UIA font families/sizes MUST reflect explicit appearance overrides
and heading scale. Source mode continues to expose exact raw text without
inventing semantic styles. Queries read captured owner-published state and never
perform layout, take focus or retain a consumer callback.

`SetOnSelectionChanged` publishes owner-thread source-selection state from the
same canonical path as keyboard, native text/TSF, UIA and programmatic selection.
It changes neither source nor history. Unchanged source revision, endpoints and
gesture state are silent; callbacks may replace themselves, change selection
reentrantly or retire the editor. Notification state MUST publish before invocation so those changes cannot
replay an older event. Standard callback failures retain committed selection;
allocation failure is fatal at the native callback boundary.
For a notifying source transaction with both `SetOnTextChanged` and
`SetOnSelectionChanged` registered, the editor first synchronizes native text
input and internal selection state, invokes the text callback for the committed
source, then publishes the final selection if the original editor, host binding
and source revision remain current. The text callback may clear or replace
itself without suppressing that transaction's selection notification. A nested
source edit, rebind, detach or retirement supersedes the outer selection event;
the outer transaction MUST NOT publish stale selection state. This ordering lets
the consumer derive document facts from committed source before receiving the
selection notification. Selection-only changes and non-notifying TSF preview,
cancellation and selection imports retain their immediate selection callback. A
non-notifying text-service preview or cancellation that changes the source
revision publishes selection state even when its anchor, caret and gesture state
are unchanged; reimporting the same source revision and selection is silent. Such
transient notifications never become committed text or history events;
`SetText` remains silent for the text callback.
`IsSelectionGestureActive` identifies pointer selection/press updates. Consumers
may hide transient selection UI and return immediately while it is true.
Matching pointer release or capture cancellation MUST publish a final inactive
gesture notification even when the source endpoints did not change. No polling
timer or consumer subclass of pointer/key handlers is required.

RichTextEditor object source editing shares the authoritative full-source ACP
store. F2/UIA Invoke opens an acknowledged code/HTML/metadata leaf in place;
Esc/F2/Ctrl+Enter closes it while preserving committed history and redo. Raw
Unicode input and native/embedded previews/cancellation rebase one opaque
physical-line envelope. Entry/exit MUST refuse active composition, stale or
unprepared targets and disabled/hidden owners. Enabled read-only owners may
inspect source. UIA actions MUST retain owner-thread dispatch, embedded foreign-
thread refusal, binding/publication/epoch and lifetime guards, and MUST NOT
acquire OS focus. Presentation transitions retire old fragments immediately
even without changing source revision. Old providers cannot reappear after
reflow, detachment or restoration. Synthetic protocol coverage does not replace
the separate physical IME/assistive-technology gates.
Source-bearing leaves without a diagnostic expose source-entry help through
HelpText. An existing diagnostic explanation takes priority; finishing an
incomplete metadata fence removes that explanation and restores action help.

Status: normative intended contract
Last reviewed: 2026-10-25

RichTextEditor v0 is a bounded retained preview using ControlHost's shared native
TSF/IMM and application-bridged embedded text services. SupportsTextInput is true;
ReadTextInput publishes exact multiline Unicode Markdown source for its current
focusId/revision. Source ACPs include hidden syntax and original CRLF. Geometry
uses the same prepared source/display mapping, wrapping, scroll, flow and DPI as
painting. Hidden delimiters yield an empty rectangle list, never the viewport.
Composition previews and cancellation preserve both histories; hosts restore the
entry state before one notifying committed transaction. Rejected text/ranges are
atomic. New application edits supersede old previews. Lifetime/focus/rebinding
guards stop stale writes and outgoing notifications after editor retirement.

`RichTextEditor::ApplySourceEdits` commits a snapshot/revision-guarded source plan
through the same editable, invalidation, composition synchronization and callback
path as ReplaceSelection. All replacements publish before one source notification;
source ACPs, backward selection and UIA TextPattern2 caret retain their exact
meaning. Read-only/disabled plans do nothing. Rejected plans preserve both histories
and selection. Unchanged spelling may update selection silently without rebuilding
syntax/geometry. A callback may undo the whole plan, replace source, throw a standard
exception or destroy its editor; no subsequent stale access/notification is allowed.
Native and application-bridged embedded client tests must observe complete source,
selection/caret and TextChanged after owner publication, plus one undo for the plan.

Native and embedded UIA expose the same Document, Text, Text2 and TextEdit
semantics. Source text, selection, active composition/conversion targets and
projected rectangles remain coherent with the published snapshot. Text2 uses the
actual caret end of backward selection. Visible ranges include complete wrapped
lines intersecting the viewport. Rich ranges retain bounded source offsets in
the latest published text, clamped to scalar/CRLF boundaries after replacement;
they do not track shifted text identity. Reacquire DocumentRange for the full
grown source. Endpoint-to-range movement requires a common live owner and
collapses crossing endpoints; bounded GetText preserves valid UTF-16.
Read-only permits selection; disabled actions
fail. IsReadOnly is a text attribute; unsupported attributes return UIA's reserved
unsupported value. Multiline source has no duplicate ValuePattern. Retained
providers disconnect on owner replacement. Rendered-only TextPattern text and
annotations remain future scope; semantic structure/style is specified below. Synthetic OS protocol,
real in-process UIA client and WARP tests do not close real IME/Narrator/NVDA,
physical/focus-taking or native ARM64 qualification. See the
[guide](../../docs/rich-text.md) and [plan](../Plans/WIP/RichTextEditor_2026-10-17.md).

Rich caret/visible/composition/conversion ranges retain bounded metadata rather
than another full source string. The warm caret plus visible-range C++ allocation
budget is at most 4 KiB at both 32 and 65,536 source units; prepared snapshot/layout
costs and Windows/driver allocations are measured separately.

Rich source ranges implement ScrollIntoView with top/bottom painted-edge alignment
clamped to document extent, including caret and oversized ranges. Hidden-only
markers do not scroll to an offscreen anchor. Scrolling preserves selection,
source/history and focus and uses existing host invalidation/preparation. Native
foreign-thread mutations use bounded owner dispatch; embedded mutations require
the owner thread. Both disconnect after hide/root replacement and reject disabled
controls; read-only selection/scroll remains available.
Text controls without these range actions return UIA_E_NOTSUPPORTED rather than
success without an action.

Single-selection AddToSelection unions touching/overlapping spans, or selects
from an insertion point. A degenerate add moves the insertion point. Removal
trims an edge, collapses a fully removed selection at its active caret, and ignores
degenerate/nonoverlapping spans. Existing backward direction survives. Disjoint
union or interior subtraction returns UIA_E_INVALIDOPERATION without mutation.
These operations publish TextSelectionChanged, never source/history changes.

FindText searches bounded exact source, including hidden syntax, with ordinal
Unicode comparison and forward/backward/case options. Matches respect scalar/CRLF
boundaries. Empty/malformed needles and invalid unit/endpoint/length arguments
return E_INVALIDARG. Outside acknowledged semantic rendering, FindAttribute supports the uniform IsReadOnly VT_BOOL value;
unknown attributes return UIA_E_NOTSUPPORTED. No match returns S_OK with null.
Line commands use prepared wrapped source ranges rather than logical-line guesses.
Outside acknowledged semantic rendering, unsupported Format promotes to Word;
Paragraph/Page promote to Document in both modes. Character
commands use the shared Unicode 17.0.0 default extended grapheme rules, including
Hangul, Indic conjuncts, pictographic ZWJ context and regional-indicator parity.
Keyboard deletion/movement and rich mouse hit stops use the same logical
boundaries. Explicit source selections and text-service ACP geometry preserve
valid scalar/CRLF offsets inside a grapheme; this is separate from character
navigation. Full visual bidi/word navigation and physical assistive-technology
acceptance remain separate gates. Source-only presentation has no semantic children.

## Semantic rich-text accessibility

Explicit semantic Rich presentation MUST publish structure/style only with its
current source, native geometry, host binding, typography and device generation.
UIA/TSF queries never drain preparation. Pending/failing or source-mode
presentation keeps the source text interface and publishes no rich metadata.
The immutable `SemanticDocument::SourceRuns()` partitions exact raw source:
hidden syntax is Regular, decoded values keep their whole source spelling and
generated/repeated output creates no duplicate source interval. This index is
prepared once with semantic publication, including failure-atomic staging.

Text ranges support FontWeight (VT_I4), IsItalic (VT_BOOL), UnderlineStyle and
StrikethroughStyle (VT_I4), StyleId (VT_I4), FontName (VT_BSTR), FontSize (VT_R8
points), and IsReadOnly (VT_BOOL). Heading sizes/weights follow native text
styles; font names are the requested body/monospace faces, including nominal
source style for object-owned tokens rather than a generated glyph fallback.
Nonuniform attributes return UIA's reserved mixed value; unknown attributes
return its reserved unsupported value. FindAttribute requires the exact type,
searches forward/backward for the maximal matching source interval and returns
null for no match. Format expansion/movement uses the complete prepared source
partition, including hidden delimiters; counts are bounded even for INT_MIN/MAX.
Other source text/range/selection/scroll semantics remain unchanged.

Semantic elements form the actual native/application-bridged fragment tree:
headings expose Text plus HeadingLevel; quotes use Group; lists/ListItems retain
their containers; links use inert Hyperlink with destination/title help; images
use Image with a Unicode alternative; tasks use CheckBox plus Toggle. Tables
expose Table and Grid, row DataItems, and cell GridItem/TableItem providers with
column headers, dimensions and row/column coordinates, including padded cells.
Links advertise no Invoke/navigation action. Child bounds use prepared native
rectangles clipped to the text/ancestor viewport; offscreen children remain
navigable with empty bounds. Hit testing returns the innermost semantic fragment.
Keyboard focus remains on the document, with no OS focus acquisition by queries.

RangeFromChild accepts only a current descendant of the same editor and returns
its exact source interval. GetEnclosingElement returns the innermost source
owner; DocumentRange itself encloses the editor. GetChildren returns only the
intersecting immediate children, ordered by source start. Explicit child ranges
and clones retain distinct cell identity when padded cells share an empty ACP.
They add no virtual source characters. Child creation rechecks publication after
outgoing/reentrant boundaries. Child providers hold weak document identity and
the owner binding epoch, so edits, source/profile/rendering changes, root
replacement and detach/rebind cannot alias a successor. Geometry-only reflow may
retain the same logical publication and refresh its bounds. Source-identity
ranges retain the existing bounded latest-source behavior.

Toggle edits only the raw task-state character through one source transaction,
preserves directional selection, and produces one undo and committed source
notification. Live effective-disabled state rejects with ELEMENTNOTENABLED;
read-only/active composition rejects with INVALIDOPERATION. Pending or retired
semantic owners cannot commit. Native foreign-thread actions use existing
bounded owner dispatch; embedded mutations use COM owner-apartment routing and
reject direct foreign-thread execution. Allocation failures return HRESULTs and
preserve state. A committed callback may destroy/rebind the owner; no subsequent
control dereference is permitted. Semantic epoch/binding changes invalidate the
reachable structure so clients can reacquire current parts. These protocols do
not establish physical IME, Narrator/NVDA or native ARM64 acceptance.

Collapsed Backspace/Delete at an interior ACP remove the complete containing
grapheme. An exact boundary still removes its preceding/following neighbor.
TextField (single/multiline), editable ComboBox and RichTextEditor share this
policy, including a legacy ACP between surrogate halves. Undo retains the entry
source and ACP. Explicit noncollapsed source intervals keep their original
meaning; rich ranges still require valid scalar/CRLF endpoints. Text-service
geometry and explicit selection setters do not silently snap to navigation stops.

Bulk Character movement snaps once using the existing nearest/forward-tie
policy, then walks established boundaries. Negative movement counts the bounded
prefix and visits its target ordinal. INT_MIN/INT_MAX requests return actual
signed movement without negation overflow or repeated prefix scans. Zero-count
pure helper calls preserve the clamped input offset. The command adds no index,
cache, retained text or C++ allocation; prepared layout supplies its existing
boundary index for collapsed deletion.
The [source-command qualification](../../Measurements/RichText/README.md)
records functional tests separately from the open performance and real AT gates.

Shared text-element segmentation uses a pure allocation-free forward walker,
with pinned generated property data and all original Unicode 17 GraphemeBreakTest
vectors, without exclusions. Arbitrary-caret wrappers recover context from the
beginning; callers already walking established boundaries use the forward service
instead of rescanning each prefix. Password masking and source/display mapping
follow these same elements. Legacy malformed UTF-16 still makes bounded progress;
the rich model rejects malformed source. See the [data provenance and generator](../../ThirdParty/Unicode/README.md).

UTF-16 character halves are
buffered outside the document and retired on selection, source, focus, host,
visibility, disabled and read-only transitions. AltGr text remains accepted.

Native focus transfer can synchronously re-enter activation through `WM_SETFOCUS`. A failed or superseded outer
request may retire only its own session generation, including when the newer session uses the same retained editor.
Each accepted explicit logical focus request supersedes an older request. A rejected request for an unrelated
ineligible target MUST NOT advance the focus-request revision or disturb healthy current focus; a rejected request
for the current owner MUST still prune stale retained focus and native text-input state before returning.
After application or TSF callbacks, an older
setter stops before assigning, clearing or publishing focus belonging to the newer request. Standard exceptions
from focus callbacks are contained at the host boundary. A live control whose override fails before acknowledging
the transition receives the base focus-state acknowledgement, unless a newer request has superseded that transition.
A failed old blur still clears the old control's acknowledged focus when a different control wins. Reset snapshots
and clears interaction observers before invoking callbacks, preserves explicit callback-selected focus, and suppresses
automatic focus restoration during the reset. Root replacement retires any focus chosen in the old tree while that
tree remains owned; callback-selected focus in the installed new tree survives. A root replacement cannot borrow a
rootless focus target from the tree it is retiring.
Retiring a native text-input cache never transfers keyboard focus. Reset and root replacement on an inactive
host preserve the other window's keyboard focus; text-service retirement has no focus-restoration option.
If a host focus notification removes its newly focused editor, the setter retires its native session, TSF document
and cached text before returning, even if the notification throws. A returned live child acknowledges lost focus.
Restoring a retiring TSF document's HWND association preserves a different document that already owns TSF focus.
After the association callback, a newer host transition or a different callback-selected document takes precedence.

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

Keyboard traversal and UI Automation use the same indexed logical-child tree, including PageHost's current page.
Outgoing transition pages are visual-only. Hidden/disabled ancestors exclude their focus subtree; UIA paths,
navigation, point hits, label lookup and action resolution retain the same logical indices, including empty slots.
Retained providers disconnect when their page/control identity leaves that tree rather than binding to a successor.
Native key-down and system-key-down messages remain unhandled when neither a control nor a host action consumes
them, so the application's window procedure and DefWindowProc retain their normal fallback behavior.

Native retained-host pumps MUST call `DispatchKeyboardMessage` after application
accelerators/TSF pretranslation and before ordinary Windows translation. For an
exact attached-HWND Return/Space key-down, it dispatches the real window procedure
once before deciding whether to produce a character. A handled command MUST NOT
produce a character for a successor editor. Unhandled text-input keys and a
handler's explicit `AllowTranslatedCharacter` permission may translate only while
the original weak owner, root and logical-focus epoch remain current. Idempotent
focus refreshes preserve ownership; focus round trips, root retirement, detach
and failed callbacks revoke it. Nested dispatch MUST NOT leak permission into an
outer route. Multiline TextField/RichTextEditor Return reserves exactly one later
character; preview/submission/default/button commands do not. Printable editor-key
suppression MUST preserve the guarded same-owner Space character permission. Unmodified Return
and Space during active native IME composition remain IME-owned: the command pump
returns false without dispatch or character translation, and retained key-down/
key-up handlers and host default/cancel actions MUST NOT receive those keys.
This applies equally to TextField and RichTextEditor. After composition ends,
ordinary Space translates and inserts exactly once into its original editor;
Return resumes its ordinary single-line/default or multiline text ownership.
Modified-key routing retains its existing policy. Foreign/child HWND messages,
system keys and IME-owned keys retain the existing path. Standalone
committed character input is unchanged. There is no queue scan, text removal,
timer, sticky suppression flag or character reinterpretation in this boundary.
`HandleMessage` without the pretranslation hook cannot establish this guarantee.

Form editors expose `LabeledBy`, `IsRequiredForForm`, `IsDataValidForForm`, textual error `ItemStatus`, description
`HelpText` and the stable field identity as `AutomationId`. A `NumericStepper` attaches these semantics to its
inner editable text provider. The visual label remains a real element with its own full name and mnemonic target;
form layout never inserts a duplicate hidden announcement. A label relationship borrows a weak lifetime identity
and resolves only to the same live, visible element in the current host snapshot. Hidden, detached, expired or
same-path replacement labels leave `LabeledBy` empty until a valid relationship is explicitly established.
Snapshot publication raises property changes for effective relationships and metadata, including removal through
visibility or ownership changes; unchanged republishing raises none. Native and embedded providers read immutable
published relationships, never a mutable raw label pointer from a client thread. Retained providers disconnect
when their element retires, and publication stops after a reentrant property notification retires its owner.
Property and text notifications recheck the semantic editor lifetime between outgoing calls; embedded publication
also checks its attached root lifetime and preparation's interaction revision. A detached retained editor or hidden
field stops the old event batch too, even while a snapshot and provider keep their own storage alive. Read-only
embedded properties still use the last published snapshot until the application's next coherent publication;
actions already return `UIA_E_ELEMENTNOTAVAILABLE` for an unavailable editor and never change its retained draft.

Opt-in layout prepares matching visible, hit and provider bounds before publishing. Reflow preserves editor
identity, focus, draft, selection and composition; it does not take HWND focus. A captured press whose control
geometry moved is cancelled before a later button-up can commit through the old rectangle. An unchanged layout
retains capture. Removal, hiding or disabling uses the existing interaction retirement rules.

Localized adaptive layout must preserve the focused control's stable identity through wrapping,
disclosure and DPI changes, and publish matching visible/hit/UIA rectangles. If a focused child is
collapsed, return focus to the disclosure header; if an application removes an action, the caller
chooses its safe focus successor. Layout never chooses a destructive default. ExpandCollapse,
Toggle, full text and selection semantics must match the confirmed displayed state. Color, focus
chrome and checked state are distinct; status meaning also has text and an icon. Qualify pointer,
Tab/Shift+Tab, Space, Enter/Escape dispatch and UIA bounds in both hosting modes under the
[localized adaptive layout plan](../Plans/Done/LocalizedAdaptiveLayout_2026-09-19.md).

Qualification exercises complete transitions: initial state, input, callback effect, acknowledged
state, removal/failure, focus recovery and teardown. A screenshot of hover or a successful Invoke
return does not prove the requested state change. Semantic headings/labels follow visual reading
order; full-value access must not require duplicate hidden announcements. Repeated unchanged status
does not produce repeated notifications. Application navigation supplies its own stable target and
stale-completion policy; the library does not choose the destination or steal focus on completion.

A persistent full-value inspection is a non-modal overlay and does not move logical or native focus.
An outside pointer press dismisses it while consuming that press's complete gesture through its matching
button-up or cancellation, so the same gesture cannot activate the control underneath. Native hosts
retain HWND capture only for that dismissal gesture; embedded hosts consume its delivered move, up, or
cancel events. A later independent gesture returns to normal control dispatch.

Every entry of a native menu is a UI Automation element, whether or not any entry has a description.
Its concise `Name` is `accessibleName` or the decoded label; `secondaryText` is exposed as `HelpText`.
Entries retain MenuItem roles, exact command invocation and acknowledged checked state. Menu
accessibility records are prepared when a provider is requested or a client is listening. State and
bounds changes mark the popup dirty and coalesce into a posted publication; an owner-thread provider
query first flushes that dirty snapshot. Worker-thread queries read the last complete immutable
snapshot. A real focus transition is published synchronously on the owner thread so the first
worker-thread `GetFocus` sees the focused element and focus-event attribution uses the publication
time; ordinary menu state and scrolling remain coalesced. This avoids rebuilding all popup records
for each intermediate input while retaining complete per-entry semantics
([measurement](../../Measurements/PlainMenuUia/2026-10-02/README.md)).

Tree and Grid model getters may reenter application code. Snapshot construction revalidates the control lifetime,
its path and borrowed model after each getter and aborts the entire unpublished snapshot if they change. Navigation,
point geometry and focused-item lookup follow the same rule. Native publication permits two attempts per flush;
a later query or queued flush handles any further mutation. A flush that cannot finish (a getter that throws,
allocation failure, or state that changes during both attempts) keeps the last complete snapshot published, and
`WM_GETOBJECT` and root acquisition still return the canonical root, which answers from that snapshot. Before the
first complete publication the root has no snapshot and answers `UIA_E_ELEMENTNOTAVAILABLE`, as a retired element
does, until a later query or flush publishes one. Clients reacquire a collapsed semantic root after its
control is replaced; retained elements never bind to a replacement at the same path. Grid row, header and cell
properties return `UIA_E_ELEMENTNOTAVAILABLE` when their record is no longer materialized in the current snapshot.
These rules do not extend borrowed model lifetime or qualify destruction of the entire host from a callback.

Modal and asynchronous menu tracking both activate the root popup
for native keyboard dispatch; submenus never activate. Dismissal restores the previously focused
owner control while the menu still owns focus. Logical entry navigation does not change that
session's native focus target. It scrolls a popup only as far as the chosen row needs, except that reaching the
first or last navigable row (Home, End or a wrapping arrow) also shows the menu's edge: its padding and any header or
separator before or after that row, when they fit with it. Native activation of a popup never selects an entry: it restores only a row
that keyboard or UIA navigation already chose, so a pointer-opened menu has no keyboard target and
publishes no transient focus. Explicit UIA focus of any focusable row in a native menu popup,
including non-command rows such as sliders, follows the session rule for both root menus and
submenus, independently of their activation style. The rule belongs to the popup host, not to the
MenuItem role: other controls with that role keep ordinary UIA focus transfer, and embedded host
focus stays with its bridge. Bounds follow the visible scrolled row geometry and DPI reflow; a row
whose bounds change republishes the provider snapshot even without a focus change, so hit testing
and BoundingRectangle match the scrolled rows. A row the viewport scrolls away stays an element,
reports `IsOffscreen` and no rectangle, and its UIA focus scrolls it into view. Posted UIA
actions carry popup-instance identity so HWND reuse cannot dispatch an old action into a new menu;
retained providers disconnect on teardown. Implementation and validation are tracked in the
[menu description plan](../Plans/WIP/MenuDescriptions_2026-09-21.md).

The modal menu loop prioritizes registered menu-bar hover handoffs with mouse and keyboard input, before
draining popup paints or unrelated owner traffic. A continuously dirty popup must not starve a queued root
switch. Existing hover sequence validation, popup lifetime checks and keyboard/pointer ordering remain required.

Context-menu mnemonics consume the Unicode character produced by the active keyboard layout
(`WM_CHAR`/`WM_SYSCHAR`), rather than deriving a letter from a virtual key. An explicit ampersand
mnemonic wins over an inferred label mnemonic; when multiple enabled rows match, successive
characters move keyboard selection through those rows and do not invoke one until the match is
unique. A slider row's mnemonic moves the keyboard selection to the slider, whose stop the arrows choose and Enter
commits. AltGr (a right Alt with Ctrl down) keeps the menu open, so its characters can be mnemonics; any other Alt
dismisses it. Right-to-left sessions map horizontal arrows to the reading direction for submenu opening,
closing, root switching, and slider values: the key that closes a submenu does only that, and the forward key on a
row without a submenu moves to the next root, as Right does left-to-right. A menu that another thread's message
dismisses ends its loop at once. High-contrast selected rows use the system selection
foreground/background pair, and disabled row text uses opaque system GrayText.

While a popup holds mouse capture Windows sends no `WM_SETCURSOR`, so the menu chooses the cursor on every delivered
pointer move: the standard arrow over its popups (and while one drags a slider or scrollbar), and outside them the
cursor the window under the pointer chooses when that window belongs to the menu's thread (it receives
`WM_NCHITTEST` then `WM_SETCURSOR` as without capture); another thread's window is never messaged and shows the
arrow. That window is messaged from a posted message, outside the menu's own handlers, so its `WM_SETCURSOR`
handling may close the menu. The cursor is chosen once more when the menu takes or moves capture, and the window under
the pointer chooses again as soon as the menu closes. These are one-shot reads of the pointer; an idle menu never polls
it. If no delivered move has established pointer history, the capture-time read also
initializes that history. A stationary capture message must preserve a keyboard-opened
popup's first-item selection; a genuinely moved pointer must still select its hovered
row. Capture changes must not overwrite existing delivered movement history.
A menu's row layouts and accessibility proxies exist only while it is open: closing it returns them, even
while a client still holds its row elements. `DebugGetContextMenuResources` counts them exactly (popups, row text
layouts and the accessibility records of menu-popup snapshots, wherever held), so a test asserts that a closed menu
holds none, whatever the renderer and allocator keep.

Custom controls overriding `OnFocusChanged` MUST invoke their base implementation so `HasFocus`,
focus chrome and UIA keyboard-focus properties acknowledge the host transition. A stored host
focus pointer alone is insufficient. Consumers qualify both visible focus and raw provider state.

`SetFocusControl(control, false)` changes logical focus without taking HWND keyboard focus, including for editors
and repeated calls on the already focused editor. A visible, sized native host may maintain its editor cache while
inactive; it activates TSF and its system caret only while that HWND actually owns keyboard focus. Hidden or zero-sized
native hosts have no active text-input session. Embedded hosts retain application-owned focus and scheduling.

Win32 focus stays on a window host's HWND while focus moves between its controls (Tab, arrows, pointer,
`SetFocusControl`), so the host raises the UIA focus-changed event itself, from the difference between two published
snapshots, for the element GetFocus reports: a tree's focused item, a grid's focused row, a control, or the window
itself once no control has focus. It raises it only while its window holds the foreground's keyboard focus and a client
listens: a window that has just lost the foreground (a queued message moving focus after the user switched away)
announces nothing. When the window itself gains focus, Windows raises the focus event and UI Automation answers it. The
first focus event of a window it has not reported before is answered with a call of the fragment root's GetFocus, on
whatever thread, which reads the published snapshot when it runs (window-host providers are not marshaled to the
window's thread). Its later focus events for the window are answered without GetFocus: it asks the window's root element
whether it has the keyboard focus, and reports that element when it does and nothing when the focus is inside a control
(observed with an in-process client on Windows 11 build 26200: the activation of a seen window whose focus is in a
control was reported by nothing, and a click in that turn by nothing but the host). Whatever the activation itself
focuses or restores is published but not announced on the way, so a window's first activation is reported by that call.
A later one, which UI Automation answers without asking, the host reports itself: when the turn of the gain ends (see
below) it announces the focused element, unless it announced a move of that turn already or the root element itself
has the keyboard focus (a root that stands for its single focused control, which UI Automation reports). Which of the two
a gain is, the host reads from the count of GetFocus calls as the gain begins, before it publishes anything: UI
Automation asks only for the first focus event of a window it answers, so a call counted by then answered an earlier
event, and this gain's comes later and reads what the gain published. A click that activates the
window sets its control after the window's WM_SETFOCUS, in the same turn of the loop and before UI Automation has acted
on the event, so the host decides who reports that move. In a window no GetFocus call has ever begun on, the call that
answers the first focus event comes after the move and reports the clicked control, so the host leaves the move to it
and a client hears the control once, as it hears the control an activation focuses. Each call counts itself in the
window's provider target before it loads the snapshot, and the host reads the count right after it stores the snapshot
of the move, all four operations sequentially consistent, so a call the count does not include loads the snapshot after
the store, and one it does include may have answered the event with the control the activation focused, which the host
follows with its own announcement (at worst a duplicate). In a window UI Automation has asked before nothing else
reports the clicked control, so the host announces it itself and a client hears it once, and the end of the turn adds
nothing. The turn ends when the registered message the host posts to its window at the gain is dispatched (the message
of an earlier gain, which the window lost before the loop turned, ends nothing), or when the window loses focus; should a
window procedure never hand the host that message, it ends after 500 ms, so a move is never left to the system's event
for long, but the activation of a window UI Automation answers without asking is then not announced. A move in a
later turn, such as a click in the window that is already active, is announced by the host. An element's SetFocus moves
the host's logical focus before it takes Win32 focus, so the activation never first focuses (and reports) the window's
first control; a UIA client's SetFocus first has UI Automation focus the hosting window, which reports the window's
current focus the way a dialog's activation does, and the requested element is the last one reported. Native menu popups
raise theirs once a keyboard transition completes, and embedded hosts raise theirs from the snapshot diff. A
native HWND host coalesces ordinary dirty state into a posted snapshot publication. Owner-thread UIA
queries flush dirty state before reading; worker-thread queries read the last complete immutable
snapshot. The event-diff baseline advances separately when the queued publication drains, so an
owner-thread query does not consume pending events. An interested host publishes an actual focus
transition synchronously before returning, which keeps the first worker `GetFocus` answer current
and captures focus-event attribution at that publication. Detaching retires the snapshot and
attachment identity so an old queued flush cannot publish into a later host on the same HWND. Without
a requested provider or UIA listener, mutations remain dirty without loading a snapshot, resolving focus paths
or posting a new flush. Existing queued publication is preserved; a later owner query resolves the latest revision.
An internal diagnostics override is scoped to the owning thread, HWND and attachment cookie and changes only the
mutation-interest decision, never provider-query freshness, other hosts or global listener state. These
native scheduling rules do not change embedded hosting: the application still calls
`UpdateAccessibility` after coherent preparation, placement or focus changes. A
focus-changed callback that removes the control it was told about leaves no control focused. A control disabled, hidden
or removed while it has focus loses it at the host's next message, which publishes the change, so a client hears the
window. Replacing the whole tree (`SetRoot`) is compared with the tree before it, not with the empty snapshot standing
in during the swap.

A window-host element keeps the identity of the control it was created for, as an embedded element does. Once that
control is removed, or another control takes its tree path (a rebuilt list or tree), every call on the old element,
including a tree item's or grid row's, its runtime id and an action already queued for the window's thread, returns
`UIA_E_ELEMENTNOTAVAILABLE` and never reaches the new control, and the new control's elements (its items and rows too)
have different runtime ids: they carry a per-process serial the control receives when first published, which no later
control reuses. The window's element is bound the same way while a collapsed semantic root (below) makes it a
control's element: once that control is replaced, or stops being the only one, a client's window element is gone like
any other, except that its focus and point queries answer that nothing is there, and the window gives a newly
acquiring client a fresh element for what it now shows. A window's element acquired while no control collapsed into
it stands for the window, and is gone in the same way once a control collapses into it. An element's focus or
invocation that runs a focus-changed callback which rebuilds the controls ends there and reports the element gone:
`Button::Invoke` does not touch the button the callback destroyed, and the callback's own focus choice stands. So does
a selection action (Select, AddToSelection, or the focus of a grid row or a grid cell, which selects it)
whose selection delegate rebuilds the controls: the destroyed grid or tree is never focused. Every control that focuses
itself from an input handler (a press, a double click, a context menu, a mnemonic, a key or tab press that selects a
tab, removing the tab that held the focus) stops there when the focus callbacks destroyed it, and touches nothing of
its own. A grid input that changes the selection (a click, a key, Ctrl+A, or collapsing the group of a selected row by
a key or a press on its header) likewise touches nothing of a grid its selection delegate destroyed, and neither do a
checkbox toggle whose model change moves the selection and the application's own model changes (`NotifyDataChanged`,
`SetModel`, `SetSelectionMode`); a tree's `NotifyDataChanged` calls its delegate last. A republish that adds, removes
or replaces a semantic control raises StructureChanged (ChildrenInvalidated) on the window's element, so a client
navigates again.
Structural changes alone (`AddChild`, `ClearChildren`) republish the tree at the next focus, size, pointer or state
change, or through `RefreshAccessibilitySnapshot`. An event about a control comes from that control's element; only
the control a collapsed semantic root stands for reports through the window's element.

Library event tests use a client thread subscribed to the actual window subtree. Bounded quiet waits fail explicitly
on timeout, while diagnostic provider-attempt/client-arrival timelines remain separate from event semantics.
Observed silence cannot rule out later delivery. Unchanged Grid republishing must emit no selection changes and
the client assertion remains empty; an unexplained late event remains an investigation rather than a waived assertion.

A window whose only semantic control is a Tree, a Grid, a Button, a Checkbox or Toggle, a TextField, a ComboBox, a Slider or a
ColorSwatch, or whose root has the Status role, has a collapsed semantic root: the window's element stands for that control, with
its control type, name, value and patterns, and the control has no element of its own. What the control exposes of its own is
that element's children: a tree's items, a grid's headers and then its rows (a row's children are its cells, and a cell's parent
is its row), a masked field's reveal button. The window's element is their parent and their fragment root. It is also the
selection container of a tree item or grid row, the containing grid of a grid cell, the enclosing element of a text range of the
field, and the element every event about the control comes from, so a client walks from the window's element to the parts and
back, and sees the control as it sees the same control in a window with a second control, apart from which element stands for
it. That includes its events: UI Automation drops an event raised on an element it cannot reach from the window through the
parents, so an item whose parent chain ended at a second element for its tree (as a collapsed root's items once did, with the
tree's element parentless) never reached a client subscribed to the window. The [selection events](#selection-events) of a tree or a
grid reach it the same way (the invalidation of a selection is raised on the window's element). A status root keeps its other semantic controls as
children of its own. An embedded view is never collapsed: its root element is the child of the application's element (the site's
`Navigate`), the control's element is the root's child and the control's items are the control's children, the same chain from
the application's element to an item, so the application's element is where an embedded client subscribes.

An element resolves its control without scanning the tree. Each published snapshot carries lookup tables built with
it: a control's record by its path (which also gives its place among its siblings), a fragment's hit rectangle by kind,
path and item, and a control's record by its address. A provider call examines a few table slots, however many
controls the window holds, where a scan of the records examined half of them for every call and a client walking every
element paid that for each one; an event finds the path of its control by a search of the sorted addresses, about log2
of the records, and a walk down that path to confirm it. The tables describe the tree as published, so an event about a
control they do not hold (added since the publish, hidden, or no element) searches the live tree, as it always did, and
a control they do hold is confirmed against the live tree before it is used. What lives inside one control still scans
that control's own items (a tree's items by id, a grid's rows and cells), and a hit test by point scans the hit
rectangles in paint order. A diagnostics counter of what resolutions examine, and a check of the tables against a scan
of the records, back the tests.

Buttons with acknowledged disclosure state expose ExpandCollapse, consistent state properties and
state-change notifications. Expand/Collapse requests are idempotent against current acknowledged state,
reject disabled controls and invoke caller behavior only when a change is requested. Caller callbacks
run outside the accessibility snapshot mutex and may replace the tree. They own content visibility
and focus recovery; the provider does not invent a body subtree or directly change application state.
Clearing disclosure removes the pattern; removed/hidden controls cannot be activated through old
providers. Native and embedded lifetime/acknowledgement tests qualify this implementation, with
[final native receipts](../../Measurements/LocalizedAdaptiveLayout/2026-09-20/native-ci-main-78b3/README.md)
for all six x64/ARM64 configurations. ARM64 Menu foreground capability skips remain explicit.

A RadioButton is a RadioButton element with SelectionItem, selected while it is checked, and no Invoke. Select chooses
it as its mnemonic does, focus included. RemoveFromSelection of the chosen one, and AddToSelection while its group holds
another choice, return `UIA_E_INVALIDOPERATION`; its SelectionContainer is null, because its group is no element,
so a choice in a group reports its place among the group's present choices as PositionInSet and SizeOfSet. A
change of choice raises ElementSelected from the chosen radio button and IsSelected changes from both, whether a click,
a key, UI Automation or the application made it. A Tree item that expands or collapses raises its ExpandCollapseState
change, and the rows that appear or leave invalidate the structure. A tree with prepared accessibility rows raises that
change for its focused item before or after the change (the item a key, an expander click or the focused row's
ExpandCollapse acts on), read from both sources, which are never traversed; another item's expansion raises the
structure change only.

A TabControl is a Tab element whose children are its visible tabs, TabItem elements named by their titles and
identified by stable tab ids through reordering. Its page follows it as a sibling in the flat tree of semantic controls,
as a property sheet's tab strip and page do. The selected tab is its Selection, single and required, and each tab item
has SelectionItem. Select chooses the tab as a click does, without moving the focus; AddToSelection of another tab and
RemoveFromSelection of the selected one return `UIA_E_INVALIDOPERATION`; SetFocus chooses the tab and focuses the tab
control. A focused tab control's focus element is its selected tab, and each tab reports its place among the visible
tabs as PositionInSet and SizeOfSet. A header scrolled out of the strip is offscreen,
and a closed or hidden tab's element is gone. A change of tab raises ElementSelected and the IsSelected changes, and a
tab added, closed, hidden, moved or renamed invalidates the structure. A page indicator with at least
two pages is a Slider element whose RangeValue is the page, from 1 to the page count; SetValue chooses the nearest
whole page as an arrow key does and refuses a value outside the pages. Its name is the application's: it no longer
replaces it with an English "Page 2 of 5".

Enabled state includes visible ancestors: a disabled container disables its retained descendants for UIA
properties, actions and focus, including Tree items, Grid fragments and text-range selection. Owner-thread
execution rechecks that state and reports `UIA_E_ELEMENTNOTENABLED` before invoking consumer behavior.
Queries remain available, and enabling the ancestor again permits actions on the same live identities.
Native text-service locks use the same effective-interaction rule.
This is not a claim of consumer screen-reader acceptance.



RedXe's adapter supplies widget-local physical coordinates; EmbeddedHost converts them to DIPs exactly once. It extends the generic input
contract to distinguish capture by a control from page-pan arbitration, and forwards Move/Up/Cancel for the captured
pointer. One accepted slider Down owns the gesture; edge navigation retains its reserved region. Cancel on hide,
capture loss, detach and invalidating geometry changes; no volume setter on cancellation.

Keyboard, focus, text and UIA are implementation gates, not optional polish. DxUi.lib supplies embedded text
snapshots, application-side TSF/clipboard services and lazy embedded UIA attach; those library APIs are supported
in `capabilities.json`. The host still owns OS focus, message routing and HWND association. Bounded COM/POD
transport to a plugin tree is a consumer adapter. No top-level HWND, `DxUi::Control*`, `std::function` or
STL string crosses the RedXe ABI. Clipboard and text-service behavior use explicit host requests.

An optional accessibility mechanism exposes virtual-child providers to the application's UIA root through
`EmbeddedAccessibilitySite`. Record interfaces, ownership, UI-thread marshalling, generation invalidation,
screen-coordinate transforms, and provider behavior after detach. A surviving assistive-technology reference must
never dereference a destroyed control or unloaded owner. Hidden controls and background modal contents leave
navigation; labels, Toggle, RangeValue, selection, text/value and live-state patterns must agree with visible
confirmed state. Library synthetic tests cover the attach API. End-to-end RedXe routing, real IME/touch/screen-reader
acceptance and matched text/UIA performance remain RedXe AV release gates (`embedded-host-text-uia-bridge`).

DxUi must support preview-versus-commit slider events, keyboard steps, cancellation, and externally acknowledged
values. The old `SetOnValueChanged` alone does not establish AV's commit-on-release behavior. AV commands remain in
the plugin; visual changes do not themselves mutate Windows devices. At least one real touch/IME/screen-reader
verification supplements synthetic tests.

### Current library input surface

Editors take the characters a keyboard types and leave the system its own. AltGr arrives as Ctrl+Alt: it runs no Ctrl
editing shortcut and types its character. Alt alone types nothing in an editor; a window host offers a `WM_SYSCHAR` to the
mnemonics and leaves one that names none to `DefWindowProc`, so Alt+Space opens the window menu and a menu bar takes its
access keys, as over a Win32 edit control.

EmbeddedHost dispatches pointer Down/Move/Up/Wheel/Leave/Cancel, keyboard down/up and character events to its retained
tree, with stale-focus/capture pruning. A second Down on the same control within the system double-click interval and a
16 DIP slop calls `OnMouseDoubleClick` (word selection, row activation) instead of `OnMouseDown`. Pointer Down/Move/Up/Wheel switch the host to pointer modality so keyboard-only
focus chrome does not appear on a touch. They also record the event's `PointerEvent::device` (the mouse when the
application names none), which `ControlHost::GetPointerDevice` reports to the control that handles it. A window host
records the device of each mouse message it handles from the thread's extra message information: Windows marks a message
it promoted from a touch or pen contact with 0xFF515700 in the upper 24 of its low 32 bits and sets 0x80 for touch
(`PointerDeviceFromMessageExtraInfo`), and anything else is the mouse. Hit-testing stays valid while the cached surface is paint-dirty. A changed interaction revision makes a new hit
incoherent until the next preparation. Keys, characters and text-service calls hit-test nothing and reach the focused
control, which interaction pruning keeps current, so a changed revision does not refuse them: only a hidden, suspended
or never-prepared view, or one whose input a failed callback suspended until its next preparation, does. Refusing them
lost typing, such as the character after one that showed a validation message or the second half of a surrogate pair. A drag already captured continues when some other control's bounds change and
the captured control stays in the tree and unmoved, with it and every ancestor enabled and visible. Moving, hiding,
disabling or removing the captured control, hiding or disabling an ancestor, or resizing the view, cancels that drag.
The next preparation applies that rule; input dispatched while the bounds revision is still unprepared cancels the
drag, so a consumer applies pane layout on its next preparation rather than inside the change callback.
WindowHost applies the same rule at message entry: a captured control that is still in the tree but no longer
interactive receives `OnCaptureLost` (its drag reports Cancel) instead of losing the capture silently.
A press during a captured gesture goes to the captured control, and the release of that second button keeps the
capture: only a release that leaves neither the primary nor the secondary button down ends it, so a right-click while a
slider's thumb is held neither ends the drag nor strands it. An unhandled press clears the focus only when it continued
no captured gesture and moved no focus itself, so a container that forwards presses (ScrollPanel) does not blur a child
that focused itself on a press it then declined. A control with no meaning of its own for a double-click treats it as a
press: a second right-click within the double-click time opens a Grid's, Tree's or ComboBox's context menu again, and a
second click on a Tree's or ScrollPanel's scrollbar pages or grabs again.
Native capture acquisition is synchronous and reentrant: `SetCapture` can deliver `WM_CAPTURECHANGED` to the
previous HWND before the initiating input handler returns. The previous owner's `OnCaptureLost` callback may replace
the initiating host's root and retire the control that requested capture. Controls MUST publish pending capture
state before transfer, retain a weak lifetime across it, and revalidate both lifetime and current host ownership
before later member access or callbacks. A retired requester consumes the original input without continuing against
the replacement tree. This does not extend to destroying the owning host during its own callback.
ControlHost retains native Win32 TSF/IME and UIA behavior, exercised by the
ported suites. EmbeddedHost owns no OS-focus HWND. Application-side `TextInputServices` borrows the caller's HWND;
`AttachAccessibility` publishes providers without creating one. Cross-plugin COM/POD transport, composition/IME
routing through a consumer host, and real assistive-technology attachment remain RedXe AV release gates
(`embedded-host-text-uia-bridge`). Native popup windows are a Win32-host capability; embedded consumers use
ComboBox/PopupLayer overlays or host-owned menu services.

Slider::SetOnChange reports Preview while dragging and exactly one Commit on accepted release, including an
unchanged final value. Capture loss, Escape, hiding or detach reports Cancel and restores the initial value. Keyboard
steps report Commit. Hover and press ease painted chrome only: a 6 DIP track, inner thumb 14→20 DIP hover and 20→16 DIP
pressed, inside a fixed 24 DIP gray chrome disc. A drag that a touch contact began (`ControlHost::GetPointerDevice` at the
press) also eases in a 48 DIP touch halo and eases it out after the release; Cancel removes it at once. The 48 DIP hit
band and 24 DIP grab radius do not change with hover, press or the halo. Keyboard steps and
RequestValue ease the painted thumb to the committed value, then stop requesting ticks. Pointer drags and SetValue snap
the painted position so live acknowledgement cannot lag. Reduced motion snaps every visual and requests no slider ticks.
The halo's 48 DIP target is clipped to the Slider bounds and any active ancestor viewport and host clips; hit testing stays
within the declared control bounds, and popup/menu overlay layers paint above it. Give the Slider 48 DIP of cross-axis room
to show the full halo while keeping neighboring content outside its bounds.
SetValue updates from externally acknowledged state without firing an input callback, including snapping a pending
animation when the acknowledged value equals its accepted target. Existing
SetOnValueChanged remains the legacy live-value observer; AV uses SetOnChange and calls the OS setter only on Commit.
Callback-driven root replacement is supported and covered by a regression test.

### Embedded text-state transport

`ReadTextInput` returns an owned snapshot of the available focused control, a view revision and caret/viewport
bounds in view-local DIPs. It clears its output on failure. No snapshot is available while hidden, suspended,
detached or incoherently prepared. A cooperative `Prepare` that returns `S_FALSE` while `NeedsPreparation()` is true
indicates yielded work, not ready text geometry; ordinary `TextField` input remains unavailable until preparation
completes on a later owner turn. `ApplyTextInput` accepts only that view's current revision; any intervening
invalidation, focus/tree replacement or device/attachment lifetime change invalidates the token. Read again after
an accepted edit. These C++ records stay within a module; a consumer defines its own bounded plugin transport.

Preview imports displayed text, selection and composition markers without a model notification. Commit notifies
once relative to the pre-composition text, including when the committed text already matches the preview. Cancel,
focus loss, hiding, zero-size suspension and device replacement discard composition without committing it.
Externally replaced text survives cancellation. Preview/cancel/selection preserve TextField undo/redo history;
a committed composition is one undoable edit. Escape cancels composition before ordinary application handling. Read-only policy belongs to the control and cannot be changed by
an imported snapshot; selection is still available. Validate text/range/clauses before mutation, with a 65,536
UTF-16-unit text ceiling and 256 clause boundaries. A callback may destroy the edited control; no access follows
without lifetime/focus revalidation. All APIs are UI-thread operations outside composition/rendering.

This API does not by itself attach an OS text store. `TextInputServices` is the application-side TSF/clipboard
adapter in the same archive. Real IME/touch/screen-reader acceptance and RedXe plugin transport remain consumer
gates. Native and embedded controls share the existing text model and rendering.

Application code that DxUi calls (control overrides, callbacks, delegates and model getters) throws only `std::exception`
types. One escaping it is contained where the host dispatches: a window message, an embedded input entry point, an
animation tick or a UI Automation action ends that dispatch, a key, character or mnemonic counting as taken, and the host
stays usable; a failed UI Automation action also republishes. The boundary catches named standard exceptions only,
mapping allocation failure to `E_OUTOFMEMORY`. Translate another type, such as C++/WinRT `hresult_error`, inside
the callback; unsupported exceptions can terminate the process.

Native text-store notification callbacks may destroy or replace their control, change focus, or replace text.
The store revalidates control lifetime, focus and text before applying selection or notifying TSF. Callback
exceptions return failure across the COM boundary; they never unwind through it. A failed continuation preserves
the callback's newer state and balances the sink edit transaction. Editable ComboBox callbacks receive an owned
text snapshot so their argument and callable survive destruction of the control.

The native WindowHost text store stages edits for each TSF lock. One TSF edit inserts at most 65,536 UTF-16 units, while the
document is as long as its field allows. The shared ACP store separately honors the target's document ceiling:
application-side snapshots remain limited to 65,536 units. `QueryInsert`, query-only insertion and mutation MUST
use the same prospective replacement length, reject overflow before mutation with cleared outputs, and permit
same-length replacements at capacity. The advised sink may advise again to change its mask, and another sink is
refused with `CONNECT_E_ADVISELIMIT`. An active composition previews text without a
model notification; `OnEndComposition` restores the captured base and applies the final state once, so the whole
composition is one undo step even when the last preview already equals the committed text. Sequential completed
compositions in one TSF lock have separate undo units. Disconnect cancels the preview and restores the base only
while the live document still matches the preview owned by that composition; a newer application replacement wins.
The native IMM path follows the same history rule: preview and cancellation
preserve history, while the result string is one committed edit. Synchronizing newer application text retires the
old preview and its metadata immediately. Late composition/result payloads remain rejected until a fresh
`WM_IME_STARTCOMPOSITION`. Result notifications may synchronize, replace text, move focus or begin a newer
composition; the old message cannot overwrite that state or clear its successor's metadata, and focus transfer
preserves the already committed result. Cancellation revalidates the live control, session, composition and owned
preview after extensible state reads; replacement or throwing callbacks cannot restore an old base into a successor.
A failed owned state read leaves composition retired and permits a later ordinary synchronization.
A composition message with no GCS flags cancels the owned inline preview;
IME read-string updates do not cancel it. Composition range, caret, conversion-target and
clause metadata are cleared when composition ends; range and caret geometry follows the control's current text
viewport, including single-line horizontal scroll and multiline scrolling. A masked TextField does not activate
TSF, rejects native text-store locks and discards IMM composition. Its UI Automation Text pattern is unavailable,
and its accessible name/value/text never contains the password. Host `WM_GETTEXT`, `WM_GETTEXTLENGTH` and
`WM_SETTEXT` remain window-caption messages while an editor has focus; editor state is exposed by the supported
control and UI Automation APIs.

Native TSF activation stages its document, context and store before publishing the session. It revalidates the
focused control and transition generation after each callback-capable TSF operation, including context push and
focus association. Deactivation detaches the old session before calling TSF and cleans up only its own retained
objects; a reentrant replacement session remains active when the old context is popped. Each production text store
is bound to the host's dispatch cookie, so callbacks from a retired same-control session cannot edit text or clear
composition metadata owned by its replacement. Once activation commits, one bounded posted reconciliation reports
text, selection and layout changes made by synchronous TSF callbacks; a combined text/layout change emits one layout
notification. Deactivation and activation-failure cleanup clear thread-manager focus only when it still names the
document being retired. Process-exit detach abandons borrowed observers before native teardown; a synchronous TSF
pop callback cannot restore focus or reactivate the editor, and the retained root and native text cache are cleared.

Changing a field to read-only or masked cancels its pending native preview while preserving the current policy.
Hiding or suspending a zero-size native host cancels its native session. Late IME messages cannot reactivate a
hidden, suspended or unfocused HWND. Showing or resizing a host resumes an eligible editor only while that HWND
retains keyboard focus; resumption does not activate another window. Native text-store locks also require the
current logical editor to remain visible through its ancestors. `EM_REPLACESEL` rejects read-only and masked
editors before inspecting the replacement pointer.

### Application-side text services

TextInputServices in the same DxUi.lib attaches to an application-owned HWND on its COM STA. It creates no
renderer, swap chain, worker or timer. SetClient lazily creates a TSF document and associates focus; clear the
client on focus loss/view removal and detach before destroying the HWND. One client represents one immutable
focusId. The embedded snapshot exposes that identity separately from its frequently changing revision;
leaving and returning to a field changes focus identity even when no intermediate snapshot was read.

SetClient and ClearClient publish retirement before calling the old client or TSF. A client or TSF callback may
install a successor; the older operation stops and releases only its own staged or retired resources. Deferred
lock requests retain an immutable store generation and cannot dispatch into a successor. Retirement preserves
an independently focused TSF document when restoring the HWND association. Detach clears the HWND identity before
callbacks and rejects reentrant Attach/SetClient until teardown finishes; a later explicit Attach remains valid.

The application implements TextInputClient in its own module. Its Read/Apply/Cancel and physical-screen geometry
operations adapt application-defined transport; neither the HWND nor a DxUi C++ object crosses a plugin ABI.
Read returns no state after its focus identity is replaced. Apply validates the original revision; Cancel verifies
the focus identity and cancels preview only. Geometry is transformed once by the application using the actual
displayed viewport. Retained TSF objects become disconnected before their client or control lifetime ends.

The COM text store shares native control adaptation with the application client adapter. Application edits are
staged for one TSF lock because initial insertion may precede composition-start notification. Preview changes
displayed text without committing; completion commits once, including unchanged preview text. Failed/stale/focus-
replaced transactions do not retry against a new revision. An owned callback argument survives disconnection.
`ITextStoreACP::SetText`, `InsertTextAtSelection` and `SetSelection` do not echo their own text or selection edits
through `OnTextChange` or `OnSelectionChange`, including while `RequestLock` is active. The store reports an actual
prepared layout change separately. External client changes are reported outside an active lock; a change observed
from a callback during a lock is coalesced onto the existing posted owner-thread work message and reported after the
lock unwinds. A nested message loop that consumes that message early does not repost until lock release. Notification
callbacks retain the advised sink and stop the remaining notification sequence if the sink connection, target or
reported state changes reentrantly. Native-host synchronization follows the same no-echo rule for TSF-originated
edits while reporting separately observed control changes.

Forward PreTranslate before TranslateMessage/DispatchMessage and HandleMessage from the application window
procedure. Nested synchronous locks fail; asynchronous requests coalesce into one pending lock with the strongest
requested access. A generation-tagged posted message, registered like DxUi's other private messages (see the
[window hosting contract](../Rendering/Rendering_Win32Host.md)), grants it after the active lock, without recursion or
a timer.
Clear/detach invalidates queued messages. NotifyChanged publishes external edits outside an active TSF lock; if a
callback reports a replacement while the lock is active, the store coalesces it onto the deferred message and emits
it after the transaction ends. A failed stale continuation therefore cannot hide the callback's newer document or
announce a partial TSF edit as an application change.
Escape clears a composition before ordinary editor handling; the application refreshes its focused client afterward.

The shared clipboard backend opens the clipboard once, without retry sleeps. Reads honor the allocation extent,
require a terminator and reject malformed UTF-16. Native transport has no embedded-editor size ceiling: valid
selections of 100,000 UTF-16 units and larger are supported subject to representable allocation size and available
memory. Writes check terminator/byte arithmetic before allocation and reject embedded NUL; allocation and ownership
failures are explicit. The application-side embedded edit/snapshot ceiling remains 65,536 units: an oversized paste
fails without truncation or document mutation. Copy requires a selection; masked copy/cut
and read-only cut/paste fail before clipboard access. A failed copy cannot delete selected text. Paste normalizes
line endings and commits once against the captured revision. Embedded controls have no clipboard HWND: commands
go through the application service. Automated control suites inject a private clipboard, including their setup/read
helpers, and never use a desktop clipboard as a test data channel.

Rich file-reference paste optionally uses StructuredTextClipboard with bounded wide
CF_HDROP and Unicode fallback under one nonblocking clipboard ownership attempt.
ANSI file lists and malformed/unterminated buffers MUST fail without publishing
partial content. Limits are 16 files, 32767 UTF-16 units per file and 65536 total
file units. Existing TextClipboard Read/Write and ordinary text clients remain
Unicode-only. Application TextClipboardPasteTransform prepares source without
mutation; EmbeddedHost validates focus/revision before transformation and the
normal ApplyTextInput commit validates again. A changed document path MUST
invalidate that transport revision. Read-only/composition checks precede reads;
stale source, selection, base or ownership MUST NOT commit old clipboard work.
Prepared canonical Rich contexts MUST transform individual paths into images or
full-filename links through the same native/embedded source replacement contract.
Mixed lists MUST remain ordered and commit once. Source/code contexts retain
literal text; the transport MUST NOT open, move or copy referenced files.

File drag-and-drop remains pending under the
[file-drop plan](../Plans/WIP/RichTextFileDrop_2026-10-20.md). Its intended contract
is a Unicode external file list, application-owned OLE registration and COPY-only
reference insertion at a prepared source caret. Preview MUST preserve source,
selection and focus; a successful drop MUST commit once and preserve the prior
directional selection in undo. Native and embedded routes MUST reject stale
ownership/geometry/source/base, composition, unavailable targets and malformed
data before mutation. The future adapter MUST reuse the paste image/link policy
without clipboard access or synchronous file I/O. This requirement does not
claim an implemented drop capability or completed drop tests.

This implements a TSF/clipboard service, not a real IME/AT product pass. The generic consumer connection, full
display-attribute/IME acceptance, real touch/IME/screen-reader checks and matched text/UIA performance remain open
on RedXe AV (`embedded-host-text-uia-bridge`). Library `AttachAccessibility` is a separate supported API; it does
not close those gates.
Reference: Microsoft [text stores](https://learn.microsoft.com/en-us/windows/win32/tsf/text-stores) and
[composition ordering](https://learn.microsoft.com/en-us/windows/win32/tsf/compositions).

### Prepared text geometry and public consumer

EmbeddedHost exposes revision-checked HitTestTextInput and GetTextInputRangeBounds in view-local DIPs.
Dirty/unprepared geometry returns S_FALSE with cleared outputs; state remains editable when its tree is coherent.
ReadTextInput omits optional bounds until preparation completes. A successful fully clipped range has empty bounds
and clipped=true. The app performs the one DIP-to-physical-screen conversion. Geometry for staged TSF text returns
TS_E_NOLAYOUT, and the app calls TextInputServices::NotifyLayoutChanged after preparation. Notifications retain
the store through callbacks that release the application's last reference. Clear/disconnect prevents stale focus
sessions from receiving edits, cancellation, or geometry.
An asynchronously requested lock consumed while another lock is active retains its pending flags without reposting
into a nested message loop. Releasing the active lock schedules one coalesced notification. If that post fails, the
flags remain pending and the next real lock request retries; there is no timer, busy poll or unbounded immediate retry.

Native host caption messages (`WM_GETTEXT`, `WM_GETTEXTLENGTH`, `WM_SETTEXT`) keep their HWND meaning while an
editor has logical focus. Editor text is available through control APIs and the supported text/value providers.
The edit-message shim preserves `EM_SETSEL` anchor/active endpoint order even for a reversed range; `EM_GETSEL`
returns the ordered bounds. End -1 means the text end and start -1 collapses the selection at the requested end,
including the native CRLF-to-logical LF mapping. See the [Windows selection contract](https://learn.microsoft.com/en-us/windows/win32/controls/em-setsel).

The shared text store supports the documented InsertTextAtSelection flags: NOQUERY permits absent ACP outputs,
QUERYONLY makes no edit and obeys the same capacity limit, and their combination is invalid. These rules follow
the [Windows insertion contract](https://learn.microsoft.com/en-us/windows/win32/api/textstor/nf-textstor-itextstoreacp-inserttextatselection).
Prepared-layout notification follows the [Windows layout contract](https://learn.microsoft.com/en-us/windows/win32/api/textstor/nf-textstor-itextstoreacpsink-onlayoutchange).

Samples/EmbeddedControls --text-input consumes only public headers and the one archive. It demonstrates a
profile-name text field, application-owned TSF attachment, physical geometry, private-clipboard output checks,
focus cancellation and Unicode rendering alongside the supplied-device toggle and slider. Live mode uses
per-monitor DPI and resizes the caller-owned target/swap chain outside composition. Real IME/touch/UIA acceptance
remains a separate gate; no UIA bridge is claimed by this text-service sample.


### Shared embedded UI Automation providers

#### Optional prepared Tree row source

`ITreeModel::CapturePreparedAccessibilityRows()` may return an immutable shared source for the complete current
visible-row semantics, including offscreen rows. The default implementation returns null; null keeps the existing
row-by-row capture through `GetVisibleItemCount()` and `GetVisibleItem()`. The source count must equal the model's
visible item count or it is discarded in favor of that same fallback. `GetItem(visibleIndex)` and
`FindItem(itemId)` are bounded, allocation-free reads returning an empty optional for a missing row. A row view
contains visible index, ID, borrowed text, depth, `hasChildren`, `expanded`, and optional borrowed `badgeText`.
DxUi exposes `badgeText` as that TreeItem's UI Automation `ItemStatus`; an omitted or empty value is the empty
status and does not change the stable row `Name`. Badge text storage follows the same retained source lifetime as
row text.

The source's shared identity is the semantic epoch used by accessibility snapshot comparison. Preserve identity only
while the ordered visible-row sequence and each row's ID, text, depth, child presence, expanded state, and badge
status are all unchanged. Any change to those fields, including reordering or replacement with the same count and IDs,
requires a new shared identity. Selection is independent of row semantics and does not require replacing the source.
On an identity change, the snapshot treats the Tree children as changed; it does not walk the source to diff rows.

Text and badge-status storage are borrowed from the source and must stay valid through every read for as long as a
shared source reference remains readable. Consumers own bounded, allocation-free source queries and capture, preparation and
admission, and arranging final source destruction on the appropriate lane, including releases by foreign retained
readers. Capture only retains an already prepared source; it does not construct, copy, or traverse rows. DxUi does
not create a row-preparation worker or define the consumer's preparation lane. These requirements define source ownership and
semantics; they do not establish a performance qualification or change the supported-capabilities record.

EmbeddedHost exposes lazy AttachAccessibility, UpdateAccessibility, GetAccessibilityProvider and
DisconnectAccessibility methods. The low-level bridge reuses the native provider/pattern implementation. The
application supplies a module-local EmbeddedAccessibilitySite for parent/sibling navigation, fragment-root identity,
OS-focus requests and posted/coalesced completion work. A consumer plugin adapts its own COM site to this C++
interface; no application HWND or C++ library ownership crosses its ABI. This component alone does not attach
RedXe's window or establish its release gate.

Attach after coherent preparation on the application's COM STA, with a nonzero process-unique attachment runtime ID.
The application provides the displayed physical-screen viewport; its dimensions must match prepared pixels. DIP
bounds convert once and clip to that viewport, including negative monitor origins. The same root provider identity
is reused during attachment. Child runtime IDs also distinguish the retained control's lifetime, so replacement at
the same tree path cannot reuse a surviving old provider. Providers advertise UseComThreading; standard UIA COM
proxies marshal actions to the owning STA. Raw foreign-thread actions fail instead of touching controls. See the
Windows [provider threading contract](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/ne-uiautomationcore-provideroptions).

Call UpdateAccessibility after changed preparation, placement or OS focus. Unchanged updates reuse the snapshot
without allocation. No snapshot work occurs in Composite, and consumers that never attach accessibility pay no
snapshot allocation or UIA wake-up cost. Prepared snapshots expose confirmed Toggle, RangeValue, text/value and
focus state. Changed active snapshots raise applicable property, text, focus, structure and (for a Tree or a Grid)
[selection](#selection-events) events only while UIA clients listen. Hidden controls leave navigation; background modal views must be disconnected by the application.
ActionCompleted allows the application to post one coalesced refresh/focus/navigation operation, without reentering
the tree inside an accessibility callback.
Embedded focus events name the focused tree item or grid row returned by `GetFocus`, including moves within an
already-focused control. No focus event is raised while placement reports no application keyboard focus.
Native Slider, Splitter and ProgressBar value changes republish their accessible range values; Slider limit and step
changes also publish immediately and request preparation. A listening client receives changed RangeValue value,
minimum, maximum, small-change and large-change properties outside the snapshot mutex. Callbacks that retire the target stop stale
notifications. Embedded publication remains the application's prepared-frame responsibility.

Tree item and Grid header/row/cell `IsOffscreen` queries agree with the published bounds after ancestor clipping:
a nonempty partially visible rectangle is onscreen, and a fully clipped fragment is offscreen. Navigable retained
selection providers need not have visible geometry.

Native WindowHost structure invalidation uses one lazily created thread-pool work object per accessibility target.
The owner retains its canonical root and published snapshot in a single replaceable pending slot; one delivery and
one pending generation may be retained. A later publication replaces that pending generation instead of growing a
queue. Delivery runs in an MTA without borrowing controls or dereferencing the host, so a real UIA callback can
read the current snapshot and synchronously invoke an owner action while the owner pumps messages. A successor
is submitted only for a new explicit publication received during delivery. Work creation/COM initialization
failure is logged; no retry timer or polling is introduced. Disconnected targets are checked before delivery, and
surviving providers still enforce their ordinary stale-target guards. Teardown does not wait for a UIA client:
each submitted callback retains its target until return, and closing the work object defers its reclamation until
outstanding callbacks finish.

Embedded structure invalidation uses the same bounded, coalesced delivery protocol. The canonical provider stays
in its supplied STA/site apartment. The owner registers that provider in the COM Global Interface Table once;
delivery resolves a marshalled proxy in its MTA before calling UIA. No raw embedded provider crosses apartments.
This invalidation carries no historical property values, so the slot and held delivery retain no row snapshot;
provider reads resolve the current coherent epoch. Disconnect clears pending work and revokes the registration on
the owner before releasing the canonical provider. Already resolved proxies enforce the disconnected target
guards, and teardown never joins client delivery. Registration/work creation failure logs the failed delivery and
allows a later explicit semantic publication to retry; there is no synchronous delivery fallback. Selection, text and
focus events retain their separate event contracts; embedded control-property transport is described below. See the [COM apartment interface contract](https://learn.microsoft.com/en-us/windows/win32/com/accessing-interfaces-across-apartments).

The canonical root resolves the current published snapshot and does not retain its creation snapshot. Retained
non-root providers keep their creation snapshot for control-lifetime validation and borrowed-source ownership;
they never access a replacement control merely because its path or row IDs are unchanged. Keeping only the root
alive must not retain a superseded prepared source after publication.

Embedded control-property events use the same event-driven work item as structure invalidation. The owner captures
the property values and registers a weak-identity sender in the COM Global Interface Table; the delivery MTA resolves
a proxy before calling UIA. Interface-valued properties such as `LabeledBy` also register each non-null old/new
reference on the owner apartment and resolve it on the delivery apartment; scalar and BSTR values remain owned copies.
Relationship event values use weak-identity providers, so retaining either value does not retain a prepared row epoch.
A replaced pending reference is retired outside the notification lock. Failed value marshalling or resolution drops
that event with a diagnostic; it must not manufacture an empty relationship value. No raw STA provider crosses
apartments. There is one active batch and one pending batch,
with at most one pending entry per current control identity/property ID. Repeated changes preserve the oldest
undelivered value and replace the latest value. Each publication removes pending entries for retired controls, so a
held client cannot accumulate retired control epochs. Batches own scalar/BSTR values and registrations, not row
snapshots. Disconnect clears the pending batch; an active delivery checks disconnection before each event. Delivery
does not block the owner, and teardown never joins a client callback. COM registrations are revoked when the last
batch reference releases them, in its initialized apartment, as permitted by the
[GIT revocation contract](https://learn.microsoft.com/en-us/windows/win32/api/objidl/nf-objidl-iglobalinterfacetable-revokeinterfacefromglobal).
Capture/marshalling/work failures are logged without a synchronous fallback or automatic retry loop. Selection,
text and focus events retain their separate contracts.

Coalescing concerns entries still in the library's pending batch, not the number of callbacks delivered by UIA.
Holding a client callback does not by itself prove that the library's active delivery is held. Real-client tests
deduplicate successfully resolved equal payloads, reject unexpected values, and use a bounded internal delivery
gate when proving replacement of the pending batch. The gate runs outside the notification mutex before UIA;
it is diagnostic-only and dormant during ordinary delivery. Client fixtures close admission, unsubscribe and drain
accepted callbacks before retiring references, since UIA can deliver callbacks after unsubscription.

Internal embedded property-event providers retain the target and the published control's weak lifetime/identity,
then resolve properties against the current coherent snapshot. UIA can retain these event providers after delivery;
they must not pin complete creation snapshots or prepared row epochs. Replacing the control invalidates them even
when its path is reused. Public row providers and text ranges keep their existing creation-snapshot ownership.
Runtime IDs use that retained control identity after checking the current snapshot; an event sender need not hold
a creation snapshot to expose the same runtime ID as its public control element.

Hide, zero-size suspension, device replacement and detach disconnect the target before destroying controls. Old
provider actions return UIA_E_ELEMENTNOTAVAILABLE after disconnect/root replacement, and a reattachment has a new
identity. A surviving reference never accesses a replaced control by path. The application is responsible for
keeping the module containing provider code mapped until surviving references can no longer call it; releasing the
control tree is not permission to unload code still referenced by COM.

Slider::RequestValue is the user-action counterpart to silent model SetValue. It validates range, enabled state and
the absence of an active drag, then reports one Commit through the same callback used by keyboard input. UIA uses
this operation, so accessible volume edits reach the consumer model. UIA text setters validate bounded UTF-16 and
revalidate lifetime after callbacks; a callback that destroys its field cannot be followed by stale synchronization.

The component's synthetic tests exercise all three AV control kinds, physical geometry at 144 DPI, real COM
marshaling, text edits, same-path replacement, hidden/detached references and zero allocation in 1,000 clean updates.
End-to-end application tree/event routing, real screen-reader/IME/touch acceptance and matched resource gates
remain required before the RedXe AV text/UIA release gate (`embedded-host-text-uia-bridge`).

A Grid row returned by Selection.GetSelection must provide working SelectionItem IsSelected and
SelectionContainer getters even when it is offscreen or beyond the bounded row-materialization cache.
These queries use immutable selection IDs and must not materialize every selected row. Removing the
model/row invalidates retained providers; stale selection containers cannot survive that removal.

A Grid exposes `IGridProvider` with snapshot-backed `RowCount` and `ColumnCount`, and `GetItem` accepts
every valid model row and displayed column, including rows outside the viewport. Columns count in display order, as
the headers, the pixels and Ctrl+C do after a reorder: `GetItem`'s column, `GridItem.Column`, the order of a row's
cells and of the words of its name, and the column headers all follow it, while the application's delegate calls keep
model indices. Every column has a header element, offscreen while its column is scrolled out, so a cell of any column
names its header. Rows and cells have ScrollItem: `ScrollIntoView` scrolls the row, and a cell's column, into the
viewport. An action on a cell (SetFocus, Toggle) scrolls it in first, so a cell a client reached through `GetItem`, or
one of a scrolled-out column, acts where the user sees it. A request for an uncached row reads
that row's cells in addition to the ordinary visible and selected snapshot on the owning UI thread, then
publishes them in an immutable snapshot. It does not
scroll, select, or focus the row. The host retains at most the 16 most recently materialized offscreen rows;
when a row is evicted, retained cell providers return `UIA_E_ELEMENTNOTAVAILABLE` rather than resolving
through a changed row id. A grid's model-assignment generation is part of that identity, so reassigning a model
at the same address invalidates row and cell providers from the previous assignment. Row and cell factories bind
their returned peers to the snapshot that supplied the lookup, including cached `GetItem`, parent/child navigation,
point hits, focus and selection queries; concurrent reassignment cannot make those peers adopt
the replacement model. Visible and selected rows continue to use
the existing snapshot paths and limits. Out-of-range row or column indices return `E_INVALIDARG`.
Requested-row capture uses the same borrowed-model, geometry and publication revision guards as ordinary snapshots.
A callback that changes the same model can abandon that request with `UIA_E_ELEMENTNOTAVAILABLE`; a fresh request
may materialize the current row. A remembered row that a later refresh deletes, or whose cells the model can no longer
read, drops only that request: the rest of the window still publishes, and its retained cell providers report
`UIA_E_ELEMENTNOTAVAILABLE`. No returned row mixes cell values from superseded and current captures. Native
materialization flushes immediately on its owner, while ordinary mutations retain lazy publication and event coalescing.

For a native HWND host, a foreign-thread `GetItem` that needs an uncached row dispatches a bounded request
to the window thread, which must pump messages. In an embedded host, snapshot-backed requests for rows
already present may be read from any thread; materializing a new offscreen row is owner-thread-only and
returns `RPC_E_WRONG_THREAD` from another thread. The embedded owner-thread request publishes to the live target only
while its attached root, Grid control lifetime, and viewport placement still match the request snapshot. Neither path
creates a full-model snapshot.

A Tree with `SetMultiSelectEnabled(true)` reports `CanSelectMultiple`, lists every selected visible item from
Selection.GetSelection in visible order and answers SelectionItem `IsSelected` from that set, while only its focused
item (which may be outside the selection) reports `HasKeyboardFocus` and is what the window's `GetFocus` names.
SelectionItem `Select` replaces the selection, `AddToSelection` adds an item and `RemoveFromSelection` removes it through
the tree's own delegate callbacks, and `SetFocus` moves the focus alone. A change of the set raises the
[selection events](#selection-events), as a single selection's change does. A tree without multi-select reports what it
always did. See [Tree multi-select](UI_ControlsAndLayout.md#tree-multi-select).

Native consumers that know an attached HWND can acquire its canonical root with
`DxUi::CreateWindowHostAccessibilityProvider(hwnd)` from `<DxUi/DxUi.h>`. Adopt the returned owned COM
reference with `wil::com_ptr::attach`; null means the window or attachment is unavailable. Call only for a
window in the same process. A foreign-thread call dispatches to the owner with a five-second bound; the owner must
pump messages. Null also reports a failed or timed-out dispatch, and a late reply never writes into the caller's
stack. Repeated acquisitions share identity during one attachment. Detach invalidates access to the retired
tree; reattachment creates a distinct identity. Keep the owning module loaded while any provider is retained.
This API requires neither private implementation headers nor a consumer diagnostics build define.

### Selection events

A publish that changes the selection of a Tree or a Grid raises UI Automation's selection events, only while a client
listens: a window host after the focus announcement of the same publish (a list reports its focus, then its selection),
an embedded host from `UpdateAccessibility`. A tree's items are its visible items, of which it selects one, or several
with `SetMultiSelectEnabled(true)`; a grid's are its rows, of which it selects one or several (`GridSelectionMode`). Every
change counts, whatever made it: a click, a key, a UI Automation request or the application's own setters (which are silent
to the delegate, not to clients). The rules are WPF's for its selectors:

- A selection that became exactly one item that was not selected raises `ElementSelected` on that item, which says that the
  others left the selection.
- Any other change raises `ElementAddedToSelection` on each item that joined and `ElementRemovedFromSelection` on each that
  left.
- Each item those name, and each that left a selection that became one new item, also raises the SelectionItem `IsSelected`
  property change.
- More than 20 changed items raise one `Selection_Invalidated` on the control instead, so selecting everything in a long list
  raises one event. So does a change that would name an item without an element: a tree's item that left its rows (removed,
  or hidden by a collapsed ancestor), or a grid's row that left the selection out of view or left the model. A grid's rows
  are virtualized: a snapshot holds those on screen and up to 256 selected ones off screen. `Grid.GetItem` can request
  another offscreen row on demand, subject to the separate 16-row recent-materialization cache above. Of a selection that became
  one new item, that item's `ElementSelected` says the others left it,
  so an item without an element (a tree's item that left its rows, a grid's row out of view or gone) needs no event there.
- Items that only moved and a republish that changed no selection raise nothing. Turning a tree's multi-select off removes
  the items it drops; turning it on keeps the selected item, which changes nothing.

Each item's events come from its own element (a tree item, a grid row) and the invalidation from the control's element,
which is the window's element when a collapsed semantic root (above) stands for the control. The
comparison is made in publishing, never in composition. A control whose selection did not change costs a comparison of
its selected ids and no allocation. A change costs at most the ids between the common start and end of the two
selections, sorted once a side when there are more than 16, and names at most 21 items before it is reported as an
invalidation; a difference in size of more than 20 is an invalidation without comparing an id. A multi-select tree raises
what it did before single selections and grids raised these events.

Something can run while the events are raised: an outgoing call of UI Automation's in a single-threaded apartment
dispatches messages, and a message can hide, remove or replace the control, or disconnect its host. The raising then ends:
the events left are dropped once the host is disconnected (or an embedded view's root is gone), or once the control is no
longer the one published at its path, and never reach an element that is gone. The providers that raise them never touch a
control.

### RichMarkdown incomplete metadata

Status: source-envelope diagnostic integration in development under M5a; final
profile/physical qualification remains separate.

A source-bearing `SemanticRole::Metadata` child MUST expose UIA Group and retain
its exact envelope source range through TextPattern `RangeFromChild`, including
BOM and original line endings. An unclosed YAML/TOML envelope MUST provide the
missing-fence explanation through HelpText and a structured syntax diagnostic
naming the opener/source owner. No input focus is required by diagnostic queries.
Closing/editing the source MUST retire old fragments before another prepared
publication; the closed successor MUST have no stale missing-fence explanation.
This does not validate the metadata payload or establish physical IME/AT support.

Semantic snapshot liveness MUST cover source commits/reset, transient input
service previews/restoration, presentation/profile changes, binding retirement
and destruction before a later snapshot publication. Queries MUST use a data-only
shared epoch rather than dereferencing a live control on a foreign UIA thread.
Providers MUST retain their captured semantic revision, preventing resurrection
on mode/binding cycles even when the CPU semantic document is reused. Geometry
reflow alone MAY retain that identity. Immutable historical document pointers
MUST remain stable for diffs and in-flight queries; liveness is an explicit guard,
not a mutable nullable document accessor. Capture source/epoch pairs before
fallible snapshot staging. Lazy activation failure MUST preserve source/mode;
retirement itself MUST allocate no memory or rebuild a snapshot on the input path.

M4e structural commands use the same atomic source transaction and native/
embedded host synchronization. TSF and UIA TextPattern MUST expose complete
committed raw Markdown, hidden syntax and CRLF ACPs, never intermediate edits or
display coordinates. Source and directional selection publish once. Current
composition blocks semantic commands; source imports/preview/cancel/commit keep
their existing exact transaction policy. Pending analysis MUST NOT be drained
from input. If an active text-service session's cached state cannot be copied,
structural commands MUST return Unavailable without mutating source/history.
TryReadNativeTextInputState returning false leaves its output unspecified; callers
MUST ignore it. Its allocation-failure path MUST NOT construct a new aggregate.
Hardware Enter falls back to raw CRLF if no current structural plan
is available. Shift+Enter always keeps that raw replacement behavior.

In semantic Rich view Ctrl+B/I toggle Markdown styles and Ctrl+Enter toggles
tasks. Enter continues/exits lists or inserts table rows. Tab/Shift+Tab first
navigate a current table cell or indent/outdent an eligible list; otherwise they
MUST return control to host focus traversal. Both native WM_KEYDOWN and embedded DispatchKey MUST
offer Tab to the focused control before host traversal. A declined callback that
retires/unbinds that control or changes focus MUST consume the old frame rather
than traversing a successor. Ordinary live-control layout changes do not alone
cancel logical traversal. Padded-cell navigation retains a
revision-local visual site, including multiple cells at one raw ACP. Literal
pipe/backslash typing in empty/padded cells escapes source spelling. Source view
and raw TSF replacements do not acquire this structural policy.

A task press MUST stage an immutable current source plan without editing text.
Only matching release inside its checkbox can apply it. Leaving the rectangle,
capture/focus loss, source/layout/binding retirement or stale selection cancels
the gesture. Capture release is a reentrancy boundary; lifetime, binding and
interaction revision are rechecked before application. Links remain inert.
Structured semantic children/actions/styles follow the contract above; no physical IME/AT or
ARM64 runtime qualification follows from synthetic transport/client tests.

RichTextEditor's explicit M4d semantic rendering preview keeps TSF/UIA document
text, selection, history and composition offsets in exact raw UTF-16 source ACPs.
Range rectangles use actual mapped glyph fragments; hidden-only ranges remain
empty and transformed/image/task tokens are atomic. Source-range scrolling uses
these fragments without taking focus. Source line units remain ordered raw ACP
partitions, with cell-local wrapping and atomic objects coalesced across wrapped
SDK lines. All geometry is unavailable until its current host/source/resource
publication is acknowledged; queries never finish pending preparation. This is
per editor: pending work in a sibling preview MUST NOT revoke current semantic
actions, glyph geometry or vertical navigation. The editor's host binding, exact
source revision, viewport width, typography/resource epochs and semantic document
identity must remain coherent. Its own source/width/resource changes still reject
old geometry immediately, including after undo restores equal source bytes.
Pending semantic preparation uses raw source graphemes for logical commands,
not another Markdown dialect's v0 projection. Exact document Home/End includes
trailing raw trivia. A collapsed visual delete from inside a transformed/object
source token removes its complete owned token through one undoable source plan;
programmatic replacement and imported text-service state retain exact ACPs.
Generated separators use the explicit compatible M4e joining policy; semantic
UIA children/format attributes
and physical IME/AT qualification remain open in M4 and the existing platform gates.

M4d.2 source caret geometry distinguishes empty/trailing/container-only lines and
empty/padded cells without adding source characters. TextPattern2, TSF selections
and RangeFromPoint continue to expose exact raw UTF-16 ACPs. Native visual site
affinity disambiguates equal-ACP cells for painting and text-service caret bounds;
an unchanged source/selection synchronization retains it. Double-clicking a
caret-only semantic site keeps a collapsed raw selection and the clicked cell
affinity instead of selecting a neighboring paragraph/cell word. It inserts no
source and creates no history entry. Explicit source
selection or source/mode/profile replacement clears it. Caret-only boxes do not
give hidden/generated source ranges selection rectangles. Empty physical-line
deletion respects raw Unicode graphemes and CRLF; empty cell scaffolding owns no
deletable token. M4e semantic typing/commands materialize the focused padded
cell without virtual ACPs; raw imports retain exact source replacement. Synthetic
composition transport and in-process UIA clients complement the separate physical
IME/assistive-technology qualification gates.

The control that called a native publish treats it as a reentrancy boundary too: before touching itself again or
calling a pending delegate, it checks that its lifetime survived the publish. Tree and Grid selection helpers that
allow their input callers to continue report whether the control survived both its delegate and accessibility
publishing, and so do Tree's selection requests (Select, AddToSelection and RemoveFromSelection), which return false
for a tree that did not survive them. A handled input (including select-all and keyboard group collapse) remains
handled when the control is destroyed, but performs no subsequent focus, capture, activation or invalidation. Tree
model reconciliation does not call its pending selection-set delegate once publishing has destroyed the tree, and a
multi-select mode change does not invalidate after that destruction. A provider's own identity checks and the event
raiser's checks protect their continuations independently; retaining a provider or snapshot does not retain the
control.
