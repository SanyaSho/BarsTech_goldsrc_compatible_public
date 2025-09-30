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
// common.c -- misc functions used in client and server
#include <cctype>
#include <cstdarg>
#include <cstdio>

#include "quakedef.h"
#include "bspfile.h"
#include "cdll_int.h"
#include "modinfo.h"
#include "delta.h"
#include "sv_main.h"

char gpszProductString[ 32 ] = {};
char gpszVersionString[ 32 ] = {};

void SZ_Alloc( const char* name, sizebuf_t* buf, int startsize )
{
	if( startsize < 256 )
		startsize = 256;

	buf->buffername = name;
	buf->data = reinterpret_cast<byte*>( Hunk_AllocName( startsize, const_cast<char*>("sizebuf") ) );
	buf->maxsize = startsize;
	buf->cursize = 0;
	buf->flags = 0;
}

void SZ_Clear( sizebuf_t* buf )
{
	buf->cursize = 0;
	buf->flags &= ~FSB_OVERFLOWED;
}

void* SZ_GetSpace( sizebuf_t* buf, int length )
{
	if( buf->cursize + length > buf->maxsize )
	{
		const char* name = buf->buffername;

		if( !name )
			name = "???";

		if( !( buf->flags & FSB_ALLOWOVERFLOW ) )
		{
			if( buf->maxsize == 0 )
			{
				Sys_Error( "SZ_GetSpace:  Tried to write to an uninitialized sizebuf_t: %s", name );
			}

			Sys_Error( "SZ_GetSpace: overflow without FSB_ALLOWOVERFLOW set on %s", name );
		}

		if( length > buf->maxsize )
		{
			if( !( buf->flags & FSB_ALLOWOVERFLOW ) )
			{
				Sys_Error( "SZ_GetSpace: %i is > full buffer size on %s", length, name );
			}

			Con_DPrintf( const_cast<char*>("SZ_GetSpace: %i is > full buffer size on %s, ignoring"), length, name );
		}

		Con_Printf( const_cast<char*>("SZ_GetSpace: overflow on %s\n"), name );

		buf->cursize = length;
		buf->flags |= FSB_OVERFLOWED;

		return buf->data;
	}

	void* data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write( sizebuf_t* buf, const void* data, int length )
{
	byte* pSpace = reinterpret_cast<byte*>( SZ_GetSpace( buf, length ) );

	if( buf->flags & FSB_OVERFLOWED )
		return;

	Q_memcpy( pSpace, (void*)data, length );
}

void SZ_Print( sizebuf_t* buf, const char* data )
{
	const int len = Q_strlen( (char*)data ) + 1;

	// byte* cast to keep VC++ happy
	byte* pSpace = buf->data[ buf->cursize - 1 ] ? 
		reinterpret_cast<byte*>( SZ_GetSpace( buf, len ) ) :		// no trailing 0
		reinterpret_cast<byte*>( SZ_GetSpace( buf, len - 1 ) ) - 1;	// write over trailing 0

	if( buf->flags & FSB_OVERFLOWED )
		return;

	Q_memcpy( pSpace, (void*)data, len );
}

#define NUM_SAFE_ARGVS  7

static char* largv[ MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1 ];
static char* argvdummy = const_cast<char*>(" ");

static char* const safeargvs[ NUM_SAFE_ARGVS ] =
{ const_cast<char*>("-stdvid"), const_cast<char*>("-nolan"), const_cast<char*>("-nosound"), const_cast<char*>("-nocdaudio"), const_cast<char*>("-nojoy"), const_cast<char*>("-nomouse"), const_cast<char*>("-dibonly") };

int		com_argc = 0;
char** com_argv = nullptr;

#define CMDLINE_LENGTH	256
char	com_cmdline[ CMDLINE_LENGTH ];

char com_clientfallback[ FILENAME_MAX ] = {};
char com_gamedir[ FILENAME_MAX ] = {};

char com_token[ 1024 * 2 ] = {};

static bool s_com_token_unget = false;

bool com_ignorecolons = false;

/*
============================================================================

LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

//#if 0
void Q_memset(void *dest, int fill, int count)
{
	int		i;

	if ((((uintptr_t)dest | count) & 3) == 0)
	{
		count >>= 2;
		fill = fill | (fill << 8) | (fill << 16) | (fill << 24);
		for (i = 0; i<count; i++)
			((int *)dest)[i] = fill;
	}
	else
	for (i = 0; i<count; i++)
		((byte *)dest)[i] = fill;
}

void Q_memcpy(void *dest, const void *src, int count)
{
	int		i;

	if ((((uintptr_t)dest | (uintptr_t)src | count) & 3) == 0)
	{
		count >>= 2;
		for (i = 0; i<count; i++)
			((int *)dest)[i] = ((int *)src)[i];
	}
	else
	for (i = 0; i<count; i++)
		((byte *)dest)[i] = ((byte *)src)[i];
}

int Q_memcmp(void *m1, void *m2, int count)
{
	while (count)
	{
		count--;
		if (((byte *)m1)[count] != ((byte *)m2)[count])
			return -1;
	}
	return 0;
}

void Q_strcpy(char *dest, const char *src)
{
	while (*src)
	{
		*dest++ = *src++;
	}
	*dest++ = 0;
}

void Q_strncpy(char *dest, const char *src, int count)
{
	while (*src && count--)
	{
		*dest++ = *src++;
	}
	if (count)
		*dest++ = 0;
}

int Q_strlen(const char *str)
{
	int		count;

	count = 0;
	while (str[count])
		count++;

	return count;
}

char *Q_strrchr(char *s, char c)
{
	int len = Q_strlen(s);
	s += len;
	while (len--)
	if (*--s == c) return (char*)s;
	return 0;
}

void Q_strcat(char *dest, char *src)
{
	dest += Q_strlen(dest);
	Q_strcpy(dest, src);
}

int Q_strcmp(const char *s1, const char *s2)
{
	while (1)
	{
		if (*s1 != *s2)
			return -1;		// strings not equal	
		if (!*s1)
			return 0;		// strings are equal
		s1++;
		s2++;
	}

	return -1;
}

char* Q_strlwr(char* s1) {
	char* s;

	s = s1;
	while (*s) {
		*s = tolower(*s);
		s++;
	}
	return s1;
}

char* Q_strstr(const char* s1, const char* search)
{
	return (char*)strstr(s1, search);
}

int Q_strncmp(const char *s1, const char *s2, int count)
{
	while (1)
	{
		if (!count--)
			return 0;
		if (*s1 != *s2)
			return -1;		// strings not equal	
		if (!*s1)
			return 0;		// strings are equal
		s1++;
		s2++;
	}

	return -1;
}

int Q_strncasecmp(const char *s1, const char *s2, int n)
{
	int		c1, c2;

	while (1)
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		// strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;		// strings not equal
		}
		if (!c1)
			return 0;		// strings are equal
		//		s1++;
		//		s2++;
	}

	return -1;
}

int Q_strnicmp(const char* s1, const char* s2, int n)
{
	return Q_strncasecmp(s1, s2, n);
}

int Q_strcasecmp(const char *s1, const char *s2)
{
	return Q_strncasecmp(s1, s2, 99999);
}

int Q_stricmp(const char *s1, const char *s2)
{
	return Q_strcasecmp(s1, s2);
}

//#endif

int Q_atoi(const char *str)
{
	int		val;
	int		sign;
	int		c;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;

	val = 0;

	//
	// check for hex
	//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val << 4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val << 4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val << 4) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

	//
	// check for character
	//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}

	//
	// assume decimal
	//
	while (1)
	{
		c = *str++;
		if (c <'0' || c > '9')
			return val*sign;
		val = val * 10 + c - '0';
	}

	return 0;
}


float Q_atof(const char *str)
{
	double	val;
	int		sign;
	int		c;
	int		decimal, total;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;

	val = 0;

	//
	// check for hex
	//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val * 16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val * 16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val * 16) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

	//
	// check for character
	//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}

	//
	// assume decimal
	//
	decimal = -1;
	total = 0;
	while (1)
	{
		c = *str++;
		if (c == '.')
		{
			decimal = total;
			continue;
		}
		if (c <'0' || c > '9')
			break;
		val = val * 10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val*sign;
	while (total > decimal)
	{
		val /= 10;
		total--;
	}

	return val*sign;
}


const unsigned char mungify_table[] =
{
	0x7A, 0x64, 0x05, 0xF1,
	0x1B, 0x9B, 0xA0, 0xB5,
	0xCA, 0xED, 0x61, 0x0D,
	0x4A, 0xDF, 0x8E, 0xC7
};

const unsigned char mungify_table2[] =
{
	0x05, 0x61, 0x7A, 0xED,
	0x1B, 0xCA, 0x0D, 0x9B,
	0x4A, 0xF1, 0x64, 0xC7,
	0xB5, 0x8E, 0xDF, 0xA0
};

unsigned char mungify_table3[] =
{
	0x20, 0x07, 0x13, 0x61,
	0x03, 0x45, 0x17, 0x72,
	0x0A, 0x2D, 0x48, 0x0C,
	0x4A, 0x12, 0xA9, 0xB5
};


void COM_UngetToken()
{
	s_com_token_unget = true;
}

// Anti-proxy/aimbot obfuscation code
// COM_UnMunge should reversably fixup the data
void COM_Munge(unsigned char* data, int len, int seq)
{
	int i;
	int mungelen;
	int c;
	int* pc;
	unsigned char* p;
	int j;

	mungelen = len & ~3;
	mungelen /= 4;

	for (i = 0; i < mungelen; i++)
	{
		pc = (int*)&data[i * 4];
		c = *pc;
		c ^= ~seq;
		c = LongSwap(c);

		p = (unsigned char*)&c;
		for (j = 0; j < 4; j++)
		{
			*p++ ^= (0xa5 | (j << j) | j | mungify_table[(i + j) & 0x0f]);
		}

		c ^= seq;
		*pc = c;
	}
}

void COM_UnMunge(unsigned char* data, int len, int seq)
{
	int i;
	int mungelen;
	int c;
	int* pc;
	unsigned char* p;
	int j;

	mungelen = len & ~3;
	mungelen /= 4;

	for (i = 0; i < mungelen; i++)
	{
		pc = (int*)&data[i * 4];
		c = *pc;
		c ^= seq;

		p = (unsigned char*)&c;
		for (j = 0; j < 4; j++)
		{
			*p++ ^= (0xa5 | (j << j) | j | mungify_table[(i + j) & 0x0f]);
		}

		c = LongSwap(c);
		c ^= ~seq;
		*pc = c;
	}
}


void COM_Munge2(unsigned char* data, int len, int seq)
{
	int i;
	int mungelen;
	int c;
	int* pc;
	unsigned char* p;
	int j;

	mungelen = len & ~3;
	mungelen /= 4;

	for (i = 0; i < mungelen; i++)
	{
		pc = (int*)&data[i * 4];
		c = *pc;
		c ^= ~seq;
		c = LongSwap(c);

		p = (unsigned char*)&c;
		for (j = 0; j < 4; j++)
		{
			*p++ ^= (0xa5 | (j << j) | j | mungify_table2[(i + j) & 0x0f]);
		}

		c ^= seq;
		*pc = c;
	}
}

void COM_UnMunge2(unsigned char* data, int len, int seq)
{
	int i;
	int mungelen;
	int c;
	int* pc;
	unsigned char* p;
	int j;

	mungelen = len & ~3;
	mungelen /= 4;

	for (i = 0; i < mungelen; i++)
	{
		pc = (int*)&data[i * 4];
		c = *pc;
		c ^= seq;

		p = (unsigned char*)&c;
		for (j = 0; j < 4; j++)
		{
			*p++ ^= (0xa5 | (j << j) | j | mungify_table2[(i + j) & 0x0f]);
		}

		c = LongSwap(c);
		c ^= ~seq;
		*pc = c;
	}
}

void COM_Munge3(unsigned char* data, int len, int seq)
{
	int i;
	int mungelen;
	int c;
	int* pc;
	unsigned char* p;
	int j;

	mungelen = len & ~3;
	mungelen /= 4;

	for (i = 0; i < mungelen; i++)
	{
		pc = (int*)&data[i * 4];
		c = *pc;
		c ^= ~seq;
		c = LongSwap(c);

		p = (unsigned char*)&c;
		for (j = 0; j < 4; j++)
		{
			*p++ ^= (0xa5 | (j << j) | j | mungify_table3[(i + j) & 0x0f]);
		}

		c ^= seq;
		*pc = c;
	}
}

void COM_UnMunge3(unsigned char* data, int len, int seq)
{
	int i;
	int mungelen;
	int c;
	int* pc;
	unsigned char* p;
	int j;

	mungelen = len & ~3;
	mungelen /= 4;

	for (i = 0; i < mungelen; i++)
	{
		pc = (int*)&data[i * 4];
		c = *pc;
		c ^= seq;

		p = (unsigned char*)&c;
		for (j = 0; j < 4; j++)
		{
			*p++ ^= (0xa5 | (j << j) | j | mungify_table3[(i + j) & 0x0f]);
		}

		c = LongSwap(c);
		c ^= ~seq;
		*pc = c;
	}
}


//Updated version of COM_Parse from Quake:
//Allows retrieving the last token by calling COM_UngetToken to mark it
//Supports Unicode
//Has buffer overflow checks
//Allows colons to be treated as regular characters using com_ignorecolons
char* COM_Parse(char* data)
{
	int c;
	uchar32 wchar;
	int len;

	if (s_com_token_unget)
	{
		s_com_token_unget = 0;
		return data;
	}

	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		return NULL;
	}

skipwhite:
	// skip whitespace
	while (!V_UTF8ToUChar32(data, wchar) && wchar <= 32)
	{
		if (!wchar)
			return NULL;
		data = Q_UnicodeAdvance(data, 1);
	}

	c = *data;

	// skip // comments till the next line
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;	// start over new line
	}

	// handle quoted strings specially: copy till the end or another quote
	if (c == '\"')
	{
		data++;	// skip starting quote
		while (true)
		{
		inquotes:
			c = *data++;	// get char and advance
			if (!c)	// EOL
			{
				com_token[len] = 0;
				return data - 1;	// we are done with that, but return data to show that token is present
			}
			if (c == '\"')	// closing quote
			{
				com_token[len] = 0;
				return data;
			}

			com_token[len] = c;
			len++;

			if (len == (ARRAYSIZE(com_token) - 1))	// check if buffer is full
			{
				com_token[len] = 0;
				return data;
			}
		}
	}

	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' || (!com_ignorecolons && c == ':'))
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' || (!com_ignorecolons && c == ':'))
			break;
	} while (len < (ARRAYSIZE(com_token) - 1) && (c < 0 || c > 32));

	com_token[len] = 0;
	return data;
}

char* COM_ParseLine( char* data )
{
	if( s_com_token_unget )
	{
		s_com_token_unget = false;

		return data;
	}

	com_token[ 0 ] = 0;

	if( !data )
		return nullptr;

	int len = 0;

	char c = *data;

	do
	{
		com_token[ len ] = c;
		data++;
		len++;
		c = *data;
	}
	while( c>=' ' && len < ( ARRAYSIZE( com_token ) - 1 ) );

	com_token[ len ] = 0;

	//Skip unprintable characters.
	while( *data && *data < ' ' )
	{
		++data;
	}

	//End of data.
	if( *data == '\0' )
		return nullptr;

	return data;
}

int COM_TokenWaiting( char* buffer )
{
	for( auto p = buffer; *p && *p != '\n'; ++p )
	{
		if( !isspace( *p ) || isalnum( *p ) )
			return true;
	}

	return false;
}

int COM_CheckParm( char* parm )
{
	for( int i = 1; i<com_argc; i++ )
	{
		if( !com_argv[ i ] )
			continue;               // NEXTSTEP sometimes clears appkit vars.
		if( !Q_strcmp( (char*)parm, (char*)com_argv[ i ] ) )
			return i;
	}

	return 0;
}

void COM_InitArgv( int argc, char** argv )
{
	int             i, n;

	// reconstitute the command line for the cmdline externally visible cvar
	n = 0;

	for( int j = 0; ( j<MAX_NUM_ARGVS ) && ( j< argc ); j++ )
	{
		i = 0;

		while( ( n < ( CMDLINE_LENGTH - 1 ) ) && argv[ j ][ i ] )
		{
			com_cmdline[ n++ ] = argv[ j ][ i++ ];
		}

		if( n < ( CMDLINE_LENGTH - 1 ) )
			com_cmdline[ n++ ] = ' ';
		else
			break;
	}

	com_cmdline[ n ] = 0;

	bool safe = false;

	for( com_argc = 0; ( com_argc<MAX_NUM_ARGVS ) && ( com_argc < argc );
		 com_argc++ )
	{
		largv[ com_argc ] = argv[ com_argc ];
		if( !Q_strcmp( "-safe", (char*)argv[ com_argc ] ) )
			safe = true;
	}

	if( safe )
	{
		// force all the safe-mode switches. Note that we reserved extra space in
		// case we need to add these, so we don't need an overflow check
		for( i = 0; i<NUM_SAFE_ARGVS; i++ )
		{
			largv[ com_argc ] = safeargvs[ i ];
			com_argc++;
		}
	}

	largv[ com_argc ] = argvdummy;
	com_argv = largv;
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

qboolean bigendien;
short (*BigShort)(short l);
short (*LittleShort)(short l);
int (*BigLong)(int l);
int (*LittleLong)(int l);
float (*BigFloat)(float l);
float (*LittleFloat)(float l);

short   ShortSwap(short l)
{
	byte    b1, b2;

	b1 = l & 255;
	b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

short   ShortNoSwap(short l)
{
	return l;
}

int    LongSwap(int l)
{
	byte    b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

int     LongNoSwap(int l)
{
	return l;
}

float FloatSwap(float f)
{
	union
	{
		float   f;
		byte    b[4];
	} dat1, dat2;


	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap(float f)
{
	return f;
}


typedef struct bf_write_s
{
	int nCurOutputBit;
	unsigned char* pOutByte;
	sizebuf_t* pbuf;
} bf_write_t;

typedef struct bf_read_s
{
	int nMsgReadCount;	// was msg_readcount
	sizebuf_t* pbuf;
	int nBitFieldReadStartByte;
	int nBytesRead;
	int nCurInputBit;
	unsigned char* pInByte;
} bf_read_t;

// Bit field reading/writing storage.
bf_read_t bfread;
ALIGN16 bf_write_t bfwrite;


void COM_BitOpsInit(void)
{
	Q_memset(&bfwrite, 0, sizeof(bf_write_t));
	Q_memset(&bfread, 0, sizeof(bf_read_t));
}


void COM_Init()
{
	unsigned short swaptest = 1;

	if (*(byte*)&swaptest == 1)
	{
		bigendien = 0;
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = 1;
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}

	COM_BitOpsInit();

}

void COM_Shutdown()
{
	//Nothing
}

#ifdef _2020_PATCH

void COM_FileBase(char* in, char* out)
{
	COM_FileBase_s(in, out, -1);
}

char* COM_FileBase_s(char* in, char* out, int size)
{
	if (!in || !in[0])
	{
		*out = '\0';
		return NULL;
	}

	int len = Q_strlen(in);
	if (len <= 0)
		return NULL;

	// scan backward for '.'
	int end = len - 1;
	while (end && in[end] != '.' && !(in[end] == '\\' || in[end] == '/'))
		end--;

	// no '.', copy to end
	if (in[end] != '.')
	{
		end = len - 1;
	}
	else
	{
		// Found ',', copy to left of '.'
		end--;
	}

	// Scan backward for '/'
	int start = len - 1;
	while (start >= 0 && !(in[start] == '\\' || in[start] == '/'))
		start--;

	if (start < 0 || !(in[start] == '\\' || in[start] == '/'))
	{
		start = 0;
	}
	else
	{
		start++;
	}

	// Length of new sting
	int maxcopy = end - start + 1;
	if (size >= 0 && maxcopy >= size)
		return NULL;

	// Copy partial string
	Q_strncpy(out, &in[start], maxcopy);
	out[maxcopy] = '\0';
	return out;
}
#else
void COM_FileBase(char* in, char* out)
{
	if (!in)
		return;

	if (!*in)
		return;

	size_t uiLength = strlen(in);

	// scan backward for '.'
	size_t end = uiLength - 1;
	while (end && in[end] != '.' && in[end] != '/' && in[end] != '\\')
		--end;

	if (in[end] != '.')	// no '.', copy to end
		end = uiLength - 1;
	else
		--end;				// Found ',', copy to left of '.'


	// Scan backward for '/'
	size_t start = uiLength;
	while (start > 0 && in[start - 1] != '/' && in[start - 1] != '\\')
		--start;

	// Length of new string
	uiLength = end - start + 1;

	// Copy partial string
	strncpy(out, &in[start], uiLength);
	// Terminate it
	out[uiLength] = '\0';
}
#endif

bool COM_SetupDirectories()
{
	com_clientfallback[ 0 ] = '\0';
	com_gamedir[ 0 ] = '\0';

	char basedir[ 512 ];

	COM_ParseDirectoryFromCmd( "-basedir", basedir, "valve" );
	COM_ParseDirectoryFromCmd( "-game", com_gamedir, basedir );

	bool bResult = FileSystem_SetGameDirectory( basedir, com_gamedir[ 0 ] ? com_gamedir : nullptr );

	if( bResult )
	{
		//TODO: serverinfo is only 256 characters large, but 512 is passed in. - Solokiller
		Info_SetValueForStarKey( serverinfo, "*gamedir", com_gamedir, 512 );
	}

	return bResult;
}

void COM_ParseDirectoryFromCmd( const char *pCmdName, char *pDirName, const char *pDefault )
{
	const char* pszResult = nullptr;

	if( com_argc > 1 )
	{
		for( int arg = 1; arg < com_argc; ++arg )
		{
			auto pszArg = com_argv[ arg ];

			if( pszArg )
			{
				if( pCmdName && *pCmdName == *pszArg )
				{
					if( *pCmdName && !strcmp( pCmdName, pszArg ) )
					{
						if( arg < com_argc - 1 )
						{
							auto pszValue = com_argv[ arg + 1 ];

							if( *pszValue != '+' && *pszValue != '-' )
							{
								strcpy( pDirName, pszValue );
								pszResult = pDirName;
								break;
							}
						}
					}
				}
			}
		}
	}

	if( !pszResult )
	{
		if( pDefault )
		{
			strcpy( pDirName, pDefault );
			pszResult = pDirName;
		}
		else
		{
			pszResult = pDirName;
			*pDirName = '\0';
		}
	}

	const auto uiLength = strlen( pszResult );

	if( uiLength > 0 )
	{
		auto pszEnd = &pDirName[ uiLength - 1 ];
		if( *pszEnd == '/' || *pszEnd == '\\' )
			*pszEnd = '\0';
	}
}

void COM_FixSlashes( char *pname )
{
	for( char* pszNext = pname; *pszNext; ++pszNext )
	{
		if( *pszNext == '\\' )
			*pszNext = '/';
	}
}

void COM_AddDefaultDir( char* pszDir )
{
	if( pszDir && *pszDir )
	{
		FileSystem_AddFallbackGameDir( pszDir );
	}
}

void COM_AddAppDirectory(char* pszBaseDir, const char* appName)
{
	FS_AddSearchPath( pszBaseDir, "PLATFORM" );
}

char* COM_FileExtension( char* in )
{
	static char exten[ 8 ];

	for( char* pszExt = in; *pszExt; pszExt++ )
	{
		if( *pszExt == '.' )
		{
			//Skip the '.'
			pszExt++;

			size_t uiIndex;

			for( uiIndex = 0; *pszExt && uiIndex < ARRAYSIZE( exten ) - 1; pszExt++, uiIndex++ )
			{
				exten[ uiIndex ] = *pszExt;
			}

			exten[ uiIndex ] = '\0';
			return exten;
		}
	}

	return const_cast<char*>("");
}

void COM_DefaultExtension( char* path, char* extension )
{
	//
	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	//
	char* src = path + strlen( path ) - 1;

	while( *src != '/' && src != path )
	{
		if( *src == '.' )
			return;                 // it has an extension
		src--;
	}

	strncat( path, extension, MAX_PATH - strlen( src ) );
}

void COM_StripExtension( char* in, char* out )
{
	char* c, * d = NULL;
	int i;

	// Search for the first dot after the last path separator
	c = in;
	while (*c)
	{
		if (*c == '/' || *c == '\\')
		{
			d = NULL;	// reset dot location on path separator
		}
		else if (d == NULL && *c == '.')
		{
			d = c;		// store first dot location in the file name
		}
		c++;
	}

	if (out == in)
	{
		if (d != NULL)
		{
			*d = 0;
		}
	}
	else
	{
		if (d != NULL)
		{
			i = d - in;
			Q_memcpy(out, in, i);
			out[i] = 0;
		}
		else
		{
			Q_strcpy(out, in);
		}
	}
}

char *COM_LastFileExtension(char *in)
{
	char *psz;
	int pos = -1;

	if (!in)
		return const_cast<char*>("");

	if (!in[0])
		return const_cast<char*>("");

	psz = in;
	do
	{
		if (*psz == '.')
			pos = psz - in;
	} while (*++psz);

	if (pos >= 0)
		return const_cast<char*>(COM_FileExtension(&in[pos]));
	else
		return const_cast<char*>("");
}

qboolean COM_HasFileExtension(char *in, char *ext)
{
	char *psz;

	if (!in)
		return false;

	if (!ext)
		return false;

	psz = ext;

	if (ext[0] == '.')
		psz = &ext[1];

	return !Q_strcmp(COM_LastFileExtension(in), psz);
}

void COM_AddExtension(char *path, char *extension, int size)
{
	size_t n;

	if (extension && path && size > 0 && !COM_HasFileExtension(path, extension))
	{
		n = size - 1;
		if (extension[0] != '.')
			strncat(path, ".", n);
		strncat(path, extension, n);
	}
}

void COM_StripTrailingSlash( char* ppath )
{
	const auto uiLength = strlen( ppath );

	if( uiLength > 0 )
	{
		auto pszEnd = &ppath[ uiLength - 1 ];

		if( *pszEnd == '/' || *pszEnd == '\\' )
		{
			*pszEnd = '\0';
		}
	}
}

void COM_CreatePath( char* path )
{
	auto pszNext = path + 1;

	while( *pszNext )
	{
		if( *pszNext == '\\' || *pszNext == '/' )
		{
			const auto cSave = *pszNext;
			*pszNext = '\0';

			FS_CreateDirHierarchy( path, nullptr );

			*pszNext = cSave;
		}

		++pszNext;
	}
}

typedef struct
{
	unsigned char chunkID[4];
	long chunkSize;
	short wFormatTag;
	unsigned short wChannels;
	unsigned long dwSamplesPerSec;
	unsigned long dwAvgBytesPerSec;
	unsigned short wBlockAlign;
	unsigned short wBitsPerSample;
} FormatChunk;

const int WAVE_HEADER_LENGTH = 128;

unsigned int COM_GetApproxWavePlayLength( const char* filepath )
{
	char buf[WAVE_HEADER_LENGTH + 1];
	int filelength;
	FileHandle_t hFile;
	FormatChunk format;

	hFile = FS_Open(filepath, "rb");

	if (hFile)
	{
		filelength = FS_Size(hFile);

		if (filelength <= WAVE_HEADER_LENGTH)
			return 0;

		FS_Read(buf, WAVE_HEADER_LENGTH, 1, hFile);
		FS_Close(hFile);

		buf[WAVE_HEADER_LENGTH] = 0;

		if (!Q_strncasecmp(buf, "RIFF", 4) && !Q_strncasecmp(&buf[8], "WAVE", 4) && !Q_strncasecmp(&buf[12], "fmt ", 4))
		{
			Q_memcpy(&format, &buf[12], sizeof(FormatChunk));

			filelength -= WAVE_HEADER_LENGTH;

			if (format.dwAvgBytesPerSec > 999)
			{
				return filelength / (format.dwAvgBytesPerSec / 1000);
			}

			return 1000 * filelength / format.dwAvgBytesPerSec;
		}
	}

	return 0;
}

char* Info_Serverinfo()
{
	return serverinfo;
}

char* va( char* format, ... )
{
	static char string[ 1024 ];

	va_list argptr;

	va_start( argptr, format );
	vsnprintf( string, sizeof( string ), format, argptr );
	va_end( argptr );

	return string;
}

const int MAX_VEC_STRINGS = 16;

char* vstr( vec_t* v )
{
	static char string[ MAX_VEC_STRINGS ][ 1024 ];
	static int idx = 0;

	idx = ( idx + 1 ) % MAX_VEC_STRINGS;

	snprintf( string[ idx ], ARRAYSIZE( string[ idx ] ), "%.4f %.4f %.4f", v[ 0 ], v[ 1 ], v[ 2 ] );

	return string[ idx ];
}

int memsearch( byte* start, int count, int search )
{
	if( count <= 0 )
		return -1;

	int result = 0;

	if( *start != search )
	{
		while( 1 )
		{
			++result;
			if( result == count )
				break;

			if( start[ result ] == search )
				return result;
		}

		result = -1;
	}

	return result;
}

int Q_FileNameCmp( char* file1, char* file2 )
{
	for( auto pszLhs = file1, pszRhs = file2; *pszLhs && *pszRhs; ++pszLhs, ++pszRhs )
	{
		if( ( *pszLhs != '/' || *pszRhs != '\\' ) &&
			( *pszLhs != '\\' || *pszRhs != '/' ) )
		{
			if( tolower( *pszLhs ) != tolower( *pszRhs ) )
				return -1;
		}
	}

	return 0;
}

cache_user_t* loadcache = nullptr;
byte* loadbuf = nullptr;
int loadsize = 0;

byte* COM_LoadFile( char* path, int usehunk, int* pLength )
{
	g_engdstAddrs.COM_LoadFile( &path, &usehunk, &pLength );

	if( pLength )
		*pLength = 0;

	FileHandle_t hFile = FS_Open( path, "rb" );

	if( hFile == FILESYSTEM_INVALID_HANDLE )
		return nullptr;

	const int iSize = FS_Size( hFile );

	/*char info[188];
	sprintf(info, "1: path %s, size %d\n", path, iSize);
	OutputDebugStringA(info);*/

	char base[ 4096 ];
	COM_FileBase( path, base );
	base[ 32 ] = '\0';

	void* pBuffer = 0;

	if( usehunk == 0 )
	{
		pBuffer = Z_Malloc( iSize + 1 );
	}
	else if( usehunk == 1 )
	{
		pBuffer = Hunk_AllocName( iSize + 1, base );
	}
	else if( usehunk == 2 )
	{
		pBuffer = Hunk_TempAlloc( iSize + 1 );
	}
	else if( usehunk == 3 )
	{
		pBuffer = Cache_Alloc( loadcache, iSize + 1, base );
	}
	else if( usehunk == 4 )
	{
		pBuffer = loadbuf;

		if( iSize >= loadsize )
		{
			pBuffer = Hunk_TempAlloc( iSize + 1 );
		}
	}
	else if( usehunk == 5 )
	{
		pBuffer = Mem_Malloc( iSize + 1 );
	}
	else
	{
		Sys_Error( "COM_LoadFile: bad usehunk" );
	}

	if( !pBuffer )
		Sys_Error( "COM_LoadFile: not enough space for %s", path );

	*( ( byte* ) pBuffer + iSize ) = '\0';

	FS_Read( pBuffer, iSize, 1, hFile );
	FS_Close( hFile );

	if( pLength )
		*pLength = iSize;

	return ( byte * ) pBuffer;
}

