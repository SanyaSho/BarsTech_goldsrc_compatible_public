#-----------------------------------------------------------------------------
# VGUI2.CMAKE
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

set( VGUI2_SOURCE_FILES )
BEGIN_SRC( VGUI2_SOURCE_FILES "Source Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/public/interface.cpp"
		"${SRCDIR}/public/UnicodeFileHelpers.cpp"
		"${SRCDIR}/public/filesystem_helpers.cpp"

		"Bitmap.cpp"
		"Border.cpp"
		"LocalizedStringTable.cpp"
		"MemoryBitmap.cpp"
		"Scheme.cpp"
		"VGuiKeyValues.cpp"
		"VPanel.cpp"
		"VPanelWrapper.cpp"
		"system_win32.cpp"
		"input.cpp"
		"vgui.cpp"
		"vgui_internal.cpp"
		"vgui_key_translation.cpp"
	)
END_SRC( VGUI2_SOURCE_FILES "Source Files" )

set( VGUI2_HEADER_FILES )
BEGIN_SRC( VGUI2_HEADER_FILES "Header Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/public/html/htmlmessages.h"
		"${SRCDIR}/public/html/ichromehtmlwrapper.h"
		"${SRCDIR}/public/html/ihtmlchrome.h"
		"${SRCDIR}/public/html/ihtmlserializer.h"
		"${SRCDIR}/public/html/ipainthtml.h"

		"${SRCDIR}/public/vgui/Cursor.h"
		"${SRCDIR}/public/vgui/Dar.h"
		"${SRCDIR}/public/vgui/IBorder.h"
		"${SRCDIR}/public/vgui/IClientPanel.h"
		"${SRCDIR}/public/vgui/IHTML.h"
		"${SRCDIR}/public/vgui/IImage.h"
		"${SRCDIR}/public/vgui/IInput.h"
		"${SRCDIR}/public/vgui/IInputInternal.h"
		"${SRCDIR}/public/vgui/IKeyValues.h"
		"${SRCDIR}/public/vgui/ILocalize.h"
		"${SRCDIR}/public/vgui/IPanel.h"
		"${SRCDIR}/public/vgui/IScheme.h"
		"${SRCDIR}/public/vgui/ISurface.h"
		"${SRCDIR}/public/vgui/ISurfaceV28.h"
		"${SRCDIR}/public/vgui/ISurfaceV29.h"
		"${SRCDIR}/public/vgui/ISystem.h"
		"${SRCDIR}/public/vgui/IVGui.h"
		"${SRCDIR}/public/vgui/IVguiMatInfo.h"
		"${SRCDIR}/public/vgui/IVguiMatInfoVar.h"
		"${SRCDIR}/public/vgui/IVguiV06.h"
		"${SRCDIR}/public/vgui/KeyCode.h"
		"${SRCDIR}/public/vgui/MouseCode.h"
		"${SRCDIR}/public/vgui/Point.h"
		"${SRCDIR}/public/vgui/VGUI2.h"

		"Bitmap.h"
		"Border.h"
		"LocalizedStringTable.h"
		"MemoryBitmap.h"
		"Scheme.h"
		"VGuiKeyValues.h"
		"VPanel.h"
		"VPanelWrapper.h"
		"input.h"
		"system_win32.h"
		"vgui.h"
		"vgui_internal.h"
		"vgui_key_translation.h"
	)
END_SRC( VGUI2_HEADER_FILES "Header Files" )

add_library(
	vgui2 SHARED
	${VGUI2_SOURCE_FILES} ${VGUI2_HEADER_FILES}
)
install_library(
	TARGET vgui2
	INSTALL_DEST "${GAMEDIR}"
)
set_property(
	TARGET vgui2
	PROPERTY FOLDER "Shared Libraries"
)

target_include_directories(
	vgui2 PRIVATE

	"${SRCDIR}" # FIXME(SanyaSho): Some headers want to include "public/"

	"${SRCDIR}/common"
	"${SRCDIR}/public"
	"${SRCDIR}/public/tier1"
	"${SRCDIR}/public/vgui"
	"${SRCDIR}/public/vgui_controls"
)

target_compile_definitions(
	vgui2 PRIVATE

	NO_MALLOC_OVERRIDE
	MINMAX
	ALTERNATE_LE
)

target_link_libraries(
	vgui2 PRIVATE

	$<${IS_WINDOWS}:shlwapi>

	tier0
	tier1
	vstdlib # TODO(SanyaSho): Remove this!
)
