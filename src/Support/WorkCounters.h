#pragma once

// Private work counters (Specs/Core/Core_PerformanceAndResources.md, "Work counters"). One thread-local record of plain
// integer increments, present in every configuration and dormant until a test reads it: counting costs an increment, no
// allocation, lock or I/O. Nothing here is public API. Tests read the record through a WorkCounterScope delta.
//
// Adding a counter: add one X(group, name) line to its subsystem's list below, in alphabetical order (not at the end of
// the list), and increment it with ++Detail::Work().group.name where the work happens. Every counter is a uint64_t that
// only grows; snapshots, deltas, receipts and the per-name visitor pick it up from the list without further edits. A new
// subsystem adds its own list, struct and one line in WorkCounters and DXUI_WORK_COUNTER_FIELDS.

#include <cstddef>
#include <cstdint>
#include <span>
#include <unknwn.h>

// Frames and hosts: what one preparation, render or composition did.
#define DXUI_WORK_COUNTERS_FRAME(X)                                                                                                                            \
    X(frame, brushColorReuses)         /* Transition brushes a host served again by SetColor instead of creating a brush (W2.8). */                            \
    X(frame, brushEvictions)           /* Keyed solid brushes ControlHost::TrimCaches released, unused by the last two frames, above the bound. */             \
    X(frame, clipPushesSkipped)        /* Inner clips a ScrollPanel did not push because the paint clip already lay inside its viewport (W4.10). */            \
    X(frame, composites)               /* EmbeddedHost::Composite calls that drew the prepared surface. */                                                     \
    X(frame, compositionDevices)       /* DirectComposition devices a thread's shared native device stack created (DCompositionCreateDevice2). */              \
    X(frame, d2dDevices)               /* Direct2D devices a thread's shared native device stack created (ID2D1Factory1::CreateDevice). */                     \
    X(frame, drawCalls)                /* Direct2D draw calls seen by a test executable's counting wrappers; 0 wherever none is installed. */                  \
    X(frame, fullVisualPreparations)   /* Preparation passes that prepared every visible control's visual content, not only the pending ones. */               \
    X(frame, hitRevisionMoves)         /* Moves of a host's pointer hit revision (ControlHost::AdvanceHitRevision): InvalidateHitTest and the named sites. */  \
    X(frame, hitTestReuses)            /* Pointer hit tests a host answered from the hit result of the same pointer event, client point and hit revision. */   \
    X(frame, hitTests)                 /* Host hit tests that walked the control tree (overlays, then children) to find the control under a point. */          \
    X(frame, inkEscapes)               /* Controls whose paint first drew text beyond their bounds plus reach (W4.10): their extent becomes unbounded. */      \
    X(frame, invalidations)            /* ControlHost::Invalidate requests (a native InvalidateRect or an embedded invalidation callback), changed or not. */  \
    X(frame, pacedFrames)              /* Native paced frames (W3.6): a paced-frame message or skip timer that sampled the compositor clock and ticked. */     \
    X(frame, pacerDeferredPaints)      /* WM_PAINT a paced native host validated and deferred to its next credit instead of rendering. */                      \
    X(frame, pacerExcessUnits)         /* Frame-latency units absorbed while the credit was held: a kept credit's frame had queued (W3.6). */                  \
    X(frame, pacerInvariantViolations) /* Debug: pacer transitions that left a deferred paint with no wake; must stay 0 (W3.6). */                             \
    X(frame, pacerStaleFrames)         /* Paced-frame messages a native host ignored: their request was cancelled, replaced or belongs to an earlier chain. */ \
    X(frame, pacerStatsFallbacks)      /* Paced frames sampled at the current time because the compositor's frame statistics were unavailable. */              \
    X(frame, pacerSynchronousTimeouts) /* Synchronous native renders that found no credit within their bound and so did not present. */                        \
    X(frame, pacerTargetsOutOfRange)   /* Paced frames whose compositor target lay over three periods ahead: sampled now (W3.6). */                            \
    X(frame, pacerTeardownOrderViolations) /* Swap chains released while their pacer still held a wait or the waitable object; must stay 0. */                 \
    X(frame, pacerUnavailable)             /* Native swap chains created without a frame-latency waitable object, so their host ran unpaced. */                \
    X(frame, pacerWaitTimeouts)            /* Paced waits that saw no retired frame within their timeout (a stalled compositor); the frame stays deferred. */  \
    X(frame, paintCulled)                  /* Visible children a Panel did not paint because their painted extent missed the paint clip (W4.10). */            \
    X(frame, paintExecutions)              /* Control::Paint calls dispatched by a host, Panel or PageHost: one per painted control. */                        \
    X(frame, paintRegionFull)        /* Embedded preparations that repainted the whole view (W4.10): a full region, a new target, or no region support. */     \
    X(frame, paintRegionRects)       /* Region rectangles embedded preparations repainted alone, each cleared and painted under its own clip (W4.10). */       \
    X(frame, pixelsPainted)          /* Device pixels inside the area a preparation or render cleared and repainted. */                                        \
    X(frame, plainLabelPreparations) /* Label::PrepareVisualContent calls on a Label with neither a typography override nor a mnemonic. */                     \
    X(frame, preparationMembershipSearches) /* IsControlInTree searches started while a host preparation pass (PreparePendingPageLayout) runs. */              \
    X(frame, preparations)                  /* EmbeddedHost::Prepare calls that painted the surface. */                                                        \
    X(frame, presents)                      /* Native swap-chain Present or Present1 calls. */                                                                 \
    X(frame, presentStatuses)               /* Native presents that returned a success status other than S_OK (W3.6: the credit is kept). */                   \
    X(frame, presentsWithoutCredit)         /* Native presents made while the paced host held no frame-latency credit; must stay 0 (W3.6). */                  \
    X(frame, renders)                       /* Native WindowHost render passes (BeginDraw to EndDraw). */                                                      \
    X(frame, shadowPasses)                  /* Shadow effect passes drawn (DrawShadowEffectPass): popups, cards and other elevated surfaces. */                \
    X(frame, swapChains)                    /* Swap chains native hosts created, for composition or for a window. */                                           \
    X(frame, tickCalls)                     /* Control::Tick calls the frame schedule made for due requests (whole-tree walks excluded). */                    \
    X(frame, tickDiscoveryWalks)            /* Whole-tree ticks an embedded root installation asked for, so controls can claim ticks. */                       \
    X(frame, tickHeldWhileHidden)           /* Requests that came due while their native host was hidden and waited for its show. */                           \
    X(frame, tickLegacyTreeWalks)           /* Whole-tree ticks an anonymous RequestAnimation() asked for; the library's own controls never do. */             \
    X(frame, tickRequesters)                /* Due requests the frame schedule found at its wakes, one per requesting control. */                              \
    X(frame, tooltipDeadlineTicks)          /* Host tooltip-layer Ticks at a reached show or hide deadline; never between deadlines. */                        \
    X(frame, tooltipShowsDropped)           /* Pending tooltip shows a host dropped because it was hidden before they came due. */                             \
    X(frame, tooltipTargetMarks)            /* Changes that marked a pending or shown supplemental tooltip's target for re-resolution at the next wake. */     \
    X(frame, tooltipTargetValidations)      /* Wakes that resolved the supplemental tooltip's target again: marked, or at the tooltip's own deadline. */       \
    X(frame, treeVisitsHitTest)             /* Controls a hit test examined: the root and each child a container offered to it. */                             \
    X(frame, treeVisitsPaint)               /* Controls a paint walk examined, visible or not. */                                                              \
    X(frame, treeVisitsStatistics)          /* Controls an EmbeddedHost::GetStatistics retained-resource walk examined. */                                     \
    X(frame, treeVisitsTick)                /* Controls an animation tick walk examined, visible or not. */

