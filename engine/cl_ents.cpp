#include "quakedef.h"
#include "cl_ents.h"
#include "client.h"
#include "host.h"
#include "render.h"

#if !defined(GLQUAKE)
#include "r_local.h"
#else
#include "glquake.h"
#endif

#include "cl_main.h"
#include "r_part.h"
#include "cl_parse.h"
#include "r_studio.h"
#include "cl_pred.h"
#include "pmove.h"
#include "pr_cmds.h"
#if defined(GLQUAKE)
#include "gl_model.h"
#else
#include "model.h"
#endif
#include "common/entity_types.h"
#include "chase.h"
#include "pmovetst.h"
#include "cmodel.h"
#include "cl_tent.h"
#include "cl_parsefn.h"
#include "cl_extrap.h"
#include "cl_spectator.h"
#include "cl_parsefn.h"

#define FLASHLIGHT_DISTANCE 2000

cvar_t cl_showerror = { const_cast<char*>("cl_showerror"), const_cast<char*>("0") };
cvar_t cl_nosmooth = { const_cast<char*>("cl_nosmooth"), const_cast<char*>("0") };
cvar_t cl_smoothtime = { const_cast<char*>("cl_smoothtime"), const_cast<char*>("0.1"), FCVAR_ARCHIVE };
cvar_t cl_fixmodelinterpolationartifacts = { const_cast<char*>("cl_fixmodelinterpolationartifacts"), const_cast<char*>("1") };

qboolean CL_EntityTeleported(cl_entity_t *ent)
{
	if (ent->curstate.origin[0] - ent->prevstate.origin[0] <= 128.0
		&& ent->curstate.origin[1] - ent->prevstate.origin[1] <= 128.0)
	{
		return ent->curstate.origin[2] - ent->prevstate.origin[2] > 128.0;
	}
	return true;
}

qboolean CL_CompareTimestamps(float t1, float t2)
{
	// HL25 -> return (int(t1 * 1000.0) - int(1000.0 * t2) + 1) <= 2;
#if defined( FEATURE_HL25 )
	return (int(t1 * 1000.0) - int(1000.0 * t2) + 1) <= 2;
#else
	return abs(int(t1 * -1000.0) - int(-1000.0 * t2)) <= 1;
#endif // FEATURE_HL25
}

qboolean CL_ParametricMove(cl_entity_t *ent)
{
	vec3_t delta;
	float dt, t, scale;

	if (ent->curstate.starttime == 0.0 || ent->curstate.impacttime == 0.0)
		return false;

	VectorSubtract(ent->curstate.endpos, ent->curstate.startpos, delta);
	dt = ent->curstate.impacttime - ent->curstate.starttime;

	if (dt != 0.0)
	{
		t = max((double)ent->lastmove, cl.time);

		scale = clamp((t - ent->curstate.starttime) / dt, 0.0f, 1.0f);

		VectorMA(ent->curstate.startpos, scale, delta, ent->curstate.origin);

		ent->lastmove = t;
	}

	VectorCopy(ent->curstate.origin, ent->origin);
	VectorNormalize(delta);
	if (Length(delta) > 0.0)
		VectorAngles(delta, ent->curstate.angles);

	return true;
}

void CL_ProcessEntityUpdate(cl_entity_t *ent)
{
	int movetype;
	float msg_time;

	ent->index = ent->curstate.number;
	ent->model = CL_GetModelByIndex(ent->curstate.modelindex);
	COM_NormalizeAngles(ent->curstate.angles);

	if ((ent->model != NULL && ent->model->name[0] == '*')
		|| ((movetype = ent->curstate.movetype) != MOVETYPE_NONE
		&& movetype != MOVETYPE_STEP
		&& movetype != MOVETYPE_WALK
		&& movetype != MOVETYPE_FLY
		&& ent->curstate.starttime == 0.0
		&& ent->curstate.impacttime == 0.0) )
	{
		ent->curstate.animtime = ent->curstate.msg_time;
	}

	if (!CL_CompareTimestamps(ent->curstate.animtime, ent->prevstate.animtime) || ent->curstate.movetype == MOVETYPE_NONE)
	{
		R_UpdateLatchedVars(ent);
		CL_UpdatePositions(ent);
	}

	if (ent->player)
		ent->curstate.angles[PITCH] /= -3.0;

	VectorCopy(ent->curstate.origin, ent->origin);

	CL_ParametricMove(ent);

	VectorCopy(ent->curstate.angles, ent->angles);

	for (int i = 0; i < 4; i++)
	{
		VectorCopy(ent->curstate.origin, ent->attachment[i]);
	}
}

int CL_ParseDeltaHeader(qboolean *remove, qboolean *custom, int *numbase, qboolean *newbl, int *newblindex, qboolean full, int *offset)
{
	int numend;
	int onlyone = 0;

	*custom = false;
	*newbl = false;

	if (full)
	{
		*remove = false;
		if (MSG_ReadBits(1))
		{

			// num - *numbase == 1
			*numbase += 1;

			onlyone = 1;
		}
	}
	else
	{
		*remove = MSG_ReadBits(1) != 0;
	}

	// когда delta == 1, новое число не записывается в сообщение
	if (!onlyone)
	{
		if (MSG_ReadBits(1))
		{
			*numbase = MSG_ReadBits(11);
		}
		else
		{
			*numbase += MSG_ReadBits(6);
		}
	}

	if (!*remove)
	{
		*custom = MSG_ReadBits(1) != 0;

		if (cl.instanced_baseline_number)
		{
			*newbl = MSG_ReadBits(1) != 0;
			if (*newbl)
			{
				*newblindex = MSG_ReadBits(6);
			}
		}
		else
		{
			*newbl = false;
		}

		if (!full || *newbl)
			*offset = 0;
		else
		{
			*offset = MSG_ReadBits(1) == 0 ? 0 : MSG_ReadBits(6);
		}
	}

	return *numbase;
}