void COM_FreeFile( void *buffer )
{
	g_engdstAddrs.COM_FreeFile( &buffer );

	if( buffer )
		Mem_Free( buffer );
}

byte* COM_LoadHunkFile( char* path )
{
	return COM_LoadFile( path, 1, nullptr );
}

byte* COM_LoadTempFile( char* path, int* pLength )
{
	return COM_LoadFile( path, 2, pLength );
}

void COM_LoadCacheFile( char* path, cache_user_t* cu )
{
	loadcache = cu;
	COM_LoadFile( path, 3, nullptr );
}

byte* COM_LoadStackFile( char* path, void* buffer, int bufsize, int* length )
{
	loadbuf = reinterpret_cast<byte*>( buffer );
	loadsize = bufsize;

	return COM_LoadFile( path, 4, length );
}

byte* COM_LoadFileForMe( char* filename, int* pLength )
{
	return COM_LoadFile( filename, 5, pLength );
}

byte* COM_LoadFileLimit( char* path, int pos, int cbmax, int* pcbread, FileHandle_t* phFile )
{
	auto hFile = *phFile;

	if( FILESYSTEM_INVALID_HANDLE == *phFile )
	{
		hFile = FS_Open( path, "rb" );
	}

	byte* pData = nullptr;

	if( FILESYSTEM_INVALID_HANDLE != hFile )
	{
		const auto uiSize = FS_Size( hFile );

		if( uiSize < static_cast<decltype( uiSize )>( pos ) )
			Sys_Error( "COM_LoadFileLimit: invalid seek position for %s", path );

		FS_Seek( hFile, pos, FILESYSTEM_SEEK_HEAD );

		*pcbread = min( cbmax, static_cast<int>( uiSize ) );

		char base[ 32 ];

		if( path )
		{
			COM_FileBase( path, base );
		}

		pData = reinterpret_cast<byte*>( Hunk_TempAlloc( *pcbread + 1 ) );

		if( pData )
		{
			pData[ uiSize ] = '\0';

			FS_Read( pData, uiSize, 1, hFile );
		}
		else
		{
			if( path )
				Sys_Error( "COM_LoadFileLimit: not enough space for %s", path );

			FS_Close( hFile );
		}
	}

	*phFile = hFile;

	return pData;
}

