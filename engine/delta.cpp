#include "quakedef.h"
#include "delta.h"
#include "server.h"
#include "common.h"
#include "client.h"

delta_definition_list_t* g_defs;
delta_encoder_t* g_encoders;
delta_registry_t* g_deltaregistry;

#define DELTA_D_DEF(member) const_cast<char*>(#member), offsetof(delta_description_s, member)
#define DELTA_DEF(structname, member) { const_cast<char*>(#member), offsetof(structname, member) }

static delta_definition_t g_DeltaDataDefinition[] =
{
	DELTA_DEF(delta_description_s, fieldType),
	DELTA_DEF(delta_description_s, fieldName),
	DELTA_DEF(delta_description_s, fieldOffset),
	DELTA_DEF(delta_description_s, fieldSize),
	DELTA_DEF(delta_description_s, significant_bits),
	DELTA_DEF(delta_description_s, premultiply),
	DELTA_DEF(delta_description_s, postmultiply),
	DELTA_DEF(delta_description_s, flags),
};

static delta_description_t g_MetaDescription[] =
{
	{ DT_INTEGER, DELTA_D_DEF(fieldType), 1, 32, 1.0, 1.0, 0, 0, 0 },
	{ DT_STRING, DELTA_D_DEF(fieldName), 1, 1, 1.0, 1.0, 0, 0, 0 },
	{ DT_INTEGER, DELTA_D_DEF(fieldOffset), 1, 16, 1.0, 1.0, 0, 0, 0 },
	{ DT_INTEGER, DELTA_D_DEF(fieldSize), 1, 8, 1.0, 1.0, 0, 0, 0 },
	{ DT_INTEGER, DELTA_D_DEF(significant_bits), 1, 8, 1.0, 1.0, 0, 0, 0 },
	{ DT_FLOAT, DELTA_D_DEF(premultiply), 1, 32, 4000.0, 1.0, 0, 0, 0 },
	{ DT_FLOAT, DELTA_D_DEF(postmultiply), 1, 32, 4000.0, 1.0, 0, 0, 0 },
};

delta_t g_MetaDelta[] =
{
	{ 0, ARRAYSIZE(g_MetaDescription), "", NULL, g_MetaDescription },
};

static delta_definition_t g_EventDataDefinition[] =
{
	DELTA_DEF(event_args_s, entindex),
	DELTA_DEF(event_args_s, origin[0]),
	DELTA_DEF(event_args_s, origin[1]),
	DELTA_DEF(event_args_s, origin[2]),
	DELTA_DEF(event_args_s, angles[0]),
	DELTA_DEF(event_args_s, angles[1]),
	DELTA_DEF(event_args_s, angles[2]),
	DELTA_DEF(event_args_s, fparam1),
	DELTA_DEF(event_args_s, fparam2),
	DELTA_DEF(event_args_s, iparam1),
	DELTA_DEF(event_args_s, iparam2),
	DELTA_DEF(event_args_s, bparam1),
	DELTA_DEF(event_args_s, bparam2),
	DELTA_DEF(event_args_s, ducking),
};

static delta_definition_t g_EntityDataDefinition[] =
{
	DELTA_DEF(entity_state_s, startpos[0]),
	DELTA_DEF(entity_state_s, startpos[1]),
	DELTA_DEF(entity_state_s, startpos[2]),
	DELTA_DEF(entity_state_s, endpos[0]),
	DELTA_DEF(entity_state_s, endpos[1]),
	DELTA_DEF(entity_state_s, endpos[2]),
	DELTA_DEF(entity_state_s, impacttime),
	DELTA_DEF(entity_state_s, starttime),
	DELTA_DEF(entity_state_s, origin[0]),
	DELTA_DEF(entity_state_s, origin[1]),
	DELTA_DEF(entity_state_s, origin[2]),
	DELTA_DEF(entity_state_s, angles[0]),
	DELTA_DEF(entity_state_s, angles[1]),
	DELTA_DEF(entity_state_s, angles[2]),
	DELTA_DEF(entity_state_s, modelindex),
	DELTA_DEF(entity_state_s, frame),
	DELTA_DEF(entity_state_s, movetype),
	DELTA_DEF(entity_state_s, colormap),
	DELTA_DEF(entity_state_s, skin),
	DELTA_DEF(entity_state_s, solid),
	DELTA_DEF(entity_state_s, scale),
	DELTA_DEF(entity_state_s, effects),
	DELTA_DEF(entity_state_s, sequence),
	DELTA_DEF(entity_state_s, animtime),
	DELTA_DEF(entity_state_s, framerate),
	DELTA_DEF(entity_state_s, controller[0]),
	DELTA_DEF(entity_state_s, controller[1]),
	DELTA_DEF(entity_state_s, controller[2]),
	DELTA_DEF(entity_state_s, controller[3]),
	DELTA_DEF(entity_state_s, blending[0]),
	DELTA_DEF(entity_state_s, blending[1]),
	DELTA_DEF(entity_state_s, body),
	DELTA_DEF(entity_state_s, owner),
	DELTA_DEF(entity_state_s, rendermode),
	DELTA_DEF(entity_state_s, renderamt),
	DELTA_DEF(entity_state_s, renderfx),
	DELTA_DEF(entity_state_s, rendercolor.r),
	DELTA_DEF(entity_state_s, rendercolor.g),
	DELTA_DEF(entity_state_s, rendercolor.b),
	DELTA_DEF(entity_state_s, weaponmodel),
	DELTA_DEF(entity_state_s, gaitsequence),
	DELTA_DEF(entity_state_s, mins[0]),
	DELTA_DEF(entity_state_s, mins[1]),
	DELTA_DEF(entity_state_s, mins[2]),
	DELTA_DEF(entity_state_s, maxs[0]),
	DELTA_DEF(entity_state_s, maxs[1]),
	DELTA_DEF(entity_state_s, maxs[2]),
	DELTA_DEF(entity_state_s, aiment),
	DELTA_DEF(entity_state_s, basevelocity[0]),
	DELTA_DEF(entity_state_s, basevelocity[1]),
	DELTA_DEF(entity_state_s, basevelocity[2]),
	DELTA_DEF(entity_state_s, friction),
	DELTA_DEF(entity_state_s, gravity),
	DELTA_DEF(entity_state_s, spectator),
	DELTA_DEF(entity_state_s, velocity[0]),
	DELTA_DEF(entity_state_s, velocity[1]),
	DELTA_DEF(entity_state_s, velocity[2]),
	DELTA_DEF(entity_state_s, team),
	DELTA_DEF(entity_state_s, playerclass),
	DELTA_DEF(entity_state_s, health),
	DELTA_DEF(entity_state_s, usehull),
	DELTA_DEF(entity_state_s, oldbuttons),
	DELTA_DEF(entity_state_s, onground),
	DELTA_DEF(entity_state_s, iStepLeft),
	DELTA_DEF(entity_state_s, flFallVelocity),
	DELTA_DEF(entity_state_s, weaponanim),
	DELTA_DEF(entity_state_s, eflags),
	DELTA_DEF(entity_state_s, iuser1),
	DELTA_DEF(entity_state_s, iuser2),
	DELTA_DEF(entity_state_s, iuser3),
	DELTA_DEF(entity_state_s, iuser4),
	DELTA_DEF(entity_state_s, fuser1),
	DELTA_DEF(entity_state_s, fuser2),
	DELTA_DEF(entity_state_s, fuser3),
	DELTA_DEF(entity_state_s, fuser4),
	DELTA_DEF(entity_state_s, vuser1[0]),
	DELTA_DEF(entity_state_s, vuser1[1]),
	DELTA_DEF(entity_state_s, vuser1[2]),
	DELTA_DEF(entity_state_s, vuser2[0]),
	DELTA_DEF(entity_state_s, vuser2[1]),
	DELTA_DEF(entity_state_s, vuser2[2]),
	DELTA_DEF(entity_state_s, vuser3[0]),
	DELTA_DEF(entity_state_s, vuser3[1]),
	DELTA_DEF(entity_state_s, vuser3[2]),
	DELTA_DEF(entity_state_s, vuser4[0]),
	DELTA_DEF(entity_state_s, vuser4[1]),
	DELTA_DEF(entity_state_s, vuser4[2]),
};

