#include "quakedef.h"
#include "DemoPlayerWrapper.h"
#include "IDemoPlayer.h"
#include "vgui_int.h"
#include "SystemWrapper.h"

IDemoPlayer* s_DemoPlayer = NULL;
int s_DemoPlayerMode;

bool DemoPlayer_Init()
{
	s_DemoPlayerMode = 0;
	s_DemoPlayer = (IDemoPlayer*)gSystemWrapper.GetModule(const_cast<char*>(DEMOPLAYER_INTERFACE_VERSION), const_cast<char*>("demoplayer"), NULL);

	if (s_DemoPlayer == NULL)
	{
		Con_DPrintf(const_cast<char*>("Failed to load demo player module.\n"));
		return false;
	}

	s_DemoPlayer->SetMasterMode(COM_CheckParm(const_cast<char*>("-demoedit")) != 0);
	return true;
}

void DemoPlayer_Stop()
{
	s_DemoPlayer->Stop();
}

void DemoPlayer_ReadNetchanState(
	int *incoming_sequence,
	int *incoming_acknowledged,
	int *incoming_reliable_acknowledged,
	int *incoming_reliable_sequence,
	int *outgoing_sequence,
	int *reliable_sequence,
	int *last_reliable_sequence)
{
	if (s_DemoPlayer)
		s_DemoPlayer->ReadNetchanState(incoming_sequence,
		incoming_acknowledged,
		incoming_reliable_acknowledged,
		incoming_reliable_sequence,
		outgoing_sequence,
		reliable_sequence,
		last_reliable_sequence);
}

BOOL DemoPlayer_IsActive()
{
	return s_DemoPlayer != NULL && s_DemoPlayerMode != 0;
}

void DemoPlayer_SetActive(int state)
{
	s_DemoPlayerMode = state;
}

void DemoPlayer_GetDemoViewInfo(ref_params_t *rp, float *view, int *viewmodel)
{
	if (s_DemoPlayer)
		s_DemoPlayer->GetDemoViewInfo(rp, view,	viewmodel);
}

int DemoPlayer_ReadDemoMessage(byte *buffer, int size)
{
	if (s_DemoPlayer)
		return s_DemoPlayer->ReadDemoMessage(buffer, size);
	return 0;
}

void DemoPlayer_SetEditMode(int state)
{
	if (s_DemoPlayer)
		s_DemoPlayer->SetEditMode(state != 0);
}

int DemoPlayer_LoadGame(char *filename)
{
	if (s_DemoPlayer)
		return s_DemoPlayer->LoadGame(filename);
	return 0;
}

void DemoPlayer_ShowGUI()
{
	if (s_DemoPlayer)
		VGuiWrap2_ShowDemoPlayer();
}
