/**
*	@file
*
*	dynamic variable tracking
*/

#include "quakedef.h"
#include "cdll_int.h"
#include "client.h"
#include "host.h"

cvar_t* cvar_vars = nullptr;
char* cvar_null_string = const_cast<char*>("");
cvarhook_t* cvar_hooks = NULL;

void Cvar_Init()
{
}

void Cvar_Shutdown()
{
	cvar_vars = nullptr;
}

cvar_t* Cvar_FindVar( const char* var_name )
{
	g_engdstAddrs.pfnGetCvarPointer( &var_name );

	for( cvar_t* var = cvar_vars; var; var = var->next )
	{
		if( !Q_strcmp( var_name, var->name ) )
		{
			return var;
		}
	}

	return nullptr;
}

cvar_t* Cvar_FindPrevVar( char* var_name )
{
	cvar_t* pPrev;
	cvar_t* pNext;

	for( pPrev = cvar_vars, pNext = cvar_vars->next;
		 pNext && !Q_strcasecmp( var_name, pNext->name );
		 pPrev = pNext, pNext = pNext->next )
	{
	}

	if( !pNext )
		return nullptr;

	return pPrev;
}

float Cvar_VariableValue( char* var_name )
{
	cvar_t* var = Cvar_FindVar( var_name );

	if( !var )
		return 0;

	return Q_atof( var->string );
}

int Cvar_VariableInt( char* var_name )
{
	cvar_t* var = Cvar_FindVar( var_name );

	if( !var )
		return 0;

	return Q_atoi( var->string );
}

char* Cvar_VariableString( char* var_name )
{
	cvar_t* var = Cvar_FindVar( var_name );

	if( !var )
		return cvar_null_string;

	return var->string;
}

char* Cvar_CompleteVariable( char* partial, int forward )
{
	static char lastpartial[ 256 ] = {};

	int len = Q_strlen( partial );

	if( !len )
		return nullptr;

	char search[ 256 ];

	Q_strncpy( search, partial, ARRAYSIZE( search ) );

	//Strip trailing whitespace.
	for( int i = len - 1; search[ len - 1 ] == ' '; --i )
	{
		search[ i ] = '\0';
		len = i;
	}

	cvar_t* pCvar = cvar_vars;

	//User continuing search, continue where we left off.
	if( !Q_strcasecmp( search, lastpartial ) )
	{
		for( ; pCvar; pCvar = pCvar->next )
		{
			if( !Q_strcmp( search, pCvar->name ) )
			{
				bool bFound = false;

				if( forward )
				{
					pCvar = pCvar->next;
					bFound = true;
				}
				else
				{
					cvar_t* pPrev = Cvar_FindPrevVar( pCvar->name );

					bFound = pPrev != nullptr;

					if( pPrev )
						pCvar = pPrev;
					else
						pCvar = cvar_vars;
				}

				if( bFound )
				{
					Q_strncpy( lastpartial, pCvar->name, ARRAYSIZE( lastpartial ) );
					return pCvar->name;
				}
			}
		}
	}

	// check functions
	for( ; pCvar; pCvar = pCvar->next )
	{
		if( !Q_strncmp( search, pCvar->name, len ) )
		{
			Q_strncpy( lastpartial, pCvar->name, ARRAYSIZE( lastpartial ) );
			return pCvar->name;
		}
	}

	return nullptr;
}

