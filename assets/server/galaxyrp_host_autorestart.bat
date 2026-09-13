@echo off
setlocal enabledelayedexpansion

:: galaxyrp_host_autorestart.bat -- Windows dedicated server launcher with auto-restart
::
:: Same as galaxyrp_host.bat, but if the server dies it is brought straight back up. A clean
:: shutdown -- /quit, or any exit with code 0 -- ends the loop instead, so the server stays
:: stoppable without reaching for Task Manager. Every restart is appended to
:: galaxyrp_restart.log next to this script.

:: Main settings
set jk_dedicated=2
set jk_net_port=29070
set jk_config=galaxyrp_server.cfg
set jk_executable_64=taystjkded.x86_64.exe
set jk_executable_32=taystjkded.x86.exe

:: Auto-restart settings
set jk_restart_delay=5
set jk_restart_min_uptime=10
set jk_restart_max_failures=5
set jk_restart_log=galaxyrp_restart.log
set jk_restart_log_max=1048576

:: GalaxyRP fix: [TaystJK] the restart log belongs next to THIS script, in the GalaxyRP folder --
:: which is also where the server's own logs land, since fs_homepath below points at the engine
:: folder and fs_game puts everything the server writes under GalaxyRP\. Resolve it from %~dp0
:: before the cd on the next line, because after that the working directory is the engine folder.
set "jk_restart_log_path=%~dp0%jk_restart_log%"

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

:: PowerShell gives an unambiguous yyyy-MM-dd HH:mm:ss stamp for the log. It is only ever used
:: for that text: every decision this script makes runs off the :jk_now_cs clock below, so a
:: machine without PowerShell still restarts correctly, just with a locale-formatted date.
set jk_have_powershell=0
where powershell >nul 2>&1 && set jk_have_powershell=1

:: %TIME% is documented as 24-hour, but it is still locale data, so prove the clock parses into a
:: sane centisecond-of-day before trusting it to measure uptime. If it does not, uptime is
:: reported as unknown and the give-up rule is disabled -- better to keep restarting than to stop
:: a healthy server because the clock could not be read.
set jk_uptime_ok=1
call :jk_now_cs jk_clock_probe
if not defined jk_clock_probe set jk_uptime_ok=0
if "%jk_uptime_ok%"=="1" if %jk_clock_probe% lss 0 set jk_uptime_ok=0
if "%jk_uptime_ok%"=="1" if %jk_clock_probe% gtr 8640000 set jk_uptime_ok=0

set /a jk_restart_count=0
set /a jk_fast_failures=0

call :jk_log "wrapper started -- executable %jk_executable%, port %jk_net_port%"
if "%jk_uptime_ok%"=="0" call :jk_log "note: the system clock could not be parsed, so uptime is unknown and the give-up rule is off"
echo GalaxyRP auto-restart wrapper. Restart log: %jk_restart_log_path%
echo A clean shutdown ^(/quit^) stops the wrapper; a crash restarts it after %jk_restart_delay%s.

:jk_restart_loop
call :jk_now_cs jk_started

:: Launch. fs_portable 1 + fs_homepath . keep every file the server writes (config changes, the
:: accounts database, logs) inside this server folder instead of the Windows user profile.
%jk_executable% +set dedicated %jk_dedicated% +set net_port %jk_net_port% +set fs_portable 1 +set fs_homepath . +set fs_game GalaxyRP +exec %jk_config%
set jk_status=!errorlevel!

call :jk_now_cs jk_ended
if "%jk_uptime_ok%"=="1" (
	set /a jk_run_time=^(!jk_ended! - !jk_started!^) / 100
	if !jk_run_time! lss 0 set /a jk_run_time+=86400
) else (
	set /a jk_run_time=-1
)
call :jk_format_uptime !jk_run_time! jk_uptime_text

if !jk_status! equ 0 (
	call :jk_log "clean exit after !jk_uptime_text! -- not restarting"
	echo Server exited cleanly after !jk_uptime_text!. Not restarting.
	exit /b 0
)

set /a jk_restart_count+=1

:: A server that dies within seconds is not crashing under load -- it is failing to start at all:
:: a bad cvar in the .cfg, a missing pk3, a port already taken. Restarting that as fast as the
:: loop can go achieves nothing except a pinned CPU and a log that eats the disk.
if !jk_run_time! geq 0 (
	if !jk_run_time! lss %jk_restart_min_uptime% (
		set /a jk_fast_failures+=1
	) else (
		set /a jk_fast_failures=0
	)
)