// Wakes by source: each turn DxUi takes on the owner thread because something woke it.
#define DXUI_WORK_COUNTERS_WAKES(X)                                                                                                                            \
    X(wakes, animationTimer)  /* AnimationDispatcher periodic (every-frame) WM_TIMER ticks. */                                                                 \
    X(wakes, deadlineTimer)   /* AnimationDispatcher one-shot WM_TIMER wakes for the earliest scheduled deadline. */                                           \
    X(wakes, embeddedAdvance) /* EmbeddedHost::AdvanceAnimation calls the application made while animation was requested. */                                   \
    X(wakes, menuHoverTimer)  /* Menu submenu-hover WM_TIMER messages. */                                                                                      \
    X(wakes, pacedFrame)      /* WindowHost paced-frame messages a thread-pool wait on the frame-latency object posted, stale ones included (W3.6). */         \
    X(wakes, pacerTimer)      /* WindowHost one-shot skip timers: a paced tick that presented nothing keeps its credit and ticks again a period later. */      \
    X(wakes, paintMessage)    /* WindowHost WM_PAINT messages. */

// Text and typography beyond the resource kinds below.
#define DXUI_WORK_COUNTERS_TEXT(X)                                                                                                                             \
    X(text, formatCacheOverBound)       /* TrimCaches passes that left the text-format map above its bound because every remaining entry was in use. */        \
    X(text, formatCacheTrims)           /* TrimCaches passes that evicted text formats above the key bound (unused by the last two frames). */                 \
    X(text, formatEvictions)            /* Configured text formats TrimCaches evicted. */                                                                      \
    X(text, lineMetricsHeapFallbacks)   /* Text measurements with more lines than the stack buffer, which read their line metrics into a heap buffer. */       \
    X(text, nativeTextInputStateCopies) /* Text-input states copied with their document (TryReadNativeTextInputState). */                                      \
    X(text, setTextRenderingParams)     /* ID2D1RenderTarget::SetTextRenderingParams calls a host made to bind or clear rendering parameters. */               \
    X(text, textLayoutCacheBypasses)  /* Host-drawn strings the host's layout table did not serve, drawn through DrawText: no paint in progress, a box that is \
                                         not positive and finite, a string over 4,096 units, or the test seam. */                                              \
    X(text, textLayoutCacheHits)      /* Host-drawn strings drawn with a layout the host's table already held. */                                              \
    X(text, textLayoutCacheMisses)    /* Host-drawn strings whose layout the host's table created and kept. */                                                 \
    X(text, textLayoutCacheOverflows) /* Host-drawn strings drawn through DrawText because the table was at its bound and every way of the set was in use. */

