#include "quakedef.h"
#include "sv_user.h"
#include "host.h"
#include "pmove.h"
#include "r_studio.h"
#include "pr_edict.h"
#include "sv_main.h"
#include "world.h"
#include "sv_pmove.h"
#include "pr_cmds.h"
#include "sv_phys.h"
#include "sv_upld.h"

typedef struct command_s
{
	char *command;
} command_t;

sv_adjusted_positions_t truepositions[MAX_CLIENTS];
qboolean g_balreadymoved;

float s_LastFullUpdate[MAX_CLIENTS + 1];

cvar_t sv_voicecodec = { const_cast<char*>("sv_voicecodec"), const_cast<char*>("voice_speex") };
cvar_t sv_voicequality = { const_cast<char*>("sv_voicequality"), const_cast<char*>("3") };

edict_t* sv_player;

int giSkip;
qboolean nofind;

command_t clcommands[23] = { const_cast<char*>("status"), const_cast<char*>("god"), const_cast<char*>("notarget"), const_cast<char*>("fly"), const_cast<char*>("name"), const_cast<char*>("noclip"), const_cast<char*>("kill"), const_cast<char*>("pause"), const_cast<char*>("spawn"), const_cast<char*>("new"), const_cast<char*>("sendres"), const_cast<char*>("dropclient"), const_cast<char*>("kick"), const_cast<char*>("ping"), const_cast<char*>("dlfile"), const_cast<char*>("nextdl"), const_cast<char*>("setinfo"), const_cast<char*>("showinfo"), const_cast<char*>("sendents"), const_cast<char*>("fullupdate"), const_cast<char*>("setpause"), const_cast<char*>("unpause"), NULL };

cvar_t sv_edgefriction = { const_cast<char*>("edgefriction"), const_cast<char*>("2"), FCVAR_SERVER, };
cvar_t sv_maxspeed = { const_cast<char*>("sv_maxspeed"), const_cast<char*>("320"), FCVAR_SERVER };
cvar_t sv_accelerate = { const_cast<char*>("sv_accelerate"), const_cast<char*>("10"), FCVAR_SERVER };
cvar_t sv_footsteps = { const_cast<char*>("mp_footsteps"), const_cast<char*>("1"), FCVAR_SERVER };
cvar_t sv_rollspeed = { const_cast<char*>("sv_rollspeed"), const_cast<char*>("0.0") };
cvar_t sv_rollangle = { const_cast<char*>("sv_rollangle"), const_cast<char*>("0.0") };
cvar_t sv_unlag = { const_cast<char*>("sv_unlag"), const_cast<char*>("1") };
cvar_t sv_maxunlag = { const_cast<char*>("sv_maxunlag"), const_cast<char*>("0.5") };
cvar_t sv_unlagpush = { const_cast<char*>("sv_unlagpush"), const_cast<char*>("0.0") };
cvar_t sv_unlagsamples = { const_cast<char*>("sv_unlagsamples"), const_cast<char*>("1") };
cvar_t mp_consistency = { const_cast<char*>("mp_consistency"), const_cast<char*>("1"), FCVAR_SERVER };
cvar_t sv_voiceenable = { const_cast<char*>("sv_voiceenable"), const_cast<char*>("1"), FCVAR_SERVER | FCVAR_ARCHIVE };

void SV_GetTrueOrigin(int player, vec_t* origin);
void SV_GetTrueMinMax(int player, float** fmin, float** fmax);
void SV_SetupMove(client_t* host_client);
qboolean SV_FullUpdate_Internal();
void SV_RestoreMove(client_t* host_client);

void SV_ParseConsistencyResponse(client_t *pSenderClient)
{
	vec3_t mins;
	vec3_t maxs;
	vec3_t cmins;
	vec3_t cmaxs;
	unsigned char nullbuffer[32];
	unsigned char resbuffer[32];

	int length = 0;
	int c = 0;
	Q_memset(nullbuffer, 0, sizeof(nullbuffer));
	int value = MSG_ReadShort();
	if (value <= 0 || msg_readcount + value > net_message.cursize)
		Sys_Error("SV_ParseConsistencyResponse: invalid length: %d\n", value);
	COM_UnMunge(&net_message.data[msg_readcount], value, svs.spawncount);
	MSG_StartBitReading(&net_message);

	while (MSG_ReadBits(1))
	{
		int idx = MSG_ReadBits(12);
		if (idx < 0 || idx >= sv.num_resources)
		{
			c = -1;
			break;
		}

		resource_t *r = &sv.resourcelist[idx];
		if (!(r->ucFlags & RES_CHECKFILE))
		{
			c = -1;
			break;
		}

		Q_memcpy(resbuffer, r->rguc_reserved, sizeof(resbuffer));
		if (!Q_memcmp(resbuffer, nullbuffer, sizeof(resbuffer)))
		{
			if (MSG_ReadBits(32) != *(uint32 *)&r->rgucMD5_hash[0])
				c = idx + 1;
		}
		else
		{
			MSG_ReadBitData(cmins, 12);
			MSG_ReadBitData(cmaxs, 12);
			Q_memcpy(resbuffer, r->rguc_reserved, sizeof(resbuffer));
			COM_UnMunge(resbuffer, sizeof(resbuffer), svs.spawncount);
			FORCE_TYPE ft = (FORCE_TYPE)resbuffer[0];
			if (ft == force_model_samebounds)
			{
				Q_memcpy(mins, &resbuffer[1], 12);
				Q_memcpy(maxs, &resbuffer[13], 12);
				if (!VectorCompare(cmins, mins) || !VectorCompare(cmaxs, maxs))
					c = idx + 1;
			}
			else if (ft == force_model_specifybounds)
			{
				Q_memcpy(mins, &resbuffer[1], 12);
				Q_memcpy(maxs, &resbuffer[13], 12);
				for (int i = 0; i < 3; i++)
				{
					if (cmins[i] < mins[i] || cmaxs[i] > maxs[i])
					{
						c = idx + 1;
						break;
					}
				}
			}
			else if (ft == force_model_specifybounds_if_avail)
			{
				Q_memcpy(mins, &resbuffer[1], 12);
				Q_memcpy(maxs, &resbuffer[13], 12);
				if (cmins[0] != -1.0 || cmins[1] != -1.0 || cmins[2] != -1.0 || cmaxs[0] != -1.0 || cmaxs[1] != -1.0 || cmaxs[2] != -1.0)
				{
					for (int i = 0; i < 3; i++)
					{
						if (cmins[i] < mins[i] || cmaxs[i] > maxs[i])
						{
							c = idx + 1;
							break;
						}
					}
				}
			}
			else
			{
				msg_badread = 1;
				c = idx + 1;
				break;
			}

		}

		if (msg_badread)
			break;

		++length;
	}

	MSG_EndBitReading(&net_message);
	if (c < 0 || length != sv.num_consistency)
	{
		msg_badread = 1;
		Con_Printf(const_cast<char*>("SV_ParseConsistencyResponse:  %s:%s sent bad file data\n"), host_client->name, NET_AdrToString(host_client->netchan.remote_address));
		SV_DropClient(host_client, FALSE, const_cast<char*>("Bad file data"));
		return;
	}

	if (c > 0)
	{
		char dropmessage[256];
		if (gEntityInterface.pfnInconsistentFile(host_client->edict, sv.resourcelist[c - 1].szFileName, dropmessage))
		{
			if (Q_strlen(dropmessage) > 0)
				SV_ClientPrintf("%s", dropmessage);

			SV_DropClient(host_client, FALSE, const_cast<char*>("Bad file %s"), dropmessage);
		}
		return;
	}

	// Reset has_force_unmodified if we have received response from the client
	host_client->has_force_unmodified = FALSE;
}

