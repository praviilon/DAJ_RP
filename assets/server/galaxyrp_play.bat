@echo off

:: Main settings
set jk_executable_64=taystjk.x86_64.exe
set jk_executable_32=taystjk.x86.exe

:: GalaxyRP fix: [TaystJK] this script lives inside the GalaxyRP folder, one level below the
:: engine executables -- switch to that parent folder FIRST, using %~dp0 (this batch file's own
:: location) rather than relying on whatever folder it happened to be double-clicked or launched
:: from. This makes both the executable check below and "fs_homepath ." further down resolve
:: against the engine's own folder, not wherever the script's working directory started out.
cd /d "%~dp0.."

:: Executable check -- prefer the 64-bit TaystJK client, fall back to 32-bit, and bail out with
:: an error instead of silently trying to launch something that isn't there.
if exist "%jk_executable_64%" (
	set jk_executable=%jk_executable_64%
) else if exist "%jk_executable_32%" (
	set jk_executable=%jk_executable_32%
) else (
	echo ERROR: Could not find %jk_executable_64% or %jk_executable_32% next to the GalaxyRP folder.
	echo Make sure a TaystJK client build is installed alongside GalaxyRP.
	pause
	exit /b 1
)

:: Welcome message
echo   _____________________________________________________
echo "| ___________________________________________________ |"
echo "||   _____       _                    _____  _____   ||"
echo "||  / ____|     | |                  |  __ \|  __ \  ||"
echo "|| | |  __  __ _| | __ ___  ___   _  | |__) | |__) | ||"
echo "|| | | |_ |/ _` | |/ _` \ \/ / | | | |  _  /|  ___/  ||"
echo "|| | |__| | (_| | | (_| |>  <| |_| | | | \ \| |      ||"
echo "||  \_____|\__,_|_|\__,_/_/\_\\__, | |_|  \_\_|      ||"
echo "||                             __/ |                 ||"
echo "||        ___________________ |___/ _______          ||"
echo "||                                                   ||"
echo "||               A JKA ROLEPLAYING MOD               ||"
echo "||___________________________________________________||"
echo "|_____________________________________________________|"
echo.

:: Show options
echo [1] Press ENTER to play locally
set /p option=[2] Type or paste an IP to connect:

:: Check options. fs_portable 1 + fs_homepath . keep every file the client writes (settings,
:: saved profiles, screenshots) inside this folder instead of the Windows user profile.
:: GalaxyRP fix: [TaystJK] dropped "+set net_port 29070 +set dedicated 0 +exec server.cfg" from
:: the "play locally" branch -- those are dedicated-server-only settings (server.cfg isn't even
:: this mod's config filename, that's galaxyrp_server.cfg) that don't belong on a client launch
:: and appear to have been copy-pasted here by mistake.
if "%option%" == "" (
	start "" %jk_executable% +set fs_portable 1 +set fs_homepath . +set fs_game GalaxyRP
) else (
	start "" %jk_executable% +set fs_portable 1 +set fs_homepath . +set fs_game GalaxyRP +connect %option%
)
