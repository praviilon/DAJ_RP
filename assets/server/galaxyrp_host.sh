#!/bin/sh

# galaxyrp_host.sh -- Linux dedicated server launcher

# Main settings
jk_dedicated=2
jk_net_port=29070
jk_config=galaxyrp_server.cfg
jk_executable_64="taystjkded.x86_64"
jk_executable_32="taystjkded.i386"

# GalaxyRP: [TaystJK] this script lives inside the GalaxyRP folder, one level below the engine
# executables -- switch to that parent folder FIRST, using the script's own location (not whatever
# directory the script happened to be launched from). This makes both the executable check below
# and the absolute homepath computed further down resolve against the engine's own folder, not
# wherever the script's working directory started out.
cd "$(dirname "$0")/.." || exit 1
ENGINE_DIR="$(pwd)"

# Executable check -- prefer the 64-bit TaystJK dedicated server, fall back to 32-bit, and bail
# out with an error instead of silently trying to launch something that isn't there.
if [ -f "$jk_executable_64" ]; then
	jk_executable="$jk_executable_64"
elif [ -f "$jk_executable_32" ]; then
	jk_executable="$jk_executable_32"
else
	echo "ERROR: Could not find $jk_executable_64 or $jk_executable_32 next to the GalaxyRP folder."
	echo "Make sure a TaystJK dedicated server build is installed alongside GalaxyRP."
	printf "Press Enter to exit..."
	read -r _
	exit 1
fi

chmod +x "$jk_executable" 2>/dev/null

# Launch. fs_portable 1 + an absolute fs_homepath keep every file the server writes (config
# changes, the accounts database, logs) inside this server folder instead of the user's home
# directory.
"./$jk_executable" +set dedicated "$jk_dedicated" +set net_port "$jk_net_port" +set fs_portable 1 +set fs_homepath "$ENGINE_DIR" +set fs_game GalaxyRP +exec "$jk_config"
