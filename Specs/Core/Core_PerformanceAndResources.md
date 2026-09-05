# Performance and resources

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

The offscreen approach has a real memory and dirty-paint cost compared with the AV RFC's original custom-instancing
idea. The choice is a reuse proposal subject to measurement, not an already approved resource regression. Record
both preparation and final composition; a single composite draw does not make the entire UI a one-draw renderer.

| Resource / scenario | Proposed acceptance rule |
| --- | --- |
| Static visible, hidden, minimized, occluded, display-off | No embedded-owned periodic timer, worker, polling, preparation, repaint or present. Host visibility rules control all work. |
| Clean composition | Zero application heap allocations, text shaping, target creation, D2D repaint or texture upload; at most one composite draw per layout viewport and one bounded geometry upload only when its transform changes. |
| Dirty updates | Coalesce to the latest bounded snapshot; measure CPU time, D2D/GPU submissions, allocations and bytes separately. No allocation growth over repeated changes. |
| Texture count | At most two resident layout surfaces per AV instance; identical layouts share. Replacement may temporarily retain one old/new pair per changed surface until publication, then releases the old one. |
| Surface memory | Compute from actual physical extents and format. One 1280×720 BGRA surface is 3,686,400 bytes (3.52 MiB); two are 7.03 MiB before driver overhead. At 2× raster dimensions, two are 28.13 MiB. Include replacement peaks rather than hiding them. |
| Provisional safety caps | 64 MiB resident surface payload and 128 MiB replacement peak per instance; 256 KiB composite dynamic buffers per instance. These are ceilings, not preallocations or normal targets. Checked arithmetic, device dimension limits and host admission must reject excess explicitly. |
| Shared resources | One device-generation pool in the AV module; initial shared cache target 8 MiB, with D2D/DWrite/driver retained overhead reported separately. Cache residency and multi-instance totals must be measured. |
| Responsive input | Pending visual acknowledgement within AV's 100 ms target; measure p50/p95 preparation/frame latency during drag and text entry on a named hardware fixture and WARP. Report results, not invented timings. |
| Multiple widgets / recovery | Test the host-supported instance bound, staged pages, repeated raise/dismiss, DPI and device loss; no proportional growth in devices, workers or immutable caches. |

At native resolution, record the total active-page plus overlay texture footprint as well as per-instance numbers.
Test 96/144/192 DPI and all AV sizes, including portrait; do not mistake logical pixels for texture pixels. Allocation
counters distinguish library-controlled work from OS/driver internals, which remain measured even when not directly
bounded by the application allocator. If the prototype misses acceptable resource or latency targets, optimize the
dirty/caching path or revise the rendering adapter with measured justification; keep the shared control model and
do not silently fork controls into AV.

### Single-library implementation budgets

EmbeddedHost has one cached surface (64 MiB maximum, 128 MiB transactional replacement peak) and shares immutable
composition state, D2D device and DWrite factory through GraphicsDevice. A consumer using tile/raised views admits
their summed surface cost. Composite is a single triangle with shader constants derived from SV_VertexID and needs
no per-view vertex/index/dynamic constant buffer. Hidden state performs no timer subscription; native WindowHost
alone uses the event-driven animation dispatcher. Diagnostics are borrowed and optional; composition does not emit
them. The private window-message payload registry is bounded to 128 windows and 128 queued payloads; saturation
fails immediately and releases transferred ownership. Teardown invalidates queued tokens and drains outside its lock.