void Cvar_DirectSet( cvar_t* var, char* value )
{
	if( !value || !var )
		return;

	char szNew[ 1024 ];

	const char* pszNewValue = value;

	if( var->flags & FCVAR_PRINTABLEONLY )
	{
		szNew[ 0 ] = '\0';

		char* i = szNew;

		if( Q_UnicodeValidate( value ) )
		{
			Q_strncpy( szNew, value, ARRAYSIZE( szNew ) );
		}
		else
		{
			for( const char* pszNext = value; *pszNext; ++pszNext )
			{
				if( ( unsigned char ) ( *pszNext - ' ' ) <= 0x5Eu )
					*i++ = *pszNext;
			}

			*i = '\0';
		}

		if( !Q_UnicodeValidate( i ) )
			Q_UnicodeRepair( i );

		if( !Q_strlen( szNew ) )
			Q_strcpy( szNew, "empty" );

		pszNewValue = szNew;
	}

	if( var->flags & FCVAR_NOEXTRAWHITEPACE )
	{
		if( pszNewValue != szNew )
			Q_strncpy( szNew, pszNewValue, ARRAYSIZE( szNew ) );

		Q_StripUnprintableAndSpace( szNew );
	}

	const bool bChanged = Q_strcmp( var->string, pszNewValue ) != 0;

	if( var->flags & FCVAR_USERINFO )
	{
		if( cls.state != ca_dedicated
			|| ( Info_SetValueForKey( Info_Serverinfo(), var->name, pszNewValue, 512 ),
				 SV_BroadcastCommand(const_cast<char*>("fullserverinfo \"%s\"\n"), Info_Serverinfo() ),
				 cls.state != ca_dedicated))
		{
			Info_SetValueForKey( cls.userinfo, var->name, pszNewValue, 256 );

			if (!bChanged)
			{
				Z_Free(var->string);
				var->string = (char *)Z_Malloc(Q_strlen(pszNewValue) + 1);
				Q_strcpy(var->string, pszNewValue);
				var->value = Q_atof(var->string);
				return;
			}

			if( bChanged && cls.state > ca_connecting )
			{
				MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
				SZ_Print( &cls.netchan.message, va( const_cast<char*>("setinfo \"%s\" \"%s\"\n"), var->name, pszNewValue ) );
			}
		}
	}

	if( ( var->flags & FCVAR_SERVER ) && bChanged )
	{
		if( !( var->flags & FCVAR_UNLOGGED ) )
		{
			if( var->flags & FCVAR_PROTECTED )
			{
				Log_Printf( "Server cvar \"%s\" = \"%s\"\n", var->name, "***PROTECTED***" );
				SV_BroadcastPrintf( "\"%s\" changed to \"%s\"\n", var->name, "***PROTECTED***" );
			}
			else
			{
				Log_Printf( "Server cvar \"%s\" = \"%s\"\n", var->name, pszNewValue );
				SV_BroadcastPrintf( "\"%s\" changed to \"%s\"\n", var->name, pszNewValue );
			}
		}

		const char* pszPrintValue = pszNewValue;

		if( var->flags & FCVAR_PROTECTED )
		{
			pszPrintValue = "0";

			if( Q_strlen( pszNewValue ) > 0 )
			{
				if( Q_strcasecmp( pszNewValue, "none" ) )
					pszPrintValue = "1";
			}
		}

		Steam_SetCVar( var->name, pszPrintValue );
	}
	
	Z_Free( var->string );
	var->string = reinterpret_cast<char*>( Z_Malloc( Q_strlen( pszNewValue ) + 1 ) );
	Q_strcpy( var->string, pszNewValue );
	var->value = Q_atof( var->string );
}

void Cvar_Set( char* var_name, char* value )
{
	cvar_t* var = Cvar_FindVar( var_name );

	if( !var )
	{	// there is an error in C code if this happens
		Con_Printf( const_cast<char*>("Cvar_Set: variable %s not found\n"), var_name );
		return;
	}

	Cvar_DirectSet( var, value );

	for (cvarhook_t* pHook = cvar_hooks; pHook; pHook = pHook->next)
	{
		if (pHook->cvar == var)
		{
			pHook->hook(var);
			break;
		}
	}
}

void Cvar_SetValue( char* var_name, float value )
{
	char val[ 32 ];

	const int intValue = static_cast<int>( floor( value ) );

	//Set as integer if it's a whole number, to within a small margin of error.
	if( fabs( value - intValue ) < 0.000001 )
	{
		snprintf( val, ARRAYSIZE( val ), "%d", intValue );
	}
	else
	{
		snprintf( val, ARRAYSIZE( val ), "%f", value );
	}

	Cvar_Set( var_name, val );
}

