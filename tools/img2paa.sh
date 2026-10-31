#!/usr/bin/env bash
# Convert PNG/TGA textures to PAA using DayZ Tools' ImageToPAA.exe under GE-Proton.
# Uses its own Proton prefix so the DayZ Tools prefix in compatdata/ is never touched.
#
#   tools/img2paa.sh SRC.png [SRC2.png ...] DEST_DIR
#
# ImageToPAA picks the compression from the filename suffix (_co, _nohq, _smdi, ...).
set -euo pipefail

STEAM_ROOT="${STEAM_ROOT:-$HOME/.steam/debian-installation}"
PROTON="${PROTON:-$STEAM_ROOT/compatibilitytools.d/GE-Proton10-32/proton}"
IMAGETOPAA="$STEAM_ROOT/steamapps/common/DayZ Tools/Bin/ImageToPAA/ImageToPAA.exe"
PREFIX="${IMG2PAA_PREFIX:-$HOME/.local/share/dayz-img2paa-prefix}"

if [ "$#" -lt 2 ]; then
    echo "usage: $0 SRC.png [SRC2.png ...] DEST_DIR" >&2
    exit 1
fi
dest="${*: -1}"
mkdir -p "$dest" "$PREFIX"
dest="$(realpath "$dest")"

for src in "${@:1:$#-1}"; do
    src="$(realpath "$src")"
    name="$(basename "${src%.*}")"
    out="$dest/$name.paa"
    rm -f "$out"
    # Always pass both arguments: with none, ImageToPAA opens a blocking usage dialog.
    STEAM_COMPAT_DATA_PATH="$PREFIX" STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_ROOT" \
        timeout 300 "$PROTON" run "$IMAGETOPAA" "Z:$src" "Z:$out" >/dev/null 2>&1 || true
    if [ ! -s "$out" ]; then
        echo "FAILED: $src" >&2
        exit 1
    fi
    echo "$out"
done
