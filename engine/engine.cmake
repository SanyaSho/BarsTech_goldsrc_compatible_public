#-----------------------------------------------------------------------------
# ENGINE.CMAKE
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

include_guard( GLOBAL )

is_feature_enabled( HL25 HL25 )
is_feature_enabled( HL25_WEBM_PLAYER HL25_WEBM_PLAYER )

set( ENGINE_SOURCE_FILES )
BEGIN_SRC( ENGINE_SOURCE_FILES "Source Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/common/BaseSystemModule.cpp"
		"${SRCDIR}/common/FilePaths.cpp"
		"${SRCDIR}/common/ObjectList.cpp"
		"${SRCDIR}/common/TokenLine.cpp"
		"cl_extrap.cpp"
		"download.cpp"
		"bitmap_win.cpp"
		"buildnum.cpp"
		"cd.cpp"
		"cdll_exp.cpp"
		"cdll_int.cpp"
		"chase.cpp"
		"CircularBuffer.cpp"
		"cl_demo.cpp"
		"cl_draw.cpp"
		"cl_ents.cpp"
		"cl_main.cpp"
		"cl_parse.cpp"
		"cl_parsefn.cpp"
		"cl_pmove.cpp"
		"cl_pred.cpp"
		"cl_spectator.cpp"
		"cl_tent.cpp"
		"cmd.cpp"
		"cmodel.cpp"
		"common.cpp"
		"com_custom.cpp"
		"console.cpp"
		"crc.cpp"
		"cvar.cpp"
		"decals.cpp"
		"delta.cpp" # Set opt level to MaxSpeed
		"DemoPlayerWrapper.cpp"
		"eventapi.cpp"
		"filesystem.cpp"
		"filesystem_internal.cpp"
		"hashpak.cpp"
		"host.cpp"
		"host_cmd.cpp"
		"HUD.cpp"
		"info.cpp"
		"dinput.cpp"
		"ipratelimit.cpp"
		"ipratelimitWrapper.cpp"
		"keys.cpp"
		"LoadBlob.cpp"
		"l_studio.cpp"
		"mathlib.cpp"
		"mem.cpp"
		"module.cpp"
		"net_api.cpp"
		"net_chan.cpp"
		"net_ws.cpp"
		"pe_win32.cpp"
		"pmove.cpp"
		"pmovetst.cpp"
		"pr_cmds.cpp"
		"pr_edict.cpp"
		"r_efx.cpp"
		"r_part.cpp"
		"r_studio.cpp" # (Don't optimize in Debug)
		"r_trans.cpp"
		"r_triangle.cpp"
		"Sequence.cpp"
		"snd_dma.cpp"
		"snd_mem.cpp"
		"snd_mix.cpp"
		"snd_sdl.cpp"
		"sv_remoteaccess.cpp"
		"sv_user.cpp"
		"sv_log.cpp"
		"sv_main.cpp"
		"sv_phys.cpp"
		"sv_secure.cpp"
		"sv_steam3.cpp"
		"sv_upld.cpp"
		"SystemWrapper.cpp"
		"sys_dll.cpp" # BasicRuntimeChecks: EnableFastChecks; BufferSecurityCheck: true
		"sys_dll2.cpp"
		"sys_engine.cpp"
		"sys_getmodes.cpp"
		"sys_sdlwind.cpp"
		"textures.cpp"
		"thread.cpp"
		"tmessage.cpp"
		"traceinit.cpp"
		"vgui2/BaseUISurface.cpp"
		"vgui2/BaseUISurface_Font.cpp"
		"vgui2/BaseUI_Interface.cpp"
		"vgui2/FontTextureCache.cpp"
		"vgui2/text_draw.cpp"
		"vgui_EngineSurface.cpp"
		"VGUI_EngineSurfaceWrap.cpp"
		"vgui_int.cpp"
		"vgui_intwrap.cpp"
		"vgui_intwrap2.cpp"
		"view.cpp"
		"voice.cpp"
		"voice_gain.cpp"
		"voice_mixer_controls.cpp"
		"voice_record_dsound.cpp"
		"voice_record_wavein.cpp"
		"voice_sound_engine_interface.cpp"
		"voice_wavefile.cpp"
		"wad.cpp"
		"world.cpp"
		"$<${HL25_WEBM_PLAYER}:yuv2rgb.cpp>"
		"zone.cpp"
		"${SRCDIR}/public/interface.cpp"
		"${SRCDIR}/public/registry.cpp"
		"${SRCDIR}/public/steamid.cpp"
		"${SRCDIR}/public/sys_procinfo.cpp"
		"${SRCDIR}/public/vgui_controls/vgui_controls.cpp"
		"snd_win.cpp"
		"sv_move.cpp"
		"sv_pmove.cpp"
		"$<${HL25_WEBM_PLAYER}:webm_video.cpp>"

		"engine.rc"
	)
END_SRC( ENGINE_SOURCE_FILES "Source Files" )