// убейся нахуй автор сие процедуры
//-----------------------------------------------------------------------------
// Purpose: Bad data was received, just flushes incoming delta data.
//-----------------------------------------------------------------------------
void CL_FlushEntityPacket(qboolean needbitencodingframe)
{
	qboolean remove, custom, newbl;
	int newindex, numbase, offset, newblindex, num;
	delta_t **ppcustom, **ppplayer, **ppentity;
	entity_state_t olde, newe;
	con_nprint_t np;

	remove = false;
	newblindex = 0;
	custom = false;
	newbl = false;
	numbase = 0;

	ppplayer = DELTA_LookupRegistration("entity_state_player_t");
	ppentity = DELTA_LookupRegistration("entity_state_t");
	ppcustom = DELTA_LookupRegistration("custom_entity_state_t");

	np.time_to_live = 1.0;
	np.color[0] = 1.0;
	np.color[1] = 0.2;
	np.color[2] = 0.0;
	np.index = 0;
	Con_NXPrintf(&np, "WARNING:  " __FUNCTION__);

	Q_memset(&olde, 0, sizeof(entity_state_t));

	cl.validsequence = 0;
	cl.frames[cl.parsecountmod].invalid = true;

	if (needbitencodingframe)
		MSG_StartBitReading(&net_message);

	while (true)
	{
		unsigned int peeklong = MSG_PeekBits(16);
		if (!peeklong)
			num = MSG_ReadBits(16);
		else
			num = CL_ParseDeltaHeader(&remove, &custom, &numbase, &newbl, &newblindex, false, &offset);

		if (msg_badread)
			Host_EndGame(const_cast<char*>("msg_badread in packetentities\n"));

		if (!num)
			break;

		if (!remove)
		{
			if (custom)
			{
				DELTA_ParseDelta((byte*)&olde, (byte*)&newe, *ppcustom, sizeof(entity_state_t));
			}
			else
			{
				if (CL_IsPlayerIndex(num))
					DELTA_ParseDelta((byte*)&olde, (byte*)&newe, *ppplayer, sizeof(entity_state_t));
				else
					DELTA_ParseDelta((byte*)&olde, (byte*)&newe, *ppentity, sizeof(entity_state_t));
			}
		}
	}

	if (needbitencodingframe)
		MSG_EndBitReading(&net_message);
}

void CL_ClearPacket(packet_entities_t* pPacket)
{
	if (pPacket->entities)
		Mem_Free(pPacket->entities);

	pPacket->entities = NULL;
	pPacket->num_entities = 0;
}

int CL_InterpolateModel(cl_entity_t *e)
{
	position_history_t *ph0, *ph1;
	vec3_t pos, angles;
	vec_t frac[3];
	float dt, d2t, scale, alpha;

	VectorCopy(e->curstate.origin, e->origin);
	VectorCopy(e->curstate.angles, e->angles);

	if (cls.timedemo)
		return 1;
	if (!e->model || e->model->name[0] == '*' && !r_bmodelinterp.value)
		return 1;
	if (cl.maxclients == 1)
		return 1;

	if (cl.moving && cl.onground == e->index)
		return 1;

	dt = cl.time - ex_interp.value;
	CL_FindInterpolationUpdates(e, dt, &ph0, &ph1, NULL);

	if (!ph0 || !ph1)
		return 0;

	d2t = dt - ph1->animtime;
	if (d2t < 0.0f)
		return 0;

	if (ph1->animtime == 0.0f || (VectorCompare(ph1->origin, vec3_origin) && !VectorCompare(ph0->origin, vec3_origin)))
	{
		VectorCopy(ph0->origin, e->origin);
		VectorCopy(ph0->angles, e->angles);
		return 0;
	}

	if (ph1->animtime == ph0->animtime)
	{
		VectorCopy(ph0->origin, e->origin);
		VectorCopy(ph0->angles, e->angles);
		return 1;
	}

	VectorSubtract(ph0->origin, ph1->origin, frac);

	scale = d2t / (ph0->animtime - ph1->animtime);

	if (scale < 0)
		return 0;

	scale = min(scale, 1.0f);

	VectorMA(ph1->origin, scale, frac, pos);

	for (int i = 0; i < 3; i++)
	{
		alpha = ph0->angles[i] - ph1->angles[i];

		if (alpha > 180.0f)
		{
			alpha -= 360.0f;
		}
		else if (alpha < -180.0f)
		{
			alpha += 360.0f;
		}

		angles[i] = ph1->angles[i] + alpha * scale;
	}

	COM_NormalizeAngles(angles);

	VectorCopy(pos, e->origin);
	VectorCopy(angles, e->angles);

	return 1;
}

void CL_ResetLatchedState(int pnum, packet_entities_t* pack, cl_entity_t* ent)
{
	if ((pack->flags[pnum >> 3] & byte(1 << (pnum & 7))) != 0)
	{
		VectorCopy(ent->curstate.origin, ent->latched.prevorigin);
		VectorCopy(ent->curstate.angles, ent->latched.prevangles);
		R_ResetLatched(ent, true);
		CL_ResetPositions(ent);
		if (ent->curstate.starttime != 0.0 && ent->curstate.impacttime != 0.0)
			ent->lastmove = cl.time;
	}
}

void CL_ProcessPlayerState(int playerindex, entity_state_t* state)
{
	entity_state_t* pent;

	pent = &cl.frames[cl.parsecountmod].playerstate[playerindex];
	pent->number = state->number;
	pent->messagenum = cl.parsecount;
	pent->msg_time = cl.mtime[0];
	ClientDLL_ProcessPlayerState(pent, state);
}

void CL_ProcessPacket(frame_t *frame)
{
	entity_state_t *p_prevstate, *p_curstate, *state;
	cl_entity_t *ent;

	for (int i = 0; i < frame->packet_entities.num_entities; i++)
	{
		state = &frame->packet_entities.entities[i];
		state->messagenum = cl.parsecount;
		state->msg_time = cl.mtime[0];

		ent = &cl_entities[state->number];
		ent->player = CL_IsPlayerIndex(state->number);

		if (state->number - 1 == cl.playernum)
			ClientDLL_TxferLocalOverrides(state, &frame->clientdata);

		p_prevstate = &ent->prevstate;
		p_curstate = &ent->curstate;

		*p_prevstate = *p_curstate;
		*p_curstate = *state;

		CL_ProcessEntityUpdate(ent);

		CL_ResetLatchedState(i, &frame->packet_entities, ent);

		if (ent->player)
		{
			int playerindex = state->number - 1;

			CL_ProcessPlayerState(playerindex, state);

			if (playerindex == cl.playernum)
				CL_CheckPredictionError();
		}
	}
}

qboolean CL_IsPlayerIndex(int index)
{
	return index >= 1 && index <= cl.maxclients;
}