// Popups and menus.
#define DXUI_WORK_COUNTERS_POPUPS(X)                                                                                                                           \
    X(popups, backdropBlurPasses)            /* Gaussian blur passes drawn over a popup's captured backdrop (Menu, ComboBox, transient surfaces). */           \
    X(popups, backdropCapturePixels)         /* Screen pixels copied by GDI backdrop captures. */                                                              \
    X(popups, backdropCaptures)              /* Synchronous GDI BitBlt captures of the screen behind a popup. */                                               \
    X(popups, comboBoxItemWidthMeasurements) /* ComboBox popup text widths measured: one per item when its widths are stale, the no-matches text once. */      \
    X(popups, comboBoxPopupLayouts)          /* ComboBox popup placements computed: UpdatePopupLayout calls whose inputs changed since the last placement. */  \
    X(popups, comboBoxPopupRebuilds)         /* ComboBox::RebuildPopupItems calls: one filter of the items by the editable text. */                            \
    X(popups, menuPopupPaints)               /* MenuPopup content paints: one per render pass of a menu popup window. */                                       \
    X(popups, menuRowsPainted)               /* Menu rows those paints drew (each paint draws every row it finds visible). */

// Lists: Grid and Tree.
#define DXUI_WORK_COUNTERS_LISTS(X)                                                                                                                            \
    X(lists, gridCollectOrderedGroups)  /* Grid collections of the model's groups (every GetGroup, sorted and sanitized). */                                   \
    X(lists, gridColumnDescRebuilds)    /* Grid column-description cache fills: one GetColumn per column. */                                                   \
    X(lists, gridGetColumnCalls)        /* IGridModel::GetColumn calls a Grid made, those of the cache fills included. */                                      \
    X(lists, gridGroupLayoutRebuilds)   /* Grid group-layout cache fills: one collection and its collapsed-row prefix. */                                      \
    X(lists, gridSelectionCopies)       /* Copies of a Grid's whole ordered selection (Ctrl+C, a Ctrl+Shift range, data-path verification). */                 \
    X(lists, treeExpansionRecordCopies) /* Tree row records copied to start an expand or collapse animation (copy, not the model's read). */                   \
    X(lists, treeExpansionRecords)      /* Whole Tree row records an expand or collapse animation keeps to draw rows the model no longer has. */               \
    X(lists, treeIdIndexBuilds)         /* Builds of a Tree's id-to-row index: one model read of every visible row. */                                         \
    X(lists, treeItemReads)             /* ITreeModel::GetVisibleItem calls a Tree made outside Paint, ITreeModel's own FindVisibleItemById included. */       \
    X(lists, treeModelLookups)          /* ITreeModel::FindVisibleItemById calls a Tree made. */

