# UI Style Guide — SPSS-like Structure (Original Branding)

Product name: **StatDesk** (working title; not an IBM mark).

## Chrome

- Document windows: standard macOS title bar; multiple datasets, syntax docs, one Output Viewer.
- Active dataset name shown in window title / toolbar indicator.
- Toolbar icons: original SF Symbols / custom assets only.

## Dialogs

Button order (left→right, trailing actions on macOS): **OK**, **Paste**, **Reset**, **Cancel**, **Help**.  
Mirror SPSS field order and subcommand tabs; generate valid syntax on Paste.

## Data View

- Virtualized case × variable grid; row numbers; frozen panes.
- Toggle value labels; show missing as system-missing / user-missing.
- Status bar chips: Filter On | Weight On | Split File.

## Variable View

Column order fixed:

1. Name 2. Type 3. Width 4. Decimals 5. Label  
6. Values 7. Missing 8. Columns 9. Align 10. Measure 11. Role

## Typography

- UI: system San Francisco for chrome.
- Data grid: monospaced figures preferred for numeric cells.
- Syntax: monospaced editor font with SPSS-like keyword highlighting.

## Output Viewer

- Outline: Log → Notes → Warnings → Tables → Charts (hierarchical).
- Pivot values stored separately from display strings; footnotes supported.
