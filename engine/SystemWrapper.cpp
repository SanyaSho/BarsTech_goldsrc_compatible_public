#include <cstdarg>

#include "quakedef.h"
#include "buildnum.h"
#include "FilePaths.h"
#include "SystemWrapper.h"
#include "TokenLine.h"

#include "render.h"
#include "cdll_exp.h"
#if defined(GLQUAKE)
#include "gl_screen.h"
#else
#include "screen.h"
#endif
#include "vgui_int.h"

void SystemWrapperCommandForwarder();

void COM_RemoveEvilChars( char* string )
{
	for( char* i = string; i && *i; ++i )
	{
		if( *i == '%' || *i < ' ' )
			*i = ' ';
	}
}

SystemWrapper gSystemWrapper;

bool EngineWrapper::Init( IBaseSystem* system, int serial, char* name )
{
	return BaseSystemModule::Init(system, serial, name);
}

void EngineWrapper::RunFrame( double time )
{
	BaseSystemModule::RunFrame(time);
}

void EngineWrapper::ReceiveSignal( ISystemModule* module, unsigned int signal, void* data )
{
	BaseSystemModule::ReceiveSignal( module, signal, data );
}

void EngineWrapper::ExecuteCommand( int commandID, char* commandLine )
{
	BaseSystemModule::ExecuteCommand( commandID, commandLine );
}

void EngineWrapper::RegisterListener( ISystemModule* module )
{
	BaseSystemModule::RegisterListener( module );
}

void EngineWrapper::RemoveListener( ISystemModule* module )
{
	BaseSystemModule::RemoveListener( module );
}

IBaseSystem* EngineWrapper::GetSystem()
{
	return BaseSystemModule::GetSystem();
}

unsigned int EngineWrapper::GetSerial()
{
	return BaseSystemModule::GetSerial();
}

char* EngineWrapper::GetStatusLine()
{
	return const_cast<char*>("No status available.\n");
}

char* EngineWrapper::GetType()
{
	return const_cast<char*>(ENGINEWRAPPER_INTERFACE_VERSION);
}

char* EngineWrapper::GetName()
{
	return BaseSystemModule::GetName();
}

unsigned int EngineWrapper::GetState()
{
	return BaseSystemModule::GetState();
}

int EngineWrapper::GetVersion()
{
	return BaseSystemModule::GetVersion();
}

void EngineWrapper::ShutDown()
{
	BaseSystemModule::ShutDown();
}

bool EngineWrapper::GetViewOrigin( float* origin )
{
	VectorCopy(r_refdef.vieworg, origin);
	return true;
}

bool EngineWrapper::GetViewAngles( float* angles )
{
	VectorCopy(r_refdef.viewangles, angles);
	return true;
}

int EngineWrapper::GetTraceEntity()
{
	//Nothing
	return 0;
}

float EngineWrapper::GetCvarFloat( char* szName )
{
	cvar_t* pCvar = Cvar_FindVar( szName );

	if( !pCvar )
		return 0;

	return pCvar->value;
}

char* EngineWrapper::GetCvarString( char* szName )
{
	cvar_t* pCvar = Cvar_FindVar( szName );

	if( !pCvar )
		return nullptr;

	return pCvar->string;
}

void EngineWrapper::SetCvar( char* szName, char* szValue )
{
	Cvar_Set(szName, szValue);
}

void EngineWrapper::Cbuf_AddText( char* text )
{
	Cbuf_AddText( text );
}

void EngineWrapper::DemoUpdateClientData( client_data_t* cdat )
{
	ClientDLL_DemoUpdateClientData(cdat);
	scr_fov_value = cdat->fov;
}

void EngineWrapper::CL_QueueEvent( int flags, int index, float delay, event_args_t* pargs )
{
	CL_QueueEvent(flags, index, delay, pargs);
}

void EngineWrapper::HudWeaponAnim( int iAnim, int body )
{
	hudWeaponAnim(iAnim, body);
}

void EngineWrapper::CL_DemoPlaySound( int channel, char* sample, float attenuation, float volume, int flags, int pitch )
{
	CL_DemoPlaySound(channel, sample, attenuation, volume, flags, pitch);
}

void EngineWrapper::ClientDLL_ReadDemoBuffer( int size, byte* buffer )
{
	ClientDLL_ReadDemoBuffer(size, buffer);
}