void COM_WriteFile( char* filename, void* data, int len )
{
	char name[ MAX_PATH ];
	snprintf( name, ARRAYSIZE( name ), "%s", filename );

	COM_FixSlashes( name );
	COM_CreatePath( name );

	auto hFile = FS_Open( name, "wb" );

	if( FILESYSTEM_INVALID_HANDLE != hFile )
	{
		Sys_Printf( "COM_WriteFile: %s\n", name );
		FS_Write( data, len, 1, hFile );
		FS_Close( hFile );
	}
	else
	{
		Sys_Printf( "COM_WriteFile: failed on %s\n", name );
	}
}

int COM_FileSize( char* filename )
{
	int result;

	auto hFile = FS_Open( filename, "rb" );

	if( FILESYSTEM_INVALID_HANDLE != hFile )
	{
		result = FS_Size( hFile );
		FS_Close( hFile );
	}
	else
	{
		result = -1;
	}

	return result;
}

bool COM_ExpandFilename( char* filename )
{
	char netpath[ MAX_PATH ];

	FS_GetLocalPath( filename, netpath, ARRAYSIZE( netpath ) );
	strcpy( filename, netpath );

	return *filename != '\0';
}

int COM_CompareFileTime( char* filename1, char* filename2, int* iCompare )
{
	*iCompare = 0;

	if( filename1 && filename2 )
	{
		const auto iLhs = FS_GetFileTime( filename1 );
		const auto iRhs = FS_GetFileTime( filename2 );

		if( iLhs < iRhs )
		{
			*iCompare = -1;
		}
		else if( iLhs > iRhs )
		{
			*iCompare = 1;
		}

		return true;
	}

	return false;
}

