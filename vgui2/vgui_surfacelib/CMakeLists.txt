#-----------------------------------------------------------------------------
# VGUI2_SURFACELIB.CMAKE
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

set( VGUI2_SURFACELIB_SOURCE_FILES )
BEGIN_SRC( VGUI2_SURFACELIB_SOURCE_FILES "Source Files" )
	SRC_GRP(
		SOURCES
		"FontAmalgam.cpp"
		"FontManager.cpp"
		"Win32Font.cpp"
	)
END_SRC( VGUI2_SURFACELIB_SOURCE_FILES "Source Files" )

set( VGUI2_SURFACELIB_HEADER_FILES )
BEGIN_SRC( VGUI2_SURFACELIB_HEADER_FILES "Header Files" )
	SRC_GRP(
		SOURCES
		#TODO(SanyaSho): Fill this!
	)
END_SRC( VGUI2_SURFACELIB_HEADER_FILES "Header Files" )

add_library(
	vgui2_surfacelib STATIC
	${VGUI2_SURFACELIB_SOURCE_FILES} ${VGUI2_SURFACELIB_HEADER_FILES}
)

target_include_directories(
	vgui2_surfacelib PRIVATE

	"${SRCDIR}" # FIXME(SanyaSho): Some headers want to include "public/"

	"${SRCDIR}/public"
	"${SRCDIR}/public/tier1"
	"${SRCDIR}/public/tier0"
	"${SRCDIR}/public/vgui"
	"${SRCDIR}/public/vgui_controls"
)

target_compile_definitions(
	vgui2_surfacelib PRIVATE

	VGUI2_SURFACELIB
	NO_MALLOC_OVERRIDE
	MINMAX
	ALTERNATE_LE
)
