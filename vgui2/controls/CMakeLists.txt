#-----------------------------------------------------------------------------
# VGUI2_CONTROLS.CMAKE
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

set( VGUI2_CONTROLS_SOURCE_FILES )
BEGIN_SRC( VGUI2_CONTROLS_SOURCE_FILES "Source Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/public/FileSystem_Helpers.cpp"
		"${SRCDIR}/tier1/CharacterSet.cpp"
		"${SRCDIR}/vgui2/src/vgui_key_translation.cpp"
		"AnimatingImagePanel.cpp"
		"AnimationController.cpp"
		"BitmapImagePanel.cpp"
		"BuildFactoryHelper.cpp"
		"BuildGroup.cpp"
		"BuildModeDialog.cpp"
		"Button.cpp"
		"CheckButton.cpp"
		"CheckButtonList.cpp"
		"ComboBox.cpp"
		"Controls.cpp"
		"DirectorySelectDialog.cpp"
		"Divider.cpp"
		"EditablePanel.cpp"
		"ExpandButton.cpp"
		"FileOpenDialog.cpp"
		"FocusNavGroup.cpp"
		"Frame.cpp"
		"GraphPanel.cpp"
		"HTML.cpp"
		"Image.cpp"
		"ImageList.cpp"
		"ImagePanel.cpp"
		"InputDialog.cpp"
		"KeyBindingHelpDialog.cpp"
		"KeyBoardEditorDialog.cpp"
		"Label.cpp"
		"ListPanel.cpp"
		"ListViewPanel.cpp"
		"Menu.cpp"
		"MenuBar.cpp"
		"MenuButton.cpp"
		"MenuItem.cpp"
		"MessageBox.cpp"
		"Panel.cpp"
		"PanelListPanel.cpp"
		"ProgressBar.cpp"
		"ProgressBox.cpp"
		"PropertyDialog.cpp"
		"PropertyPage.cpp"
		"PropertySheet.cpp"
		"QueryBox.cpp"
		"RadioButton.cpp"
		"RichText.cpp"
		"ScrollBar.cpp"
		"ScrollBarSlider.cpp"
		"SectionedListPanel.cpp"
		"Slider.cpp"
		"Splitter.cpp"
		"TextEntry.cpp"
		"TextImage.cpp"
		"ToggleButton.cpp"
		"Tooltip.cpp"
		"ToolWindow.cpp"
		"TreeView.cpp"
		"TreeViewListControl.cpp"
		"URLLabel.cpp"
		"WizardPanel.cpp"
		"WizardSubPanel.cpp"
	)
END_SRC( VGUI2_CONTROLS_SOURCE_FILES "Source Files" )

