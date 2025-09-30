#include "quakedef.h"
#include "client.h"
#include "cl_ents.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "cl_spectator.h"
#include "pm_movevars.h"
#include "pmove.h"

overviewInfo_t gDevOverview;
local_state_t spectatorState;

cvar_t dev_overview = { const_cast<char*>("dev_overview"), const_cast<char*>("0") };

BOOL CL_IsSpectateOnly()
{
	RecEngCL_IsSpectateOnly();

	return cls.spectator != 0;
}

BOOL CL_IsDevOverviewMode()
{
	return dev_overview.value && (sv_cheats.value || cls.spectator);
}

void CL_CalculateDevOverviewParameters()
{
	float xSize;
	float ySize;

	float xMax = -32000.0;
	float yMax = -32000.0;
	float zMax = -32000.0;

	float xMin = 32000.0;
	float yMin = 32000.0;
	float zMin = 32000.0;

	for (int i = 0; i < cl.worldmodel->numvertexes; i++)
	{
		mvertex_t vertex = cl.worldmodel->vertexes[i];

		xMax = max(xMax, vertex.position[0]);
		yMax = max(yMax, vertex.position[1]);
		zMax = max(zMax, vertex.position[2]);

		xMin = min(xMin, vertex.position[0]);
		yMin = min(yMin, vertex.position[1]);
		zMin = min(zMin, vertex.position[2]);
	}

	gDevOverview.z_min = zMax + 1.0;
	gDevOverview.z_max = zMin - 1.0;

	gDevOverview.origin[0] = (xMax + xMin) * 0.5;
	gDevOverview.origin[1] = (yMax + yMin) * 0.5;
	gDevOverview.origin[2] = (zMax + zMin) * 0.5;

	ySize = yMax - yMin;
	xSize = xMax - xMin;

	// 1.3333 -> (4.0f / 3.0f) screen aspect
	// 8192 -> max coord resolution

	if( ySize > xSize )
	{
		gDevOverview.rotated = false;
		gDevOverview.zoom = min( 8192.0 / ( xSize * 1.3333 ), 8192.0 / ySize );
	}
	else
	{
		gDevOverview.rotated = true;
		gDevOverview.zoom = min( 8192.0 / ( ySize * 1.3333 ), 8192.0 / xSize );
	}
}

void CL_InitSpectator()
{
	Q_memset(&spectatorState, 0, sizeof(spectatorState));
	Q_memset(&gDevOverview, 0, sizeof(gDevOverview));

	CL_CalculateDevOverviewParameters();

	if (cls.spectator)
	{
		spectatorState.playerstate.friction = 1.0f;
		spectatorState.playerstate.gravity = 1.0f;
		spectatorState.playerstate.number = cl.playernum + 1;
		spectatorState.playerstate.usehull = 1;
		spectatorState.playerstate.movetype = MOVETYPE_NOCLIP;
		spectatorState.client.maxspeed = movevars.spectatormaxspeed;
	}
}

qboolean CL_AddEntityToPhysList( int entIndex )
{
	if( pmove->numphysent >= MAX_PHYSENTS )
	{
		Con_DPrintf( const_cast<char*>(__FUNCTION__ ":  pmove->numphysent >= MAX_PHYSENTS\n") );
		return false;
	}

	cl_entity_t* pEnt = &cl_entities[ entIndex ];

	if( pEnt->curstate.modelindex )
	{
		CL_AddStateToEntlist( &pmove->physents[ pmove->numphysent++ ], &pEnt->curstate );
		return true;
	}

	return false;
}

void CL_MoveSpectatorCamera()
{
	if( cls.state == ca_active && cls.spectator )
	{
		CL_SetUpPlayerPrediction( false, true );
		CL_SetSolidPlayers( cl.playernum );

		double time = cl.time;

		CL_RunUsercmd( &spectatorState, &spectatorState, &cl.cmd, true, &time, (unsigned int)( 100.0 * time ) );

		VectorCopy(spectatorState.client.velocity, cl.simvel);
		VectorCopy(spectatorState.playerstate.origin, cl.simorg);
		VectorCopy(spectatorState.client.punchangle, cl.punchangle);
		VectorCopy(spectatorState.client.view_ofs, cl.viewheight);
	}
}

void CL_SetDevOverView( refdef_t* refdef )
{
	gDevOverview.origin[ PITCH ] -= cl.cmd.sidemove / 128.0;
	gDevOverview.origin[ YAW ] -= cl.cmd.forwardmove / 128.0;

	if( cl.cmd.upmove > 0.0 )
		gDevOverview.z_min += 1.0;

	if( cl.cmd.upmove < 0.0 )
		gDevOverview.z_min -= 1.0;

	if( cl.cmd.buttons & IN_DUCK )
		gDevOverview.z_max -= 1.0;

	if( cl.cmd.buttons & IN_JUMP )
		gDevOverview.z_max += 1.0;

	if( cl.cmd.buttons & IN_ATTACK )
		gDevOverview.zoom += 0.01;

	if( cl.cmd.buttons & IN_ATTACK2 )
		gDevOverview.zoom -= 0.01;

	refdef->vieworg[ PITCH ] = gDevOverview.origin[ PITCH ];
	refdef->vieworg[ YAW ] = gDevOverview.origin[ YAW ];
	refdef->vieworg[ ROLL ] = 16000;

	refdef->onlyClientDraws = false;

	refdef->viewangles[ PITCH ] = 90;
	refdef->viewangles[ YAW ] = gDevOverview.rotated ? 90 : 0;
	refdef->viewangles[ ROLL ] = 0;

	if( dev_overview.value < 2 )
	{
		Con_Printf(
			const_cast<char*>(" Overview: Zoom %.2f, Map Origin (%.2f, %.2f, %.2f), Z Min %.2f, Z Max %.2f, Rotated %i\n"),
			gDevOverview.zoom,
			gDevOverview.origin[ PITCH ],
			gDevOverview.origin[ YAW ],
			gDevOverview.origin[ ROLL ],
			gDevOverview.z_min,
			gDevOverview.z_max,
			gDevOverview.rotated );
	}
}
