#!/bin/sh

# galaxyrp_play.sh -- Linux client launcher

# Main settings
jk_executable_64="taystjk.x86_64"
jk_executable_32="taystjk.i386"

# GalaxyRP: [TaystJK] this script lives inside the GalaxyRP folder, one level below the engine
# executables -- switch to that parent folder FIRST, using the script's own location (not whatever
# directory the script happened to be launched from). This makes both the executable check below
# and the absolute homepath computed further down resolve against the engine's own folder, not
# wherever the script's working directory started out.
cd "$(dirname "$0")/.." || exit 1
ENGINE_DIR="$(pwd)"

# Executable check -- prefer the 64-bit TaystJK client, fall back to 32-bit, and bail out with
# an error instead of silently trying to launch something that isn't there.
if [ -f "$jk_executable_64" ]; then
	jk_executable="$jk_executable_64"
elif [ -f "$jk_executable_32" ]; then
	jk_executable="$jk_executable_32"
else
	echo "ERROR: Could not find $jk_executable_64 or $jk_executable_32 next to the GalaxyRP folder."
	echo "Make sure a TaystJK client build is installed alongside GalaxyRP."
	printf "Press Enter to exit..."
	read -r _
	exit 1
fi

chmod +x "$jk_executable" 2>/dev/null

# Welcome message
printf '%s\n' '  _____________________________________________________'
printf '%s\n' ' | ___________________________________________________ |'
printf '%s\n' ' ||   _____       _                    _____  _____   ||'
printf '%s\n' ' ||  / ____|     | |                  |  __ \|  __ \  ||'
printf '%s\n' ' || | |  __  __ _| | __ ___  ___   _  | |__) | |__) | ||'
printf '%s\n' ' || | | |_ |/ _` | |/ _` \ \/ / | | | |  _  /|  ___/  ||'
printf '%s\n' ' || | |__| | (_| | | (_| |>  <| |_| | | | \ \| |      ||'
printf '%s\n' ' ||  \_____|\__,_|_|\__,_/_/\_\\__, | |_|  \_\_|      ||'
printf '%s\n' ' ||                             __/ |                 ||'
printf '%s\n' ' ||        ___________________ |___/ _______          ||'
printf '%s\n' ' ||                                                   ||'
printf '%s\n' ' ||               A JKA ROLEPLAYING MOD               ||'
printf '%s\n' ' ||___________________________________________________||'
printf '%s\n' ' |_____________________________________________________|'
printf '\n'

# Show options
echo "[1] Press ENTER to play locally"
printf "[2] Type or paste an IP to connect: "
read -r option

# Launch (detached, so this script/terminal returns immediately -- matches the Windows client
# script's "start ""). fs_portable 1 + an absolute fs_homepath keep every file the client writes
# (settings, saved profiles, screenshots) inside this folder instead of the user's home directory.
# GalaxyRP fix: [TaystJK] the TaystJK client (unlike the dedicated server) defaults fs_forcegame
# to "taystjk", which unconditionally overwrites fs_gamedir back to "taystjk" at the end of
# FS_Startup -- after fs_game GalaxyRP has already added GalaxyRP to the search path, but before
# the engine decides where to actually WRITE files. Game content still loads fine from GalaxyRP
# (it's on the search path), but screenshots, saved configs, and demos would silently land in
# <fs_homepath>/taystjk/ instead of GalaxyRP/. Setting fs_forcegame explicitly to GalaxyRP here
# neutralizes that override (fs_forcegame ends up equal to fs_gamedir, so the override no-ops)
# and makes GalaxyRP the actual write target too.
if [ -z "$option" ]; then
	nohup "./$jk_executable" +set fs_portable 1 +set fs_homepath "$ENGINE_DIR" +set fs_game GalaxyRP +set fs_forcegame GalaxyRP >/dev/null 2>&1 &
else
	nohup "./$jk_executable" +set fs_portable 1 +set fs_homepath "$ENGINE_DIR" +set fs_game GalaxyRP +set fs_forcegame GalaxyRP +connect "$option" >/dev/null 2>&1 &
fi
