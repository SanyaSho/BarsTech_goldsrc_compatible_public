#ifndef ENGINE_DEMOPLAYERWRAPPER_H
#define ENGINE_DEMOPLAYERWRAPPER_H

bool DemoPlayer_Init();
void DemoPlayer_Stop();
void DemoPlayer_ReadNetchanState(
	int *incoming_sequence,
	int *incoming_acknowledged,
	int *incoming_reliable_acknowledged,
	int *incoming_reliable_sequence,
	int *outgoing_sequence,
	int *reliable_sequence,
	int *last_reliable_sequence);
BOOL DemoPlayer_IsActive();
void DemoPlayer_SetActive(int state);
void DemoPlayer_GetDemoViewInfo(ref_params_t *rp, float *view, int *viewmodel);

int DemoPlayer_ReadDemoMessage(byte *buffer, int size);
void DemoPlayer_SetEditMode(int state);
int DemoPlayer_LoadGame(char *filename);
void DemoPlayer_ShowGUI();

#endif //ENGINE_DEMOPLAYERWRAPPER_H
