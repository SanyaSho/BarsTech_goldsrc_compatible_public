//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "voice_wavefile.h"
#include "FileSystem.h"

static unsigned long ReadDWord(FileHandle_t fp) 
{
	unsigned long ret;  
	FS_Read(&ret, 4, 1, fp); 
	return ret;
}

static unsigned short ReadWord(FileHandle_t fp) 
{
	unsigned short ret; 
	FS_Read(&ret, 2, 1, fp);
	return ret;
}

static void WriteDWord(FileHandle_t fp, unsigned long val) 
{
	FS_Write(&val, 4, 1, fp);
}

static void WriteWord(FileHandle_t fp, unsigned short val) 
{
	FS_Write(&val, 2, 1, fp);
}



bool ReadWaveFile(
	const char *pFilename,
	char *&pData,
	int &nDataBytes,
	int &wBitsPerSample,
	int &nChannels,
	int &nSamplesPerSec)
{
	FileHandle_t fp = FS_Open(pFilename, "rb");
	if(!fp)
		return false;

	FS_Seek(fp, 22, FILESYSTEM_SEEK_HEAD);
	
	nChannels = ReadWord(fp);
	nSamplesPerSec = ReadDWord(fp);

	FS_Seek(fp, 34, FILESYSTEM_SEEK_HEAD);
	wBitsPerSample = ReadWord(fp);

	FS_Seek(fp, 40, FILESYSTEM_SEEK_HEAD);
	nDataBytes = ReadDWord(fp);
	ReadDWord(fp);
	pData = new char[nDataBytes];
	if(!pData)
	{
		FS_Close(fp);
		return false;
	}
	FS_Read(pData, nDataBytes, 1, fp);
	return true;
}

bool WriteWaveFile(
	const char *pFilename, 
	const char *pData, 
	int nBytes, 
	int wBitsPerSample, 
	int nChannels, 
	int nSamplesPerSec)
{
	FileHandle_t fp = FS_Open(pFilename, "wb");
	if(!fp)
		return false;

	// Write the RIFF chunk.
	FS_Write("RIFF", 4, 1, fp);
	WriteDWord(fp, 0);
	FS_Write("WAVE", 4, 1, fp);
	

	// Write the FORMAT chunk.
	FS_Write("fmt ", 4, 1, fp);
	
	WriteDWord(fp, 0x10);
	WriteWord(fp, 1);	// WAVE_FORMAT_PCM
	WriteWord(fp, (unsigned short)nChannels);	
	WriteDWord(fp, (unsigned long)nSamplesPerSec);
	WriteDWord(fp, (unsigned long)((wBitsPerSample / 8) * nChannels * nSamplesPerSec));
	WriteWord(fp, (unsigned short)((wBitsPerSample / 8) * nChannels));
	WriteWord(fp, (unsigned long)wBitsPerSample);

	// Write the DATA chunk.
	FS_Write("data", 4, 1, fp);
	WriteDWord(fp, (unsigned long)nBytes);
	FS_Write(pData, nBytes, 1, fp);


	// Go back and write the length of the riff file.
	unsigned long dwVal = FS_Tell(fp) - 8;
	FS_Seek(fp, 4, FILESYSTEM_SEEK_HEAD);
	WriteDWord(fp, dwVal);

	FS_Close(fp);
	return true;
}