// Rich documents.
#define DXUI_WORK_COUNTERS_RICH(X)                                                                                                                             \
    X(rich, chunksLaid)          /* Rich text layout chunks laid out by a document-layout preparation (not reused). */                                         \
    X(rich, diagramPreparations) /* Native diagram paint objects begun by a semantic geometry preparation (not carried). */                                    \
    X(rich,                                                                                                                                                    \
      displayUnitsWritten) /* Display UTF-16 units a semantic build emitted from syntax; units copied from reused blocks are semanticDisplayUnitsCopied. */    \
    X(rich, documentBytesCopied) /* Bytes of a RichTextEditor's own source or display text copied into a string: slices, flattens, text-input copies. */       \
    X(rich, documentFlattens)    /* Copies of an editor's whole non-empty source or display text (a subset of documentBytesCopied). */                         \
    X(rich, elementsBuilt)       /* Semantic elements a semantic document build created. */                                                                    \
    X(rich, hoveredTitleCalls)   /* RichTextEditor::HoveredTitle resolutions: the hover title under a point, for the host's tooltip. */                        \
    X(rich, hoverScanElements)   /* Semantic elements a pointer-hover lookup tested one by one; the prepared hover index adds none. */                         \
    X(rich, identityMapNodes)    /* Nodes or boxes inserted into an identity map to pair an old revision with a new one; block-range pairing adds none. */     \
    X(rich, imageUploadBytes)    /* Pixel bytes rich-image preparation uploaded into Direct2D bitmaps (creation with pixels or CopyFromMemory). */             \
    X(rich, markdownBlockUnitsScanned) /* Source units the Markdown block scanner stepped over; closed roots imported from the previous parse add none. */     \
    X(rich, markdownFrontMatterValidations)       /* YAML or TOML front-matter validations a Markdown parse ran. */                                            \
    X(rich, markdownFrontMatterValidationsReused) /* Front-matter results a Markdown parse carried from its previous parse (unchanged envelope). */            \
    X(rich, markdownInlineFullReparses)     /* Markdown parses with a previous parse that rebuilt every leaf's inlines (reference definitions changed). */     \
    X(rich, markdownInlineLeavesCopied)     /* Leaves and opaque HTML blocks whose inline forest a Markdown parse copied, shifted, from its previous parse. */ \
    X(rich, markdownInlineLeavesParsed)     /* Leaves and opaque HTML blocks whose inlines a Markdown parse built from source. */                              \
    X(rich, markdownInlineUnitsScanned)     /* Source units inline parsing read to build parsed leaves and opaque HTML blocks; copied leaves add none. */      \
    X(rich, markdownLexicalUnitsScanned)    /* Source units the Markdown lexical scanner stepped over; prefix and suffix tokens copied add none. */            \
    X(rich, markdownNodesCopied)            /* Block and inline nodes a Markdown parse copied, shifted, from its previous parse. */                            \
    X(rich, markdownParses)                 /* Markdown parse preparations staged (MarkdownParser::Begin and its variants that did not coalesce). */           \
    X(rich, markdownReferenceUnitsRead)     /* Source units link reference definition extraction (blocks) and collection (inlines) read. */                    \
    X(rich, markdownRootTriviaUnitsScanned) /* Source units of trivia lines between top-level blocks the root partition scanned; copied lines add none. */     \
    X(rich, markdownSourceUnitsCopied)      /* UTF-16 units written into a Markdown parse's owned source copy. */                                              \
    X(rich, markdownSourceUnitsMaterialized) /* Of those, units read out of the document's piece tree rather than the previous parse's source. */              \
    X(rich, mathPreparations)                /* Prepared math formulas built (beside the testRichMathPreparations seam). */                                    \
    X(rich, otherDocumentBytesCopied)        /* Bytes copied out of any other RichText::Document (code-fence line numbers, labels, scratch documents). */      \
    X(rich, offscreenChunksLaidFirst)        /* v0 chunks laid outside the viewport window while a chunk meeting it was not laid yet (W7.3). */                \
    X(rich, estimateSampleLayouts)           /* Sample layouts measuring line height and wrapping before any v0 chunk height is known (W7.3). */               \
    X(rich, otherDocumentFlattens)           /* Copies of such a whole non-empty other Document (a subset of otherDocumentBytesCopied). */                     \
    X(rich, preparationTurns)                /* RichTextEditor::PrepareContent calls: owner turns of cooperative rich-content preparation. */                  \
    X(rich, semanticBlocksReused)            /* Top-level semantic blocks a SemanticBuilder copied from its previous publication instead of building. */       \
    X(rich, semanticBytesCopied) /* Bytes a semantic build copied from reused blocks: display text, elements with their strings, paragraphs, spans, runs,      \
                                    headings, records and source formats. */                                                                                   \
    X(rich, semanticDisplayUnitsCopied)     /* Display UTF-16 units copied, with their spans and runs, from reused semantic blocks. */                         \
    X(rich, semanticElementsCopied)         /* Semantic elements copied (shifted) from reused blocks. */                                                       \
    X(rich, semanticFullBuildAbbreviations) /* Full semantic builds because abbreviation definitions exist or were edited. */                                  \
    X(rich, semanticFullBuildCapacity)      /* Full semantic builds because an object, record or paragraph admission bound could change. */                    \
    X(rich, semanticFullBuildFootnotes)     /* Full semantic builds because the document has footnotes (document-wide numbering and footer). */                \
    X(rich, semanticFullBuildFrontMatter)   /* Full semantic builds because the edit touched front matter. */                                                  \
    X(rich, semanticFullBuildNoPrevious)    /* Full semantic builds with no compatible previous publication (first build, other options or syntax). */         \
    X(rich, semanticFullBuildPresentation)  /* Full semantic builds because a source block, source region or native fallback presentation is active. */        \
    X(rich, semanticFullBuildReferences)    /* Full semantic builds because the edit touched a link reference definition. */                                   \
    X(rich, semanticFullBuildStructure)     /* Full semantic builds because a reused block's syntax nodes or identities did not pair. */                       \
    X(rich, semanticIncrementalBuilds)      /* Semantic builds that reused the previous publication's unchanged top-level blocks. */                           \
    X(rich, svgRasterizations)              /* SVG strips rasterized into image bitmaps (beside the testRichSvgRasterizations seam). */                        \
    X(rich, viewportPublications)           /* v0 document publications of the viewport's new chunks before the rest of a preparation is laid (W7.3). */       \
    X(rich, wholeLayoutsLaid)               /* Whole-document text layouts a document-layout preparation built (not reused). */

