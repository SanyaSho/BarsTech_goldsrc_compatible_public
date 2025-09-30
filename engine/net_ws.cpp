#include "quakedef.h"
#include "client.h"
#include "server.h"
#include "net.h"
#include "net_ws.h"
#include "net_chan.h"
#include "pr_cmds.h"
#include "strtools.h"
#include "draw.h"
#include "kbutton.h"
#include "cl_parse.h"
#include "screen.h"
#include "gl_screen.h"
#include "vid.h"
#include "../cl_dll/kbutton.h"
#include "cl_main.h"
#include "host.h"
#include <WSipx.h>
#include <winsock.h>

qboolean net_thread_initialized;

loopback_t loopbacks[2];
packetlag_t g_pLagData[NS_MAX];	// List of lag structures, if fakelag is set.
float gFakeLag;
int net_configured;
netadr_t net_local_ipx_adr;
netadr_t net_local_adr;
netadr_t net_from;
sizebuf_t net_message;
qboolean noip;
qboolean noipx;
qboolean use_thread;

unsigned char net_message_buffer[NET_MAX_PAYLOAD];
unsigned char in_message_buf[NET_MAX_PAYLOAD];
sizebuf_t in_message;
netadr_t in_from;

// Original engine behavior
#define INV_SOCK 0

SOCKET ip_sockets[NS_MAX] = { INV_SOCK, INV_SOCK, INV_SOCK };

SOCKET ipx_sockets[NS_MAX] = { INV_SOCK, INV_SOCK, INV_SOCK };

LONGPACKET gNetSplit;
net_messages_t* messages[NS_MAX];
net_messages_t* normalqueue;

HANDLE hNetThread;
DWORD dwNetThreadId;
CRITICAL_SECTION net_cs;

cvar_t net_address = { const_cast<char*>("net_address"), const_cast<char*>("") };
cvar_t ipname = { const_cast<char*>("ip"), const_cast<char*>("localhost") };
cvar_t defport = { const_cast<char*>("port"), const_cast<char*>("27015") };
cvar_t ip_clientport = { const_cast<char*>("ip_clientport"), const_cast<char*>("0") };
cvar_t clientport = { const_cast<char*>("clientport"), const_cast<char*>("27005") };
int net_sleepforever = 1;

cvar_t clockwindow = { const_cast<char*>("clockwindow"), const_cast<char*>("0.5") };

cvar_t iphostport = { const_cast<char*>("ip_hostport"), const_cast<char*>("0") };
cvar_t hostport = { const_cast<char*>("hostport"), const_cast<char*>("0") };
cvar_t multicastport = { const_cast<char*>("multicastport"), const_cast<char*>("27025") };

cvar_t ipx_hostport = { const_cast<char*>("ipx_hostport"), const_cast<char*>("0") };
cvar_t ipx_clientport = { const_cast<char*>("ipx_clientport"), const_cast<char*>("0") };

cvar_t fakelag = { const_cast<char*>("fakelag"), const_cast<char*>("0.0") };
cvar_t fakeloss = { const_cast<char*>("fakeloss"), const_cast<char*>("0.0") };
cvar_t net_graph = { const_cast<char*>("net_graph"), const_cast<char*>("0"), FCVAR_ARCHIVE };
cvar_t net_graphwidth = { const_cast<char*>("net_graphwidth"), const_cast<char*>("150") };
cvar_t net_scale = { const_cast<char*>("net_scale"), const_cast<char*>("5"), FCVAR_ARCHIVE };
cvar_t net_graphpos = { const_cast<char*>("net_graphpos"), const_cast<char*>("1"), FCVAR_ARCHIVE };

#define NUM_LATENCY_SAMPLES 8

#define	TIMINGS	1024       // Number of values to track (must be power of 2) b/c of masking
#define LATENCY_AVG_FRAC 0.5
#define PACKETLOSS_AVG_FRAC 0.5
#define PACKETCHOKE_AVG_FRAC 0.5

#define GRAPH_RED	(0.9 /* 255*/)
#define GRAPH_GREEN (0.9 /* 255*/)
#define GRAPH_BLUE	(0.7 /* 255*/)

#define LERP_HEIGHT 24

#define COLOR_DROPPED	0
#define COLOR_INVALID	1
#define COLOR_SKIPPED	2
#define COLOR_CHOKED	3
#define COLOR_NORMAL	4

//color24 colors[24];
byte colors[LERP_HEIGHT][3];

byte holdcolor[3] = {255, 0, 0};
byte extrap_base_color[3] = {255, 255, 255};

packet_latency_t	packet_latency[TIMINGS];
cmdinfo_t			cmdinfo[TIMINGS];
netbandwidthgraph_t	graph[TIMINGS];

float framerate;
float packet_loss;
float packet_choke;

netcolor_t netcolors[5] = {
{ 255, 0,   0,   255 },
{ 0,   0,   255, 255 },
{ 240, 127, 63,  255 },
{ 255, 255, 0,   255 },
{ 63,  255, 63,  150 }
};

// custom code
static FILE* m_fLogStream = fopen("wslog.log", "wb");

void DbgPrint(FILE *m_fLogStream, const char* format, ...)
{
	char szFormat[MAXBYTE * 4];
	va_list pszArgs;

	if (m_fLogStream == nullptr) return;

	va_start(pszArgs, format);
	vsprintf(szFormat, format, pszArgs);
	va_end(pszArgs);

	fprintf(m_fLogStream, szFormat);
	fflush(m_fLogStream);
}

char getSymbol(char in)
{
	if (in >= 0x00 && in <= 0x1F)
		return '.';
	if (in == 0x7F) return '.';
	return in;
}

void LogArray(FILE* out, PBYTE data, INT size)
{
	FILE* f;
	int i, j;
	bool hexalign = false;
	BYTE hexspaces;
	char offset[9];
	char b[4];

	f = out;

	if ((size & 15) != 0)
	{
		hexspaces = size & 15;
		hexalign = true;
	}
	for (i = 0; i < size; i++)
	{
		if ((i & 15) == 0)
		{
			sprintf(offset, "%08X", i);
			fprintf(f, "%s	%02X ", offset, data[i]);
		}
		else
		{
			if ((i & 15) == 15)
			{
				fprintf(f, "%02X	", data[i]);
				for (j = 0; j < 16; j++)
					fprintf(f, "%c", getSymbol(data[i - 15 + j]));
				fprintf(f, "\r\n");
			}
			else
			{
				fprintf(f, "%02X ", data[i]);
			}
		}

		if (hexalign && i == size - 1)
		{
			for (j = 0; j < (15 - hexspaces); j++)
				fprintf(f, "   ");
			fprintf(f, "  ");
			fprintf(f, "	");
			for (j = 0; j < hexspaces; j++)
				fprintf(f, "%c", getSymbol(data[i - (hexspaces - 1) + j]));
		}
	}
}
// custom code end

void NET_ThreadLock()
{
	if (use_thread && net_thread_initialized)
		EnterCriticalSection(&net_cs);
}

void NET_ThreadUnlock()
{
	if (use_thread && net_thread_initialized)
		LeaveCriticalSection(&net_cs);
}

unsigned short Q_ntohs(unsigned short netshort)
{
	return ntohs(netshort);
}

void NetadrToSockadr(netadr_t* a, struct sockaddr* s)
{
	Q_memset(s, 0, sizeof(*s));

	sockaddr_in* s_in = (sockaddr_in*)s;

	switch (a->type)
	{
	case NA_BROADCAST:
		s_in->sin_family = AF_INET;
		s_in->sin_addr.s_addr = INADDR_BROADCAST;
		s_in->sin_port = a->port;
		break;
	case NA_IP:
		s_in->sin_family = AF_INET;
		s_in->sin_addr.s_addr = *(int*)&a->ip;
		s_in->sin_port = a->port;
		break;
	case NA_IPX:
		s->sa_family = AF_IPX;
		Q_memcpy(s->sa_data, a->ipx, 10);
		*(unsigned short*)&s->sa_data[10] = a->port;
		break;
	case NA_BROADCAST_IPX:
		s->sa_family = AF_IPX;
		Q_memset(&s->sa_data, 0, 4);
		Q_memset(&s->sa_data[4], 255, 6);
		*(unsigned short*)&s->sa_data[10] = a->port;
		break;
	default:
		break;
	}
}

void SockadrToNetadr(struct sockaddr* s, netadr_t* a)
{
	if (s->sa_family == AF_INET)
	{
		a->type = NA_IP;
		*(int*)&a->ip = ((struct sockaddr_in*)s)->sin_addr.s_addr;
		a->port = ((struct sockaddr_in*)s)->sin_port;
	}
	else if (s->sa_family == AF_IPX)
	{
		a->type = NA_IPX;
		Q_memcpy(a->ipx, s->sa_data, sizeof(a->ipx));
		a->port = *(unsigned short*)&s->sa_data[10];
	}
}

qboolean NET_CompareAdr(netadr_t a, netadr_t b)
{
	if (a.type != b.type)
	{
		return FALSE;
	}
	if (a.type == NA_LOOPBACK)
	{
		return TRUE;
	}

	if (a.type == NA_IP)
	{
		if (a.ip[0] == b.ip[0] &&
			a.ip[1] == b.ip[1] &&
			a.ip[2] == b.ip[2] &&
			a.ip[3] == b.ip[3] &&
			a.port == b.port)
			return TRUE;
	}
	else if (a.type == NA_IPX)
	{
		if (Q_memcmp(a.ipx, b.ipx, 10) == 0 &&
			a.port == b.port)
			return TRUE;
	}

	return FALSE;
}


