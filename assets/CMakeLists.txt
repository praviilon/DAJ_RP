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

install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/server/"
	DESTINATION "${JKAInstallDir}/GalaxyRP"
	COMPONENT ${JKAMPCoreComponent})
