#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# Prefer an explicit SDK, then the SDK prepared in this task, then system install.
local_sdk="$project_dir/../../work/toolchain/devkitpro"
if [ -z "${DEVKITPRO:-}" ]; then
    if [ -f "$local_sdk/devkitARM/3ds_rules" ]; then
        DEVKITPRO=$(CDPATH= cd -- "$local_sdk" && pwd)
    else
        DEVKITPRO=/opt/devkitpro
    fi
fi
DEVKITARM=${DEVKITARM:-"$DEVKITPRO/devkitARM"}
export DEVKITPRO DEVKITARM
export PATH="$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH"

if [ ! -f "$DEVKITARM/3ds_rules" ] || [ ! -f "$DEVKITPRO/libctru/lib/libctru.a" ]; then
    printf '%s\n' '缺少 devkitARM 或 libctru。请按 README 安装 devkitPro 的 3ds-dev 软件包。' >&2
    exit 1
fi
exec make -C "$project_dir" "$@"