void CL_SetPacketFlag(packet_entities_t* pack, int index, int flag)
{
	if (flag)
		pack->flags[index >> 3] |= 1 << (index & 7);
	else
		pack->flags[index >> 3] &= ~(1 << (index & 7));
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *frame - 
//-----------------------------------------------------------------------------
void CL_ResetFrameStats(frame_t* frame)
{
	frame->clientbytes = 0;
	frame->tentitybytes = 0;
	frame->soundbytes = 0;
	frame->eventbytes = 0;
	frame->msgbytes = 0;
	frame->packetentitybytes = 0;
	frame->playerinfobytes = 0;
	frame->usrbytes = 0;
	frame->voicebytes = 0;
}

/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata(void)
{
	int			i, j;
	int			command_ack;
	delta_t **ppdelta, **ppweapon;
	clientdata_t baseline;
	weapon_data_t wbaseline;
	entity_state_t *ps;
	float		latency;   // Our latency (round trip message time to server and back)
	frame_t		*frame;    // The frame we are parsing in.
	static int last_incoming_sequence, last_command_ack;
	int fromframe, idx;
	
	ppdelta = DELTA_LookupRegistration("clientdata_t");
	ppweapon = DELTA_LookupRegistration("weapon_data_t");

	Q_memset(&baseline, 0, sizeof(clientdata_t));

	// This is the last movement that the server ack'd
	command_ack = cls.netchan.incoming_acknowledged;

	// This is the frame update that this message corresponds to
	i = cls.netchan.incoming_sequence;

	// Did we drop some frames?
	if (i > last_incoming_sequence + 1)
	{
		// Mark as dropped
		for (j = last_incoming_sequence + 1;
			j < i;
			j++)
		{
			if (cl.frames[j & CL_UPDATE_MASK].receivedtime >= 0.0)
			{
				cl.frames[j & CL_UPDATE_MASK].receivedtime = -1.875f;
				cl.frames[j & CL_UPDATE_MASK].latency = 0;
			}
		}
	}

	// Ack'd incoming messages.
	cl.parsecount = i;
	// Index into window.
	cl.parsecountmod = cl.parsecount & CL_UPDATE_MASK;
	// Frame at index.
	frame = &cl.frames[cl.parsecountmod];

	// Mark network received time
	frame->time = cl.mtime[0];

	// Send time for that frame.
	parsecounttime = cl.commands[command_ack & CL_UPDATE_MASK].senttime;

	// Current time that we got a response to the command packet.
	cl.commands[command_ack & CL_UPDATE_MASK].receivedtime = realtime;

	// Time now that we are parsing.
	frame->receivedtime = realtime;

	CL_ResetFrameStats(frame);

	if (last_command_ack != -1)
	{
		if (cls.spectator)
			ClientDLL_TxferPredictionData(&spectatorState.playerstate, &spectatorState.playerstate, &spectatorState.client, &spectatorState.client,
			spectatorState.weapondata, spectatorState.weapondata);
		else
		{
			int last_predicted = (last_incoming_sequence + command_ack - last_command_ack) & CL_UPDATE_MASK;
			entity_state_t* ps = &frame->playerstate[cl.playernum];
			entity_state_t* pps = &cl.predicted_frames[last_predicted].playerstate;
			clientdata_t* pcd = &frame->clientdata;
			clientdata_t* ppcd = &cl.predicted_frames[last_predicted].client;
			weapon_data_t* wd = frame->weapondata;
			weapon_data_t* pwd = cl.predicted_frames[last_predicted].weapondata;

			ClientDLL_TxferPredictionData(ps, pps, pcd, ppcd, wd, pwd);
		}
	}

	g_flLatency = frame->latency = realtime - parsecounttime;

	last_incoming_sequence = i;
	last_command_ack = command_ack;

	if (cls.spectator)
	{
		cl.stats[STAT_HEALTH] = 1;
	}
	else
	{
		int bits;
		int moreweapons;
		clientdata_t* from;
		weapon_data_t* fdata;

		MSG_StartBitReading(&net_message);

		bits = MSG_ReadBits(1);

		if (bits)
		{
			// delta_sequence
			fromframe = MSG_ReadBits(8);
			from = &cl.frames[fromframe & CL_UPDATE_MASK].clientdata;
		}
		else
		{
			from = &baseline;
		}

		DELTA_ParseDelta((byte*)from, (byte*)&frame->clientdata, *ppdelta, sizeof(clientdata_t));
		Q_memset(&wbaseline, 0, sizeof(wbaseline));

		while (1)
		{
			moreweapons = MSG_ReadBits(1);
			if (!moreweapons)
				break;

			idx = MSG_ReadBits(6);

			if (bits)
				fdata = &cl.frames[fromframe & CL_UPDATE_MASK].weapondata[idx];
			else
				fdata = &wbaseline;

			DELTA_ParseDelta((byte*)fdata, (byte*)&frame->weapondata[idx], *ppweapon, sizeof(weapon_data_t));
		}

		MSG_EndBitReading(&net_message);
		Q_strncpy(cls.physinfo, frame->clientdata.physinfo, sizeof(cls.physinfo) - 1);
		cls.physinfo[sizeof(cls.physinfo) - 1] = 0;
		cl.stats[STAT_HEALTH] = frame->clientdata.health;
		cl.weapons = frame->clientdata.weapons;
		cl.maxspeed = frame->clientdata.maxspeed;
		cl.pushmsec = frame->clientdata.pushmsec;
	}
}

extern void DbgPrint(FILE*, const char* format, ...);
extern FILE* m_fMessages;

void CL_ParsePacketEntities(qboolean delta, int *playerbits)
{
	int newblindex, offset = 0, numbase, oldindex, newindex;
	delta_t **ppcustom, **ppentity, **ppplayer;
	qboolean remove, custom, newbl, uncompressed;
	entity_state_t *p_baseline;
	packet_entities_t *pents, dummy, *oldp;
	int entsinpacket, newp_number, newnum, num;
	int				oldpacket, newpacket;

	newblindex = 0;
	remove = false;
	custom = false;
	newbl = false;
	numbase = 0;
	ppentity = DELTA_LookupRegistration("entity_state_t");
	ppplayer = DELTA_LookupRegistration("entity_state_player_t");
	ppcustom = DELTA_LookupRegistration("custom_entity_state_t");

	entsinpacket = 1;
	newpacket = cl.parsecountmod;

	if (cls.signon == 1)
	{
		cls.signon = SIGNONS;
		CL_SignonReply();
	}

	pents = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	// Retrieve size of packet.
	newp_number = MSG_ReadShort();
	// Make sure that we always have at least one slot allocated in memory.
	entsinpacket = max(entsinpacket, newp_number);

	if (pents->entities != NULL)
		Mem_Free((void*)pents->entities);

	pents->entities = (entity_state_t*)Mem_ZeroMalloc(sizeof(entity_state_t) * entsinpacket);
	pents->num_entities = newp_number;
	Q_memset(pents->flags, 0, sizeof(pents->flags));

	// If this is a delta update, grab the sequence value and make sure it's the one we requested.
	if (delta)
	{
		// Frame we are delta'ing from.
		oldpacket = MSG_ReadByte();
	}
	else
	{
		// Delta too old or is initial message
		oldpacket = -1;

		if (cls.demowaiting)
		{
			cls.demowaiting = false;
			MSG_WriteChar(&cls.netchan.message, clc_stringcmd);
			MSG_WriteString(&cls.netchan.message, "fullupdate");
		}
	}

	uncompressed = false;

	// we can't append entities to the list
	if (!cl.max_edicts)
	{
		CL_FlushEntityPacket(true);
		CL_ClearPacket(pents);
		return;
	}

	// See if we can use the packet still
	if (oldpacket == -1)
	{
		// Use a fake old value with zero values.
		oldp = &dummy;

		// Clear out the client's entity states..
		Q_memset(&dummy, 0, sizeof(dummy));

		dummy.num_entities = 0;

		// Flag this is a full update
		uncompressed = true;
	}
	else
	{
		// Have we already looped around and flushed this info?
		if ((byte)(cls.netchan.incoming_sequence - oldpacket) >= CL_UPDATE_MASK)
		{
			CL_FlushEntityPacket(true);
			CL_ClearPacket(pents);
			return;
		}

		// Otherwise, mark where we are valid to and point to the packet entities we'll be updating from.
		oldp = &cl.frames[oldpacket & CL_UPDATE_MASK].packet_entities;
	}

	cl.validsequence = cls.netchan.incoming_sequence;

	MSG_StartBitReading(&net_message);
	newindex = 0;
	oldindex = 0;

	while (1)
	{
		// в этом пакете 16 бит заполненных нулями - завершение
		unsigned int peeklong = MSG_PeekBits(16);

		if (peeklong)
			newnum = CL_ParseDeltaHeader(&remove, &custom, &numbase, &newbl, &newblindex, uncompressed, &offset);
		else
			newnum = MSG_ReadBits(16);

		if (!newnum)
			break;

		if (oldindex < oldp->num_entities)
			num = oldp->entities[oldindex].number;
		else
			num = 9999;

		// установка индекса в случае наличия в кадре принятых ранее сущностей
		if (newnum > num)
		{
			if (uncompressed)
			{
				Con_Printf(const_cast<char*>("WARNING: oldcopy invalid on non-delta compressed update\n"));
				CL_FlushEntityPacket(false);
				CL_ClearPacket(pents);
			}
			else
			{
				for (; newnum > num; newindex++)
				{
					if (newindex >= MAX_PACKET_ENTITIES)
						Host_EndGame(const_cast<char*>(__FUNCTION__ ": newindex == MAX_PACKET_ENTITIES"));

					pents->entities[newindex] = oldp->entities[oldindex];

					oldindex++;

					if (oldindex < oldp->num_entities)
						num = oldp->entities[oldindex].number;
					else
						num = 9999;
				}
			}
		}


		if (newnum >= num)
		{
			if (newnum == num)
			{
				if (uncompressed)
				{
					cl.validsequence = 0;
					Con_Printf(const_cast<char*>("WARNING: delta on full update"));
				}

				if (remove)
				{
					R_KillDeadBeams(num);
					// изменение индекса после удаления сущности?
					oldindex++;
				}
				else
				{
					if (custom)
					{
						DELTA_ParseDelta((byte*)&oldp->entities[oldindex], (byte*)&pents->entities[newindex], *ppcustom, sizeof(entity_state_t));
						pents->entities[newindex].entityType = ENTITY_BEAM;
						pents->entities[newindex].number = newnum;

					}
					else
					{
						int sz;
						if (CL_IsPlayerIndex(newnum))
							sz = DELTA_ParseDelta((byte*)&oldp->entities[oldindex], (byte*)&pents->entities[newindex], *ppplayer, sizeof(entity_state_t));
						else
							sz = DELTA_ParseDelta((byte*)&oldp->entities[oldindex], (byte*)&pents->entities[newindex], *ppentity, sizeof(entity_state_t));

						pents->entities[newindex].entityType = ENTITY_NORMAL;
						if (CL_IsPlayerIndex(newnum) && playerbits)
							*playerbits += sz;

						pents->entities[newindex].number = newnum;
					}
					oldindex++;
					newindex++;
				}
			}
		}
		else if (remove)
		{
			// ...
			if (uncompressed)
			{
				Con_Printf(const_cast<char*>("WARNING: remove invalid on non-delta compressed update\n"));
				CL_FlushEntityPacket(false);
				CL_ClearPacket(pents);
				MSG_EndBitReading(&net_message);
				if (msg_badread)
					Host_EndGame(const_cast<char*>("msg_badread in packetentities\n"));
				CL_ProcessPacket(&cl.frames[newpacket]);
				return;
			}
		}
		else
		{
			if (newindex >= MAX_PACKET_ENTITIES)
				Host_EndGame(const_cast<char*>(__FUNCTION__ ": newindex == MAX_PACKET_ENTITIES"));

			if (newbl)
			{
				p_baseline = &cl.instanced_baseline[newblindex];
			}
			else if (offset)
			{
				p_baseline = &pents->entities[newindex - offset];
			}
			else
			{
				p_baseline = &cl_entities[newnum].baseline;
			}

			// создание новой сущности под данным индексом
			CL_EntityNum(newnum);

			if (custom)
			{
				DELTA_ParseDelta((byte*)p_baseline, (byte*)&pents->entities[newindex], *ppcustom, sizeof(entity_state_t));
				pents->entities[newindex].entityType = ENTITY_BEAM;
			}
			else
			{
				int sz;

				if (CL_IsPlayerIndex(newnum))
					sz = DELTA_ParseDelta((byte*)p_baseline, (byte*)&pents->entities[newindex], *ppplayer, sizeof(entity_state_t));
				else
					sz = DELTA_ParseDelta((byte*)p_baseline, (byte*)&pents->entities[newindex], *ppentity, sizeof(entity_state_t));

				pents->entities[newindex].entityType = ENTITY_NORMAL;

				if (CL_IsPlayerIndex(newnum) && playerbits)
					*playerbits += sz;
			}

			pents->entities[newindex].number = newnum;

			if (cl_needinstanced.value != 0.0 && newnum > cl.highentity && newbl == false)
			{
				Con_DPrintf(const_cast<char*>("Non-instanced %i %s without baseline\n"), newnum, pents->entities[newindex].modelindex ? cl.model_precache[pents->entities[newindex].modelindex]->name : "?");
			}

			if (!uncompressed || !cls.passive)
				CL_SetPacketFlag(pents, newindex, 1);

			newindex++;
		}

	}

	for (; oldindex < oldp->num_entities; oldindex++, newindex++)
	{
		if (newindex >= MAX_PACKET_ENTITIES)
			Host_EndGame(const_cast<char*>(__FUNCTION__ ": newindex == MAX_PACKET_ENTITIES"));

		pents->entities[newindex] = oldp->entities[oldindex];
	}

	MSG_EndBitReading(&net_message);

	if (msg_badread)
		Host_EndGame(const_cast<char*>("msg_badread in packetentities\n"));

	CL_ProcessPacket(&cl.frames[newpacket]);
}

void CL_PrintEntity(cl_entity_t* ent)
{

}

void CL_Particle(float* origin, int color, float life, int zpos, int zvel)
{
	particle_t* act, *part = free_particles;

	if (free_particles == NULL)
		return;

	free_particles = free_particles->next;
	act = active_particles;
	active_particles = part;
	part->next = act;
	part->die = life + cl.time;
	part->color = color;

#if !defined(GLQUAKE)
	part->packedColor = hlRGB((word*)host_basepal, color);
#endif

	part->type = pt_static;
	part->vel[0] = vec3_origin[0];
	part->vel[1] = vec3_origin[1];
	part->vel[2] = zvel + vec3_origin[2];
	part->org[0] = origin[0];
	part->org[1] = origin[1];
	part->org[2] = zpos + origin[2];
}

void CL_TouchLight(dlight_t* dl)
{
	int idx = dl - cl_dlights;
	if (idx < MAX_DLIGHTS)
		r_dlightchanged |= 1 << idx;
}

void CL_PlayerFlashlight()
{
	cl_entity_t* ent;
	dlight_t* dl;

	ent = cl_entities + cl.playernum + 1;

	if (ent->curstate.effects & (EF_BRIGHTLIGHT | EF_DIMLIGHT) && cl.worldmodel)
	{
		if (cl.pLight && cl.pLight->key == 1)
			dl = cl.pLight;
		else
			dl = efx.CL_AllocDlight(1);

		if (ent->curstate.effects & EF_BRIGHTLIGHT)
		{
			dl->color.r = dl->color.g = dl->color.b = 250;
			dl->radius = 400;
			VectorCopy(ent->origin, dl->origin);
			dl->origin[ROLL] += 16.0f;
		}
		else
		{
			pmtrace_t trace;
			vec3_t vecForward, end;
			float falloff;

			AngleVectors(r_playerViewportAngles, vecForward, 0, 0);

			VectorCopy(ent->origin, dl->origin);
			VectorAdd(dl->origin, cl.viewheight, dl->origin);
			VectorMA(dl->origin, FLASHLIGHT_DISTANCE, vecForward, end);
			// Trace a line outward, use studio box to avoid expensive studio hull calcs
			pmove->usehull = 2;
			trace = PM_PlayerTrace(dl->origin, end, PM_STUDIO_BOX, -1);
			if (trace.ent > 0)
			{
				// If we hit a studio model, light it at it's origin so it lights properly (no falloff)
				if (pmove->physents[trace.ent].studiomodel)
				{
					VectorCopy(pmove->physents[trace.ent].origin, trace.endpos);
				}
			}
			VectorCopy(trace.endpos, dl->origin);
			falloff = trace.fraction * FLASHLIGHT_DISTANCE;
			if (falloff < 500)
				falloff = 1.0;
			else
				falloff = 500.0 / (falloff);
			falloff *= falloff;
			dl->radius = 80;
			dl->color.r = dl->color.g = dl->color.b = 255 * falloff;

#if 0
			dlight_t* halo;
			halo = efx.CL_AllocDlight(2);
			halo->color.r = halo->color.g = halo->color.b = 60;
			halo->radius = 200;
			VectorCopy(ent->origin, halo->origin);
			halo->origin[2] += 16;
			halo->die = cl.time + 0.2;
#endif
		}

		cl.pLight = dl;
		dl->die = cl.time + 0.2f;
		CL_TouchLight(dl);
	}
	else
	{
		if (cl.pLight && cl.pLight->key == 1)
			cl.pLight->die = cl.time;

		cl.pLight = NULL;
	}

	if (cl_entities[cl.viewentity].curstate.effects & EF_MUZZLEFLASH)
		cl.viewent.curstate.effects |= EF_MUZZLEFLASH;
}

void CL_AddModelEffects(int keynum, cl_entity_t* ent, model_t* model)
{
	int flags;
	dlight_t* dl;
	vec3_t oldorigin;

	if (!model)
		return;

	VectorCopy(ent->latched.prevorigin, oldorigin);
	flags = ent->model->flags;

	if (flags & EF_GIB)
	{
		efx.R_RocketTrail(oldorigin, ent->origin, 2);
	}
	else if (flags & EF_ZOMGIB)
	{
		efx.R_RocketTrail(oldorigin, ent->origin, 4);
	}
	else if (flags & EF_TRACER)
	{
		efx.R_RocketTrail(oldorigin, ent->origin, 3);
	}
	else if (flags & EF_TRACER2)
	{
		efx.R_RocketTrail(oldorigin, ent->origin, 5);
	}
	else if (flags & EF_ROCKET)
	{
		efx.R_RocketTrail(oldorigin, ent->origin, 0);
		dl = efx.CL_AllocDlight(keynum);
		VectorCopy(ent->origin, dl->origin);
		dl->color.r = dl->color.g = dl->color.b = 200;
		dl->radius = 200.0;
		dl->die = cl.time + 0.01;
	}
	else if (flags & EF_GRENADE)
	{
		efx.R_RocketTrail(oldorigin, ent->origin, 1);
	}
	else if (flags & EF_TRACER3)
	{
		efx.R_RocketTrail(oldorigin, ent->origin, 6);
	}
}

void CL_LinkCustomEntity(cl_entity_t* ent, entity_state_t* state)
{
	entity_state_t* p_prevstate, * p_curstate;

	if (ent->model->type != mod_sprite)
		Sys_Error("Bad model on beam ( not sprite ) ");

	p_prevstate = &ent->prevstate;
	p_curstate = &ent->curstate;

	*p_prevstate = *p_curstate;

	ent->latched.prevsequence = ent->curstate.sequence;
	VectorCopy(ent->origin, ent->latched.prevorigin);
	VectorCopy(ent->angles, ent->latched.prevangles);

	if (!ClientDLL_AddEntity(ET_BEAM, &cl_entities[ent->index]))
		Con_DPrintf(const_cast<char*>("Overflow beam entity list!\n"));
}

void CL_LinkPacketEntities()
{
	cl_entity_t* ent;
	packet_entities_t* pack;
	entity_state_t* s1, * s2;
	float				f;
	vec3_t				old_origin;
	int					i;
	int					pnum;
	dlight_t* dl;
	entity_state_t* p_curstate;
	int domove;
	float animtime;

	pack = &cl.frames[cl.parsecountmod].packet_entities;

	for (pnum = 0; pnum < pack->num_entities; pnum++)
	{
		s1 = &pack->entities[pnum];
		s2 = s1;	// FIXME: no interpolation right now

		if (CL_IsPlayerIndex(s1->number) || !s1->modelindex || s1->effects & EF_NODRAW)
			continue;

		ent = &cl_entities[s1->number];
		p_curstate = &ent->curstate;

		*p_curstate = *s1;

		if (!ent->model)
		{
			Con_Printf(const_cast<char*>("Tried to link edict %i without model\n"), ent->index);
			continue;
		}

		if (ent->curstate.impacttime && ent->curstate.starttime)
			domove = 1;
		else
		{
			domove = 0;

			if (ent->curstate.animtime == ent->prevstate.animtime && !VectorCompare(ent->curstate.origin, ent->prevstate.origin))
				ent->lastmove = cl.time + 0.2;

			if (ent->curstate.eflags & EFLAG_SLERP)
				animtime = ent->curstate.animtime;

			if (ent->curstate.eflags & EFLAG_SLERP && ent->curstate.animtime && ent->model->type == mod_studio)
			{
				if (cl.time > ent->lastmove)
					ent->curstate.movetype = MOVETYPE_STEP;
				else
				{
					ent->curstate.movetype = MOVETYPE_NONE;
					R_ResetLatched(ent, true);

					if (cl_fixmodelinterpolationartifacts.value >= 1.0)
						ent->curstate.animtime = ent->latched.prevanimtime = animtime;

					VectorCopy(ent->curstate.origin, ent->latched.prevorigin);
					VectorCopy(ent->curstate.angles, ent->latched.prevangles);
				}
			}

			if (ent->model->name[0] == '*')
				CL_InterpolateModel(ent);
			else
			{
				if (domove)
				{
					CL_ParametricMove(ent);
					VectorCopy(ent->curstate.origin, ent->origin);
					VectorCopy(ent->curstate.angles, ent->angles);
				}
				else
				{
					if ((ent->curstate.movetype == MOVETYPE_NONE || ent->curstate.movetype == MOVETYPE_WALK || ent->curstate.movetype == MOVETYPE_STEP ||
						ent->curstate.movetype == MOVETYPE_FLY) &&
						(!g_bIsCStrike && !g_bIsCZero || ent->curstate.movetype != MOVETYPE_STEP || cls.netchan.remote_address.type == NA_LOOPBACK))
					{
						VectorCopy(ent->curstate.origin, ent->origin);
						VectorCopy(ent->curstate.angles, ent->angles);
					}
					else
					{
						if (!CL_InterpolateModel(ent))
							continue;
					}
				}
			}

			if (s1->entityType & ENTITY_NORMAL)
			{
				if (cl_numvisedicts == MAX_VISEDICTS)
					return;

				if (ent->curstate.aiment)
					ent->curstate.movetype = MOVETYPE_FOLLOW;

				if (ent->curstate.effects & EF_NOINTERP)
					R_ResetLatched(ent, false);

				if (CL_EntityTeleported(ent))
				{
					VectorCopy(ent->curstate.origin, ent->latched.prevorigin);
					VectorCopy(ent->curstate.angles, ent->latched.prevangles);
					CL_ResetPositions(ent);
				}

				if (ent->curstate.effects & EF_BRIGHTFIELD)
					efx.R_EntityParticles(ent);

				if (ent->index != 1)
				{
					if (ent->curstate.effects & EF_BRIGHTLIGHT)
					{
						dl = efx.CL_AllocDlight(ent->index);
						VectorCopy(ent->origin, dl->origin);
						dl->origin[ROLL] += 16.0f;
						dl->color.r = dl->color.g = dl->color.b = 250;
						dl->radius = RandomFloat(400, 431);
						dl->die = cl.time + 0.001f;
					}

					if (ent->curstate.effects & EF_DIMLIGHT)
					{
						dl = efx.CL_AllocDlight(ent->index);
						VectorCopy(ent->origin, dl->origin);
						dl->color.r = dl->color.g = dl->color.b = 100;
						dl->radius = RandomFloat(200, 231);
						dl->die = cl.time + 0.001f;
					}
				}

				if (ent->curstate.effects & EF_LIGHT)
					efx.R_RocketFlare(ent->origin);

				CL_AddModelEffects(s1->number, ent, ent->model);
				for (i = 0; i < 4; i++)
				{
					VectorCopy(ent->origin, ent->attachment[i]);
				}
				ClientDLL_AddEntity(ET_NORMAL, ent);
			}
			else
				CL_LinkCustomEntity(ent, s1);

		}
	}
}

void CL_LinkPlayers()
{
	int				i, j;
	player_info_t* info;
	entity_state_t* state;
	entity_state_t	exact;
	double			playertime;
	cl_entity_t* ent;
	int				msec;
	frame_t* frame;
	int				oldphysent;
	dlight_t* dl;

	frame = &cl.frames[cl.parsecountmod];

	for (j = 0, state = frame->playerstate; j < MAX_CLIENTS; j++, state++)
	{
		if (state->messagenum != cl.parsecount)
			continue;	// not present this frame

		if (!ClientDLL_IsThirdPerson() && cl.viewentity - 1 == j && !chase_active.value)
			continue;

		if (!state->modelindex || state->effects & EF_NODRAW)
			continue;

		if (cl_numvisedicts >= MAX_VISEDICTS)
			break;

		ent = &cl_entities[j + 1];

		if (ent->index != j + 1)
			ent->index = j + 1;

		if (cl.playernum == j)
		{
			for (int i = 0; i < 3; i++)
			{
				ent->origin[i] = ent->curstate.origin[i] = ent->prevstate.origin[i] = state->origin[i];
				ent->angles[i] = ent->curstate.angles[i];
			}
		}

		if (ent->curstate.effects & EF_NOINTERP)
			R_ResetLatched(ent, false);

		if (CL_EntityTeleported(ent))
		{
			VectorCopy(ent->curstate.origin, ent->latched.prevorigin);
			VectorCopy(ent->curstate.angles, ent->latched.prevangles);
			CL_ResetPositions(ent);
		}

		if (cl.playernum == j)
		{
			VectorCopy(cl.simorg, ent->origin);
		}
		else
		{
			VectorCopy(ent->curstate.origin, ent->origin);
			VectorCopy(ent->curstate.angles, ent->angles);
			CL_ComputePlayerOrigin(ent);
		}

		if (cl.viewentity - 1 != j)
		{
			if (ent->curstate.effects & EF_BRIGHTLIGHT)
			{
				dl = efx.CL_AllocDlight(4);
				VectorCopy(ent->origin, dl->origin);
				dl->origin[ROLL] += 16.0f;
				dl->color.r = dl->color.g = dl->color.b = 250;
				dl->radius = RandomFloat(400, 431);
				dl->die = cl.time + 0.001f;
			}

			if (ent->curstate.effects & EF_DIMLIGHT)
			{
				dl = efx.CL_AllocDlight(4);
				VectorCopy(ent->origin, dl->origin);
				dl->color.r = dl->color.g = dl->color.b = 100;
				dl->radius = RandomFloat(200, 231);
				dl->die = cl.time + 0.001f;
			}
		}

		for (i = 0; i < 4; i++)
		{
			VectorCopy(ent->origin, ent->attachment[i]);
		}
		ClientDLL_AddEntity(ET_PLAYER, ent);
	}

	CL_PlayerFlashlight();
}

void CL_AddStateToEntlist(physent_t* pe, entity_state_t* state)
{
	model_t* pModel;

	pe->info = state->number;

	if (CL_IsPlayerIndex(state->number))
		pe->player = state->number;

	VectorCopy(state->origin, pe->origin);
	VectorCopy(state->angles, pe->angles);
	VectorCopy(state->mins, pe->mins);
	VectorCopy(state->maxs, pe->maxs);

	pe->frame = state->frame;
	pe->sequence = state->sequence;
	pe->rendermode = state->rendermode;
	pe->skin = state->skin;
	pe->solid = state->solid;
	Q_memcpy(pe->controller, state->controller, sizeof(state->controller));
	Q_memcpy(pe->blending, state->blending, sizeof(state->blending));
	pe->movetype = state->movetype;
	pe->takedamage = 0;
	pe->blooddecal = 0;

	if (pe->solid == SOLID_BSP)
	{
		pModel = CL_GetModelByIndex(state->modelindex);

		pe->model = pModel;
		pe->studiomodel = NULL;

		Q_strncpy(pe->name, pModel->name, sizeof(pe->name));
		pe->name[sizeof(pe->name) - 1] = 0;
	}
	else if (pe->solid == SOLID_BBOX)
	{
		pe->studiomodel = NULL;
		pe->model = NULL;

		pModel = CL_GetModelByIndex(state->modelindex);

		Q_strncpy(pe->name, pModel->name, sizeof(pe->name));
		pe->name[sizeof(pe->name) - 1] = 0;

		if (pModel->flags & STUDIO_TRACE_HITBOX)
			pe->studiomodel = pModel;
	}
	else if (CL_GetModelByIndex(state->modelindex)->type == mod_studio)
	{
		pe->model = NULL;

		pModel = CL_GetModelByIndex(state->modelindex);

		pe->studiomodel = pModel;

		Q_strncpy(pe->name, pModel->name, sizeof(pe->name));
		pe->name[sizeof(pe->name) - 1] = 0;
	}
	else
	{
		pModel = CL_GetModelByIndex(state->modelindex);

		pe->studiomodel = NULL;
		pe->model = pModel;

		Q_strncpy(pe->name, pModel->name, sizeof(pe->name));
		pe->name[sizeof(pe->name) - 1] = 0;
	}

	pe->iuser1 = state->iuser1;
	pe->iuser2 = state->iuser2;
	pe->iuser3 = state->iuser3;
	pe->iuser4 = state->iuser4;
	pe->fuser1 = state->fuser1;
	pe->fuser2 = state->fuser2;
	pe->fuser3 = state->fuser3;
	pe->fuser4 = state->fuser4;
	VectorCopy(state->vuser1, pe->vuser1);
	VectorCopy(state->vuser2, pe->vuser2);
	VectorCopy(state->vuser3, pe->vuser3);
	VectorCopy(state->vuser4, pe->vuser4);

	pe->blending[0] = state->blending[0];
	pe->blending[1] = state->blending[1];

	pe->controller[0] = state->controller[0];
	pe->controller[1] = state->controller[1];
	pe->controller[2] = state->controller[2];
	pe->controller[3] = state->controller[3];

	pe->rendermode = state->rendermode;
	pe->sequence = state->sequence;
	pe->team = state->team;
	pe->classnumber = state->playerclass;
}

void CL_SetSolidEntities()
{
	frame_t* pframe;
	entity_state_t* state;
	model_t* model;
	vec3_t delta;
	physent_t* pe;

	pmove = &g_clmove;
	Q_memset(&g_clmove.physents[0], 0, sizeof(physent_t));
	Q_memset(&pmove->visents[0], 0, sizeof(physent_t));

	pmove->physents[0].model = cl.worldmodel;

	if (cl.worldmodel)
	{
		Q_strncpy(pmove->physents[0].name, cl.worldmodel->name, sizeof(pmove->physents[0].name));
		pmove->physents[0].name[sizeof(pmove->physents[0].name) - 1] = 0;
	}

	pmove->physents[0].info = 0;
	pmove->physents[0].blooddecal = 0;
	VectorCopy(vec3_origin, pmove->physents[0].origin);
	pmove->numphysent = 1;
	pmove->physents[0].solid = SOLID_BSP;
	pmove->physents[0].movetype = MOVETYPE_NONE;
	pmove->numvisent = 1;
	pmove->physents[0].takedamage = 1;

	pmove->visents[0] = pmove->physents[0];
	pmove->nummoveent = 0;

	pframe = &cl.frames[cl.parsecountmod];

	for (int i = 0; i < pframe->packet_entities.num_entities; i++)
	{
		// индексация энтитей
		state = &pframe->packet_entities.entities[i];

		if (CL_IsPlayerIndex(state->number))
			continue;

		if (!state->modelindex)
			continue;

		model = CL_GetModelByIndex(state->modelindex);

		if (model == NULL)
			continue;

		// FIXME : инкрементация не должна выполняться относительно i
		// 29.12.23 -- к чему этот комментарий? я уже забыл...
		if (state->owner != 0 && cl.playernum == state->owner - 1)
			continue;


		if (model->hulls[1].firstclipnode || model->type == mod_studio)
		{
			pe = &pmove->visents[pmove->numvisent];
			CL_AddStateToEntlist(pe, state);
			pmove->numvisent++;
		}

		if (state->solid == SOLID_TRIGGER || (state->solid == SOLID_NOT && state->skin >= -1))
			continue;

		if (state->mins[ROLL] == 0.0 && state->maxs[ROLL] == 1.0)
			continue;

		VectorSubtract(state->maxs, state->mins, delta);

		if (Length(delta) == 0.0 || !model->hulls[1].firstclipnode && model->type != mod_studio)
			continue;

		if (state->solid || state->skin != -16)
		{
			if (pmove->numphysent < MAX_PHYSENTS)
			{
				pe = &pmove->physents[pmove->numphysent];
				pmove->numphysent++;
				CL_AddStateToEntlist(pe, state);
				continue;
			}
			else
			{
				Con_DPrintf(const_cast<char*>(__FUNCTION__ ":  pmove->numphysent >= MAX_PHYSENTS\n"));
			}
		}
		else
		{
			if (pmove->nummoveent < MAX_MOVEENTS)
			{
				pe = &pmove->moveents[pmove->nummoveent];
				pmove->nummoveent++;
				CL_AddStateToEntlist(pe, state);
				continue;
			}
			else
			{
				Con_DPrintf(const_cast<char*>(__FUNCTION__ ":  pmove->nummoveent >= MAX_MOVEENTS\n"));
			}
		}

	}
}


void CL_SetUpPlayerPrediction(qboolean dopred, qboolean bIncludeLocalClient)
{
	entity_state_t* state;
	predicted_player* pplayer;
	cl_entity_t* ent;

	pmove = &g_clmove;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		state = &cl.frames[cl.parsecountmod].playerstate[i];
		pplayer = &predicted_players[i];

		pplayer->active = false;

		if (state->messagenum != cl.parsecount || !state->modelindex)
			continue;

		if (!(state->effects & EF_NODRAW) || bIncludeLocalClient || cl.playernum == i)
		{
			pplayer->active = true;
			pplayer->movetype = state->movetype;
			pplayer->solid = state->solid;
			pplayer->usehull = state->usehull;

			if (cl.playernum == i)
			{
				VectorCopy(cl.frames[cl.parsecountmod].playerstate[i].origin, pplayer->origin);
				VectorCopy(cl.frames[cl.parsecountmod].playerstate[i].angles, pplayer->angles);
			}
			else
			{
				ent = cl_entities + i + 1;
				CL_ComputePlayerOrigin(ent);
				VectorCopy(ent->origin, pplayer->origin);
				VectorCopy(ent->angles, pplayer->angles);
			}
		}
	}
}

