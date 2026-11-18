#!/usr/bin/env bash
# Compila la GDExtension. Uso:
#   ./build.sh            -> Linux (debug)
#   ./build.sh windows    -> Windows .dll (debug, con MinGW)
#   ./build.sh all        -> Linux + Windows, debug + release
set -u
cd "$(dirname "$0")"
JOBS=${JOBS:-10}

build() { # plataforma target
	local extra=""
	[ "$1" = "windows" ] && extra="use_mingw=yes"
	local log="build_$1_$2.log"
	echo "== $1 / $2"
	if scons platform="$1" target="$2" $extra -j"$JOBS" > "$log" 2>&1; then
		echo "   OK"
	else
		grep -E -B2 -A8 "error:|Error [0-9]" "$log" | head -150
		echo "   FALLO (ver $log)"
		return 1
	fi
}

case "${1:-linux}" in
	linux) build linux template_debug ;;
	windows) build windows template_debug ;;
	all)
		build linux template_debug && build windows template_debug &&
			build linux template_release && build windows template_release
		;;
	*) echo "uso: $0 [linux|windows|all]"; exit 2 ;;
esac
