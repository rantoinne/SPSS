# Running StatDesk

Short companion to the root [README](../README.md).

## Quick path (Xcode 27 installed)

```bash
cd /path/to/SPSS
./Scripts/verify.sh                 # engine + swift build/test/smoke
./Scripts/generate-xcode-app.sh     # build & open StatDesk.app
```

## GUI only

```bash
swift run StatDesk
# or
open Package.swift   # scheme StatDesk → Run
```

## CLI only (no GUI)

```bash
cmake -S engine -B engine/build && cmake --build engine/build -j
swift build
swift run statdesk-cli syntax parity/fixtures/sample.csv parity/fixtures/sample.sps
```

## Python BEGIN PROGRAM example

```text
BEGIN PROGRAM PYTHON.
print("hello from StatDesk")
END PROGRAM.
```

Requires `python3` on PATH. Runs in a subprocess; does not share the in-memory dataset yet (integration surface for later hooks).