if !jk_fast_failures! geq %jk_restart_max_failures% (
	call :jk_log "giving up after !jk_fast_failures! starts that each lasted under %jk_restart_min_uptime%s -- last exit=!jk_status!"
	echo.
	echo ERROR: the server has failed to stay up !jk_fast_failures! times in a row,
	echo each time for less than %jk_restart_min_uptime% seconds. The last exit code was !jk_status!.
	echo That is a startup problem, not a crash -- check %jk_config%, the port, and the
	echo server's own console output above. Not restarting again.
	pause
	exit /b 1
)

call :jk_log "restart #!jk_restart_count! exit=!jk_status! after !jk_uptime_text!"
echo Server exited with code !jk_status! after !jk_uptime_text! -- restarting in %jk_restart_delay%s...
timeout /t %jk_restart_delay% /nobreak >nul 2>&1 || ping -n %jk_restart_delay% 127.0.0.1 >nul 2>&1
goto jk_restart_loop

:: ---------------------------------------------------------------------------------------------
:: Centiseconds since midnight, parsed out of %TIME%. The 1%%a-100 form stops 08 and 09 being
:: read as bad octal; "%%TIME%%: =0%%" pads the space-padded hour; the :., delimiter set covers
:: both decimal separators in use. Returns nothing at all if the parse fails, which is what the
:: probe above looks for.
::
:: Both seconds and hundredths must actually be present. Without that guard a clock reading
:: "19:06" -- no seconds field at all -- still produced a number, because the missing tokens
:: expand to nothing and 1-100 is perfectly good arithmetic. The result looked plausible, sat
:: inside the sanity range the probe checks, and advanced only once a minute, which would have
:: made every short run measure as 0s and tripped the give-up rule on a healthy server.
:jk_now_cs
setlocal
set "_cs="
for /f "tokens=1-4 delims=:.," %%a in ("%TIME: =0%") do (
	if not "%%c"=="" if not "%%d"=="" set /a "_cs=(((1%%a-100)*60+(1%%b-100))*60+(1%%c-100))*100+(1%%d-100)" 2>nul
)
endlocal & set "%~1=%_cs%"
exit /b

:: Seconds as a short human figure: 4h12m, 3m07s, 8s. Uptime is the single most useful number in
:: the log -- it separates "one bad map" from "this server never came up at all".
:jk_format_uptime
setlocal
set /a _t=%~1
if !_t! lss 0 (
	set "_o=an unknown time"
) else (
	set /a _h=_t/3600
	set /a _m=^(_t %% 3600^)/60
	set /a _s=_t %% 60
	:: pad the trailing unit, so this reads identically to the .sh and .command versions --
	:: 4h12m, 3m07s, 8s. Pad into separate variables: the branches below compare the raw
	:: numbers, and a leading zero has no business in a numeric comparison.
	set "_mp=!_m!"
	set "_sp=!_s!"
	if !_m! lss 10 set "_mp=0!_m!"
	if !_s! lss 10 set "_sp=0!_s!"
	if !_h! gtr 0 (
		set "_o=!_h!h!_mp!m"
	) else if !_m! gtr 0 (
		set "_o=!_m!m!_sp!s"
	) else (
		set "_o=!_t!s"
	)
)
endlocal & set "%~2=%_o%"
exit /b

:: Append one line to the restart log, rotating it first if it has grown past the cap. A server
:: that crash-loops for a week should not be able to fill the disk with its own restart log.
:jk_log
for %%F in ("%jk_restart_log_path%") do if exist "%jk_restart_log_path%" if %%~zF gtr %jk_restart_log_max% move /y "%jk_restart_log_path%" "%jk_restart_log_path%.old" >nul 2>&1
call :jk_timestamp jk_ts
>>"%jk_restart_log_path%" echo !jk_ts! %~1
exit /b

:jk_timestamp
setlocal
set "_ts="
if "%jk_have_powershell%"=="1" for /f "usebackq delims=" %%T in (`powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss'"`) do set "_ts=%%T"
if not defined _ts set "_ts=%DATE% %TIME%"
endlocal & set "%~1=%_ts%"
exit /b
