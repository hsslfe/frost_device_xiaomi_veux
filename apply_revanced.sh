#!/usr/bin/env bash

set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../../.. && pwd)"
cd "$ROOT"

echo "========================================"
echo "Applying ReVanced patches..."
echo "========================================"

apply_patch() {
    local repo="$1"
    local patch="$ROOT/$2"

    echo
    echo "[$repo]"

    # Skip if already applied
    if git -C "$repo" apply --reverse --check "$patch" >/dev/null 2>&1; then
        echo "Already applied, skipping."
        return
    fi

    # Apply patch
    if git -C "$repo" apply --check "$patch"; then
        git -C "$repo" apply "$patch"
        echo "Patch applied successfully."
    else
        echo "Error: Failed to apply patch. Stopping."
        exit 1
    fi
}

apply_patch frameworks/base \
    device/xiaomi/veux/patches/0001-frameworks-base-Revanced-hide.patch

apply_patch packages/apps/Settings \
    device/xiaomi/veux/patches/0002-settings-Revanced-toggle.patch

apply_patch system/sepolicy \
    device/xiaomi/veux/patches/0003-sepolicy-Revanced-property.patch

echo
echo "========================================"
echo "ReVanced patches applied successfully!"
echo "========================================"
