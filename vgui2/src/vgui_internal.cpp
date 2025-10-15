#include "vgui.h"
#include "vgui_internal.h"
#include "interface.h"

#include "vgui/ISurface.h"
#include "FileSystem.h"
#include "vgui/IKeyValues.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"

IKeyValues *g_pKeyValues;
IKeyValues *keyvalues() // For tier1
{
	return g_pKeyValues;
}

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: finds a particular interface in the factory set
//-----------------------------------------------------------------------------
IBaseInterface *InitializeInterface( const char *interfaceName, CreateInterfaceFn *factoryList, int numFactories )
{
	IBaseInterface *retval;

	for ( int i = 0; i < numFactories; i++ )
	{
		CreateInterfaceFn factory = factoryList[i];
		if ( !factory )
			continue;

		retval = factory( interfaceName, nullptr );
		if ( retval )
			return retval;
	}

	// No provider for requested interface!!!
	// Assert( !"No provider for requested interface!!!" );

	return nullptr;
}

ISurface *g_pSurface = NULL;
IFileSystem *g_pFileSystem = NULL;
ILocalize *g_pLocalize = NULL;
IPanel *g_pIPanel = NULL;

bool VGui_InternalLoadInterfaces( CreateInterfaceFn *factoryList, int numFactories ) // 68
{
	if ( numFactories <= 0 )
	{
		g_pSurface = NULL;
		g_pFileSystem = NULL;
		g_pKeyValues = NULL;
		g_pLocalize = NULL;
		g_pIPanel = NULL;

		return 0;
	}

	g_pSurface = (ISurface *)InitializeInterface( VGUI_SURFACE_INTERFACE_VERSION, factoryList, numFactories );
	g_pFileSystem = (IFileSystem *)InitializeInterface( FILESYSTEM_INTERFACE_VERSION, factoryList, numFactories );
	g_pKeyValues = (IKeyValues *)InitializeInterface( KEYVALUES_INTERFACE_VERSION, factoryList, numFactories );
	g_pLocalize = (ILocalize *)InitializeInterface( VGUI_LOCALIZE_INTERFACE_VERSION, factoryList, numFactories );
	g_pIPanel = (IPanel *)InitializeInterface( VGUI_PANEL_INTERFACE_VERSION, factoryList, numFactories );

	return g_pSurface && g_pFileSystem && g_pKeyValues && g_pLocalize && g_pIPanel;
}
};
