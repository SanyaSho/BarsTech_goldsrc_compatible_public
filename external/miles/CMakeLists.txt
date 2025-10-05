#-----------------------------------------------------------------------------
# CMAKELISTS.TXT
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

set( CMAKE_POSITION_INDEPENDENT_CODE ON )

project( MSS CXX C ASM_MASM )

set( CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE )

set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_VISIBILITY_PRESET hidden )

set( SRCDIR "${CMAKE_CURRENT_LIST_DIR}/src/src" )

include_directories(
	"${SRCDIR}/include"
)

if( WIN32 )
	add_compile_options(
		/permissive
	)

	add_compile_definitions(
		_CRT_SECURE_NO_DEPRECATE
		_SECURE_SCL=0
		_HAS_ITERATOR_DEBUGGING

		WIN32
		_WIN32

		#WIN64 # TODO(SanyaSho): WIN64
		#_WIN64 # TODO(SanyaSho): WIN64
	)
else()
endif()

set(
	MSS_SOURCE_FILES

	"${SRCDIR}/sdk/adpcm_c.cpp"
	"${SRCDIR}/sdk/flt.cpp"
	"${SRCDIR}/sdk/genericmss.cpp"
	"${SRCDIR}/sdk/hlbank.cpp"
	"${SRCDIR}/sdk/hlparse.cpp"
	"${SRCDIR}/sdk/io.cpp"
	"${SRCDIR}/sdk/m3d.cpp"
	"${SRCDIR}/sdk/milesasync.cpp"
	"${SRCDIR}/sdk/mileseventaudition.cpp"
	"${SRCDIR}/sdk/mileseventbank.cpp"
	"${SRCDIR}/sdk/MilesEventExec.cpp"
	"${SRCDIR}/sdk/MilesEventHandle.cpp"
	"${SRCDIR}/sdk/mileseventqueue.cpp"
	"${SRCDIR}/sdk/mileseventsupport.cpp"
	"${SRCDIR}/sdk/miscutil.cpp"
	"${SRCDIR}/sdk/mix.cpp"
	"${SRCDIR}/sdk/mss.cpp"
	"${SRCDIR}/sdk/mssdbg.c"
	"${SRCDIR}/sdk/mssdig.cpp"
	"${SRCDIR}/sdk/mssevent.cpp"
	"${SRCDIR}/sdk/mssinput.cpp"
	"${SRCDIR}/sdk/mssstrm.cpp"
	"${SRCDIR}/sdk/msssys.c"
	"${SRCDIR}/sdk/rib.cpp"
	"${SRCDIR}/sdk/rrcore.c"
	"${SRCDIR}/sdk/wavefile.cpp"
	"${SRCDIR}/sdk/win32io.cpp"
	"${SRCDIR}/sdk/win32_rratomics.cpp"
	"${SRCDIR}/sdk/win32_rrcpu.c"
	"${SRCDIR}/sdk/win32_rrthreads.c"
	"${SRCDIR}/sdk/win32_rrtime.cpp"

	"${SRCDIR}/sdk/mss.rc"

	"${SRCDIR}/sdk/mssa32.asm" # TODO(SanyaSho): !WIN64
)

add_library(
	mss32 SHARED
	"${MSS_SOURCE_FILES}"
)

target_compile_definitions(
	mss32 PRIVATE

	__RADFINAL__
	USE_MSS
	__RADINDLL__
)

target_link_libraries(
	mss32 PRIVATE

	ws2_32
	winmm
)