qboolean SV_FileInConsistencyList(const char* filename, consistency_t** ppconsist)
{
	for (int i = 0; i < ARRAYSIZE(sv.consistency_list); i++)
	{
		if (!sv.consistency_list[i].filename)
			return 0;

		if (!Q_strcasecmp(filename, sv.consistency_list[i].filename))
		{
			if (ppconsist)
				*ppconsist = &sv.consistency_list[i];
			return 1;
		}
	}

	return 0;
}

int SV_TransferConsistencyInfo(void)
{
	consistency_t *pc;

	int c = 0;
	for (int i = 0; i < sv.num_resources; i++)
	{
		resource_t *r = &sv.resourcelist[i];
		if (r->ucFlags == (RES_CUSTOM | RES_REQUESTED | RES_POKEMONGFLAG) || (r->ucFlags & RES_CHECKFILE))
			continue;

		if (!SV_FileInConsistencyList(r->szFileName, &pc))
			continue;

		r->ucFlags |= RES_CHECKFILE;

		char filename[MAX_PATH];
		if (r->type != t_sound)
		{
			Q_strncpy(filename, r->szFileName, sizeof(filename) - 1);
			filename[sizeof(filename) - 1] = 0;
		}
		else
		{
			_snprintf(filename, sizeof(filename), "sound/%s", r->szFileName);
		}
		MD5_Hash_File(r->rgucMD5_hash, filename, FALSE, FALSE, NULL);

		if (r->type == t_model)
		{
			if (pc->check_type == force_model_samebounds)
			{
				vec3_t mins;
				vec3_t maxs;

				if (!R_GetStudioBounds(filename, mins, maxs))
					Host_Error(__FUNCTION__ ": Server unable to get bounds for %s\n", filename);

				Q_memcpy(&r->rguc_reserved[1], mins, sizeof(mins));
				Q_memcpy(&r->rguc_reserved[13], maxs, sizeof(maxs));
			}
			else if (pc->check_type == force_model_specifybounds || pc->check_type == force_model_specifybounds_if_avail)
			{
				Q_memcpy(&r->rguc_reserved[1], pc->mins, sizeof(pc->mins));
				Q_memcpy(&r->rguc_reserved[13], pc->maxs, sizeof(pc->maxs));
			}
			else
			{
				c++;
				continue;
			}
			r->rguc_reserved[0] = pc->check_type;
			COM_Munge(r->rguc_reserved, 32, svs.spawncount);
		}
		c++;
	}
	return c;
}

void SV_SendConsistencyList(sizebuf_t *msg)
{
	host_client->has_force_unmodified = FALSE;

	if (svs.maxclients == 1 || mp_consistency.value == 0.0f || sv.num_consistency == 0 || host_client->proxy)
	{
		MSG_WriteBits(0, 1);
		return;
	}

	host_client->has_force_unmodified = TRUE;

	int delta = 0;
	int lastcheck = 0;
	resource_t *r = sv.resourcelist;

	MSG_WriteBits(1, 1);

	for (int i = 0; i < sv.num_resources; i++, r++)
	{
		if (r && (r->ucFlags & RES_CHECKFILE) != 0)
		{
			MSG_WriteBits(1, 1);
			delta = i - lastcheck;

			if (delta > 31)
			{
				MSG_WriteBits(0, 1);
				MSG_WriteBits(i, 10);	// LIMIT: Here it write index, not a diff, with resolution of 10 bits. So, it limits not adjacent index to 1023 max.
			}
			else
			{
				// Write 5 bits index delta, so it is up to 31
				MSG_WriteBits(1, 1);
				MSG_WriteBits(delta, 5);
			}

			lastcheck = i;
		}
	}

	MSG_WriteBits(0, 1);
}

void SV_PreRunCmd(void)
{
}

void SV_CopyEdictToPhysent(physent_t* pe, int e, edict_t* check)
{
	model_t* pModel;

	VectorCopy(check->v.origin, pe->origin);

	pe->info = e;

	if (e < 1 || e > svs.maxclients)
	{
		pe->player = 0;
	}
	else
	{
		SV_GetTrueOrigin(e - 1, pe->origin);
		pe->player = pe->info;
	}

	VectorCopy(check->v.angles, pe->angles);

	pe->studiomodel = NULL;
	pe->rendermode = check->v.rendermode;

	if (check->v.solid == SOLID_BSP)
	{
		pe->model = sv.models[check->v.modelindex];
		Q_strncpy(pe->name, pe->model->name, sizeof(pe->name) - 1);
		pe->name[sizeof(pe->name) - 1] = 0;
	}
	else if (check->v.solid == SOLID_NOT)
	{
		if (check->v.modelindex)
		{
			pe->model = sv.models[check->v.modelindex];
			Q_strncpy(pe->name, pe->model->name, sizeof(pe->name) - 1);
			pe->name[sizeof(pe->name) - 1] = 0;
		}
		else
		{
			pe->model = NULL;
		}
	}
	else
	{
		pe->model = NULL;
		if (check->v.solid != SOLID_BBOX)
		{
			VectorCopy(check->v.mins, pe->mins);
			VectorCopy(check->v.maxs, pe->maxs);

			if (check->v.classname)
			{
				Q_strncpy(pe->name, &pr_strings[check->v.classname], sizeof(pe->name) - 1);
				pe->name[sizeof(pe->name) - 1] = 0;
			}
			else
			{
				pe->name[0] = '?';
				pe->name[1] = 0;
			}
		}
		else
		{
			if (check->v.modelindex)
			{
				pModel = sv.models[check->v.modelindex];
				if (pModel)
				{
					if (pModel->flags & STUDIO_TRACE_HITBOX)
						pe->studiomodel = pModel;

					Q_strncpy(pe->name, pModel->name, sizeof(pe->name) - 1);
					pe->name[sizeof(pe->name) - 1] = 0;
				}
			}

			VectorCopy(check->v.mins, pe->mins);
			VectorCopy(check->v.maxs, pe->maxs);
		}
	}

	pe->skin = check->v.skin;
	pe->frame = check->v.frame;
	pe->solid = check->v.solid;
	pe->sequence = check->v.sequence;
	pe->movetype = check->v.movetype;

	Q_memcpy(&pe->controller[0], &check->v.controller[0], sizeof(check->v.controller));
	Q_memcpy(&pe->blending[0], &check->v.blending[0], sizeof(check->v.blending));

	pe->iuser1 = check->v.iuser1;
	pe->iuser2 = check->v.iuser2;
	pe->iuser3 = check->v.iuser3;
	pe->iuser4 = check->v.iuser4;
	pe->fuser1 = check->v.fuser1;
	pe->fuser2 = check->v.fuser2;
	pe->fuser3 = check->v.fuser3;
	pe->fuser4 = check->v.fuser4;

	VectorCopy(check->v.vuser1, pe->vuser1);
	VectorCopy(check->v.vuser2, pe->vuser2);
	VectorCopy(check->v.vuser3, pe->vuser3);

	pe->takedamage = 0;
	pe->blooddecal = 0;

	VectorCopy(check->v.vuser4, pe->vuser4);
}

