#!/bin/bash
# PackageIOS.sh
# v7.2 — Package StellarSystem for iOS (requires macOS + Xcode)

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$SCRIPT_DIR")")")"
PROJECT_FILE="$PROJECT_ROOT/StellarSystem.uproject"
UAT_PATH="${UE_ROOT:-/Users/Shared/EpicGames/UE_5.3/Engine/Build/BatchFiles/Mac/RunUAT.sh}"

echo "=========================================="
echo "  StellarSystem v7.2 — iOS Packager"
echo "=========================================="
echo "Project: $PROJECT_FILE"
echo "UAT:     $UAT_PATH"
echo ""

if [ ! -f "$PROJECT_FILE" ]; then
    echo "ERROR: Project file not found: $PROJECT_FILE"
    exit 1
fi

if [ ! -f "$UAT_PATH" ]; then
    echo "ERROR: RunUAT.sh not found at $UAT_PATH"
    echo "Set UE_ROOT environment variable to your UE5 root."
    echo "iOS builds require macOS + Xcode."
    exit 1
fi

CONFIG="${1:-Shipping}"
TARGET="${2:-StellarSystemIOS}"

echo "Configuration: $CONFIG"
echo "Target:        $TARGET"
echo ""

# iOS packaging requires Mac
if [ "$(uname)" != "Darwin" ]; then
    echo "WARNING: iOS builds require macOS. This is $(uname)."
    echo "The generated Xcode project can be built on a Mac."
fi

"$UAT_PATH" BuildGame \
    -project="$PROJECT_FILE" \
    -targetplatform=IOS \
    -configuration="$CONFIG" \
    -target="$TARGET" \
    -pak \
    -compressed \
    -distribution

echo ""
echo "=========================================="
echo "  iOS packaging complete!"
echo "  Output: Binaries/IOS/StellarSystem-$CONFIG.ipa"
echo "=========================================="
