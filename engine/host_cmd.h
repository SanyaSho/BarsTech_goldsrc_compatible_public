#ifndef ENGINE_HOST_CMD_H
#define ENGINE_HOST_CMD_H

#include "GameUI/CareerDefs.h"

#define FILETIME_TO_QWORD(ft) \
	((((uint64)ft.dwHighDateTime) << 32) + ft.dwLowDateTime)

#define FILETIME_TO_PAIR(f,h)\
	(((uint64)f << 32) | h)

#define SAVEFILE_HEADER		MAKEID('V','A','L','V')		// little-endian "VALV"
#define SAVEGAME_HEADER		MAKEID('J','S','A','V')		// little-endian "JSAV"
#define SAVEGAME_VERSION	0x0071						// Version 0.71

typedef void(*SV_SAVEGAMECOMMENT_FUNC)(char *, int);

typedef struct GAME_HEADER_s
{
	char mapName[32];
	char comment[80];
	int mapCount;
} GAME_HEADER;

typedef struct SAVE_HEADER_s
{
	int saveId;
	int version;
	int skillLevel;
	int entityCount;
	int connectionCount;
	int lightStyleCount;
	float time;
	char mapName[32];
	char skyName[32];
	int skyColor_r;
	int skyColor_g;
	int skyColor_b;
	float skyVec_x;
	float skyVec_y;
	float skyVec_z;
} SAVE_HEADER;

typedef struct SAVELIGHTSTYLE_s
{
	int index;
	char style[MAX_LIGHTSTYLES];
} SAVELIGHTSTYLE;

typedef struct TITLECOMMENT_s
{
	char *pBSPName;
	char *pTitleName;
} TITLECOMMENT;

extern CareerStateType g_careerState;
extern qboolean noclip_anglehack;
extern int current_skill;
extern int gHostSpawnCount;

extern bool g_iQuitCommandIssued;
extern bool g_bMajorMapChange;

void Host_InitCommands();

void Host_UpdateStats(void);
void Host_NextDemo();
void Host_Quit_f();

#endif //ENGINE_HOST_CMD_H