// stopline
void SV_AddLinksToPM_(areanode_t* node, float* pmove_mins, float* pmove_maxs)
{
	link_t* l;
	edict_t* check;
	int e;
	physent_t* ve;
	int i;
	link_t* next;
	float* fmax;
	float* fmin;
	physent_t* pe;

	for (l = node->solid_edicts.next; l != &node->solid_edicts; l = next)
	{
		check = (edict_t*)&l[-1];
		next = l->next;
		if (check->v.groupinfo)
		{
			if (g_groupop)
			{
				if (g_groupop == GROUP_OP_NAND && (check->v.groupinfo & sv_player->v.groupinfo))
					continue;
			}
			else
			{
				if (!(check->v.groupinfo & sv_player->v.groupinfo))
					continue;
			}
		}

		if (check->v.owner == sv_player)
			continue;

		if (check->v.solid != SOLID_BSP && check->v.solid != SOLID_BBOX && check->v.solid != SOLID_SLIDEBOX && check->v.solid != SOLID_NOT)
			continue;

		e = NUM_FOR_EDICT(check);
		ve = &pmove->visents[pmove->numvisent];
		pmove->numvisent = pmove->numvisent + 1;
		SV_CopyEdictToPhysent(ve, e, check);

		if ((check->v.solid == SOLID_NOT) && (!check->v.skin || !check->v.modelindex))
			continue;

		if ((check->v.flags & FL_MONSTERCLIP) && check->v.solid == SOLID_BSP)
			continue;

		if (check == sv_player)
			continue;

		if ((check->v.flags & FL_CLIENT) && check->v.health <= 0.0)
			continue;

		if (check->v.mins[2] == 0.0 && check->v.maxs[2] == 1.0 || Length(check->v.size) == 0.0)
			continue;

		fmin = check->v.absmin;
		fmax = check->v.absmax;
		if (check->v.flags & FL_CLIENT)
			SV_GetTrueMinMax(e - 1, &fmin, &fmax);

		for (i = 0; i < 3; i++)
		{
			if (fmin[i] > pmove_maxs[i] || fmax[i] < pmove_mins[i])
				break;
		}

		if (i != 3)
			continue;

		if (check->v.solid || check->v.skin != -16)
		{
			if (pmove->numphysent >= MAX_PHYSENTS)
			{
				Con_DPrintf(const_cast<char*>("SV_AddLinksToPM:  pmove->numphysent >= MAX_PHYSENTS\n"));
				return;
			}
			pe = &pmove->physents[pmove->numphysent++];
		}
		else
		{
			if (pmove->nummoveent >= MAX_MOVEENTS)
			{
				Con_DPrintf(const_cast<char*>("SV_AddLinksToPM:  pmove->nummoveent >= MAX_MOVEENTS\n"));
				continue;
			}
			pe = &pmove->moveents[pmove->nummoveent++];
		}

		Q_memcpy(pe, ve, sizeof(physent_t));
	}

	if (node->axis != -1)
	{
		if (pmove_maxs[node->axis] > node->dist)
			SV_AddLinksToPM_(node->children[0], pmove_mins, pmove_maxs);

		if (pmove_mins[node->axis] < node->dist)
			SV_AddLinksToPM_(node->children[1], pmove_mins, pmove_maxs);
	}
}

void SV_AddLinksToPM(areanode_t* node, vec_t* origin)
{
	vec3_t pmove_mins;
	vec3_t pmove_maxs;

	Q_memset(&pmove->physents[0], 0, sizeof(physent_t));
	Q_memset(&pmove->visents[0], 0, sizeof(physent_t));

	pmove->physents[0].model = sv.worldmodel;
	if (sv.worldmodel != NULL)
	{
		Q_strncpy(pmove->physents[0].name, sv.worldmodel->name, sizeof(pmove->physents[0].name) - 1);
		pmove->physents[0].name[sizeof(pmove->physents[0].name) - 1] = 0;
	}
	
	VectorCopy(vec3_origin, pmove->physents[0].origin);

	pmove->physents[0].info = 0;
	pmove->physents[0].solid = SOLID_BSP;
	pmove->physents[0].movetype = MOVETYPE_NONE;
	pmove->physents[0].takedamage = 1;
	pmove->physents[0].blooddecal = 0;

	pmove->numphysent = 1;
	
	pmove->numvisent = 1;
	
	pmove->visents[0] = pmove->physents[0];
	
	pmove->nummoveent = 0;

	for (int i = 0; i < 3; i++)
	{
		pmove_mins[i] = origin[i] - 256.0f;
		pmove_maxs[i] = origin[i] + 256.0f;
	}
	SV_AddLinksToPM_(node, pmove_mins, pmove_maxs);
}

void SV_PlayerRunPreThink(edict_t* player, float time)
{
	gGlobalVariables.time = time;
	gEntityInterface.pfnPlayerPreThink(player);
}

qboolean SV_PlayerRunThink(edict_t* ent, float frametime, double clienttimebase)
{
	float thinktime;

	if (!(ent->v.flags & (FL_DORMANT | FL_KILLME)))
	{
		thinktime = ent->v.nextthink;
		if (thinktime <= 0.0 || frametime + clienttimebase < thinktime)
			return true;

		if (thinktime < clienttimebase)
			thinktime = (float)clienttimebase;

		ent->v.nextthink = 0;
		gGlobalVariables.time = thinktime;
		gEntityInterface.pfnThink(ent);
	}

	if (ent->v.flags & FL_KILLME)
		ED_Free(ent);

	return ent->free == false;
}

