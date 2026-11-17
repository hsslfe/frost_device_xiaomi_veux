#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

check_only=false
if [[ ${1:-} == --check ]]; then
    check_only=true
    shift
fi
if [[ $# -gt 1 ]]; then
    echo "Usage: $0 [--check] [ANDROID_SOURCE_ROOT]" >&2
    exit 2
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_root=${1:-$(cd -- "$script_dir/../../../.." && pwd)}
av_dir="$source_root/frameworks/av"
patch_file="$script_dir/../patches/torch-strength/frameworks_av.patch"

if [[ ! -f "$av_dir/services/camera/libcameraservice/common/CameraProviderExtension.h" ]]; then
    echo "Expected the Lunaris frameworks/av checkout at: $av_dir" >&2
    exit 2
fi

if git -C "$av_dir" apply --reverse --check "$patch_file" 2>/dev/null; then
    echo "Torch strength framework patch is already applied."
elif git -C "$av_dir" apply --check "$patch_file"; then
    if "$check_only"; then
        echo "Torch strength framework patch is missing; run without --check." >&2
        exit 1
    fi
    git -C "$av_dir" apply "$patch_file"
    echo "Applied the torch strength framework patch."
else
    echo "Framework patch conflicts. No files changed; review frameworks/av before building." >&2
    exit 2
fi
