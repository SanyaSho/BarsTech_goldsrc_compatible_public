#include "quakedef.h"
#include "client.h"
#include "cl_demo.h"
//#include "gl_draw.h"
#include "draw.h"
#include "tmessage.h"
#include "vgui2/text_draw.h"
#include "cl_main.h"

#define MSGFILE_NAME		0
#define MSGFILE_TEXT		1
#define MAX_MESSAGES		1000			// I don't know if this table will balloon like every other feature in Half-Life
										// But, for now, I've set this to a reasonable value

client_textmessage_t gMessageParms = {};

client_textmessage_t* gMessageTable = nullptr;
int gMessageTableCount = 0;

char	gNetworkTextMessageBuffer[MAX_NETMESSAGE][512];
const char* gNetworkMessageNames[MAX_NETMESSAGE] = { NETWORK_MESSAGE1, NETWORK_MESSAGE2, NETWORK_MESSAGE3, NETWORK_MESSAGE4 };

client_textmessage_t	gNetworkTextMessage[MAX_NETMESSAGE] =
{
	0, // effect
	255,255,255,255,
	255,255,255,255,
	-1.0f, // x
	-1.0f, // y
	0.0f, // fadein
	0.0f, // fadeout
	0.0f, // holdtime
	0.0f, // fxTime,
	NETWORK_MESSAGE1,  // pName message name.
	gNetworkTextMessageBuffer[0]    // pMessage
};

//From HL1 SDK
char* memfgets( byte* pMemFile, int fileSize, int* pfilePos, char* pBuffer, int bufferSize )
{
	int filePos = *pfilePos;

	// Bullet-proofing
	if( !pMemFile || !pBuffer )
		return NULL;

	if( filePos >= fileSize )
		return NULL;

	int i = *pfilePos;
	int last = fileSize;

	// fgets always NULL terminates, so only read bufferSize-1 characters
	if( last - filePos > ( bufferSize - 1 ) )
		last = filePos + ( bufferSize - 1 );

	int stop = 0;

	// Stop at the next newline (inclusive) or end of rgba
	while( i < last && !stop )
	{
		if( pMemFile[ i ] == '\n' )
			stop = 1;
		i++;
	}


	// If we actually advanced the pointer, copy it over
	if( i != filePos )
	{
		// We read in size bytes
		int size = i - filePos;
		// copy it out
		Q_memcpy( pBuffer, pMemFile + filePos, sizeof( byte ) * size );

		// If the rgba isn't full, terminate (this is always true)
		if( size < bufferSize )
			pBuffer[ size ] = 0;

		// Update file pointer
		*pfilePos = i;
		return pBuffer;
	}

	// No data read, bail
	return NULL;
}

// The string "pText" is assumed to have all whitespace from both ends cut out
int IsComment(char* pText)
{
	if (pText)
	{
		int length = Q_strlen(pText);

		if (length >= 2 && pText[0] == '/' && pText[1] == '/')
			return 1;

		// No text?
		if (length > 0)
			return 0;
	}
	// No text is a comment too
	return 1;
}

// The string "pText" is assumed to have all whitespace from both ends cut out
int IsStartOfText(char* pText)
{
	if (pText)
	{
		if (pText[0] == '{')
			return 1;
	}
	return 0;
}


// The string "pText" is assumed to have all whitespace from both ends cut out
int IsEndOfText(char* pText)
{
	if (pText)
	{
		if (pText[0] == '}')
			return 1;
	}
	return 0;
}

int IsWhiteSpace(char space)
{
	if (space == ' ' || space == '\t' || space == '\r' || space == '\n')
		return 1;
	return 0;
}

const char* SkipSpace(const char* pText)
{
	if (pText)
	{
		int pos = 0;
		while (pText[pos] && IsWhiteSpace(pText[pos]))
			pos++;
		return pText + pos;
	}

	return NULL;
}


const char* SkipText(const char* pText)
{
	if (pText)
	{
		int pos = 0;
		while (pText[pos] && !IsWhiteSpace(pText[pos]))
			pos++;
		return pText + pos;
	}

	return NULL;
}

int ParseFloats(const char* pText, float* pFloat, int count)
{
	const char* pTemp = pText;
	int index = 0;

	while (pTemp && count > 0)
	{
		// Skip current token / float
		pTemp = SkipText(pTemp);
		// Skip any whitespace in between
		pTemp = SkipSpace(pTemp);
		if (pTemp)
		{
			// Parse a float
			pFloat[index] = (float)Q_atof(pTemp);
			count--;
			index++;
		}
	}

	if (count == 0)
		return 1;

	return 0;
}

