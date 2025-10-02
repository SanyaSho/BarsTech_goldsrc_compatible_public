#-----------------------------------------------------------------------------
# TIER0.CMAKE
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

set( TIER0_SOURCE_FILES )
BEGIN_SRC( TIER0_SOURCE_FILES "Source Files" )
	SRC_GRP(
		SOURCES
		"assert_dialog.cpp"
		"cpu.cpp"
		"dbg.cpp"
		"fasttimer.cpp"
		"mem.cpp"
		"memblockhdr.cpp"
		"memstd.cpp"
		"memvalidate.cpp"
		"mem_helpers.cpp"
		"minidump.cpp"
		"pch_tier0.cpp"
		"platform.cpp"
		"pme.cpp"
		"testthread.cpp"
		"thread.cpp"
		"threadtools.cpp"
		"tier0.cpp"
		"validator.cpp"
		"valobject.cpp"
		"vprof.cpp"
	)
END_SRC( TIER0_SOURCE_FILES "Source Files" )

set( TIER0_HEADER_FILES )
BEGIN_SRC( TIER0_HEADER_FILES "Header Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/public/tier0/basetypes.h"
		"${SRCDIR}/public/tier0/commonmacros.h"
		"${SRCDIR}/public/tier0/dbg.h"
		"${SRCDIR}/public/tier0/dbgflag.h"
		"${SRCDIR}/public/tier0/eventmasks.h"
		"${SRCDIR}/public/tier0/eventmodes.h"
		"${SRCDIR}/public/tier0/fasttimer.h"
		"${SRCDIR}/public/tier0/ia32detect.h"
		"${SRCDIR}/public/tier0/icommandline.h"
		"${SRCDIR}/public/tier0/ioctlcodes.h"
		"${SRCDIR}/public/tier0/k8performancecounters.h"
		"${SRCDIR}/public/tier0/l2cache.h"
		"${SRCDIR}/public/tier0/mem.h"
		"${SRCDIR}/public/tier0/memalloc.h"
		"${SRCDIR}/public/tier0/memdbgoff.h"
		"${SRCDIR}/public/tier0/memdbgon.h"
		"${SRCDIR}/public/tier0/memoverride.cpp"
		"${SRCDIR}/public/tier0/minidump.h"
		"${SRCDIR}/public/tier0/MinMax.h"
		"${SRCDIR}/public/tier0/p4performancecounters.h"
		"${SRCDIR}/public/tier0/p5p6performancecounters.h"
		"${SRCDIR}/public/tier0/platform.h"
		"${SRCDIR}/public/tier0/pmelib.h"
		"${SRCDIR}/public/tier0/protected_things.h"
		"${SRCDIR}/public/tier0/testthread.h"
		"${SRCDIR}/public/tier0/threadtools.h"
		"${SRCDIR}/public/tier0/tslist.h"
		"${SRCDIR}/public/tier0/validator.h"
		"${SRCDIR}/public/tier0/valobject.h"
		"${SRCDIR}/public/tier0/valve_off.h"
		"${SRCDIR}/public/tier0/valve_on.h"
		"${SRCDIR}/public/tier0/vcrmode.h"
		"${SRCDIR}/public/tier0/vcr_shared.h"
		"${SRCDIR}/public/tier0/vprof.h"
		"${SRCDIR}/public/tier0/wchartypes.h"
		"${SRCDIR}/public/tier0/xbox_codeline_defines.h"
	)
END_SRC( TIER0_HEADER_FILES "Header Files" )

add_library(
	tier0 SHARED
	${TIER0_SOURCE_FILES} ${TIER0_HEADER_FILES}
)
install_library(
	TARGET tier0
	INSTALL_DEST "${GAMEDIR}"
)

target_include_directories(
	tier0 PRIVATE

	"${SRCDIR}" # FIXME(SanyaSho): Some headers want to include "public/"

	"${SRCDIR}/common"
	"${SRCDIR}/public"
	"${SRCDIR}/public/tier1"
	"${SRCDIR}/public/tier0"
)

target_compile_definitions(
	tier0 PRIVATE

	TIER0_DLL_EXPORT
	NO_VCR
)
