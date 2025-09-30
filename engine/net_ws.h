#ifndef ENGINE_NET_WS_H
#define ENGINE_NET_WS_H

#include "net.h"
#include "netadr.h"
#include "enums.h"
#include "winquake.h"

const int MAX_ROUTEABLE_PACKET = 1400;

#define SPLIT_SIZE				(MAX_ROUTEABLE_PACKET - sizeof(SPLITPACKET))

// Create general message queues
const int NUM_MSG_QUEUES = 40;
const int MSG_QUEUE_SIZE = 1536;

const int NET_HEADER_FLAG_SPLITPACKET = -2;

typedef struct loopmsg_s
{
	unsigned char data[NET_MAX_MESSAGE];
	int datalen;
} loopmsg_t;

const int MAX_LOOPBACK = 4;

typedef struct loopback_s
{
	loopmsg_t msgs[MAX_LOOPBACK];
	int get;
	int send;
} loopback_t;

typedef struct packetlag_s
{
	unsigned char* pPacketData;	// Raw stream data is stored.
	int nSize;
	netadr_t net_from_;
	float receivedTime;
	struct packetlag_s* pNext;
	struct packetlag_s* pPrev;
} packetlag_t;

typedef struct
{
	int latency;
	int	choked;
} packet_latency_t;

typedef struct
{
	unsigned short client;
	unsigned short players;
	unsigned short entities;
	unsigned short tentities;
	unsigned short sound;
	unsigned short event;
	unsigned short usr;
	unsigned short msgbytes;
	unsigned short voicebytes;
} netbandwidthgraph_t;

typedef struct
{
	float		cmd_lerp;
	int			size;
	bool		sent;
} cmdinfo_t;

typedef struct
{
	byte color[3];
	byte alpha;
} netcolor_t;

typedef struct net_messages_s
{
	struct net_messages_s* next;
	qboolean preallocated;
	unsigned char* buffer;
	netadr_t from;
	int buffersize;
} net_messages_t;

// Split long packets. Anything over 1460 is failing on some routers.
typedef struct LONGPACKET_t
{
	int currentSequence;
	int splitCount;
	int totalSize;
	// TODO: It should be NET_MAX_MESSAGE, but value differs
	char buffer[MAX_UDP_PACKET];	// This has to be big enough to hold the largest message
} LONGPACKET;

// Use this to pick apart the network stream, must be packed
#include <pshpack1.h>
typedef struct SPLITPACKET_t
{
	int netID;
	int sequenceNumber;
	unsigned char packetID;
} SPLITPACKET;
#include <poppack.h>

const int NET_WS_MAX_FRAGMENTS = 5;

extern qboolean net_thread_initialized;
extern cvar_t net_address;
extern cvar_t ipname;
extern cvar_t defport;
extern cvar_t ip_clientport;
extern cvar_t clientport;
extern int net_sleepforever;
extern loopback_t loopbacks[2];
extern packetlag_t g_pLagData[3];
extern float gFakeLag;
extern int net_configured;
extern netadr_t net_local_ipx_adr;
extern netadr_t net_local_adr;
extern netadr_t net_from;
extern qboolean noip;
extern qboolean noipx;
extern sizebuf_t net_message;
extern cvar_t clockwindow;
extern qboolean use_thread;
extern cvar_t iphostport;
extern cvar_t hostport;
extern cvar_t ipx_hostport;
extern cvar_t ipx_clientport;
extern cvar_t multicastport;
extern cvar_t fakelag;
extern cvar_t fakeloss;
extern cvar_t net_graph;
extern cvar_t net_graphwidth;
extern cvar_t net_scale;
extern cvar_t net_graphpos;
extern unsigned char net_message_buffer[NET_MAX_PAYLOAD];
extern unsigned char in_message_buf[NET_MAX_PAYLOAD];
extern sizebuf_t in_message;
extern netadr_t in_from;
extern SOCKET ip_sockets[3];
extern SOCKET ipx_sockets[3];
extern LONGPACKET gNetSplit;
extern net_messages_t* messages[3];
extern net_messages_t* normalqueue;


void NET_ThreadLock();
void NET_ThreadUnlock();
unsigned short Q_ntohs(unsigned short netshort);
void NetadrToSockadr(netadr_t* a, struct sockaddr* s);
void SockadrToNetadr(struct sockaddr* s, netadr_t* a);
qboolean NET_CompareAdr(netadr_t a, netadr_t b);
qboolean NET_CompareClassBAdr(netadr_t a, netadr_t b);
qboolean NET_IsReservedAdr(netadr_t a);
qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b);
char* NET_AdrToString(netadr_t a);
char* NET_BaseAdrToString(netadr_t a);
qboolean NET_StringToSockaddr(char* s, struct sockaddr* sadr);
qboolean NET_StringToAdr(char* s, netadr_t* a);
qboolean NET_IsLocalAddress(netadr_t adr);
char* NET_ErrorString(int code);
void NET_TransferRawData(sizebuf_t* msg, unsigned char* pStart, int nSize);
qboolean NET_GetLoopPacket(netsrc_t sock, netadr_t* in_from, sizebuf_t* msg);
void NET_SendLoopPacket(netsrc_t sock, int length, void* data, netadr_t to);
void NET_RemoveFromPacketList(packetlag_t* pPacket);
void NET_ClearLaggedList(packetlag_t* pList);
void NET_AddToLagged(netsrc_t sock, packetlag_t* pList, packetlag_t* pPacket, netadr_t* net_from_, sizebuf_t messagedata, float timestamp);
void NET_AdjustLag();
qboolean NET_LagPacket(qboolean newdata, netsrc_t sock, netadr_t* from, sizebuf_t* data);
void NET_FlushSocket(netsrc_t sock);
qboolean NET_GetLong(unsigned char* pData, int size, int* outSize);
qboolean NET_QueuePacket(netsrc_t sock);
int NET_Sleep();
void NET_StartThread();
void NET_StopThread();
void* net_malloc(size_t size);
net_messages_t* NET_AllocMsg(int size);
void NET_FreeMsg(net_messages_t* pmsg);
qboolean NET_GetPacket(netsrc_t sock);
void NET_AllocateQueues();
void NET_FlushQueues();
int NET_SendLong(netsrc_t sock, SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen);
void NET_SendPacket(netsrc_t sock, int length, void* data, netadr_t to);
SOCKET NET_IPSocket(char* net_interface, int port, qboolean multicast);
void NET_OpenIP();
SOCKET NET_IPXSocket(int hostshort);
void NET_OpenIPX();
void NET_GetLocalAddress();
int NET_IsConfigured();
bool NET_CheckPort(int port);
void NET_Config(qboolean multiplayer);
void MaxPlayers_f();
void NET_Init();
void NET_ClearLagData(qboolean bClient, qboolean bServer);
void NET_Shutdown();
qboolean NET_JoinGroup(netsrc_t sock, netadr_t addr);
qboolean NET_LeaveGroup(netsrc_t sock, netadr_t addr);
void NET_DrawString(int x, int y, int font, float r, float g, float b, char* fmt, ...);
void SCR_NetGraph(void);

#endif 