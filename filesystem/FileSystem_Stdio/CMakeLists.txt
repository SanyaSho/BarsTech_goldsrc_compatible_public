#-----------------------------------------------------------------------------
# FILESYSTEM_STDIO.CMAKE
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

set( FILESYSTEM_STDIO_SOURCE_FILES )
BEGIN_SRC( FILESYSTEM_STDIO_SOURCE_FILES "Source Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/common/StringUtils.cpp"
		"${SRCDIR}/public/characterset.cpp"
		"${SRCDIR}/public/interface.cpp"
		"CFileHandle.cpp"
		"CFileSystem.cpp"
		"CFileSystem.obsolete.cpp"
	)
END_SRC( FILESYSTEM_STDIO_SOURCE_FILES "Source Files" )

set( FILESYSTEM_STDIO_HEADER_FILES )
BEGIN_SRC( FILESYSTEM_STDIO_HEADER_FILES "Header Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/public/characterset.h"
		"${SRCDIR}/public/interface.h"
		"CFileHandle.h"
		"CFileSystem.h"
		"CPackFileEntry.h"
		"CSearchPath.h"
		"PackFile.h"
	)
END_SRC( FILESYSTEM_STDIO_HEADER_FILES "Header Files" )

add_library(
	filesystem_stdio SHARED
	${FILESYSTEM_STDIO_SOURCE_FILES} ${FILESYSTEM_STDIO_HEADER_FILES}
)
install_library(
	TARGET filesystem_stdio
	INSTALL_DEST "${GAMEDIR}"
)
set_property(
	TARGET filesystem_stdio
	PROPERTY FOLDER "Shared Libraries"
)

target_include_directories(
	filesystem_stdio PRIVATE

	"${SRCDIR}/common"
	"${SRCDIR}/public"
	"${SRCDIR}/public/tier1"
)

target_compile_definitions(
	filesystem_stdio PRIVATE

	_SCL_SECURE_NO_WARNINGS

	NO_MALLOC_OVERRIDE
	filesystem_stdio_EXPORTS
)
