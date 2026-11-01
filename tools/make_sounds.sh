#!/usr/bin/env bash
# Cut the hand truck's sounds (HandCart/data/sounds/*.ogg, mono 44.1 kHz Vorbis) from two
# Pixabay downloads (Pixabay Content License: free to use in a mod, but not to pass on
# unaltered, so the originals aren't in the repo). Put them in sounds_src/:
#   freesound_community-gravel-road-6747.mp3   (Pixabay sound effect 6747, "gravel road")
#   dragon-studio-groaning-metal-511322.mp3    (Pixabay sound effect 511322, "groaning metal")
#
#   tools/make_sounds.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/sounds_src"
OUT="$ROOT/HandCart/data/sounds"
G="$SRC/freesound_community-gravel-road-6747.mp3"
M="$SRC/dragon-studio-groaning-metal-511322.mp3"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$OUT"
q() { ffmpeg -hide_banner -loglevel error -y "$@"; }

# Gravel crunching, looped: 0.1-7.2 s with the last 0.4 s crossfaded into the start
q -i "$G" -af "aresample=44100,pan=mono|c0=0.5*c0+0.5*c1,atrim=0.1:7.2" "$TMP/g.wav"
q -i "$TMP/g.wav" -af "atrim=start=0.4" "$TMP/body.wav"
q -i "$TMP/g.wav" -af "atrim=end=0.4" "$TMP/head.wav"
q -i "$TMP/body.wav" -i "$TMP/head.wav" -filter_complex "[0][1]acrossfade=d=0.4:c1=tri:c2=tri,loudnorm=I=-20:TP=-2,aresample=44100" -c:a libvorbis -q:a 4 "$OUT/handcart_gravel.ogg"

# Clanks and a creak: pieces of the groaning metal (start, end, fade-out start, fade length)
cut() { q -i "$M" -af "aresample=44100,pan=mono|c0=0.5*c0+0.5*c1,atrim=$1:$2,asetpts=PTS-STARTPTS,afade=t=in:d=0.01,afade=t=out:st=$3:d=$4,loudnorm=I=-18:TP=-1,aresample=44100" -c:a libvorbis -q:a 4 "$OUT/$5.ogg"; }
cut 2.95 3.45 0.30 0.20 handcart_clank1
cut 3.50 4.20 0.45 0.25 handcart_clank2
cut 3.00 4.40 1.00 0.40 handcart_clank3
cut 0.00 1.10 0.80 0.30 handcart_creak
ls -l "$OUT"