static delta_definition_t g_UsercmdDataDefinition[] =
{
	DELTA_DEF(usercmd_s, lerp_msec),
	DELTA_DEF(usercmd_s, msec),
	DELTA_DEF(usercmd_s, lightlevel),
	DELTA_DEF(usercmd_s, viewangles[0]),
	DELTA_DEF(usercmd_s, viewangles[1]),
	DELTA_DEF(usercmd_s, viewangles[2]),
	DELTA_DEF(usercmd_s, buttons),
	DELTA_DEF(usercmd_s, forwardmove),
	DELTA_DEF(usercmd_s, sidemove),
	DELTA_DEF(usercmd_s, upmove),
	DELTA_DEF(usercmd_s, impulse),
	DELTA_DEF(usercmd_s, weaponselect),
	DELTA_DEF(usercmd_s, impact_index),
	DELTA_DEF(usercmd_s, impact_position[0]),
	DELTA_DEF(usercmd_s, impact_position[1]),
	DELTA_DEF(usercmd_s, impact_position[2]),
};

static delta_definition_t g_WeaponDataDefinition[] =
{
	DELTA_DEF(weapon_data_s, m_iId),
	DELTA_DEF(weapon_data_s, m_iClip),
	DELTA_DEF(weapon_data_s, m_flNextPrimaryAttack),
	DELTA_DEF(weapon_data_s, m_flNextSecondaryAttack),
	DELTA_DEF(weapon_data_s, m_flTimeWeaponIdle),
	DELTA_DEF(weapon_data_s, m_fInReload),
	DELTA_DEF(weapon_data_s, m_fInSpecialReload),
	DELTA_DEF(weapon_data_s, m_flNextReload),
	DELTA_DEF(weapon_data_s, m_flPumpTime),
	DELTA_DEF(weapon_data_s, m_fReloadTime),
	DELTA_DEF(weapon_data_s, m_fAimedDamage),
	DELTA_DEF(weapon_data_s, m_fNextAimBonus),
	DELTA_DEF(weapon_data_s, m_fInZoom),
	DELTA_DEF(weapon_data_s, m_iWeaponState),
	DELTA_DEF(weapon_data_s, iuser1),
	DELTA_DEF(weapon_data_s, iuser2),
	DELTA_DEF(weapon_data_s, iuser3),
	DELTA_DEF(weapon_data_s, iuser4),
	DELTA_DEF(weapon_data_s, fuser1),
	DELTA_DEF(weapon_data_s, fuser2),
	DELTA_DEF(weapon_data_s, fuser3),
	DELTA_DEF(weapon_data_s, fuser4),
};

static delta_definition_t g_ClientDataDefinition[] =
{
	DELTA_DEF(clientdata_s, origin[0]),
	DELTA_DEF(clientdata_s, origin[1]),
	DELTA_DEF(clientdata_s, origin[2]),
	DELTA_DEF(clientdata_s, velocity[0]),
	DELTA_DEF(clientdata_s, velocity[1]),
	DELTA_DEF(clientdata_s, velocity[2]),
	DELTA_DEF(clientdata_s, viewmodel),
	DELTA_DEF(clientdata_s, punchangle[0]),
	DELTA_DEF(clientdata_s, punchangle[1]),
	DELTA_DEF(clientdata_s, punchangle[2]),
	DELTA_DEF(clientdata_s, flags),
	DELTA_DEF(clientdata_s, waterlevel),
	DELTA_DEF(clientdata_s, watertype),
	DELTA_DEF(clientdata_s, view_ofs[0]),
	DELTA_DEF(clientdata_s, view_ofs[1]),
	DELTA_DEF(clientdata_s, view_ofs[2]),
	DELTA_DEF(clientdata_s, health),
	DELTA_DEF(clientdata_s, bInDuck),
	DELTA_DEF(clientdata_s, weapons),
	DELTA_DEF(clientdata_s, flTimeStepSound),
	DELTA_DEF(clientdata_s, flDuckTime),
	DELTA_DEF(clientdata_s, flSwimTime),
	DELTA_DEF(clientdata_s, waterjumptime),
	DELTA_DEF(clientdata_s, maxspeed),
	DELTA_DEF(clientdata_s, m_iId),
	DELTA_DEF(clientdata_s, ammo_nails),
	DELTA_DEF(clientdata_s, ammo_shells),
	DELTA_DEF(clientdata_s, ammo_cells),
	DELTA_DEF(clientdata_s, ammo_rockets),
	DELTA_DEF(clientdata_s, m_flNextAttack),
	DELTA_DEF(clientdata_s, physinfo),
	DELTA_DEF(clientdata_s, fov),
	DELTA_DEF(clientdata_s, weaponanim),
	DELTA_DEF(clientdata_s, tfstate),
	DELTA_DEF(clientdata_s, pushmsec),
	DELTA_DEF(clientdata_s, deadflag),
	DELTA_DEF(clientdata_s, iuser1),
	DELTA_DEF(clientdata_s, iuser2),
	DELTA_DEF(clientdata_s, iuser3),
	DELTA_DEF(clientdata_s, iuser4),
	DELTA_DEF(clientdata_s, fuser1),
	DELTA_DEF(clientdata_s, fuser2),
	DELTA_DEF(clientdata_s, fuser3),
	DELTA_DEF(clientdata_s, fuser4),
	DELTA_DEF(clientdata_s, vuser1[0]),
	DELTA_DEF(clientdata_s, vuser1[1]),
	DELTA_DEF(clientdata_s, vuser1[2]),
	DELTA_DEF(clientdata_s, vuser2[0]),
	DELTA_DEF(clientdata_s, vuser2[1]),
	DELTA_DEF(clientdata_s, vuser2[2]),
	DELTA_DEF(clientdata_s, vuser3[0]),
	DELTA_DEF(clientdata_s, vuser3[1]),
	DELTA_DEF(clientdata_s, vuser3[2]),
	DELTA_DEF(clientdata_s, vuser4[0]),
	DELTA_DEF(clientdata_s, vuser4[1]),
	DELTA_DEF(clientdata_s, vuser4[2]),
};