void SV_CheckMovingGround(edict_t* player, float frametime)
{
	edict_t* groundentity;

	if (player->v.flags & FL_ONGROUND)
	{
		groundentity = player->v.groundentity;
		if (groundentity)
		{
			if (groundentity->v.flags & FL_CONVEYOR)
			{
				if (player->v.flags & FL_BASEVELOCITY)
					VectorMA(player->v.basevelocity, groundentity->v.speed, groundentity->v.movedir, player->v.basevelocity);
				else
					VectorScale(groundentity->v.movedir, groundentity->v.speed, player->v.basevelocity);
				player->v.flags |= FL_BASEVELOCITY;
			}
		}
	}

	if (!(player->v.flags & FL_BASEVELOCITY))
	{
		VectorMA(player->v.velocity, frametime * 0.5f + 1.0f, player->v.basevelocity, player->v.velocity);
		player->v.basevelocity[0] = player->v.basevelocity[1] = player->v.basevelocity[2] = 0;
	}

	player->v.flags &= ~FL_BASEVELOCITY;
}

void SV_ConvertPMTrace(trace_t* dest, pmtrace_t* src, edict_t* ent)
{
	dest->allsolid = src->allsolid;
	dest->startsolid = src->startsolid;
	dest->inopen = src->inopen;
	dest->inwater = src->inwater;
	dest->fraction = src->fraction;
	
	VectorCopy(src->endpos, dest->endpos);
	VectorCopy(src->plane.normal, dest->plane.normal);

	dest->plane.dist = src->plane.dist;
	dest->hitgroup = src->hitgroup;
	dest->ent = ent;
}

void SV_ForceFullClientsUpdate(void)
{
	byte data[9216];
	sizebuf_t msg;

	Q_memset(&msg, 0, sizeof(msg));
	msg.buffername = "Force Update";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof(data);

	for (int i = 0; i < svs.maxclients; ++i)
	{
		client_t* client = &svs.clients[i];
		if (client == host_client || client->active || client->connected || client->spawned)
			SV_FullClientUpdate(client, &msg);
	}

	Con_DPrintf(const_cast<char*>("Client %s started recording. Send full update.\n"), host_client->name);
	Netchan_CreateFragments(true, &host_client->netchan, &msg);
	Netchan_FragSend(&host_client->netchan);
}

void SV_RunCmd(usercmd_t* ucmd, int random_seed)
{
	int i;
	edict_t* ent;
	trace_t trace;
	float frametime;
	usercmd_t cmd = *ucmd;

	if (host_client->ignorecmdtime > realtime)
	{
		host_client->cmdtime += (double)ucmd->msec / 1000.0;
		return;
	}

	host_client->ignorecmdtime = 0;

	if ((float)cmd.msec > 50.0f)
	{
		cmd.msec = (byte)((float)ucmd->msec / 2.0f);
		SV_RunCmd(&cmd, random_seed);
		cmd.msec = (byte)((float)ucmd->msec / 2.0f);
		cmd.impulse = 0;
		SV_RunCmd(&cmd, random_seed);
		return;
	}

	if (host_client->fakeclient == false)
		SV_SetupMove(host_client);

	gEntityInterface.pfnCmdStart(sv_player, ucmd, random_seed);

	frametime = (float)ucmd->msec / 1000.0f;

	host_client->svtimebase += frametime;
	host_client->cmdtime += (double)frametime;

	if (ucmd->impulse)
	{
		sv_player->v.impulse = ucmd->impulse;

		// Disable fullupdate via impulse 204
		if (ucmd->impulse == 204)
			SV_FullUpdate_Internal();
		//	SV_ForceFullClientsUpdate();
	}

	sv_player->v.clbasevelocity[0] = sv_player->v.clbasevelocity[1] = sv_player->v.clbasevelocity[2] = 0;

	sv_player->v.button = ucmd->buttons;

	SV_CheckMovingGround(sv_player, frametime);
	VectorCopy(sv_player->v.v_angle, pmove->oldangles);

	if (!sv_player->v.fixangle)
	{
		VectorCopy(ucmd->viewangles, sv_player->v.v_angle);
	}

	SV_PlayerRunPreThink(sv_player, (float)host_client->svtimebase);
	SV_PlayerRunThink(sv_player, frametime, host_client->svtimebase);

	if (Length(sv_player->v.basevelocity) > 0.0)
	{
		VectorCopy(sv_player->v.basevelocity, sv_player->v.clbasevelocity);
	}

	pmove->server = TRUE;
	pmove->multiplayer = (svs.maxclients > 1) ? TRUE : FALSE;
	pmove->time = float(1000.0 * host_client->svtimebase);
	pmove->usehull = (sv_player->v.flags & FL_DUCKING) != 0;
	pmove->maxspeed = sv_maxspeed.value;
	pmove->clientmaxspeed = sv_player->v.maxspeed;
	pmove->flDuckTime = (float)sv_player->v.flDuckTime;
	pmove->bInDuck = sv_player->v.bInDuck;
	pmove->flTimeStepSound = sv_player->v.flTimeStepSound;
	pmove->iStepLeft = sv_player->v.iStepLeft;
	pmove->flFallVelocity = sv_player->v.flFallVelocity;
	pmove->flSwimTime = (float)sv_player->v.flSwimTime;
	pmove->oldbuttons = sv_player->v.oldbuttons;

	Q_strncpy(pmove->physinfo, host_client->physinfo, sizeof(pmove->physinfo) - 1);
	pmove->physinfo[sizeof(pmove->physinfo) - 1] = 0;

	VectorCopy(sv_player->v.velocity, pmove->velocity);

	VectorCopy(sv_player->v.movedir, pmove->movedir);

	VectorCopy(sv_player->v.v_angle, pmove->angles);

	VectorCopy(sv_player->v.basevelocity, pmove->basevelocity);

	VectorCopy(sv_player->v.view_ofs, pmove->view_ofs);

	VectorCopy(sv_player->v.punchangle, pmove->punchangle);

	pmove->deadflag = sv_player->v.deadflag;
	pmove->effects = sv_player->v.effects;
	pmove->gravity = sv_player->v.gravity;
	pmove->friction = sv_player->v.friction;
	pmove->spectator = 0;
	pmove->waterjumptime = sv_player->v.teleport_time;

	pmove->cmd = cmd;

	pmove->dead = sv_player->v.health <= 0.0;
	pmove->movetype = sv_player->v.movetype;
	pmove->flags = sv_player->v.flags;
	pmove->player_index = NUM_FOR_EDICT(sv_player) - 1;

	pmove->iuser1 = sv_player->v.iuser1;
	pmove->iuser2 = sv_player->v.iuser2;
	pmove->iuser3 = sv_player->v.iuser3;
	pmove->iuser4 = sv_player->v.iuser4;
	pmove->fuser1 = sv_player->v.fuser1;
	pmove->fuser2 = sv_player->v.fuser2;
	pmove->fuser3 = sv_player->v.fuser3;
	pmove->fuser4 = sv_player->v.fuser4;

	VectorCopy(sv_player->v.vuser1, pmove->vuser1);
	VectorCopy(sv_player->v.vuser2, pmove->vuser2);
	VectorCopy(sv_player->v.vuser3, pmove->vuser3);
	VectorCopy(sv_player->v.vuser4, pmove->vuser4);

	VectorCopy(sv_player->v.origin, pmove->origin);

	SV_AddLinksToPM(sv_areanodes, pmove->origin);

	pmove->frametime = frametime;
	pmove->runfuncs = TRUE;
	pmove->PM_PlaySound = PM_SV_PlaySound;
	pmove->PM_TraceTexture = PM_SV_TraceTexture;
	pmove->PM_PlaybackEventFull = PM_SV_PlaybackEventFull;

	gEntityInterface.pfnPM_Move(pmove, TRUE);

	sv_player->v.deadflag = pmove->deadflag;
	sv_player->v.effects = pmove->effects;
	sv_player->v.teleport_time = pmove->waterjumptime;
	sv_player->v.waterlevel = pmove->waterlevel;
	sv_player->v.watertype = pmove->watertype;
	sv_player->v.flags = pmove->flags;
	sv_player->v.friction = pmove->friction;
	sv_player->v.movetype = pmove->movetype;
	sv_player->v.maxspeed = pmove->clientmaxspeed;
	sv_player->v.iStepLeft = pmove->iStepLeft;

	VectorCopy(pmove->view_ofs, sv_player->v.view_ofs);
	VectorCopy(pmove->movedir, sv_player->v.movedir);
	VectorCopy(pmove->punchangle, sv_player->v.punchangle);

	if (pmove->onground == -1)
	{
		sv_player->v.flags &= ~FL_ONGROUND;
	}
	else
	{
		sv_player->v.flags |= FL_ONGROUND;
		sv_player->v.groundentity = EDICT_NUM(pmove->physents[pmove->onground].info);
	}

	VectorCopy(pmove->origin, sv_player->v.origin);
	VectorCopy(pmove->velocity, sv_player->v.velocity);
	VectorCopy(pmove->basevelocity, sv_player->v.basevelocity);

	if (!sv_player->v.fixangle)
	{
		VectorCopy(pmove->angles, sv_player->v.v_angle);
		VectorCopy(pmove->angles, sv_player->v.angles);
		sv_player->v.angles[0] = -(pmove->angles[0] / 3.0f);
	}

	sv_player->v.bInDuck = pmove->bInDuck;
	sv_player->v.flDuckTime = (int)pmove->flDuckTime;
	sv_player->v.flTimeStepSound = pmove->flTimeStepSound;
	sv_player->v.flFallVelocity = pmove->flFallVelocity;
	sv_player->v.flSwimTime = (int)pmove->flSwimTime;
	sv_player->v.oldbuttons = pmove->cmd.buttons;

	sv_player->v.iuser1 = pmove->iuser1;
	sv_player->v.iuser2 = pmove->iuser2;
	sv_player->v.iuser3 = pmove->iuser3;
	sv_player->v.iuser4 = pmove->iuser4;
	sv_player->v.fuser1 = pmove->fuser1;
	sv_player->v.fuser2 = pmove->fuser2;
	sv_player->v.fuser3 = pmove->fuser3;
	sv_player->v.fuser4 = pmove->fuser4;


	VectorCopy(pmove->vuser1, sv_player->v.vuser1);
	VectorCopy(pmove->vuser2, sv_player->v.vuser2);
	VectorCopy(pmove->vuser3, sv_player->v.vuser3);
	VectorCopy(pmove->vuser4, sv_player->v.vuser4);

	SetMinMaxSize(sv_player, player_mins[pmove->usehull], player_maxs[pmove->usehull], 0);

	if (host_client->edict->v.solid != SOLID_NOT)
	{
		SV_LinkEdict(sv_player, TRUE);
		vec3_t vel;

		VectorCopy(sv_player->v.velocity, vel);
		for (i = 0; i < pmove->numtouch; ++i)
		{
			pmtrace_t* tr = &pmove->touchindex[i];
			ent = EDICT_NUM(pmove->physents[tr->ent].info);
			SV_ConvertPMTrace(&trace, tr, ent);
			VectorCopy(tr->deltavelocity, sv_player->v.velocity);
			SV_Impact(ent, sv_player, &trace);
		}
		VectorCopy(vel, sv_player->v.velocity);
	}

	gGlobalVariables.time = (float)host_client->svtimebase;
	gGlobalVariables.frametime = frametime;
	gEntityInterface.pfnPlayerPostThink(sv_player);
	gEntityInterface.pfnCmdEnd(sv_player);

	if (host_client->fakeclient == false)
		SV_RestoreMove(host_client);
}

