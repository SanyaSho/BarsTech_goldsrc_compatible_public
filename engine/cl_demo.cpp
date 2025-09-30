#include "quakedef.h"
#include "cl_demo.h"
#include "server.h"
#include "DemoPlayerWrapper.h"
#include "client.h"
#include "host.h"
#include "cl_main.h"
#if defined(GLQUAKE)
#include "gl_screen.h"
#else
#include "screen.h"
#endif
#include "r_studio.h"
#include "cl_parsefn.h"
#include "cdll_exp.h"
#include "shake.h"
#include "cl_parse.h"
#include "tmessage.h"

char gDemoMessageBuffer[512];

demodirectory_t demodir;
cvar_t cl_gamegauge = { const_cast<char*>("cl_gg"), const_cast<char*>("0") };
int g_rgFPS[GG_MAX_FRAMES];
int g_iMinFPS, g_iMaxFPS;
int g_iSecond;
demoentry_t* pCurrentEntry;
demoheader_t demoheader;
int nCurEntryIndex;
demo_info_t g_rp;
demo_info_t dem_frames[MAX_DEMO_FRAMES];
float g_demotimescale = 1.0f;
ScreenFade sf_demo;
qboolean bInFadeout;

client_textmessage_t tm_demomessage =
{
	0,
	255, 255, 255, 255,
	255, 255, 255, 255,
	-1, -1,
	0, 0,
	0, 0,
	"__DEMOMESSAGE__",
	gDemoMessageBuffer
};

demo_api_t demoapi = 
{
	&CL_DemoAPIRecording,
	&CL_DemoAPIPlayback,
	&CL_DemoAPITimedemo,
	&CL_WriteClientDLLMessage
};

float CL_DemoOutTime()
{
	return (float)realtime;
}

float CL_DemoInTime()
{
	return (float)(g_demotimescale * realtime);
}

void CL_FixPointers(demo_info_t* info)
{
	info->rp.cmd = &info->cmd;
	info->rp.movevars = &info->movevars;
}

// копия COM_CopyFileChunk для демок?

#define BLOCK_SIZE 1024   // For copying operations

void CL_DemoCopyFileChunk(FILE* dst, FILE* src, int nSize)
{
	int   copysize = nSize;
	char  copybuf[BLOCK_SIZE];
	if (copysize > BLOCK_SIZE)
	{
		while (copysize > BLOCK_SIZE)
		{
			FS_Read(copybuf, BLOCK_SIZE, 1, src);
			FS_Write(copybuf, BLOCK_SIZE, 1, dst);
			copysize -= BLOCK_SIZE;
		}
		copysize = nSize - ((nSize - (BLOCK_SIZE + 1)) & ~(BLOCK_SIZE - 1)) - BLOCK_SIZE;
	}
	FS_Read(copybuf, copysize, 1, src);
	FS_Write(copybuf, copysize, 1, dst);
	FS_Flush(src);
	FS_Flush(dst);
}

void CL_SetDemoViewInfo(ref_params_t* rp, vec_t* view, int viewmodel)
{
	g_rp.rp = *rp;
	CL_FixPointers(&g_rp);
	g_rp.cmd = *rp->cmd;
	g_rp.movevars = *rp->movevars;
	VectorCopy(view, g_rp.view);
	g_rp.viewmodel = viewmodel;
}

void CL_GetDemoViewInfo_OLD(ref_params_t* rp, vec_t* view, int* viewmodel)
{
	vec3_t delta;
	float targettime, frac, d;
	movevars_t* oldmv;
	usercmd_t* oldcmd;
	int oldviewport[4];
	demo_info_t* from, * to;
	int i;

	oldmv = rp->movevars;
	oldcmd = rp->cmd;

	for (i = 0; i < 4; i++)
		oldviewport[i] = rp->viewport[i];

	*rp = dem_frames[0].rp;

	rp->movevars = oldmv;
	rp->cmd = oldcmd;

	for (i = 0; i < 4; i++)
		rp->viewport[i] = oldviewport[i];

	if (cls.timedemo == true)
	{
		VectorCopy(dem_frames[0].view, view);
		*viewmodel = dem_frames[0].viewmodel;
		return;
	}

	targettime = CL_DemoInTime() - cls.demostarttime - 0.02f;
	to = &dem_frames[0];

	for (i = 1; i < MAX_DEMO_FRAMES - 1; i++)
	{
		from = &dem_frames[i];
		CL_FixPointers(from);

		if (from->timestamp == 0.0f || from->timestamp < targettime)
			break;

		to = from;
	}

	if (from == NULL)
		return;

	if (from->timestamp == 0.0f || from->timestamp == to->timestamp)
		return;

	frac = (targettime - from->timestamp) / (to->timestamp - from->timestamp);
	frac = clamp(frac, 0.0f, 1.0f);

	VectorSubtract(to->rp.vieworg, from->rp.vieworg, delta);
	VectorMA(from->rp.vieworg, frac, delta, rp->vieworg);

	for (i = 0; i < 3; i++)
	{
		// HL25 has 'ang2' local there
		d = to->rp.viewangles[i] - from->rp.viewangles[i];

		if (d > 180.0f)
		{
			d -= 360.0f;
		}
		else if (d < -180.0f)
		{
			d += 360.0f;
		}

		rp->viewangles[i] = from->rp.viewangles[i] + d * frac;
	}

	COM_NormalizeAngles(rp->viewangles);

	VectorSubtract(to->rp.simvel, from->rp.simvel, delta);
	VectorMA(from->rp.simvel, frac, delta, rp->simvel);

	VectorSubtract(to->rp.simorg, from->rp.simorg, delta);
	VectorMA(from->rp.simorg, frac, delta, rp->simorg);

	for (i = 0; i < 3; i++)
		rp->viewheight[i] = from->rp.viewheight[i] + (to->rp.viewheight[i] - from->rp.viewheight[i]) * frac;

	VectorSubtract(to->view, from->view, delta);
	VectorMA(from->view, frac, delta, view);

	*viewmodel = to->viewmodel;
}

void CL_GetDemoViewInfo(ref_params_t* rp, float* view, int* viewmodel)
{
	if (DemoPlayer_IsActive())
		DemoPlayer_GetDemoViewInfo(rp, view, viewmodel);
	else
		CL_GetDemoViewInfo_OLD(rp, view, viewmodel);
}

void CL_AppendDemo_f()
{
	char name[MAX_PATH], szTempName[MAX_PATH], szOriginalName[MAX_PATH], szMapName[MAX_PATH];
	demoentry_t temp;
	int track, nSize, c, copysize, swlen;
	FileHandle_t fp;
	byte cmd;
	float f;

	if (cmd_source != src_command)
		return;

	if (cls.demorecording || cls.demoplayback)
		return Con_Printf(const_cast<char*>("Appenddemo only available when not running or recording a demo.\n"));

	if (cls.state != ca_active)
		return Con_Printf(const_cast<char*>("You must be in an active map spawned on a machine that allows creation of files in %s\n"), com_gamedir);

	c = Cmd_Argc();
	if (c != 2 && c != 3)
		return Con_Printf(const_cast<char*>("appenddemo <demoname> <cd track>\n"));

	if (strstr(Cmd_Argv(1), ".."))
		return Con_Printf(const_cast<char*>("Relative pathnames are not allowed.\n"));

	track = -1;
	if (c == 3)
	{
		track = Q_atoi(Cmd_Argv(2));
		Con_Printf(const_cast<char*>("Forcing CD track to %i\n"), track);
	}

	snprintf(name, MAX_PATH, "%s", Cmd_Argv(1));
	COM_DefaultExtension(name, const_cast<char*>(".dem"));

	if (!COM_HasFileExtension(name, const_cast<char*>("dem")))
		return Con_Printf(const_cast<char*>("Error: couldn't open %s, demo files must have .dem extension.\n"), name);

	Con_Printf(const_cast<char*>("Appending to demo %s.\n"), name);
	Q_strncpy(cls.demofilename, name, 255);
	cls.demofilename[255] = 0;

	fp = FS_OpenPathID(name, "rb", "GAMECONFIG");
	if (!fp)
	{
		Con_Printf(const_cast<char*>("Error:  couldn't open demo file %s for appending.\n"), name);
		return FS_Close(fp);
	}

	COM_StripExtension(name, szTempName);
	COM_AddExtension(szTempName, const_cast<char*>(".dm2"), sizeof(szTempName));

	if (!COM_HasFileExtension(szTempName, const_cast<char*>("dm2")))
	{
		Con_Printf(const_cast<char*>("Error: could not open temporary file %s.\n"), szTempName);
		return FS_Close(fp);
	}

	cls.demofile = FS_OpenPathID(szTempName, "w+b", "GAMECONFIG");
	if (!cls.demofile)
	{
		Con_Printf(const_cast<char*>("ERROR: couldn't open %s.\n"), name);
		return FS_Close(fp);
	}

	FS_Seek(fp, 0, FILESYSTEM_SEEK_TAIL);
	copysize = FS_Tell(fp);
	FS_Seek(fp, 0, FILESYSTEM_SEEK_HEAD);
	FS_Seek(cls.demofile, 0, FILESYSTEM_SEEK_HEAD);

	CL_DemoCopyFileChunk((FILE*)cls.demofile, (FILE*)fp, copysize);

	FS_Close(fp);

	FS_Seek(cls.demofile, 0, FILESYSTEM_SEEK_HEAD);
	FS_Read(&demoheader, sizeof(demoheader), 1, cls.demofile);

	if (Q_strncmp(demoheader.szFileStamp, "HLDEMO", 6))
	{
		Con_Printf(const_cast<char*>("%s is not a demo file\n"), name);
		unlink(szTempName);
		FS_Close(cls.demofile);
		cls.demofile = NULL;
		return;
	}

	COM_FileBase(cl.worldmodel->name, szMapName);
	if (Q_strcmp(demoheader.szMapName, szMapName))
	{
		Con_Printf(const_cast<char*>("%s was recorded on a different map, cannot append.\n"), name);
		unlink(szTempName);
		FS_Close(cls.demofile);
		cls.demofile = NULL;
		return;
	}

	if (demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL)
	{
		Con_Printf(const_cast<char*>("ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n"),
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL);
		unlink(szTempName);
		FS_Close(cls.demofile);
		cls.demofile = NULL;
		return;
	}

	FS_Seek(cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&demodir.nEntries, sizeof(demodir.nEntries), 1, cls.demofile);

	if (demodir.nEntries < 1 || demodir.nEntries > DEMO_MAX_DIR_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: demo had bogus # of directory entries:  %i\n"), demodir.nEntries);
		FS_Close(cls.demofile);
		cls.demofile = NULL;
		return;
	}

	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc(sizeof(demoentry_t) * (demodir.nEntries + 1));
	FS_Read(demodir.p_rgEntries, sizeof(demoentry_t) * demodir.nEntries, 1, cls.demofile);

	nCurEntryIndex = demodir.nEntries;
	pCurrentEntry = &demodir.p_rgEntries[demodir.nEntries];

	demodir.nEntries++;

	Q_memset(pCurrentEntry, 0, sizeof(demoentry_t));
	FS_Seek(cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);

	pCurrentEntry->nEntryType = DEMO_NORMAL;
	snprintf(pCurrentEntry->szDescription, sizeof(pCurrentEntry->szDescription), "Playback '%i'", demodir.nEntries - 1);
	pCurrentEntry->nFlags = 0;
	pCurrentEntry->nCDTrack = track;
	pCurrentEntry->fTrackTime = 0.0;
	cmd = dem_jumptime;
	pCurrentEntry->nOffset = FS_Tell(cls.demofile);

	cls.demostarttime = CL_DemoOutTime();
	cls.demoframecount = 0;
	cls.demostartframe = host_framecount;

	FS_Write(&cmd, sizeof(cmd), 1, cls.demofile);
	f = LittleFloat(CL_DemoOutTime() - cls.demostarttime);

	FS_Write(&f, sizeof(f), 1, cls.demofile);
	swlen = LittleLong(host_framecount - cls.demostartframe);

	FS_Write(&swlen, sizeof(swlen), 1, cls.demofile);

	cls.demowaiting = true;
	cls.demorecording = true;
	cls.td_lastframe = -1;
	cls.td_startframe = host_framecount;
	cls.demoappending = true;
}