delta_description_t *DELTA_FindField(delta_t *pFields, const char *pszField)
{
	int fieldCount = pFields->fieldCount;
	delta_description_t *pitem = pFields->pdd;

	for (int i = 0; i < fieldCount; i++, pitem++)
	{
		if (!Q_strcasecmp(pitem->fieldName, pszField))
			return pitem;
	}

	Con_Printf(const_cast<char*>(__FUNCTION__ ":  Warning, couldn't find %s\n"), pszField);
	return NULL;
}

int DELTA_FindFieldIndex(struct delta_s *pFields, const char *fieldname)
{
	int fieldCount = pFields->fieldCount;
	delta_description_t *pitem = pFields->pdd;

	for (int i = 0; i < fieldCount; i++, pitem++)
	{
		if (!Q_strcasecmp(pitem->fieldName, fieldname))
			return i;
	}

	Con_Printf(const_cast<char*>(__FUNCTION__ ":  Warning, couldn't find %s\n"), fieldname);
	return -1;
}

void DELTA_SetField(delta_s *pFields, const char *fieldname)
{
	delta_description_t *pTest = DELTA_FindField(pFields, fieldname);

	if (pTest == NULL)
		return;

	pTest->flags |= FDT_MARK;
}

void DELTA_UnsetField(delta_s *pFields, const char *fieldname)
{
	delta_description_t *pTest = DELTA_FindField(pFields, fieldname);

	if (pTest == NULL)
		return;

	pTest->flags &= ~FDT_MARK;
}

void DELTA_SetFieldByIndex(struct delta_s *pFields, int fieldNumber)
{
	pFields->pdd[fieldNumber].flags |= FDT_MARK;
}

void DELTA_UnsetFieldByIndex(struct delta_s *pFields, int fieldNumber)
{
	pFields->pdd[fieldNumber].flags &= ~FDT_MARK;
}

void DELTA_ClearFlags(delta_t *pFields)
{
	int i;
	delta_description_t *pitem;
	for (i = 0, pitem = pFields->pdd; i < pFields->fieldCount; i++, pitem++)
	{
		pitem->flags = 0;
	}
}

int DELTA_TestDelta(unsigned char *from, unsigned char *to, delta_t *pFields)
{
	int i;
	char *st1, *st2;
	delta_description_t *pTest;
	int fieldType;
	int fieldCount = pFields->fieldCount;
	int length = 0;
	int different;
	int neededBits = 0;
	int highestBit = -1;

	for (i = 0, pTest = pFields->pdd; i < fieldCount; i++, pTest++)
	{
		different = FALSE;

		fieldType = pTest->fieldType & ~DT_SIGNED;
		switch (fieldType)
		{
		case DT_BYTE:
			different = from[pTest->fieldOffset] != to[pTest->fieldOffset];
			break;
		case DT_SHORT:
			different = *(uint16 *)&from[pTest->fieldOffset] != *(uint16 *)&to[pTest->fieldOffset];
			break;
		case DT_FLOAT:
		case DT_INTEGER:
		case DT_ANGLE:
			different = *(uint32 *)&from[pTest->fieldOffset] != *(uint32 *)&to[pTest->fieldOffset];
			break;
		case DT_TIMEWINDOW_8:
			different = (int32)(*(float *)&from[pTest->fieldOffset] * 100.0) != (int32)(*(float *)&to[pTest->fieldOffset] * 100.0);
			break;
		case DT_TIMEWINDOW_BIG:
			different = (int32)(*(float *)&from[pTest->fieldOffset] * 1000.0) != (int32)(*(float *)&to[pTest->fieldOffset] * 1000.0);
			break;
		case DT_STRING:
			st1 = (char*)&from[pTest->fieldOffset];
			st2 = (char*)&to[pTest->fieldOffset];
			if (!(!*st1 && !*st2 || *st1 && *st2 && !Q_strcasecmp(st1, st2)))	// Not sure why it is case insensitive, but it looks so
			{
				pTest->flags |= FDT_MARK;

				different = TRUE;
				length = Q_strlen(st2) * 8;
			}
			break;
		default:
			Con_Printf(const_cast<char*>("Bad field type %i\n"), fieldType);
			break;
		}

		if (different)
		{
			highestBit = i;
			neededBits += fieldType == DT_STRING ? length + 8 : pTest->significant_bits;
		}
	}

	if (highestBit != -1)
	{
		neededBits += highestBit / 8 * 8 + 8;
	}

	return neededBits;
}

int DELTA_CountSendFields(delta_t *pFields)
{
	int i, c;
	int fieldCount = pFields->fieldCount;
	delta_description_t *pitem;
	for (i = 0, c = 0, pitem = pFields->pdd; i < fieldCount; i++, pitem++)
	{
		if (pitem->flags & FDT_MARK)
		{
			c++;
			pitem->stats.sendcount++;
		}
	}
	return c;
}

