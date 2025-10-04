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


### GLEW

function( target_use_glew target )
	set( GLEW_INCLUDE_DIR "${SRCDIR}/external/GLEW/include" )

	set( GLEW_HEADER_FILES )
	BEGIN_SRC( GLEW_HEADER_FILES "Header Files" )
		SRC_GRP(
			SUBGROUP "GLEW Header Files"
			SOURCES
			"${GLEW_INCLUDE_DIR}/GL/eglew.h"
			"${GLEW_INCLUDE_DIR}/GL/glew.h"
			"${GLEW_INCLUDE_DIR}/GL/glxew.h"
			"${GLEW_INCLUDE_DIR}/GL/wglew.h"
		)
	END_SRC( GLEW_HEADER_FILES "Header Files" )

	target_sources( ${target} PRIVATE ${GLEW_HEADER_FILES} )
	target_include_directories( ${target} PRIVATE "${GLEW_INCLUDE_DIR}" )
	target_link_directories( ${target} PRIVATE "${SRCDIR}/external/GLEW/lib" )
	target_link_libraries( ${target} PRIVATE $<${IS_WINDOWS}:glew32s> $<${IS_POSIX}:GLEW> )
endfunction()


### OGG, Vorbis, VPX, WebM

function( target_use_webm target ) # TODO(SanyaSho): Linux support
	set( WEBM_VIDEO_PRECOMP_DIR "${SRCDIR}/external/webm_video/build_win32" )
	set( WEBM_VIDEO_INCLUDE_DIR "${WEBM_VIDEO_PRECOMP_DIR}/include" )
	set( WEBM_VIDEO_LIB_DIR "${WEBM_VIDEO_PRECOMP_DIR}/lib" )

	set( OGG_INCLUDE_DIR "${SRCDIR}/external/webm_video/libogg/include" )
	set( OGG_EXTRA_INCLUDE_DIR "${WEBM_VIDEO_INCLUDE_DIR}/libogg" )
	set( VORBIS_INCLUDE_DIR "${SRCDIR}/external/webm_video/libvorbis/include" )
	set( VPX_INCLUDE_DIR "${SRCDIR}/external/webm_video/libvpx" )
	set( VPX_EXTRA_INCLUDE_DIR "${WEBM_VIDEO_INCLUDE_DIR}/libvpx" )
	set( WEBM_INCLUDE_DIR "${SRCDIR}/external/webm_video/libwebm" )

	set( OGG_HEADER_FILES )
	BEGIN_SRC( OGG_HEADER_FILES "Header Files" )
		SRC_GRP(
			SUBGROUP "OGG Header Files"
			SOURCES
			"${OGG_INCLUDE_DIR}/ogg/ogg.h"
			"${OGG_INCLUDE_DIR}/ogg/os_types.h"

			"${OGG_EXTRA_INCLUDE_DIR}/ogg/config_types.h"
		)
	END_SRC( OGG_HEADER_FILES "Header Files" )

	set( VORBIX_HEADER_FILES )
	BEGIN_SRC( VORBIX_HEADER_FILES "Header Files" )
		SRC_GRP(
			SUBGROUP "Vorbis Header Files"
			SOURCES
			"${VORBIS_INCLUDE_DIR}/vorbis/codec.h"
			"${VORBIS_INCLUDE_DIR}/vorbis/vorbisenc.h"
			"${VORBIS_INCLUDE_DIR}/vorbis/vorbisfile.h"
		)
	END_SRC( VORBIX_HEADER_FILES "Header Files" )

	set( VPX_HEADER_FILES )
	BEGIN_SRC( VPX_HEADER_FILES "Header Files" )
		SRC_GRP(
			SUBGROUP "VPX Header Files"
			SOURCES
			"${VPX_INCLUDE_DIR}/args.h"
			"${VPX_INCLUDE_DIR}/ivfdec.h"
			"${VPX_INCLUDE_DIR}/ivfenc.h"
			"${VPX_INCLUDE_DIR}/md5_utils.h"
			"${VPX_INCLUDE_DIR}/rate_hist.h"
			"${VPX_INCLUDE_DIR}/tools_common.h"
			"${VPX_INCLUDE_DIR}/video_common.h"
			"${VPX_INCLUDE_DIR}/video_reader.h"
			"${VPX_INCLUDE_DIR}/video_writer.h"
			"${VPX_INCLUDE_DIR}/vp8/common/alloccommon.h"
			"${VPX_INCLUDE_DIR}/vp8/common/arm/loopfilter_arm.h"
			"${VPX_INCLUDE_DIR}/vp8/common/blockd.h"
			"${VPX_INCLUDE_DIR}/vp8/common/coefupdateprobs.h"
			"${VPX_INCLUDE_DIR}/vp8/common/common.h"
			"${VPX_INCLUDE_DIR}/vp8/common/default_coef_probs.h"
			"${VPX_INCLUDE_DIR}/vp8/common/entropy.h"
			"${VPX_INCLUDE_DIR}/vp8/common/entropymode.h"
			"${VPX_INCLUDE_DIR}/vp8/common/entropymv.h"
			"${VPX_INCLUDE_DIR}/vp8/common/extend.h"
			"${VPX_INCLUDE_DIR}/vp8/common/filter.h"
			"${VPX_INCLUDE_DIR}/vp8/common/findnearmv.h"
			"${VPX_INCLUDE_DIR}/vp8/common/header.h"
			"${VPX_INCLUDE_DIR}/vp8/common/invtrans.h"
			"${VPX_INCLUDE_DIR}/vp8/common/loopfilter.h"
			"${VPX_INCLUDE_DIR}/vp8/common/mips/msa/vp8_macros_msa.h"
			"${VPX_INCLUDE_DIR}/vp8/common/modecont.h"
			"${VPX_INCLUDE_DIR}/vp8/common/mv.h"
			"${VPX_INCLUDE_DIR}/vp8/common/onyx.h"
			"${VPX_INCLUDE_DIR}/vp8/common/onyxc_int.h"
			"${VPX_INCLUDE_DIR}/vp8/common/onyxd.h"
			"${VPX_INCLUDE_DIR}/vp8/common/postproc.h"
			"${VPX_INCLUDE_DIR}/vp8/common/ppflags.h"
			"${VPX_INCLUDE_DIR}/vp8/common/quant_common.h"
			"${VPX_INCLUDE_DIR}/vp8/common/reconinter.h"
			"${VPX_INCLUDE_DIR}/vp8/common/reconintra.h"
			"${VPX_INCLUDE_DIR}/vp8/common/reconintra4x4.h"
			"${VPX_INCLUDE_DIR}/vp8/common/setupintrarecon.h"
			"${VPX_INCLUDE_DIR}/vp8/common/swapyv12buffer.h"
			"${VPX_INCLUDE_DIR}/vp8/common/systemdependent.h"
			"${VPX_INCLUDE_DIR}/vp8/common/threading.h"
			"${VPX_INCLUDE_DIR}/vp8/common/treecoder.h"
			"${VPX_INCLUDE_DIR}/vp8/common/vp8_entropymodedata.h"
			"${VPX_INCLUDE_DIR}/vp8/common/vp8_skin_detection.h"
			"${VPX_INCLUDE_DIR}/vp8/decoder/dboolhuff.h"
			"${VPX_INCLUDE_DIR}/vp8/decoder/decodemv.h"
			"${VPX_INCLUDE_DIR}/vp8/decoder/decoderthreading.h"
			"${VPX_INCLUDE_DIR}/vp8/decoder/detokenize.h"
			"${VPX_INCLUDE_DIR}/vp8/decoder/ec_types.h"
			"${VPX_INCLUDE_DIR}/vp8/decoder/error_concealment.h"
			"${VPX_INCLUDE_DIR}/vp8/decoder/onyxd_int.h"
			"${VPX_INCLUDE_DIR}/vp8/decoder/treereader.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/bitstream.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/block.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/boolhuff.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/dct_value_cost.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/dct_value_tokens.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/defaultcoefcounts.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/denoising.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/encodeframe.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/encodeintra.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/encodemb.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/encodemv.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/ethreading.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/firstpass.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/lookahead.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/mcomp.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/modecosts.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/mr_dissim.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/onyx_int.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/pickinter.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/picklpf.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/quantize.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/ratectrl.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/rdopt.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/segmentation.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/temporal_filter.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/tokenize.h"
			"${VPX_INCLUDE_DIR}/vp8/encoder/treewriter.h"
			"${VPX_INCLUDE_DIR}/vp8/vp8_ratectrl_rtc.h"
			"${VPX_INCLUDE_DIR}/vp9/common/arm/neon/vp9_iht_neon.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_alloccommon.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_blockd.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_common.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_common_data.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_entropy.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_entropymode.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_entropymv.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_enums.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_filter.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_frame_buffers.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_idct.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_loopfilter.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_mfqe.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_mv.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_mvref_common.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_onyxc_int.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_postproc.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_ppflags.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_pred_common.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_quant_common.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_reconinter.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_reconintra.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_scale.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_scan.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_seg_common.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_thread_common.h"
			"${VPX_INCLUDE_DIR}/vp9/common/vp9_tile_common.h"
			"${VPX_INCLUDE_DIR}/vp9/decoder/vp9_decodeframe.h"
			"${VPX_INCLUDE_DIR}/vp9/decoder/vp9_decodemv.h"
			"${VPX_INCLUDE_DIR}/vp9/decoder/vp9_decoder.h"
			"${VPX_INCLUDE_DIR}/vp9/decoder/vp9_detokenize.h"
			"${VPX_INCLUDE_DIR}/vp9/decoder/vp9_dsubexp.h"
			"${VPX_INCLUDE_DIR}/vp9/decoder/vp9_job_queue.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/mips/msa/vp9_fdct_msa.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_alt_ref_aq.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_aq_360.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_aq_complexity.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_aq_cyclicrefresh.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_aq_variance.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_bitstream.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_block.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_blockiness.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_context_tree.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_cost.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_denoiser.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_encodeframe.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_encodemb.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_encodemv.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_encoder.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_ethread.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_extend.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_ext_ratectrl.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_firstpass.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_firstpass_stats.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_job_queue.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_lookahead.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_mbgraph.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_mcomp.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_multi_thread.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_noise_estimate.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_non_greedy_mv.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_partition_models.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_picklpf.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_pickmode.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_quantize.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_ratectrl.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_rd.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_rdopt.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_resize.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_segmentation.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_skin_detection.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_speed_features.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_subexp.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_svc_layercontext.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_temporal_filter.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_temporal_filter_constants.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_tokenize.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_tpl_model.h"
			"${VPX_INCLUDE_DIR}/vp9/encoder/vp9_treewriter.h"
			"${VPX_INCLUDE_DIR}/vp9/ratectrl_rtc.h"
			"${VPX_INCLUDE_DIR}/vp9/simple_encode.h"
			"${VPX_INCLUDE_DIR}/vp9/vp9_cx_iface.h"
			"${VPX_INCLUDE_DIR}/vp9/vp9_dx_iface.h"
			"${VPX_INCLUDE_DIR}/vp9/vp9_iface_common.h"
			"${VPX_INCLUDE_DIR}/vpx/internal/vpx_codec_internal.h"
			"${VPX_INCLUDE_DIR}/vpx/internal/vpx_ratectrl_rtc.h"
			"${VPX_INCLUDE_DIR}/vpx/vp8.h"
			"${VPX_INCLUDE_DIR}/vpx/vp8cx.h"
			"${VPX_INCLUDE_DIR}/vpx/vp8dx.h"
			"${VPX_INCLUDE_DIR}/vpx/vpx_codec.h"
			"${VPX_INCLUDE_DIR}/vpx/vpx_decoder.h"
			"${VPX_INCLUDE_DIR}/vpx/vpx_encoder.h"
			"${VPX_INCLUDE_DIR}/vpx/vpx_ext_ratectrl.h"
			"${VPX_INCLUDE_DIR}/vpx/vpx_frame_buffer.h"
			"${VPX_INCLUDE_DIR}/vpx/vpx_image.h"
			"${VPX_INCLUDE_DIR}/vpx/vpx_integer.h"
			"${VPX_INCLUDE_DIR}/vpx/vpx_tpl.h"
			"${VPX_INCLUDE_DIR}/vpxenc.h"
			"${VPX_INCLUDE_DIR}/vpxstats.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/fdct16x16_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/fdct32x32_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/fdct4x4_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/fdct8x8_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/fdct_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/highbd_convolve8_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/highbd_convolve8_sve.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/highbd_idct_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/idct_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/mem_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/sum_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/transpose_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/vpx_convolve8_neon.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/vpx_convolve8_neon_asm.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/vpx_neon_sve2_bridge.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/arm/vpx_neon_sve_bridge.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/bitreader.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/bitreader_buffer.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/bitwriter.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/bitwriter_buffer.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/fwd_txfm.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/inv_txfm.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/loongarch/bitdepth_conversion_lsx.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/loongarch/fwd_txfm_lsx.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/loongarch/loopfilter_lsx.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/loongarch/txfm_macros_lsx.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/loongarch/variance_lsx.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/loongarch/vpx_convolve_lsx.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/common_dspr2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/convolve_common_dspr2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/fwd_txfm_msa.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/inv_txfm_dspr2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/inv_txfm_msa.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/loopfilter_filters_dspr2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/loopfilter_macros_dspr2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/loopfilter_masks_dspr2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/loopfilter_msa.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/macros_msa.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/txfm_macros_msa.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/mips/vpx_convolve_msa.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/postproc.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/ppc/bitdepth_conversion_vsx.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/ppc/inv_txfm_vsx.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/ppc/transpose_vsx.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/ppc/txfm_common_vsx.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/ppc/types_vsx.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/prob.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/psnr.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/quantize.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/skin_detection.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/ssim.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/txfm_common.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/variance.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/vpx_convolve.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/vpx_dsp_common.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/vpx_filter.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/bitdepth_conversion_avx2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/bitdepth_conversion_sse2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/convolve.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/convolve_avx2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/convolve_sse2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/convolve_ssse3.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/fwd_dct32x32_impl_avx2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/fwd_dct32x32_impl_sse2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/fwd_txfm_impl_sse2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/fwd_txfm_sse2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/highbd_inv_txfm_sse2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/highbd_inv_txfm_sse4.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/inv_txfm_sse2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/inv_txfm_ssse3.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/mem_sse2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/quantize_sse2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/quantize_ssse3.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/transpose_sse2.h"
			"${VPX_INCLUDE_DIR}/vpx_dsp/x86/txfm_common_sse2.h"
			"${VPX_INCLUDE_DIR}/vpx_mem/include/vpx_mem_intrnl.h"
			"${VPX_INCLUDE_DIR}/vpx_mem/vpx_mem.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/arm.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/arm_cpudetect.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/asmdefs_mmi.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/bitops.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/compiler_attributes.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/emmintrin_compat.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/loongarch.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/mem.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/mem_ops.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/mem_ops_aligned.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/mips.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/ppc.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/static_assert.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/system_state.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/vpx_once.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/vpx_timer.h"
			"${VPX_INCLUDE_DIR}/vpx_ports/x86.h"
			"${VPX_INCLUDE_DIR}/vpx_scale/vpx_scale.h"
			"${VPX_INCLUDE_DIR}/vpx_scale/yv12config.h"
			"${VPX_INCLUDE_DIR}/vpx_util/endian_inl.h"
			"${VPX_INCLUDE_DIR}/vpx_util/loongson_intrinsics.h"
			"${VPX_INCLUDE_DIR}/vpx_util/vpx_atomics.h"
			"${VPX_INCLUDE_DIR}/vpx_util/vpx_debug_util.h"
			"${VPX_INCLUDE_DIR}/vpx_util/vpx_pthread.h"
			"${VPX_INCLUDE_DIR}/vpx_util/vpx_thread.h"
			"${VPX_INCLUDE_DIR}/vpx_util/vpx_timestamp.h"
			"${VPX_INCLUDE_DIR}/vpx_util/vpx_write_yuv_frame.h"
			"${VPX_INCLUDE_DIR}/warnings.h"
			"${VPX_INCLUDE_DIR}/webmdec.h"
			"${VPX_INCLUDE_DIR}/webmenc.h"
			"${VPX_INCLUDE_DIR}/y4menc.h"
			"${VPX_INCLUDE_DIR}/y4minput.h"

			"${VPX_EXTRA_INCLUDE_DIR}/vp8_rtcd.h"
			"${VPX_EXTRA_INCLUDE_DIR}/vp9_rtcd.h"
			"${VPX_EXTRA_INCLUDE_DIR}/vpx_config.h"
			"${VPX_EXTRA_INCLUDE_DIR}/vpx_dsp_rtcd.h"
			"${VPX_EXTRA_INCLUDE_DIR}/vpx_scale_rtcd.h"
			"${VPX_EXTRA_INCLUDE_DIR}/vpx_version.h"
		)
	END_SRC( VPX_HEADER_FILES "Header Files" )

	set( WEBM_HEADER_FILES )
	BEGIN_SRC( WEBM_HEADER_FILES "Header Files" )
		SRC_GRP(
			SUBGROUP "WebM Header Files"
			SOURCES
			"${WEBM_INCLUDE_DIR}/common/file_util.h"
			"${WEBM_INCLUDE_DIR}/common/hdr_util.h"
			"${WEBM_INCLUDE_DIR}/common/indent.h"
			"${WEBM_INCLUDE_DIR}/common/libwebm_util.h"
			"${WEBM_INCLUDE_DIR}/common/video_frame.h"
			"${WEBM_INCLUDE_DIR}/common/vp9_header_parser.h"
			"${WEBM_INCLUDE_DIR}/common/vp9_level_stats.h"
			"${WEBM_INCLUDE_DIR}/common/webmids.h"
			"${WEBM_INCLUDE_DIR}/common/webm_constants.h"
			"${WEBM_INCLUDE_DIR}/common/webm_endian.h"
			"${WEBM_INCLUDE_DIR}/m2ts/vpxpes2ts.h"
			"${WEBM_INCLUDE_DIR}/m2ts/vpxpes_parser.h"
			"${WEBM_INCLUDE_DIR}/m2ts/webm2pes.h"
			"${WEBM_INCLUDE_DIR}/mkvmuxer/mkvmuxer.h"
			"${WEBM_INCLUDE_DIR}/mkvmuxer/mkvmuxertypes.h"
			"${WEBM_INCLUDE_DIR}/mkvmuxer/mkvmuxerutil.h"
			"${WEBM_INCLUDE_DIR}/mkvmuxer/mkvwriter.h"
			"${WEBM_INCLUDE_DIR}/mkvparser/mkvparser.h"
			"${WEBM_INCLUDE_DIR}/mkvparser/mkvreader.h"
			"${WEBM_INCLUDE_DIR}/sample_muxer_metadata.h"
			"${WEBM_INCLUDE_DIR}/testing/test_util.h"
			"${WEBM_INCLUDE_DIR}/vttreader.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/include/webm/buffer_reader.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/include/webm/callback.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/include/webm/dom_types.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/include/webm/element.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/include/webm/file_reader.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/include/webm/id.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/include/webm/istream_reader.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/include/webm/reader.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/include/webm/status.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/include/webm/webm_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/ancestory.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/audio_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/bit_utils.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/block_additions_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/block_group_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/block_header_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/block_more_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/block_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/bool_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/byte_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/chapters_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/chapter_atom_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/chapter_display_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/cluster_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/colour_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/content_encodings_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/content_encoding_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/content_encryption_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/content_enc_aes_settings_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/cues_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/cue_point_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/cue_track_positions_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/date_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/ebml_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/edition_entry_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/element_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/float_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/id_element_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/id_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/info_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/int_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/mastering_metadata_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/master_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/master_value_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/parser_utils.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/projection_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/recursive_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/seek_head_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/seek_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/segment_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/simple_tag_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/size_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/skip_callback.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/skip_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/slices_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/tags_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/tag_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/targets_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/time_slice_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/tracks_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/track_entry_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/unknown_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/var_int_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/video_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/virtual_block_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/src/void_parser.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/test_utils/element_parser_test.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/test_utils/limited_reader.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/test_utils/mock_callback.h"
			"${WEBM_INCLUDE_DIR}/webm_parser/test_utils/parser_test.h"
			"${WEBM_INCLUDE_DIR}/webvtt/vttreader.h"
			"${WEBM_INCLUDE_DIR}/webvtt/webvttparser.h"
			"${WEBM_INCLUDE_DIR}/webvttparser.h"
		)
	END_SRC( WEBM_HEADER_FILES "Header Files" )

	target_sources( ${target} PRIVATE ${OGG_HEADER_FILES} ${VORBIX_HEADER_FILES} ${VPX_HEADER_FILES} ${WEBM_HEADER_FILES} )
	target_include_directories( ${target} PRIVATE "${WEBM_VIDEO_INCLUDE_DIR}" "${OGG_INCLUDE_DIR}" "${OGG_EXTRA_INCLUDE_DIR}" "${VORBIS_INCLUDE_DIR}" "${VPX_INCLUDE_DIR}" "${VPX_EXTRA_INCLUDE_DIR}" "${WEBM_INCLUDE_DIR}" )
	target_link_directories( ${target} PRIVATE "$<$<CONFIG:Release>:${WEBM_VIDEO_LIB_DIR}/release>" "$<$<CONFIG:Debug>:${WEBM_VIDEO_LIB_DIR}/debug>" )
	target_link_libraries( ${target} PRIVATE ogg vorbis $<$<CONFIG:Release>:vpxmd> $<$<CONFIG:Debug>:vpxmdd> webm )
endfunction()