void CL_SwapDemo_f()
{
	char name[MAX_PATH], szTempName[MAX_PATH], szOriginalName[MAX_PATH];
	demoentry_t temp, * newentry, * oldentry;
	int nSegment1, nSegment2, n, c, i;
	FileHandle_t fp;


	if (cmd_source != src_command)
		return;

	if (cls.demorecording || cls.demoplayback)
		return Con_Printf(const_cast<char*>("Swapdemo only available when not running or recording a demo.\n"));

	c = Cmd_Argc();

	if (c != 4)
		return Con_Printf(const_cast<char*>("Swapdemo <demoname> <seg#> <seg#>\nSwaps segments, segment 1 cannot be moved\n"));

	if (strstr(Cmd_Argv(1), ".."))
		return Con_Printf(const_cast<char*>("Relative pathnames are not allowed.\n"));

	nSegment1 = Q_atoi(Cmd_Argv(2));
	if (nSegment1 <= 1)
		return Con_Printf(const_cast<char*>("Cannot swap the STARTUP segment.\n"));

	nSegment2 = Q_atoi(Cmd_Argv(3));
	snprintf(name, MAX_PATH, "%s", Cmd_Argv(1));
	COM_DefaultExtension(name, const_cast<char*>(".dem"));

	if (!COM_HasFileExtension(name, const_cast<char*>("dem")))
		return Con_Printf(const_cast<char*>("Error: couldn't open %s, demo files must have .dem extension.\n"), name);

	Con_Printf(const_cast<char*>("Swapping segment %i for %i from demo %s.\n"), nSegment1, nSegment2, name);
	Q_strncpy(szOriginalName, name, 255);
	szOriginalName[255] = 0;

	nSegment1--;
	nSegment2--;

	fp = FS_Open(szOriginalName, "rb");

	if (fp == NULL)
		return Con_Printf(const_cast<char*>("Error: couldn't open demo file %s for swapping.\n"), name);

	COM_StripExtension(name, szTempName);
	COM_AddExtension(szTempName, const_cast<char*>(".dm2"), sizeof(szTempName));

	if (!COM_HasFileExtension(szTempName, const_cast<char*>("dm2")))
	{
		Con_Printf(const_cast<char*>("Error: couldn't open temporary file %s.\n"), szTempName);
		return FS_Close(fp);
	}

	cls.demofile = FS_OpenPathID(szTempName, "w+b", "GAMECONFIG");
	if (!cls.demofile)
	{
		Con_Printf(const_cast<char*>("Error: couldn't open %s.\n"), szTempName);
		return FS_Close(fp);
	}

	FS_Seek(fp, 0, FILESYSTEM_SEEK_TAIL);
	n = FS_Tell(fp);

	FS_Seek(fp, 0, FILESYSTEM_SEEK_HEAD);
	FS_Seek(cls.demofile, 0, FILESYSTEM_SEEK_HEAD);

	CL_DemoCopyFileChunk((FILE*)cls.demofile, (FILE*)fp, n);

	FS_Seek(fp, 0, FILESYSTEM_SEEK_HEAD);
	FS_Seek(cls.demofile, 0, FILESYSTEM_SEEK_HEAD);

	FS_Read(&demoheader, sizeof(demoheader), 1, fp);

	if (Q_strncmp(demoheader.szFileStamp, "HLDEMO", 6))
	{
		Con_Printf(const_cast<char*>("%s is not a demo file\n"), name);
		FS_Close(cls.demofile);
		FS_Close(fp);

		unlink(szTempName);
		cls.demofile = NULL;

		return;
	}

	if (demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL)
	{
		Con_Printf(const_cast<char*>("ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n"),
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL);
		FS_Close(cls.demofile);
		FS_Close(fp);
		unlink(szTempName);
		cls.demofile = 0;
		return;
	}

	FS_Seek(fp, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&demodir.nEntries, sizeof(demodir.nEntries), 1, fp);

	if (demodir.nEntries < 1 || demodir.nEntries > DEMO_MAX_DIR_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: demo had bogus # of directory entries:  %i\n"), demodir.nEntries);
		FS_Close(cls.demofile);
		FS_Close(fp);
		unlink(szTempName);
		cls.demofile = NULL;
		return;
	}

	if (nSegment1 >= demodir.nEntries || nSegment2 >= demodir.nEntries)
	{
		Con_Printf(const_cast<char*>("ERROR: demo only has %d entries\n"), demodir.nEntries);
		FS_Close(cls.demofile);
		FS_Close(fp);
		unlink(szTempName);
		cls.demofile = NULL;
		return;
	}

	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc(sizeof(demoentry_t) * demodir.nEntries);
	FS_Read(demodir.p_rgEntries, sizeof(demoentry_t) * demodir.nEntries, 1, fp);

	oldentry = &demodir.p_rgEntries[nSegment1];
	newentry = &demodir.p_rgEntries[nSegment2];

	temp = *oldentry;
	*oldentry = *newentry;
	*newentry = temp;

	FS_Seek(cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Write(&demodir.nEntries, sizeof(demodir.nEntries), 1, cls.demofile);

	for (i = 0; i < demodir.nEntries; i++)
		FS_Write(&demodir.p_rgEntries[i], sizeof(demoentry_t), 1, cls.demofile);

	FS_Close(cls.demofile);
	cls.demofile = NULL;

	FS_Close(fp);
	Con_Printf(const_cast<char*>("Replacing old demo with edited version.\n"));

	unlink(szOriginalName);
	rename(szTempName, szOriginalName);

	Mem_Free(demodir.p_rgEntries);
	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;
}

void CL_SetDemoInfo_f()
{
	char* pszPath, name[MAX_PATH], szOriginalName[256], szTempName[MAX_PATH], *pszArg;
	int nSegment, n, iArg;
	FileHandle_t file;
	demoentry_t* pseg;

	if (cmd_source != src_command)
		return;

	if (cls.demorecording || cls.demoplayback)
		return Con_Printf(const_cast<char*>("Setdemoinfo only available when not running or recording a demo.\n"));

	if (Cmd_Argc() <= 2)
	{
		Con_Printf(const_cast<char*>("Setdemoinfo <demoname> <seg#> <info ...>\n"));
		Con_Printf(const_cast<char*>("   title \"text\"\n"));
		Con_Printf(const_cast<char*>("   play tracknum\n"));
		Con_Printf(const_cast<char*>("   fade <in | out> <fast | slow>\n\n"));
		Con_Printf(const_cast<char*>("Use -option to disable, e.g., -title\n"));
		return;
	}

	pszPath = (PCHAR)Cmd_Argv(1);

	if (strstr(pszPath, ".."))
		return Con_Printf(const_cast<char*>("Relative pathnames are not allowed.\n"));

	nSegment = Q_atoi(Cmd_Argv(2));

	if (nSegment <= 1)
		return Con_Printf(const_cast<char*>("Cannot Setdemoinfo the STARTUP segment.\n"));

	_snprintf(name, sizeof(name), "%s", Cmd_Argv(1));
	COM_DefaultExtension(name, const_cast<char*>(".dem"));

	if (!COM_HasFileExtension(name, const_cast<char*>("dem")))
		return Con_Printf(const_cast<char*>("Error: couldn't open file %s, demo files must have .dem extension.\n"), name);

	Con_Printf(const_cast<char*>("Setting info for segment %i in demo %s.\n"), nSegment, name);
	Q_strncpy(szOriginalName, name, sizeof(szOriginalName) - 1);
	szOriginalName[sizeof(szOriginalName) - 1] = 0;

	nSegment--;

	file = FS_Open(szOriginalName, "rb");
	if (!file)
		return Con_Printf(const_cast<char*>("Error: couldn't open demo file %s for Setdemoinfo.\n"), name);

	COM_StripExtension(name, szTempName);
	COM_AddExtension(szTempName, const_cast<char*>(".dm2"), sizeof(szTempName));

	if (!COM_HasFileExtension(szTempName, const_cast<char*>("dm2")))
	{
		Con_Printf(const_cast<char*>("Error: couldn't open temporary file %s.\n"), szTempName);
		FS_Close(file);
	}

	cls.demofile = FS_OpenPathID(szTempName, "w+b", "GAMECONFIG");
	if (!cls.demofile)
	{
		Con_Printf(const_cast<char*>("Error: couldn't open %s.\n"), name);
		FS_Close(file);
		return;
	}

	FS_Seek(file, 0, FILESYSTEM_SEEK_TAIL);
	n = FS_Tell(file);
	FS_Seek(file, 0, FILESYSTEM_SEEK_HEAD);
	FS_Seek(cls.demofile, 0, FILESYSTEM_SEEK_HEAD);
	CL_DemoCopyFileChunk((FILE*)cls.demofile, (FILE*)file, n);
	FS_Seek(file, 0, FILESYSTEM_SEEK_HEAD);
	FS_Seek(cls.demofile, 0, FILESYSTEM_SEEK_HEAD);
	FS_Read(&demoheader, sizeof(demoheader_t), 1, file);

	if (Q_strncmp(demoheader.szFileStamp, "HLDEMO", 6))
	{
		Con_Printf(const_cast<char*>("%s is not a demo file\n"), name);
		FS_Close(cls.demofile);
		FS_Close(file);
		unlink(szTempName);
		cls.demofile = 0;
		return;
	}

	if (demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL)
	{
		Con_Printf(const_cast<char*>("ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n"),
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL);

		FS_Close(cls.demofile);
		FS_Close(file);
		unlink(szTempName);
		cls.demofile = NULL;
		return;
	}

	FS_Seek(file, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&demodir.nEntries, sizeof(demodir.nEntries), 1, file);

	if (demodir.nEntries < 1 || demodir.nEntries > DEMO_MAX_DIR_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: demo had bogus # of directory entries:  %i\n"), demodir.nEntries);
		FS_Close(cls.demofile);
		FS_Close(file);
		unlink(szTempName);
		cls.demofile = NULL;
		return;
	}

	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc(sizeof(demoentry_t) * demodir.nEntries);
	FS_Read(demodir.p_rgEntries, sizeof(demoentry_t) * demodir.nEntries, 1, file);

	iArg = 3;
	pseg = &demodir.p_rgEntries[nSegment];
	while (true)
	{
		pszArg = (char*)Cmd_Argv(iArg);

		if (!pszArg || !pszArg[0])
			break;

		iArg++;

		if (!Q_strcasecmp(pszArg, "-TITLE"))
		{
			pseg->nFlags &= ~FDEMO_TITLE;
			continue;
		}

		if (!Q_strcasecmp(pszArg, "-PLAY"))
		{
			pseg->nFlags &= ~FDEMO_CDA;
			continue;
		}

		if (!Q_strcasecmp(pszArg, "-FADE"))
		{
			pseg->nFlags &= ~(FDEMO_FADEIN_SLOW | FDEMO_FADEIN_FAST | FDEMO_FADEOUT_SLOW | FDEMO_FADEOUT_FAST);
			continue;
		}

		if (!Q_strcasecmp(pszArg, "TITLE"))
		{
			char* pszTitle;
			pszTitle = (char*)Cmd_Argv(iArg++);
			if (pszTitle && pszTitle[0])
			{
				Q_strncpy(pseg->szDescription, pszTitle, sizeof(pseg->szDescription));
				pseg->szDescription[sizeof(pseg->szDescription) - 1] = 0;
				pseg->nFlags |= FDEMO_TITLE;
			}
			else
				Con_Printf(const_cast<char*>("Title flag requires a double-quoted value.\n"));

			continue;
		}

		if (!Q_strcasecmp(pszArg, "PLAY"))
		{
			char* pszTracknum = (char*)Cmd_Argv(iArg++);
			if (pszTracknum && pszTracknum[0])
			{
				pseg->nFlags |= FDEMO_CDA;
				pseg->nCDTrack = Q_atoi(pszTracknum);
			}
			else
				Con_Printf(const_cast<char*>("Play flag requires a cd track #.\n"));

			continue;
		}

		if (!Q_strcasecmp(pszArg, "FADE"))
		{
			char* pszDirection = (char*)Cmd_Argv(iArg++);
			if (pszDirection && pszDirection[0])
			{
				if (Q_strcasecmp(pszDirection, "IN"))
				{
					if (Q_strcasecmp(pszDirection, "OUT"))
						Con_Printf(const_cast<char*>("Fade flag requires a direction (in or out).\n"));
					else
					{
						char* pszSpeed = (char*)Cmd_Argv(iArg++);
						if (pszSpeed && pszSpeed[0])
						{
							if (Q_strcasecmp(pszSpeed, "FAST"))
							{
								if (Q_strcasecmp(pszSpeed, "SLOW"))
									Con_Printf(const_cast<char*>("Fade flag requires a speed (fast or slow).\n"));
								else
									pseg->nFlags |= FDEMO_FADEOUT_SLOW;
							}
							else
								pseg->nFlags |= FDEMO_FADEOUT_FAST;
						}
						else
							Con_Printf(const_cast<char*>("Fade flag requires a speed (fast or slow).\n"));
					}
				}
				else
				{
					char* pszSpeed = (char*)Cmd_Argv(iArg++);
					if (pszSpeed && pszSpeed[0])
					{
						if (Q_strcasecmp(pszSpeed, "FAST"))
						{
							if (Q_strcasecmp(pszSpeed, "SLOW"))
								Con_Printf(const_cast<char*>("Fade flag requires a speed (fast or slow).\n"));
							else
								pseg->nFlags |= FDEMO_FADEIN_SLOW;
						}
						else
							pseg->nFlags |= FDEMO_FADEIN_FAST;
					}
					else
						Con_Printf(const_cast<char*>("Fade flag requires a speed (fast or slow).\n"));
				}
			}
			else
				Con_Printf(const_cast<char*>("Fade flag requires a direction and speed (in fast, e.g.)\n"));

			continue;
		}

		// больше нечего парсить
		Con_Printf(const_cast<char*>("Setdemoinfo, unrecognized flag:  %s\n"), pszArg);
		break;
	}

	FS_Seek(cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Write(&demodir.nEntries, sizeof(demodir.nEntries), 1, cls.demofile);

	for (int i = 0; i < demodir.nEntries; i++)
		FS_Write(&demodir.p_rgEntries[i], sizeof(demoentry_t), 1, cls.demofile);

	FS_Close(cls.demofile);
	cls.demofile = NULL;
	FS_Close(file);
	Con_Printf(const_cast<char*>("Replacing old demo with edited version.\n"));
	unlink(szOriginalName);
	rename(szTempName, szOriginalName);
	Mem_Free(demodir.p_rgEntries);
	demodir.p_rgEntries = 0;
	demodir.nEntries = NULL;
}

void CL_RemoveDemo_f()
{
	char name[MAX_PATH], szTempName[MAX_PATH], szOriginalName[MAX_PATH];
	demoentry_t temp;
	int nSegmentToRemove, nSize;
	FileHandle_t file;
	demodirectory_t newdir;

	if (cmd_source != src_command)
		return;

	if (cls.demorecording || cls.demoplayback)
		return Con_Printf(const_cast<char*>("Removedemo only available when not running or recording a demo.\n"));

	if (Cmd_Argc() != 3)
		return Con_Printf(const_cast<char*>("removedemo <demoname> <segment to remove>\nSegment 1 cannot be removed\n"));

	if (strstr(Cmd_Argv(1), ".."))
		return Con_Printf(const_cast<char*>("Relative pathnames are not allowed.\n"));

	nSegmentToRemove = Q_atoi(Cmd_Argv(2));
	if (nSegmentToRemove <= 1)
		return Con_Printf(const_cast<char*>("Cannot swap the STARTUP segment.\n"));

	snprintf(name, MAX_PATH, "%s", Cmd_Argv(1));
	COM_DefaultExtension(name, const_cast<char*>(".dem"));

	if (!COM_HasFileExtension(name, const_cast<char*>("dem")))
		return Con_Printf(const_cast<char*>("Error: couldn't open %s, demo files must have .dem extension.\n"), name);

	Con_Printf(const_cast<char*>("Removing segment %i from demo %s.\n"), nSegmentToRemove, name);
	Q_strncpy(szOriginalName, name, 255);
	szOriginalName[255] = 0;
	file = FS_Open(szOriginalName, "rb");

	nSegmentToRemove--;

	if (!file)
		return Con_Printf(const_cast<char*>("Error: couldn't open demo file %s for segment removal.\n"), name);

	COM_StripExtension(name, szTempName);
	COM_AddExtension(szTempName, const_cast<char*>(".dm2"), MAX_PATH);

	if (!COM_HasFileExtension(szTempName, const_cast<char*>("dm2")))
	{
		Con_Printf(const_cast<char*>("Error: couldn't open temporary file %s.\n"), szTempName);
		return FS_Close(file);
	}

	cls.demofile = FS_OpenPathID(szTempName, "w+b", "GAMECONFIG");
	if (!cls.demofile)
	{
		Con_Printf(const_cast<char*>("Error: couldn't open %s.\n"), szTempName);
		return FS_Close(file);
	}

	FS_Seek(file, 0, FILESYSTEM_SEEK_HEAD);
	FS_Seek(cls.demofile, 0, FILESYSTEM_SEEK_HEAD);
	FS_Read(&demoheader, sizeof(demoheader), 1, file);
	FS_Write(&demoheader, sizeof(demoheader), 1, cls.demofile);

	if (Q_strncmp(demoheader.szFileStamp, "HLDEMO", 6))
	{
		Con_Printf(const_cast<char*>("%s is not a demo file\n"), name);
		FS_Close(cls.demofile);
		FS_Close(file);
		cls.demofile = 0;
		return;
	}

	if (demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL)
	{
		Con_Printf(const_cast<char*>("ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n"),
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL);
		FS_Close(cls.demofile);
		FS_Close(file);
		cls.demofile = 0;
		return;
	}

	FS_Seek(file, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&demodir.nEntries, sizeof(demodir.nEntries), 1, file);

	if (demodir.nEntries < 1 || demodir.nEntries > DEMO_MAX_DIR_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: demo had bogus # of directory entries:  %i\n"), demodir.nEntries);
		FS_Close(cls.demofile);
		FS_Close(file);
		cls.demofile = NULL;
		return;
	}

	if (demodir.nEntries == 1)
	{
		Con_Printf(const_cast<char*>("ERROR: Can't remove last directory entry.\n"));
		FS_Close(cls.demofile);
		FS_Close(file);
		cls.demofile = NULL;
		return;
	}

	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc(sizeof(demoentry_t) * demodir.nEntries);
	FS_Read(demodir.p_rgEntries, sizeof(demoentry_t) * demodir.nEntries, 1, file);

	newdir.nEntries = demodir.nEntries - 1;
	newdir.p_rgEntries = (demoentry_t*)Mem_Malloc(sizeof(demoentry_t) * newdir.nEntries);

	for (int i = 0, j = 0; i < demodir.nEntries; i++)
	{
		if (i != nSegmentToRemove)
		{
			newdir.p_rgEntries[j] = demodir.p_rgEntries[i];
			newdir.p_rgEntries[j].nOffset = FS_Tell(cls.demofile);
			FS_Seek(file, demodir.p_rgEntries[j].nOffset, FILESYSTEM_SEEK_HEAD);
			CL_DemoCopyFileChunk((FILE*)cls.demofile, (FILE*)file, newdir.p_rgEntries[j].nFileLength);
			j++;
		}
	}

	demoheader.nDirectoryOffset = FS_Tell(cls.demofile);
	FS_Write(&newdir.nEntries, sizeof(newdir.nEntries), 1, cls.demofile);

	for (int i = 0; i < newdir.nEntries; i++)
		FS_Write(&newdir.p_rgEntries[i], sizeof(demoentry_t), 1, cls.demofile);

	FS_Seek(cls.demofile, 0, FILESYSTEM_SEEK_HEAD);
	FS_Write(&demoheader, sizeof(demoheader), 1, cls.demofile);
	FS_Close(cls.demofile);
	cls.demofile = NULL;
	FS_Close(file);
	Con_Printf(const_cast<char*>("Replacing old demo with edited version.\n"));
	unlink(szOriginalName);
	rename(szTempName, szOriginalName);
	Mem_Free(newdir.p_rgEntries);
	Mem_Free(demodir.p_rgEntries);
	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;
}

void CL_ListDemo_f()
{
	FileHandle_t file;
	demoheader_t header;
	char name[MAX_PATH];
	char type[32];
	demodirectory_t directory;

	if (cmd_source != src_command)
		return;
	if (cls.demorecording || cls.demoplayback)
		return Con_Printf(const_cast<char*>("Listdemo only available when not running or recording.\n"));

	snprintf(name, MAX_PATH, "%s", Cmd_Argv(1));
	COM_DefaultExtension(name, const_cast<char*>(".dem"));
	Con_Printf(const_cast<char*>("Demo contents for %s.\n"), name);
	cls.demofile = FS_Open(name, "rb");

	if (!cls.demofile)
		return Con_Printf(const_cast<char*>("ERROR: couldn't open.\n"));

	FS_Read(&header, sizeof(header), 1, cls.demofile);
	if (Q_strncmp(header.szFileStamp, "HLDEMO", 6))
	{
		Con_Printf(const_cast<char*>("%s is not a demo file\n"), name);
		FS_Close(cls.demofile);
		cls.demofile = NULL;
		return;
	}

	Con_Printf(const_cast<char*>("Demo file protocols %iN:%iD\nServer protocols at %iN:%iD\n"),
		header.nNetProtocolVersion, header.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL);

	Con_Printf(const_cast<char*>("Map name :  %s\n"), header.szMapName);
	Con_Printf(const_cast<char*>("Game .dll:  %s\n"), header.szDllDir);
	FS_Seek(cls.demofile, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&directory.nEntries, sizeof(directory.nEntries), 1, cls.demofile);

	if (directory.nEntries < 1 || directory.nEntries > DEMO_MAX_DIR_ENTRIES)
	{
		Con_Printf(const_cast<char*>("ERROR: demo had bogus # of directory entries:  %i\n"), directory.nEntries);
		FS_Close(cls.demofile);
		cls.demofile = NULL;
		return;
	}

	directory.p_rgEntries = (demoentry_t*)Mem_Malloc(sizeof(demoentry_t) * directory.nEntries);
	FS_Read(directory.p_rgEntries, sizeof(demoentry_t) * directory.nEntries, 1, cls.demofile);
	Con_Printf(const_cast<char*>("\n"));

	for (int i = 0; i < directory.nEntries; i++)
	{
		if (directory.p_rgEntries[i].nEntryType)
			snprintf(type, sizeof(type), "Normal segment");
		else
			snprintf(type, sizeof(type), "Start segment");

		Con_Printf(const_cast<char*>("%i:  %s\n  \"%20s\"\nTime:  %.2f s.\nFrames:  %i\nSize:  %.2fK\n"), i + 1, directory.p_rgEntries[i].nEntryType, directory.p_rgEntries[i].szDescription,
			directory.p_rgEntries[i].fTrackTime, directory.p_rgEntries[i].nFrames, (float)directory.p_rgEntries[i].nFileLength * 1024.0f);

		if (directory.p_rgEntries[i].nFlags != 0)
			Con_Printf(const_cast<char*>("  Flags:\n"));

		if (directory.p_rgEntries[i].nFlags & FDEMO_TITLE)
			Con_Printf(const_cast<char*>("     Title \"%s\"\n"), directory.p_rgEntries[i].szDescription);;

		if (directory.p_rgEntries[i].nFlags & FDEMO_CDA)
			Con_Printf(const_cast<char*>("     Play %i\n"), directory.p_rgEntries[i].nCDTrack);

		if (directory.p_rgEntries[i].nFlags & FDEMO_FADEIN_SLOW)
			Con_Printf(const_cast<char*>("     Fade in slow\n"));

		if (directory.p_rgEntries[i].nFlags & FDEMO_FADEIN_FAST)
			Con_Printf(const_cast<char*>("     Fade in fast\n"));

		if (directory.p_rgEntries[i].nFlags & FDEMO_FADEOUT_SLOW)
			Con_Printf(const_cast<char*>("     Fade out slow\n"));

		if (directory.p_rgEntries[i].nFlags & FDEMO_FADEOUT_FAST)
			Con_Printf(const_cast<char*>("     Fade out fast\n"));

		Con_Printf(const_cast<char*>("\n"));
	}

	FS_Close(cls.demofile);
	cls.demofile = NULL;
	Mem_Free(directory.p_rgEntries);
}

void CL_DemoSectionStart()
{
	char szCommand[128];

	if (pCurrentEntry == NULL)
		return;

	if (pCurrentEntry->nFlags & FDEMO_CDA)
	{
		snprintf(szCommand, sizeof(szCommand), "cd stop\ncd play %i\n", pCurrentEntry->nCDTrack);
		Cbuf_AddText(szCommand);
	}

	if (pCurrentEntry->nFlags & FDEMO_FADEIN_FAST)
	{
		sf_demo.duration = 2048;
		sf_demo.holdTime = 0;
		sf_demo.r = sf_demo.g = sf_demo.b = 0;
		sf_demo.a = 255;
		sf_demo.fadeFlags = 0;
		V_ScreenFade(0, 0, &sf_demo);
	}

	if (pCurrentEntry->nFlags & FDEMO_FADEIN_SLOW)
	{
		sf_demo.duration = 8192;
		sf_demo.holdTime = 0;
		sf_demo.fadeFlags = 0;
		sf_demo.r = sf_demo.g = sf_demo.b = 0;
		sf_demo.a = 255;
		V_ScreenFade(0, 0, &sf_demo);
	}
}

qboolean CL_SwitchToNextDemoSection()
{
	if (nCurEntryIndex > 0 && pCurrentEntry && pCurrentEntry->nFlags & FDEMO_CDA)
		Cbuf_AddText(const_cast<char*>("cd stop\n"));

	nCurEntryIndex++;

	if (nCurEntryIndex >= demodir.nEntries)
	{
		Host_EndGame(const_cast<char*>("End of demo.\n"));
		return false;
	}

	pCurrentEntry = &demodir.p_rgEntries[nCurEntryIndex];
	cls.forcetrack = pCurrentEntry->nCDTrack;

	FS_Seek(cls.demofile, pCurrentEntry->nOffset, FILESYSTEM_SEEK_HEAD);

	cls.demostartframe = host_framecount;
	cls.demoframecount = 0;
	cls.demostarttime = CL_DemoInTime();

	return true;
}

void CL_DemoWriteSequenceInfo(FileHandle_t fp, int length)
{
	sequence_info_t si;

	si.incoming_sequence = cls.netchan.incoming_sequence;
	si.incoming_acknowledged = cls.netchan.incoming_acknowledged;
	si.incoming_reliable_acknowledged = cls.netchan.incoming_reliable_acknowledged;
	si.incoming_reliable_sequence = cls.netchan.incoming_reliable_sequence;
	si.outgoing_sequence = cls.netchan.outgoing_sequence;
	si.reliable_sequence = cls.netchan.reliable_sequence;
	si.last_reliable_sequence = cls.netchan.last_reliable_sequence;
	si.length = LittleLong(length);
	FS_Write(&si, sizeof(si), 1, fp);
}

void CL_ReadNetchanState_OLD()
{
	FS_Read(&cls.netchan.incoming_sequence, sizeof(int), 1, cls.demofile);
	FS_Read(&cls.netchan.incoming_acknowledged, sizeof(int), 1, cls.demofile);
	FS_Read(&cls.netchan.incoming_reliable_acknowledged, sizeof(int), 1, cls.demofile);
	FS_Read(&cls.netchan.incoming_reliable_sequence, sizeof(int), 1, cls.demofile);
	FS_Read(&cls.netchan.outgoing_sequence, sizeof(int), 1, cls.demofile);
	FS_Read(&cls.netchan.reliable_sequence, sizeof(int), 1, cls.demofile);
	FS_Read(&cls.netchan.last_reliable_sequence, sizeof(int), 1, cls.demofile);
}

void CL_ReadNetchanState()
{
	if (DemoPlayer_IsActive())
		DemoPlayer_ReadNetchanState(
			&cls.netchan.incoming_sequence,
			&cls.netchan.incoming_acknowledged,
			&cls.netchan.incoming_reliable_acknowledged,
			&cls.netchan.incoming_reliable_sequence,
			&cls.netchan.outgoing_sequence,
			&cls.netchan.reliable_sequence,
			&cls.netchan.last_reliable_sequence);
	else
		CL_ReadNetchanState_OLD();
}

void CL_DemoWriteCmdInfo(FileHandle_t fp)
{
	FS_Write(&g_rp, sizeof(g_rp), 1, fp);
}

void CL_DemoFrameInfo(demo_info_t* rp, float time)
{
	for (int i = MAX_DEMO_FRAMES - 1; i > 0; i--)
	{
		Q_memcpy(&dem_frames[i], &dem_frames[i - 1], sizeof(demo_info_t));
	}

	dem_frames[0] = *rp;
	CL_FixPointers(&dem_frames[0]);
	dem_frames[0].timestamp = time;
}

void CL_DemoReadCmdInfo(float time)
{
	demo_info_t rp;

	FS_Read(&rp, sizeof(rp), 1, cls.demofile);

	g_rp.cmd = rp.cmd;
	g_rp.movevars = rp.movevars;
	VectorCopy(rp.view, g_rp.view);
	g_rp.viewmodel = rp.viewmodel;
	CL_DemoFrameInfo(&rp, time);
}

void CL_RecordHUDCommand(const char* pszCmd)
{
	hud_command_t hc;
	hc.cmd = clc_stringcmd;
	hc.time = LittleFloat(CL_DemoOutTime() - cls.demostarttime);
	hc.frame = LittleLong(host_framecount - cls.demostartframe);
	Q_memset(hc.szNameBuf, 0, sizeof(hc.szNameBuf));
	Q_strncpy(hc.szNameBuf, (char*)pszCmd, sizeof(hc.szNameBuf) - 1);
	hc.szNameBuf[sizeof(hc.szNameBuf) - 1] = 0;
	FS_Write(&hc, sizeof(hc.szNameBuf), 1, cls.demofile);
}

void CL_WriteDLLUpdate(client_data_t* cdata)
{
	cl_demo_data_t demcmd;

	if (cls.demofile == NULL)
		return;

	demcmd.cmd = dem_angles;
	demcmd.time = LittleFloat(CL_DemoOutTime() - cls.demostarttime);
	demcmd.frame = LittleLong(host_framecount - cls.demostartframe);
	VectorCopy(cdata->origin, demcmd.origin);
	VectorCopy(cdata->viewangles, demcmd.viewangles);
	demcmd.iWeaponBits = cdata->iWeaponBits;
	demcmd.fov = cdata->fov;
	FS_Write(&demcmd, sizeof(demcmd), 1, cls.demofile);
}

void CL_DemoAnim(int anim, int body)
{
	demo_anim_t demcmd;

	if (cls.demofile == NULL)
		return;

	demcmd.cmd = dem_wpnanim;
	demcmd.time = LittleFloat(CL_DemoOutTime() - cls.demostarttime);
	demcmd.frame = LittleLong(host_framecount - cls.demostartframe);
	demcmd.anim = LittleLong(anim);
	demcmd.body = LittleLong(body);
	FS_Write(&demcmd, sizeof(demcmd), 1, cls.demofile);
}

void CL_DemoQueueSound(int channel, const char* sample, float volume, float attenuation, int flags, int pitch)
{
	demo_sound1_t demcmd1;
	demo_sound2_t demcmd2;

	if (cls.demofile == NULL)
		return;

	demcmd1.cmd = dem_playsound;
	demcmd1.time = LittleFloat(CL_DemoOutTime() - cls.demostarttime);
	demcmd1.frame = LittleLong(host_framecount - cls.demostartframe);
	demcmd1.channel = LittleLong(channel);
	demcmd1.length = LittleLong(Q_strlen(sample));

	FS_Write(&demcmd1, sizeof(demo_sound1_t), 1, cls.demofile);
	FS_Write(sample, Q_strlen(sample), 1, cls.demofile);

	demcmd2.volume = LittleFloat(volume);
	demcmd2.attenuation = LittleFloat(attenuation);
	demcmd2.flags = LittleLong(flags);
	demcmd2.pitch = LittleLong(pitch);

	FS_Write(&demcmd2, sizeof(demo_sound2_t), 1, cls.demofile);
}

void CL_DemoPlaySound(int channel, char* sample, float attenuation, float volume, int flags, int pitch)
{
	sfx_t sfxsentence, * sfx;

	if (flags & SND_SENTENCE)
	{
		_snprintf(sfxsentence.name, sizeof(sfxsentence.name), "!%s", rgpszrawsentence[Q_atoi(&sample[1])]);
		sfx = &sfxsentence;
	}
	else
	{
		sfx = CL_LookupSound(sample);
	}

	if (channel == CHAN_STATIC)
		S_StartStaticSound(cl.playernum + 1, CHAN_STATIC, sfx, cl.simorg, volume, attenuation, flags, pitch);
	else
		S_StartDynamicSound(cl.playernum + 1, channel, sfx, cl.simorg, volume, attenuation, flags, pitch);
}

void CL_DemoParseSound()
{
	char sample[256];
	int channel, i, flags, pitch;
	float attenuation, volume;

	FS_Read(&channel, sizeof(channel), 1, cls.demofile);
	channel = LittleLong(channel);

	FS_Read(&i, sizeof(i), 1, cls.demofile);
	i = min(i, (int)sizeof(sample) - 1);

	FS_Read(sample, i, 1, cls.demofile);
	sample[i] = 0;

	FS_Read(&attenuation, sizeof(attenuation), 1, cls.demofile);
	attenuation = LittleFloat(attenuation);

	FS_Read(&volume, sizeof(volume), 1, cls.demofile);
	volume = LittleFloat(volume);

	FS_Read(&flags, sizeof(flags), 1, cls.demofile);
	flags = LittleLong(flags);

	FS_Read(&pitch, sizeof(pitch), 1, cls.demofile);
	pitch = LittleLong(pitch);

	CL_DemoPlaySound(channel, sample, attenuation, volume, flags, pitch);
}

void CL_DemoEvent(int flags, int idx, float delay, event_args_t* pargs)
{
	demo_event_t demcmd;

	if (cls.demofile == NULL)
		return;

	demcmd.cmd = dem_event;
	demcmd.time = LittleFloat(CL_DemoOutTime() - cls.demostarttime);
	demcmd.frame = LittleLong(host_framecount - cls.demostartframe);
	demcmd.flags = LittleLong(flags);
	demcmd.idx = LittleLong(idx);
	demcmd.delay = LittleFloat(delay);
	FS_Write(&demcmd, sizeof(demcmd), 1, cls.demofile);
	FS_Write(pargs, sizeof(event_args_t), 1, cls.demofile);
}

void CL_DemoParseEvent()
{
	event_args_t eargs;
	int flags, i;
	float delay;

	FS_Read(&flags, sizeof(flags), 1, cls.demofile);
	flags = LittleLong(flags);

	FS_Read(&i, sizeof(i), 1, cls.demofile);
	i = LittleLong(i);

	FS_Read(&delay, sizeof(delay), 1, cls.demofile);
	delay = LittleFloat(delay);

	FS_Read(&eargs, sizeof(eargs), 1, cls.demofile);
	CL_QueueEvent(flags, i, delay, &eargs);
}

void CL_DemoParseAnim()
{
	int i, body;

	FS_Read(&i, sizeof(i), 1, cls.demofile);
	i = LittleLong(i);

	FS_Read(&body, sizeof(body), 1, cls.demofile);
	body = LittleLong(body);

	hudWeaponAnim(i, body);
}

void CL_BeginDemoStartup()
{
	FileHandle_t fh;

	if (cls.demoheader)
		FS_Close(cls.demoheader);

	fh = FS_OpenPathID("demoheader.dmf", "wb", "GAMECONFIG");

	cls.demoheader = fh;

	if (fh)
		Con_DPrintf(const_cast<char*>("Spooling demo header.\n"));
	else
		Con_DPrintf(const_cast<char*>("ERROR: couldn't open temporary header file.\n"));
}

void CL_WriteDemoStartup(int start, sizebuf_t* msg)
{
	int len;
	demo_command_t demcmd;

	if (cls.demoheader == NULL)
		return;

	len = msg->cursize - start;
	if (len <= 0)
		return;

	demcmd.cmd = DEMO_STARTUP;
	demcmd.time = LittleFloat(CL_DemoOutTime() - cls.demostarttime);
	demcmd.frame = LittleLong(host_framecount - cls.demostartframe);

	FS_Write(&demcmd, sizeof(demcmd), 1, cls.demoheader);

	CL_DemoWriteCmdInfo(cls.demoheader);
	CL_DemoWriteSequenceInfo(cls.demoheader, len);

	FS_Write(&msg->data[start], len, 1, cls.demoheader);
}

void CL_StopPlayback()
{
	if (cls.demoplayback)
	{
		if (DemoPlayer_IsActive())
			DemoPlayer_Stop();
		else
			FS_Close(cls.demofile);

		cls.demoplayback = 0;
		cls.demofile = 0;
		cls.state = cactive_t::ca_disconnected;
		cls.demoframecount = 0;

		if (demodir.p_rgEntries != NULL)
			Mem_Free(demodir.p_rgEntries);

		demodir.p_rgEntries = NULL;
		demodir.nEntries = 0;

		if (cl_gamegauge.value != 0)
			CL_FinishGameGauge();

		if (cls.timedemo)
			CL_FinishTimeDemo();

		Cbuf_AddText(const_cast<char*>("cd stop\n"));
	}
}

void CL_WriteDemoMessage(int start, sizebuf_t* msg)
{
	int len;
	demo_command_t demcmd;

	if (!cls.demorecording || cls.demofile == NULL)
		return;

	len = msg->cursize - start;

	if (len <= 0)
		return;

	cls.demoframecount++;

	demcmd.cmd = DEMO_NORMAL;
	demcmd.time = LittleFloat(CL_DemoOutTime() - cls.demostarttime);
	demcmd.frame = LittleLong(host_framecount - cls.demostartframe);

	FS_Write(&demcmd, sizeof(demcmd), 1, cls.demofile);

	CL_DemoWriteCmdInfo(cls.demoheader);
	CL_DemoWriteSequenceInfo(cls.demoheader, len);

	FS_Write(&msg->data[start], len, 1, cls.demofile);
}

void CL_Stop_f()
{
	if (cmd_source != cmd_source_t::src_command)
		return;

	if (!cls.demorecording)
	{
		Con_Printf(const_cast<char*>("Not recording a demo.\n"));
		return;
	}

	byte c = DEMO_PROTOCOL;
	FS_Write(&c, sizeof(c), 1, cls.demofile);

	float target = CL_DemoOutTime();

	float pInput = target - cls.demostarttime;
	float f = LittleFloat(pInput);
	FS_Write(&f, sizeof(float), 1, cls.demofile);

	int swlen = LittleLong(host_framecount - cls.demostartframe);
	FS_Write(&swlen, sizeof(swlen), 1, cls.demofile);

	int pos = FS_Tell(cls.demofile);
	pCurrentEntry->nFileLength = pos - pCurrentEntry->nOffset;
	pCurrentEntry->fTrackTime = target - cls.demostarttime;
	pCurrentEntry->nFrames = cls.demoframecount;
	FS_Write(&demodir.nEntries, sizeof(demodir.nEntries), 1, cls.demofile);

	for (int i = 0; i < demodir.nEntries; i++)
		FS_Write(&demodir.p_rgEntries[i], sizeof(demoentry_t), 1, cls.demofile);

	if (demodir.p_rgEntries)
		Mem_Free(demodir.p_rgEntries);

	demodir.p_rgEntries = NULL;
	demodir.nEntries = NULL;

	demoheader.nDirectoryOffset = pos;

	FS_Seek(cls.demofile, 0, FILESYSTEM_SEEK_HEAD);
	FS_Write(&demoheader, sizeof(demoheader_t), 1, cls.demofile);
	FS_Close(cls.demofile);

	cls.demofile = NULL;
	cls.demorecording = false;
	cls.td_lastframe = host_framecount;

	if (cls.demoappending)
	{
		char szNewName[MAX_PATH], szOldName[MAX_PATH];

		Con_Printf(const_cast<char*>("Replacing old demo with appended version.\n"));
		Q_strncpy(szNewName, cls.demofilename, 259);
		szNewName[259] = 0;
		unlink(cls.demofilename);
		COM_StripExtension(cls.demofilename, szOldName);
		COM_DefaultExtension(szOldName, const_cast<char*>(".dm2"));
		rename(szOldName, szNewName);
	}

	Con_Printf(const_cast<char*>("Completed demo\n"));
	Con_DPrintf(const_cast<char*>("Recording time %.2f\nHost frames %i\n"), target - cls.demostarttime, cls.td_lastframe - cls.td_startframe);
}

void CL_Record_f()
{
	int fmt, format;
	FileHandle_t file;
	char pOutput[1024], name[MAX_PATH], szMapName[MAX_PATH], szDll[MAX_PATH];
	float f;
	int swlen, track;
	byte cmd;

	if (Cmd_Argc() <= 1)
		return Con_Printf(const_cast<char*>("record <demoname> <cd>\n"));

	if (cls.demorecording)
		return Con_Printf(const_cast<char*>("Already recording.\n"));

	if (cls.demoplayback)
		return Con_Printf(const_cast<char*>("Can't record during demo playback.\n"));

	if (!cls.demoheader || cls.state != ca_active)
		return Con_Printf(const_cast<char*>("You must be in an active map spawned on a machine that allows creation of files in %s\n"), com_gamedir);

	if (Cmd_Argc() != 2 && Cmd_Argc() != 3)
		return Con_Printf(const_cast<char*>("record <demoname> <cd track>\n"));

	if (strstr(Cmd_Argv(1), ".."))
		return Con_Printf(const_cast<char*>("Relative pathnames are not allowed.\n"));

	track = -1;
	if (Cmd_Argc() == 3)
	{
		track = Q_atoi(Cmd_Argv(2));
		Con_Printf(const_cast<char*>("Forcing CD track to %i\n"), track);
	}

	_snprintf(name, sizeof(name), "%s", Cmd_Argv(1));
	COM_AddExtension(name, const_cast<char*>(".dem"), sizeof(name));

	if (!COM_HasFileExtension(name, const_cast<char*>("dem")))
		return Con_Printf(const_cast<char*>("Error: couldn't open demo file %s\n"), name);

	Con_Printf(const_cast<char*>("recording to %s.\n"), name);
	cls.demofile = FS_OpenPathID(name, "wb", "GAMECONFIG");

	if (!cls.demofile)
		return Con_Printf(const_cast<char*>("Error: couldn't open demo file %s, demo files must have .dem extension.\n"), name);

	Q_memset(&demoheader, 0, sizeof(demoheader));
	Q_strncpy(demoheader.szFileStamp, "HLDEMO", 6);
	COM_FileBase(cl.worldmodel->name, szMapName);

	Q_strncpy(demoheader.szMapName, szMapName, sizeof(szMapName));
	demoheader.szMapName[sizeof(szMapName) - 1] = 0;

	Q_strncpy(szDll, com_gamedir, sizeof(szDll) - 1);
	szDll[sizeof(szDll) - 1] = 0;

	COM_FileBase(szDll, demoheader.szDllDir);

	demoheader.mapCRC = cl.serverCRC;
	demoheader.nDemoProtocol = DEMO_PROTOCOL;
	demoheader.nNetProtocolVersion = PROTOCOL_VERSION;
	demoheader.nDirectoryOffset = 0;

	FS_Write(&demoheader, sizeof(demoheader), 1, cls.demofile);

	Q_memset(&demodir, 0, sizeof(demodir));

	demodir.nEntries = 2;
	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc(2 * sizeof(demoentry_t));

	Q_memset(demodir.p_rgEntries, 0, demodir.nEntries * sizeof(demoentry_t));

	pCurrentEntry = demodir.p_rgEntries;

	Q_strcpy(pCurrentEntry->szDescription, "LOADING");
	pCurrentEntry->nEntryType = 0;
	pCurrentEntry->nFlags = 0;
	pCurrentEntry->nCDTrack = track;
	pCurrentEntry->fTrackTime = 0.0;
	pCurrentEntry->nOffset = FS_Tell(cls.demofile);

	cls.demowaiting = true;
	cls.demorecording = true;

	FS_Tell(cls.demofile);

	byte c = DEMO_PROTOCOL;

	FS_Write(&c, sizeof(c), 1, cls.demoheader);

	float fTime = LittleFloat(CL_DemoOutTime() - cls.demostarttime);
	FS_Write(&fTime, sizeof(fTime), 1, cls.demoheader);

	int iFrames = LittleLong(host_framecount - cls.demostartframe);
	FS_Write(&iFrames, sizeof(iFrames), 1, cls.demoheader);

	FS_Flush(cls.demoheader);

	int nSizeToCopy = FS_Tell(cls.demoheader);

	FS_Seek(cls.demoheader, 0, FILESYSTEM_SEEK_HEAD);

	CL_DemoCopyFileChunk((FILE*)cls.demofile, (FILE*)cls.demoheader, nSizeToCopy);

	FS_Seek(cls.demoheader, nSizeToCopy, FILESYSTEM_SEEK_HEAD);

	cls.demostarttime = CL_DemoOutTime();
	cls.demoframecount = 0;
	cls.demostartframe = host_framecount;

	pCurrentEntry->nFileLength = FS_Tell(cls.demofile) - pCurrentEntry->nOffset;

	pCurrentEntry++;

	_snprintf(pCurrentEntry->szDescription, sizeof(pCurrentEntry->szDescription), "Playback");
	pCurrentEntry->nFlags = 0;
	pCurrentEntry->nCDTrack = track;
	pCurrentEntry->fTrackTime = 0.0;
	pCurrentEntry->nOffset = FS_Tell(cls.demofile);

	c = dem_jumptime;
	FS_Write(&c, sizeof(c), 1, cls.demofile);

	fTime = LittleFloat(CL_DemoOutTime() - cls.demostarttime);
	FS_Write(&fTime, sizeof(fTime), 1, cls.demoheader);

	iFrames = LittleLong(host_framecount - cls.demostartframe);
	FS_Write(&iFrames, sizeof(iFrames), 1, cls.demoheader);

	cls.demoappending = false;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;
}

void CL_PlayDemo_f()
{
	FileHandle_t file;
	char name[256];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2 && Cmd_Argc() != 3)
	{
		Con_Printf(const_cast<char*>("playdemo <demoname> <replayspeed>: plays a demo\n"));
		Con_Printf(const_cast<char*>("replayspeed == 1.0 default\n"));
		return;
	}

	g_demotimescale = 1.0f;

	if (Cmd_Argc() == 3)
	{
		g_demotimescale = Q_atof(Cmd_Argv(2));

		if (g_demotimescale <= 0.001f)
			g_demotimescale = 1.0f;

		Con_Printf(const_cast<char*>("Playdemo:  Time scale %f\n"), g_demotimescale);
	}

	CL_Disconnect();
	DemoPlayer_SetActive(0);

	Q_strncpy(name, Cmd_Argv(1), sizeof(name) - 5);
	COM_DefaultExtension(name, const_cast<char*>(".dem"));
	Con_Printf(const_cast<char*>("Playing demo from %s.\n"), name);

	cls.demofile = FS_Open(name, "rb");

	if (!cls.demofile)
	{
		Con_Printf(const_cast<char*>("ERROR: couldn't open.\n"));
		cls.demonum = -1;
		return;
	}

	FS_Read(&demoheader, sizeof(demoheader), 1, cls.demofile);

	if (Q_strncmp(demoheader.szFileStamp, "HLDEMO", 6))
	{
		Con_Printf(const_cast<char*>("%s is not a demo file\n"), name);
		FS_Close(cls.demofile);
		cls.demofile = 0;
		cls.demonum = -1;
		return;
	}

	if (demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL)
		Con_Printf(const_cast<char*>("WARNING! demo protocol outdated.\nDemo file protocols %i:%i, Engine protocol is at %i:%i\n"),
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL);

	FS_Seek(cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&demodir.nEntries, sizeof(demodir.nEntries), 1, cls.demofile);

	if (demodir.nEntries < 1 || demodir.nEntries > DEMO_MAX_DIR_ENTRIES)
	{
		COM_ExplainDisconnection(true, const_cast<char*>("Error: Corrupt demo file."));
		FS_Close(cls.demofile);
		cls.demofile = 0;
		cls.demonum = -1;
		CL_Disconnect();
		return;
	}

	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc(sizeof(demoentry_t) * demodir.nEntries);
	FS_Read(demodir.p_rgEntries, sizeof(demoentry_t) * demodir.nEntries, 1, cls.demofile);

	// _WIN32 -> buffer for net_from *(long*)adr.ipx = 0;
	netadr_t adr;
#if defined(_WIN32)
	*(int*)adr.ipx = 0;
#endif

	nCurEntryIndex = 0;
	pCurrentEntry = demodir.p_rgEntries;
	FS_Seek(cls.demofile, demodir.p_rgEntries->nOffset, FILESYSTEM_SEEK_HEAD);

	cls.forcetrack = pCurrentEntry->nCDTrack;
	cls.demoplayback = true;
	cls.state = ca_connected;
	cls.passive = false;
	cls.spectator = false;
	cls.demostarttime = CL_DemoInTime();
	cls.demostartframe = host_framecount;

	adr = net_from;

	Netchan_Setup(NS_CLIENT, &cls.netchan, adr, -1, &cls, CL_GetFragmentSize);
	NET_ClearLagData(true, false);

	cls.demoframecount = 0;
	cls.lastoutgoingcommand = -1;
	cls.nextcmdtime = realtime;

	DispatchDirectUserMsg("InitHUD", 0, 0);
}

void CL_ViewDemo_f()
{
	char name[260];

	if (cmd_source != src_command)
		return;

	DemoPlayer_SetActive(1);

	if (!DemoPlayer_IsActive())
		return Con_Printf(const_cast<char*>("viewdemo not available.\n"));

	DemoPlayer_ShowGUI();

	if (Cmd_Argc() != 2)
		return Con_Printf(const_cast<char*>("viewdemo <demoname>: shows a demo\n"));

	CL_Disconnect();

	Q_strncpy(name, Cmd_Argv(1), 255);
	COM_DefaultExtension(name, const_cast<char*>(".dem"));
	Con_Printf(const_cast<char*>("Playing demo from %s.\n"), name);

	if (DemoPlayer_LoadGame(name))
	{
		cls.demoplayback = true;
		cls.state = ca_connected;
		cls.passive = false;
		cls.spectator = false;

		netadr_t adr = net_from;

		Netchan_Setup(NS_CLIENT, &cls.netchan, adr, -1, &cls, CL_GetFragmentSize);
		NET_ClearLagData(true, false);
		
		cls.lastoutgoingcommand = -1;
		cls.nextcmdtime = realtime;
		
		DispatchDirectUserMsg("InitHUD", 0, 0);
	}
	else
	{
		Con_Printf(const_cast<char*>("Viewdemo: couldn't load demo %s.\n"), name);
		cls.demonum = -1;
	}
}

void CL_FinishTimeDemo()
{
	int frames = host_framecount - cls.td_startframe - 1;
	double times = (realtime - cls.td_starttime) != 0 ? (realtime - cls.td_starttime) : 1.0;
	Con_Printf(const_cast<char*>("%i frames %5.3f seconds %5.3f fps\n"), frames, times, (double)frames / times);
}

void CL_TimeDemo_f()
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
		return Con_Printf(const_cast<char*>("timedemo <demoname>: gets demo speeds\n"));

	CL_PlayDemo_f();

	cls.timedemo = true;
	cls.td_starttime = realtime;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;
}

