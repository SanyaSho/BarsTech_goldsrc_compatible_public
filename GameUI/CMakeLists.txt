#-----------------------------------------------------------------------------
# GAMEUI.CMAKE
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

set( GAMEUI_SOURCE_FILES )
BEGIN_SRC( GAMEUI_SOURCE_FILES "Source Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/common/Random.cpp"

		"${SRCDIR}/game_shared/bot/bot_profile.cpp"
		"${SRCDIR}/game_shared/shared_util.cpp"

		"${SRCDIR}/public/interface.cpp"

		"BackgroundMenuButton.cpp"
		"BasePanel.cpp"
		"BitmapButton.cpp"
		"BitmapImagePanel.cpp"
		"CareerBaseFrame.cpp"
		"CareerBox.cpp"
		"CareerGame.cpp"
		"CareerGameTasks.cpp"
		"CareerMapFrame.cpp"
		"CareerProfile.cpp"
		"CareerProfileFrame.cpp"
		"CDKeyEntryDialog.cpp"
		"CommandCheckButton.cpp"
		"ContentControlDialog.cpp"
		"CreateMultiplayerGameBotPage.cpp"
		"CreateMultiplayerGameDialog.cpp"
		"CreateMultiplayerGameGameplayPage.cpp"
		"CreateMultiplayerGameServerPage.cpp"
		"CvarLatch.cpp"
		"CvarNegateCheckButton.cpp"
		"CvarSlider.cpp"
		"CvarTextEntry.cpp"
		"CvarToggleCheckButton.cpp"
		"DottedLabel.cpp"
		"GameConsole.cpp"
		"GameConsoleDialog.cpp"
		"GameUI_Interface.cpp"
		"HelpDialog.cpp"
		"ImageButton.cpp"
		"KeyToggleCheckButton.cpp"
		"LabeledCommandComboBox.cpp"
		"labelslidercombo.cpp"
		"LoadGameDialog.cpp"
		"LoadingDialog.cpp"
		"LogoFile.cpp"
		"ModInfo.cpp"
		"MultiplayerAdvancedDialog.cpp"
		"MusicManager.cpp"
		"NewGameDialog.cpp"
		"OptionsDialog.cpp"
		"OptionsSubAdvanced.cpp"
		"OptionsSubAudio.cpp"
		"OptionsSubKeyboard.cpp"
		"OptionsSubMouse.cpp"
		"OptionsSubMultiplayer.cpp"
		"OptionsSubVideo.cpp"
		"OptionsSubVoice.cpp"
		"PanelListPanel.cpp"
		"PlayerListDialog.cpp"
		"RefreshSteamLogin.cpp"
		"RunGameEngine.cpp"
		"SaveGameDialog.cpp"
		"ScriptObject.cpp"
		"Sys_Utils.cpp"
		"TaskButton.cpp"
		"Taskbar.cpp"
		"TextEntryBox.cpp"
		"vcontrolslistpanel.cpp"
		"VGuiSystemModuleLoader.cpp"

		"${SRCDIR}/common/randoverride.cpp"
	)
END_SRC( GAMEUI_SOURCE_FILES "Source Files" )

