#ifndef ENGINE_CVAR_H
#define ENGINE_CVAR_H

#include <cstdio>

#include "cvardef.h"

typedef void (*pfnCvar_HookVariable_t)(cvar_t*);

typedef struct cvarhook_s
{
    pfnCvar_HookVariable_t hook;
    cvar_t * cvar;
    cvarhook_s* next;
} cvarhook_t;

/*

cvar_t variables are used to hold scalar or string variables that can be changed or displayed at the console or prog code as well as accessed directly
in C code.

it is sufficient to initialize a cvar_t with just the first two fields, or
you can add ,FCVAR_* flags for variables that you want saved to the configuration
file when the game is quit:

cvar_t	r_draworder = {"r_draworder","1"};
cvar_t	scr_screensize = {"screensize","1",FCVAR_ARCHIVE};

Cvars must be registered before use, or they will have a 0 value instead of the float interpretation of the string.  Generally, all cvar_t declarations should be registered in the apropriate init function before any console commands are executed:
Cvar_RegisterVariable (&host_framerate);


C code usually just references a cvar in place:
if ( r_draworder.value )

It could optionally ask for the value to be looked up for a string name:
if (Cvar_VariableValue ("r_draworder"))

The user can access cvars from the console in two ways:
r_draworder			prints the current value
r_draworder 0		sets the current value to 0
Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.
*/

void Cvar_Init();

void Cvar_Shutdown();

cvar_t* Cvar_FindVar( const char* var_name );

cvar_t* Cvar_FindPrevVar( char* var_name );

/**
*	Adds a freestanding variable to the variable list.
*	registers a cvar that already has the name, string, and optionally the
*	archive elements set.
*/
void Cvar_RegisterVariable( cvar_t* variable );

/**
*	equivalent to "<name> <variable>" typed at the console
*/
void Cvar_Set( char* var_name, char* value );

/**
*	expands value to a string and calls Cvar_Set
*/
void Cvar_SetValue( char* var_name, float value );

/**
*	returns 0 if not defined or non numeric
*/
float Cvar_VariableValue( char* var_name );

/**
*	returns 0 if not defined or non numeric
*/
int Cvar_VariableInt( char* var_name );

/**
*	returns an empty string if not defined
*/
char* Cvar_VariableString( char* var_name );

/**
*	attempts to match a partial variable name for command line completion
*	returns nullptr if nothing fits
*/
char* Cvar_CompleteVariable( char *partial, int forward );

/**
*	Sets a cvar's value.
*/
void Cvar_DirectSet( cvar_t* var, char* value );

/**
*	called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
*	command.  Returns true if the command was a variable reference that
*	was handled. (print or change)
*	Handles variable inspection and changing from the console
*/
qboolean Cvar_Command();

/**
*	Writes lines containing "set variable value" for all variables
*	with the archive flag set to true.
*/
void Cvar_WriteVariables( FileHandle_t f );

extern cvar_t* cvar_vars;

void Cvar_CmdInit();

void Cvar_RemoveHudCvars();
void Cvar_UnlinkExternals();

int Cvar_CountServerVariables();

qboolean Cvar_HookVariable(const char* var_name, cvarhook_t* pHook);

#endif //ENGINE_CVAR_H