void CL_WavePlayLen_f()
{
	unsigned int len;

	if (cmd_source == src_command)
	{
		if (Cmd_Argc() == 2)
		{
			len = COM_GetApproxWavePlayLength(Cmd_Argv(1));
			if (len)
				Con_Printf(const_cast<char*>("Play time is approximately %dms\n"), len);
			else
				Con_Printf(const_cast<char*>("Unable to read %s, file may be missing or incorrectly formatted.\n"), Cmd_Argv(1));
		}
		else
		{
			Con_Printf(const_cast<char*>("waveplaylen <wave file name>: returns approximate number of milliseconds a wave file will take to play.\n"));
		}
	}
}

void CL_GameGauge_f()
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
		return Con_Printf(const_cast<char*>("gg <demoname> : Game Gauge 99\n"));

	CL_PlayDemo_f();
	
	Cvar_Set(const_cast<char*>("cl_gg"), const_cast<char*>("1"));

	g_iMaxFPS = 0;
	g_iMinFPS = 0x7FFFFFFF;
	g_iSecond = 0;
	g_flTime = 0.0;
	g_iFrames = host_framecount;

	cls.td_starttime = realtime;
	cls.td_startframe = host_framecount;
	cls.timedemo = true;
	cls.td_lastframe = -1;
}