void DELTA_MarkSendFields(unsigned char *from, unsigned char *to, delta_t *pFields)
{
	int i;
	char *st1, *st2;
	delta_description_t *pTest;
	int fieldType;
	int fieldCount = pFields->fieldCount;

	for (i = 0, pTest = pFields->pdd; i < fieldCount; i++, pTest++)
	{
		fieldType = pTest->fieldType & ~DT_SIGNED;
		switch (fieldType)
		{
		case DT_BYTE:
			if (from[pTest->fieldOffset] != to[pTest->fieldOffset])
				pTest->flags |= FDT_MARK;
			break;
		case DT_SHORT:
			if (*(uint16 *)&from[pTest->fieldOffset] != *(uint16 *)&to[pTest->fieldOffset])
				pTest->flags |= FDT_MARK;
			break;
		case DT_FLOAT:
		case DT_INTEGER:
		case DT_ANGLE:
			if (*(uint32 *)&from[pTest->fieldOffset] != *(uint32 *)&to[pTest->fieldOffset])
				pTest->flags |= FDT_MARK;
			break;
		case DT_TIMEWINDOW_8:
			if ((int32)(*(float *)&from[pTest->fieldOffset] * 100.0) != (int32)(*(float *)&to[pTest->fieldOffset] * 100.0))
				pTest->flags |= FDT_MARK;
			break;
		case DT_TIMEWINDOW_BIG:
			if ((int32)(*(float *)&from[pTest->fieldOffset] * 1000.0) != (int32)(*(float *)&to[pTest->fieldOffset] * 1000.0))
				pTest->flags |= FDT_MARK;
			break;
		case DT_STRING:
			st1 = (char*)&from[pTest->fieldOffset];
			st2 = (char*)&to[pTest->fieldOffset];
			if (!(!*st1 && !*st2 || *st1 && *st2 && !Q_strcasecmp(st1, st2)))	// Not sure why it is case insensitive, but it looks so
				pTest->flags |= FDT_MARK;
			break;
		default:
			Con_Printf(const_cast<char*>("Bad field type %i\n"), fieldType);
			break;
		}
	}
	if (pFields->conditionalencode)
		pFields->conditionalencode(pFields, from, to);
}

void DELTA_SetSendFlagBits(delta_t *pFields, int *bits, int *bytecount)
{
	delta_description_t *pTest;
	int i;
	int lastbit = -1;
	int fieldCount = pFields->fieldCount;

	Q_memset(bits, 0, 8);

	for (i = fieldCount - 1, pTest = &pFields->pdd[i]; i >= 0; i--, pTest--)
	{
		if (pTest->flags & FDT_MARK)
		{
			if (lastbit == -1)
				lastbit = i;
			bits[i > 31 ? 1 : 0] |= 1 << (i & 0x1F);
		}
	}

	*bytecount = (lastbit >> 3) + 1;
}

void DELTA_WriteMarkedFields(unsigned char *from, unsigned char *to, delta_t *pFields)
{
	int i;
	delta_description_t *pTest;
	int fieldSign;
	int fieldType;

	float f2;
	int fieldCount = pFields->fieldCount;

	for (i = 0, pTest = pFields->pdd; i < fieldCount; i++, pTest++)
	{
		if (!(pTest->flags & FDT_MARK))
			continue;

		fieldSign = pTest->fieldType & DT_SIGNED;
		fieldType = pTest->fieldType & ~DT_SIGNED;
		switch (fieldType)
		{
		case DT_BYTE:
			if (fieldSign)
			{
				int8 si8 = *(int8 *)&to[pTest->fieldOffset];
				si8 = (int8)((double)si8 * pTest->premultiply);
				MSG_WriteSBits(si8, pTest->significant_bits);
			}
			else
			{
				uint8 i8 = *(uint8 *)&to[pTest->fieldOffset];
				i8 = (uint8)((double)i8 * pTest->premultiply);
				MSG_WriteBits(i8, pTest->significant_bits);
			}
			break;
		case DT_SHORT:
			if (fieldSign)
			{
				int16 si16 = *(int16 *)&to[pTest->fieldOffset];
				si16 = (int16)((double)si16 * pTest->premultiply);
				MSG_WriteSBits(si16, pTest->significant_bits);
			}
			else
			{
				uint16 i16 = *(uint16 *)&to[pTest->fieldOffset];
				i16 = (uint16)((double)i16 * pTest->premultiply);
				MSG_WriteBits(i16, pTest->significant_bits);
			}
			break;
		case DT_FLOAT:
		{
			 double val = (double)(*(float *)&to[pTest->fieldOffset]) * pTest->premultiply;
			 if (fieldSign)
			 {
				 MSG_WriteSBits((int32)val, pTest->significant_bits);
			 }
			 else
			 {
				 MSG_WriteBits((uint32)val, pTest->significant_bits);
			 }
			 break;
		}
		case DT_INTEGER:
		{
			if (fieldSign)
			{
			   int32 signedInt = *(int32 *)&to[pTest->fieldOffset];
			   if (pTest->premultiply < 0.9999 || pTest->premultiply > 1.0001)
			   {
				   signedInt = (int32)((double)signedInt * pTest->premultiply);
			   }
			   MSG_WriteSBits(signedInt, pTest->significant_bits);
			}
			else
			{
			   uint32 unsignedInt = *(uint32 *)&to[pTest->fieldOffset];
			   if (pTest->premultiply < 0.9999 || pTest->premultiply > 1.0001)
			   {
				   unsignedInt = (uint32)((double)unsignedInt * pTest->premultiply);
			   }
			   MSG_WriteBits(unsignedInt, pTest->significant_bits);
			}
			break;
		}
		case DT_ANGLE:
			f2 = *(float *)&to[pTest->fieldOffset];
			MSG_WriteBitAngle(f2, pTest->significant_bits);
			break;
		case DT_TIMEWINDOW_8:
		{
			f2 = *(float *)&to[pTest->fieldOffset];
			int32 twVal = (int)(sv.time * 100.0) - (int)(f2 * 100.0);
			MSG_WriteSBits(twVal, 8);
			break;
		}
		case DT_TIMEWINDOW_BIG:
		{
			f2 = *(float *)&to[pTest->fieldOffset];
			int32 twVal = (int)(sv.time * pTest->premultiply) - (int)(f2 * pTest->premultiply);
			MSG_WriteSBits((int32)twVal, pTest->significant_bits);
			break;
		}
		case DT_STRING:
			MSG_WriteBitString((const char *)&to[pTest->fieldOffset]);
			break;
		default:
			Con_Printf(const_cast<char*>("unknown send field type\n"));
			break;
		}
	}
}

int DELTA_CheckDelta(unsigned char *from, unsigned char *to, delta_t *pFields)
{
	int sendfields;

	DELTA_ClearFlags(pFields);
	DELTA_MarkSendFields(from, to, pFields);
	sendfields = DELTA_CountSendFields(pFields);

	return sendfields;
}

int DELTA_WriteDelta(unsigned char *from, unsigned char *to, qboolean force, delta_t *pFields, void(*callback)(void))
{
	int sendfields;

	DELTA_ClearFlags(pFields);
	DELTA_MarkSendFields(from, to, pFields);
	sendfields = DELTA_CountSendFields(pFields);

	_DELTA_WriteDelta(from, to, force, pFields, callback, sendfields);
	return sendfields;
}