bool EngineWrapper::ValidStuffText(const char *pMsg)
{
	return ValidStuffText(pMsg) != false;
}

void EngineWrapper::Cbuf_AddFilteredText(char *text)
{
	Cbuf_AddFilteredText(text);
}

bool SystemWrapper::Init( IBaseSystem* system, int serial, char* name )
{
	BaseSystemModule::Init( system, serial, name );

	m_Commands.Init();
	m_Modules.Init();
	m_Libraries.Init();
	m_Listener.Init();

	auto pEngineWrapper = new EngineWrapper();
	m_EngineWrapper = pEngineWrapper;

	AddModule( pEngineWrapper, const_cast<char*>(ENGINEWRAPPER_INTERFACE_VERSION) );

	m_State = SYSMODSTATE_RUNNING;

	return true;
}

void SystemWrapper::RunFrame( double time )
{
	m_SystemTime += time;
	++m_Tick;

	if( m_State == SYSMODSTATE_RUNNING )
	{
		for( void* pObject = m_Listener.GetFirst(); pObject; pObject = m_Listener.GetNext() )
		{
			if( m_State == SYSMODSTATE_SHUTDOWN )
				break;

			((ISystemModule*)pObject)->RunFrame( m_SystemTime );
		}

		m_LastTime = m_SystemTime;
	}
}

void SystemWrapper::ReceiveSignal( ISystemModule* module, unsigned int signal, void* data )
{
	BaseSystemModule::ReceiveSignal( module, signal, data );
}

void SystemWrapper::ExecuteCommand( int commandID, char* commandLine )
{
	switch( commandID )
	{
	case CMD_ID_MODULES:
		CMD_Modules(commandLine);
		break;

	case CMD_ID_LOADMODULE:
		CMD_LoadModule( commandLine );
		break;

	case CMD_ID_UNLOADMODULE:
		CMD_UnloadModule( commandLine );
		break;

	default:
		Printf( const_cast<char*>("SystemWrapper::ExecuteCommand: unknown command ID %i.\n"), commandID );
		break;
	}
}

void SystemWrapper::RegisterListener( ISystemModule* module )
{
	BaseSystemModule::RegisterListener( module );
}

void SystemWrapper::RemoveListener( ISystemModule* module )
{
	BaseSystemModule::RemoveListener( module );
}

IBaseSystem* SystemWrapper::GetSystem()
{
	return BaseSystemModule::GetSystem();
}

unsigned int SystemWrapper::GetSerial()
{
	return BaseSystemModule::GetSerial();
}

char* SystemWrapper::GetStatusLine()
{
	return const_cast<char*>("No status available.\n");
}

char* SystemWrapper::GetType()
{
	return const_cast<char*>("basesystem002");
}

char* SystemWrapper::GetName()
{
	return BaseSystemModule::GetName();
}

unsigned int SystemWrapper::GetState()
{
	return BaseSystemModule::GetState();
}

int SystemWrapper::GetVersion()
{
	return BaseSystemModule::GetVersion();
}

void SystemWrapper::ShutDown()
{
	m_Listener.Clear( false );
	m_Commands.Clear( true );

	while( true )
	{
		//The module will remove itself on shutdown.
		void* pObject = m_Modules.GetFirst();

		if( !pObject )
			break;

		((ISystemModule*)pObject)->ShutDown();
	}

	while( true )
	{
		library_t* pLib = (library_t*)m_Libraries.RemoveTail();

		if( !pLib )
			break;

		if( pLib->handle )
			Sys_UnloadModule( pLib->handle );

		Mem_Free( pLib );
	}

	if( m_EngineWrapper )
		m_EngineWrapper->ShutDown();

	Cmd_RemoveWrapperCmds();

	m_State = SYSMODSTATE_SHUTDOWN;
}

double SystemWrapper::GetTime()
{
	return m_SystemTime;
}

unsigned int SystemWrapper::GetTick()
{
	return m_Tick;
}

void SystemWrapper::SetFPS( float fps )
{
	//Nothing
}

void SystemWrapper::Printf( char* fmt, ... )
{
	static char string[ 8192 ];

	va_list va;

	va_start( va, fmt );
	vsnprintf( string, ARRAYSIZE( string ), fmt, va );
	va_end( va );

	Con_Printf( const_cast<char*>("%s"), string );
}