int SV_ValidateClientCommand(char* pszCommand)
{
	char* p;
	int i = 0;

	COM_Parse(pszCommand);
	while ((p = clcommands[i].command) != NULL)
	{
		if (!Q_strcasecmp(com_token, p))
		{
			return 1;
		}
		i++;
	}
	return 0;
}

float SV_CalcClientTime(client_t* cl)
{
	float minping;
	float maxping;
	int backtrack;

	float ping = 0.0;
	int count = 0;
	backtrack = (int)sv_unlagsamples.value;

	if (backtrack < 1)
		backtrack = 1;

	if (backtrack >= (SV_UPDATE_BACKUP <= 16 ? SV_UPDATE_BACKUP : 16))
		backtrack = (SV_UPDATE_BACKUP <= 16 ? SV_UPDATE_BACKUP : 16);

	if (backtrack <= 0)
		return 0.0f;

	for (int i = 0; i < backtrack; i++)
	{
		client_frame_t* frame = &cl->frames[SV_UPDATE_MASK & (cl->netchan.incoming_acknowledged - i)];
		if (frame->ping_time <= 0.0f)
			continue;

		count++;
		ping += frame->ping_time;
	}

	if (!count)
		return 0.0f;

	minping = 9999.0;
	maxping = -9999.0;
	ping /= count;

	for (int i = 0; i < (SV_UPDATE_BACKUP <= 4 ? SV_UPDATE_BACKUP : 4); i++)
	{
		client_frame_t* frame = &cl->frames[SV_UPDATE_MASK & (cl->netchan.incoming_acknowledged - i)];
		if (frame->ping_time <= 0.0f)
			continue;

		if (frame->ping_time < minping)
			minping = frame->ping_time;

		if (frame->ping_time > maxping)
			maxping = frame->ping_time;
	}

	if (maxping < minping || fabs(maxping - minping) <= 0.2)
		return ping;

	return 0.0f;
}

void SV_ComputeLatency(client_t* cl)
{
	cl->latency = SV_CalcClientTime(cl);
}

int SV_UnlagCheckTeleport(vec_t* v1, vec_t* v2)
{
	for (int i = 0; i < 3; i++)
	{
		if (fabs(v1[i] - v2[i]) > 128.0f)
			return 1;
	}

	return 0;
}

void SV_GetTrueOrigin(int player, vec_t* origin)
{
	if (!host_client->lw || !host_client->lc)
		return;

	if (sv_unlag.value == 0 || svs.maxclients <= 1 || !host_client->active)
		return;

	if (player < 0 || player >= svs.maxclients)
		return;

	if (truepositions[player].active && truepositions[player].needrelink)
	{
		VectorCopy(truepositions[player].oldorg, origin);
	}
}

