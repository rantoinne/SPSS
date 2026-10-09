#!/usr/bin/env bash
# Build and launch the StatDesk macOS GUI.
# Requires full Xcode (SwiftUI macros). Tested with Xcode 27.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! xcodebuild -version >/dev/null 2>&1; then
  echo "Full Xcode is required to build the StatDesk GUI."
  echo "Install Xcode, then: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer"
  exit 1
fi

echo "Using: $(xcodebuild -version | tr '\n' ' ')"
echo "Building StatDesk (SwiftPM executable with @main SwiftUI app)…"
swift build --product StatDesk -c debug

BIN="$(swift build --product StatDesk -c debug --show-bin-path)/StatDesk"
APP="$ROOT/.build/StatDesk.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BIN" "$APP/Contents/MacOS/StatDesk"
cp "$ROOT/apps/SPSSMac/Info.plist" "$APP/Contents/Info.plist"
# Minimal package structure for local run (ad-hoc sign)
/usr/bin/plutil -replace CFBundleExecutable -string StatDesk "$APP/Contents/Info.plist" 2>/dev/null || true
if ! /usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$APP/Contents/Info.plist" >/dev/null 2>&1; then
  /usr/libexec/PlistBuddy -c "Add :CFBundleExecutable string StatDesk" "$APP/Contents/Info.plist"
fi
if ! /usr/libexec/PlistBuddy -c "Print :CFBundlePackageType" "$APP/Contents/Info.plist" >/dev/null 2>&1; then
  /usr/libexec/PlistBuddy -c "Add :CFBundlePackageType string APPL" "$APP/Contents/Info.plist"
fi

codesign --force --deep -s - "$APP" 2>/dev/null || true

echo ""
echo "Built: $APP"
echo "Open in Xcode (optional): open Package.swift"
echo "  then select scheme 'StatDesk' (or StatDeskApp) and Run."
echo ""
echo "Launching StatDesk.app…"
open "$APP"