void Cvar_RegisterVariable( cvar_t* variable )
{
	// first check to see if it has allready been defined
	if( Cvar_FindVar( variable->name ) )
	{
		Con_Printf( const_cast<char*>("Can't register variable %s, already defined\n"), variable->name );
		return;
	}

	// check for overlap with a command
	if( Cmd_Exists( variable->name ) )
	{
		Con_Printf( const_cast<char*>("Cvar_RegisterVariable: %s is a command\n"), variable->name );
		return;
	}

	// copy the value off, because future sets will Z_Free it
	char* oldstr = variable->string;
	variable->string = reinterpret_cast<char*>( Z_Malloc( Q_strlen( variable->string ) + 1 ) );
	Q_strcpy( variable->string, oldstr );
	variable->value = Q_atof( variable->string );

	//Insert into list in alphabetical order.
	cvar_t dummyvar;

	dummyvar.name = const_cast<char*>(" ");
	dummyvar.next = cvar_vars;

	cvar_t* pPrev = &dummyvar;
	cvar_t* pNext = cvar_vars;

	while( pNext && stricmp( pNext->name, variable->name ) <= 0 )
	{
		pPrev = pNext;
		pNext = pNext->next;
	}

	// link the variable in
	pPrev->next = variable;
	variable->next = pNext;
	cvar_vars = dummyvar.next;
}

void Cvar_RemoveHudCvars()
{
	if( !cvar_vars )
		return;

	cvar_t* pPrev = nullptr;
	cvar_t* pCvar = cvar_vars;
	cvar_t* pNext;

	while( pCvar )
	{
		pNext = pCvar->next;

		if( pCvar->flags & FCVAR_CLIENTDLL )
		{
			Z_Free( pCvar->string );
			Z_Free( pCvar );
		}
		else
		{
			pCvar->next = pPrev;
			pPrev = pCvar;
		}

		pCvar = pNext;
	}

	pCvar = pPrev;
	pPrev = NULL;

	while (pCvar)
	{
		pNext = pCvar->next;
		pCvar->next = pPrev;
		pPrev = pCvar;
		pCvar = pNext;
	}

	cvar_vars = pPrev;
}

const char* Cvar_IsMultipleTokens( const char* varname )
{
	static char firstToken[ 512 + 4 ];

	firstToken[ 0 ] = '\0';

	char* pszData = const_cast<char*>( varname );

	int count = 0;

	while( 1 )
	{
		pszData = COM_Parse( pszData );

		if( Q_strlen( com_token ) <= 0 )
			break;

		if( count )
		{
			++count;
		}
		else
		{
			count = 1;
			Q_strncpy( firstToken, com_token, ARRAYSIZE( firstToken ) );
		}
	}

	if( count != 1 )
		return firstToken;

	return nullptr;
}

