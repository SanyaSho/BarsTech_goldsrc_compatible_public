//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: win32 dependant ASM code for CPU capability detection
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================

#include <windows.h>

#pragma optimize( "", off )
#pragma warning( disable: 4800 ) //'int' : forcing value to bool 'true' or 'false' (performance warning)

typedef unsigned __int64	QWORD;

#if defined(_WIN32) && !defined(_WIN64)


#define RDTSC _asm _emit 0fh _asm _emit 031h
#define CPUID _asm _emit 0fh _asm _emit 0a2h
#define EMMS _asm _emit 0fh _asm _emit 077h

// If CPUID supported return 1, else return 0
unsigned int HasCpuid()
{
	unsigned int supported = 0;

	_asm {
		push	ebx				; save ebx
		pushfd					; save flags
		pushfd
		pop		eax
		mov		ebx,eax			; save old flags
		xor		eax,(1 SHL 21)	; flip id bit
		push	eax
		popfd
		pushfd
		pop	eax
		popfd					; restore origional flags
		cmp		ebx,eax			; did ID bit change?
		pop		ebx
		je		Done			; if no change fail
		mov		supported,1		; CPUID is supported
	Done:
	}

	return supported;
}


// If RDTSC supported return 1, else return 0
unsigned int HasTsc()
{
	unsigned int supported = 0;

	if(! HasCpuid())
		return 0;

	_asm {
		; check that max cpuid levels must be at least 1
		mov		eax,0
		CPUID
		cmp		eax,1
		jl		Done

		; check TSC bit in feature flags
		mov		eax,1
		CPUID
		bt		edx,4			; check TSD bit
		jnc		Done

		; RDTSC is supported
		mov		supported,1
	Done:
	}

	return supported;
}


// --------------------------------------------------------------------------
QWORD GetRDTSC(void)
{
	QWORD clock;
	_asm
	{
		// cpuid serializes for out of order processors
		push ebx
		push ecx
		xor eax, eax
		CPUID
		RDTSC
		mov dword ptr clock, eax
		mov dword ptr clock+4, edx
		xor eax, eax
		CPUID
		pop ecx
		pop ebx
	}
	return clock;
}
// -----------------------------------------------------------------------
int PROC_GetSpeed(void)
{
	QWORD StartClock, ElapClock;
	DWORD StartTime, times = 0;
	int RetVal;

	if (!HasTsc())
		return 166;

	// try to get rid of the variability
	StartClock = GetRDTSC();
	StartTime = timeGetTime();
	// this loop should take 1 sec +- 1 ms
	while (timeGetTime() < StartTime + 250)
		;
	ElapClock = GetRDTSC() - StartClock + 500000;

	// try to get rid of the variability
	StartClock = GetRDTSC();
	StartTime = timeGetTime();
	// this loop should take 1 sec +- 1 ms
	while (timeGetTime() < StartTime + 1000)
		;
	ElapClock = GetRDTSC() - StartClock + 500000;
	RetVal = (DWORD)(ElapClock/1000000);
	return RetVal;
}

#pragma optimize( "", off )
// --------------------------------------------------------------------------
int PROC_IsMMX(void)
{
    int retval = 1;
    DWORD RegEDX;

    __try
	{
        _asm
		{
            mov eax, 1      // set up CPUID to return processor version and features
                            //      0 = vendor string, 1 = version info, 2 = cache info
            CPUID           // code bytes = 0fh,  0a2h
            mov RegEDX, edx // features returned in edx
		}
    } __except(EXCEPTION_EXECUTE_HANDLER) { retval = FALSE; }

    if (retval == FALSE)
            return 0;           // processor does not support CPUID

    if (RegEDX & 0x800000)          // bit 23 is set for MMX technology
    {
       __try { 
		   EMMS // _asm emms 
	   }          // try executing the MMX instruction "emms"
       __except(EXCEPTION_EXECUTE_HANDLER) { retval = FALSE; }
    }

    else
            return 0;           // processor supports CPUID but does not support MMX technology

    // if retval == 0 here, it means the processor has MMX technology but
    // floating-point emulation is on; so MMX technology is unavailable

    return retval;
}
// --------------------------------------------------------------------------
#pragma optimize( "", on )

#elif defined(_WIN64)
// x64-based processor always supports mmx technology
int PROC_IsMMX(void)
{
	return 1;
}
#else
// unknown platform
int PROC_IsMMX(void)
{
	return 0;
}
#endif

