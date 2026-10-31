#!/usr/bin/env bash
# Stage the HandCart mod for packing: copies HandCart/ to build/HandCart/ with every .p3d
# binarized (ODOL, model.cfg baked in - the held-pose animations only exist after this).
# DayZMissions' tools/build_mod.py packs and signs build/HandCart as @HandCart
# (~/Projects/DayZMissions/mods/HandCart is a symlink to it).
#
#   tools/build.sh
#
# Binarize runs under GE-Proton in its own prefix (shared with img2paa.sh; never the DayZ
# Tools prefix in compatdata/). It scans its whole drive for addons, so the source gets a
# drive of its own: P: is a scratch copy of HandCart/, O: the output.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STEAM_ROOT="${STEAM_ROOT:-$HOME/.steam/debian-installation}"
GE="$(ls -d "$STEAM_ROOT"/compatibilitytools.d/GE-Proton* | sort -V | tail -n1)"
BINARIZE="$STEAM_ROOT/steamapps/common/DayZ Tools/Bin/Binarize/binarize.exe"
export WINEPREFIX="${IMG2PAA_PREFIX:-$HOME/.local/share/dayz-img2paa-prefix}/pfx"
export WINEDEBUG=-all

WORK="$ROOT/build/.binarize"
rm -rf "$WORK"
mkdir -p "$WORK/P" "$WORK/O"
cp -r "$ROOT/HandCart" "$WORK/P/"
ln -sfn "$WORK/P" "$WINEPREFIX/dosdevices/p:"
ln -sfn "$WORK/O" "$WINEPREFIX/dosdevices/o:"

cd "$WORK/P"
timeout 300 "$GE/files/bin/wine64" "$BINARIZE" -targetBonesInterval=56 -maxProcesses=1 -always \
	"-textures=O:\\tex" "P:\\HandCart" "O:\\HandCart" >"$WORK/binarize.log" 2>&1 || {
	echo "binarize failed, see $WORK/binarize.log" >&2
	exit 1
}

STAGE="$ROOT/build/HandCart"
# DayZMissions' workshop_publish.sh records a new Workshop item's id in the staged folder
# (mods/HandCart links here): keep it in the source so the next build still has it
if [ -f "$STAGE/workshop.id" ] && [ ! -f "$ROOT/HandCart/workshop.id" ]; then
	cp "$STAGE/workshop.id" "$ROOT/HandCart/workshop.id"
fi
rm -rf "$STAGE"
cp -r "$ROOT/HandCart" "$STAGE"
rm -f "$STAGE/model.cfg"   # baked into the binarized models
while IFS= read -r -d '' src; do
	rel="${src#"$ROOT/HandCart/"}"
	out="$WORK/O/HandCart/$rel"
	if [ "$(head -c 4 "$out" 2>/dev/null)" != ODOL ]; then
		echo "binarize produced no ODOL for $rel, see $WORK/binarize.log" >&2
		exit 1
	fi
	cp "$out" "$STAGE/$rel"
done < <(find "$ROOT/HandCart" -name '*.p3d' -print0)

echo "staged $STAGE ($(find "$STAGE" -name '*.p3d' | wc -l) models binarized)"