int _DELTA_WriteDelta(unsigned char *from, unsigned char *to, qboolean force, delta_t *pFields, void(*callback)(void), qboolean sendfields)
{
	int i;
	int bytecount;
	int bits[2];

	if (sendfields || force)
	{
		DELTA_SetSendFlagBits(pFields, bits, &bytecount);

		if (callback)
			callback();

		MSG_WriteBits(bytecount, 3);
		
		for (i = 0; i < bytecount; i++)
		{
			MSG_WriteBits(((byte*)bits)[i], 8);
		}

		DELTA_WriteMarkedFields(from, to, pFields);
	}

	return 1;
}

int DELTA_ParseDelta(unsigned char *from, unsigned char *to, delta_t *pFields, int nBufSize)
{
	delta_description_t *ptest;
	int i;
	int bits[2];	// this is a limit with 64 fields max in delta
	int nbytes;
	int bitfieldnumber;
	int fieldCount = pFields->fieldCount;
	int fieldType;
	int fieldSign;

	double d2;
	float t;
	int addt;
	char *st1, *st2;
	char c;
	int startbit;
	int j;
	qboolean finish;

	startbit = MSG_CurrentBit();
	Q_memset(bits, 0, 8);

	nbytes = MSG_ReadBits(3);

	for (i = 0; i < nbytes; i++)
	{
		((byte*)bits)[i] = MSG_ReadBits(8);
	}

	for (i = 0, ptest = pFields->pdd; i < fieldCount; i++, ptest++)
	{
		if (ptest->fieldOffset < 0 || ptest->fieldOffset >= nBufSize)
			continue;

		fieldType = ptest->fieldType & ~DT_SIGNED;

		bitfieldnumber = 1 << (i & 0x1F);

		if (!(bitfieldnumber & bits[i > 31]))
		{
			// Field was not sent to us, just transfer info from the "from"
			switch (fieldType)
			{
			case DT_BYTE:
				if ((int)(ptest->fieldOffset + sizeof(byte)) <= nBufSize)
					to[ptest->fieldOffset] = from[ptest->fieldOffset];
				break;
			case DT_SHORT:
				if ((int)(ptest->fieldOffset + sizeof(uint16)) <= nBufSize)
					*(uint16 *)&to[ptest->fieldOffset] = *(uint16 *)&from[ptest->fieldOffset];
				break;
			case DT_FLOAT:
			case DT_INTEGER:
			case DT_ANGLE:
			case DT_TIMEWINDOW_8:
			case DT_TIMEWINDOW_BIG:
				if ((int)(ptest->fieldOffset + sizeof(uint32)) <= nBufSize)
					*(uint32 *)&to[ptest->fieldOffset] = *(uint32 *)&from[ptest->fieldOffset];
				break;
			case DT_STRING:
				j = 0;
				st1 = (char*)&to[ptest->fieldOffset];
				st2 = (char*)&from[ptest->fieldOffset];

				// check buffer overrun
				while (((unsigned char*)st2 - from) < nBufSize)
				{
					*st1++ = *st2++;
					j++;

					if (!*st2)
						break;
				}

				// add null termination
				if (j > 0)
				{
					st1[j - 1] = 0;
				}
				break;
			default:
				Con_Printf(const_cast<char*>("unparseable field type %i\n"), fieldType);
			}
			continue;
		}

		ptest->stats.receivedcount++;

		fieldSign = ptest->fieldType & DT_SIGNED;
		switch (fieldType)
		{
		case DT_BYTE:
			if ((int)(ptest->fieldOffset + sizeof(byte)) <= nBufSize)
			{
				if (fieldSign)
				{
					d2 = (double)MSG_ReadSBits(ptest->significant_bits);

					if (ptest->premultiply <= 0.9999 || ptest->premultiply >= 1.0001)
					{
						d2 = d2 / ptest->premultiply;
					}
					if (ptest->postmultiply <= 0.9999 || ptest->postmultiply >= 1.0001)
					{
						d2 = d2 * ptest->postmultiply;
					}
					*(int8 *)&to[ptest->fieldOffset] = (int8)d2;
				}
				else
				{
					d2 = (double)MSG_ReadBits(ptest->significant_bits);

					if (ptest->premultiply <= 0.9999 || ptest->premultiply >= 1.0001)
					{
						d2 = d2 / ptest->premultiply;
					}
					if (ptest->postmultiply <= 0.9999 || ptest->postmultiply >= 1.0001)
					{
						d2 = d2 * ptest->postmultiply;
					}
					*(uint8 *)&to[ptest->fieldOffset] = (uint8)d2;
				}
			}
			break;
		case DT_SHORT:
			if ((int)(ptest->fieldOffset + sizeof(uint16)) <= nBufSize)
			{
				if (fieldSign)
				{
					d2 = (double)MSG_ReadSBits(ptest->significant_bits);

					if (ptest->premultiply <= 0.9999 || ptest->premultiply >= 1.0001)
					{
						d2 = d2 / ptest->premultiply;
					}
					if (ptest->postmultiply <= 0.9999 || ptest->postmultiply >= 1.0001)
					{
						d2 = d2 * ptest->postmultiply;
					}
					*(int16 *)&to[ptest->fieldOffset] = (int16)d2;
				}
				else
				{
					d2 = (double)MSG_ReadBits(ptest->significant_bits);

					if (ptest->premultiply <= 0.9999 || ptest->premultiply >= 1.0001)
					{
						d2 = d2 / ptest->premultiply;
					}
					if (ptest->postmultiply <= 0.9999 || ptest->postmultiply >= 1.0001)
					{
						d2 = d2 * ptest->postmultiply;
					}
					*(uint16 *)&to[ptest->fieldOffset] = (uint16)d2;
				}
			}
			break;
		case DT_FLOAT:
			if ((int)(ptest->fieldOffset + sizeof(float)) <= nBufSize)
			{
				if (fieldSign)
				{
					d2 = (double)MSG_ReadSBits(ptest->significant_bits);
				}
				else
				{
					d2 = (double)MSG_ReadBits(ptest->significant_bits);
				}

				if (ptest->premultiply <= 0.9999 || ptest->premultiply >= 1.0001)
				{
					d2 = d2 / ptest->premultiply;
				}
				if (ptest->postmultiply <= 0.9999 || ptest->postmultiply >= 1.0001)
				{
					d2 = d2 * ptest->postmultiply;
				}
				*(float *)&to[ptest->fieldOffset] = (float)d2;
			}
			break;
		case DT_INTEGER:
			if ((int)(ptest->fieldOffset + sizeof(uint32)) <= nBufSize)
			{
				if (fieldSign)
				{
					d2 = (double)MSG_ReadSBits(ptest->significant_bits);

					if (ptest->premultiply <= 0.9999 || ptest->premultiply >= 1.0001)
					{
						d2 = d2 / ptest->premultiply;
					}
					if (ptest->postmultiply <= 0.9999 || ptest->postmultiply >= 1.0001)
					{
						d2 = d2 * ptest->postmultiply;
					}
					*(int32 *)&to[ptest->fieldOffset] = (int32)d2;
				}
				else
				{
					d2 = (double)MSG_ReadBits(ptest->significant_bits);

					if (ptest->premultiply <= 0.9999 || ptest->premultiply >= 1.0001)
					{
						d2 = d2 / ptest->premultiply;
					}
					if (ptest->postmultiply <= 0.9999 || ptest->postmultiply >= 1.0001)
					{
						d2 = d2 * ptest->postmultiply;
					}
					*(uint32 *)&to[ptest->fieldOffset] = (uint32)d2;
				}
			}
			break;
		case DT_ANGLE:
			if ((int)(ptest->fieldOffset + sizeof(float)) <= nBufSize)
			{
				*(float*)&to[ptest->fieldOffset] = MSG_ReadBitAngle(ptest->significant_bits);
			}
			break;
		case DT_TIMEWINDOW_8:
			if ((int)(ptest->fieldOffset + sizeof(float)) <= nBufSize)
			{
				addt = MSG_ReadSBits(8);

				t = (float)((cl.mtime[0] * 100.0 - addt) / 100.0);
				*(float *)&to[ptest->fieldOffset] = t;
			}
			break;
		case DT_TIMEWINDOW_BIG:
			if ((int)(ptest->fieldOffset + sizeof(float)) <= nBufSize)
			{
				addt = MSG_ReadSBits(ptest->significant_bits);

				if (ptest->premultiply <= 0.9999 || ptest->premultiply >= 1.0001)
				{
					t = (float)((cl.mtime[0] * ptest->premultiply - addt) / ptest->premultiply);
				}
				else
				{
					t = (float)(cl.mtime[0] - addt);
				}
				*(float *)&to[ptest->fieldOffset] = t;
			}
			break;
		case DT_STRING:
			j = 0;
			st1 = (char*)&to[ptest->fieldOffset];

			// check buffer overrun
			while ((ptest->fieldOffset + j) < nBufSize)
			{
				c = MSG_ReadBits(8);
				*st1++ = c;
				j++;

				if (!c)
					break;
			}

			// add null termination
			if (j > 0)
			{
				st1[j - 1] = 0;
			}

			break;
		default:
			Con_Printf(const_cast<char*>("unparseable field type %i\n"), fieldType);
			break;
		}
	}

	return MSG_CurrentBit() - startbit;
}

