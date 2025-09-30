#include "MilesOodleIO.h"

#include "mss.h"

#include "c:\devel\projects\oodle\public\Oodle.h"

RADDEFSTART

// warning : OodleMilesFile is duplicated in milesasync.cpp and MilesOodleIO.cpp
RADSTRUCT OodleMilesFile
{
	enum { check_val = 0x70AD331F };

	OodleIOQStream	m_stream;
	S64				m_pos;
	OodleFileInfo	m_info;
	U64				m_check;
};

void OodleMilesFile_UpdateInfo(OodleMilesFile * omf)
{
	if ( omf->m_info.size != OODLE_FILE_SIZE_INVALID )
		return;

	OodleIOQFile file = OodleIOQStream_GetFile(omf->m_stream);
	if ( file )
	{
		OodleIOQ_GetInfo(file,&omf->m_info);
	}
}

static 
U32 AILLIBCALLBACK
OodleMiles_FileOpen(char const* i_FileName, UINTa* o_FileHandle)
{
	OodleMilesFile * omf = (OodleMilesFile * ) OodleMalloc( sizeof(OodleMilesFile) );
	
	S32 bufSize = OodleIOQStream_MakeValidBufSize(64*1024);
	S32 initialRead = bufSize/2;
	omf->m_stream = OodleIOQStream_RequestOpen(i_FileName,NULL,true,bufSize,true,initialRead);
	
	omf->m_pos = 0;
	omf->m_info.size = OODLE_FILE_SIZE_INVALID;
	omf->m_check = OodleMilesFile::check_val;

	*o_FileHandle = (UINTa) omf;
    
    return 1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static
void AILLIBCALLBACK
OodleMiles_FileClose(UINTa i_FileHandle)
{
    if (i_FileHandle == 0)
    {
        return;
    }
    
	OodleMilesFile * omf = (OodleMilesFile *) i_FileHandle;
	
	if ( omf->m_stream != 0 )
	{
		OodleIOQStream_RequestClose(omf->m_stream,NULL);
		omf->m_stream = 0;
	}
	
	OodleFreeSized(omf,sizeof(*omf));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static
S32 AILLIBCALLBACK
OodleMiles_FileSeek(UINTa i_FileHandle, S32 i_Offset, U32 i_Type)
{
    if (i_FileHandle == 0)
    {
        return 0;
    }
	OodleMilesFile * omf = (OodleMilesFile *) i_FileHandle;

	OodleMilesFile_UpdateInfo(omf);
	
	if ( omf->m_info.size == OODLE_FILE_SIZE_INVALID )
		return 0;

	S64 fileSize = omf->m_info.size;

    if (i_Type == AIL_FILE_SEEK_END)
    {
		omf->m_pos = fileSize + i_Offset;
    }
    else if (i_Type == AIL_FILE_SEEK_CURRENT)
    {
		omf->m_pos += i_Offset;
    }
    else
    {
		omf->m_pos = i_Offset;
    }

	omf->m_pos = RR_CLAMP(omf->m_pos,0,fileSize);

	OodleIOQStream_Seek(omf->m_stream,omf->m_pos);
	OodleIOQStream_Retire(omf->m_stream,omf->m_pos);

    return (S32) omf->m_pos;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

	
static 
U32 AILLIBCALLBACK
OodleMiles_FileRead(UINTa i_FileHandle, void* o_Buffer, U32 i_ReadBytes)
{
    if (i_FileHandle == 0)
    {
        return 0;
    }
	OodleMilesFile * omf = (OodleMilesFile *) i_FileHandle;

	OodleMilesFile_UpdateInfo(omf);
	
	if ( omf->m_info.size == OODLE_FILE_SIZE_INVALID )
		return 0;
		
	S64 remain = omf->m_info.size - omf->m_pos;
	
	S32 readBytes = (S32) RR_MIN( (S64)i_ReadBytes , remain );

	if ( readBytes <= 0 )
		return 0;

    S32 bytesRead = OodleIOQStream_SyncRead(omf->m_stream,omf->m_pos,o_Buffer,readBytes,-1);
    
    omf->m_pos += bytesRead;

	return bytesRead;
}



void Oodle_SetMilesFileIO()
{

	AIL_set_file_callbacks(OodleMiles_FileOpen,OodleMiles_FileClose,OodleMiles_FileSeek,OodleMiles_FileRead);

}
RADDEFEND