SOCKET NET_IPSocket(char* net_interface, int port, qboolean multicast)
{
	SOCKET newsocket;
	qboolean _true = TRUE;

	if ((newsocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
	{
		int err = WSAGetLastError();
		if (err != WSAEAFNOSUPPORT)
		{
			Con_Printf(const_cast<char*>("WARNING: UDP_OpenSocket: port: %d socket: %s"), port, NET_ErrorString(err));
		}
		return INV_SOCK;
	}

	if (ioctlsocket(newsocket, FIONBIO, (u_long*)&_true) == SOCKET_ERROR)
	{
		Con_Printf(const_cast<char*>("WARNING: UDP_OpenSocket: port: %d  ioctl FIONBIO: %s\n"), port, NET_ErrorString(WSAGetLastError()));
		return INV_SOCK;
	}

	qboolean i = TRUE;
	if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char*)&i, sizeof(i)) == SOCKET_ERROR)
	{
		Con_Printf(const_cast<char*>("WARNING: UDP_OpenSocket: port: %d  setsockopt SO_BROADCAST: %s\n"), port, NET_ErrorString(WSAGetLastError()));
		return INV_SOCK;
	}

	if (COM_CheckParm(const_cast<char*>("-reuse")) || multicast)
	{
		if (setsockopt(newsocket, SOL_SOCKET, SO_REUSEADDR, (char*)&_true, sizeof(qboolean)) == SOCKET_ERROR)
		{
			Con_Printf(const_cast<char*>("WARNING: UDP_OpenSocket: port: %d  setsockopt SO_REUSEADDR: %s\n"), port, NET_ErrorString(WSAGetLastError()));
			return INV_SOCK;
		}
	}

	struct sockaddr_in address;

	if (net_interface && *net_interface && Q_strcasecmp(net_interface, "localhost"))
		NET_StringToSockaddr(net_interface, (sockaddr*)&address);
	else
		address.sin_addr.s_addr = INADDR_ANY;

	if (port == -1) // TODO: Always false?
		address.sin_port = 0;
	else
		address.sin_port = htons((u_short)port);

	address.sin_family = AF_INET;

	if (bind(newsocket, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
	{
		Con_Printf(const_cast<char*>("WARNING: UDP_OpenSocket: port: %d  bind: %s\n"), port, NET_ErrorString(WSAGetLastError()));
		closesocket(newsocket);
		return INV_SOCK;
	}

	qboolean bLoopBack = COM_CheckParm(const_cast<char*>("-loopback")) != 0;
	if (setsockopt(newsocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&bLoopBack, sizeof(bLoopBack)) == SOCKET_ERROR)
	{
		Con_DPrintf(const_cast<char*>("WARNING: UDP_OpenSocket: port %d setsockopt IP_MULTICAST_LOOP: %s\n"), port, NET_ErrorString(WSAGetLastError()));
	}

	return newsocket;
}

void NET_OpenIP()
{
	int sv_port = 0;
	int cl_port = 0;

	int dedicated = cls.state == ca_dedicated;

	NET_ThreadLock();

	if (ip_sockets[NS_SERVER] == INV_SOCK)
	{
		int port = (int)iphostport.value;

		if (!NET_CheckPort(port))
		{
			port = (int)hostport.value;
			if (!NET_CheckPort(port))
			{
				port = (int)defport.value;
				hostport.value = defport.value;
			}
		}
		ip_sockets[NS_SERVER] = NET_IPSocket(ipname.string, port, FALSE);

		if (ip_sockets[NS_SERVER] == INV_SOCK && dedicated)
		{
			Sys_Error("%s: Couldn't allocate dedicated server IP port %d.", __func__, port);
		}
		sv_port = port;
	}

	NET_ThreadUnlock();

	if (dedicated)
		return;

	NET_ThreadLock();

	if (ip_sockets[NS_CLIENT] == INV_SOCK)
	{
		int port = (int)ip_clientport.value;

		if (!NET_CheckPort(port))
		{
			port = (int)clientport.value;
			if (!NET_CheckPort(port))
				port = -1;
		}
		ip_sockets[NS_CLIENT] = NET_IPSocket(ipname.string, port, FALSE);
		if (ip_sockets[NS_CLIENT] == INV_SOCK)
			ip_sockets[NS_CLIENT] = NET_IPSocket(ipname.string, -1, FALSE);
		cl_port = port;
	}

	if (ip_sockets[NS_MULTICAST] == INV_SOCK)
	{
		ip_sockets[NS_MULTICAST] = NET_IPSocket(ipname.string, multicastport.value, TRUE);
		if (ip_sockets[NS_MULTICAST] == INV_SOCK && !dedicated)
			Con_Printf(const_cast<char*>("Warning! Couldn't allocate multicast IP port.\n"));
	}

	NET_ThreadUnlock();

	static qboolean bFirst = TRUE;
	if (bFirst)
	{
		bFirst = FALSE;
		Con_Printf(const_cast<char*>("NET Ports:  server %i, client %i\n"), sv_port, cl_port);
	}
}

SOCKET NET_IPXSocket(int hostshort)
{
	int err;
	u_long optval = 1;
	SOCKET newsocket;

	if ((newsocket = socket(PF_IPX, SOCK_DGRAM, NSPROTO_IPX)) == INVALID_SOCKET)
	{
		err = WSAGetLastError();
		if (err != WSAEAFNOSUPPORT)
		{
			Con_Printf(const_cast<char*>("WARNING: IPX_Socket: port: %d  socket: %s\n"), hostshort, NET_ErrorString(err));
		}

		return INV_SOCK;
	}
	if (ioctlsocket(newsocket, FIONBIO, &optval) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		Con_Printf(const_cast<char*>("WARNING: IPX_Socket: port: %d  ioctl FIONBIO: %s\n"), hostshort, NET_ErrorString(err));
		return INV_SOCK;
	}
	if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (const char*)&optval, sizeof(optval)) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		Con_Printf(const_cast<char*>("WARNING: IPX_Socket: port: %d  setsockopt SO_BROADCAST: %s\n"), hostshort, NET_ErrorString(err));
		return INV_SOCK;
	}
	if (setsockopt(newsocket, SOL_SOCKET, 4, (const char*)&optval, sizeof(optval)) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		return INV_SOCK;
	}

	SOCKADDR_IPX address;
	address.sa_family = AF_IPX;
	Q_memset(address.sa_netnum, 0, 4);
	Q_memset(address.sa_nodenum, 0, 6);

	if (hostshort == -1)
		address.sa_socket = 0;
	else address.sa_socket = htons((u_short)hostshort);

	if (bind(newsocket, (struct sockaddr*)&address, sizeof(SOCKADDR_IPX)) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		Con_Printf(const_cast<char*>("WARNING: IPX_Socket: port: %d  bind: %s\n"), hostshort, NET_ErrorString(err));
		closesocket(newsocket);
		return INV_SOCK;
	}
	return newsocket;
}

void NET_OpenIPX()
{
	int dedicated = cls.state == ca_dedicated;

	NET_ThreadLock();

	if (ipx_sockets[NS_SERVER] == INV_SOCK)
	{
		int port = ipx_hostport.value;
		if (!NET_CheckPort(port))
		{
			port = hostport.value;
			if (!NET_CheckPort(port))
			{
				hostport.value = defport.value;
				port = defport.value;
			}
		}
		ipx_sockets[NS_SERVER] = NET_IPXSocket(port);
	}

	NET_ThreadUnlock();

	if (dedicated)
		return;

	NET_ThreadLock();

	if (ipx_sockets[NS_CLIENT] == INV_SOCK)
	{
		int port = ipx_clientport.value;
		if (!NET_CheckPort(port))
		{
			port = clientport.value;
			if (!NET_CheckPort(port))
				port = -1;
		}
		ipx_sockets[NS_CLIENT] = NET_IPXSocket(port);

		if (ipx_sockets[NS_CLIENT] == INV_SOCK)
			ipx_sockets[NS_CLIENT] = NET_IPXSocket(-1);
	}

	NET_ThreadUnlock();
}

void NET_GetLocalAddress()
{
	char buff[512];
	struct sockaddr_in address;
	int namelen;
	int net_error;

	Q_memset(&net_local_adr, 0, sizeof(netadr_t));

	Q_memset(&net_local_ipx_adr, 0, sizeof(netadr_t));

	if (noip)
	{
		Con_Printf(const_cast<char*>("TCP/IP Disabled.\n"));
	}
	else
	{
		if (Q_strcmp(ipname.string, "localhost"))
			Q_strncpy(buff, ipname.string, ARRAYSIZE(buff) - 1);
		else
		{
			gethostname(buff, ARRAYSIZE(buff));
		}

		buff[ARRAYSIZE(buff) - 1] = 0;

		NET_StringToAdr(buff, &net_local_adr);
		namelen = sizeof(address);
		if (getsockname(ip_sockets[NS_SERVER], (struct sockaddr*)&address, (int*)&namelen) == SOCKET_ERROR)
		{
			noip = TRUE;
			net_error = WSAGetLastError();

			Con_Printf(const_cast<char*>("Could not get TCP/IP address, TCP/IP disabled\nReason:  %s\n"), NET_ErrorString(net_error));
		}
		else
		{
			net_local_adr.port = address.sin_port;
			Con_Printf(const_cast<char*>("Server IP address %s\n"), NET_AdrToString(net_local_adr));
			Cvar_Set(const_cast<char*>("net_address"), va(NET_AdrToString(net_local_adr)));
		}
	}

	if (noipx)
	{
		Con_Printf(const_cast<char*>("No IPX Support.\n"));
	}
	else
	{
		namelen = 14;
		if (getsockname(ipx_sockets[NS_SERVER], (struct sockaddr*)&address, (int*)&namelen) == SOCKET_ERROR)
		{
			noipx = TRUE;
			net_error = WSAGetLastError();

		}
		else
		{
			SockadrToNetadr((struct sockaddr*)&address, &net_local_ipx_adr);
			Con_Printf(const_cast<char*>("Server IPX address %s\n"), NET_AdrToString(net_local_ipx_adr));
		}
	}
}