void COM_CopyFile( char* netpath, char* cachepath )
{
	int remaining, count;

	char buf[4096];

	FileHandle_t in, out;
	
	in = FS_Open(netpath, "rb");

	if( FILESYSTEM_INVALID_HANDLE != in )
	{

		COM_CreatePath( cachepath );

		FileHandle_t out = FS_Open( cachepath, "wb" );

		remaining = FS_Size(in);

		while (remaining > 0)
		{
			if (remaining < sizeof(buf))
			{
				count = remaining;
			}
			else
			{
				count = sizeof(buf);
			}

			FS_Read(buf, count, 1, in);
			FS_Write(buf, count, 1, out);
			remaining -= count;
		}

		FS_Close( in );
		FS_Close( out );
	}
}

#define COM_COPY_CHUNK_SIZE 1024

void COM_CopyFileChunk( FileHandle_t dst, FileHandle_t src, int nSize )
{
	char copybuf[COM_COPY_CHUNK_SIZE];

	int copysize = nSize;

	if( copysize > COM_COPY_CHUNK_SIZE )
	{
		while( copysize > COM_COPY_CHUNK_SIZE )
		{
			FS_Read( copybuf, COM_COPY_CHUNK_SIZE, 1, src );
			FS_Write( copybuf, COM_COPY_CHUNK_SIZE, 1, dst );
			copysize -= COM_COPY_CHUNK_SIZE;
		}

		//Compute size left
		copysize = nSize - ( ( nSize - ( COM_COPY_CHUNK_SIZE + 1 ) ) & ~( COM_COPY_CHUNK_SIZE - 1 ) ) - COM_COPY_CHUNK_SIZE;
	}

	FS_Read( copybuf, copysize, 1, src );
	FS_Write( copybuf, copysize, 1, dst );
	FS_Flush( src );
	FS_Flush( dst );
}