void SystemWrapper::DPrintf( char* fmt, ... )
{
	static char string[ 8192 ];

	va_list va;

	va_start( va, fmt );
	vsnprintf( string, ARRAYSIZE( string ), fmt, va );
	va_end( va );

	Con_Printf( string );
}

void SystemWrapper::RedirectOutput( char* buffer, int maxSize )
{
	Con_Printf( const_cast<char*>("WARNIG! SystemWrapper::RedirectOutput: not implemented.\n") );
}

IFileSystem* SystemWrapper::GetFileSystem()
{
	return g_pFileSystem;
}

byte* SystemWrapper::LoadFile( const char* name, int* length )
{
	return COM_LoadFile( (char*)name, 5, length );
}

void SystemWrapper::FreeFile( byte* fileHandle )
{
	COM_FreeFile( fileHandle );
}

void SystemWrapper::SetTitle( char* text )
{
	Con_Printf( const_cast<char*>("TODO: SystemWrapper::SetTitle ?\n") );
}

void SystemWrapper::SetStatusLine( char* text )
{
	Con_Printf( const_cast<char*>("TODO: SystemWrapper::SetStatusLine ?\n") );
}

void SystemWrapper::ShowConsole( bool visible )
{
}

void SystemWrapper::LogConsole( char* filename )
{
	if( filename )
		Cmd_ExecuteString( const_cast<char*>("log on"), src_command );
	else
		Cmd_ExecuteString( const_cast<char*>("log off"), src_command );
}

vgui2::Panel *SystemWrapper::GetPanel()
{
	return (vgui2::Panel *)VGuiWrap2_GetPanel();
}

bool SystemWrapper::InitVGUI( IVGuiModule* module )
{
	return false;
}

bool SystemWrapper::RegisterCommand( char* name, ISystemModule* module, int commandID )
{
	for( command_t* pObject = (command_t*)m_Commands.GetFirst(); pObject; pObject = (command_t*)m_Commands.GetNext() )
	{
		if( !stricmp( pObject->name, name ) )
		{
			Printf( const_cast<char*>("WARNING! System::RegisterCommand: command \"%s\" already exists.\n"), name );
			return false;
		}
	}

	command_t* pCmd = (command_t*)Mem_ZeroMalloc( sizeof(command_t) );

	strncpy( pCmd->name, name, ARRAYSIZE( pCmd->name ) );
	pCmd->name[ ARRAYSIZE( pCmd->name ) - 1 ] = '\0';

	pCmd->module = module;
	pCmd->commandID = commandID;

	m_Commands.Add( pCmd );

	Cmd_AddWrapperCommand( pCmd->name, &SystemWrapperCommandForwarder );

	return true;
}

void SystemWrapper::GetCommandMatches( char* string, ObjectList* pMatchList )
{
	pMatchList->Clear(true);
}

void SystemWrapper::ExecuteString( char* commands )
{
	if( !commands || !( *commands ) )
		return;

	//Remove format characters to block format string attacks.
	COM_RemoveEvilChars( commands );

	char singleCmd[ 256 ];

	char* pszDest;

	const char* pszSource = commands;

	bool bInQuote = false;

	while( *pszSource )
	{
		//Parse out single commands and execute them.
		pszDest = singleCmd;

		int i;
		for( i = 0; i < ARRAYSIZE( singleCmd ); ++i )
		{
			char c = *pszSource++;

			*pszDest++ = c;

			if( c == '"' )
			{
				bInQuote = !bInQuote;
			}
			else if( c == ';' && !bInQuote )
			{
				//End of command and not in a quoted string.
				break;
			}
		}

		if( i >= ARRAYSIZE( singleCmd ) )
		{
			Printf( const_cast<char*>("WARNING! System::ExecuteString: Command token too long.\n") );
			return;
		}

		*pszDest = '\0';

		char* pszCmd = singleCmd;

		while( *pszCmd == ' ' )
		{
			++pszCmd;
		}

		DispatchCommand( pszCmd );
	}
}

void SystemWrapper::ExecuteFile( char* filename )
{
	char cmd[ 1024 ];

	snprintf( cmd, ARRAYSIZE( cmd ) - 1, "exec %s\n", filename );
	cmd[ ARRAYSIZE( cmd ) - 1 ] = '\0';

	Cmd_ExecuteString( cmd, src_command );
}

