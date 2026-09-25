# Grid

Virtualized multi-column data grid over a borrowed model.

- **Consumer supplies**: a borrowed `IDxGridModel`, optional `IDxGridDelegate`, columns, row height, selection; call `NotifyDataChanged` after model changes.
- **Metrics**: row 28, group header 28, header 32, icon 18; Compact density scales by 0.82. Sort glyph animates 140 ms.
- **Colors**: header `headerBackground`; lines `gridLine`; selected `selectionFill`/`selectionText`, unfocused `selectionInactiveFill`, hover `hoverFill`. Status badges pair `infoFill`/`infoText`, `warningFill`/`warningText`, `errorFill`/`errorText` — always with a word, never color alone.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