void COM_Log( char* pszFile, char* fmt, ... )
{
	char string[ 1024 ];
	va_list va;

	va_start( va, fmt );

	char* pszFileName = pszFile ? pszFile : const_cast<char*>("c:\\hllog.txt");

	vsnprintf( string, ARRAYSIZE( string ), fmt, va );
	va_end( va );

	FileHandle_t hFile = FS_Open( pszFileName, "a+t" );

	if( FILESYSTEM_INVALID_HANDLE != hFile )
	{
		FS_FPrintf( hFile, "%s", string );
		FS_Close( hFile );
	}
}

void COM_ListMaps( char* pszSubString )
{
	FileHandle_t fp;
	const char* findfn;

	int nSubStringLen = pszSubString && *pszSubString ? strlen( pszSubString ) : 0;

	Con_Printf( const_cast<char*>("-------------\n") );
	int showOutdated = 1;

	char curDir[ 4096 ];
	char mapwild[ 64 ];
	char sz[ 64 ];

	dheader_t header;

	do
	{
		strcpy( mapwild, "maps/*.bsp" );
		for( findfn = Sys_FindFirst( mapwild, NULL ); findfn != NULL; findfn = Sys_FindNext( NULL ) )
		{
			if (snprintf( curDir, ARRAYSIZE( curDir ), "maps/%s", findfn ) > sizeof(curDir) - 1)
			{
				Con_Printf(const_cast<char*>("Map name too long: %s\n"), findfn);
				continue;
			}
			FS_GetLocalPath( curDir, curDir, ARRAYSIZE( curDir ) );
			if( strstr( curDir, com_gamedir ) && ( !nSubStringLen || !strnicmp( findfn, pszSubString, nSubStringLen ) ) )
			{
				memset( &header, 0, sizeof( header ) );

				sprintf( sz, "maps/%s", findfn );

				fp = FS_Open( sz, "rb" );

				if( fp )
				{
					FS_Read( &header, sizeof( header ), 1, fp );
					FS_Close( fp );
				}

				if( header.version == BSPVERSION )
				{
					if( !showOutdated )
						Con_Printf( const_cast<char*>("%s\n"), findfn );
				}
				else if( showOutdated )
				{
					Con_Printf( const_cast<char*>("OUTDATED:  %s\n"), findfn );
				}
			}
		}
		Sys_FindClose();
		showOutdated--;
	}
	while( showOutdated != -1 );
}

void COM_GetGameDir( char* szGameDir )
{
	if( szGameDir )
		snprintf( szGameDir, MAX_PATH - 1, "%s", com_gamedir );
}

char* COM_SkipPath( char* pathname )
{
	char* pszLast = pathname;

	for(char* pszPath = pathname; *pszPath; pszPath++ )
	{
		if( *pszPath == '\\' || *pszPath == '/' )
		{
			pszLast = pszPath;
		}
	}

	return pszLast;
}

char* COM_BinPrintf( byte* buf, int nLen )
{
	static char szReturn[ 4096 ];

	memset( szReturn, 0, sizeof( szReturn ) );

	char szChunk[ 10 ];
	for( int i = 0; i < nLen; ++i )
	{
		snprintf( szChunk, ARRAYSIZE( szChunk ), "%02x", buf[ i ] );

		strncat( szReturn, szChunk, ARRAYSIZE( szReturn ) - 1 - strlen( szReturn ) );
	}

	return szReturn;
}

unsigned char COM_Nibble( char c )
{
	if( ( c >= '0' ) &&
		( c <= '9' ) )
	{
		return ( unsigned char ) ( c - '0' );
	}

	if( ( c >= 'A' ) &&
		( c <= 'F' ) )
	{
		return ( unsigned char ) ( c - 'A' + 0x0a );
	}

	if( ( c >= 'a' ) &&
		( c <= 'f' ) )
	{
		return ( unsigned char ) ( c - 'a' + 0x0a );
	}

	return '0';
}