void DELTA_AddEncoder(char *name, void(*conditionalencode)(struct delta_s *, const unsigned char *, const unsigned char *))
{
	delta_encoder_t *p = (delta_encoder_t *)Mem_ZeroMalloc(sizeof(delta_encoder_t));
	p->name = Mem_Strdup(name);
	p->conditionalencode = conditionalencode;
	p->next = g_encoders;
	g_encoders = p;
}

void DELTA_ClearEncoders(void)
{
	delta_encoder_t *n, *p = g_encoders;
	while (p)
	{
		n = p->next;
		Mem_Free(p->name);
		Mem_Free(p);
		p = n;
	}
	g_encoders = NULL;
}

encoder_t DELTA_LookupEncoder(char *name)
{
	delta_encoder_t *p = g_encoders;
	while (p)
	{
		if (!Q_strcasecmp(name, p->name))
		{
			return p->conditionalencode;
		}
		p = p->next;
	}
	return NULL;
}

int DELTA_CountLinks(delta_link_t *plinks)
{
	delta_link_t *p = plinks;

	int c;
	for (c = 0; p != NULL; c++)
	{
		p = p->next;
	}

	return c;
}

void DELTA_ReverseLinks(delta_link_t **plinks)
{
	delta_link_t *n, *p = *plinks;
	delta_link_t *newlist = NULL;

	while (p)
	{
		n = p->next;
		p->next = newlist;
		newlist = p;
		p = n;
	}

	*plinks = newlist;
}

void DELTA_ClearLinks(delta_link_t **plinks)
{
	delta_link_t *n, *p = *plinks;
	while (p)
	{
		n = p->next;
		Mem_Free(p);
		p = n;
	}
	*plinks = 0;
}

delta_t *DELTA_BuildFromLinks(delta_link_t **pplinks)
{
	delta_description_t *pdesc, *pcur;
	delta_t *pdelta;
	delta_link_t *p;
	int count;

	pdelta = (delta_t *)Mem_ZeroMalloc(sizeof(delta_t));

	DELTA_ReverseLinks(pplinks);

	count = DELTA_CountLinks(*pplinks);

	pdesc = (delta_description_t *)Mem_ZeroMalloc(sizeof(delta_description_t)* count);

	for (p = *pplinks, pcur = pdesc; p != NULL; p = p->next, pcur++)
	{
		Q_memcpy(pcur, p->delta, sizeof(delta_description_t));
		Mem_Free(p->delta);
		p->delta = NULL;
	}

	DELTA_ClearLinks(pplinks);

	pdelta->dynamic = 1;
	pdelta->fieldCount = count;
	pdelta->pdd = pdesc;

	return pdelta;
}

int DELTA_FindOffset(int count, delta_definition_t *pdef, char *fieldname)
{
	for (int i = 0; i < count; i++)
	{
		if (!Q_strcasecmp(fieldname, pdef[i].fieldName))
			return pdef[i].fieldOffset;
	}

	Sys_Error("Couldn't find offset for %s!!!\n", fieldname);
}

qboolean DELTA_ParseType(delta_description_t *pdelta, char **pstream)
{
	// Read the stream till we hit the end
	while (*pstream = COM_Parse(*pstream), Q_strlen(com_token) > 0)
	{
		if (!Q_strcasecmp(com_token, ","))
			return TRUE;	// end of type description

		if (!Q_strcasecmp(com_token, "|"))
			continue;	// skip | token

		// Determine field type
		if (!Q_strcasecmp(com_token, "DT_SIGNED"))
			pdelta->fieldType |= DT_SIGNED;
		else if (!Q_strcasecmp(com_token, "DT_BYTE"))
			pdelta->fieldType |= DT_BYTE;
		else if (!Q_strcasecmp(com_token, "DT_SHORT"))
			pdelta->fieldType |= DT_SHORT;
		else if (!Q_strcasecmp(com_token, "DT_FLOAT"))
			pdelta->fieldType |= DT_FLOAT;
		else if (!Q_strcasecmp(com_token, "DT_INTEGER"))
			pdelta->fieldType |= DT_INTEGER;
		else if (!Q_strcasecmp(com_token, "DT_ANGLE"))
			pdelta->fieldType |= DT_ANGLE;
		else if (!Q_strcasecmp(com_token, "DT_TIMEWINDOW_8"))
			pdelta->fieldType |= DT_TIMEWINDOW_8;
		else if (!Q_strcasecmp(com_token, "DT_TIMEWINDOW_BIG"))
			pdelta->fieldType |= DT_TIMEWINDOW_BIG;
		else if (!Q_strcasecmp(com_token, "DT_STRING"))
			pdelta->fieldType |= DT_STRING;
		else
			Sys_Error("DELTA_ParseField:  Unknown type or type flag %s\n", com_token);
	}

	// We are hit the end of the stream
	Con_Printf(const_cast<char*>("DELTA_ParseField:  Expecting fieldtype info\n"));

	return false;
}

