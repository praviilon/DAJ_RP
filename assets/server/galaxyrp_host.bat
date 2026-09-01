@echo off

:: galaxyrp_host.bat -- Windows dedicated server launcher

:: Main settings
set jk_dedicated=2
set jk_net_port=29070
set jk_config=galaxyrp_server.cfg
set jk_executable_64=taystjkded.x86_64.exe
set jk_executable_32=taystjkded.x86.exe

:: GalaxyRP fix: [TaystJK] this script lives inside the GalaxyRP folder, one level below the
:: engine executables -- switch to that parent folder FIRST, using %~dp0 (this batch file's own
:: location) rather than relying on whatever folder it happened to be double-clicked or launched
:: from. This makes both the executable check below and "fs_homepath ." further down resolve
:: against the engine's own folder, not wherever the script's working directory started out.
cd /d "%~dp0.."

:: Executable check -- prefer the 64-bit TaystJK dedicated server, fall back to 32-bit, and bail
:: out with an error instead of silently trying to launch something that isn't there.
if exist "%jk_executable_64%" (
	set jk_executable=%jk_executable_64%
) else if exist "%jk_executable_32%" (
	set jk_executable=%jk_executable_32%
) else (
	echo ERROR: Could not find %jk_executable_64% or %jk_executable_32% next to the GalaxyRP folder.
	echo Make sure a TaystJK dedicated server build is installed alongside GalaxyRP.
	pause
	exit /b 1
)

:: Launch. fs_portable 1 + fs_homepath . keep every file the server writes (config changes, the
:: accounts database, logs) inside this server folder instead of the Windows user profile.
%jk_executable% +set dedicated %jk_dedicated% +set net_port %jk_net_port% +set fs_portable 1 +set fs_homepath . +set fs_game GalaxyRP +exec %jk_config%