void CL_FinishGameGauge()
{
	Cvar_Set(const_cast<char*>("cl_gg"), const_cast<char*>("0"));

	double time = max(1.0f, (float)realtime - cls.td_starttime);
	Con_Printf(const_cast<char*>("%5.2f Half-Life v%s\n"), (host_framecount - cls.td_startframe - 1) / time, gpszVersionString);
	Con_Printf(const_cast<char*>("%d Min\n"), g_iMinFPS);
	Con_Printf(const_cast<char*>("%d Max\n"), g_iMaxFPS);

	for (int i = 0; i < g_iSecond; i++)
		Con_Printf(const_cast<char*>("%d Second %d\n"), g_rgFPS[i], i + 1);

	FileHandle_t f = FS_OpenPathID("fps.txt", "w", "GAMECONFIG");
	FS_FPrintf(f, "%5.2f Half-Life v%s\n", (host_framecount - cls.td_startframe - 1) / time, gpszVersionString);
	FS_FPrintf(f, "%d Min\n", g_iMinFPS);
	FS_FPrintf(f, "%d Max\n", g_iMaxFPS);

	for (int i = 0; i < g_iSecond; i++)
		FS_FPrintf(f, "%d Second %d\n", g_rgFPS[i], i + 1);

	FS_Close(f);

	if (!COM_CheckParm(const_cast<char*>("-noquit")))
		Cbuf_AddText(const_cast<char*>("quit\n"));
}