void SystemWrapper::Errorf( char* fmt, ... )
{
	static char string[ 1024 ];

	va_list va;

	va_start( va, fmt );
	vsnprintf( string, ARRAYSIZE( string ), fmt, va );
	va_end( va );

	Printf( const_cast<char*>("***** FATAL ERROR *****\n") );
	Printf( const_cast<char*>("%s"), string );
	Printf( const_cast<char*>("*** STOPPING SYSTEM ***\n") );

	Stop();
}

char* SystemWrapper::CheckParam( char* param )
{
	const int index = COM_CheckParm( param );

	if( !index )
		return NULL;

	if( index + 1 >= com_argc )
		return const_cast<char*>("");

	return com_argv[ index + 1 ];
}

bool SystemWrapper::AddModule( ISystemModule* module, char* name )
{
	if( !module )
		return false;

	if( !module->Init( this, m_SerialCounter, name ) )
	{
		Printf( const_cast<char*>("ERROR! System::AddModule: couldn't initialize module %s.\n"), name );
		return false;
	}

	m_Modules.AddHead( module );

	++m_SerialCounter;

	return true;
}

ISystemModule* SystemWrapper::GetModule( char* interfacename, char* library, char* instancename )
{
	ISystemModule* pModule = FindModule( interfacename, instancename );

	if( pModule )
		return pModule;

	library_t* pLib = GetLibrary( library );

	if( !pLib )
		return nullptr;
	
	pModule = (ISystemModule*)pLib->createInterfaceFn( interfacename, nullptr );

	if( pModule )
	{
		AddModule( pModule, instancename );
		return pModule;
	}

	Printf( const_cast<char*>("ERROR! System::GetModule: interface \"%s\" not found in library %s.\n"), interfacename, pLib->name );

	return nullptr;
}

bool SystemWrapper::RemoveModule( ISystemModule* module )
{
	if( !module )
		return true;

	module->ShutDown();

	//Remove all commands.
	for (command_t* pObject = (command_t*)m_Commands.GetFirst(); pObject; pObject = (command_t*)m_Commands.GetNext())
	{
		if( pObject->module->GetSerial() == module->GetSerial() )
		{
			m_Commands.Remove( pObject );
			Mem_Free( pObject );
		}
	}

	for(ISystemModule* pObject = (ISystemModule*)m_Modules.GetFirst(); pObject; pObject = (ISystemModule*)m_Modules.GetNext())
	{
		if( pObject->GetSerial() == module->GetSerial() )
		{
			m_Modules.Remove( pObject );
			return true;
		}
	}

	return false;
}

void SystemWrapper::Stop()
{
	m_State = SYSMODSTATE_SHUTDOWN;
}

char* SystemWrapper::COM_GetBaseDir()
{
	return BaseSystemModule::COM_GetBaseDir();
}

void SystemWrapper::CMD_Modules(char *cmdLine)
{
	for (ISystemModule* pObject = (ISystemModule*)m_Modules.GetFirst(); pObject; pObject = (ISystemModule*)m_Modules.GetNext())
	{
		Printf(const_cast<char*>("%s(%s):%s"), pObject->GetName(), pObject->GetType(), pObject->GetStatusLine());
	}

	Printf(const_cast<char*>("--- %i modules in total ---\n"), m_Modules.CountElements());
}

void SystemWrapper::CMD_LoadModule( char* cmdLine )
{
	TokenLine params( cmdLine );

	if( params.CountToken() <= 1 )
	{
		Printf( const_cast<char*>("Syntax: loadmodule <module> [<library>] [<name>]\n") );
		return;
	}

	char* pszModuleName = params.GetToken( 1 );
	char* pszLibName;
	char* pszName = nullptr;

	if( params.CountToken() == 2 )
	{
		pszLibName = pszModuleName;
	}
	else
	{
		pszLibName = params.GetToken( 2 );

		if( params.CountToken() != 3 )
		{
			pszName = params.GetToken( 3 );
		}
	}

	GetModule( pszModuleName, pszLibName, pszName );
}

void SystemWrapper::CMD_UnloadModule( char* cmdLine )
{
	TokenLine params( cmdLine );

	if( params.CountToken() <= 1 )
	{
		Printf( const_cast<char*>("Syntax: unloadmodule <module> [<name>]\n") );
		return;
	}

	char* pszType = params.GetToken( 1 );
	char* pszName = nullptr;

	if( params.CountToken() == 2 || params.CountToken() == 3 )
	{
		if( params.CountToken() == 3 )
		{
			pszName = params.GetToken( 2 );
		}

		auto pModule = FindModule( pszType, pszName );

		if( pModule )
		{
			RemoveModule( pModule );
			return;
		}
	}

	Printf( const_cast<char*>("Module not found.\n") );
}

