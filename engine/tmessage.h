#ifndef ENGINE_TMESSAGE_H
#define ENGINE_TMESSAGE_H

/**
*	Maximum number of game_text messages that can be displayed at any one time.
*/

#define DEMO_MESSAGE "__DEMOMESSAGE__"
#define NETWORK_MESSAGE1 "__NETMESSAGE__1"
#define NETWORK_MESSAGE2 "__NETMESSAGE__2"
#define NETWORK_MESSAGE3 "__NETMESSAGE__3"
#define NETWORK_MESSAGE4 "__NETMESSAGE__4"
#define MAX_NETMESSAGE	4

char* memfgets( byte* pMemFile, int fileSize, int* pfilePos, char* pBuffer, int bufferSize );

/**
*	Trims leading and trailing whitespace
*/
void TrimSpace( const char* source, char* dest );

void TextMessageShutdown();

void TextMessageInit();

void SetDemoMessage( const char* pszMessage, float fFadeInTime, float fFadeOutTime, float fHoldTime );

client_textmessage_t* TextMessageGet( const char* pName );

int TextMessageDrawCharacter( int x, int y, int number, int r, int g, int b );

void CL_ParseTextMessage(void);

extern client_textmessage_t* gMessageTable;
extern int					gMessageTableCount;

extern client_textmessage_t	gNetworkTextMessage[MAX_NETMESSAGE];
extern char					gNetworkTextMessageBuffer[MAX_NETMESSAGE][512];
extern const char* gNetworkMessageNames[MAX_NETMESSAGE];

#endif //ENGINE_TMESSAGE_H