void CL_SetSolidPlayers( int playernum )
{
	predicted_player *pplayer;
	physent_t *pent;

	if (!cl_solid_players.value)
		return;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!((playernum == -1 && i == cl.playernum || predicted_players[i].active) && (i != playernum || playernum == -1)))
			continue;

		/*if (playernum == -1)
		{
			if (cl.playernum != i && !predicted_players[i].active)
				continue;
		}
		else if (!predicted_players[i].active || playernum == i)
			continue;*/

		pplayer = &predicted_players[i];

		pent = &pmove->physents[pmove->numphysent];

		if (!pplayer->solid)
			continue;

		Q_memset(pent, 0, sizeof(physent_t));

		pent->player = 1;
		pent->model = 0;

		VectorCopy(pplayer->origin, pent->origin);
		VectorCopy(player_mins[pplayer->usehull], pent->mins);
		VectorCopy(player_maxs[pplayer->usehull], pent->maxs);

		_snprintf(pent->name, sizeof(pent->name), "player %i", i);

		pent->skin = 0;
		pent->solid = pplayer->solid;
		pent->info = i + 1;
		pent->movetype = pplayer->movetype;
		pent->blooddecal = 0;
		pent->takedamage = 1;

		VectorCopy(pplayer->angles, pent->angles);

		pent->iuser1 = cl.frames[cl.parsecountmod].playerstate->iuser1;
		pent->iuser2 = cl.frames[cl.parsecountmod].playerstate->iuser2;
		pent->iuser3 = cl.frames[cl.parsecountmod].playerstate->iuser3;
		pent->iuser4 = cl.frames[cl.parsecountmod].playerstate->iuser4;
		pent->fuser1 = cl.frames[cl.parsecountmod].playerstate->fuser1;
		pent->fuser2 = cl.frames[cl.parsecountmod].playerstate->fuser2;
		pent->fuser3 = cl.frames[cl.parsecountmod].playerstate->fuser3;
		pent->fuser4 = cl.frames[cl.parsecountmod].playerstate->fuser4;

		VectorCopy(cl.frames[cl.parsecountmod].playerstate->vuser1, pent->vuser1);
		VectorCopy(cl.frames[cl.parsecountmod].playerstate->vuser2, pent->vuser2);
		VectorCopy(cl.frames[cl.parsecountmod].playerstate->vuser3, pent->vuser3);
		VectorCopy(cl.frames[cl.parsecountmod].playerstate->vuser4, pent->vuser4);

		pent->blending[0] = cl.frames[cl.parsecountmod].playerstate->blending[0];
		pent->blending[1] = cl.frames[cl.parsecountmod].playerstate->blending[1];

		pent->controller[0] = cl.frames[cl.parsecountmod].playerstate->controller[0];
		pent->controller[1] = cl.frames[cl.parsecountmod].playerstate->controller[1];
		pent->controller[2] = cl.frames[cl.parsecountmod].playerstate->controller[2];
		pent->controller[3] = cl.frames[cl.parsecountmod].playerstate->controller[3];

		pent->frame = cl.frames[cl.parsecountmod].playerstate->frame;
		pent->rendermode = cl.frames[cl.parsecountmod].playerstate->rendermode;
		pent->sequence = cl.frames[cl.parsecountmod].playerstate->sequence;
		pent->team = cl.frames[cl.parsecountmod].playerstate->team;
		pent->classnumber = cl.frames[cl.parsecountmod].playerstate->playerclass;

		pmove->numphysent++;
	}
}