// Trims all whitespace from the front and end of a string
void TrimSpace(const char* source, char* dest)
{
	int start, end, length;

	start = 0;
	end = strlen(source);

	while (source[start] && IsWhiteSpace(source[start]))
		start++;

	end--;
	while (end > 0 && IsWhiteSpace(source[end]))
		end--;

	end++;

	length = end - start;
	if (length > 0)
		Q_strncpy(dest, source + start, length);
	else
		length = 0;

	// Terminate the dest string
	dest[length] = 0;
}

int IsToken(const char* pText, const char* pTokenName)
{
	if (!pText || !pTokenName)
		return 0;

	if (!Q_strncasecmp(pText + 1, pTokenName, Q_strlen(pTokenName)))
		return 1;

	return 0;
}

int ParseDirective(const char* pText)
{
	if (pText && pText[0] == '$')
	{
		float tempFloat[8];

		if (IsToken(pText, "position"))
		{
			if (ParseFloats(pText, tempFloat, 2))
			{
				gMessageParms.x = tempFloat[0];
				gMessageParms.y = tempFloat[1];
			}
		}
		else if (IsToken(pText, "effect"))
		{
			if (ParseFloats(pText, tempFloat, 1))
			{
				gMessageParms.effect = (int)tempFloat[0];
			}
		}
		else if (IsToken(pText, "fxtime"))
		{
			if (ParseFloats(pText, tempFloat, 1))
			{
				gMessageParms.fxtime = tempFloat[0];
			}
		}
		else if (IsToken(pText, "color2"))
		{
			if (ParseFloats(pText, tempFloat, 3))
			{
				gMessageParms.r2 = (int)tempFloat[0];
				gMessageParms.g2 = (int)tempFloat[1];
				gMessageParms.b2 = (int)tempFloat[2];
			}
		}
		else if (IsToken(pText, "color"))
		{
			if (ParseFloats(pText, tempFloat, 3))
			{
				gMessageParms.r1 = (int)tempFloat[0];
				gMessageParms.g1 = (int)tempFloat[1];
				gMessageParms.b1 = (int)tempFloat[2];
			}
		}
		else if (IsToken(pText, "fadein"))
		{
			if (ParseFloats(pText, tempFloat, 1))
			{
				gMessageParms.fadein = tempFloat[0];
			}
		}
		else if (IsToken(pText, "fadeout"))
		{
			if (ParseFloats(pText, tempFloat, 3))
			{
				gMessageParms.fadeout = tempFloat[0];
			}
		}
		else if (IsToken(pText, "holdtime"))
		{
			if (ParseFloats(pText, tempFloat, 3))
			{
				gMessageParms.holdtime = tempFloat[0];
			}
		}
		else
		{
			Con_DPrintf(const_cast<char*>("Unknown token: %s\n"), pText);
		}

		return 1;
	}
	return 0;
}

#define NAME_HEAP_SIZE 16384

