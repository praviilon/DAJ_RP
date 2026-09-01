# Make sure the user is not executing this script directly
if(NOT InOpenJK)
	message(FATAL_ERROR "Use the top-level cmake script!")
endif(NOT InOpenJK)

# GalaxyRP fix: [packaging] read the mod's own version out of rp_version.h so the pk3 name
# stays in sync with the actual build (matches the "GalaxyRP_v<major>.<minor>.<bugfix>.pk3"
# naming used in past releases) instead of a fixed, unversioned "GalaxyRP.pk3".
file(STRINGS "${CMAKE_SOURCE_DIR}/galaxyrp/game/rp_version.h" RPVersionDefines
	REGEX "^#define[ \t]+VERSION_(MAJOR|MINOR|BUGFIX)_RELEASE[ \t]+[0-9]+")
foreach(RPVersionDefine ${RPVersionDefines})
	if(RPVersionDefine MATCHES "VERSION_MAJOR_RELEASE[ \t]+([0-9]+)")
		set(RPVersionMajor ${CMAKE_MATCH_1})
	elseif(RPVersionDefine MATCHES "VERSION_MINOR_RELEASE[ \t]+([0-9]+)")
		set(RPVersionMinor ${CMAKE_MATCH_1})
	elseif(RPVersionDefine MATCHES "VERSION_BUGFIX_RELEASE[ \t]+([0-9]+)")
		set(RPVersionBugfix ${CMAKE_MATCH_1})
	endif()
endforeach()
set(RPVersion "${RPVersionMajor}.${RPVersionMinor}.${RPVersionBugfix}")
message(STATUS "GalaxyRP version is ${RPVersion}")

# GalaxyRP fix: [packaging] assets/server carries a play/host launcher pair for every OS this mod
# supports (galaxyrp_play/host.bat for Windows, .sh for Linux, .command for macOS), committed
# side-by-side so they're easy to find and maintain together -- but a given build should only ever
# ship the 2 scripts for its OWN target platform, not all 6. CMAKE_SYSTEM_NAME already reflects the
# right platform for every build that matters here (native on each CI runner; explicitly set by
# both Linux cross-toolchain files under cmake/Toolchains/) with no extra flag needed. Both places
# below that copy assets/server content (the POST_BUILD dev-build copy and the install() for
# packaging/CI) drop the other platforms' scripts using this same list.
set(GalaxyRPAllLauncherFiles
	galaxyrp_play.bat galaxyrp_host.bat
	galaxyrp_play.sh galaxyrp_host.sh
	galaxyrp_play.command galaxyrp_host.command)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(GalaxyRPLauncherFiles galaxyrp_play.bat galaxyrp_host.bat)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(GalaxyRPLauncherFiles galaxyrp_play.command galaxyrp_host.command)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(GalaxyRPLauncherFiles galaxyrp_play.sh galaxyrp_host.sh)
else()
	message(WARNING "GalaxyRP: unrecognized CMAKE_SYSTEM_NAME '${CMAKE_SYSTEM_NAME}' -- shipping "
		"every platform's launcher script since it's unclear which one applies. Set "
		"GalaxyRPLauncherFiles explicitly (e.g. -DGalaxyRPLauncherFiles=galaxyrp_play.sh;galaxyrp_host.sh) "
		"to fix this.")
	set(GalaxyRPLauncherFiles ${GalaxyRPAllLauncherFiles})
endif()
set(GalaxyRPExcludedLauncherFiles ${GalaxyRPAllLauncherFiles})
list(REMOVE_ITEM GalaxyRPExcludedLauncherFiles ${GalaxyRPLauncherFiles})

# Generate PK3 file
set(PK3FILE GalaxyRP_v${RPVersion}.pk3)
set(PK3PATH ${CMAKE_SOURCE_DIR}/assets/client)
set(TMPFILE ${CMAKE_CURRENT_BINARY_DIR}/${PK3FILE})
set(OUTFILE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${PK3FILE})

file(GLOB PK3ASSETS "${PK3PATH}/*")

include(InstallZIP)
add_zip_command(${TMPFILE} ${OUTFILE} DIR "${PK3PATH}" FILES "${PK3ASSETS}" )

# Finish and cleanup
# GalaxyRP fix: [packaging] add ALL so this target (which builds the pk3) is included in the
# default build ("Build Solution" in VS / plain `cmake --build .`) instead of requiring a
# separate explicit `--target assets` build step before install -- previously this had to be
# built by hand, which is easy to miss and leaves `cmake --install` failing to find the pk3.
add_custom_target(assets ALL DEPENDS "${TMPFILE}")
add_custom_command(TARGET assets POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E remove -f "${TMPFILE}"
)