set( ENGINE_HEADER_FILES )
BEGIN_SRC( ENGINE_HEADER_FILES "Header Files" )
	SRC_GRP(
		SOURCES
		"adivtab.h"
		"anorms.h"
		"anorm_dots.h"
		"APIProxy.h"
		"bitmap_win.h"
		"buildnum.h"
		"cd.h"
		"cdaudio.h"
		"cdll_exp.h"
		"cdll_int.h"
		"cd_internal.h"
		"chase.h"
		"CircularBuffer.h"
		"client.h"
		"cl_demo.h"
		"cl_draw.h"
		"cl_ents.h"
		"cl_extrap.h"
		"cl_main.h"
		"cl_parse.h"
		"cl_parsefn.h"
		"cl_pmove.h"
		"cl_pred.h"
		"cl_spectator.h"
		"cl_tent.h"
		"cmd.h"
		"cmodel.h"
		"color.h"
		"common.h"
		"com_custom.h"
		"consistency.h"
		"console.h"
		"custom.h"
		"customentity.h"
		"cvar.h"
		"decals.h"
		"delta.h"
		"DemoPlayerWrapper.h"
		"download.h"
		"edict.h"
		"eiface.h"
		"eventapi.h"
		"filesystem.h"
		"hashpak.h"
		"host.h"
		"host_cmd.h"
		"IEngine.h"
		"IGame.h"
		"info.h"
		"inst_baseline.h"
		"ipratelimit.h"
		"ipratelimitWrapper.h"
		"ithread.h"
		"keys.h"
		"LoadBlob.h"
		"mathlib.h"
		"mem.h"
		"modelgen.h"
		"modinfo.h"
		"module.h"
		"net.h"
		"net_api_int.h"
		"net_chan.h"
		"net_ws.h"
		"pmove.h"
		"pmovetst.h"
		"progdefs.h"
		"progs.h"
		"protocol.h"
		"pr_cmds.h"
		"pr_edict.h"
		"quakedef.h"
		"render.h"
		"sbar.h"
		"server.h"
		"shake.h"
		"sound.h"
		"spritegn.h"
		"studio.h"
		"sv_log.h"
		"sv_main.h"
		"sv_move.h"
		"sv_phys.h"
		"sv_pmove.h"
		"sv_remoteaccess.h"
		"sv_secure.h"
		"sv_steam3.h"
		"sv_upld.h"
		"sv_user.h"
		"sys.h"
		"SystemWrapper.h"
		"sys_getmodes.h"
		"textures.h"
		"tmessage.h"
		"traceinit.h"
		"userid.h"
		"vgui2/BasePanel.h"
		"vgui2/BaseUISurface.h"
		"vgui2/BaseUISurface_Font.h"
		"vgui2/BaseUI_Interface.h"
		"vgui2/FontTextureCache.h"
		"vgui2/IMouseControl.h"
		"vgui2/text_draw.h"
		"vgui_EngineSurface.h"
		"VGUI_EngineSurfaceWrap.h"
		"vgui_int.h"
		"vid.h"
		"view.h"
		"voice.h"
		"voice_gain.h"
		"voice_mixer_controls.h"
		"voice_sound_engine_interface.h"
		"voice_wavefile.h"
		"wad.h"
		"$<${HL25_WEBM_PLAYER}:webm_video.h>"
		"winquake.h"
		"world.h"
		"$<${HL25_WEBM_PLAYER}:yuv2rgb.h>"
		"zone.h"
	)
END_SRC( ENGINE_HEADER_FILES "Header Files" )

set( ENGINE_HW_SOURCE_FILES )
BEGIN_SRC( ENGINE_HW_SOURCE_FILES "Source Files" )
	SRC_GRP(
		SUBGROUP "HW Source Files"
		SOURCES
		"DetailTexture.cpp" # GL
		"glHud.cpp" # GL
		"glide2x.cpp" # GL
		"gl_draw.cpp" # GL
		"gl_mesh.cpp" # GL
		"gl_model.cpp" # GL
		"gl_refrag.cpp" # GL
		"gl_rlight.cpp" # GL
		"gl_rmain.cpp" # GL
		"gl_rmisc.cpp" # GL
		"gl_rsurf.cpp" # GL
		"gl_screen.cpp" # GL
		"gl_vidnt.cpp" # GL
		"gl_warp.cpp" # GL
		"qgl.cpp" # GL
		"vgui_EngineSurfaceHW.cpp" # GL
	)
END_SRC( ENGINE_HW_SOURCE_FILES "Source Files" )

set( ENGINE_HW_HEADER_FILES )
BEGIN_SRC( ENGINE_HW_HEADER_FILES "Header Files" )
	SRC_GRP(
		SUBGROUP "HW Header Files"
		SOURCES
		"DetailTexture.h"
		"glHud.h"
		"glquake.h"
		"gl_draw.h"
		"gl_hw.h"
		"gl_mesh.h"
		"gl_model.h"
		"gl_refrag.h"
		"gl_rmain.h"
		"gl_rmisc.h"
		"gl_rsurf.h"
		"gl_screen.h"
		"gl_vidnt.h"
		"gl_warp_sin.h"
		"qgl.h"
	)
