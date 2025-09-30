#ifndef ENGINE_MEM_H
#define ENGINE_MEM_H

/**
*	@file
*
*	Heap allocation functions
*/

#ifdef _DEBUG
#define Mem_Malloc(size) Mem_Malloc_(size, __FILE__, __LINE__)
#define Mem_ZeroMalloc(size) Mem_ZeroMalloc_(size, __FILE__, __LINE__)
#define Mem_Realloc(ptr, size) Mem_Realloc_(ptr, size, __FILE__, __LINE__)
#define Mem_Calloc(nmemb, size) Mem_Calloc_(nmemb, size, __FILE__, __LINE__)
#define Mem_Strdup(strSource) Mem_Strdup_(strSource, __FILE__, __LINE__)
#define Mem_Free(ptr) Mem_Free_(ptr, __FILE__, __LINE__)

void* Mem_Malloc_(size_t size, const char* file, int line);
void* Mem_ZeroMalloc_(size_t size, const char* file, int line);
void* Mem_Realloc_(void* ptr, size_t size, const char* file, int line);
void* Mem_Calloc_(int nmemb, size_t size, const char* file, int line);
char* Mem_Strdup_(const char* strSource, const char* file, int line);
void Mem_Free_(void* ptr, const char* file, int line);
#else
void *Mem_Malloc( size_t size );
void *Mem_ZeroMalloc( size_t size );
void *Mem_Realloc( void *ptr, size_t size );
void *Mem_Calloc( int nmemb, size_t size );
char *Mem_Strdup( const char *strSource );
void Mem_Free( void *ptr );
#endif

#endif //ENGINE_MEM_H
