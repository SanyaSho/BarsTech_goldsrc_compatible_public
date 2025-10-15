// vgui_internal.h
#pragma once

#include "interface.h"

#include "vgui/ISurface.h"
#include "FileSystem.h"
#include "vgui/IKeyValues.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"

#include "vgui/IInputInternal.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"

extern IKeyValues *g_pKeyValues;
class IKeyValues *keyvalues(); // For tier1

namespace vgui2
{
extern vgui2::ISurface *g_pSurface;
extern IFileSystem *g_pFileSystem;
extern vgui2::ILocalize *g_pLocalize;
extern vgui2::IPanel *g_pIPanel;

// input.cpp
extern vgui2::IInputInternal *g_pInput;

// Scheme.cpp
extern vgui2::ISchemeManager *g_pScheme;

// system_win32.cpp
extern vgui2::ISystem *g_pSystem;

// vgui.cpp
extern vgui2::IVGui *g_pIVgui;

bool VGui_InternalLoadInterfaces( CreateInterfaceFn *factoryList, int numFactories );
};
