- The `W4.0` spike of retained command lists closes as a no-go, decided by the developer on 23 November 2026: hosts keep
  running `Paint` for every visible control, and the performance contract now says so and why. Replaying a control's
  recorded `ID2D1CommandList` with `DrawImage` cost 0.85 to 1.07 of painting it on hardware (`Label`, `Button`,
  `CardPanel` at 4K), against the required half, because Direct2D submits every recorded primitive again; FX5 frames were
  not faster, or 7 to 8% slower with every leaf control retained. On the window host `DrawImage` of a list also draws
  ClearType text grayscale. Not drawing an unchanged control costs 0.04 to 0.09 of painting it and a bitmap tile 0.24 to
  0.53, which is what plan item `W4.10` (region invalidation, culling and tiles for effect-heavy content) builds on. No
  library code changes; the prototype, its oracle scripts and its per-control benchmark stay on a branch of their own. The
  receipt is `Measurements/PerformanceAndMotion_2026-11-13/W4.0/`.