int NET_IsConfigured()
{
	return net_configured;
}

bool NET_CheckPort(int port)
{
	return port != 0;
}

void NET_Config( qboolean multiplayer )
{
	static qboolean old_config;

	if (old_config == multiplayer)
	{
		return;
	}

	old_config = multiplayer;

	if (multiplayer)
	{
		if (!noip)
			NET_OpenIP();
		if (!noipx)
			NET_OpenIPX();

		static qboolean bFirst = TRUE;
		if (bFirst)
		{
			bFirst = FALSE;
			NET_GetLocalAddress();
		}
	}
	else
	{
		NET_ThreadLock();

		for (int sock = 0; sock < NS_MAX; sock++)
		{
			if (ip_sockets[sock] != INV_SOCK)
			{
				closesocket(ip_sockets[sock]);
				ip_sockets[sock] = INV_SOCK;
			}

			if (ipx_sockets[sock] != INV_SOCK)
			{
				closesocket(ipx_sockets[sock]);
				ipx_sockets[sock] = INV_SOCK;
			}
		}

		NET_ThreadUnlock();
	}

	net_configured = multiplayer ? 1 : 0;

}

void NET_Shutdown()
{
	NET_ThreadLock();

	NET_ClearLaggedList(g_pLagData);
	NET_ClearLaggedList(&g_pLagData[1]);

	NET_ThreadUnlock();

	NET_Config(FALSE);
	NET_FlushQueues();
}

void MaxPlayers_f()
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf(const_cast<char*>("\"maxplayers\" is \"%u\"\n"), svs.maxclients);
		return;

	}

	if (sv.active)
	{
		Con_Printf(const_cast<char*>("maxplayers cannot be changed while a server is running.\n"));
		return;
	}


	int n = Q_atoi(Cmd_Argv(1));
	if (n < 1)
		n = 1;

	if (n > svs.maxclientslimit)
	{
		n = svs.maxclientslimit;
		Con_Printf(const_cast<char*>("\"maxplayers\" set to \"%u\"\n"), svs.maxclientslimit);
	}
	svs.maxclients = n;

	if (n == 1)
		Cvar_Set(const_cast<char*>("deathmatch"), const_cast<char*>("0"));
	else
		Cvar_Set(const_cast<char*>("deathmatch"), const_cast<char*>("1"));
}

void NET_InitColors()
{
	/*double basecolor;
	float bluefrac[2];

	for (int i = 0; i < 24; i++)
	{
#if !defined (WIN32)
		if (i > 7)
		{
			basecolor = (float)(i - 8) / 16.0f;
			colors[i].r = 255 - basecolor * 5.0f;
			colors[i].g = 127 - basecolor * 127.0f;
			bluefrac[0] = 0.0f;
			bluefrac[1] = basecolor * bluefrac[0];
		}
		else
#endif
		{
			basecolor = (float)(i) / 8.0f;
			colors[i].r = 63 - basecolor * 63.0f;
			colors[i].g = 0 + basecolor * 63.0f;
			bluefrac[0] = 100.0f;
			bluefrac[1] = basecolor * 155.0f;
		}
		colors[i].b = bluefrac[0] + bluefrac[1];
	}*/

	int i, j;
	byte mincolor[2][3];
	byte maxcolor[2][3];
	float	dc[2][3];
	int		hfrac;
	float	f;

	mincolor[0][0] = 63;
	mincolor[0][1] = 0;
	mincolor[0][2] = 100;

	maxcolor[0][0] = 0;
	maxcolor[0][1] = 63;
	maxcolor[0][2] = 255;

	mincolor[1][0] = 255;
	mincolor[1][1] = 127;
	mincolor[1][2] = 0;

	maxcolor[1][0] = 250;
	maxcolor[1][1] = 0;
	maxcolor[1][2] = 0;

	for (i = 0; i < 3; i++)
	{
		dc[0][i] = (float)(maxcolor[0][i] - mincolor[0][i]);
		dc[1][i] = (float)(maxcolor[1][i] - mincolor[1][i]);
	}

	hfrac = LERP_HEIGHT / 3;

	for (i = 0; i < LERP_HEIGHT; i++)
	{
		if (i < hfrac)
		{
			f = (float)i / (float)hfrac;
			for (j = 0; j < 3; j++)
			{
				colors[i][j] = mincolor[0][j] + f * dc[0][j];
			}
		}
		else
		{
			f = (float)(i - hfrac) / (float)(LERP_HEIGHT - hfrac);
			for (j = 0; j < 3; j++)
			{
				colors[i][j] = mincolor[1][j] + f * dc[1][j];
			}
		}
	}
}

void NET_Init()
{
	NET_InitColors();

	Cmd_AddCommand(const_cast<char*>("maxplayers"), MaxPlayers_f);

	Cvar_RegisterVariable(&net_address);
	Cvar_RegisterVariable(&ipname);
	Cvar_RegisterVariable(&iphostport);
	Cvar_RegisterVariable(&hostport);
	Cvar_RegisterVariable(&defport);
	Cvar_RegisterVariable(&ip_clientport);
	Cvar_RegisterVariable(&clientport);
	Cvar_RegisterVariable(&clockwindow);
	Cvar_RegisterVariable(&multicastport);
	Cvar_RegisterVariable(&ipx_hostport);
	Cvar_RegisterVariable(&ipx_clientport);
	Cvar_RegisterVariable(&fakelag);
	Cvar_RegisterVariable(&fakeloss);
	Cvar_RegisterVariable(&net_graph);
	Cvar_RegisterVariable(&net_graphwidth);
	Cvar_RegisterVariable(&net_scale);
	Cvar_RegisterVariable(&net_graphpos);

	if (COM_CheckParm(const_cast<char*>("-netthread")))
		use_thread = TRUE;

	if (COM_CheckParm(const_cast<char*>("-netsleep")))
		net_sleepforever = 0;

	if (COM_CheckParm(const_cast<char*>("-noipx")))
		noipx = TRUE;

	if (COM_CheckParm(const_cast<char*>("-noip")))
		noip = TRUE;

	int port = COM_CheckParm(const_cast<char*>("-port"));
	if (port)
		Cvar_SetValue(const_cast<char*>("hostport"), Q_atof(com_argv[port + 1]));

	int clockwindow_ = COM_CheckParm(const_cast<char*>("-clockwindow"));
	if (clockwindow_)
		Cvar_SetValue(const_cast<char*>("clockwindow"), Q_atof(com_argv[clockwindow_ + 1]));

	net_message.data = (byte*)&net_message_buffer;
	net_message.maxsize = sizeof(net_message_buffer);
	net_message.flags = 0;
	net_message.buffername = "net_message";

	in_message.data = (byte*)&in_message_buf;
	in_message.maxsize = sizeof(in_message_buf);
	in_message.flags = 0;
	in_message.buffername = "in_message";

	for (int i = 0; i < NS_MAX; i++)
	{
		g_pLagData[i].pPrev = &g_pLagData[i];
		g_pLagData[i].pNext = &g_pLagData[i];
	}

	NET_AllocateQueues();
	Con_DPrintf(const_cast<char*>("Base networking initialized.\n"));

}

void NET_ClearLagData(qboolean bClient, qboolean bServer)
{
	NET_ThreadLock();

	if (bClient)
	{
		NET_ClearLaggedList(&g_pLagData[0]);
		NET_ClearLaggedList(&g_pLagData[2]);
	}

	if (bServer)
	{
		NET_ClearLaggedList(&g_pLagData[1]);
	}

	NET_ThreadUnlock();
}


char* NET_AdrToString(netadr_t a)
{
	static char s[64];

	Q_memset(s, 0, sizeof(s));

	if (a.type == NA_LOOPBACK)
		_snprintf(s, sizeof(s), "loopback");
	else if (a.type == NA_IP)
		_snprintf(s, sizeof(s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));
	else 
		_snprintf(s, sizeof(s), "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%i", a.ipx[0], a.ipx[1], a.ipx[2], a.ipx[3], a.ipx[4], a.ipx[5], a.ipx[6], a.ipx[7], a.ipx[8], a.ipx[9], ntohs(a.port));

	return s;
}

void NET_SendLoopPacket(netsrc_t sock, int length, void* data, netadr_t to)
{
	NET_ThreadLock();

	loopback_t* loop = &loopbacks[sock ^ 1];

	int i = loop->send & (MAX_LOOPBACK - 1);
	loop->send++;

	Q_memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;

	NET_ThreadUnlock();
}

qboolean NET_StringToSockaddr(char* s, struct sockaddr* sadr)
{
	Q_memset(sadr, 0, sizeof(*sadr));

	// IPX support.
	if (Q_strlen(s) >= 24 && s[8] == ':' && s[21] == ':')
	{
		sadr->sa_family = AF_IPX;
		int val = 0;
		for (int i = 0; i < 20; i += 2)
		{
			if (s[i] == ':')
			{
				--i; // Skip one char on next iteration.
				continue;
			}
			// Convert from hexademical represantation to sockaddr
			char temp[3] = { s[i], s[i + 1], '\0' };
			sscanf(temp, "%x", &val);
			sadr->sa_data[i / 2] = (char)val;
		}

		sscanf(s + 22, "%u", &val);
		*(uint16*)&sadr->sa_data[10] = htons(val);

		return TRUE;
	}

	auto* sadr_in = (sockaddr_in*)sadr;

	sadr_in->sin_family = AF_INET;

	char copy[128];
	Q_strncpy(copy, s, sizeof(copy) - 1);
	copy[sizeof(copy) - 1] = 0;

	// Parse port
	char* colon = strchr(copy, ':');
	if (colon != nullptr)
	{
		*colon = '\0';
		sadr_in->sin_port = htons(Q_atoi(colon + 1));
	}
	else
	{
		sadr_in->sin_port = 0;
	}

	// Parse address
	sadr_in->sin_addr.s_addr = inet_addr(copy);
	if (sadr_in->sin_addr.s_addr == INADDR_NONE)
	{
		struct hostent* host = gethostbyname(copy);

		if (host == nullptr || host->h_addr == nullptr)
		{
			return FALSE;
		}

		sadr_in->sin_addr.s_addr = *(uint32*)host->h_addr;
	}
	return TRUE;
}


