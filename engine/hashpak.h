#ifndef ENGINE_HASHPAK_H
#define ENGINE_HASHPAK_H

#include "custom.h"

#define HASHPAK_EXTENSION	".hpk"
#define HASHPAK_VERSION		1

// 128 KiB
#define MAX_FILE_SIZE		131072
#define MAX_FILE_ENTRIES	32768

typedef struct hash_pack_queue_s
{
	char *pakname;
	resource_t resource;
	int datasize;
	void *data;
	struct hash_pack_queue_s *next;
} hash_pack_queue_t;

typedef struct hash_pack_entry_s
{
	resource_t resource;
	int nOffset;
	int nFileLength;
} hash_pack_entry_t;

typedef struct hash_pack_directory_s
{
	int nEntries;
	hash_pack_entry_t *p_rgEntries;
} hash_pack_directory_t;

typedef struct hash_pack_header_s
{
	char szFileStamp[4];
	int version;
	int nDirectoryOffset;
} hash_pack_header_t;

extern hash_pack_queue_t *gp_hpak_queue;
extern hash_pack_directory_t hash_pack_dir;
extern hash_pack_header_t hash_pack_header;

void HPAK_FlushHostQueue();

void HPAK_Init();

void HPAK_CheckIntegrity( const char* pakname );

void HPAK_AddLump(qboolean bUseQueue, char* pakname, resource_t* pResource, void* pData, FileHandle_t fpSource );

int HPAK_ResourceForHash( char* pakname, byte* hash, resource_t* pResourceEntry );

void HPAK_CheckSize(char *pakname);
qboolean HPAK_GetDataPointer(char *pakname, resource_s *pResource, unsigned char **pbuffer, int *bufsize);
void HPAK_RemoveLump(char *pakname, resource_t *pResource);

#endif //ENGINE_HASHPAK_H
