#!/usr/bin/env bash
# Rebuild HandCart and (re)start the local test (extra server launch parameters in
# SERVER_ARGS, e.g. -doInvMoveLog; another mission in MISSION, e.g. handcart.nasdara):
# stops any running DayZ server and client,
# then starts the handcart mission's server (fresh persistence) and a client connected to
# it, through ~/Projects/DayZMissions. Returns once the client has finished loading in, or
# after WAIT seconds (default 300).
#
#   tools/restart_test.sh
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MISSIONS="${MISSIONS:-$HOME/Projects/DayZMissions}"
STEAM_ROOT="${STEAM_ROOT:-$HOME/.steam/debian-installation}"
SERVER_PROFILES="$STEAM_ROOT/steamapps/common/DayZServer/profiles"
CLIENT_LOGS="$STEAM_ROOT/steamapps/compatdata/221100/pfx/drive_c/users/steamuser/AppData/Local/DayZ"
LOGS="${LOGS:-$ROOT/build/logs}"
mkdir -p "$LOGS"

"$ROOT/tools/build.sh" || exit 1

# [D]/[d] brackets: the patterns must not match this script's own command line
pkill -f '[D]ayZ_x64.exe'
pkill -f '[D]ayZ_BE.exe'
pkill -f '[.]/DayZServer -config=serverDZ.cfg -port=2302'
for _ in $(seq 1 60); do
	pgrep -f '[.]/DayZServer -config=serverDZ.cfg -port=2302' >/dev/null || break
	sleep 1
done

start=$(date +%s)
(cd "$MISSIONS" && WIPE=1 nohup tools/run_server.sh "${MISSION:-handcart}" ${SERVER_ARGS:-} >"$LOGS/server.out" 2>&1 &)
for _ in $(seq 1 180); do
	f=$(ls -t "$SERVER_PROFILES"/script_*.log 2>/dev/null | head -n1)
	if [ -n "$f" ] && [ "$(stat -c %Y "$f")" -ge "$start" ]; then
		grep -q "Can't compile" "$f" && { echo "server scripts don't compile: $f" >&2; exit 1; }
		grep -q "test site ready" "$f" && break
	fi
	sleep 1
done
echo "server up"

(cd "$MISSIONS" && nohup tools/run_client.sh "${MISSION:-handcart}" >"$LOGS/client.out" 2>&1 &)
for _ in $(seq 1 "${WAIT:-300}"); do
	f=$(ls -t "$CLIENT_LOGS"/script_*.log 2>/dev/null | head -n1)
	if [ -n "$f" ] && [ "$(stat -c %Y "$f")" -ge "$start" ]; then
		grep -q "Can't compile" "$f" && { echo "client scripts don't compile: $f" >&2; exit 1; }
		grep -q "drawing the held truck" "$f" && break
	fi
	sleep 1
done
echo "client log: $f"
