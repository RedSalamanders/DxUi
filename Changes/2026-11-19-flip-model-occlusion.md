- Measured what a flip-model `Present` returns on the Windows 11 baseline (RedHome register H-20, CV-6-08): on 25H2,
  flip-sequential and flip-discard swap chains returned `S_OK` while their window was shown, minimized, cloaked or
  hidden, and a blt-model control returned `DXGI_STATUS_OCCLUDED`. The window-hosting contract's statement that
  Windows does not return that status for flip-model swap chains now cites the record in
  `Measurements/FlipModelOcclusion_2026-11-19/`. Documentation and evidence only; behavior unchanged.