qboolean NET_StringToAdr(char* s, netadr_t* a)
{
	struct sockaddr sadr;

	if (Q_strcmp(s, "localhost") && Q_strcmp(s, "127.0.0.1"))
	{
		if (!NET_StringToSockaddr(s, &sadr))
		{
			return FALSE;
		}
		SockadrToNetadr(&sadr, a);
	}
	else
	{
		Q_memset(a, 0, sizeof(netadr_t));
		a->type = NA_LOOPBACK;
	}
	return TRUE;
}

char* NET_ErrorString(int code)
{
	switch (code)
	{
	case WSAEINTR: return const_cast<char*>("WSAEINTR");
	case WSAEBADF: return const_cast<char*>("WSAEBADF");
	case WSAEACCES: return const_cast<char*>("WSAEACCES");
	case WSAEFAULT: return const_cast<char*>("WSAEFAULT");

	case WSAEINVAL: return const_cast<char*>("WSAEINVAL");
	case WSAEMFILE: return const_cast<char*>("WSAEMFILE");
	case WSAEWOULDBLOCK: return const_cast<char*>("WSAEWOULDBLOCK");
	case WSAEINPROGRESS: return const_cast<char*>("WSAEINPROGRESS");
	case WSAEALREADY: return const_cast<char*>("WSAEALREADY");
	case WSAENOTSOCK: return const_cast<char*>("WSAENOTSOCK");
	case WSAEDESTADDRREQ: return const_cast<char*>("WSAEDESTADDRREQ");

	case WSAEMSGSIZE: return const_cast<char*>("WSAEMSGSIZE");
	case WSAEPROTOTYPE: return const_cast<char*>("WSAEPROTOTYPE");
	case WSAENOPROTOOPT: return const_cast<char*>("WSAENOPROTOOPT");
	case WSAEPROTONOSUPPORT: return const_cast<char*>("WSAEPROTONOSUPPORT");
	case WSAESOCKTNOSUPPORT: return const_cast<char*>("WSAESOCKTNOSUPPORT");
	case WSAEOPNOTSUPP: return const_cast<char*>("WSAEOPNOTSUPP");
	case WSAEPFNOSUPPORT: return const_cast<char*>("WSAEPFNOSUPPORT");
	case WSAEAFNOSUPPORT: return const_cast<char*>("WSAEAFNOSUPPORT");
	case WSAEADDRINUSE: return const_cast<char*>("WSAEADDRINUSE");
	case WSAEADDRNOTAVAIL: return const_cast<char*>("WSAEADDRNOTAVAIL");
	case WSAENETDOWN: return const_cast<char*>("WSAENETDOWN");

	case WSAENETUNREACH: return const_cast<char*>("WSAENETUNREACH");
	case WSAENETRESET: return const_cast<char*>("WSAENETRESET");
	case WSAECONNABORTED: return const_cast<char*>("WSWSAECONNABORTEDAEINTR");
	case WSAECONNRESET: return const_cast<char*>("WSAECONNRESET");
	case WSAENOBUFS: return const_cast<char*>("WSAENOBUFS");
	case WSAEISCONN: return const_cast<char*>("WSAEISCONN");
	case WSAENOTCONN: return const_cast<char*>("WSAENOTCONN");
	case WSAESHUTDOWN: return const_cast<char*>("WSAESHUTDOWN");
	case WSAETOOMANYREFS: return const_cast<char*>("WSAETOOMANYREFS");
	case WSAETIMEDOUT: return const_cast<char*>("WSAETIMEDOUT");
	case WSAECONNREFUSED: return const_cast<char*>("WSAECONNREFUSED");
	case WSAELOOP: return const_cast<char*>("WSAELOOP");
	case WSAENAMETOOLONG: return const_cast<char*>("WSAENAMETOOLONG");
	case WSAEHOSTDOWN: return const_cast<char*>("WSAEHOSTDOWN");

	case WSASYSNOTREADY: return const_cast<char*>("WSASYSNOTREADY");
	case WSAVERNOTSUPPORTED: return const_cast<char*>("WSAVERNOTSUPPORTED");
	case WSANOTINITIALISED: return const_cast<char*>("WSANOTINITIALISED");
	case WSAEDISCON: return const_cast<char*>("WSAEDISCON");

	case WSAHOST_NOT_FOUND: return const_cast<char*>("WSAHOST_NOT_FOUND");
	case WSATRY_AGAIN: return const_cast<char*>("WSATRY_AGAIN");
	case WSANO_RECOVERY: return const_cast<char*>("WSANO_RECOVERY");
	case WSANO_DATA: return const_cast<char*>("WSANO_DATA");

	default: return const_cast<char*>("NO ERROR");
	}
}

void NET_TransferRawData(sizebuf_t* msg, unsigned char* pStart, int nSize)
{
	DbgPrint(m_fLogStream, "new packet of %d bytes received:\r\n", nSize);
	LogArray(m_fLogStream, pStart, nSize);
	DbgPrint(m_fLogStream, "\r\n");

	Q_memcpy(msg->data, pStart, nSize);
	msg->cursize = nSize;
}


void NET_RemoveFromPacketList(packetlag_t* pPacket)
{
	pPacket->pPrev->pNext = pPacket->pNext;
	pPacket->pNext->pPrev = pPacket->pPrev;
	pPacket->pPrev = 0;
	pPacket->pNext = 0;
}

void NET_ClearLaggedList(packetlag_t* pList)
{
	packetlag_t* p = pList->pNext;
	while (p && p != pList)
	{
		packetlag_t* n = p->pNext;
		NET_RemoveFromPacketList(p);
		if (p->pPacketData)
		{
			Mem_Free(p->pPacketData);
			p->pPacketData = NULL;
		}
		Mem_Free(p);
		p = n;
	}

	pList->pPrev = pList;
	pList->pNext = pList;
}

void NET_AddToLagged(netsrc_t sock, packetlag_t* pList, packetlag_t* pPacket, netadr_t* net_from_, sizebuf_t messagedata, float timestamp)
{
	unsigned char* pStart;

	if (pPacket->pPrev || pPacket->pNext)
	{
		Con_Printf(const_cast<char*>("Packet already linked\n"));
		return;
	}

	pPacket->pPrev = pList->pPrev;
	pList->pPrev->pNext = pPacket;
	pList->pPrev = pPacket;
	pPacket->pNext = pList;

	pStart = (unsigned char*)Mem_Malloc(messagedata.cursize);
	Q_memcpy(pStart, messagedata.data, messagedata.cursize);
	pPacket->pPacketData = pStart;
	pPacket->nSize = messagedata.cursize;
	pPacket->receivedTime = timestamp;
	Q_memcpy(&pPacket->net_from_, net_from_, sizeof(netadr_t));
}

qboolean NET_LagPacket(qboolean newdata, netsrc_t sock, netadr_t* from, sizebuf_t* data)
{
	if (gFakeLag <= 0.0)
	{
		NET_ClearLagData(TRUE, TRUE);
		return newdata;
	}

	float curtime = realtime;
	if (newdata)
	{
		if (fakeloss.value != 0.0)
		{
			if (sv_cheats.value)
			{
				static int losscount[NS_MAX] = {};
				++losscount[sock];
				if (fakeloss.value <= 0.0f)
				{
					int ninterval = fabs(fakeloss.value);
					if (ninterval < 2)
						ninterval = 2;
					if ((losscount[sock] % ninterval) == 0)
						return FALSE;
				}
				else
				{
					if (RandomLong(0, 100) <= fakeloss.value)
						return FALSE;
				}
			}
			else
			{
				Cvar_SetValue(const_cast<char*>("fakeloss"), 0.0);
			}
		}

		packetlag_t* pNewPacketLag = (packetlag_t*)Mem_ZeroMalloc(sizeof(packetlag_t));
		NET_AddToLagged(sock, &g_pLagData[sock], pNewPacketLag, from, *data, curtime);
	}

	packetlag_t* pPacket = g_pLagData[sock].pNext;

	while (pPacket != &g_pLagData[sock])
	{
		if (pPacket->receivedTime <= curtime - gFakeLag / 1000.0)
			break;

		pPacket = pPacket->pNext;
	}

	if (pPacket == &g_pLagData[sock])
		return FALSE;

	NET_RemoveFromPacketList(pPacket);
	NET_TransferRawData(&in_message, pPacket->pPacketData, pPacket->nSize);
	Q_memcpy(&in_from, &pPacket->net_from_, sizeof(in_from));
	if (pPacket->pPacketData)
		free(pPacket->pPacketData);

	Mem_Free(pPacket);
	return TRUE;
}

void NET_FlushSocket(netsrc_t sock)
{
	SOCKET net_socket = ip_sockets[sock];
	if (net_socket != INV_SOCK)
	{
		struct sockaddr from;
		int fromlen;
		unsigned char buf[MAX_UDP_PACKET];

		fromlen = 16;
		while (recvfrom(net_socket, (char*)buf, sizeof buf, 0, &from, &fromlen) > 0)
			;
	}
}