void CL_GGSpeeds(float flCurTime)
{
	int frames;

	if (g_flTime == 0.0f)
	{
		g_flTime = flCurTime;
		return;
	}

	if (flCurTime - g_flTime < 1.0f)
		return;

	if (scr_con_current == 0.0f)
	{
		frames = host_framecount - g_iFrames;

		if (frames > g_iMaxFPS)
			g_iMaxFPS = frames;
		if (frames < g_iMinFPS)
			g_iMinFPS = frames;

		if (g_iSecond < GG_MAX_FRAMES)
			g_rgFPS[g_iSecond++] = frames;
	}
	else
	{
		cls.td_starttime = realtime;
		cls.td_startframe = host_framecount;
	}

	g_iFrames = host_framecount;
	g_flTime = flCurTime;
}

void CL_WriteClientDLLMessage(int size, byte* buf)
{
	byte cmd;
	float f;
	int frame;
	int swlen;

	if (!cls.demorecording || cls.demofile == NULL || size < 0)
		return;

	cmd = dem_clientdll;
	FS_Write(&cmd, sizeof(cmd), 1, cls.demofile);

	// convert values to little-endian format

	f = LittleFloat(CL_DemoOutTime() - cls.demostarttime);
	FS_Write(&f, sizeof(f), 1, cls.demofile);

	frame = LittleLong(host_framecount - cls.demostartframe);
	FS_Write(&frame, sizeof(frame), 1, cls.demofile);

	swlen = LittleLong(size);
	FS_Write(&swlen, sizeof(swlen), 1, cls.demofile);

	FS_Write(buf, size, 1, cls.demofile);
}