qboolean DELTA_ParseField(int count, delta_definition_t *pdefinition, delta_link_t *pField, char **pstream)
{
	qboolean readpost;

	readpost = false;
	if (Q_strcasecmp(com_token, "DEFINE_DELTA"))
	{
		if (Q_strcasecmp(com_token, "DEFINE_DELTA_POST"))
		{
			Sys_Error(__FUNCTION__ ":  Expecting DEFINE_*, got %s\n", com_token);
		}
		readpost = true;
	}

	*pstream = COM_Parse(*pstream);
	if (Q_strcasecmp(com_token, "("))
	{
		Sys_Error(__FUNCTION__ ":  Expecting (, got %s\n", com_token);
	}

	*pstream = COM_Parse(*pstream);
	if (Q_strlen(com_token) <= 0)
	{
		Sys_Error(__FUNCTION__ ":  Expecting fieldname\n");
	}

	Q_strncpy(pField->delta->fieldName, com_token, sizeof(pField->delta->fieldName) - 1);
	pField->delta->fieldName[sizeof(pField->delta->fieldName) - 1] = 0;
	
	pField->delta->fieldOffset = DELTA_FindOffset(count, pdefinition, com_token);

	*pstream = COM_Parse(*pstream);
	if (!DELTA_ParseType(pField->delta, pstream))
	{
		return false;
	}

	*pstream = COM_Parse(*pstream);
	pField->delta->fieldSize = 1;
	pField->delta->significant_bits = Q_atoi(com_token);
	*pstream = COM_Parse(*pstream);
	*pstream = COM_Parse(*pstream);
	pField->delta->premultiply = (float)Q_atof(com_token);

	if (readpost)
	{
		*pstream = COM_Parse(*pstream);
		*pstream = COM_Parse(*pstream);
		pField->delta->postmultiply = (float)Q_atof(com_token);
	}
	else
	{
		pField->delta->postmultiply = 1.0f;
	}

	*pstream = COM_Parse(*pstream);
	if (Q_strcasecmp(com_token, ")"))
		Con_Printf(const_cast<char*>(__FUNCTION__ ":  Expecting ), got %s\n"), com_token);

	*pstream = COM_Parse(*pstream);
	if (Q_strcasecmp(com_token, ","))
		COM_UngetToken();

	return TRUE;
}

void DELTA_FreeDescription(delta_t **ppdesc)
{
	delta_t *p;

	if (ppdesc == NULL)
		return;

	p = *ppdesc;
	if (p != NULL)
	{
		if (p->dynamic != NULL)
			Mem_Free(p->pdd);
		Mem_Free(p);
		*ppdesc = 0;
	}
}

void DELTA_AddDefinition(char *name, delta_definition_t *pdef, int numelements)
{
	delta_definition_list_t *p = g_defs;
	while (p != NULL)
	{
		if (!Q_strcasecmp(name, p->ptypename))
			break;
		p = p->next;
	}

	if (p == NULL)
	{
		p = (delta_definition_list_t *)Mem_ZeroMalloc(sizeof(delta_definition_list_t));
		p->ptypename = Mem_Strdup(name);
		p->next = g_defs;
		g_defs = p;
	}

	p->pdefinition = pdef;
	p->numelements = numelements;
}

void DELTA_ClearDefinitions(void)
{
	delta_definition_list_t *n, *p = g_defs;
	while (p != NULL)
	{
		n = p->next;
		Mem_Free(p->ptypename);
		Mem_Free(p);
		p = n;
	}
	g_defs = 0;
}

delta_definition_t *DELTA_FindDefinition(char *name, int *count)
{
	delta_definition_list_t *p = g_defs;

	*count = 0;

	while (p != NULL)
	{
		if (!Q_strcasecmp(name, p->ptypename))
		{
			*count = p->numelements;
			return p->pdefinition;
		}
		p = p->next;
	}

	return NULL;
}

void DELTA_SkipDescription(char **pstream)
{
	*pstream = COM_Parse(*pstream);
	do
	{
		*pstream = COM_Parse(*pstream);
		if (Q_strlen(com_token) <= 0)
			Sys_Error("Error during description skip");

	} while (Q_strcasecmp(com_token, "}"));
}

qboolean DELTA_ParseOneField(char **ppstream, delta_link_t **pplist, int count, delta_definition_t *pdefinition)
{
	delta_link_t *newlink;
	delta_link_t link;

	while (true)
	{
		if (!Q_strcasecmp(com_token, "}"))
		{
			COM_UngetToken();
			break;
		}

		*ppstream = COM_Parse(*ppstream);
		if (Q_strlen(com_token) <= 0)
			break;

		Q_memset(&link, 0, sizeof(delta_link_t));
		link.delta = (delta_description_t *)Mem_ZeroMalloc(sizeof(delta_description_t));
		if (!DELTA_ParseField(count, pdefinition, &link, ppstream))
			return false;

		newlink = (delta_link_t *)Mem_ZeroMalloc(sizeof(delta_link_t));
		newlink->delta = link.delta;
		newlink->next = *pplist;
		*pplist = newlink;
	}
	return true;
}

