- Corrected the flip-model occlusion record (RedHome register H-20, CV-6-08). A second probe run adds a window
  covered by a visible child and by another topmost window: a flip-model `Present` still returned `S_OK` in every
  state. RedXe's renderer records `DXGI_STATUS_OCCLUDED` arriving when one of its own native child windows covered
  its swap chain, which this probe did not reproduce. The contract now says the status is not a usable occlusion
  signal for flip-model swap chains, instead of saying Windows never returns it. Documentation and evidence only.