qboolean NET_GetLong(unsigned char* pData, int size, int* outSize)
{
	static int gNetSplitFlags[NET_WS_MAX_FRAGMENTS];
	SPLITPACKET* pHeader = (SPLITPACKET*)pData;

	int sequenceNumber = pHeader->sequenceNumber;
	unsigned char packetID = pHeader->packetID;
	unsigned int packetCount = packetID & 0xF;
	unsigned int packetNumber = (unsigned int)packetID >> 4;

	if (packetNumber >= NET_WS_MAX_FRAGMENTS || packetCount > NET_WS_MAX_FRAGMENTS)
	{
		Con_Printf(const_cast<char*>("Malformed packet number (%i/%i)\n"), packetNumber + 1, packetCount);
		return FALSE;
	}

	if (gNetSplit.currentSequence == -1 || sequenceNumber != gNetSplit.currentSequence)
	{
		gNetSplit.currentSequence = pHeader->sequenceNumber;
		gNetSplit.splitCount = packetCount;

		if (net_showpackets.value == 4.0f)
			Con_Printf(const_cast<char*>("<-- Split packet restart %i count %i seq\n"), gNetSplit.splitCount, sequenceNumber);
	}

	unsigned int packetPayloadSize = size - sizeof(SPLITPACKET);
	if (gNetSplitFlags[packetNumber] == sequenceNumber)
	{
		Con_Printf(const_cast<char*>(__FUNCTION__ ":  Ignoring duplicated split packet %i of %i ( %i bytes )\n"), packetNumber + 1, packetCount, packetPayloadSize);
	}
	else
	{
		if (packetNumber == packetCount - 1)
			gNetSplit.totalSize = packetPayloadSize + SPLIT_SIZE * (packetCount - 1);

		--gNetSplit.splitCount;
		gNetSplitFlags[packetNumber] = sequenceNumber;

		if (net_showpackets.value == 4.0f)
		{
			Con_Printf(const_cast<char*>("<-- Split packet %i of %i, %i bytes %i seq\n"), packetNumber + 1, packetCount, packetPayloadSize, sequenceNumber);
		}

		if (SPLIT_SIZE * packetNumber + packetPayloadSize > MAX_UDP_PACKET)
		{
			Con_Printf(const_cast<char*>("Malformed packet size (%i, %i)\n"), SPLIT_SIZE * packetNumber, packetPayloadSize);
			return FALSE;
		}

		Q_memcpy(&gNetSplit.buffer[SPLIT_SIZE * packetNumber], pHeader + 1, packetPayloadSize);
	}

	if (gNetSplit.splitCount > 0)
		return FALSE;

	if (packetCount > 0)
	{
		for (unsigned int i = 0; i < packetCount; i++)
		{
			if (gNetSplitFlags[i] != gNetSplit.currentSequence)
			{
				Con_Printf(
					const_cast<char*>("Split packet without all %i parts, part %i had wrong sequence %i/%i\n"),
					packetCount,
					i + 1,
					gNetSplitFlags[i],
					gNetSplit.currentSequence);
				return FALSE;
			}
		}
	}

	gNetSplit.currentSequence = -1;
	if (gNetSplit.totalSize <= MAX_UDP_PACKET)
	{
		Q_memcpy(pData, gNetSplit.buffer, gNetSplit.totalSize);
		*outSize = gNetSplit.totalSize;
		return TRUE;
	}
	else
	{
		Con_Printf(const_cast<char*>("Split packet too large! %d bytes\n"), gNetSplit.totalSize);
		return FALSE;
	}
}

qboolean NET_QueuePacket(netsrc_t sock)
{

	int ret = -1;
	unsigned char buf[MAX_UDP_PACKET];

	for (int protocol = 0; protocol < 2; protocol++)
	{
		SOCKET net_socket;

		if (protocol == 0)
			net_socket = ip_sockets[sock];
		else
			net_socket = ipx_sockets[sock];

		if (net_socket == INV_SOCK)
			continue;

		struct sockaddr from;
		int fromlen = sizeof(from);
		ret = recvfrom(net_socket, (char*)buf, sizeof buf, 0, &from, &fromlen);
		if (ret == -1)
		{
			int err = WSAGetLastError();

			if (err == WSAENETRESET)
				continue;

			if (err != WSAEWOULDBLOCK && err != WSAECONNRESET && err != WSAECONNREFUSED)
			{
				if (err == WSAEMSGSIZE)
				{
					Con_DPrintf(const_cast<char*>(__FUNCTION__ ":  Ignoring oversized network message\n"));
				}
				else
				{
					if (cls.state != ca_dedicated)
					{
						Sys_Error(__FUNCTION__ ": %s", NET_ErrorString(err));
					}
					Con_Printf(const_cast<char*>(__FUNCTION__ ": %s\n"), NET_ErrorString(err));
				}
			}
			continue;
		}

		SockadrToNetadr(&from, &in_from);
		if (ret != MAX_UDP_PACKET)
			break;

		Con_Printf(const_cast<char*>(__FUNCTION__ ": Oversize packet from %s\n"), NET_AdrToString(in_from));
	}

	if (ret == -1 || ret == MAX_UDP_PACKET)
	{
		return NET_LagPacket(FALSE, sock, NULL, NULL);
	}

	NET_TransferRawData(&in_message, buf, ret);

	if (*(int32*)in_message.data != NET_HEADER_FLAG_SPLITPACKET)
	{
		return NET_LagPacket(TRUE, sock, &in_from, &in_message);
	}

	if (in_message.cursize < 9)
	{
		Con_Printf(const_cast<char*>("Invalid split packet length %i\n"), in_message.cursize);
		return FALSE;
	}

	return NET_GetLong(in_message.data, ret, &in_message.cursize);

}


int NET_Sleep()
{
	fd_set fdset;
	FD_ZERO(&fdset);

	SOCKET number = 0;
	for (int sock = 0; sock < NS_MAX; sock++)
	{
		SOCKET net_socket = ip_sockets[sock];
		if (net_socket != INV_SOCK)
		{
			FD_SET(net_socket, &fdset);

			if (number < net_socket)
				number = net_socket;
		}

		net_socket = ipx_sockets[sock];
		if (net_socket != INV_SOCK)
		{
			FD_SET(net_socket, &fdset);

			if (number < net_socket)
				number = net_socket;
		}
	}

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 20 * 1000;

	return select((int)(number + 1), &fdset, NULL, NULL, net_sleepforever == 0 ? &tv : NULL);
}

DWORD WINAPI NET_ThreadMain(LPVOID lpThreadParameter)
{
	while (true)
	{
		while (NET_Sleep())
		{
			qboolean bret = FALSE;
			for (int sock = 0; sock < NS_MAX; sock++)
			{
				NET_ThreadLock();

				bret = NET_QueuePacket((netsrc_t)sock);
				if (bret)
				{
					net_messages_t* pmsg = NET_AllocMsg(in_message.cursize);
					pmsg->next = nullptr;
					Q_memcpy(pmsg->buffer, in_message.data, in_message.cursize);
					Q_memcpy(&pmsg->from, &in_from, sizeof(pmsg->from));

					// add to tail of the list
					net_messages_t* p = messages[sock];
					if (p)
					{
						while (p->next)
							p = p->next;

						p->next = pmsg;
					}
					// add to head
					else
					{
						messages[sock] = pmsg;
					}
				}

				NET_ThreadUnlock();
			}

			if (!bret)
				break;
		}

		Sys_Sleep(1);
	}

	return 0;
}

void NET_StartThread()
{
	if (use_thread)
	{
		if (!net_thread_initialized)
		{
			net_thread_initialized = TRUE;

			InitializeCriticalSection(&net_cs);
			hNetThread = CreateThread(0, 0, NET_ThreadMain, 0, 0, &dwNetThreadId);
			if (!hNetThread)
			{
				DeleteCriticalSection(&net_cs);
				net_thread_initialized = FALSE;
				use_thread = FALSE;
				Sys_Error("Couldn't initialize network thread, run without -netthread\n");
			}
		}
	}
}

void NET_StopThread()
{
	if (use_thread)
	{
		if (net_thread_initialized)
		{
			TerminateThread(hNetThread, 0);
			DeleteCriticalSection(&net_cs);
			net_thread_initialized = FALSE;
		}
	}
}

void* net_malloc(size_t size)
{
	return Mem_ZeroMalloc(size);
}

net_messages_t* NET_AllocMsg(int size)
{
	net_messages_t* pmsg;
	if (size <= MSG_QUEUE_SIZE && normalqueue)
	{
		pmsg = normalqueue;
		pmsg->buffersize = size;
		normalqueue = pmsg->next;
	}
	else
	{
		pmsg = (net_messages_t*)net_malloc(sizeof(net_messages_t));
		pmsg->buffer = (unsigned char*)net_malloc(size);
		pmsg->buffersize = size;
		pmsg->preallocated = FALSE;
	}

	return pmsg;
}

void NET_AllocateQueues()
{
	for (int i = 0; i < NUM_MSG_QUEUES; i++)
	{
		net_messages_t* p = (net_messages_t*)Mem_ZeroMalloc(sizeof(net_messages_t));
		p->buffer = (unsigned char*)Mem_ZeroMalloc(MSG_QUEUE_SIZE);
		p->preallocated = TRUE;
		p->next = normalqueue;
		normalqueue = p;
	}

	NET_StartThread();
}

void NET_FlushQueues()
{
	NET_StopThread();

	for (int i = 0; i < NS_MAX; i++)
	{
		net_messages_t* p = messages[i];
		while (p)
		{
			net_messages_t* n = p->next;
			Mem_Free(p->buffer);
			Mem_Free(p);
			p = n;
		}

		messages[i] = NULL;
	}

	net_messages_t* p = normalqueue;
	while (p)
	{
		net_messages_t* n = p->next;
		Mem_Free(p->buffer);
		Mem_Free(p);
		p = n;
	}
	normalqueue = NULL;
}

