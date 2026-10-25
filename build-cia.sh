#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# makerom 0.19.0 has a title-version macro collision (Project_CTR issue #169).
# Accept explicit paths or tool names resolved through PATH.
MAKEROM=${MAKEROM:-makerom}
BANNERTOOL=${BANNERTOOL:-bannertool}
export MAKEROM BANNERTOOL
for tool in "$MAKEROM" "$BANNERTOOL"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf '缺少打包工具：%s。请参阅 README 中的 CIA 编译说明。\n' "$tool" >&2
        exit 1
    fi
done
makerom_help=$("$MAKEROM" -help 2>&1 || true)
case "$makerom_help" in
    *"v0.19.0"*)
        printf '%s\n' 'makerom 0.19.0 存在版本编码缺陷，请使用 0.18.4 或已修复版本，并设置 MAKEROM 路径。' >&2
        exit 1
        ;;
esac

# Rebuild the same ELF used by the .3dsx version; do not strip debug symbols.
"$project_dir/build.sh"
cd "$project_dir"
mkdir -p build-cia
"$BANNERTOOL" makesmdh -s '涂鸦跃界' \
    -l 'Doodle Hop 3DS - Touch arcade' -p 'Xiaoguai' \
    -i packaging/icon.png -r regionfree -f visible,nosavebackups \
    -o build-cia/icon.smdh
"$BANNERTOOL" makebanner -i packaging/banner.png -a packaging/silence.wav \
    -o build-cia/banner.bnr
"$MAKEROM" -f cia -target t -rsf packaging/app.rsf \
    -elf doodle-hop-3ds.elf -icon build-cia/icon.smdh -banner build-cia/banner.bnr \
    -ver 1072 -o build-cia/doodle-hop-3ds.cia
mv build-cia/doodle-hop-3ds.cia doodle-hop-3ds.cia
cp build/doodle-hop-3ds.map doodle-hop-3ds.map
printf '%s\n' '已生成 doodle-hop-3ds.cia（版本 1.3.0），Title ID 见 packaging/app.rsf。'
