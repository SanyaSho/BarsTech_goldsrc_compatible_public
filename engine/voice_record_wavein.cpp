#include "winsani_in.h"
#include <windows.h>
#include "winsani_out.h"
#include <mmsystem.h>
#include "quakedef.h"
#include "sound.h"
#include "ivoicerecord.h"
#include "CircularBuffer.h"

#define WAVE_MAX_CHANNELS	15

#define WAVE_FLAG_PASSED	(1<<0)

void WinMMPrintError(MMRESULT mmrError)
{
	CHAR szText[260];

	waveInGetErrorTextA(mmrError, szText, sizeof(szText));
	Con_Printf(const_cast<char*>("%s\n"), szText);
}

class VoiceRecord_WaveIn : public IVoiceRecord
{
protected:

	virtual				~VoiceRecord_WaveIn();


	// IVoiceRecord.
public:

	VoiceRecord_WaveIn();
	// Use this to delete the object.
	virtual void		Release();

	// Start/stop capturing.
	virtual bool		RecordStart();
	virtual void		RecordStop();

	// Idle processing.
	virtual void		Idle();

	// Get the most recent N samples. If nSamplesWanted is less than the number of
	// available samples, it discards the first samples and gives you the last ones.
	virtual int			GetRecordedData(short *pOut, int nSamplesWanted);