// UI Automation.
#define DXUI_WORK_COUNTERS_ACCESSIBILITY(X)                                                                                                                    \
    X(accessibility, chunksCloned)                 /* Record-spine chunks a successor generation cloned to replace a record in them (W8.2). */                 \
    X(accessibility, geometryMarksCollapsed)       /* Times geometry marks filled their capacity and became one mark on their common ancestor (W8.2). */       \
    X(accessibility, geometryPublicationsDeferred) /* Geometry-only publications held for the 100 ms interval or the layout's settle (W8.2). */                \
    X(accessibility, incrementalPublications) /* Publications that shared the previous generation's records instead of building the whole window (W8.2). */    \
    X(accessibility, marksWhileDormant)       /* Marks on a dirty native window no client listens to, noted without a refresh (W8.2). */                       \
    X(accessibility, mutations)        /* Accessibility mutations controls raised (RefreshWindowHostAccessibilitySnapshot with a host), built or not. */       \
    X(accessibility, pointHitRebuilds) /* Successor generations that rebuilt every bounding rectangle for a marked geometry change (W8.2). */                  \
    X(accessibility, recordsCloned)    /* Records a successor generation re-read from their controls; every other record is shared (W8.2). */                  \
    X(accessibility, recordsMarked)    /* Record marks noted for the next publication (name, value, focus or geometry; W8.2). */                               \
    X(accessibility, snapshotBuilds)   /* Whole-window UI Automation snapshot builds that reached the tree walk. */                                            \
    X(accessibility, snapshotBytes)    /* Bytes reserved by those snapshots' per-control record arrays (excluding owned strings). */                           \
    X(accessibility, snapshotNodes)    /* Control records those snapshots hold. */                                                                             \
    X(accessibility, spineBytes)       /* Bytes a successor generation copied: its record spine and the chunks it cloned (W8.2). */                            \
    X(accessibility, undeclaredInvalidations) /* Invalidations of a published host that declared nothing for UI Automation: the next publication is whole. */  \
    X(accessibility, wordUnitsVisited)        /* UTF-16 units text-range word movement stepped over, the enclosing-word scan from offset 0 included. */

// Memory: counted memory work; byte metrics live in receipts, not here.
#define DXUI_WORK_COUNTERS_MEMORY(X)                                                                                                                           \
    X(memory, controlHeapSizeReads) /* HeapSize calls control object allocation made, one per block allocated while an allocation sink was set. */