int NET_SendLong(netsrc_t sock, SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
	static long gSequenceNumber = 1;

	// Do we need to break this packet up?
	if (sock == NS_SERVER && len > MAX_ROUTEABLE_PACKET)
	{
		// yep
		gSequenceNumber++;
		if (gSequenceNumber < 0)
		{
			gSequenceNumber = 1;
		}

		char packet[MAX_ROUTEABLE_PACKET];
		SPLITPACKET* pPacket = (SPLITPACKET*)packet;
		pPacket->netID = NET_HEADER_FLAG_SPLITPACKET;
		pPacket->sequenceNumber = gSequenceNumber;
		int packetNumber = 0;
		int totalSent = 0;
		int packetCount = (len + SPLIT_SIZE - 1) / SPLIT_SIZE;

		while (len > 0)
		{
			int size = min(int(SPLIT_SIZE), len);

			pPacket->packetID = (packetNumber << 4) + packetCount;

			Q_memcpy(packet + sizeof(SPLITPACKET), buf + (packetNumber * SPLIT_SIZE), size);

			if (net_showpackets.value == 4.0f)
			{
				netadr_t adr;
				Q_memset(&adr, 0, sizeof(adr));

				SockadrToNetadr((struct sockaddr*)to, &adr);

				Con_DPrintf(const_cast<char*>("Sending split %i of %i with %i bytes and seq %i to %s\n"),
					packetNumber + 1,
					packetCount,
					size,
					gSequenceNumber,
					NET_AdrToString(adr));
			}

			int ret = sendto(s, packet, size + sizeof(SPLITPACKET), flags, to, tolen);
			if (ret < 0)
			{
				return ret;
			}

			if (ret >= size)
			{
				totalSent += size;
			}

			len -= size;
			packetNumber++;
		}

		return totalSent;
	}

	int nSend = sendto(s, buf, len, flags, to, tolen);
	return nSend;
}

void NET_SendPacket(netsrc_t sock, int length, void* data, netadr_t to)
{
	DbgPrint(m_fLogStream, "sent one packet of %d bytes from %d:\r\n", length, sock);
	LogArray(m_fLogStream, (LPBYTE)data, length);
	DbgPrint(m_fLogStream, "\r\n");

	if (to.type == NA_LOOPBACK)
	{
		NET_SendLoopPacket(sock, length, data, to);
		return;
	}

	SOCKET net_socket;
	if (to.type == NA_BROADCAST)
	{
		net_socket = ip_sockets[sock];
		if (net_socket == INV_SOCK)
			return;
	}
	else if (to.type == NA_IP)
	{
		net_socket = ip_sockets[sock];
		if (net_socket == INV_SOCK)
			return;
	}
	else if (to.type == NA_IPX)
	{
		net_socket = ipx_sockets[sock];
		if (net_socket == INV_SOCK)
			return;
	}
	else if (to.type == NA_BROADCAST_IPX)
	{
		net_socket = ipx_sockets[sock];
		if (net_socket == INV_SOCK)
			return;
	}
	else
	{
		Sys_Error(__FUNCTION__ ": bad address type");
	}

	struct sockaddr addr;
	NetadrToSockadr(&to, &addr);

	int ret = NET_SendLong(sock, net_socket, (const char*)data, length, 0, &addr, sizeof(addr));
	if (ret == -1)
	{
		int err = WSAGetLastError();

		// wouldblock is silent
		if (err == WSAEWOULDBLOCK)
			return;

		if (err == WSAECONNREFUSED || err == WSAECONNRESET)
			return;

		// some PPP links dont allow broadcasts
		if (err == WSAEADDRNOTAVAIL && (to.type == NA_BROADCAST
			|| to.type == NA_BROADCAST_IPX
			))
			return;

		// let dedicated servers continue after errors
		if (cls.state == ca_dedicated)
		{
			Con_Printf(const_cast<char*>(__FUNCTION__ ": ERROR: %s\n"), NET_ErrorString(err));
		}
		else
		{
			if (err == WSAEADDRNOTAVAIL || err == WSAENOBUFS)
			{
				Con_DPrintf(const_cast<char*>(__FUNCTION__ ": Warning: %s : %s\n"), NET_ErrorString(err), NET_AdrToString(to));
			}
			else
			{
				Sys_Error(__FUNCTION__ ": ERROR: %s\n", NET_ErrorString(err));
			}
		}
	}


}

qboolean NET_CompareClassBAdr(netadr_t a, netadr_t b)
{
	if (a.type != b.type)
	{
		return FALSE;
	}
	if (a.type == NA_LOOPBACK)
	{
		return TRUE;
	}

	if (a.type == NA_IP)
	{
		if (a.ip[0] == b.ip[0] &&
			a.ip[1] == b.ip[1])
			return TRUE;
	}
#ifdef _WIN32
	else if (a.type == NA_IPX)
	{
		return TRUE;
	}
#endif // _WIN32

	return FALSE;
}

qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b)
{
	if (a.type != b.type)
	{
		return FALSE;
	}
	if (a.type == NA_LOOPBACK)
	{
		return TRUE;
	}

	if (a.type == NA_IP)
	{
		if (a.ip[0] == b.ip[0] &&
			a.ip[1] == b.ip[1] &&
			a.ip[2] == b.ip[2] &&
			a.ip[3] == b.ip[3])
			return TRUE;
	}
#ifdef _WIN32
	else if (a.type == NA_IPX)
	{
		if (Q_memcmp(a.ipx, b.ipx, 10) == 0)
			return TRUE;
	}
#endif // _WIN32

	return FALSE;
}

char* NET_BaseAdrToString(netadr_t a)
{
	static char s[64];

	if (a.type == NA_LOOPBACK)
	{
		_snprintf(s, sizeof(s), "loopback");
	}
	else if (a.type == NA_IP)
	{
		_snprintf(s, sizeof(s), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);
	}
#ifdef _WIN32
	else // NA_IPX
	{
		_snprintf(s, sizeof(s), "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x", a.ipx[0], a.ipx[1], a.ipx[2], a.ipx[3], a.ipx[4], a.ipx[5], a.ipx[6], a.ipx[7], a.ipx[8], a.ipx[9]);
	}
#endif // _WIN32

	return s;
}

qboolean NET_IsLocalAddress(netadr_t adr)
{
	return adr.type == NA_LOOPBACK ? TRUE : FALSE;
}

qboolean NET_IsReservedAdr(netadr_t a)
{
	if (a.type == NA_LOOPBACK)
	{
		return TRUE;
	}
	if (a.type == NA_IP)
	{
		if (a.ip[0] == 10 || a.ip[0] == 127)
		{
			return TRUE;
		}
		if (a.ip[0] == 172 && a.ip[1] >= 16)
		{
			if (a.ip[1] >= 32)
			{
				return FALSE;
			}
			return TRUE;
		}
		if (a.ip[0] == 192 && a.ip[1] >= 168)
		{
			return TRUE;
		}
		return FALSE;
	}
#ifdef _WIN32
	else if (a.type == NA_IPX)
	{
		return TRUE;
	}
#endif // _WIN32

	return FALSE;
}

qboolean NET_GetLoopPacket(netsrc_t sock, netadr_t* in_from, sizebuf_t* msg)
{
	loopback_t* loop = &loopbacks[sock];

	if (loop->send - loop->get > 4)
	{
		loop->get = loop->send - 4;
	}

	if (loop->get < loop->send)
	{
		int i = loop->get & (MAX_LOOPBACK - 1);
		loop->get++;

		NET_TransferRawData(msg, loop->msgs[i].data, loop->msgs[i].datalen);

		Q_memset(in_from, 0, sizeof(netadr_t));
		in_from->type = NA_LOOPBACK;

		return TRUE;
	}
	return FALSE;
}

void NET_FreeMsg(net_messages_t* pmsg)
{
	if (pmsg->preallocated)
	{
		net_messages_t* tmp = normalqueue;
		normalqueue = pmsg;
		pmsg->next = tmp;
	}
	else
	{
		Mem_Free(pmsg->buffer);
		Mem_Free(pmsg);
	}
}

void NET_AdjustLag()
{
	static double lasttime = realtime;

	double dt = realtime - lasttime;
	if (dt <= 0.0)
	{
		dt = 0.0;
	}
	else
	{
		if (dt > 0.1)
		{
			dt = 0.1;
		}
	}
	lasttime = realtime;

	if (sv_cheats.value || fakelag.value == 0.0)
	{
		if (fakelag.value != gFakeLag)
		{
			float diff = fakelag.value - gFakeLag;
			float converge = dt * 200.0;
			if (fabs(diff) < converge)
				converge = fabs(diff);
			if (diff < 0.0)
				converge = -converge;
			gFakeLag = gFakeLag + converge;
		}
	}
	else
	{
		Con_Printf(const_cast<char*>("Server must enable cheats to activate fakelag\n"));
		Cvar_SetValue(const_cast<char*>("fakelag"), 0.0);
		gFakeLag = 0;
	}
}

qboolean NET_GetPacket(netsrc_t sock)
{
	qboolean bret;

	NET_AdjustLag();
	NET_ThreadLock();
	if (NET_GetLoopPacket(sock, &in_from, &in_message))
	{

		DbgPrint(m_fLogStream, "got one packet of %d bytes from %d:\r\n", in_message.cursize, in_from);
		LogArray(m_fLogStream, (LPBYTE)in_message.data, in_message.cursize);
		DbgPrint(m_fLogStream, "\r\n");

		bret = NET_LagPacket(TRUE, sock, &in_from, &in_message);
	}
	else
	{
		if (!use_thread)
		{
			bret = NET_QueuePacket(sock);
			if (!bret)
				bret = NET_LagPacket(FALSE, sock, NULL, NULL);
		}
		else
		{
			bret = NET_LagPacket(FALSE, sock, NULL, NULL);
		}
	}

	if (bret)
	{
		Q_memcpy(net_message.data, in_message.data, in_message.cursize);
		net_message.cursize = in_message.cursize;
		Q_memcpy(&net_from, &in_from, sizeof(netadr_t));
		NET_ThreadUnlock();
		return bret;
	}

	net_messages_t* pmsg = messages[sock];
	if (pmsg)
	{
		net_message.cursize = pmsg->buffersize;
		messages[sock] = pmsg->next;
		Q_memcpy(net_message.data, pmsg->buffer, net_message.cursize);
		net_from = pmsg->from;
		msg_readcount = 0;
		NET_FreeMsg(pmsg);
		bret = TRUE;
	}
	NET_ThreadUnlock();
	return bret;
}