static void TextMessageParse( byte* pMemFile, int fileSize )
{
	char		buf[512], trim[512];
	char		*pCurrentText = 0, *pNameHeap;
	char		 currentName[512], nameHeap[NAME_HEAP_SIZE];
	int			lastNamePos;

	int			mode = MSGFILE_NAME;	// Searching for a message name	
	int			lineNumber, filePos, lastLinePos;
	int			messageCount;

	client_textmessage_t	textMessages[MAX_MESSAGES];

	int			i, nameHeapSize, textHeapSize, messageSize, nameOffset;

	lastNamePos = 0;
	lineNumber = 0;
	filePos = 0;
	lastLinePos = 0;
	messageCount = 0;


	while (memfgets(pMemFile, fileSize, &filePos, buf, 512) != NULL)
	{

		if (messageCount >= MAX_MESSAGES)
		{
			Sys_Error("tmessage::TextMessageParse : messageCount>=MAX_MESSAGES");
		}

		TrimSpace(buf, trim);
		switch (mode)
		{
		case MSGFILE_NAME:
			if (IsComment(trim))	// Skip comment lines
				break;

			if (ParseDirective(trim))	// Is this a directive "$command"?, if so parse it and break
				break;

			if (IsStartOfText(trim))
			{
				mode = MSGFILE_TEXT;
				pCurrentText = (char*)(pMemFile + filePos);
				break;
			}
			if (IsEndOfText(trim))
			{
				Con_DPrintf(const_cast<char*>("Unexpected '}' found, line %d\n"), lineNumber);
				return;
			}
			Q_strncpy(currentName, trim, sizeof(currentName) - 1);
			currentName[sizeof(currentName) - 1] = 0;
			break;

		case MSGFILE_TEXT:
			if (IsEndOfText(trim))
			{
				int length = strlen(currentName);

				// Save name on name heap
				if (lastNamePos + length > NAME_HEAP_SIZE)
				{
					Con_DPrintf(const_cast<char*>("Error parsing file!  length > %i bytes\n"), NAME_HEAP_SIZE);
					return;
				}
				Q_strcpy(nameHeap + lastNamePos, currentName);

				// Terminate text in-place in the memory file (it's temporary memory that will be deleted)
				pMemFile[lastLinePos - 1] = 0;

				// Save name/text on heap
				textMessages[messageCount] = gMessageParms;
				textMessages[messageCount].pName = nameHeap + lastNamePos;
				lastNamePos += strlen(currentName) + 1;
				textMessages[messageCount].pMessage = pCurrentText;
				messageCount++;

				// Reset parser to search for names
				mode = MSGFILE_NAME;
				break;
			}
			if (IsStartOfText(trim))
			{
				Con_DPrintf(const_cast<char*>("Unexpected '{' found, line %d\n"), lineNumber);
				return;
			}
			break;
		}
		lineNumber++;
		lastLinePos = filePos;
	}

	Con_DPrintf(const_cast<char*>("Parsed %d text messages\n"), messageCount);
	nameHeapSize = lastNamePos;
	textHeapSize = 0;
	for (i = 0; i < messageCount; i++)
		textHeapSize += Q_strlen(textMessages[i].pMessage) + 1;


	messageSize = (messageCount * sizeof(client_textmessage_t));

	// Must malloc because we need to be able to clear it after initialization
	gMessageTable = (client_textmessage_t*)Mem_Malloc(textHeapSize + nameHeapSize + messageSize);

	// Copy table over
	Q_memcpy(gMessageTable, textMessages, messageSize);

	// Copy Name heap
	pNameHeap = ((char*)gMessageTable) + messageSize;
	Q_memcpy(pNameHeap, nameHeap, nameHeapSize);
	nameOffset = pNameHeap - gMessageTable[0].pName;

	// Copy text & fixup pointers
	pCurrentText = pNameHeap + nameHeapSize;

	for (i = 0; i < messageCount; i++)
	{
		gMessageTable[i].pName += nameOffset;		// Adjust name pointer (parallel rgba)
		Q_strcpy(pCurrentText, gMessageTable[i].pMessage);	// Copy text over
		gMessageTable[i].pMessage = pCurrentText;
		pCurrentText += Q_strlen(pCurrentText) + 1;
	}

#if _DEBUG
	if ((pCurrentText - (char*)gMessageTable) != (textHeapSize + nameHeapSize + messageSize))
		Con_Printf(const_cast<char*>("Overflow text message buffer!!!!!\n"));
#endif
	gMessageTableCount = messageCount;
}

void TextMessageShutdown()
{
	if( gMessageTable )
	{
		Mem_Free( gMessageTable );
		gMessageTable = nullptr;
	}
}

void TextMessageInit()
{
	TextMessageShutdown();

	int fileSize;
	byte* pFileData = COM_LoadTempFile( const_cast<char*>("titles.txt"), &fileSize );

	if( pFileData )
		TextMessageParse( pFileData, fileSize );
}

void SetDemoMessage( const char* pszMessage, float fFadeInTime, float fFadeOutTime, float fHoldTime )
{
	if( pszMessage && pszMessage[0])
	{
		Q_strcpy( const_cast<char*>( tm_demomessage.pMessage ), (char*)pszMessage );
		tm_demomessage.fadein = fFadeInTime;
		tm_demomessage.fadeout = fFadeOutTime;
		tm_demomessage.holdtime = fHoldTime;
	}
}

client_textmessage_t* TextMessageGet( const char* pName )
{
	int i;

	g_engdstAddrs.pfnTextMessageGet( &pName );

	if (!Q_strcasecmp(pName, DEMO_MESSAGE))
	{
		return &tm_demomessage;
	}

	// HACKHACK -- add 4 "channels" of network text
	if (!Q_strcasecmp(pName, NETWORK_MESSAGE1))
		return gNetworkTextMessage;
	else if (!Q_strcasecmp(pName, NETWORK_MESSAGE2))
		return gNetworkTextMessage + 1;
	else if (!Q_strcasecmp(pName, NETWORK_MESSAGE3))
		return gNetworkTextMessage + 2;
	else if (!Q_strcasecmp(pName, NETWORK_MESSAGE4))
		return gNetworkTextMessage + 3;

	if (gMessageTable)
	{
		for (i = 0; i < gMessageTableCount; i++)
		{
			if (!Q_strcasecmp(pName, gMessageTable[i].pName))
				return &gMessageTable[i];
		}
	}

	return NULL;
}

int TextMessageDrawCharacter( int x, int y, int number, int r, int g, int b )
{
	g_engdstAddrs.pfnDrawCharacter( &x, &y, &number, &r, &g, &b );

	if( r || g || b )
	{
		return Draw_MessageCharacterAdd( x, y, number, r, g, b, VGUI2_GetCreditsFont() );
	}

	return 0;
}