void COM_HexConvert( const char* pszInput, int nInputLength, byte* pOutput )
{
	const auto iBytes = ( ( max( nInputLength - 1, 0 ) ) / 2 ) + 1;

	auto p = pszInput;

	for( int i = 0; i < iBytes; ++i, ++p )
	{
		pOutput[ i ] = ( 0x10 * COM_Nibble( p[ 0 ] ) ) | COM_Nibble( p[ 1 ] );
	}
}

void COM_NormalizeAngles( vec_t* angles )
{
	for( int i = 0; i < 3; ++i )
	{
		if( angles[ i ] < -180.0 )
		{
			angles[ i ] = fmod( (double)angles[ i ], 360.0 ) + 360.0;
		}
		else if( angles[ i ] > 180.0 )
		{
			angles[ i ] = fmod( (double)angles[ i ], 360.0 ) - 360.0;
		}
	}
}

int COM_EntsForPlayerSlots( int nPlayers )
{
	int num_edicts = gmodinfo.num_edicts;

	int parm = COM_CheckParm(const_cast<char*>("-num_edicts"));

	if( parm != 0 )
	{
		int iSetting = atoi( com_argv[ parm + 1 ] );

		if( num_edicts <= iSetting )
			num_edicts = iSetting;
	}

	return num_edicts + 15 * ( nPlayers - 1 );
}

void COM_ExplainDisconnection( qboolean bPrint, char* fmt, ... )
{
	static char string[ 1024 ];

	va_list va;

	va_start( va, fmt );
	vsnprintf( string, ARRAYSIZE( string ), fmt, va );
	va_end( va );

	strncpy( gszDisconnectReason, string, ARRAYSIZE( gszDisconnectReason ) - 1 );
	gszDisconnectReason[ ARRAYSIZE( gszDisconnectReason ) - 1 ] = '\0';

	gfExtendedError = true;

	if( bPrint )
	{
		if( gszDisconnectReason[ 0 ] != '#' )
			Con_Printf( const_cast<char*>("%s\n"), gszDisconnectReason);
	}
}

void COM_ExtendedExplainDisconnection( qboolean bPrint, char* fmt, ... )
{
	static char string[ 1024 ];

	va_list arg_ptr;

	va_start( arg_ptr, fmt );
	vsnprintf( string, ARRAYSIZE( string ), fmt, arg_ptr );
	va_end( arg_ptr );

	strncpy( gszExtendedDisconnectReason, string, ARRAYSIZE( gszExtendedDisconnectReason ) - 1 );
	gszExtendedDisconnectReason[ ARRAYSIZE( gszExtendedDisconnectReason ) - 1 ] = '\0';

	if( bPrint )
	{
		if( gszExtendedDisconnectReason[ 0 ] != '#' )
			Con_Printf( const_cast<char*>("%s\n"), gszExtendedDisconnectReason );
	}
}