qboolean NET_JoinGroup(netsrc_t sock, netadr_t addr)
{
	ip_mreq mreq;
	mreq.imr_multiaddr.S_un.S_addr = *(unsigned int*)&addr.ip[0];
	mreq.imr_interface.S_un.S_addr = 0;

	SOCKET net_socket = ip_sockets[sock];
	if (setsockopt(net_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSAEAFNOSUPPORT)
		{
			Con_Printf(const_cast<char*>("WARNING: NET_JoinGroup: IP_ADD_MEMBERSHIP: %s"), NET_ErrorString(err));
		}
		return FALSE;
	}

	return TRUE;
}

qboolean NET_LeaveGroup(netsrc_t sock, netadr_t addr)
{
	ip_mreq mreq;
	mreq.imr_multiaddr.S_un.S_addr = *(unsigned int*)&addr.ip[0];
	mreq.imr_interface.S_un.S_addr = 0;

	SOCKET net_socket = ip_sockets[sock];
	if (setsockopt(net_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) != SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEAFNOSUPPORT)
		{
			return FALSE;
		}
	}

	return TRUE;
}

qboolean NET_AtEdge(int x, int width)
{
	return x <= 3 || x >= width - 4;
}

void NET_FillRect(vrect_t* rect, byte* color, byte alpha)
{
	Draw_FillRGBA(rect->x, rect->y, rect->width, rect->height, color[0], color[1], color[2], alpha);
}

void NET_DrawTimes(vrect_t vrect, cmdinfo_t* cmdinfo, int x, int w)
{
	int i;
	int j;
	int	extrap_point;
	int a, h;
	vrect_t  rcFill;
	int ptx, pty;

	// Draw cmd_rate value
	ptx = max(x + w - 1 - 25, 1);
	pty = max(vrect.y + vrect.height - 4 - LERP_HEIGHT + 1, 1);

	NET_DrawString(ptx, pty, 1, GRAPH_RED, GRAPH_GREEN, GRAPH_BLUE, const_cast<char*>("%i/s"), cl_cmdrate.value);

	extrap_point = LERP_HEIGHT / 3;

	for (a = 0; a < w; a++)
	{
		i = (cls.netchan.outgoing_sequence - a) & (TIMINGS - 1);
		h = min(int(cmdinfo[i].cmd_lerp / 3.0) * LERP_HEIGHT, LERP_HEIGHT);

		rcFill.x = x + w - a - 1;
		rcFill.width = 1;
		rcFill.height = 1;

		rcFill.y = vrect.y + vrect.height - 4;

		if (h >= extrap_point)
		{
			h -= extrap_point;
			rcFill.y -= extrap_point + h - 1;

			if (h - 1 >= 0)
			{
				for (int j = h - 1; j < h; j++)
				{
					NET_FillRect(&rcFill, colors[extrap_point + j], 255);
					rcFill.y--;
				}
			}
		}
		else
		{
			rcFill.y -= h;
			
			NET_FillRect(&rcFill, colors[h], 255);
		}

		rcFill.y = vrect.y + vrect.height - 4 - extrap_point;

		if (NET_AtEdge(a, w))
			NET_FillRect(&rcFill, extrap_base_color, 255);

		rcFill.y = vrect.y + vrect.height - 3;

		if (!cmdinfo[i].sent)
			NET_FillRect(&rcFill, holdcolor, 200);
	}
}

void NET_DrawString(int x, int y, int font, float r, float g, float b, char* fmt, ...)
{
	va_list fmtargs;
	static char string[1024];

	va_start(fmtargs, fmt);
	vsnprintf(string, sizeof(string), fmt, fmtargs);
	Draw_SetTextColor(r, g, b);
	Draw_String(x, y, string);
}

int NET_GraphValue(void)
{
	kbutton_t* in_graph;
	int graph_type;

	graph_type = net_graph.value;
	in_graph = ClientDLL_FindKey("in_graph");
	if (!graph_type)
	{
		if (in_graph && (in_graph->state & 1))
			return 2;
		
		return 0;
	}
	return graph_type;
}

/*
==================
NET_GetFrameData

Compute frame database for rendering netgraph computes choked, and lost packets, too.
 Also computes latency data and sets max packet size
==================
*/
void NET_GetFrameData(packet_latency_t* packet_latency, netbandwidthgraph_t* graph,
	int* choke_count, int* loss_count, int* biggest_message,
	float* latency, int* latency_count)
{
	int		i;
	int		frm;
	frame_t* frame;
	double	frame_received_time;
	double	frame_latency;

	*choke_count = 0;
	*loss_count = 0;
	*biggest_message = 0;
	*latency = 0.0;
	*latency_count = 0;

	// Fill in frame data
	for (i = cls.netchan.incoming_sequence - CL_UPDATE_BACKUP + 1;
		i <= cls.netchan.incoming_sequence;
		i++)
	{
		frm = i;

		frame = &cl.frames[frm & CL_UPDATE_MASK];

		packet_latency[frm & (TIMINGS - 1)].choked = frame->choked;
		if (frame->choked)
		{
			(*choke_count)++;
		}

		frame_received_time = frame->receivedtime;

		if (frame->invalid)
		{
			packet_latency[frm & (TIMINGS - 1)].latency = 9998;	// invalid delta
		}
		else if (frame_received_time == -1.0)
		{
			packet_latency[frm & (TIMINGS - 1)].latency = 9999; // dropped
			(*loss_count)++;
		}
		else if (frame_received_time == -3.0)
		{
			packet_latency[frm & (TIMINGS - 1)].latency = 9997;	// skipped b/c we are updating too fast
		}
		else
		{
			// Cap to 1 second latency in graph
			frame_latency = min(1.0, frame->latency);

			packet_latency[frm & (TIMINGS - 1)].latency = ((frame_latency + 0.1) / 1.1) * (scr_graphheight.value - LERP_HEIGHT - 2);

			// Only use last few samples
			if (i > (cls.netchan.incoming_sequence - NUM_LATENCY_SAMPLES))
			{
				*latency += 1000.0 * frame->latency;
				(*latency_count)++;
			}
		}

		graph[frm & (TIMINGS - 1)].client = frame->clientbytes;
		graph[frm & (TIMINGS - 1)].players = frame->playerinfobytes;
		graph[frm & (TIMINGS - 1)].entities = frame->packetentitybytes;
		graph[frm & (TIMINGS - 1)].tentities = frame->tentitybytes;
		graph[frm & (TIMINGS - 1)].sound = frame->soundbytes;
		graph[frm & (TIMINGS - 1)].event = frame->eventbytes;
		graph[frm & (TIMINGS - 1)].usr = frame->usrbytes;
		graph[frm & (TIMINGS - 1)].msgbytes = frame->msgbytes;
		graph[frm & (TIMINGS - 1)].voicebytes = frame->voicebytes;

		if (graph[frm & (TIMINGS - 1)].msgbytes > *biggest_message)
		{
			*biggest_message = graph[frm & (TIMINGS - 1)].msgbytes;
		}
	}

	if (*biggest_message > 1000)
	{
		*biggest_message = 1000;
	}
}

void NET_GetCommandInfo(cmdinfo_t *cmdinfo)
{
	int		i;

	for (i = cls.netchan.outgoing_sequence - CL_UPDATE_BACKUP + 1
		; i <= cls.netchan.outgoing_sequence
		; i++)
	{
		// Also set up the lerp point.
		cmdinfo[i & (TIMINGS - 1)].cmd_lerp = cl.commands[i & CL_UPDATE_MASK].frame_lerp;
		cmdinfo[i & (TIMINGS - 1)].sent = !cl.commands[i & CL_UPDATE_MASK].heldback;
		cmdinfo[i & (TIMINGS - 1)].size = cl.commands[i & CL_UPDATE_MASK].sendsize;
	}
}

void NET_GraphGetXY(vrect_t *rect, int width, int *x, int *y)
{
	*x = rect->x + 5;

	switch ((int)net_graphpos.value)
	{
	case 1:
		*x = rect->x + rect->width - 5 - width;
		break;
	case 2:
		*x = rect->x + (rect->width - 10 - width) / 2;
		break;
	case 0:
	default:
		break;
	}

	*y = rect->y + rect->height - LERP_HEIGHT - 5;
}

void NET_GetColorValues( int color, byte *cv, byte *alpha )
{
	int i;
	netcolor_t *pc = &netcolors[ color ];
	for ( i = 0; i < 3; i++ )
	{
		cv[ i ] = pc->color[ i ];
	}
	*alpha = pc->alpha;
}

void NET_ColorForHeight( packet_latency_t *packet, byte *color, int *ping, byte *alpha )
{
	int h = packet->latency;
	*ping = 0;
	switch ( h )
	{
	case 9999:
		NET_GetColorValues( COLOR_DROPPED, color, alpha );
		break;
	case 9998:
		NET_GetColorValues( COLOR_INVALID, color, alpha );
		break;
	case 9997:
		NET_GetColorValues( COLOR_SKIPPED, color, alpha );
		break;
	default:
		*ping = 1;
		NET_GetColorValues( packet->choked ? COLOR_CHOKED : COLOR_NORMAL, color, alpha );
		break;
	}
}

int NET_DrawDataSegment(vrect_t *rcFill, int bytes, byte r, byte g, byte b, byte alpha)
{
	int h;
	byte color[3];

	h = bytes / net_scale.value;

	color[0] = r;
	color[1] = g;
	color[2] = b;

	rcFill->height = h;
	rcFill->y -= h;

	if (rcFill->y < 2)
		return 0;

	NET_FillRect(rcFill, color, alpha);

	return 1;
}

