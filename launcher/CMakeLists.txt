#-----------------------------------------------------------------------------
# LAUNCHER.CMAKE
#
# Install as: "hl.exe"
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

set( LAUNCHER_SOURCE_FILES )
BEGIN_SRC( LAUNCHER_SOURCE_FILES "Source Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/common/CCommandLine.cpp"
		"${SRCDIR}/common/FilePaths.cpp"
		"${SRCDIR}/public/interface.cpp"
		"${SRCDIR}/public/registry.cpp"
		"launcher.cpp"
	)
END_SRC( LAUNCHER_SOURCE_FILES "Source Files" )

set( LAUNCHER_HEADER_FILES )
BEGIN_SRC( LAUNCHER_HEADER_FILES "Header Files" )
	SRC_GRP(
		SOURCES
		#TODO(SanyaSho): Fill this!
	)
END_SRC( LAUNCHER_HEADER_FILES "Header Files" )

add_executable(
	launcher WIN32
	${LAUNCHER_SOURCE_FILES} ${LAUNCHER_HEADER_FILES}
)
install_library(
	TARGET launcher
	EXECUTABLE
	INSTALL_OUTNAME "hl"
	INSTALL_DEST "${GAMEDIR}"
)

target_include_directories(
	launcher PRIVATE

	"${SRCDIR}/public"
	"${SRCDIR}/common"
)

target_include_directories(
	launcher PRIVATE

	"${SRCDIR}/public"
	"${SRCDIR}/common"
)

target_link_libraries(
	launcher PRIVATE

	$<${IS_WINDOWS}:wsock32>
)