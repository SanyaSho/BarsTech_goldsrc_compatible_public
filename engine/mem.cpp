#include <cstdlib>
#include <cstring>

#include "mem.h"

#ifdef _DEBUG
#include <stdio.h>
//#include <vector>
//#include <utility>
//extern void DbgPrint(FILE* m_fLogStream, const char* format, ...);
//extern FILE* m_fMessages;
//
//std::vector<std::pair<void*, size_t>> regions;
//unsigned int total_allocated = 0;
//unsigned int current_allocated = 0;
//
//double computeTotalPercent(size_t size)
//{
//	return (double)size / (double)total_allocated;
//}
//
//double computeCurrentPercent(size_t size)
//{
//	return (double)size / (double)current_allocated;
//}

void dumpmem()
{
/*	DbgPrint(m_fMessages, "\r\nMEMORY DUMP:\r\n");
	for (int i = 0; i < regions.size(); i++)
	{
		DbgPrint(m_fMessages, "rg %08X - %08X, size %d\r\n", regions[i].first, (size_t)regions[i].first + regions[i].second, regions[i].second);
	}
	DbgPrint(m_fMessages, "\r\n");*/
}
#endif

#ifdef _DEBUG
void *Mem_Malloc_( size_t size, const char* file, int line)
#else
void* Mem_Malloc(size_t size)
#endif
{
#ifdef _DEBUG
	/*DbgPrint(m_fMessages, "trying to allocate size %d [%8.8lf perc. of total %u, %8.8lf perc. of current %u] at %s#%d\r\n", size, 
		computeTotalPercent(size), total_allocated, computeCurrentPercent(size), current_allocated, file, line);

	current_allocated += size;
	total_allocated += size;*/
	void* rg = malloc(size);
	//regions.push_back(std::make_pair(rg, size));
	//dumpmem();
	return rg;
#else
	return malloc( size );
#endif
}

#ifdef _DEBUG
void *Mem_ZeroMalloc_( size_t size, const char* file, int line)
#else
	void* Mem_ZeroMalloc(size_t size)
#endif
{
#ifdef _DEBUG
	//DbgPrint(m_fMessages, "trying to zero-allocate size %d [%8.8lf perc. of total %u, %8.8lf perc. of current %u] at %s#%d\r\n", size, 
	//	computeTotalPercent(size), total_allocated, computeCurrentPercent(size), current_allocated, file, line);

	//current_allocated += size;
	//total_allocated += size;
#endif
	void *result = malloc( size );

#ifdef _DEBUG
	//regions.push_back(std::make_pair(result, size));
	//dumpmem();
#endif

	memset( result, 0, size );

	return result;
}

#ifdef _DEBUG
void *Mem_Realloc_( void *ptr, size_t size, const char* file, int line)
#else
void * Mem_Realloc(void* ptr, size_t size)
#endif
{
#ifdef _DEBUG
	//DbgPrint(m_fMessages, "trying to reallocate rg %08X to size %d [%8.8lf perc. of total %u, %8.8lf perc. of current %u] at %s#%d\r\n", ptr, size, 
	//	computeTotalPercent(size), total_allocated, computeCurrentPercent(size), current_allocated, file, line);

	//current_allocated += size;
	//total_allocated += size;
	void* rg = realloc(ptr, size);

	/*for (int i = 0; i < regions.size(); i++)
	{
		if (regions[i].first == rg)
		{
			current_allocated -= regions[i].second;
			regions[i].second = size;
		}
	}
	dumpmem();*/

	return rg;
#else
	return realloc( ptr, size );
#endif
}

#ifdef _DEBUG
void *Mem_Calloc_( int nmemb, size_t size, const char* file, int line)
#else
void* Mem_Calloc(int nmemb, size_t size)
#endif
{
#ifdef _DEBUG
	/*DbgPrint(m_fMessages, "trying to alloc %d objects of %d size each [%8.8lf perc. of total %u, %8.8lf perc. of current %u] at %s#%d\r\n", nmemb, size, 
		computeTotalPercent(nmemb * size), total_allocated, computeCurrentPercent(nmemb * size), current_allocated, file, line);
	current_allocated += nmemb * size;
	total_allocated += nmemb * size;*/
	void* rg = calloc(nmemb, size);
	//regions.push_back(std::make_pair(rg, nmemb * size));
	//dumpmem();
	return rg;
#else
	return calloc( nmemb, size );
#endif
}

#ifdef _DEBUG
char *Mem_Strdup_( const char *strSource, const char* file, int line )
#else
char *Mem_Strdup( const char *strSource )
#endif
{
#ifdef _DEBUG
	//DbgPrint(m_fMessages, "trying to duplicate string %s [%8.8lf perc. of total %u, %8.8lf perc. of current %u] at %s#%d\r\n", strSource, 
	//	computeTotalPercent(strlen(strSource) + 1), total_allocated, computeCurrentPercent(strlen(strSource) + 1), current_allocated, file, line);
	char* rg = _strdup(strSource);

	//current_allocated += strlen(strSource) + 1;
	//total_allocated += strlen(strSource) + 1;
	//regions.push_back(std::make_pair((void*)rg, strlen(strSource) + 1));
	//dumpmem();

	return rg;
#else
	return _strdup( strSource );
#endif
}

#ifdef _DEBUG
void Mem_Free_( void *ptr, const char* file, int line )
#else
void Mem_Free( void *ptr )
#endif
{
#ifdef _DEBUG

	/*int i;
	for (i = 0; i < regions.size(); i++)
	{
		if (regions[i].first == ptr)
		{
			DbgPrint(m_fMessages, "trying to free rg %08X [%8.8lf perc. of total %u, %8.8lf perc. of current %u] at %s#%d\r\n", ptr, 
				computeTotalPercent(regions[i].second), total_allocated, computeCurrentPercent(regions[i].second), current_allocated, file, line);
			current_allocated -= regions[i].second;
			regions.erase(regions.begin() + i);
			break;
		}
	}
	dumpmem();*/
#endif
	free( ptr );
}