	virtual bool		Init(int sampleRate);
	void InputCallback(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
private:
	bool RecordAddBuffer(int idx);
	void Term();
	void Clear();
private:

	// class size 8220, el's size 8216
	WAVEHDR headers[WAVE_MAX_CHANNELS];
	char buffers[WAVE_MAX_CHANNELS][256];
	CRITICAL_SECTION m_CS;
	HWAVEIN m_hWaveIn;
	bool m_bActive;
	
	// CCircularBuffer, size = ???
	CSizedCircularBuffer<3840> m_Buffer;

};

VoiceRecord_WaveIn::VoiceRecord_WaveIn()
{
	m_hWaveIn = NULL;
	InitializeCriticalSection(&this->m_CS);
	m_bActive = false;
}

VoiceRecord_WaveIn::~VoiceRecord_WaveIn()
{
	DeleteCriticalSection(&m_CS);
	Term();
}

void VoiceRecord_WaveIn::Release()
{
	delete this;
}

bool VoiceRecord_WaveIn::RecordStart()
{
	this->m_bActive = true;
	Idle();
	return true;
}

void VoiceRecord_WaveIn::RecordStop()
{
	this->m_bActive = false;
}

void VoiceRecord_WaveIn::Idle()
{
	for (int i = 0; i < WAVE_MAX_CHANNELS; i++)
	{
		if (headers[i].dwUser & WAVE_FLAG_PASSED && m_bActive)
			RecordAddBuffer(i);
	}

	if (m_bActive)
	{
		EnterCriticalSection(&this->m_CS);
		m_Buffer.Flush();
		LeaveCriticalSection(&this->m_CS);
	}
}

void VoiceRecord_WaveIn::Term()
{
	if (this->m_hWaveIn)
	{
		RecordStop();
		waveInReset(this->m_hWaveIn);
		waveInClose(this->m_hWaveIn);

		this->m_hWaveIn = NULL;
	}

	Clear();
}

void VoiceRecord_WaveIn::Clear()
{
	this->m_hWaveIn = NULL;
	this->m_bActive = false;

	Q_memset(this->headers, 0, sizeof(VoiceRecord_WaveIn::headers));
	Q_memset(this->buffers, 0, sizeof(VoiceRecord_WaveIn::buffers));

	m_Buffer.Flush();
}

int VoiceRecord_WaveIn::GetRecordedData(short *pOut, int nSamplesWanted)
{
	int queue, cb;
	int monosize = nSamplesWanted << 1;
	
	EnterCriticalSection(&this->m_CS);
	
	queue = this->m_Buffer.GetReadAvailable();
	
	if (queue > monosize)
		this->m_Buffer.Advance(queue - monosize);

	cb = this->m_Buffer.Read(pOut, monosize);

	LeaveCriticalSection(&this->m_CS);
	Idle();
	// cvt to stereo
	return cb >> 1;
}

void VoiceRecord_WaveIn::InputCallback(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	if (dwParam1 == WIM_DATA)
	{
		LPWAVEHDR lpHdr = (WAVEHDR *)dwParam1;
		if (this->m_bActive)
		{
			if (lpHdr->dwBytesRecorded)
			{
				EnterCriticalSection(&this->m_CS);
				m_Buffer.Write(lpHdr, lpHdr->dwBytesRecorded);
				LeaveCriticalSection(&this->m_CS);
			}
		}
		lpHdr->dwUser |= WAVE_FLAG_PASSED;
	}
}

void CALLBACK waveInProc(
	HWAVEIN   hwi,
	UINT      uMsg,
	DWORD_PTR dwInstance,
	DWORD_PTR dwParam1,
	DWORD_PTR dwParam2
	)
{
	VoiceRecord_WaveIn* lpClass = (VoiceRecord_WaveIn*)dwInstance;
	lpClass->InputCallback(hwi, uMsg, dwParam1, dwParam2);
}

bool VoiceRecord_WaveIn::Init(int sampleRate)
{
	WAVEFORMATEX wfx;

	wfx.wFormatTag = 1;
	wfx.nChannels = 1;
	wfx.nSamplesPerSec = sampleRate;
	wfx.nAvgBytesPerSec = sampleRate << 1;

	wfx.nBlockAlign = 2;
	wfx.wBitsPerSample = 16;

	// packed sizeof wfx
	wfx.cbSize = 18;
	
	MMRESULT res = waveInOpen(&this->m_hWaveIn, 0xFFFFFFFF, &wfx, (DWORD_PTR)waveInProc, (DWORD_PTR)this, CALLBACK_FUNCTION);

	if (res != MMSYSERR_NOERROR)
	{
		Con_DPrintf(const_cast<char*>("ERROR VoiceRecord_WaveIn: waveInOpen\n"));
		WinMMPrintError(res);
		Term();
		return false;
	}

	for (int i = 0; i < WAVE_MAX_CHANNELS; i++)
	{
		memset(&this->headers[i], 0, sizeof(WAVEHDR));

		this->headers[i].lpData = this->buffers[i];
		this->headers[i].dwBufferLength = sizeof(VoiceRecord_WaveIn::buffers[0]);
		this->headers[i].dwUser = WAVE_FLAG_PASSED;

		res = waveInPrepareHeader(this->m_hWaveIn, &this->headers[i], sizeof(WAVEHDR));
		if (res != MMSYSERR_NOERROR)
		{
			Con_DPrintf(const_cast<char*>("ERROR VoiceRecord_WaveIn::Init: waveInPrepareHeader\n"));
			WinMMPrintError(res);
			Term();
			return false;
		}
	}

	res = waveInStart(this->m_hWaveIn);
	if (res != MMSYSERR_NOERROR)
	{
		Con_DPrintf(const_cast<char*>("ERROR Voice_RecordStart: waveInStart\n"));
		WinMMPrintError(res);
		Term();
		return false;
	}

	return true;
}

bool VoiceRecord_WaveIn::RecordAddBuffer(int idx)
{
	memset(&this->buffers[idx], 0, sizeof(VoiceRecord_WaveIn::buffers));

	this->headers[idx].dwUser = 0;

	MMRESULT res = waveInAddBuffer(this->m_hWaveIn, &this->headers[idx], sizeof(WAVEHDR));
	if (res == MMSYSERR_NOERROR)
		return true;

	Con_DPrintf(const_cast<char*>("ERROR VoiceRecord_WaveIn::RecordAddBuffer: waveInAddBuffer\n"));
	WinMMPrintError(res);
	return false;
}

IVoiceRecord* CreateVoiceRecord_WaveIn(int sampleRate)
{
	VoiceRecord_WaveIn *pRecord = new VoiceRecord_WaveIn;
	if (pRecord && pRecord->Init(sampleRate))
	{
		return pRecord;
	}
	else
	{
		if (pRecord)
			pRecord->Release();

		return NULL;
	}
}