set( GAMEUI_HEADER_FILES )
BEGIN_SRC( GAMEUI_HEADER_FILES "Header Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/common/BaseUI/IBaseUI.h"
		"${SRCDIR}/common/GameUI/ICareerUI.h"
		"${SRCDIR}/common/GameUI/IClientVGUI.h"
		"${SRCDIR}/common/GameUI/IGameConsole.h"
		"${SRCDIR}/common/GameUI/IGameUI.h"
		"${SRCDIR}/common/GameUI/IMusicManager.h"
		"${SRCDIR}/common/ServerBrowser/IServerBrowser.h"
		"${SRCDIR}/common/IRunGameEngine.h"
		"${SRCDIR}/common/IVGuiModule.h"
		"${SRCDIR}/common/IVGuiModuleLoader.h"
		"${SRCDIR}/common/port.h"

		"${SRCDIR}/common/Random.h"
		"${SRCDIR}/cstrike/dlls/weapontype.h"
		"${SRCDIR}/game_shared/bot/bot_profile.h"
		"${SRCDIR}/game_shared/shared_util.h"
		"${SRCDIR}/game_shared/simple_checksum.h"
		"${SRCDIR}/public/steam/friends/IFriendsUser.h"
		"${SRCDIR}/public/igameuifuncs.h"
		"${SRCDIR}/public/interface.h"
		"${SRCDIR}/public/savegame_version.h"

		"${SRCDIR}/common/ifilesystemopendialog.h"

		"${SRCDIR}/common/xbox/xboxstubs.h"

		"${SRCDIR}/public/basetypes.h"
		"${SRCDIR}/public/commonmacros.h"
		"${SRCDIR}/public/coordsize.h"
		"${SRCDIR}/public/datamap.h"
		"${SRCDIR}/public/FileSystem_Helpers.cpp"
		"${SRCDIR}/public/FileSystem_Helpers.h"
		"${SRCDIR}/public/ICVar.h"
		"${SRCDIR}/public/mathlib.h"
		"${SRCDIR}/public/OfflineMode.h"
		"${SRCDIR}/public/SDK_Color.h"
		"${SRCDIR}/public/Vector.h"
		"${SRCDIR}/public/Vector2D.h"
		"${SRCDIR}/public/winlite.h"
		"${SRCDIR}/public/worldsize.h"

		"${SRCDIR}/public/appframework/AppFramework.h"
		"${SRCDIR}/public/appframework/IAppSystem.h"
		"${SRCDIR}/public/appframework/IAppSystemGroup.h"
		"${SRCDIR}/public/appframework/VguiMatSysApp.h"

		"${SRCDIR}/public/bitmap/Bitmap.h"
		"${SRCDIR}/public/bitmap/Float_BM.h"
		"${SRCDIR}/public/bitmap/ImageFormat.h"
		"${SRCDIR}/public/bitmap/PSD.h"
		"${SRCDIR}/public/bitmap/TGALoader.h"
		"${SRCDIR}/public/bitmap/TGAWriter.h"

		"${SRCDIR}/public/mathlib/halton.h"
		"${SRCDIR}/public/mathlib/lightdesc.h"
		"${SRCDIR}/public/mathlib/math_base.h"
		"${SRCDIR}/public/mathlib/ssemath.h"

		"${SRCDIR}/public/vstdlib/cvar.h"
		"${SRCDIR}/public/vstdlib/ICommandline.h"
		"${SRCDIR}/public/vstdlib/IKeyValuesSystem.h"
		"${SRCDIR}/public/vstdlib/pch_vstdlib.h"
		"${SRCDIR}/public/vstdlib/random.h"
		"${SRCDIR}/public/vstdlib/strtools.h"
		"${SRCDIR}/public/vstdlib/vstdlib.h"

		"BackgroundMenuButton.h"
		"BasePanel.h"
		"BitmapButton.h"
		"BitmapImagePanel.h"
		"CareerBaseFrame.h"
		"CareerBox.h"
		"CareerGame.h"
		"CareerGameTasks.h"
		"CareerMapFrame.h"
		"CareerProfile.h"
		"CareerProfileFrame.h"
		"CDKeyEntryDialog.h"
		"CommandCheckButton.h"
		"ContentControlDialog.h"
		"CreateMultiplayerGameBotPage.h"
		"CreateMultiplayerGameDialog.h"
		"CreateMultiplayerGameGameplayPage.h"
		"CreateMultiplayerGameServerPage.h"
		"CvarLatch.h"
		"CvarNegateCheckButton.h"
		"CvarSlider.h"
		"CvarTextEntry.h"
		"CvarToggleCheckButton.h"
		"DottedLabel.h"
		"EngineInterface.h"
		"GameConsole.h"
		"GameConsoleDialog.h"
		"GameUI_Interface.h"
		"HelpDialog.h"
		"ImageButton.h"
		"KeyToggleCheckButton.h"
		"LabeledCommandComboBox.h"
		"labelslidercombo.h"
		"LoadGameDialog.h"
		"LoadingDialog.h"
		"LogoFile.h"
		"ModInfo.h"
		"MultiplayerAdvancedDialog.h"
		"MusicManager.h"
		"NewGameDialog.h"
		"OptionsDialog.h"
		"OptionsSubAdvanced.h"
		"OptionsSubAudio.h"
		"OptionsSubKeyboard.h"
		"OptionsSubMouse.h"
		"OptionsSubMultiplayer.h"
		"OptionsSubVideo.h"
		"OptionsSubVoice.h"
		"PanelListPanel.h"
		"PlayerListDialog.h"
		"RefreshSteamLogin.h"
		"RunGameEngine.h"
		"SaveGameDialog.h"
		"ScriptObject.h"
		"Sys_Utils.h"
		"TaskButton.h"
		"Taskbar.h"
		"taskframe.h"
		"TextEntryBox.h"
		"vcontrolslistpanel.h"
		"VGuiSystemModuleLoader.h"
	)
END_SRC( GAMEUI_HEADER_FILES "Header Files" )

add_library(
	GameUI SHARED
	${GAMEUI_SOURCE_FILES} ${GAMEUI_HEADER_FILES}
)
install_library(
	TARGET GameUI
	INSTALL_DEST "${GAMEDIR}/valve/cl_dlls"
)
set_property(
	TARGET GameUI
	PROPERTY FOLDER "Shared Libraries"
)

target_include_directories(
	GameUI PRIVATE

	"${SRCDIR}/engine"
	"${SRCDIR}/pm_shared"
	"${SRCDIR}/cl_dll"
	"${SRCDIR}/game_shared/bot"
	"${SRCDIR}/game_shared"

	"${SRCDIR}/common"
	"${SRCDIR}/public"
	"${SRCDIR}/public/tier1"
	"${SRCDIR}" # FIXME(SanyaSho): Some headers want to include "public/"

	"${SRCDIR}/external/SDL2/include"
)

target_compile_definitions(
	GameUI PRIVATE

	_SCL_SECURE_NO_WARNINGS

	NO_MALLOC_OVERRIDE
	GAMEUI_EXPORTS
	CS_SHIELD_ENABLED
	GameUI_EXPORTS
)

target_link_libraries(
	GameUI PRIVATE

	vgui2_controls
	vstdlib
	tier1
	tier0
)

target_use_sdl2( GameUI )