// Intrinsic layout: Control::Measure and its measure cache.
#define DXUI_WORK_COUNTERS_LAYOUT(X)                                                                                                                           \
    X(layout, arrangedChildren)             /* Child SetBounds calls an intrinsic owner's arrangement made (FlexPanel, FormPanel, FormField). */               \
    X(layout, measureCacheEvictions)        /* Measure-cache misses that displaced a slot valid for the control's current revision and context. */             \
    X(layout, measureCacheHits)             /* Control::Measure calls answered from the control's measure cache, MeasureOverride not run. */                   \
    X(layout, measureCacheMisses)           /* Control::Measure calls with valid input that found no cached result and ran MeasureOverride. */                 \
    X(layout, measureCacheVerifyMismatches) /* Debug verification: a cache hit whose re-run MeasureOverride returned a different result. */                    \
    X(layout, measureInvalidations)         /* Control::InvalidateMeasure calls: one per control whose measured size may have changed. */                      \
    X(layout, measureOverrideExecutions)    /* MeasureOverride calls Control::Measure made for a miss (not verification re-runs). */

// Every counter of every group except the resource kinds.
#define DXUI_WORK_COUNTER_FIELDS(X)                                                                                                                            \
    DXUI_WORK_COUNTERS_FRAME(X)                                                                                                                                \
    DXUI_WORK_COUNTERS_WAKES(X)                                                                                                                                \
    DXUI_WORK_COUNTERS_TEXT(X)                                                                                                                                 \
    DXUI_WORK_COUNTERS_POPUPS(X)                                                                                                                               \
    DXUI_WORK_COUNTERS_LISTS(X)                                                                                                                                \
    DXUI_WORK_COUNTERS_RICH(X)                                                                                                                                 \
    DXUI_WORK_COUNTERS_ACCESSIBILITY(X)                                                                                                                        \
    DXUI_WORK_COUNTERS_MEMORY(X)                                                                                                                               \
    DXUI_WORK_COUNTERS_LAYOUT(X)

// Native resources created through the counted helpers of Support/CountedCreation.h, one tally each. Every kind counts
// creations; destructions are observed only for objects created while a test's ResourceLifetimeTracker is installed. An
// implicit text layout (DrawText lays out its whole string on every call) is created and destroyed inside the call.
#define DXUI_WORK_RESOURCE_KINDS(X)                                                                                                                            \
    X(bitmap)                                                                                                                                                  \
    X(brush)                                                                                                                                                   \
    X(commandList)                                                                                                                                             \
    X(d3d11Device)                                                                                                                                             \
    X(deviceContext)                                                                                                                                           \
    X(effect)                                                                                                                                                  \
    X(geometry)                                                                                                                                                \
    X(gradientStops)                                                                                                                                           \
    X(implicitTextLayout)                                                                                                                                      \
    X(layer)                                                                                                                                                   \
    X(renderingParams)                                                                                                                                         \
    X(strokeStyle)                                                                                                                                             \
    X(textFormat)                                                                                                                                              \
    X(textLayout)

namespace DxUi::Detail
{
#define DXUI_WORK_DECLARE_FIELD(group, name) uint64_t name = 0u;
struct FrameWork final
{
    DXUI_WORK_COUNTERS_FRAME(DXUI_WORK_DECLARE_FIELD)
};
struct WakeWork final
{
    DXUI_WORK_COUNTERS_WAKES(DXUI_WORK_DECLARE_FIELD)
};
struct TextWork final
{
    DXUI_WORK_COUNTERS_TEXT(DXUI_WORK_DECLARE_FIELD)
};
struct PopupWork final
{
    DXUI_WORK_COUNTERS_POPUPS(DXUI_WORK_DECLARE_FIELD)
};
struct ListWork final
{
    DXUI_WORK_COUNTERS_LISTS(DXUI_WORK_DECLARE_FIELD)
};
struct RichWork final
{
    DXUI_WORK_COUNTERS_RICH(DXUI_WORK_DECLARE_FIELD)
};
struct AccessibilityWork final
{
    DXUI_WORK_COUNTERS_ACCESSIBILITY(DXUI_WORK_DECLARE_FIELD)
};
struct MemoryWork final
{
    DXUI_WORK_COUNTERS_MEMORY(DXUI_WORK_DECLARE_FIELD)
};
struct LayoutWork final
{
    DXUI_WORK_COUNTERS_LAYOUT(DXUI_WORK_DECLARE_FIELD)
};
#undef DXUI_WORK_DECLARE_FIELD

struct ResourceTally final
{
    uint64_t created   = 0u;
    uint64_t destroyed = 0u;
};

enum class ResourceKind : uint8_t
{
#define DXUI_WORK_DECLARE_KIND(kind) kind,
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_DECLARE_KIND)
#undef DXUI_WORK_DECLARE_KIND
    Count,
};