void CL_ReadClientDLLData()
{
	byte data[32768];
	int i;

	Q_memset(data, 0, sizeof(data));

	FS_Read(&i, sizeof(i), 1, cls.demofile);
	i = min(LittleLong(i), (int)sizeof(data));

	FS_Read(data, i, 1, cls.demofile);

	ClientDLL_ReadDemoBuffer(i, data);
}

BOOL CL_DemoAPIRecording()
{
	return cls.demorecording != false;
}

BOOL CL_DemoAPIPlayback()
{
	return cls.demoplayback != false;
}

BOOL CL_DemoAPITimedemo()
{
	return cls.timedemo != false;
}

int CL_ReadDemoMessage_OLD()
{
	static int tdlastdemoframe;

	int curpos;
	byte msgbuf[MAX_POSSIBLE_MSG];
	client_data_t cdat;
	int frame;
	byte cmd;
	float f = 0.0f;
	float		fElapsedTime = 0.0f;
	qboolean bSkipMessage = false;
	int msglen;
	char szCmd[64];
	qboolean swallowmessages = true;
	float ft;
	int r;


	msglen = 0;

	if (cls.demofile == NULL)
	{
		cls.demoplayback = false;
		CL_Disconnect_f();
		Con_Printf(const_cast<char*>("Tried to read a demo message with no demo file\n"));
		return false;
	}

	do
	{
		curpos = FS_Tell(cls.demofile);
		net_message.cursize = 0;

		FS_Read(&cmd, sizeof(cmd), 1, cls.demofile);

		// timestamp
		FS_Read(&f, sizeof(f), 1, cls.demofile);
		f = LittleFloat(f);
		FS_Read(&frame, sizeof(frame), 1, cls.demofile);
		frame = LittleLong(frame);
		
		ft = CL_DemoInTime();

		fElapsedTime = ft - cls.demostarttime;

		// Time demo ignores clocks and tries to synchronize frames to what was recorded
		//  I.e., while frame is the same, read messages, otherwise, skip out.
		if (!cls.timedemo)
		{
			bSkipMessage = (f >= fElapsedTime) ? true : false;
		}
		else
		{
			bSkipMessage = false;
		}

		if (cmd &&   // Startup messages don't require looking at the timer.
			cmd != dem_read &&
			bSkipMessage)   // Not ready for a message yet, put it back on the file.
		{
			FS_Seek(cls.demofile, curpos, FILESYSTEM_SEEK_HEAD);
			return false;   // Not time yet.
		}

		switch (cmd)
		{
		case dem_jumptime:
			cls.demostarttime = CL_DemoInTime();
			cls.demostartframe = host_framecount;
			break;
		case dem_cmd:
			FS_Read(szCmd, sizeof(szCmd), 1, cls.demofile);
			if (ValidStuffText(szCmd))
			{
				Cbuf_AddFilteredText(szCmd);
				Cbuf_AddFilteredText(const_cast<char*>("\n"));
			}
			break;
		case dem_angles:
			FS_Read(&cdat, sizeof(cdat), 1, cls.demofile);
			ClientDLL_DemoUpdateClientData(&cdat);
			scr_fov_value = cdat.fov;
			break;
		case dem_read:
			// пропуск секций пока они есть
			if (!CL_SwitchToNextDemoSection())
				swallowmessages = false;
			break;
		case dem_event:
			CL_DemoParseEvent();
			break;
		case dem_wpnanim:
			CL_DemoParseAnim();
			break;
		case dem_playsound:
			CL_DemoParseSound();
			break;
		case dem_clientdll:
			CL_ReadClientDLLData();
			break;
		default:
			swallowmessages = false;
			break;
		}
	} while (swallowmessages);

	if (cls.timedemo && tdlastdemoframe == host_framecount)
	{
		FS_Seek(cls.demofile, FS_Tell(cls.demofile) - sizeof(demo_command_t), FILESYSTEM_SEEK_HEAD);
		return false;
	}

	tdlastdemoframe = host_framecount;

	if (cls.td_startframe + 1 == host_framecount)
		cls.td_starttime = realtime;

	// If not on "LOADING" section, check a few things
	if (nCurEntryIndex)
	{
		if (!cls.demoframecount)
			CL_DemoSectionStart();

		// We are now on the second frame of a new section
		if (cls.demoframecount == 1)
		{
			bInFadeout = false;

			if (pCurrentEntry->nFlags & FDEMO_TITLE)
			{
				SetDemoMessage(pCurrentEntry->szDescription, 0.01, 1.0, 2.0);
				CL_HudMessage("__DEMOMESSAGE__");
			}

			if (cls.timedemo == false)
				cls.demostarttime = CL_DemoInTime();
		}
	}

	cls.demoframecount++;

	if (nCurEntryIndex && !bInFadeout)
	{
		if (pCurrentEntry->nFlags & FDEMO_FADEOUT_FAST && pCurrentEntry->fTrackTime + 0.02 <= fElapsedTime + 0.5)
		{
			bInFadeout = true;
			sf_demo.duration = 2048;
			sf_demo.holdTime = 409;
			sf_demo.r = sf_demo.g = sf_demo.b = 0;
			sf_demo.a = 255;
			sf_demo.fadeFlags = FFADE_OUT;
			V_ScreenFade(0, 0, &sf_demo);
		}

		if (pCurrentEntry->nFlags & FDEMO_FADEOUT_SLOW && bInFadeout == false && pCurrentEntry->fTrackTime + 0.02 <= fElapsedTime + 2.0 )
		{
			sf_demo.fadeFlags = FFADE_OUT;
			bInFadeout = true;
			sf_demo.duration = 8192;
			sf_demo.holdTime = 409;
			sf_demo.r = sf_demo.g = sf_demo.b = 0;
			sf_demo.a = 255;
			V_ScreenFade(0, 0, &sf_demo);
		}
	}

	CL_DemoReadCmdInfo(fElapsedTime);
	CL_ReadNetchanState();

	r = FS_Read(&msglen, sizeof(msglen), 1, cls.demofile);

	if (r != sizeof(msglen))
	{
		Host_EndGame(const_cast<char*>("Bad demo length.\n"));
		return 0;
	}

	msglen = LittleLong(msglen);

	if (msglen < 0)
	{
		Host_EndGame(const_cast<char*>("Demo message length < 0.\n"));
		return 0;
	}

	if (msglen > MAX_POSSIBLE_MSG)
	{
		Host_EndGame(const_cast<char*>("Demo message > MAX_POSSIBLE_MSG"));
		return 0;
	}

	if (msglen > 0)
	{
		r = FS_Read(msgbuf, msglen, 1, cls.demofile);
		if (r != msglen)
		{
			Host_EndGame(const_cast<char*>("Error reading demo message data.\n"));
			return 0;
		}
	}

	Q_memcpy(net_message.data, msgbuf, msglen);
	net_message.cursize = msglen;
	return 1;
}

int CL_ReadDemoMessage()
{
	if (!DemoPlayer_IsActive())
		return CL_ReadDemoMessage_OLD();

	net_message.cursize = DemoPlayer_ReadDemoMessage(net_message.data, net_message.maxsize);

	if (!net_message.cursize)
		return false;

	DemoPlayer_ReadNetchanState(&cls.netchan.incoming_sequence, &cls.netchan.incoming_acknowledged, &cls.netchan.incoming_reliable_acknowledged, &cls.netchan.incoming_reliable_sequence, &cls.netchan.outgoing_sequence, &cls.netchan.reliable_sequence, &cls.netchan.last_reliable_sequence);

	return true;
}