set( VGUI2_CONTROLS_HEADER_FILES )
BEGIN_SRC( VGUI2_CONTROLS_HEADER_FILES "Header Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/public/vgui_controls/AnimatingImagePanel.h"
		"${SRCDIR}/public/vgui_controls/AnimationController.h"
		"${SRCDIR}/public/vgui_controls/BitmapImagePanel.h"
		"${SRCDIR}/public/vgui_controls/BuildGroup.h"
		"${SRCDIR}/public/vgui_controls/BuildModeDialog.h"
		"${SRCDIR}/public/vgui_controls/Button.h"
		"${SRCDIR}/public/vgui_controls/CheckButton.h"
		"${SRCDIR}/public/vgui_controls/CheckButtonList.h"
		"${SRCDIR}/public/vgui_controls/ComboBox.h"
		"${SRCDIR}/public/vgui_controls/Controls.h"
		"${SRCDIR}/public/vgui_controls/DialogManager.h"
		"${SRCDIR}/public/vgui_controls/DirectorySelectDialog.h"
		"${SRCDIR}/public/vgui_controls/Divider.h"
		"${SRCDIR}/public/vgui_controls/EditablePanel.h"
		"${SRCDIR}/public/vgui_controls/ExpandButton.h"
		"${SRCDIR}/public/vgui_controls/FileOpenDialog.h"
		"${SRCDIR}/public/vgui_controls/FocusNavGroup.h"
		"${SRCDIR}/public/vgui_controls/Frame.h"
		"${SRCDIR}/public/vgui_controls/GraphPanel.h"
		"${SRCDIR}/public/vgui_controls/HTML.h"
		"${SRCDIR}/public/vgui_controls/Image.h"
		"${SRCDIR}/public/vgui_controls/ImageList.h"
		"${SRCDIR}/public/vgui_controls/ImagePanel.h"
		"${SRCDIR}/public/vgui_controls/InputDialog.h"
		"${SRCDIR}/public/vgui_controls/KeyBindingHelpDialog.h"
		"${SRCDIR}/public/vgui_controls/KeyBindingMap.h"
		"${SRCDIR}/public/vgui_controls/KeyBoardEditorDialog.h"
		"${SRCDIR}/public/vgui_controls/Label.h"
		"${SRCDIR}/public/vgui_controls/ListPanel.h"
		"${SRCDIR}/public/vgui_controls/ListViewPanel.h"
		"${SRCDIR}/public/vgui_controls/MenuBar.h"
		"${SRCDIR}/public/vgui_controls/MenuButton.h"
		"${SRCDIR}/public/vgui_controls/Menu.h"
		"${SRCDIR}/public/vgui_controls/MenuItem.h"
		"${SRCDIR}/public/vgui_controls/MessageBox.h"
		"${SRCDIR}/public/vgui_controls/MessageMap.h"
		"${SRCDIR}/public/vgui_controls/PanelAnimationVar.h"
		"${SRCDIR}/public/vgui_controls/Panel.h"
		"${SRCDIR}/public/vgui_controls/PanelListPanel.h"
		"${SRCDIR}/public/vgui_controls/pch_vgui_controls.h"
		"${SRCDIR}/public/vgui_controls/PHandle.h"
		"${SRCDIR}/public/vgui_controls/ProgressBar.h"
		"${SRCDIR}/public/vgui_controls/ProgressBox.h"
		"${SRCDIR}/public/vgui_controls/PropertyDialog.h"
		"${SRCDIR}/public/vgui_controls/PropertyPage.h"
		"${SRCDIR}/public/vgui_controls/PropertySheet.h"
		"${SRCDIR}/public/vgui_controls/QueryBox.h"
		"${SRCDIR}/public/vgui_controls/RadioButton.h"
		"${SRCDIR}/public/vgui_controls/RichText.h"
		"${SRCDIR}/public/vgui_controls/ScrollBar.h"
		"${SRCDIR}/public/vgui_controls/ScrollBarSlider.h"
		"${SRCDIR}/public/vgui_controls/SectionedListPanel.h"
		"${SRCDIR}/public/vgui_controls/Slider.h"
		"${SRCDIR}/public/vgui_controls/Splitter.h"
		"${SRCDIR}/public/vgui_controls/TextEntry.h"
		"${SRCDIR}/public/vgui_controls/TextImage.h"
		"${SRCDIR}/public/vgui_controls/ToggleButton.h"
		"${SRCDIR}/public/vgui_controls/Tooltip.h"
		"${SRCDIR}/public/vgui_controls/ToolWindow.h"
		"${SRCDIR}/public/vgui_controls/TreeView.h"
		"${SRCDIR}/public/vgui_controls/TreeViewListControl.h"
		"${SRCDIR}/public/vgui_controls/URLLabel.h"
		"${SRCDIR}/public/vgui_controls/WizardPanel.h"
		"${SRCDIR}/public/vgui_controls/WizardSubPanel.h"
	)
END_SRC( VGUI2_CONTROLS_HEADER_FILES "Header Files" )

add_library(
	vgui2_controls STATIC
	${VGUI2_CONTROLS_SOURCE_FILES} ${VGUI2_CONTROLS_HEADER_FILES}
)
set_property(
	TARGET vgui2_controls
	PROPERTY FOLDER "Static Libraries"
)

target_include_directories(
	vgui2_controls PRIVATE

	"${SRCDIR}" # FIXME(SanyaSho): Some headers want to include "public/"

	"${SRCDIR}/public"
	"${SRCDIR}/public/tier1"
	"${SRCDIR}/public/tier0"
	"${SRCDIR}/public/vgui"
	"${SRCDIR}/public/vgui_controls"
)

target_compile_definitions(
	vgui2_controls PRIVATE

	NO_MALLOC_OVERRIDE
	MATHLIB_HEADER_ONLY
	MINMAX
	ALTERNATE_LE
)