void NET_DrawHatches(int x, int y, int maxmsgbytes)
{
	int starty;
	int ystep;
	vrect_t rcHatch;

	byte colorminor[3];
	byte color[3];

	ystep = (int)( 10.0 / net_scale.value );
	ystep = max( ystep, 1 );

	rcHatch.y		= y;
	rcHatch.height	= 1;
	rcHatch.x		= x;
	rcHatch.width	= 4;

	color[0] = 0;
	color[1] = 200;
	color[2] = 0;

	colorminor[0] = 63;
	colorminor[1] = 63;
	colorminor[2] = 0;

	for ( starty = rcHatch.y; rcHatch.y > 0 && ((starty - rcHatch.y)*net_scale.value < ( maxmsgbytes + 50 ) ); rcHatch.y -= ystep )
	{
		if ( !((int)((starty - rcHatch.y)*net_scale.value ) % 50 ) )
		{
			NET_FillRect( &rcHatch, color, 255 );
		}
		else if ( ystep > 5 )
		{
			NET_FillRect(&rcHatch, colorminor, 200);
		}
	}
}

void NET_DrawUpdateRate(int x, int y)
{
	NET_DrawString(x, y, 1, GRAPH_RED, GRAPH_GREEN, GRAPH_BLUE, const_cast<char*>("%i/s"), (int)cl_updaterate.value);
}

void NET_DrawTextFields(int graphvalue, int x, int y, netbandwidthgraph_t *graph, cmdinfo_t *cmdinfo,
	int count, float avg, float *framerate,
	int packet_loss, int packet_choke)
{
	static int lastout;

	char sz[256];
	int out;
	int localplayer;
	int otherplayers;

	if (count > 0)
	{
		avg /= (float)count;

		// Remove CPU influences ( average )
		avg -= host_frametime / 2.0;
		avg -= cl_updaterate.value;

		// Can't be below zero
		avg = max(0.f, avg);
	}
	else
	{
		avg = 0.0;
	}

	// Move rolling average
	*framerate = LATENCY_AVG_FRAC * (*framerate) + (1.0 - LATENCY_AVG_FRAC) * host_frametime;

	// Print it out
	y -= scr_graphheight.value;

	if ((*framerate) > 0.0)
	{
		_snprintf(sz, sizeof(sz), "%.1f fps", 1.0 / (*framerate));

		NET_DrawString(x, y, 1, GRAPH_RED, GRAPH_GREEN, GRAPH_BLUE, sz);

		if (avg > 1.0)
		{
			_snprintf(sz, sizeof(sz), "%i ms ", (int)avg);
			NET_DrawString(x + 75, y, 1, GRAPH_RED, GRAPH_GREEN, GRAPH_BLUE, sz);
		}

		y += 15;

		out = cmdinfo[((cls.netchan.outgoing_sequence - 1) & (TIMINGS - 1))].size;
		if (!out)
		{
			out = lastout;
		}
		else
		{
			lastout = out;
		}

		_snprintf(sz, sizeof(sz), "in :  %4i %.2f k/s", graph[(cls.netchan.incoming_sequence & (TIMINGS - 1))].msgbytes, cls.netchan.flow[FLOW_INCOMING].avgkbytespersec);

		NET_DrawString(x, y, 1, GRAPH_RED, GRAPH_GREEN, GRAPH_BLUE, sz);
		y += 15;

		_snprintf(sz, sizeof(sz), "out:  %4i %.2f k/s", out, cls.netchan.flow[FLOW_OUTGOING].avgkbytespersec);

		NET_DrawString(x, y, 1, GRAPH_RED, GRAPH_GREEN, GRAPH_BLUE, sz);
		y += 15;

		if (graphvalue > 2)
		{
			_snprintf(sz, sizeof(sz), "loss: %i choke: %i", (int)(packet_loss + 0.49), (int)(packet_choke + 0.49));
			NET_DrawString(x, y, 1, GRAPH_RED, GRAPH_GREEN, GRAPH_BLUE, sz);
			y += 15;
		}
	}
}

void SCR_NetGraph(void)
{
	int x, y;
	int w;
	vrect_t vrect;

	int			maxmsgbytes = 0;

	float		loss = 0.0;
	float		choke = 0.0;
	int			loss_count = 0;
	int			choke_count = 0;
	float		avg_ping = 0.0;
	int			ping_count = 0;

	int graphtype;
	kbutton_s *in_graph = ClientDLL_FindKey("in_graph");

	graphtype = net_graph.value;

	if (!graphtype && !(in_graph->state & 1))
		return;

	// With +graph key, use max area
	if (!graphtype)
	{
		graphtype = 2;
	}

	// Since we divide by scale, make sure it's sensible
	if (net_scale.value <= 0)
	{
		net_scale.value = 1.f;
	}

	// Get screen rectangle
	vrect.x = 0;
	vrect.y = 0;
	vrect.width = vid.width;
	vrect.height = vid.height;

	// Determine graph width
	w = min((int)TIMINGS, (int)net_graphwidth.value);
	if (vrect.width < w + 10)
	{
		w = vrect.width - 10;
	}

	NET_GetFrameData(packet_latency, graph, &choke_count, &loss_count, &maxmsgbytes, &avg_ping, &ping_count);

	// Packet loss
	loss = 100.0 * (float)loss_count / (float)CL_UPDATE_BACKUP;
	packet_loss = PACKETLOSS_AVG_FRAC * packet_loss + (1.0 - PACKETLOSS_AVG_FRAC) * loss;

	// Packet choking
	choke = 100.0 * (float)choke_count / (float)CL_UPDATE_BACKUP;
	packet_choke = PACKETCHOKE_AVG_FRAC * packet_choke + (1.0 - PACKETCHOKE_AVG_FRAC) * choke;

	// Grab cmd data
	NET_GetCommandInfo(cmdinfo);

	NET_GraphGetXY(&vrect, w, &x, &y);

	if (graphtype < 3)
	{
		int i;
		int h;
		int a;
		int lastvalidh = 0;

		byte		color[3];
		int			ping;
		byte		alpha;
		vrect_t		rcFill = { 0, 0, 0, 0 };

		for (a = 0; a < w; a++)
		{
			i = (cls.netchan.incoming_sequence - a) & (TIMINGS - 1);
			h = packet_latency[i].latency;

			NET_ColorForHeight(&packet_latency[i], color, &ping, &alpha);

			// Skipped
			if (!ping)
			{
				// Re-use the last latency
				h = lastvalidh;
			}
			else
			{
				lastvalidh = h;
			}

			if (h > (scr_graphheight.value - LERP_HEIGHT - 2))
			{
				h = scr_graphheight.value - LERP_HEIGHT - 2;
			}

			rcFill.x = x + w - a - 1;
			rcFill.y = y - h;
			rcFill.width = 1;

			if (ping)
			{
				rcFill.height = 1;
				NET_FillRect(&rcFill, color, alpha);
			}
			else
			{
				rcFill.height = h;
				if (rcFill.height <= 3)
				{
					NET_FillRect(&rcFill, color, alpha);
				}
				else
				{
					rcFill.height = 2;
					NET_FillRect(&rcFill, color, alpha);
					rcFill.y += rcFill.height - 2;
					NET_FillRect(&rcFill, color, alpha);
				}
			}

			rcFill.y = y;
			rcFill.height = 1;

			color[0] = 0;
			color[1] = 255;
			color[2] = 0;

			if (NET_AtEdge(a, w))
				NET_FillRect(&rcFill, color, 160);


			if (graphtype < 2)
				continue;

			// Draw a separator.
			rcFill.y = y - scr_graphheight.value - 1;
			rcFill.height = 1;

			color[0] = 255;
			color[1] = 255;
			color[2] = 255;

			NET_FillRect(&rcFill, color, 255);

			// Move up for begining of data
			rcFill.y -= 1;

			// Packet didn't have any real data...
			if (packet_latency[i].latency > 9995)
				continue;

			if (!NET_DrawDataSegment(&rcFill, graph[i].client, 255, 0, 0, 128))
				continue;

			if (!NET_DrawDataSegment(&rcFill, graph[i].players, 255, 255, 0, 128))
				continue;

			if (!NET_DrawDataSegment(&rcFill, graph[i].entities, 255, 0, 255, 128))
				continue;

			if (!NET_DrawDataSegment(&rcFill, graph[i].tentities, 0, 0, 255, 128))
				continue;

			if (!NET_DrawDataSegment(&rcFill, graph[i].sound, 0, 255, 0, 128))
				continue;

			if (!NET_DrawDataSegment(&rcFill, graph[i].event, 0, 255, 255, 128))
				continue;

			if (!NET_DrawDataSegment(&rcFill, graph[i].usr, 200, 200, 200, 128))
				continue;

			if (!NET_DrawDataSegment(&rcFill, graph[i].voicebytes, 255, 255, 255, 128))
				continue;

			// Final data chunk is total size, don't use solid line routine for this
			h = (float)graph[i].msgbytes / net_scale.value;

			color[0] = color[1] = color[2] = 240;

			rcFill.height = 1;
			rcFill.y = y - scr_graphheight.value - 1 - h;

			if (rcFill.y < 2)
				continue;

			NET_FillRect(&rcFill, color, 128);
		}

		if (graphtype == 2)
		{
			// Draw hatches for first one:
			// on the far right side
			NET_DrawHatches(x, y - scr_graphheight.value - 1, maxmsgbytes);
		}

		// Draw update rate
		NET_DrawUpdateRate(max(1, x + w - 25), max(1, y - (int)scr_graphheight.value - 1));
		
		// Draw client frame timing info
		NET_DrawTimes(vrect, cmdinfo, x, w);
	}

	NET_DrawTextFields(graphtype, x, y, graph, cmdinfo, ping_count, avg_ping, &framerate, packet_loss, packet_choke);
}
