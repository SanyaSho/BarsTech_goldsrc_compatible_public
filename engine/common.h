/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef ENGINE_COMMON_H
#define ENGINE_COMMON_H

#include <cstdio>

#include "tier0/platform.h"

/**
*	@file
*
*	general definitions
*/

enum FSB
{
	/**
	*	If not set, do a Sys_Error
	*/
	FSB_ALLOWOVERFLOW	=	1	<<	0,

	/**
	*	set if the buffer size failed
	*/
	FSB_OVERFLOWED		=	1	<<	1
};

typedef struct sizebuf_s
{
	const char* buffername;
	unsigned short flags;
	byte* data;
	int maxsize;
	int cursize;
} sizebuf_t;

void SZ_Alloc( const char* name, sizebuf_t* buf, int startsize );
void SZ_Clear( sizebuf_t* buf );
void* SZ_GetSpace( sizebuf_t* buf, int length );
void SZ_Write( sizebuf_t* buf, const void* data, int length );

/**
*	strcats onto the sizebuf
*/
void SZ_Print( sizebuf_t* buf, const char* data );

extern char gpszProductString[ 32 ];
extern char gpszVersionString[ 32 ];

extern char com_clientfallback[MAX_PATH];

extern int com_argc;
extern char** com_argv;
extern char com_gamedir[ FILENAME_MAX ];

extern char com_token[ 1024 * 2 ];

extern short (*BigShort)(short l);
extern short (*LittleShort)(short l);
extern int (*BigLong)(int l);
extern int (*LittleLong)(int l);
extern float (*BigFloat)(float l);
extern float (*LittleFloat)(float l);

void Q_memset(void *dest, int fill, int count);
void Q_memcpy(void *dest, const void *src, int count);
int Q_memcmp(void *m1, void *m2, int count);
void Q_strcpy(char *dest, const char *src);
void Q_strncpy(char *dest, const char *src, int count);
int Q_strlen(const char *str);
char *Q_strrchr(char *s, char c);
void Q_strcat(char *dest, char *src);
int Q_strcmp(const char *s1, const char *s2);
int Q_strncmp(const char *s1, const char *s2, int count);
int Q_stricmp(const char* s1, const char* s2);
int Q_strnicmp(const char* s1, const char* s2, int n);
int Q_strcasecmp(const char *s1, const char *s2);
int Q_strncasecmp(const char *s1, const char *s2, int n);
int	Q_atoi(const char *str);
float Q_atof(const char *str);
char* Q_strlwr(char* s1);
char* Q_strstr(const char* s1, const char* search);


/**
*	If true, colons are treated as regular characters, instead of being parsed as single characters.
*/
extern bool com_ignorecolons;

void COM_UngetToken();


extern void COM_Munge(unsigned char* data, int len, int seq);
extern void COM_UnMunge(unsigned char* data, int len, int seq);
extern void COM_Munge2(unsigned char* data, int len, int seq);
extern void COM_UnMunge2(unsigned char* data, int len, int seq);
extern void COM_Munge3(unsigned char* data, int len, int seq);
extern void COM_UnMunge3(unsigned char* data, int len, int seq);

int    LongSwap(int l);

/**
*	Parse a token out of a string
*/
char *COM_Parse( char *data );

/**
*	Parse a line out of a string. Used to parse out lines out of cfg files
*/
char* COM_ParseLine( char* data );

int COM_TokenWaiting( char* buffer );

/**
*	Returns the position (1 to argc-1) in the program's argument list
*	where the given parameter apears, or 0 if not present
*/
int COM_CheckParm( char* parm );
void COM_InitArgv( int argc, char** argv );
void COM_Init();
void COM_Shutdown();

void COM_FileBase( char *in, char *out );
#ifdef _2020_PATCH
char* COM_FileBase_s(char* in, char* out, int size);
#endif

bool COM_SetupDirectories();

#ifdef _2020_PATCH
void COM_ParseDirectoryFromCmd(const char* pCmdName, char* pDirName, int nLen, const char* pDefault);
#else
void COM_ParseDirectoryFromCmd(const char* pCmdName, char* pDirName, const char* pDefault);
#endif

void COM_FixSlashes( char *pname );

void COM_AddDefaultDir( char* pszDir );

void COM_AddAppDirectory(char* pszBaseDir, const char* appName);

char* COM_FileExtension( char* in );

void COM_DefaultExtension( char* path, char* extension );

void COM_StripExtension( char* in, char* out );

void COM_StripTrailingSlash( char* ppath );

qboolean COM_HasFileExtension(char *in, char *ext);

char *COM_LastFileExtension(char *in);

void COM_AddExtension(char *path, char *extension, int size);

/**
*	Creates a hierarchy of directories specified by path
*	Modifies the given string while performing this operation, but restores it to its original state
*/
void COM_CreatePath( char* path );

unsigned int COM_GetApproxWavePlayLength( const char* filepath );

char* Info_Serverinfo();

/**
*	does a varargs printf into a temp buffer, so I don't need to have
*	varargs versions of all text functions.
*	FIXME: make this buffer size safe someday
*/
char* va( char* format, ... );

/**
*	Converts a vector to a string representation.
*/
char* vstr( vec_t* v );

/**
*	Searches for a byte of data in a binary buffer
*/
int memsearch( byte* start, int count, int search );