void SV_GetTrueMinMax(int player, float** fmin, float** fmax)
{
	if (!host_client->lw || !host_client->lc)
		return;

	if (sv_unlag.value == 0.0f || svs.maxclients <= 1)
		return;

	if (!host_client->active || player < 0 || player >= svs.maxclients)
		return;

	if (truepositions[player].active && truepositions[player].needrelink)
	{
		*fmin = truepositions[player].oldabsmin;
		*fmax = truepositions[player].oldabsmax;
	}
}

entity_state_t* SV_FindEntInPack(int index, packet_entities_t* pack)
{
	for (int i = 0; i < pack->num_entities; i++)
	{
		if (pack->entities[i].number == index)
			return &pack->entities[i];
	}

	return NULL;
}

void SV_SetupMove(client_t* host_client)
{
	client_t* cl;
	float cl_interptime;
	client_frame_t* pnext;
	entity_state_t* state;
	sv_adjusted_positions_t* pos;
	float frac;
	entity_state_t* pnextstate;
	int i;
	client_frame_t* frame;
	vec3_t origin;
	vec3_t delta;

	float targettime;

	Q_memset(truepositions, 0, sizeof(truepositions));
	
	nofind = true;

	if (!gEntityInterface.pfnAllowLagCompensation())
		return;

	if (sv_unlag.value == 0.0f || !host_client->lw || !host_client->lc)
		return;

	if (svs.maxclients <= 1 || host_client->active == false)
		return;

	nofind = false;

	for (int i = 0; i < svs.maxclients; i++)
	{
		cl = &svs.clients[i];
		
		if (cl == host_client || !cl->active)
			continue;

		truepositions[i].active = 1;

		VectorCopy(cl->edict->v.origin, truepositions[i].oldorg);
		VectorCopy(cl->edict->v.absmin, truepositions[i].oldabsmin);
		VectorCopy(cl->edict->v.absmax, truepositions[i].oldabsmax);
	}

	float correct = host_client->latency;
	if (correct > 1.5)
		correct = 1.5f; // 1.9375

	if (sv_maxunlag.value != 0.0f)
	{
		if (sv_maxunlag.value < 0.0)
			Cvar_SetValue(const_cast<char*>("sv_maxunlag"), 0.0);

		if (correct >= sv_maxunlag.value)
			correct = sv_maxunlag.value;
	}

	cl_interptime = host_client->lastcmd.lerp_msec / 1000.0f;

	if (cl_interptime > 0.1)
		cl_interptime = 0.1f;

	if (host_client->next_messageinterval > cl_interptime)
		cl_interptime = (float)host_client->next_messageinterval;

	double dt = realtime - (double)correct - (double)cl_interptime + (double)sv_unlagpush.value;
	if (dt > realtime)
		dt = realtime;

	targettime = (float)dt;

	if (SV_UPDATE_BACKUP <= 0)
	{
		Q_memset(truepositions, 0, sizeof(truepositions));
		nofind = true;
		return;
	}

	frame = pnext = NULL;
	for (i = 0; i < SV_UPDATE_BACKUP; i++, frame = pnext)
	{
		pnext = &host_client->frames[SV_UPDATE_MASK & (host_client->netchan.outgoing_sequence + ~i)];
		for (int j = 0; j < pnext->entities.num_entities; j++)
		{
			state = &pnext->entities.entities[j];

			if (state->number <= 0 || state->number > svs.maxclients)
				continue;

			pos = &truepositions[state->number - 1];

			if (pos->deadflag)
				continue;

			if (state->health <= 0)
				pos->deadflag = 1;

			if (state->effects & EF_NOINTERP)
				pos->deadflag = 1;

			if (pos->temp_org_setflag)
			{
				if (SV_UnlagCheckTeleport(state->origin, pos->temp_org))
					pos->deadflag = 1;
			}
			else
			{
				pos->temp_org_setflag = 1;
			}

			VectorCopy(state->origin, pos->temp_org);
		}

		if (targettime > pnext->senttime)
			break;
	}

	if (i >= SV_UPDATE_BACKUP || targettime - pnext->senttime > 1.0)
	{
		Q_memset(truepositions, 0, sizeof(truepositions));
		nofind = 1;
		return;
	}

	if (frame)
	{
		float timeDiff = float(frame->senttime - pnext->senttime);
		if (timeDiff == 0.0)
			frac = 0.0;
		else
		{
			frac = float((targettime - pnext->senttime) / timeDiff);

			if (frac > 1.0)
				frac = 1.0; // 1.875

			if (frac < 0.0)
				frac = 0.0;
		}
	}
	else
	{
		frame = pnext;
		frac = 0.0;
	}

	for (i = 0; i < pnext->entities.num_entities; i++)
	{
		state = &pnext->entities.entities[i];

		if (state->number <= 0 || state->number > svs.maxclients)
			continue;

		cl = &svs.clients[state->number - 1];

		if (cl == host_client || !cl->active)
			continue;

		pos = &truepositions[state->number - 1];
		if (pos->deadflag)
			continue;

		if (!pos->active)
		{
			Con_DPrintf(const_cast<char*>("tried to store off position of bogus player %i/%s\n"), i, cl->name);
			continue;
		}

		pnextstate = SV_FindEntInPack(state->number, &frame->entities);

		if (pnextstate)
		{
			VectorSubtract(pnextstate->origin, state->origin, delta);
			VectorMA(state->origin, frac, delta, origin);
		}
		else
		{
			VectorCopy(state->origin, origin);
		}

		VectorCopy(origin, pos->neworg);
		VectorCopy(origin, pos->initial_correction_org);

		if (!VectorCompare(origin, cl->edict->v.origin))
		{
			VectorCopy(origin, cl->edict->v.origin);
			SV_LinkEdict(cl->edict, FALSE);
			pos->needrelink = 1;
		}
	}
}

void SV_RestoreMove(client_t* host_client)
{
	sv_adjusted_positions_t* pos;
	client_t* cli;

	if (nofind == true)
	{
		nofind = false;
		return;
	}

	if (!gEntityInterface.pfnAllowLagCompensation())
		return;

	if (svs.maxclients <= 1 || sv_unlag.value == 0.0)
		return;

	if (!host_client->lw || !host_client->lc || !host_client->active)
		return;

	for (int i = 0; i < svs.maxclients; i++)
	{
		cli = &svs.clients[i];
		pos = &truepositions[i];

		if (cli == host_client || !cli->active)
			continue;

		if (VectorCompare(pos->neworg, pos->oldorg) || !pos->needrelink)
			continue;

		if (!pos->active)
		{
			Con_DPrintf(const_cast<char*>("SV_RestoreMove:  Tried to restore 'inactive' player %i/%s\n"), i, &cli->name[4]);
			continue;
		}

		if (VectorCompare(pos->initial_correction_org, cli->edict->v.origin))
		{
			VectorCopy(pos->oldorg, cli->edict->v.origin);
			SV_LinkEdict(cli->edict, FALSE);
		}
	}
}