bool SystemWrapper::DispatchCommand( char* command )
{
	if( !command || !( *command ) )
		return true;

	for( command_t* pObject = (command_t*)m_Commands.GetFirst(); pObject; pObject = (command_t*)m_Commands.GetNext() )
	{
		if( !strnicmp( pObject->name, command, strlen( pObject->name ) ) )
		{
			pObject->module->ExecuteCommand( pObject->commandID, command );
			return true;
		}
	}

	Cmd_ExecuteString( command, src_command );
	return true;
}

ISystemModule* SystemWrapper::FindModule( char* type, char* name )
{
	if( !type || !( *type ) )
		return nullptr;

	for(ISystemModule* pObject = (ISystemModule*)m_Modules.GetFirst(); pObject; pObject = (ISystemModule*)m_Modules.GetNext())
	{
		if( !stricmp( type, pObject->GetType() ) )
		{
			if( !name || !stricmp( name, pObject->GetName() ) )
			{
				return pObject;
			}
		}
	}

	return nullptr;
}

SystemWrapper::library_t* SystemWrapper::GetLibrary( char* name )
{
	char fixedname[ MAX_PATH ];

	strncpy( fixedname, name, ARRAYSIZE( fixedname ) - 1 );

	COM_FixSlashes( fixedname );

	for(library_t* pObject = (library_t*)m_Libraries.GetFirst(); pObject; pObject = (library_t*)m_Libraries.GetNext())
	{
		if( !stricmp( name, pObject->name ) )
		{
			return pObject;
		}
	}

	library_t* pLib = (library_t*)Mem_ZeroMalloc( sizeof(library_t) );

	if( !pLib )
	{
		DPrintf(const_cast<char*>("ERROR! System::GetLibrary: out of memory (%s).\n"), name);
		return nullptr;
	}

	snprintf( pLib->name, ARRAYSIZE( pLib->name ), "%s" DEFAULT_SO_EXT, fixedname );

	FS_GetLocalCopy( pLib->name );
	pLib->handle = Sys_LoadModule( pLib->name );

	if( pLib->handle )
	{
		pLib->createInterfaceFn = Sys_GetFactory( pLib->handle );

		if( pLib->createInterfaceFn )
		{
			m_Libraries.Add( pLib );

			DPrintf( const_cast<char*>("Loaded library %s.\n"), pLib->name );

			return pLib;
		}

		DPrintf( const_cast<char*>("ERROR! System::GetLibrary: coulnd't get object factory(%s).\n"), pLib->name );

	}

	DPrintf( const_cast<char*>("ERROR! System::GetLibrary: coulnd't load library (%s).\n"), pLib->name );

	Mem_Free( pLib );

	return nullptr;
}

int COM_BuildNumber()
{
	return build_number();
}

void SystemWrapper_Init()
{
	gSystemWrapper.Init( &gSystemWrapper, 0, const_cast<char*>("SystemWrapper") );
}

void SystemWrapper_ShutDown()
{
	gSystemWrapper.ShutDown();
}

void SystemWrapper_RunFrame( double time )
{
	gSystemWrapper.RunFrame( time );
}

void SystemWrapper_ExecuteString( char* command )
{
	if( command && *command )
	{
		gSystemWrapper.ExecuteString( command );
	}
}

void SystemWrapperCommandForwarder()
{
	char cmd[ 1024 ];

	strcpy( cmd, Cmd_Argv( 0 ) );

	//Add command args
	if( Cmd_Argc() > 1 )
	{
		const size_t uiLength = strlen( cmd );

		cmd[ uiLength ] = ' ';
		cmd[ uiLength + 1 ] = '\0';
		strcat( cmd, Cmd_Args() );
	}

	cmd[ ARRAYSIZE( cmd ) - 1 ] = '\0';

	SystemWrapper_ExecuteString( cmd );
}

int SystemWrapper_LoadModule( char* interfacename, char* library, char* instancename )
{
	return gSystemWrapper.GetModule( interfacename, library, instancename ) != nullptr;
}
