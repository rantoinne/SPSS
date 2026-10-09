# UI Reference (Internal)

**Target release:** IBM SPSS Statistics **30.x** (macOS Apple Silicon)  
**Edition scope:** Base first (Phases 0–5), then Complete modules via capability registry  
**Fidelity bar:** Layout, menus, dialog structure, Data/Variable View, Syntax Editor, Output Viewer, dialog→Paste.  
**Legal:** Original branding only — do **not** copy IBM trademarks, icons, splash screens, or proprietary assets.

## Primary windows (wireframe map)

| Surface | Layout notes |
|---------|----------------|
| Data Editor | Tabs: Data View \| Variable View; status bar: Filter / Weight / Split |
| Variable View columns | Name, Type, Width, Decimals, Label, Values, Missing, Columns, Align, Measure, Role |
| Syntax Editor | Line numbers, run all / selection, error markers, toolbar |
| Output Viewer | Left outline tree + right content; Notes / Warnings / Tables / Charts |
| Chart Builder | Gallery + drop zones |
| Procedure dialogs | Button order: **OK · Paste · Reset · Cancel · Help** |

## Menu inventory (SPSS 30 Base)

- **File** — New, Open, Save, Import (CSV/Excel/SAV), Export, Print, Recent
- **Edit** — Undo, Redo, Cut, Copy, Paste, Clear, Find, Select All
- **View** — Status Bar, Value Labels, Grid Lines, Fonts
- **Data** — Define Variable Properties, Sort, Transpose, Merge, Aggregate, Split File, Select Cases, Weight Cases, Restructure
- **Transform** — Compute, Recode, Automatic Recode, Visual Binning, Rank Cases, Date/Time
- **Analyze** — Descriptive Statistics, Compare Means, Correlate, Regression, Nonparametric, …
- **Graphs** — Chart Builder, Legacy Dialogs
- **Utilities** — Variables, OMS Control Panel, Run Script, Command History
- **Window** — Minimize, Zoom, Cycle Windows
- **Help** — Topics, About (original product name)

## Format fidelity

| Format | Target |
|--------|--------|
| `.sav` | Labels, missing, formats round-trip (MVP) |
| `.zsav` | Lossy deferred / stub |
| `.por` | Lossy deferred / stub |
| `.sps` | Lossless text; Base command subset executed |
| Output docs | Native document + PDF/HTML/PNG export MVP |

## Side-by-side checklist

Validate UI against SPSS 30 on Mac for each primary window — not only unit tests. Capture annotated screenshots here when available (internal only).