void SV_ParseStringCommand(client_t* pSenderClient)
{
	char* s = MSG_ReadString();
	int ret = SV_ValidateClientCommand(s);
	switch (ret)
	{
	case 0:
		if (Q_strlen(s) > 127)
		{
			s[127] = 0;
		}
		Cmd_TokenizeString(s);
		gEntityInterface.pfnClientCommand(sv_player);
		break;
	case 1:
		Cmd_ExecuteString(s, src_client);
		break;
	case 2:	// TODO: Check not used path
		Cbuf_InsertText(s);
		break;
	}
}

void SV_ParseDelta(client_t* pSenderClient)
{
	host_client->delta_sequence = MSG_ReadByte();
}

void SV_EstablishTimeBase(client_t* cl, usercmd_t* cmds, int dropped, int numbackup, int numcmds)
{
	int cmdnum;
	double runcmd_time;
	double time_at_end = sv.time + host_frametime;

	runcmd_time = 0.0;
	cmdnum = dropped;
	if (dropped < 24)
	{
		for (; cmdnum > numbackup; cmdnum--)
			runcmd_time += (double)cl->lastcmd.msec / 1000.0;

		/*if (cmdnum > numbackup)
		{
			do
			{
				cmdnum--;
				runcmd_time += (double)cl->lastcmd.msec / 1000.0;
			}
			while (cmdnum > numbackup);
		}*/

		for (; cmdnum > 0; cmdnum--)
			runcmd_time += (double)cmds[cmdnum - 1 + numcmds].msec / 1000.0;
	}

	for (; numcmds > 0; numcmds--)
		runcmd_time += (double)cmds[numcmds - 1].msec / 1000.0;

	cl->svtimebase = time_at_end - runcmd_time;
}

void SV_ParseMove(client_t *pSenderClient)
{
	client_frame_t *frame;
	int placeholder;
	int mlen;
	unsigned int packetLossByte;
	int numcmds;
	int totalcmds;
	byte cbpktchecksum;
	usercmd_t *cmd;
//	usercmd_t cmds[64]{};
	usercmd_t cmds[64];	// @SH_CODE: no zero-filling
	usercmd_t cmdNull;
	float packet_loss;
	byte cbchecksum;
	int numbackup;

	if (g_balreadymoved)
	{
		msg_badread = true;
		return;
	}
	g_balreadymoved = true;

	frame = &host_client->frames[SV_UPDATE_MASK & host_client->netchan.incoming_acknowledged];
	Q_memset(&cmdNull, 0, sizeof(cmdNull));

	placeholder = msg_readcount + 1;
	mlen = MSG_ReadByte();
	cbchecksum = MSG_ReadByte();

	if (mlen <= 0 || msg_readcount + mlen + 2 > net_message.maxsize)
		Sys_Error("SV_ParseMove: invalid length: %d", mlen);

	COM_UnMunge(&net_message.data[placeholder + 1], mlen, host_client->netchan.incoming_sequence);

	packetLossByte = MSG_ReadByte();
	
	packet_loss = (float)(packetLossByte & 0x7F);
	pSenderClient->m_bLoopback = (packetLossByte >> 7) & 1;

	numbackup = MSG_ReadByte();
	numcmds = MSG_ReadByte();

	totalcmds = numcmds + numbackup;
	net_drop += 1 - numcmds;

	if (totalcmds < 0 || totalcmds >= MULTIPLAYER_BACKUP - 1)
	{
		Con_Printf(const_cast<char*>("SV_ReadClientMessage: too many cmds %i sent for %s/%s\n"), totalcmds, host_client->name, NET_AdrToString(host_client->netchan.remote_address));
		SV_DropClient(host_client, FALSE, const_cast<char*>("CMD_MAXBACKUP hit"));
		msg_badread = true;
		return;
	}

	usercmd_t* from = &cmdNull;
	for (int i = totalcmds - 1; i >= 0; i--)
	{
		MSG_ReadUsercmd(&cmds[i], from);
		from = &cmds[i];
	}

	if (!sv.active || !(host_client->active || host_client->spawned))
		return;

	if (msg_badread)
	{
		Con_Printf(const_cast<char*>("Client %s:%s sent a bogus command packet\n"), host_client->name, NET_AdrToString(host_client->netchan.remote_address));
		return;
	}

	cbpktchecksum = COM_BlockSequenceCRCByte(&net_message.data[placeholder + 1], msg_readcount - placeholder - 1, host_client->netchan.incoming_sequence);
	if (cbpktchecksum != cbchecksum)
	{
		Con_DPrintf(const_cast<char*>("Failed command checksum for %s:%s\n"), host_client->name, NET_AdrToString(host_client->netchan.remote_address));
		msg_badread = true;
		return;
	}

	host_client->packet_loss = packet_loss;

	if ((sv.paused || svs.maxclients <= 1 && key_dest) || (sv_player->v.flags & FL_FROZEN) != 0)
	{
		for (int i = 0; i < numcmds; i++)
		{
			cmd = &cmds[i];
			cmd->msec = 0;
			cmd->forwardmove = 0;
			cmd->sidemove = 0;
			cmd->upmove = 0;
			cmd->buttons = 0;

			if (sv_player->v.flags & FL_FROZEN)
				cmd->impulse = 0;

			VectorCopy(sv_player->v.v_angle, cmd->viewangles);
		}

		net_drop = 0;
	}
	else if ((sv_player->v.flags & FL_FROZEN) == 0)
	{
		VectorCopy(cmds[0].viewangles, sv_player->v.v_angle);
	}

	// dup and more correct in SV_RunCmd
	sv_player->v.button = cmds[0].buttons;
	sv_player->v.light_level = cmds[0].lightlevel;
	SV_EstablishTimeBase(host_client, cmds, net_drop, numbackup, numcmds);

	if (net_drop < 24)
	{
		for (; net_drop > numbackup; net_drop--)
			SV_RunCmd(&host_client->lastcmd, 0);

		for (; net_drop > 0; net_drop--)
		{
			int cmdnum = numcmds + net_drop - 1;
			SV_RunCmd(&cmds[cmdnum], host_client->netchan.incoming_sequence - cmdnum);
		}
	}

	for (int i = numcmds - 1; i >= 0; i--)
		SV_RunCmd(&cmds[i], host_client->netchan.incoming_sequence - i);

	host_client->lastcmd = cmds[0];

	frame->ping_time -= (float)(host_client->lastcmd.msec * 0.5 / 1000.0);
	if (frame->ping_time < 0.0)
		frame->ping_time = 0;

	if (sv_player->v.animtime > (float)(host_frametime + sv.time))
		sv_player->v.animtime = (float)(host_frametime + sv.time);
}

