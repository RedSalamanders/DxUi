# Rich editor presentation

Status: ACTIVE

The shared editor owns its frame, text viewport, typography, source highlights,
focus dimming and link decoration. Consumers provide policy through public
options and retain document, command and window chrome ownership.

- [ ] Retain a baseline from main `b69f452f` before implementation.
- [ ] Add validated appearance options, source highlights and focus dimming.
- [ ] Add opt-in hover/caret link underline policy with default compatibility,
  high-contrast override and source-current UI Automation attributes.
- [ ] Cover source/history invariance, native and embedded geometry, interaction,
  layout invalidation, high contrast and detached/hidden state.
- [ ] Update domain contracts, usage/design-system documentation and gallery.
- [ ] Run scoped validation and profile builds; record remaining qualification
  gates without treating focused checks as full repository acceptance.

The existing RedTilde pin `dd619ef0c5855307eb57758072658f2e3b777fd8` and current
main diverge from `f65e4a4b`; public headers are identical at both tips. The
candidate starts from main to retain newer correctness and tooling changes.
The consumer must explicitly adopt the tested candidate commit.
