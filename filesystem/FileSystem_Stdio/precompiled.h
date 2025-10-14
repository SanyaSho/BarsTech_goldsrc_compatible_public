/*
*
*    This program is free software; you can redistribute it and/or modify it
*    under the terms of the GNU General Public License as published by the
*    Free Software Foundation; either version 2 of the License, or (at
*    your option) any later version.
*
*    This program is distributed in the hope that it will be useful, but
*    WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*    General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*    In addition, as a special exception, the author gives permission to
*    link the code of this program with the Half-Life Game Engine ("HL
*    Engine") and Modified Game Libraries ("MODs") developed by Valve,
*    L.L.C ("Valve").  You must obey the GNU General Public License in all
*    respects for all of the code used other than the HL Engine and MODs
*    from Valve.  If you modify this file, you may extend this exception
*    to your version of the file, but you are not obligated to do so.  If
*    you do not wish to do so, delete this exception statement from your
*    version.
*
*/

#pragma once

#if defined( WIN32 )
#include <windows.h>
#include <time.h>
#define NORETURN __declspec(noreturn)
#else
#define NORETURN __attribute__((noreturn))
#endif // WIN32

#include "archtypes.h"
#include "interface.h"
#include "strtools.h"

// size - sizeof(buffer)
inline char *Q_strlcpy( char *dest, const char *src, size_t size )
{
	V_strncpy( dest, src, size - 1 );
	dest[size - 1] = '\0';
	return dest;
}

// a safe variant of strcpy that truncates the result to fit in the destination buffer
template <size_t size>
char *Q_strlcpy( char ( &dest )[size], const char *src )
{
	return Q_strlcpy( dest, src, size );
}

inline void Q_strncpy_s( char *dest, const char *src, int count )
{
	strncpy( dest, src, count );
	dest[count - 1] = 0;
}

template <size_t size>
size_t Q_strlcat( char ( &dest )[size], const char *src )
{
	size_t srclen; // Length of source string
	size_t dstlen; // Length of destination string

	// Figure out how much room is left
	dstlen = V_strlen( dest );
	size_t length = size - dstlen + 1;

	if ( !length )
	{
		// No room, return immediately
		return dstlen;
	}

	// Figure out how much room is needed
	srclen = V_strlen( src );

	// Copy the appropriate amount
	if ( srclen > length )
	{
		srclen = length;
	}

	memcpy( dest + dstlen, src, srclen );
	dest[dstlen + srclen] = '\0';

	return dstlen + srclen;
}

#include "FileSystem.h"

// FileSystem stuff
#include "FileSystem_Stdio.h"
