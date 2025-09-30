#ifdef WIN32
#include "winheaders.h"
#else
#include <dlfcn.h>
#endif

#include "quakedef.h"
#include "LoadBlob.h"

int FIsBlob( const char* pstFileName )
{
	return false;
}

void FreeBlob( BlobFootprint_t* pblobfootprint )
{
	if( pblobfootprint->m_hDll )
	{
#ifdef WIN32
		FreeLibrary( reinterpret_cast<HMODULE>( pblobfootprint->m_hDll ) );
#else
		dlclose( ( void * ) pblobfootprint->m_hDll );
#endif
	}
}

void* NLoadBlobFile( const char* pstFileName, BlobFootprint_t* pblobfootprint, void* pv, char fLoadAsDll )
{
	char rgchNew[ 2048 ], rgchLocalPath[ 2048 ];
	void* pFile = nullptr;
	size_t	unFileSize;
	FileHandle_t hFile;
	byte* pBlobFile = NULL;

	if( fLoadAsDll )
	{
		strcpy( rgchNew, pstFileName );

		g_pFileSystem->GetLocalPath( rgchNew, rgchLocalPath, ARRAYSIZE( rgchLocalPath ) );
		
		pFile = FS_LoadLibrary( rgchLocalPath );
		pblobfootprint->m_hDll = reinterpret_cast<int32>( pFile );

		if( pFile )
		{
			auto pFunc = reinterpret_cast<FFunction>( Sys_GetProcAddress( pFile, "F" ) );
			
			if( pFunc )
			{
				pFunc( pv );
			}
			else
			{
				Sys_UnloadModule( reinterpret_cast<CSysModule*>( pFile ) );
				pFile = nullptr;
			}
		}
	}
#if defined(WIN32) && !defined(_WIN64)
	else
	{
		hFile = g_pFileSystem->OpenFromCacheForRead(pstFileName, "rb");

		if (!hFile && strcmp(com_gamedir, "cstrike"))
		{
			hFile = g_pFileSystem->Open(pstFileName, "rb");
		}

		g_pFileSystem->Seek(hFile, 0, FILESYSTEM_SEEK_TAIL);
		unFileSize = g_pFileSystem->Tell(hFile);
		g_pFileSystem->Seek(hFile, 0, FILESYSTEM_SEEK_HEAD);

		pBlobFile = (byte*)malloc(unFileSize);
		g_pFileSystem->Read(pBlobFile, unFileSize, hFile);

		// Load an encrypted file
		pFile = LoadBlobFile(pBlobFile, pblobfootprint, (void**)pv, unFileSize);

		free(pBlobFile);
		g_pFileSystem->Close(hFile);

		return pFile;
	}
#endif

	return pFile;
}

#if defined(_WIN32) && !defined(_WIN64)
// Do the decryption and further usage of the decrypted data
// The encrypted dll is composed out of two blob headers (info and data header) and
// file sections. There isn't any PE header, but that isn't needed
// when the dll is being only loaded. Because only things we need are
// things like imports, entry point or the base address.
void* LoadBlobFile(byte* pBuffer, BlobFootprint_t* pblobfootprint, void** pv, size_t size)
{
	uint32			i;
	uint16			j; // for sections
	BYTE			bXor;
	DWORD			dwAddress;

	BlobHeader_t* pHeader;
	BlobSection_t* pSection;

	dwAddress = 0;

	bXor = 'W';

	// XOR the whole binary with letter 'W'
	// Start at first 68 bytes
	for (i = sizeof(BlobInfo_t); i < size; i++)
	{
		pBuffer[i] ^= bXor;
		bXor += pBuffer[i] + 'W';
	}
	// After the rgba has been encrypted, start to read info from it

	// After the blob info header there's the data header, containing all
	// the important information about the file.
	//
	// Some RVAs are xorred with yet another magic number to retain more
	// obfuscation.
	pHeader = (BlobHeader_t*)(pBuffer + sizeof(BlobInfo_t));

	// After these two headers, just like in a PE header, comes section 
	// tables describing where's the data located etc.
	pSection = (BlobSection_t*)(pBuffer + sizeof(BlobInfo_t)+sizeof(BlobHeader_t));

	for (j = 0; j <= pHeader->m_wSectionCount; j++)
	{
		// At special section we return raw data the address
		if (pSection[j].m_bIsSpecial)
		{
			dwAddress = pSection[j].m_dwDataAddress;
		}

		// Fill remaining section bytes with zeros, just as it is
		// said in the PE specification
		if (pSection[j].m_dwVirtualSize > pSection[j].m_dwDataSize)
		{
			memset((byte*)(pSection[j].m_dwVirtualAddress + pSection[j].m_dwDataSize), 0, pSection[j].m_dwVirtualSize - pSection[j].m_dwDataSize);
		}

		// Copy raw data over from the rgba to our process memory
		memcpy((byte*)pSection[j].m_dwVirtualAddress, pBuffer + pSection[j].m_dwDataAddress, pSection[j].m_dwDataSize);
	}

	// Resolve imports, load libraries, locate function pointers
	LoadBlobImports(pHeader);

	// Call entry point of the dll
	((BOOL(WINAPI*)(HINSTANCE, DWORD, void*))(pHeader->m_dwEntryPoint - BLOB_SHT_ENTRY))(NULL, DLL_PROCESS_ATTACH, NULL);

	// Then the F function
	((void(*)(void**))(pHeader->m_dwExportPoint ^ BLOB_XOR_EXPORT))(pv);

	return (void*)dwAddress;
}

// Load imports from a blob rgba and get loads all libraries needed
void LoadBlobImports(BlobHeader_t* pHeader)
{
	HMODULE			hPorcDll;

	PIMAGE_THUNK_DATA pThunk;
	PIMAGE_IMPORT_DESCRIPTOR pImgDescriptor;

	const char* pszProcName;

	pHeader->m_dwImageBase ^= BLOB_XOR_IMGBASE;
	pHeader->m_dwImportTable ^= BLOB_XOR_IMPORT;

	pImgDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(pHeader->m_dwImportTable);

	// iterate over all descriptors available and load all libraries
	for (; pImgDescriptor->Name; pImgDescriptor++)
	{
		hPorcDll = LoadLibrary((char*)(pHeader->m_dwImageBase + pImgDescriptor->Name));
		pThunk = (PIMAGE_THUNK_DATA)(pHeader->m_dwImageBase + pImgDescriptor->FirstThunk);

		// Every import descriptor contains thunks(entries) which are esentially just funciton pointers
		for (; pThunk->u1.Function; pThunk++)
		{
			// DLLs can either export by a name or by an ordinal
			if (IMAGE_SNAP_BY_ORDINAL(pThunk->u1.Ordinal))
			{
				// Exported by ordinal
				pszProcName = (char*)((LONG)pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32 - 1);
			}
			else
			{
				// Exported by name
				pszProcName = (char*)(pHeader->m_dwImageBase + ((PIMAGE_IMPORT_BY_NAME)((LONG)pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32 - 1))->Name);
			}

			pThunk->u1.AddressOfData = (DWORD)GetProcAddress(hPorcDll, pszProcName);
		}
	}
}
#endif
