# StatDesk

Independent Apple Silicon macOS statistical workstation with **IBM SPSS Statistics 30.x–like** workflow and UI structure (Data/Variable View, syntax Paste+Run, Output Viewer, Base + advanced MVP procedures). Original branding only — not affiliated with IBM.

**Honesty bar:** layout/menus/dialogs/Paste aim for structural familiarity. StatDesk does **not** claim numerical or pixel-perfect parity with SPSS, and does **not** read/write IBM proprietary `.sav` binary layouts (see [SAV format notes](#sav--io-limits)).

## Prerequisites

| Tool | Purpose |
|------|---------|
| **Xcode 27** (full app, not only CLT) | SwiftUI GUI, `swift test` / XCTest, SwiftUI macros |
| macOS 14+ | App + SwiftPM platform |
| CMake 3.20+ | C++ engine |
| Optional: `python3`, `Rscript` | `BEGIN PROGRAM` extensions |

```bash
# Point CLT at full Xcode if needed
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
xcodebuild -version   # expect Xcode 27.x
swift --version
cmake --version
```

## Layout

| Path | Role |
|------|------|
| `apps/SPSSMac/` | SwiftUI + AppKit shell (Data/Variable View, dialogs, Syntax, Output, Chart Builder) |
| `apps/StatDeskCLI/` | Headless CLI |
| `engine/` | C++ DatasetStore, transforms, procedures, CSV/SAV I/O, C API |
| `syntax/` | Parser, command registry, Paste generators, runtime |
| `parity/` | Feature matrix, fixtures, XCTest |
| `docs/ui-reference/` | Target release notes + style guide (internal) |
| `Scripts/verify.sh` | One-shot engine + Swift + tests |
| `Scripts/generate-xcode-app.sh` | Build/launch `.app` bundle |

## Decisions (frozen)

1. **Target:** SPSS Statistics **30.x** UI/command parity *reference*  
2. **Edition:** Base (Phases 0–5) plus gated advanced/Complete **MVP** modules (Phases 6–7)  
3. **Fidelity:** Layout/menus/dialogs/Paste — no IBM trademarks or assets  

## Build & run

### 1. C++ engine (CMake)

```bash
cd /Users/raviasthana/Documents/Personal/SPSS

cmake -S engine -B engine/build -DCMAKE_BUILD_TYPE=Release
cmake --build engine/build -j
./engine/build/spss_engine_tests
./engine/build/spss_cli descriptives parity/fixtures/sample.csv age score
```

### 2. Swift package (CLI + libraries + GUI product)

```bash
swift build
swift test
swift run statdesk-smoke
swift run statdesk-cli descriptives parity/fixtures/sample.csv age score
swift run statdesk-cli glm parity/fixtures/sample.csv score group
swift run statdesk-cli factor parity/fixtures/sample.csv age score
swift run statdesk-cli syntax parity/fixtures/sample.csv parity/fixtures/sample.sps
swift run statdesk-cli extensions
```

One-shot verification:

```bash
./Scripts/verify.sh
```

### 3. macOS GUI app

**Option A — script (recommended for local run):**

```bash
./Scripts/generate-xcode-app.sh
# Builds .build/StatDesk.app (ad-hoc signed) and opens it
```

**Option B — SwiftPM directly:**

```bash
swift run StatDesk
```

**Option C — Xcode:**

```bash
open Package.swift
# Select scheme: StatDesk (executable target StatDeskApp)
# Run (⌘R). Product sources: apps/SPSSMac (UI) + SPSSCore/SPSSSyntax.
```

`xcodebuild` example:

```bash
swift build --product StatDesk
# or open Package.swift and use Xcode’s scheme UI
```

Signing: local runs use ad-hoc `codesign -s -`. Notarized distribution is Phase 9 (not done).

## CLI usage examples

```bash
# Descriptives / frequencies
swift run statdesk-cli descriptives parity/fixtures/sample.csv age score
swift run statdesk-cli frequencies parity/fixtures/sample.csv sex

# Advanced MVP
swift run statdesk-cli glm parity/fixtures/sample.csv score group
swift run statdesk-cli cluster parity/fixtures/sample.csv 2 age score
swift run statdesk-cli logistic parity/fixtures/sample.csv sex age score
swift run statdesk-cli ctables parity/fixtures/sample.csv sex group

# Full syntax file
swift run statdesk-cli syntax parity/fixtures/sample.csv parity/fixtures/sample.sps

# Extension interpreter discovery
swift run statdesk-cli extensions
```

Sample syntax lives at `parity/fixtures/sample.sps`.

## Capability modules

Settings → Modules toggles editions (like SPSS). Implemented MVP commands unlock under:

- **Advanced:** UNIANOVA, FACTOR, QUICK CLUSTER, KM  
- **Regression:** LOGISTIC REGRESSION  
- **Custom Tables / Exact / Trees / Neural:** CTABLES, EXACT, TREE, NEURAL NETWORK  
- **Missing / Forecasting:** MULTIPLE IMPUTATION (diagnostics), TSMODEL (SMA)  
- **Extensions:** `BEGIN PROGRAM` / `BEGIN PROGRAM R` via isolated subprocess when `python3` / `Rscript` exist  

Still planned / stubbed: MIXED, Complex Samples, Categories, Conjoint, Direct Marketing, ODBC, Parquet/Arrow, notarization.

## SAV / IO limits

| Format | Behavior |
|--------|----------|
| CSV | Full read/write |
| Excel | Treated as CSV/TSV path (not OOXML) |
| `.sav` | **StatDesk portable** binary (`SPSSMAC1` magic) with labels/missing/formats round-trip **within StatDesk**. Not IBM SPSS binary-compatible. |
| `.zsav` / `.por` | Not implemented |
| IBM SPSS `.sav` from SPSS 30 | Will **not** open — use CSV export from SPSS for interchange |

## Parity & testing

- Matrix: `parity/matrices/parity-matrix.csv`  
- Fixtures: `parity/fixtures/`  
- Engine tests: `./engine/build/spss_engine_tests`  
- Swift: `swift test` + `swift run statdesk-smoke`  

Do not claim “complete SPSS compatibility” until the matrix status is `done` for that command set **and** differential goldens exist.

## Legal

Independent implementation. Do not copy IBM icons, splash screens, or trademarks. Visual language matches structure, not copyrighted chrome.
