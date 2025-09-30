#pragma once

#include "archtypes.h"

// Authentication types
enum AUTH_IDTYPE
{
	AUTH_IDTYPE_UNKNOWN	= 0,
	AUTH_IDTYPE_STEAM	= 1,
	AUTH_IDTYPE_VALVE	= 2,
	AUTH_IDTYPE_LOCAL	= 3
};

typedef struct USERID_s
{
	int idtype;
	uint64 m_SteamID;
	unsigned int clientip;
} USERID_t;