struct ResourceWork final
{
#define DXUI_WORK_DECLARE_TALLY(kind) ResourceTally kind;
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_DECLARE_TALLY)
#undef DXUI_WORK_DECLARE_TALLY
};

struct WorkCounters final
{
    FrameWork frame;
    WakeWork wakes;
    ResourceWork resources;
    TextWork text;
    PopupWork popups;
    ListWork lists;
    RichWork rich;
    AccessibilityWork accessibility;
    MemoryWork memory;
    LayoutWork layout;
};

// The calling thread's record. Constant-initialized, so reading it never runs a thread-local constructor.
inline thread_local constinit WorkCounters g_workCounters{};

[[nodiscard]] inline WorkCounters& Work() noexcept
{
    return g_workCounters;
}

[[nodiscard]] constexpr ResourceTally& Tally(ResourceWork& resources, ResourceKind kind) noexcept
{
    switch (kind)
    {
#define DXUI_WORK_TALLY_CASE(name)                                                                                                                             \
    case ResourceKind::name: return resources.name;
        DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_TALLY_CASE)
#undef DXUI_WORK_TALLY_CASE
        case ResourceKind::Count:
        default: break;
    }
    return resources.textLayout;
}

[[nodiscard]] constexpr const ResourceTally& Tally(const ResourceWork& resources, ResourceKind kind) noexcept
{
    return Tally(const_cast<ResourceWork&>(resources), kind);
}