qboolean DELTA_ParseDescription(char *name, delta_t **ppdesc, char *pstream)
{
	delta_link_t *links;
	delta_definition_t *pdefinition = NULL;
	char encoder[32];
	char source[32];
	int count;


	links = NULL;
	count = 0;
	encoder[0] = 0;

	if (ppdesc == NULL)
		Sys_Error(__FUNCTION__ ": called with no delta_description_t\n");
	
	*ppdesc = NULL;

	if (pstream == NULL)
		Sys_Error(__FUNCTION__ ": called with no data stream\n");

	while (true)
	{
		// Parse delta name
		pstream = COM_Parse(pstream);
		if (com_token[0] == 0)
		{
			break;
		}
		if (Q_strcasecmp(com_token, name))
		{
			DELTA_SkipDescription(&pstream);
		}
		else
		{
			pdefinition = DELTA_FindDefinition(com_token, &count);
			if (!pdefinition)
				Sys_Error(__FUNCTION__ ":  Unknown data type:  %s\n", com_token);

			// Parse source of conditional encoder
			pstream = COM_Parse(pstream);
			if (com_token[0] == 0)
				Sys_Error(__FUNCTION__ ":  Unknown encoder :  %s\nValid values:\nnone\ngamedll funcname\nclientdll funcname\n", com_token);
			
			if (Q_strcasecmp(com_token, "none"))
			{
				Q_strncpy(source, com_token, sizeof(source)-1);
				source[sizeof(source)-1] = 0;

				// Parse custom encoder function name
				pstream = COM_Parse(pstream);
				if (Q_strlen(com_token) <= 0)
				{
					Sys_Error(__FUNCTION__ ":  Expecting encoder\n");
				}

				Q_strncpy(encoder, com_token, sizeof(encoder)-1);
				encoder[sizeof(encoder)-1] = 0;
			}

			// Parse fields
			while (true)
			{
				pstream = COM_Parse(pstream);

				if (Q_strlen(com_token) <= 0)
					break;
				
				if (!Q_strcasecmp(com_token, "}"))
					break;
				
				if (Q_strcasecmp(com_token, "{"))
					Con_Printf(const_cast<char*>(__FUNCTION__ ":  Expecting {, got %s\n"), com_token);
				
				if (!DELTA_ParseOneField(&pstream, &links, count, pdefinition))
					return FALSE;
			}
		}
	}

	*ppdesc = DELTA_BuildFromLinks(&links);

	if (Q_strlen(encoder) > 0)
	{
		Q_strncpy((*ppdesc)->conditionalencodename, encoder, sizeof((*ppdesc)->conditionalencodename) - 1);
		(*ppdesc)->conditionalencodename[sizeof((*ppdesc)->conditionalencodename) - 1] = 0;
		(*ppdesc)->conditionalencode = NULL;
	}

	return TRUE;
}

qboolean DELTA_Load(char *name, delta_t **ppdesc, char *pszFile)
{
	char *pbuf;
	qboolean bret;

	pbuf = (char *)COM_LoadFile(pszFile, 5, 0);
	if (pbuf == NULL)
	{
		Sys_Error(__FUNCTION__ ":  Couldn't load file %s\n", pszFile);
		return false;
	}

	bret = DELTA_ParseDescription(name, ppdesc, pbuf);

	Mem_Free(pbuf);

	return bret;
}

void DELTA_RegisterDescription(char *name)
{
	delta_registry_t *p = (delta_registry_t *)Mem_ZeroMalloc(sizeof(delta_registry_t));
	p->next = g_deltaregistry;
	g_deltaregistry = p;
	p->name = Mem_Strdup(name);
	p->pdesc = NULL;
}

void DELTA_ClearRegistrations(void)
{
	delta_registry_t *n, *p = g_deltaregistry;
	while (p != NULL)
	{
		n = p->next;
		Mem_Free(p->name);
		if (p->pdesc != NULL)
			DELTA_FreeDescription(&p->pdesc);
		Mem_Free(p);
		p = n;
	}
	g_deltaregistry = NULL;
}

delta_t **DELTA_LookupRegistration(const char *name)
{
	delta_registry_t *p = g_deltaregistry;
	while (p != NULL)
	{
		if (!Q_strcasecmp(p->name, name))
			return &p->pdesc;
		
		p = p->next;
	}
	return NULL;
}

void DELTA_ClearStats(delta_t *p)
{
	if (p == NULL)
		return;

	for (int i = p->fieldCount - 1; i >= 0; i--)
	{
		p->pdd[i].stats.sendcount = 0;
		p->pdd[i].stats.receivedcount = 0;
	}
}

void DELTA_ClearStats_f(void)
{
	delta_registry_t *p;

	Con_Printf(const_cast<char*>("Clearing delta stats\n"));
	for (p = g_deltaregistry; p != NULL; p = p->next)
		DELTA_ClearStats(p->pdesc);
}

void DELTA_PrintStats(const char *name, delta_t *p)
{
	if (p == NULL)
		return;
	
	delta_description_t *dt = p->pdd;
	
	Con_Printf(const_cast<char*>("Stats for '%s'\n"), name);

	for (int i = 0; i < p->fieldCount; i++, dt++)
		Con_Printf(const_cast<char*>("  %02i % 10s:  s % 5i r % 5i\n"), i + 1, dt->fieldName, dt->stats.sendcount, dt->stats.receivedcount);
	
	Con_Printf(const_cast<char*>("\n"));
}

void DELTA_DumpStats_f(void)
{
	Con_Printf(const_cast<char*>("Delta Stats\n"));
	
	for (delta_registry_t *dr = g_deltaregistry; dr != NULL; dr = dr->next)
		DELTA_PrintStats(dr->name, dr->pdesc);
}

void DELTA_Init()
{
	Cmd_AddCommand(const_cast<char*>("delta_stats"), DELTA_DumpStats_f);
	Cmd_AddCommand(const_cast<char*>("delta_clear"), DELTA_ClearStats_f);

	DELTA_AddDefinition(const_cast<char*>("clientdata_t"), g_ClientDataDefinition, ARRAYSIZE(g_ClientDataDefinition));
	DELTA_AddDefinition(const_cast<char*>("weapon_data_t"), g_WeaponDataDefinition, ARRAYSIZE(g_WeaponDataDefinition));
	DELTA_AddDefinition(const_cast<char*>("usercmd_t"), g_UsercmdDataDefinition, ARRAYSIZE(g_UsercmdDataDefinition));
	DELTA_AddDefinition(const_cast<char*>("entity_state_t"), g_EntityDataDefinition, ARRAYSIZE(g_EntityDataDefinition));
	DELTA_AddDefinition(const_cast<char*>("entity_state_player_t"), g_EntityDataDefinition, ARRAYSIZE(g_EntityDataDefinition));
	DELTA_AddDefinition(const_cast<char*>("custom_entity_state_t"), g_EntityDataDefinition, ARRAYSIZE(g_EntityDataDefinition));
	DELTA_AddDefinition(const_cast<char*>("event_t"), g_EventDataDefinition, ARRAYSIZE(g_EventDataDefinition));
}

void DELTA_Shutdown()
{
	DELTA_ClearEncoders();
	DELTA_ClearDefinitions();
	DELTA_ClearRegistrations();
}
