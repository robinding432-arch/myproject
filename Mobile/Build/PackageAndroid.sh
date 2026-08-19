#!/bin/bash
# PackageAndroid.sh
# v7.2 — Package StellarSystem for Android (ARM64)

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$SCRIPT_DIR")")")"
PROJECT_FILE="$PROJECT_ROOT/StellarSystem.uproject"
UAT_PATH="${UE_ROOT:-/home/ue/UnrealEngine}/Engine/Build/BatchFiles/RunUAT.sh"

echo "=========================================="
echo "  StellarSystem v7.2 — Android Packager"
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
    exit 1
fi

# Default config
CONFIG="${1:-Shipping}"
TARGET="${2:-StellarSystemAndroid}"

echo "Configuration: $CONFIG"
echo "Target:        $TARGET"
echo ""

# Package command
"$UAT_PATH" BuildGame \
    -project="$PROJECT_FILE" \
    -targetplatform=Android \
    -configuration="$CONFIG" \
    -target="$TARGET" \
    -cookflavor=ARM64 \
    -pak \
    -compressed \
    -distribution \
    -nodebuginfo

echo ""
echo "=========================================="
echo "  Android packaging complete!"
echo "  Output: Binaries/Android/StellarSystem-$CONFIG-arm64.apk"
echo "=========================================="
