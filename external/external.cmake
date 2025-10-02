#-----------------------------------------------------------------------------
# EXTERNAL.CMAKE
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

include_guard( GLOBAL )


### SDL2

function( target_use_sdl2 target )
	set( SDL2_INCLUDE_DIR "${SRCDIR}/external/SDL2/include" )

	set( SDL2_HEADER_FILES )
	BEGIN_SRC( SDL2_HEADER_FILES "Header Files" )
		SRC_GRP(
			SUBGROUP "SDL2 Header Files"
			SOURCES
			"${SDL2_INCLUDE_DIR}/SDL2/begin_code.h"
			"${SDL2_INCLUDE_DIR}/SDL2/close_code.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_assert.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_atomic.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_audio.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_bits.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_blendmode.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_clipboard.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_config.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_config_android.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_config_iphoneos.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_config_macosx.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_config_minimal.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_config_nintendods.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_config_pandora.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_config_windows.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_config_wiz.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_copying.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_cpuinfo.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_endian.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_error.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_events.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_gamecontroller.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_gesture.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_haptic.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_hints.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_input.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_joystick.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_keyboard.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_keycode.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_loadso.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_log.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_main.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_messagebox.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_mouse.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_mutex.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_name.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_opengl.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_opengles.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_opengles2.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_pixels.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_platform.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_power.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_quit.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_rect.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_render.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_revision.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_rwops.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_scancode.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_shape.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_stdinc.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_surface.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_system.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_syswm.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test_assert.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test_common.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test_compare.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test_crc32.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test_font.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test_fuzzer.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test_harness.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test_images.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test_log.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test_md5.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_test_random.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_thread.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_timer.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_touch.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_types.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_version.h"
			"${SDL2_INCLUDE_DIR}/SDL2/SDL_video.h"
		)
	END_SRC( SDL2_HEADER_FILES "Header Files" )

	target_sources( ${target} PRIVATE ${SDL2_HEADER_FILES} )
	target_include_directories( ${target} PRIVATE "${SDL2_INCLUDE_DIR}" )
	target_link_directories( ${target} PRIVATE "${SRCDIR}/external/SDL2/lib" )
	target_link_libraries( ${target} PRIVATE SDL2 )
endfunction()


### STEAM API

function( target_use_steam_api target )
	set( STEAM_API_INCLUDE_DIR "${SRCDIR}/public/steam" )

	set( STEAM_API_HEADER_FILES )
	BEGIN_SRC( STEAM_API_HEADER_FILES "Header Files" )
		SRC_GRP(
			SUBGROUP "SteamAPI Header Files"
			SOURCES
			"${STEAM_API_INCLUDE_DIR}/friends/IFriendsUser.h"
			"${STEAM_API_INCLUDE_DIR}/isteamapps.h"
			"${STEAM_API_INCLUDE_DIR}/isteamclient.h"
			"${STEAM_API_INCLUDE_DIR}/isteamcontroller.h"
			"${STEAM_API_INCLUDE_DIR}/isteamfriends.h"
			"${STEAM_API_INCLUDE_DIR}/isteamgameserver.h"
			"${STEAM_API_INCLUDE_DIR}/isteamgameserverstats.h"
			"${STEAM_API_INCLUDE_DIR}/isteamhttp.h"
			"${STEAM_API_INCLUDE_DIR}/isteammatchmaking.h"
			"${STEAM_API_INCLUDE_DIR}/isteamnetworking.h"
			"${STEAM_API_INCLUDE_DIR}/isteamremotestorage.h"
			"${STEAM_API_INCLUDE_DIR}/isteamscreenshots.h"
			"${STEAM_API_INCLUDE_DIR}/isteamunifiedmessages.h"
			"${STEAM_API_INCLUDE_DIR}/isteamuser.h"
			"${STEAM_API_INCLUDE_DIR}/isteamuserstats.h"
			"${STEAM_API_INCLUDE_DIR}/isteamutils.h"
			"${STEAM_API_INCLUDE_DIR}/matchmakingtypes.h"
			"${STEAM_API_INCLUDE_DIR}/steam2compat.h"
			"${STEAM_API_INCLUDE_DIR}/steamclientpublic.h"
			"${STEAM_API_INCLUDE_DIR}/steamhttpenums.h"
			"${STEAM_API_INCLUDE_DIR}/steamtypes.h"
			"${STEAM_API_INCLUDE_DIR}/steam_api.h"
			"${STEAM_API_INCLUDE_DIR}/steam_gameserver.h"
		)
	END_SRC( STEAM_API_HEADER_FILES "Header Files" )

	target_sources( ${target} PRIVATE ${STEAM_API_HEADER_FILES} )
	target_include_directories( ${target} PRIVATE "${STEAM_API_INCLUDE_DIR}" )
	target_link_libraries( ${target} PRIVATE steam_api ) # TODO(SanyaSho): 64bit support
endfunction()


### BZIP2

set( BZ_VERSION "1.1.0" )
configure_file( "${SRCDIR}/external/bzip2/bz_version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/bz_version.h" )
add_library( bz2 STATIC "${SRCDIR}/external/bzip2/blocksort.c" "${SRCDIR}/external/bzip2/huffman.c" "${SRCDIR}/external/bzip2/crctable.c" "${SRCDIR}/external/bzip2/randtable.c" "${SRCDIR}/external/bzip2/compress.c" "${SRCDIR}/external/bzip2/decompress.c" "${SRCDIR}/external/bzip2/bzlib.c" )
set_property( TARGET bz2 PROPERTY FOLDER "3rdparty Libraries" )
target_include_directories( bz2 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" ) # Include "bz_version.h" from project build folder

function( target_use_bz2 target )
	set( BZIP2_INCLUDE_DIR "${SRCDIR}/external/bzip2" )

	set( BZIP2_HEADER_FILES )
	BEGIN_SRC( BZIP2_HEADER_FILES "Header Files" )
		SRC_GRP(
			SUBGROUP "BZ2 Header Files"
			SOURCES
			"${BZIP2_INCLUDE_DIR}/bzlib.h"
			"${BZIP2_INCLUDE_DIR}/bzlib_private.h"
		)
	END_SRC( BZIP2_HEADER_FILES "Header Files" )

	target_sources( ${target} PRIVATE ${BZIP2_HEADER_FILES} )
	target_include_directories( ${target} PRIVATE "${BZIP2_INCLUDE_DIR}" )
	target_link_libraries( ${target} PRIVATE bz2 )
endfunction()