END_SRC( ENGINE_HW_HEADER_FILES "Header Files" )

set( ENGINE_SW_SOURCE_FILES )
BEGIN_SRC( ENGINE_SW_SOURCE_FILES "Source Files" )
	SRC_GRP(
		SUBGROUP "SW Source Files"
		SOURCES
		"draw.cpp" # !GL
		"d_fill.cpp" # !GL
		"d_modech.cpp" # !GL
		"d_part.cpp" # !GL
		"d_surf.cpp" # !GL
		"d_edge.cpp" # !GL
		"d_init.cpp" # !GL
		"d_polyse.cpp" # !GL (Don't optimize in Debug)
		"d_scan.cpp" # !GL
		"d_sprite.cpp" # !GL
		"d_vars.cpp" # !GL
		"model.cpp" # !GL
		"r_aclip.cpp" # !GL
		"r_alias.cpp" # !GL
		"r_bsp.cpp" # !GL
		"r_draw.cpp" # !GL
		"r_edge.cpp" # !GL
		"r_efrag.cpp" # !GL
		"r_light.cpp" # !GL
		"r_misc.cpp" # !GL
		"r_sky.cpp" # !GL
		"r_sprite.cpp" # !GL
		"r_surf.cpp" # !GL
		"r_vars.cpp" # !GL
		"sys_getmodesdd.cpp" # !GL
		"vgui_EngineSurfaceWin32.cpp" # !GL
		"r_main.cpp" # !GL
		"screen.cpp" # !GL
		"vid_win.cpp" # !GL
	)
END_SRC( ENGINE_SW_SOURCE_FILES "Source Files" )

set( ENGINE_SW_HEADER_FILES )
BEGIN_SRC( ENGINE_SW_HEADER_FILES "Header Files" )
	SRC_GRP(
		SUBGROUP "SW Header Files"
		SOURCES
		"draw.h"
		"d_iface.h"
		"d_local.h"
		"model.h"
		"r_efx_int.h"
		"r_local.h"
		"r_part.h"
		"r_shared.h"
		"r_studio.h"
		"r_trans.h"
		"r_triangle.h"
		"vgui_EngineSurfaceWin32.h"
		"screen.h"
	)
END_SRC( ENGINE_SW_HEADER_FILES "Header Files" )

function( add_engine )
	cmake_parse_arguments(
		ARGS
		"HW;SW"
		"TARGET"
		""
		${ARGN}
	)

	if ( NOT ARGS_HW AND NOT ARGS_SW )
		message( FATAL_ERROR "add_engine must have HW or SW set!" )
	endif()

	set( IS_HW $<BOOL:${ARGS_HW}> )
	set( IS_SW $<BOOL:${ARGS_SW}> )

	add_library( ${ARGS_TARGET} SHARED ${ENGINE_SOURCE_FILES} ${ENGINE_HEADER_FILES} $<${IS_HW}:${ENGINE_HW_SOURCE_FILES}> $<${IS_HW}:${ENGINE_HW_HEADER_FILES}> $<${IS_SW}:${ENGINE_SW_SOURCE_FILES}> $<${IS_SW}:${ENGINE_SW_HEADER_FILES}> )
	install_library( TARGET ${ARGS_TARGET} INSTALL_DEST "${GAMEDIR}" )
	set_property( TARGET ${ARGS_TARGET} PROPERTY FOLDER "Engine Libraries" )

	target_compile_definitions(
		${ARGS_TARGET} PRIVATE

		_2020_PATCH

		$<${IS_HW}:GLQUAKE>
	)

	target_include_directories(
		${ARGS_TARGET} PRIVATE

		"${SRCDIR}/engine"
		"${SRCDIR}"
		"${SRCDIR}/cl_dll"
		"${SRCDIR}/common"
		"${SRCDIR}/pm_shared"
		"${SRCDIR}/public"
		"${SRCDIR}/public/tier1"
		"${SRCDIR}/utils/vgui/include"

		"${SRCDIR}/external/miles"
	)

	target_link_libraries(
		${ARGS_TARGET} PRIVATE

		$<${IS_WINDOWS}:winmm>
		$<${IS_WINDOWS}:ws2_32>
		$<${IS_WINDOWS}:dinput>
		$<${IS_WINDOWS}:dxguid>
		$<${IS_WINDOWS}:ddraw>
		$<$<AND:${IS_WINDOWS},${IS_HW}>:opengl32>

		tier0
		tier1
		vstdlib
		vgui
		vgui2_controls
		vgui2_surfacelib

		mss32
		game_controls
	)

	if( IS_HW )
		target_use_glew( ${ARGS_TARGET} )
	endif()
	target_use_bz2( ${ARGS_TARGET} )
	target_use_steam_api( ${ARGS_TARGET} )
	target_use_sdl2( ${ARGS_TARGET} )
	if ( ${HL25_WEBM_PLAYER} )
		target_use_webm( ${ARGS_TARGET} )
	endif()

endfunction()
