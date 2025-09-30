#ifndef ENGINE_LOADBLOB_H
#define ENGINE_LOADBLOB_H

#include <tier0/platform.h>

#define BLOB_XOR_IMPORT		0x872C3D47
#define BLOB_XOR_IMGBASE	0x49C042D1
#define BLOB_XOR_EXPORT		0x7A32BC85
#define BLOB_SHT_ENTRY		0x0000000C

struct BlobInfo_t
{
	char	m_szPath[10];
	char	m_szDescribe[32];
	char	m_szCompany[22];
	DWORD	m_dwAlgorithm;			// Magic number that we use to identify whenether this file has been encrypted or not
};

// Loader data
struct BlobHeader_t
{
	DWORD	m_dwCheckSum;
	WORD	m_wSectionCount;
	DWORD	m_dwExportPoint;		// VA to some function, we don't care about that here
	DWORD	m_dwImageBase;			// Base virtual address of this image (for hw it's 0x1D00000)
	DWORD	m_dwEntryPoint;			// VA to entry point
	DWORD	m_dwImportTable;		// RA to import table
};

// Data section such as .text, .rdata or other.
struct BlobSection_t
{
	DWORD	m_dwVirtualAddress;		// VA from the image base
	DWORD	m_dwVirtualSize;		// The total size of the section when loaded into memory, in bytes 
	DWORD	m_dwDataSize;			// Raw data size of the section
	DWORD	m_dwDataAddress;		// RA from the base of encrypted rgba
	BOOL	m_bIsSpecial;			// Thing to indicate whenether the section is special or not
};

struct BlobFootprint_t
{
	int32 m_hDll;
};

using FFunction = void ( * )( void* );

int FIsBlob( const char* pstFileName );

void FreeBlob( BlobFootprint_t* pblobfootprint );

void* NLoadBlobFile( const char* pstFileName, BlobFootprint_t* pblobfootprint, void* pv, char fLoadAsDll );
void* LoadBlobFile(byte* pBuffer, BlobFootprint_t* pblobfootprint, void** pv, size_t size);
void LoadBlobImports(BlobHeader_t* pHeader);

#endif //ENGINE_LOADBLOB_H