// Calls visit(group, name, value) for every counter in declaration order; a resource kind reports "<kind>.created" and
// "<kind>.destroyed" in the "resources" group. Names are string literals.
template <typename Visitor> void ForEachWorkCounter(const WorkCounters& counters, Visitor&& visit)
{
#define DXUI_WORK_VISIT_FIELD(group, name) visit(#group, #name, counters.group.name);
    DXUI_WORK_COUNTERS_FRAME(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_WAKES(DXUI_WORK_VISIT_FIELD)
#define DXUI_WORK_VISIT_KIND(kind)                                                                                                                             \
    visit("resources", #kind ".created", counters.resources.kind.created);                                                                                     \
    visit("resources", #kind ".destroyed", counters.resources.kind.destroyed);
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_VISIT_KIND)
#undef DXUI_WORK_VISIT_KIND
    DXUI_WORK_COUNTERS_TEXT(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_POPUPS(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_LISTS(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_RICH(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_ACCESSIBILITY(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_MEMORY(DXUI_WORK_VISIT_FIELD)
    DXUI_WORK_COUNTERS_LAYOUT(DXUI_WORK_VISIT_FIELD)
#undef DXUI_WORK_VISIT_FIELD
}

// The work done between two readings of the same thread's record: later minus earlier, counter by counter.
[[nodiscard]] constexpr WorkCounters Subtract(const WorkCounters& later, const WorkCounters& earlier) noexcept
{
    WorkCounters delta{};
#define DXUI_WORK_SUBTRACT_FIELD(group, name) delta.group.name = later.group.name - earlier.group.name;
    DXUI_WORK_COUNTER_FIELDS(DXUI_WORK_SUBTRACT_FIELD)
#undef DXUI_WORK_SUBTRACT_FIELD
#define DXUI_WORK_SUBTRACT_KIND(kind)                                                                                                                          \
    delta.resources.kind.created   = later.resources.kind.created - earlier.resources.kind.created;                                                            \
    delta.resources.kind.destroyed = later.resources.kind.destroyed - earlier.resources.kind.destroyed;
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_SUBTRACT_KIND)
#undef DXUI_WORK_SUBTRACT_KIND
    return delta;
}

// Resources created, over every kind.
[[nodiscard]] constexpr uint64_t TotalCreated(const WorkCounters& counters) noexcept
{
    uint64_t total = 0u;
#define DXUI_WORK_ADD_CREATED(kind) total += counters.resources.kind.created;
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_ADD_CREATED)
#undef DXUI_WORK_ADD_CREATED
    return total;
}

// Churn of a delta: per kind, the objects both created and destroyed in it (min(created, destroyed)), summed over kinds.
// Meaningful only while a ResourceLifetimeTracker observes destructions.
[[nodiscard]] constexpr uint64_t Churn(const WorkCounters& delta) noexcept
{
    uint64_t total = 0u;
#define DXUI_WORK_ADD_CHURN(kind)                                                                                                                              \
    total += delta.resources.kind.created < delta.resources.kind.destroyed ? delta.resources.kind.created : delta.resources.kind.destroyed;
    DXUI_WORK_RESOURCE_KINDS(DXUI_WORK_ADD_CHURN)
#undef DXUI_WORK_ADD_CHURN
    return total;
}

// A test's view of the work one action does on this thread: construct it, run the action, read Delta().
class WorkCounterScope final
{
public:
    WorkCounterScope() noexcept : _start(Work())
    {
    }
    [[nodiscard]] WorkCounters Delta() const noexcept
    {
        return Subtract(Work(), _start);
    }
    void Restart() noexcept
    {
        _start = Work();
    }

private:
    WorkCounters _start;
};

struct TrackedResource final
{
    IUnknown* object  = nullptr;
    ResourceKind kind = ResourceKind::Count;
};

class ResourceLifetimeTracker;
inline thread_local constinit ResourceLifetimeTracker* g_resourceLifetimeTracker = nullptr;

// Test-only observation of destructions. While installed on a thread, it holds one extra reference to every resource the
// counted helpers create on that thread, in storage the test supplies (no allocation). Sweep() drops each held reference:
// an object whose count reaches zero was released by its owners since creation and is counted destroyed; any other keeps
// its reference. Destruction therefore happens at the sweep rather than when the owner releases it; nothing else changes.
// Objects beyond the storage are counted as overflow and not tracked. Destroying the tracker drops every held reference
// without counting and restores the previous tracker.
class ResourceLifetimeTracker final
{
public:
    explicit ResourceLifetimeTracker(std::span<TrackedResource> storage) noexcept : _storage(storage), _previous(g_resourceLifetimeTracker)
    {
        g_resourceLifetimeTracker = this;
    }
    ResourceLifetimeTracker(const ResourceLifetimeTracker&)            = delete;
    ResourceLifetimeTracker& operator=(const ResourceLifetimeTracker&) = delete;
    ResourceLifetimeTracker(ResourceLifetimeTracker&&)                 = delete;
    ResourceLifetimeTracker& operator=(ResourceLifetimeTracker&&)      = delete;
    ~ResourceLifetimeTracker()
    {
        if (g_resourceLifetimeTracker == this)
            g_resourceLifetimeTracker = _previous;
        for (size_t index = 0u; index < _held; ++index)
            _storage[index].object->Release();
        _held = 0u;
    }

    void Track(ResourceKind kind, IUnknown* object) noexcept
    {
        if (! object)
            return;
        if (_held == _storage.size())
        {
            ++_overflow;
            return;
        }
        object->AddRef();
        _storage[_held++] = TrackedResource{object, kind};
    }

    // Counts and releases every tracked object its owners have released; returns how many were destroyed.
    size_t Sweep() noexcept
    {
        size_t kept      = 0u;
        size_t destroyed = 0u;
        for (size_t index = 0u; index < _held; ++index)
        {
            const TrackedResource entry = _storage[index];
            if (entry.object->Release() == 0u)
            {
                ++Tally(Work().resources, entry.kind).destroyed;
                ++destroyed;
                continue;
            }
            entry.object->AddRef();
            _storage[kept++] = entry;
        }
        _held = kept;
        return destroyed;
    }

    [[nodiscard]] size_t Held() const noexcept
    {
        return _held;
    }
    [[nodiscard]] uint64_t Overflow() const noexcept
    {
        return _overflow;
    }

private:
    std::span<TrackedResource> _storage;
    ResourceLifetimeTracker* _previous = nullptr;
    size_t _held                       = 0u;
    uint64_t _overflow                 = 0u;
};

// A host or container dispatched Paint to one control it had already found visible.
inline void NotePaintDispatch() noexcept
{
    auto& frame = Work().frame;
    ++frame.treeVisitsPaint;
    ++frame.paintExecutions;
}

// Counts one created resource and, under a tracker, holds it for destruction counting.
inline void NoteResourceCreated(ResourceKind kind, IUnknown* object) noexcept
{
    if (! object)
        return;
    ++Tally(Work().resources, kind).created;
    if (ResourceLifetimeTracker* const tracker = g_resourceLifetimeTracker)
        tracker->Track(kind, object);
}

// One implicit text layout, created and released inside a single DrawText call.
inline void NoteImplicitTextLayout() noexcept
{
    auto& tally = Work().resources.implicitTextLayout;
    ++tally.created;
    ++tally.destroyed;
}
} // namespace DxUi::Detail
