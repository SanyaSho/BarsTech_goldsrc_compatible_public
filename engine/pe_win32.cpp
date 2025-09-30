#include "quakedef.h"

#include <winsani_in.h>
#include <Windows.h>
#include <winsani_out.h>

DWORD PE_DetectTargetOS(LPBYTE lpImage)
{
	PIMAGE_DOS_HEADER     DOSHeader;
	PIMAGE_NT_HEADERS     NTHeader;

	DOSHeader = (PIMAGE_DOS_HEADER)lpImage;

	if (DOSHeader->e_magic != IMAGE_DOS_SIGNATURE)
		return FALSE;

	NTHeader = (PIMAGE_NT_HEADERS)(lpImage + DOSHeader->e_lfanew);

	if (*(WORD*)&NTHeader->Signature == IMAGE_OS2_SIGNATURE || *(WORD*)&NTHeader->Signature == IMAGE_OS2_SIGNATURE_LE)
		return *(WORD*)&NTHeader->Signature;

	return NTHeader->Signature == IMAGE_NT_SIGNATURE ? IMAGE_NT_SIGNATURE : IMAGE_DOS_SIGNATURE;
}

int PE_CountSections(LPBYTE lpImage)
{
	PIMAGE_DOS_HEADER     DOSHeader;
	PIMAGE_NT_HEADERS     NTHeader;

	DOSHeader = (PIMAGE_DOS_HEADER)lpImage;
	NTHeader = (PIMAGE_NT_HEADERS)(lpImage + DOSHeader->e_lfanew);

	return NTHeader->FileHeader.NumberOfSections;
}

IMAGE_SECTION_HEADER* PE_SectionFromDirectory(LPBYTE lpImage, int iDirNum)
{
	PIMAGE_NT_HEADERS     NTHeader;
	PIMAGE_DOS_HEADER     DOSHeader;
	PIMAGE_SECTION_HEADER Sections;
	PIMAGE_EXPORT_DIRECTORY pExportDirectory;
	int iSectionsTotal, iCurrentSection;
	DWORD RVA;

	DOSHeader = (PIMAGE_DOS_HEADER)lpImage;
	NTHeader = (PIMAGE_NT_HEADERS)(lpImage + DOSHeader->e_lfanew);
	Sections = (PIMAGE_SECTION_HEADER)(lpImage + DOSHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS));

	iSectionsTotal = PE_CountSections(lpImage);

	// передаваемый аргумент больше числа директорий в PE'шке. невалид бинарник?
	if (iDirNum >= NTHeader->OptionalHeader.NumberOfRvaAndSizes)
		return NULL;

	RVA = NTHeader->OptionalHeader.DataDirectory[iDirNum].VirtualAddress;

	// конвертация виртуального адреса в физический (относительно начала файла)

	for (iCurrentSection = 0; iCurrentSection < iSectionsTotal; iCurrentSection++, Sections++)
	{
		if (RVA >= Sections->VirtualAddress && RVA < Sections->VirtualAddress + Sections->SizeOfRawData)
			return (IMAGE_SECTION_HEADER*)(lpImage + RVA + Sections->PointerToRawData - Sections->VirtualAddress);
	}

	return NULL;
}

int PE_PopulateSections(LPBYTE lpImage, HANDLE hHeap, char** ppszOut)
{
	PIMAGE_NT_HEADERS     NTHeader;
	PIMAGE_DOS_HEADER     DOSHeader;
	PIMAGE_SECTION_HEADER Sections;
	int iSectionsTotal, iSectionLength;
	char* rgsectionnames;
	int i;

	iSectionsTotal = PE_CountSections(lpImage);
	iSectionLength = 0;

	if (PE_DetectTargetOS(lpImage) != IMAGE_NT_SIGNATURE)
		return 0;

	DOSHeader = (PIMAGE_DOS_HEADER)lpImage;
	NTHeader = (PIMAGE_NT_HEADERS)(lpImage + DOSHeader->e_lfanew);
	Sections = (PIMAGE_SECTION_HEADER)(lpImage + DOSHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS));

	if (Sections == NULL)
		return 0;

	for (i = 0; i < iSectionsTotal; i++)
		iSectionLength += strlen((char*)Sections[i].Name) + 1;
	
	rgsectionnames = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, iSectionLength);

	*ppszOut = rgsectionnames;

	for (i = 0; i < iSectionsTotal; i++)
	{
		strcpy(rgsectionnames, (char*)Sections[i].Name);
		rgsectionnames += strlen((char*)Sections[i].Name) + 1;
	}

	return iSectionLength;
}

bool PE_FindSec(LPBYTE lpImage, IMAGE_SECTION_HEADER* buffer, char* secname)
{
	PIMAGE_DOS_HEADER     DOSHeader;
	PIMAGE_SECTION_HEADER Sections;
	int iSectionsTotal, iCurrentSection;

	iSectionsTotal = PE_CountSections(lpImage);

	DOSHeader = (PIMAGE_DOS_HEADER)lpImage;
	Sections = (PIMAGE_SECTION_HEADER)(lpImage + DOSHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS));

	if (!iSectionsTotal)
		return false;

	for (iCurrentSection = 0; iCurrentSection < iSectionsTotal; iCurrentSection++, Sections++)
	{
		if (!strcmp((char*)Sections->Name, secname))
		{
			*buffer = *Sections;
			return true;
		}
	}

	return false;
}

