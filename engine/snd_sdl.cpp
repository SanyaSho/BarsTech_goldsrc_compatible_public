#include "quakedef.h"
#include "sound.h"
#include "winquake.h"

qboolean g_fUseDInput = false;

void Snd_AcquireBuffer()
{
	if (snd_isdirect)
	{
		if (++snd_buffer_count == 1)
			return S_Startup();
	}
}

void Snd_ReleaseBuffer()
{
	if (snd_isdirect)
	{
		if (!--snd_buffer_count)
		{
			S_ClearBuffer();
			S_Shutdown();
		}
	}
}

void SetMouseEnable( int fState )
{
	//Nothing since HL25
}