/*
==============================================================================

MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

//
// writing functions
//

void MSG_WriteChar( sizebuf_t *sb, int c )
{
	byte    *buf;

#ifdef PARANOID
	if( c < -128 || c > 127 )
		Sys_Error( "MSG_WriteChar: range error" );
#endif

	buf = reinterpret_cast<byte*>( SZ_GetSpace( sb, 1 ) );
	buf[ 0 ] = c;
}

void MSG_WriteByte( sizebuf_t *sb, int c )
{
	byte    *buf;

#ifdef PARANOID
	if( c < 0 || c > 255 )
		Sys_Error( "MSG_WriteByte: range error" );
#endif

	buf = reinterpret_cast<byte*>( SZ_GetSpace( sb, 1 ) );
	buf[ 0 ] = c;
}

void MSG_WriteShort( sizebuf_t *sb, int c )
{
	byte    *buf;

#ifdef PARANOID
	if( c < ( ( short ) 0x8000 ) || c >( short )0x7fff )
		Sys_Error( "MSG_WriteShort: range error" );
#endif

	buf = reinterpret_cast<byte*>( SZ_GetSpace( sb, 2 ) );
	buf[ 0 ] = c & 0xff;
	buf[ 1 ] = c >> 8;
}

void MSG_WriteWord(sizebuf_t* sb, int c)
{
	byte* buf;

#ifdef PARANOID
	if (c < ((short)0x8000) || c >(short)0x7fff)
		Sys_Error("MSG_WriteWord: range error");
#endif

	buf = reinterpret_cast<byte*>(SZ_GetSpace(sb, 2));
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void MSG_WriteLong( sizebuf_t *sb, int c )
{
	byte    *buf;

	buf = reinterpret_cast<byte*>( SZ_GetSpace( sb, 4 ) );
	buf[ 0 ] = c & 0xff;
	buf[ 1 ] = ( c >> 8 ) & 0xff;
	buf[ 2 ] = ( c >> 16 ) & 0xff;
	buf[ 3 ] = c >> 24;
}

void MSG_WriteFloat( sizebuf_t *sb, float f )
{
	union
	{
		float   f;
		int     l;
	} dat;


	dat.f = f;
	dat.l = LittleLong( dat.l );

	SZ_Write( sb, &dat.l, 4 );
}

void MSG_WriteString( sizebuf_t *sb, const char *s )
{
	if( !s )
		SZ_Write( sb, "", 1 );
	else
		SZ_Write( sb, s, Q_strlen( (char*)s ) + 1 );
}

void MSG_WriteCoord( sizebuf_t *sb, float f )
{
	MSG_WriteShort( sb, ( int ) ( f * 8 ) );
}

void MSG_WriteAngle( sizebuf_t *sb, float f )
{
	MSG_WriteByte( sb, ( ( int ) f * 256 / 360 ) & 255 );
}

void MSG_WriteHiresAngle(sizebuf_t *sb, float f)
{
	MSG_WriteShort(sb, (int64)(fmod((double)f, 360.0) * 65536.0 / 360.0) & 0xFFFF);
}

void MSG_WriteUsercmd(sizebuf_t *buf, usercmd_t *to, usercmd_t *from)
{
	MSG_StartBitWriting(buf);
	DELTA_WriteDelta((byte*)from, (byte*)to, true, *DELTA_LookupRegistration("usercmd_t"), NULL);
	MSG_EndBitWriting(buf);
}

//
// reading functions
//
int msg_readcount = 0;
bool msg_badread = false;

const uint32 BITTABLE[] =
{
	0x00000001, 0x00000002, 0x00000004, 0x00000008,
	0x00000010, 0x00000020, 0x00000040, 0x00000080,
	0x00000100, 0x00000200, 0x00000400, 0x00000800,
	0x00001000, 0x00002000, 0x00004000, 0x00008000,
	0x00010000, 0x00020000, 0x00040000, 0x00080000,
	0x00100000, 0x00200000, 0x00400000, 0x00800000,
	0x01000000, 0x02000000, 0x04000000, 0x08000000,
	0x10000000, 0x20000000, 0x40000000, 0x80000000,
	0x00000000,
};

const uint32 ROWBITTABLE[] =
{
	0x00000000, 0x00000001, 0x00000003, 0x00000007,
	0x0000000F, 0x0000001F, 0x0000003F, 0x0000007F,
	0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF,
	0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF,
	0x0000FFFF, 0x0001FFFF, 0x0003FFFF, 0x0007FFFF,
	0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
	0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF,
	0x0FFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF,
	0xFFFFFFFF,
};

const uint32 INVBITTABLE[] =
{
	0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFB, 0xFFFFFFF7,
	0xFFFFFFEF, 0xFFFFFFDF, 0xFFFFFFBF, 0xFFFFFF7F,
	0xFFFFFEFF, 0xFFFFFDFF, 0xFFFFFBFF, 0xFFFFF7FF,
	0xFFFFEFFF, 0xFFFFDFFF, 0xFFFFBFFF, 0xFFFF7FFF,
	0xFFFEFFFF, 0xFFFDFFFF, 0xFFFBFFFF, 0xFFF7FFFF,
	0xFFEFFFFF, 0xFFDFFFFF, 0xFFBFFFFF, 0xFF7FFFFF,
	0xFEFFFFFF, 0xFDFFFFFF, 0xFBFFFFFF, 0xF7FFFFFF,
	0xEFFFFFFF, 0xDFFFFFFF, 0xBFFFFFFF, 0x7FFFFFFF,
	0xFFFFFFFF,
};

void MSG_BeginReading()
{
	msg_readcount = 0;
	msg_badread = false;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar()
{
	int     c;

	if( msg_readcount + 1 > net_message.cursize )
	{
		msg_badread = true;
		return -1;
	}

	c = ( signed char ) net_message.data[ msg_readcount ];
	msg_readcount++;

	return c;
}

int MSG_ReadByte()
{
	int     c;

	if( msg_readcount + 1 > net_message.cursize )
	{
		msg_badread = true;
		return -1;
	}

	c = ( unsigned char ) net_message.data[ msg_readcount ];
	msg_readcount++;

	return c;
}

int MSG_ReadShort()
{
	int     c;

	if( msg_readcount + 2 > net_message.cursize )
	{
		msg_badread = true;
		return -1;
	}

	c = ( short ) ( net_message.data[ msg_readcount ]
					+ ( net_message.data[ msg_readcount + 1 ] << 8 ) );

	msg_readcount += 2;

	return c;
}

int MSG_ReadWord()
{
	int     c;

	if (msg_readcount + 2 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (WORD)(net_message.data[msg_readcount]
		+ (net_message.data[msg_readcount + 1] << 8));

	msg_readcount += 2;

	return c;
}

int MSG_ReadLong()
{
	int     c;

	if( msg_readcount + 4 > net_message.cursize )
	{
		msg_badread = true;
		return -1;
	}

	c = net_message.data[ msg_readcount ]
		+ ( net_message.data[ msg_readcount + 1 ] << 8 )
		+ ( net_message.data[ msg_readcount + 2 ] << 16 )
		+ ( net_message.data[ msg_readcount + 3 ] << 24 );

	msg_readcount += 4;

	return c;
}

float MSG_ReadFloat()
{
	union
	{
		byte    b[ 4 ];
		float   f;
		int     l;
	} dat;

	dat.b[ 0 ] = net_message.data[ msg_readcount ];
	dat.b[ 1 ] = net_message.data[ msg_readcount + 1 ];
	dat.b[ 2 ] = net_message.data[ msg_readcount + 2 ];
	dat.b[ 3 ] = net_message.data[ msg_readcount + 3 ];
	msg_readcount += 4;

	dat.l = LittleLong( dat.l );

	return dat.f;
}

float MSG_ReadHiresAngle(void)
{
	int c = MSG_ReadShort();
	if (msg_badread)
		return 0.0f;
	return (float)(c * (360.0 / 65536));
}

char* MSG_ReadString()
{
	static char     string[ 2048 ];
	int             l, c;

	l = 0;
	do
	{
		c = MSG_ReadChar();
		if( c == -1 || c == 0 )
			break;
		string[ l ] = c;
		l++;
	}
	while( l < sizeof( string ) - 1 );

	string[ l ] = 0;

	return string;
}

char *MSG_ReadStringLine(void)
{
	int c = 0, l = 0;
	static char string[2048];

	while ((c = MSG_ReadChar(), c) && c != '\n' && c != -1 && l < ARRAYSIZE(string) - 1)
	{
		string[l++] = c;
	}
	string[l] = 0;

	return string;
}

float MSG_ReadCoord(sizebuf_t* sb)
{
	short crd = MSG_ReadShort();
	if (msg_badread)
		return .0f;
	return crd * ( 1.0 / 8 );
}

float MSG_ReadAngle()
{
	return MSG_ReadChar() * ( 360.0 / 256 );
}

int MSG_ReadBuf(int iSize, void *pbuf)
{
	if (msg_readcount + iSize <= net_message.cursize)
	{
		Q_memcpy(pbuf, &net_message.data[msg_readcount], iSize);
		msg_readcount += iSize;

		return 1;
	}

	msg_badread = 1;
	return -1;
}

void MSG_SkipString()
{
	int code;

	do
		code = MSG_ReadChar();
	while (code != -1 && code != 0);
}

void MSG_WriteOneBit(int nValue)
{
	if (bfwrite.nCurOutputBit >= 8)
	{
		SZ_GetSpace(bfwrite.pbuf, 1);
		bfwrite.nCurOutputBit = 0;
		++bfwrite.pOutByte;
	}

	if (!(bfwrite.pbuf->flags & FSB_OVERFLOWED))
	{
		if (nValue)
		{
			*bfwrite.pOutByte |= BITTABLE[bfwrite.nCurOutputBit];
		}
		else
		{
			*bfwrite.pOutByte &= INVBITTABLE[bfwrite.nCurOutputBit * 4];
		}

		bfwrite.nCurOutputBit++;
	}
}

void MSG_StartBitWriting(sizebuf_t *buf)
{
	bfwrite.nCurOutputBit = 0;
	bfwrite.pbuf = buf;
	bfwrite.pOutByte = &buf->data[buf->cursize];
}

void MSG_EndBitWriting(sizebuf_t *buf)
{
	if (!(bfwrite.pbuf->flags & FSB_OVERFLOWED))
	{
		*bfwrite.pOutByte &= 255 >> (8 - bfwrite.nCurOutputBit);
		SZ_GetSpace(bfwrite.pbuf, 1);
		bfwrite.nCurOutputBit = 0;
		bfwrite.pOutByte = 0;
		bfwrite.pbuf = 0;
	}
}

void MSG_WriteBits(uint32 data, int numbits)
{
	if (numbits < 32)
	{
		if (data >= (uint32)(1 << numbits))
			data = ROWBITTABLE[numbits];
	}

	int surplusBytes = 0;
	if ((uint32)bfwrite.nCurOutputBit >= 8)
	{
		surplusBytes = 1;
		bfwrite.nCurOutputBit = 0;
		++bfwrite.pOutByte;
	}

	int bits = numbits + bfwrite.nCurOutputBit;
	if (bits <= 32)
	{
		int bytesToWrite = bits >> 3;
		int bitsLeft = bits & 7;
		if (!bitsLeft)
			--bytesToWrite;
		SZ_GetSpace(bfwrite.pbuf, surplusBytes + bytesToWrite);
		if (!(bfwrite.pbuf->flags & FSB_OVERFLOWED))
		{
			*(uint32 *)bfwrite.pOutByte = (data << bfwrite.nCurOutputBit) | *(uint32 *)bfwrite.pOutByte & ROWBITTABLE[bfwrite.nCurOutputBit];
			bfwrite.nCurOutputBit = 8;
			if (bitsLeft)
				bfwrite.nCurOutputBit = bitsLeft;
			bfwrite.pOutByte = &bfwrite.pOutByte[bytesToWrite];
		}
	}
	else
	{
		SZ_GetSpace(bfwrite.pbuf, surplusBytes + 4);
		if (!(bfwrite.pbuf->flags & FSB_OVERFLOWED))
		{
			*(uint32 *)bfwrite.pOutByte = (data << bfwrite.nCurOutputBit) | *(uint32 *)bfwrite.pOutByte & ROWBITTABLE[bfwrite.nCurOutputBit];
			int leftBits = 32 - bfwrite.nCurOutputBit;
			bfwrite.nCurOutputBit = bits & 7;
			bfwrite.pOutByte += 4;
			*(uint32 *)bfwrite.pOutByte = data >> leftBits;
		}
	}
}

qboolean MSG_IsBitWriting(void)
{
	return bfwrite.pbuf != 0;
}

void MSG_WriteSBits(int data, int numbits)
{
	int idata = data;

	if (numbits < 32)
	{
		int maxnum = (1 << (numbits - 1)) - 1;

		if (data > maxnum || (maxnum = -maxnum, data < maxnum))
		{
			idata = maxnum;
		}
	}

	int sigbits = idata < 0;

	MSG_WriteOneBit(sigbits);
	MSG_WriteBits(abs(idata), numbits - 1);
}

void MSG_WriteBitString(const char *p)
{
	char *pch = (char *)p;

	while (*pch)
	{
		MSG_WriteBits(*pch, 8);
		++pch;
	}

	MSG_WriteBits(0, 8);
}

void MSG_WriteBitData(void *src, int length)
{
	int i;
	byte *p = (byte *)src;

	for (i = 0; i < length; i++, p++)
	{
		MSG_WriteBits(*p, 8);
	}
}

void MSG_WriteBitAngle(float fAngle, int numbits)
{
	if (numbits >= 32)
	{
		Sys_Error("Can't write bit angle with 32 bits precision\n");
	}

	uint32 shift = (1 << numbits);
	uint32 mask = shift - 1;

	int d = (int)(shift * fmod((double)fAngle, 360.0)) / 360;
	d &= mask;

	MSG_WriteBits(d, numbits);
}

float MSG_ReadBitAngle(int numbits)
{
	return (float)(MSG_ReadBits(numbits) * (360.0 / (1 << numbits)));
}

int MSG_CurrentBit(void)
{
	int nbits;

	if (bfread.pbuf)
	{
		nbits = bfread.nCurInputBit + 8 * bfread.nBytesRead;
	}
	else
	{
		nbits = 8 * msg_readcount;
	}
	return nbits;
}

qboolean MSG_IsBitReading(void)
{
	return bfread.pbuf != 0;
}

void MSG_StartBitReading(sizebuf_t *buf)
{
	bfread.nCurInputBit = 0;
	bfread.nBytesRead = 0;
	bfread.nBitFieldReadStartByte = msg_readcount;
	bfread.pbuf = buf;
	bfread.pInByte = &buf->data[msg_readcount];
	bfread.nMsgReadCount = msg_readcount + 1;

	if (msg_readcount + 1 > buf->cursize)
	{
		msg_badread = 1;
	}
}

void MSG_EndBitReading(sizebuf_t *buf)
{
	if (bfread.nMsgReadCount > buf->cursize)
	{
		msg_badread = 1;
	}

	msg_readcount = bfread.nMsgReadCount;
	bfread.nBitFieldReadStartByte = 0;
	bfread.nCurInputBit = 0;
	bfread.nBytesRead = 0;
	bfread.pInByte = 0;
	bfread.pbuf = 0;
}

int MSG_ReadOneBit(void)
{
	int nValue;

	if (msg_badread)
	{
		nValue = 1;
	}
	else
	{
		if (bfread.nCurInputBit >= 8)
		{
			++bfread.nMsgReadCount;
			bfread.nCurInputBit = 0;
			++bfread.nBytesRead;
			++bfread.pInByte;
		}

		if (bfread.nMsgReadCount <= bfread.pbuf->cursize)
		{
			nValue = (*bfread.pInByte & BITTABLE[bfread.nCurInputBit]) != 0;
			++bfread.nCurInputBit;
		}
		else
		{
			nValue = 1;
			msg_badread = 1;
		}
	}

	return nValue;
}

uint32 MSG_ReadBits(int numbits)
{
	uint32 result;

	if (msg_badread)
	{
		result = 1;
	}
	else
	{
		if (bfread.nCurInputBit >= 8)
		{
			++bfread.nMsgReadCount;
			++bfread.nBytesRead;
			++bfread.pInByte;

			bfread.nCurInputBit = 0;
		}

		uint32 bits = (bfread.nCurInputBit + numbits) & 7;

		if ((unsigned int)(bfread.nCurInputBit + numbits) <= 32)
		{
			result = (*(unsigned int *)bfread.pInByte >> bfread.nCurInputBit) & ROWBITTABLE[numbits];

			uint32 bytes = (bfread.nCurInputBit + numbits) >> 3;

			if (bits)
			{
				bfread.nCurInputBit = bits;
			}
			else
			{
				bfread.nCurInputBit = 8;
				bytes--;
			}

			bfread.pInByte += bytes;
			bfread.nMsgReadCount += bytes;
			bfread.nBytesRead += bytes;
		}
		else
		{
			result = ((*(unsigned int *)(bfread.pInByte + 4) & ROWBITTABLE[bits]) << (32 - bfread.nCurInputBit)) | (*(unsigned int *)bfread.pInByte >> bfread.nCurInputBit);
			bfread.nCurInputBit = bits;
			bfread.pInByte += 4;
			bfread.nMsgReadCount += 4;
			bfread.nBytesRead += 4;
		}

		if (bfread.nMsgReadCount > bfread.pbuf->cursize)
		{
			result = 1;
			msg_badread = 1;
		}
	}

	return result;
}

uint32 MSG_PeekBits(int numbits)
{
	bf_read_t savebf = bfread;
	uint32 r = MSG_ReadBits(numbits);
	bfread = savebf;

	return r;
}

int MSG_ReadSBits(int numbits)
{
	int nSignBit = MSG_ReadOneBit();
	int result = MSG_ReadBits(numbits - 1);

	if (nSignBit)
		result = -result;

	return result;
}

char *MSG_ReadBitString(void)
{
	static char buf[8192];

	char *p = &buf[0];

	for (char c = MSG_ReadBits(8); c; c = MSG_ReadBits(8))
		*p++ = c;

	*p = 0;

	return buf;
}

int MSG_ReadBitData(void *dest, int length)
{
	if (length > 0)
	{
		int i = length;
		unsigned char * p = (unsigned char *)dest;

		do
		{
			*p = (unsigned char)MSG_ReadBits(8);
			p++;
			--i;
		} while (i);
	}

	return length;
}

float MSG_ReadBitCoord(void)
{
	float value = 0;

	int intval = MSG_ReadOneBit();
	int fractval = MSG_ReadOneBit();

	if (intval || fractval)
	{
		int signbit = MSG_ReadOneBit();

		if (intval)
		{
			intval = MSG_ReadBits(12);
		}

		if (fractval)
		{
			fractval = MSG_ReadBits(3);
		}

		value = (float)(fractval / 8.0 + intval);

		if (signbit)
		{
			value = -value;
		}
	}

	return value;
}

void MSG_ReadUsercmd(usercmd_t *to, usercmd_t* from)
{
	MSG_StartBitReading(&net_message);
	DELTA_ParseDelta((byte *)from, (byte *)to, SV_LookupDelta(const_cast<char*>("usercmd_t")), sizeof(usercmd_t));
	MSG_EndBitReading(&net_message);
	COM_NormalizeAngles(to->viewangles);
}

void MSG_WriteBitCoord(const float f)
{
	int signbit = f <= -0.125;
	int intval = abs((int32)f);
	int fractval = abs((int32)f * 8) & 7;

	MSG_WriteOneBit(intval);
	MSG_WriteOneBit(fractval);

	if (intval || fractval)
	{
		MSG_WriteOneBit(signbit);
		if (intval)
			MSG_WriteBits(intval, 12);
		if (fractval)
			MSG_WriteBits(fractval, 3);
	}
}

void MSG_ReadBitVec3Coord(vec_t *fa)
{
	int xflag = MSG_ReadOneBit();
	int yflag = MSG_ReadOneBit();
	int zflag = MSG_ReadOneBit();

	if (xflag)
		fa[0] = MSG_ReadBitCoord();
	if (yflag)
		fa[1] = MSG_ReadBitCoord();
	if (zflag)
		fa[2] = MSG_ReadBitCoord();
}

void MSG_WriteBitVec3Coord(const vec_t *fa)
{
	bool xflag = fa[0] <= -0.125 || fa[0] >= 0.125;
	bool yflag = fa[1] <= -0.125 || fa[1] >= 0.125;
	bool zflag = fa[2] <= -0.125 || fa[2] >= 0.125;

	MSG_WriteOneBit(xflag);
	MSG_WriteOneBit(yflag);
	MSG_WriteOneBit(zflag);

	if (xflag)
		MSG_WriteBitCoord(fa[0]);
	if (yflag)
		MSG_WriteBitCoord(fa[1]);
	if (zflag)
		MSG_WriteBitCoord(fa[2]);
}

void MSG_WriteBuf(sizebuf_t *sb, int iSize, void *buf)
{
	if (buf)
	{
		SZ_Write(sb, buf, iSize);
	}
}

const char *Q_stristr(const char *pStr, const char *pSearch)
{
	if (!pStr || !pSearch)
		return NULL;

	char const *pLetter = pStr;

	// Check the entire string
	while (*pLetter != 0)
	{
		// Skip over non-matches
		if (tolower(*pLetter) == tolower(*pSearch))
		{
			// Check for match
			char const* pMatch = pLetter + 1;
			char const* pTest = pSearch + 1;
			while (*pTest != 0)
			{
				// We've run off the end; don't bother.
				if (*pMatch == 0)
					return NULL;

				if (tolower(*pMatch) != tolower(*pTest))
					break;

				++pMatch;
				++pTest;
			}

			// Found a match!
			if (*pTest == 0)
				return pLetter;
		}

		++pLetter;
	}

	return NULL;
}

void Q_strncpy_s(char *dest, const char *src, int count)
{
	strncpy(dest, src, count);
	dest[count - 1] = 0;
}