void SV_ParseVoiceData(client_t* cl)
{
	char chReceived[4096];
	int iClient = cl - svs.clients;
	unsigned int nDataLength = MSG_ReadShort();
	if (nDataLength > sizeof(chReceived))
	{
		Con_DPrintf(const_cast<char*>("SV_ParseVoiceData: invalid incoming packet.\n"));
		SV_DropClient(cl, FALSE, const_cast<char*>("Invalid voice data\n"));
		return;
	}

	MSG_ReadBuf(nDataLength, chReceived);
	cl->m_lastvoicetime = sv.time;

	if (sv_voiceenable.value == 0.0f)
		return;

	for (int i = 0; i < MAX_CLIENTS/*svs.maxclients*/; i++)
	{
		client_t* pDestClient = &svs.clients[i];
		if (!((1 << (i & (MAX_CLIENTS - 1))) & cl->m_VoiceStreams[i >> 5]) && i != iClient)
			continue;

		if (!pDestClient->active && !pDestClient->connected && i != iClient)
			continue;

		int nSendLength = nDataLength;
		if (i == iClient && !pDestClient->m_bLoopback)
			nSendLength = 0;

		if (pDestClient->datagram.cursize + nSendLength + 6 < pDestClient->datagram.maxsize)
		{
			MSG_WriteByte(&pDestClient->datagram, svc_voicedata);
			MSG_WriteByte(&pDestClient->datagram, iClient);
			MSG_WriteShort(&pDestClient->datagram, nSendLength);
			MSG_WriteBuf(&pDestClient->datagram, nSendLength, chReceived);
		}
	}
}

void SV_IgnoreHLTV(client_t* cl)
{
}

void SV_ParseCvarValue(client_t* cl)
{
	char* value;
	value = MSG_ReadString();
	if (gNewDLLFunctions.pfnCvarValue)
		gNewDLLFunctions.pfnCvarValue(cl->edict, value);

	Con_DPrintf(const_cast<char*>("Cvar query response: name:%s, value:%s\n"), cl->name, value);
}

void SV_ParseCvarValue2(client_t* cl)
{
	int requestID = MSG_ReadLong();

	char cvarName[255];
	Q_strncpy(cvarName, MSG_ReadString(), sizeof(cvarName));
	cvarName[sizeof(cvarName) - 1] = 0;

	char* value = MSG_ReadString();

	if (gNewDLLFunctions.pfnCvarValue2)
		gNewDLLFunctions.pfnCvarValue2(cl->edict, requestID, cvarName, value);

	Con_DPrintf(const_cast<char*>("Cvar query response: name:%s, request ID %d, cvar:%s, value:%s\n"), cl->name, requestID, cvarName, value);
}

clc_func_t sv_clcfuncs[] = {
	{ clc_bad, const_cast<char*>("clc_bad"), nullptr },
	{ clc_nop, const_cast<char*>("clc_nop"), nullptr },
	{ clc_move, const_cast<char*>("clc_move"), &SV_ParseMove },
	{ clc_stringcmd, const_cast<char*>("clc_stringcmd"), &SV_ParseStringCommand },
	{ clc_delta, const_cast<char*>("clc_delta"), &SV_ParseDelta },
	{ clc_resourcelist, const_cast<char*>("clc_resourcelist"), &SV_ParseResourceList },
	{ clc_tmove, const_cast<char*>("clc_tmove"), nullptr },
	{ clc_fileconsistency, const_cast<char*>("clc_fileconsistency"), &SV_ParseConsistencyResponse },
	{ clc_voicedata, const_cast<char*>("clc_voicedata"), &SV_ParseVoiceData },
	{ clc_hltv, const_cast<char*>("clc_hltv"), &SV_IgnoreHLTV },
	{ clc_cvarvalue, const_cast<char*>("clc_cvarvalue"), &SV_ParseCvarValue },
	{ clc_cvarvalue2, const_cast<char*>("clc_cvarvalue2"), &SV_ParseCvarValue2 },
	{ clc_endoflist, const_cast<char*>("End of List"), nullptr },
};

void SV_ExecuteClientMessage(client_t* cl)
{
	g_balreadymoved = 0;
	client_frame_t* frame = &cl->frames[SV_UPDATE_MASK & cl->netchan.incoming_acknowledged];
	frame->ping_time = realtime - frame->senttime - cl->next_messageinterval;
	if (frame->senttime == 0.0)
		frame->ping_time = 0;

	if (realtime - cl->connection_started < 2.0 && frame->ping_time > 0.0)
		frame->ping_time = 0;

	SV_ComputeLatency(cl);
	host_client = cl;
	sv_player = cl->edict;
	cl->delta_sequence = -1;
	pmove = &g_svmove;

	while (true)
	{
		if (msg_badread)
		{
			Con_Printf(const_cast<char*>("SV_ReadClientMessage: badread\n"));
			return;
		}

		int c = MSG_ReadByte();
		if (c == -1)
			return;

		if (c > clc_cvarvalue2)
		{
			Con_Printf(const_cast<char*>("SV_ReadClientMessage: unknown command char (%d)\n"), c);
			SV_DropClient(cl, false, const_cast<char*>("Bad command character in client command"));
			return;
		}

		if (sv_clcfuncs[c].pfnParse)
			sv_clcfuncs[c].pfnParse(cl);
	}
}

qboolean SV_SetPlayer(int idnum)
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		client_t* client = svs.clients + i;

		if (client->spawned == true && client->active == true && client->connected == true && client->userid == idnum)
		{
			host_client = client;
			sv_player = client->edict;
			return true;
		}
	}

	Con_Printf(const_cast<char*>("Userid %i is not on the server\n"), idnum);
	return false;
}

void SV_ShowServerinfo_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
	}
	else
	{
		Info_Print(Info_Serverinfo());
	}
}

void SV_SendEnts_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
	}
	else
	{
		if (host_client->active && host_client->spawned)
		{
			if (host_client->connected)
			{
				host_client->fully_connected = TRUE;
			}
		}
	}
}

void SV_FullUpdate_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}

	if (SV_FullUpdate_Internal() > false)
		gEntityInterface.pfnClientCommand(sv_player);
}

qboolean SV_FullUpdate_Internal()
{
	int entIndex;
	float ltime;

	if (host_client->active)
	{
		entIndex = IndexOfEdict(host_client->edict);

		if (s_LastFullUpdate[entIndex] > (float)sv.time)
			s_LastFullUpdate[entIndex] = 0;

		ltime = (float)sv.time - s_LastFullUpdate[entIndex];
		if (ltime <= 0.0f)
			ltime = 0.0f;

		if (ltime < 0.45f && sv.time > 0.45)
		{
			Con_DPrintf(
				const_cast<char*>("%s is spamming fullupdate: (%f) (%f) (%f)\n"),
				host_client->name,
				(float)sv.time,
				s_LastFullUpdate[entIndex],
				ltime);
			return false;
		}
		s_LastFullUpdate[entIndex] = (float)sv.time;

		SV_ForceFullClientsUpdate();

		return true;
	}

	Con_DPrintf(const_cast<char*>("%s is trying to fullupdate when not active at (%f)\n"), host_client->name, (float)sv.time);

	return false;
}
