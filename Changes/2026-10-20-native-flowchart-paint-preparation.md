- Prepare Unicode flowchart labels through bounded DirectWrite calls and cache
  native shape/arrow resources before caller-owned raster drawing. Preserve
  immutable resources across failures and distinguish typography/resource epochs.
  Diagram objects and source editing remain separate editor integration work.
