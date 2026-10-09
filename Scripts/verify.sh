#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "== Toolchain =="
xcodebuild -version || true
swift --version

echo "== CMake engine =="
cmake -S engine -B engine/build -DCMAKE_BUILD_TYPE=Debug
cmake --build engine/build -j
./engine/build/spss_engine_tests
./engine/build/spss_cli descriptives parity/fixtures/sample.csv age score | head -c 200
echo ""

echo "== Swift package =="
swift build
swift run statdesk-smoke
swift run statdesk-cli descriptives parity/fixtures/sample.csv age score | head -c 200
echo ""
swift run statdesk-cli glm parity/fixtures/sample.csv score group | head -c 200
echo ""
swift run statdesk-cli syntax parity/fixtures/sample.csv parity/fixtures/sample.sps 2>&1 | head -n 40
echo ""
swift run statdesk-cli extensions || true

echo "== Swift tests (XCTest via Xcode toolchain) =="
swift test

echo "== GUI scheme (compile check) =="
if xcodebuild -version >/dev/null 2>&1; then
  swift build --product StatDesk
  echo "StatDesk GUI product built (run via Scripts/generate-xcode-app.sh or: swift run StatDesk)"
else
  echo "Skipping GUI build — full Xcode required"
fi

echo ""
echo "OK — engine + Swift CLI/smoke/tests verified"
echo "GUI: ./Scripts/generate-xcode-app.sh   or   open Package.swift in Xcode → scheme StatDesk"