# GalaxyRP fix: [packaging] copy the server-side content (database/, duelarena/, textfiles/, the
# .cfg/.html/.bat helper files) straight into the per-config build output
# (${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>, e.g. build-x64/Release) as part of the plain
# build, the same way the pk3 above already lands there via add_zip_command -- unlike assets/client
# (zipped into the pk3), this content is just copied as loose files, not archived. jampgame/cgame/ui
# already land in that same per-config folder automatically (that's the default MSVC/Ninja output
# location), so the build output folder alone now already looks like a complete GalaxyRP folder
# without requiring a separate `cmake --install` step -- install() below still assembles the full
# JediAcademy/GalaxyRP tree for packaging/CI, this is additive, not a replacement for it.
add_custom_command(TARGET assets POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_SOURCE_DIR}/assets/server" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
	COMMENT "Copying GalaxyRP server-side content to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
)
# GalaxyRP fix: [packaging] copy_directory above has no exclude option, so it always copies every
# platform's launcher script too -- remove the ones that don't belong to this build's target
# platform (see GalaxyRPExcludedLauncherFiles above) right after.
foreach(GalaxyRPExcludedLauncherFile ${GalaxyRPExcludedLauncherFiles})
	add_custom_command(TARGET assets POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E remove -f
			"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${GalaxyRPExcludedLauncherFile}"
	)
endforeach()

# GalaxyRP fix: [packaging] assemble the actual distributable "GalaxyRP" mod folder -- the pk3
# built above, plus the server-side content that ships alongside it (database/, duelarena/,
# textfiles/, the .cfg/.html/.bat helper files) from assets/server. This is what previously had
# to be put together by hand; jampgame/cgame/ui are installed here too (see their own
# CMakeLists.txt -- NOT packed inside the pk3, all three ship as loose files next to it, same as
# the old release layout). Deliberately does NOT depend on BuildMPEngine/BuildMPRdVanilla/
# BuildMPDed -- this folder never contains the engine executable or its renderer DLL, only the
# mod content and the three game-code modules.
install(FILES "${OUTFILE}"
	DESTINATION "${JKAInstallDir}/GalaxyRP"
	COMPONENT ${JKAMPCoreComponent})

# GalaxyRP fix: [packaging] exclude the other platforms' launcher scripts here too (see
# GalaxyRPExcludedLauncherFiles above), same reasoning as the POST_BUILD copy -- this is what
# actually lands in the packaged/CI-uploaded GalaxyRP folder, so it's the one that matters most for
# not shipping every OS's scripts in every release.
set(GalaxyRPServerInstallExcludes)
foreach(GalaxyRPExcludedLauncherFile ${GalaxyRPExcludedLauncherFiles})
	list(APPEND GalaxyRPServerInstallExcludes PATTERN "${GalaxyRPExcludedLauncherFile}" EXCLUDE)
endforeach()

# GalaxyRP fix: [packaging] install(DIRECTORY) does not preserve source file permissions -- it
# would otherwise install these scripts world-readable but NOT executable (confirmed: verified
# locally, installed galaxyrp_play.sh/host.sh came out 644, not the 755 they have in the repo).
# That was harmless for the old .bat-only setup (Windows has no Unix execute bit; a .bat's
# "executability" is determined by its extension), but it would silently break the whole point of
# the new .sh/.command scripts -- Linux's `./galaxyrp_play.sh` and macOS Finder's double-click both
# require the execute bit to actually be set. Force it back on explicitly for just the launcher
# files being installed (everything else under assets/server -- the .cfg/.html/.txt content,
# database/, textfiles/ -- has no reason to be executable).
set(GalaxyRPLauncherFilePermissions)
foreach(GalaxyRPLauncherFile ${GalaxyRPLauncherFiles})
	list(APPEND GalaxyRPLauncherFilePermissions
		PATTERN "${GalaxyRPLauncherFile}"
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
			GROUP_READ GROUP_EXECUTE
			WORLD_READ WORLD_EXECUTE)
endforeach()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/server/"
	DESTINATION "${JKAInstallDir}/GalaxyRP"
	COMPONENT ${JKAMPCoreComponent}
	${GalaxyRPServerInstallExcludes}
	${GalaxyRPLauncherFilePermissions})