void CL_MoveAiments()
{
	cl_entity_t* ent;

	for (int i = 0; i < cl_numvisedicts; i++)
	{
		ent = cl_visedicts[i];

		if (ent->curstate.aiment && ent->curstate.movetype == MOVETYPE_FOLLOW)
		{
			VectorCopy(cl_entities[ent->curstate.aiment].curstate.origin, ent->curstate.origin);
		}
	}
}

/*
===============
CL_LerpPoint
Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
float	CL_LerpPoint(void)
{
	float	f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if (!f || cls.timedemo)
	{
		f = cl.time - cl.oldtime;
		cl.time = cl.mtime[0];
		if (cls.demoplayback)
			cl.oldtime = cl.mtime[0] - f;
		return 1.0f;
	}

	if (ex_interp.value <= 0.001f)
		return 1.0f;

	return (cl.time - cl.mtime[0]) / ex_interp.value;
}

void CL_EmitEntities(void)
{
	cl_numvisedicts = 0;
	cl_numbeamentities = 0;

	if (cls.state != ca_active || !cl.validsequence || cl.frames[cl.parsecountmod].invalid)
		return;

	cl.commands[CL_UPDATE_MASK & (cls.netchan.outgoing_sequence - 1)].frame_lerp = CL_LerpPoint();

	if (cls.spectator)
		PVSMark(cl.worldmodel, Mod_LeafPVS(Mod_PointInLeaf(cl.simorg, cl.worldmodel), cl.worldmodel));

	CL_LinkPlayers();
	CL_LinkPacketEntities();
	CL_MoveAiments();
	ClientDLL_CreateEntities();
	CL_TempEntUpdate();

	if (cls.spectator)
	{
		r_oldviewleaf = 0;
		R_MarkLeaves();
	}

	CL_FireEvents();
}