qboolean Cvar_CommandWithPrivilegeCheck(qboolean bIsPrivileged)
{
	const char* pszToken = Cvar_IsMultipleTokens(Cmd_Argv(0));

	const bool bIsMultipleTokens = pszToken != nullptr;

	if (!pszToken)
		pszToken = Cmd_Argv(0);

	cvar_t* pCvar = Cvar_FindVar(pszToken);

	if (!pCvar)
		return false;

	if (!bIsPrivileged)
	{
		if (pCvar->flags & FCVAR_PRIVILEGED)
		{
			Con_Printf(const_cast<char*>("%s is a privileged variable\n"), pCvar->name);
			return true;
		}

		if (Cvar_VariableValue(const_cast<char*>("cl_filterstuffcmd")) > 0)
		{
			const char* rgpchFilterPrefixes[] = { "cl_", "gl_", "m_", "r_", "hud_" };
			const int filterEls = sizeof(rgpchFilterPrefixes) / sizeof(rgpchFilterPrefixes[0]);

			if (pCvar->flags & FCVAR_FILTERABLE)
			{
				Con_Printf(const_cast<char*>("%s is a privileged variable\n"), pCvar->name);
				return true;
			}

			for (int i = 0; i < filterEls; i++)
			{
				// compare addresses
				if (Q_stristr(pCvar->name, rgpchFilterPrefixes[i]) == pCvar->name)
				{
					Con_Printf(const_cast<char*>("%s is a privileged variable\n"), pCvar->name);
					return true;
				}
			}
		}
	}

	// perform a variable print or set
	if (bIsMultipleTokens || Cmd_Argc() == 1)
	{
		Con_Printf(const_cast<char*>("\"%s\" is \"%s\"\n"), pCvar->name, pCvar->string);
		return true;
	}

	if ((pCvar->flags & FCVAR_SPONLY) && (unsigned int)cls.state > 1 && cl.maxclients > 1)
	{
		Con_Printf(const_cast<char*>("Can't set %s in multiplayer\n"), pCvar->name);
		return true;
	}

	//As ridiculous as this seems, this is what the engine does.
	Cvar_Set(pCvar->name, Cmd_Argv(1));
	return true;
}

qboolean Cvar_Command()
{
	return Cvar_CommandWithPrivilegeCheck(true);
}

void Cvar_WriteVariables( FileHandle_t f )
{
	for( cvar_t* var = cvar_vars; var; var = var->next )
	{
		if( ( var->flags & FCVAR_ARCHIVE ) )
		{
			FS_FPrintf( f, "%s \"%s\"\n", var->name, var->string );
		}
	}
}

void Cmd_CvarListPrintCvar( cvar_t *var, FileHandle_t f )
{
	char szOutstr[ 256 ];

	const int intValue = static_cast<int>( floor( var->value ) );

	if( var->value == intValue )
	{
		snprintf( szOutstr, ARRAYSIZE( szOutstr ) - 11, "%-15s : %8i", var->name, intValue );
	}
	else
	{
		snprintf( szOutstr, ARRAYSIZE( szOutstr ) - 11, "%-15s : %8.3f", var->name, var->value );
	}

	if( var->flags & FCVAR_ARCHIVE )
	{
		Q_strcat( szOutstr, const_cast<char*>(", a") );
	}

	if( var->flags & FCVAR_SERVER )
	{
		Q_strcat( szOutstr, const_cast<char*>(", sv") );
	}

	if( var->flags & FCVAR_USERINFO )
	{
		Q_strcat( szOutstr, const_cast<char*>(", i") );
	}

	Q_strcat( szOutstr, const_cast<char*>("\n") );

	Con_Printf( const_cast<char*>("%s"), szOutstr );

	if( f != FILESYSTEM_INVALID_HANDLE )
		FS_FPrintf( f, "%s", szOutstr );
}