int PE_HasDebugInfo(LPBYTE lpImage)
{
	PIMAGE_DOS_HEADER     DOSHeader;
	PIMAGE_NT_HEADERS     NTHeader;

	DOSHeader = (PIMAGE_DOS_HEADER)lpImage;
	NTHeader = (PIMAGE_NT_HEADERS)(lpImage + DOSHeader->e_lfanew);

	return NTHeader->FileHeader.Characteristics & IMAGE_FILE_DEBUG_STRIPPED;
}

PCHAR PE_GetReloc(LPBYTE lpImage)
{
	PIMAGE_EXPORT_DIRECTORY pExportDirectory;
	IMAGE_SECTION_HEADER local;

	pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)PE_SectionFromDirectory(lpImage, IMAGE_DIRECTORY_ENTRY_EXPORT);

	if (pExportDirectory == NULL)
		return 0;

	if (PE_FindSec(lpImage, &local, const_cast<char*>(".rdata")))
	{
		return (char*)(lpImage + local.PointerToRawData +
			*(DWORD*)(lpImage + local.PointerToRawData + pExportDirectory->AddressOfNames - local.VirtualAddress)
			- local.VirtualAddress);
	}

	return NULL;
}

int PE_PopulateExportsLength(LPBYTE lpPortableExecutable)
{
	PIMAGE_EXPORT_DIRECTORY pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)PE_SectionFromDirectory(lpPortableExecutable, IMAGE_DIRECTORY_ENTRY_EXPORT);
	char* tabNames = 0, * startNames;

	if (pExportDirectory == NULL)
		return 0;

	tabNames = (char*)PE_GetReloc(lpPortableExecutable);
	startNames = tabNames;

	for (DWORD i = 0; i < pExportDirectory->NumberOfNames; ++i)
		tabNames += strlen(tabNames) + 1;

	return tabNames - startNames;
}

void PE_DumpExports(LPBYTE lpImage, char* buffer)
{
	PIMAGE_EXPORT_DIRECTORY pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)PE_SectionFromDirectory(lpImage, IMAGE_DIRECTORY_ENTRY_EXPORT);
	char* tabNames = 0, * startNames;

	if (pExportDirectory == NULL)
		return;

	tabNames = (char*)PE_GetReloc(lpImage);
	startNames = tabNames;

	for (DWORD i = 0; i < pExportDirectory->NumberOfNames; ++i)
		tabNames += strlen(tabNames) + 1;

	memcpy(buffer, startNames, tabNames - startNames);
}

int ParseDllExports(char* fname, char** pExportList)
{
	HFILE f;
	OFSTRUCT of{};
	HANDLE fh;
	LPBYTE lpFile;
	int size;

	f = OpenFile(fname, &of, 0);
	if (f == NULL)
		return NULL;

	fh = CreateFileMapping((HANDLE)f, 0, PAGE_READONLY, NULL, NULL, NULL);
	if (fh == NULL)
	{
		CloseHandle((HANDLE)f);
		return NULL;
	}

	lpFile = (LPBYTE)MapViewOfFile(fh, FILE_MAP_READ, NULL, NULL, NULL);

	if (lpFile == NULL)
	{
		CloseHandle(fh);
		CloseHandle((HANDLE)f);
		return NULL;
	}

	// обсчёт размера экспортов
	size = PE_PopulateExportsLength(lpFile);

	// если он ненулевой, то выделяем память и пишем найденное

	if (size)
	{
		*pExportList = (PCHAR)Hunk_TempAlloc(size);
		PE_DumpExports(lpFile, *pExportList);
	}
	else
	{
		*pExportList = 0;
	}

	UnmapViewOfFile(lpFile);
	CloseHandle(fh);
	CloseHandle((HANDLE)f);

	return size;
}

void DemangleFunction(char* pvFrom, char* pvTo, int cbMax)
{
	int i, count;

	for (i = 1, count = 0; i < cbMax && count < 2; i++)
	{
		pvTo[i - 1] = pvFrom[i];
		if (pvFrom[i] == '@')
			count++;
	}
	pvTo[i - 2] = '\0';

	if (i == cbMax)
		Con_DPrintf(const_cast<char*>("Export name is too large\n"));
}

void LoadDllExports(extensiondll_t* extdll, char* pszFileName)
{
	char* buffer, * effective, szDemangled[256], * extra;
	int size, cbDemangled, i, idx;

	if (!extdll->pDLLHandle || !pszFileName)
		return;

	size = ParseDllExports(pszFileName, &buffer);

	extdll->functionTable = NULL;
	extdll->functionCount = NULL;

	if (buffer == NULL)
		return;

	for (i = 0, cbDemangled = 0; i < size; i += strlen(effective) + 1)
	{
		effective = &buffer[i];
		// cpp-export? demangle it!
		if (buffer[i] == '?')
		{
			extdll->functionCount++;
			DemangleFunction(effective, szDemangled, sizeof(szDemangled));
			cbDemangled += strlen(szDemangled) + 1;
		}
	}

	extdll->functionTable = (functiontable_t*)Mem_Malloc(cbDemangled + extdll->functionCount * sizeof(functiontable_t));
	extra = (char*)&extdll->functionTable[extdll->functionCount];

	for (i = 0, idx = 0; i < size; i += strlen(effective) + 1)
	{
		effective = &buffer[i];
		// cpp-export? demangle it!
		if (buffer[i] == '?')
		{
			DemangleFunction(effective, extra, size - i);
			extdll->functionTable[idx].pFunctionName = extra;
			extdll->functionTable[idx].pFunction = (DWORD)GetProcAddress((HMODULE)extdll->pDLLHandle, effective);
			idx++;
			extra += strlen(extra) + 1;
		}
	}
}