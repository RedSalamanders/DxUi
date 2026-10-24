# Separator

Noninteractive horizontal or vertical divider for separating related controls or content.

- **Consumer supplies**: `SetOrientation(SeparatorOrientation::Horizontal|Vertical)`; default is vertical.
- **Paint**: uses the theme grid line (system border in high contrast), with compact insets; it has no hit target, focus state, or invoke pattern.
- **Accessibility**: reports UI Automation Separator control type and remains nonfocusable.
- **Toolbar**: `Toolbar::AddSeparator()` creates this same catalogued control.

The preview is static; the native control paints through DxUi.lib.