/**
*	Compares filenames
*	@return -1 if file1 is not equal to file2, 0 otherwise
*/
int Q_FileNameCmp( char* file1, char* file2 );

byte* COM_LoadFile( char* path, int usehunk, int* pLength );

void COM_FreeFile( void *buffer );

byte* COM_LoadHunkFile( char* path );

byte* COM_LoadTempFile( char* path, int* pLength );

void COM_LoadCacheFile( char* path, cache_user_t* cu );

byte* COM_LoadStackFile( char* path, void* buffer, int bufsize, int* length );

byte* COM_LoadFileForMe( char* filename, int* pLength );

byte* COM_LoadFileLimit( char* path, int pos, int cbmax, int* pcbread, FileHandle_t* phFile );

void COM_WriteFile( char* filename, void* data, int len );

int COM_FileSize( char* filename );

bool COM_ExpandFilename( char* filename );

int COM_CompareFileTime( char* filename1, char* filename2, int* iCompare );

/**
*	@param cachepath Modified by the function but restored before returning
*/
void COM_CopyFile( char* netpath, char* cachepath );

void COM_CopyFileChunk( FileHandle_t dst, FileHandle_t src, int nSize );

void COM_Log( char* pszFile, char* fmt, ... );

void COM_ListMaps( char* pszSubString );

void COM_GetGameDir( char* szGameDir );

char* COM_SkipPath( char* pathname );

char* COM_BinPrintf( byte* buf, int nLen );

unsigned char COM_Nibble( char c );

void COM_HexConvert( const char* pszInput, int nInputLength, byte* pOutput );

/**
*	Normalizes the angles to a range of [ -180, 180 ]
*/
void COM_NormalizeAngles( vec_t* angles );

int COM_EntsForPlayerSlots( int nPlayers );

/**
*	Set explanation for disconnection
*	@param bPrint Whether to print the explanation to the console
*/
void COM_ExplainDisconnection( qboolean bPrint, char* fmt, ... );

/**
*	Set extended explanation for disconnection
*	Only used if COM_ExplainDisconnection has been called as well
*	@param bPrint Whether to print the explanation to the console
*/
void COM_ExtendedExplainDisconnection( qboolean bPrint, char* fmt, ... );

void MSG_WriteChar( sizebuf_t *sb, int c );
void MSG_WriteByte( sizebuf_t *sb, int c );
void MSG_WriteShort( sizebuf_t *sb, int c );
void MSG_WriteWord( sizebuf_t* sb, int c );
void MSG_WriteLong( sizebuf_t *sb, int c );
void MSG_WriteFloat( sizebuf_t *sb, float f );
void MSG_WriteString( sizebuf_t *sb, const char *s );
void MSG_WriteCoord( sizebuf_t *sb, float f );
void MSG_WriteAngle( sizebuf_t *sb, float f );
void MSG_WriteHiresAngle(sizebuf_t *sb, float f);
void MSG_WriteUsercmd(sizebuf_t* buf, usercmd_t* to, usercmd_t* from);

void MSG_WriteOneBit(int nValue);
void MSG_StartBitWriting(sizebuf_t *buf);
void MSG_EndBitWriting(sizebuf_t *buf);
void MSG_WriteBits(uint32 data, int numbits);
qboolean MSG_IsBitWriting(void);
void MSG_WriteSBits(int data, int numbits);
void MSG_WriteBitString(const char *p);
void MSG_WriteBitData(void *src, int length);
void MSG_WriteBitAngle(float fAngle, int numbits);
float MSG_ReadBitAngle(int numbits);
int MSG_CurrentBit(void);
qboolean MSG_IsBitReading(void);
void MSG_StartBitReading(sizebuf_t *buf);
void MSG_EndBitReading(sizebuf_t *buf);
int MSG_ReadOneBit(void);
uint32 MSG_ReadBits(int numbits);
uint32 MSG_PeekBits(int numbits);
int MSG_ReadSBits(int numbits);
char *MSG_ReadBitString(void);
int MSG_ReadBitData(void *dest, int length);
float MSG_ReadBitCoord(void);
void MSG_WriteBitCoord(const float f);
void MSG_ReadBitVec3Coord(vec_t *fa);
void MSG_WriteBitVec3Coord(const vec_t *fa);
float MSG_ReadHiresAngle(void);
void MSG_SkipString();
void MSG_WriteBuf(sizebuf_t *sb, int iSize, void *buf);

extern int msg_readcount;

/**
*	set if a read goes beyond end of message
*/
extern bool msg_badread;

void MSG_BeginReading();
int MSG_ReadChar();
int MSG_ReadByte();
int MSG_ReadShort();
int MSG_ReadWord();
int MSG_ReadLong();
float MSG_ReadFloat();
char *MSG_ReadString();
int MSG_ReadBuf(int iSize, void *pbuf);
float MSG_ReadCoord(sizebuf_t* sb);
float MSG_ReadAngle();
char *MSG_ReadStringLine(void);
void MSG_ReadUsercmd(usercmd_t *to, usercmd_t* from);

const char *Q_stristr(const char *pStr, const char *pSearch);
void Q_strncpy_s(char *dest, const char *src, int count);

#endif //ENGINE_COMMON_H