void Cmd_CvarList_f()
{
	FileHandle_t f = FILESYSTEM_INVALID_HANDLE;
	bool bLogging = false;	//Output to file
	bool bArchive = false;	//Only print archived cvars
	bool bSOnly = false;	//Only print server cvars
	char szTemp[ 256 ];

	const char* pszPartial = nullptr;
	int iPartialLength = 0;
	bool bPartialMatch = false;	//Whether this is a partial match. If logging, this is false

	const int iArgC = Cmd_Argc();

	if( iArgC >= 2 )
	{
		const char* pszCommand = Cmd_Argv( 1 );

		if( !Q_strcasecmp( pszCommand, "?" ) )
		{
			Con_Printf( const_cast<char*>("CvarList           : List all cvars\nCvarList [Partial] : List cvars starting with 'Partial'\nCvarList log [Partial] : Logs cvars to file \"cvarlist.txt\" in the gamedir.\n") );
			return;
		}

		if( !Q_strcasecmp( pszCommand, "log" ) )
		{
			int count = 0;

			FileHandle_t hTestFile;

			while( 1 )
			{
				snprintf( szTemp, ARRAYSIZE( szTemp ), "cvarlist%02d.txt", count );
				COM_FixSlashes( szTemp );

				hTestFile = FS_Open( szTemp, "r" );

				if( hTestFile == FILESYSTEM_INVALID_HANDLE )
					break;

				++count;

				FS_Close( hTestFile );

				if( count == 100 )
				{
					Con_Printf( const_cast<char*>("Can't cvarlist! Too many existing cvarlist output files in the gamedir!\n") );
					return;
				}
			}

			f = FS_Open( szTemp, "wt" );

			if( f == FILESYSTEM_INVALID_HANDLE )
			{
				Con_Printf( const_cast<char*>("Couldn't open [%s] for writing!\n"), szTemp );
				return;
			}

			bLogging = true;

			if( iArgC == 3 )
			{
				pszPartial = Cmd_Argv( 2 );
				iPartialLength = Q_strlen( pszPartial );
			}
		}
		else if( !Q_strcasecmp( pszCommand, "-a" ) )
		{
			bArchive = true;
		}
		else if( !Q_strcasecmp( pszCommand, "-s" ) )
		{
			bSOnly = true;
		}
		else
		{
			pszPartial = pszCommand;
			iPartialLength = Q_strlen( pszPartial );
			bPartialMatch = iArgC == 2 && pszPartial != nullptr;
		}
	}

	int count = 0;
	Con_Printf( const_cast<char*>("CVar List\n--------------\n") );

	for( cvar_t* pCvar = cvar_vars; pCvar; pCvar = pCvar->next )
	{
		if( bArchive && !( pCvar->flags & FCVAR_ARCHIVE ) )
			continue;

		if( bSOnly && !( pCvar->flags & FCVAR_SERVER ) )
			continue;

		if( pszPartial && Q_strncasecmp( pCvar->name, pszPartial, iPartialLength ) )
			continue;

		++count;
		Cmd_CvarListPrintCvar( pCvar, f );
	}

	if( bPartialMatch && *pszPartial )
		Con_Printf( const_cast<char*>("--------------\n%3i CVars for [%s]\nCvarList ? for syntax\n"), count, pszPartial );
	else
		Con_Printf( const_cast<char*>("--------------\n%3i Total CVars\nCvarList ? for syntax\n"), count );

	if( bLogging )
	{
		FS_Close( f );
		Con_Printf( const_cast<char*>("cvarlist logged to %s\n"), szTemp );
	}
}

int Cvar_CountServerVariables()
{
	int count = 0;

	for( cvar_t* pCvar = cvar_vars; pCvar; pCvar = pCvar->next )
	{
		if( pCvar->flags & FCVAR_SERVER )
			++count;
	}

	return count;
}

void Cvar_UnlinkExternals()
{
	if( !cvar_vars )
		return;

	cvar_t* pCvar = cvar_vars;
	cvar_t** ppNext = &cvar_vars;

	while( pCvar )
	{
		if( pCvar->flags & FCVAR_EXTDLL )
		{
			*ppNext = pCvar->next;
			pCvar = pCvar->next;
		}
		else
		{
			ppNext = &pCvar->next;
			pCvar = pCvar->next;
		}
	}
}

void Cvar_CmdInit()
{
	Cmd_AddCommand( const_cast<char*>("cvarlist"), Cmd_CvarList_f );
}

qboolean Cvar_HookVariable(const char* var_name, cvarhook_t* pHook)
{
	cvar_t* cvar;

	if (!pHook || !pHook->hook)
		return FALSE;

	if (pHook->cvar || pHook->next)
		return FALSE;

	cvar = Cvar_FindVar(var_name);
	if (!cvar)
		return FALSE;

	cvarhook_t* pCur = cvar_hooks;
	pHook->cvar = cvar;

	if (pCur)
	{
		while (pCur->next)
			pCur = pCur->next;

		pCur->next = pHook;
	}
	else
	{
		// First in chain is null, assign pHook to it
		cvar_hooks = pHook;
	}

	return TRUE;
}

