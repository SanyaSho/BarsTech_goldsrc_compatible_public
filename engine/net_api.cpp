#include "quakedef.h"
#include "net_api_int.h"
#include "net_ws.h"
#include "client.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "protocol.h"

net_api_query_t *g_queries;
net_adrlist_t *g_addresses;

void Net_APIClearQueries()
{
	net_api_query_t* qr, * cur;

	for (qr = g_queries, cur = qr != NULL ? qr->next : NULL; qr != NULL; qr = cur)
		Mem_Free(qr);

	g_queries = NULL;
}

void Net_KillServerList()
{
	net_adrlist_t* adr, * curadr;

	for (adr = g_addresses, curadr = adr != NULL ? adr->next : NULL; adr != NULL; adr = curadr)
		Mem_Free(adr);

	g_addresses = NULL;
}

void Net_APIInit()
{
	g_queries = NULL;
	g_addresses = NULL;
}

void Net_APIShutDown()
{
	Net_APIClearQueries();
	Net_KillServerList();
}

net_api_query_t* Net_APIFind(int type, netadr_t* remote)
{
	net_api_query_t* q;

	for (q = g_queries; q != NULL; q = q->next)
	{
		if (q->type == type)
		{
			if (q->request.type == NA_BROADCAST && (remote->type == NA_BROADCAST || remote->type == NA_IP))
				break;
#if defined(_WIN32)
			if (q->request.type == NA_BROADCAST_IPX && (remote->type == NA_BROADCAST_IPX || remote->type == NA_IPX))
				break;
#endif
			if (NET_CompareAdr(*remote, q->request))
				break;

		}
	}

	return q;
}

net_api_query_t* Net_APIFindContext(int context)
{
	net_api_query_t* q;

	for (q = g_queries; q != NULL && q->context != context; q = q->next)
		;

	return q;
}

void Net_APIFailed(net_api_query_t* p)
{
	net_response_t r;

	if (p->callback == NULL)
		return;

	r.context = p->context;
	r.type = p->type;
	r.error = NET_ERROR_TIMEOUT;
	r.ping = realtime - p->requesttime;
	r.remote_address = p->request;
	r.response = 0;
	p->callback(&r);
}

void Net_APIKill(net_api_query_t* kill)
{
	net_api_query_t* qr, * save;

	if (kill == NULL)
		return;

	if (g_queries == NULL)
		return;

	for (qr = g_queries; qr != NULL; qr = qr->next)
	{
		if (qr == kill)
		{
			save = qr->next;
			Mem_Free(kill);
			qr = save;
			break;
		}
	}
}

void Net_APICheckTimeouts()
{
	net_api_query_t* qr, * save, * trash;
	net_response_t resp;

	for (qr = g_queries, save = NULL; qr != NULL; )
	{
		if (realtime <= qr->requesttime + qr->timeout)
		{
			qr->next = save;
			save = qr;
			qr = qr->next;
		}
		else
		{
			if (qr->callback)
			{
				resp.context = qr->context;
				resp.type = qr->type;
				resp.error = NET_ERROR_TIMEOUT;
				resp.remote_address = qr->request;
				resp.response = 0;
				resp.ping = realtime - qr->requesttime;

				qr->callback(&resp);
			}

			trash = qr;
			qr = qr->next;
			Mem_Free(trash);
		}
	}

	g_queries = save;
}

void Net_GetBatchServerList(sizebuf_t* msg, int batch)
{
	MSG_WriteByte(msg, A2M_GET_SERVERS_BATCH);
	MSG_WriteLong(msg, batch);
}

void Net_SendRequest(int context, int request, int flags, double timeout, netadr_t* remote_address, net_api_response_func_t response)
{
	byte data[1024];
	sizebuf_t msg{};
	net_api_query_t* qr;

	msg.data = data;
	msg.cursize = 0;
	msg.buffername = "Net_SendRequest";
	msg.maxsize = 1024;

	qr = g_queries;

	g_queries = (net_api_query_t*)Mem_ZeroMalloc(sizeof(net_api_query_t));
	g_queries->type = request;
	g_queries->context = context;
	g_queries->requesttime = realtime;
	g_queries->flags = flags;
	g_queries->timeout = timeout;
	g_queries->request = *remote_address;
	g_queries->callback = response;

	g_queries->next = qr;

	switch (request)
	{
	case NETAPI_REQUEST_SERVERLIST:
		Net_KillServerList();
		Net_GetBatchServerList(&msg, 0);
		break;
	case NETAPI_REQUEST_PING:
		MSG_WriteLong(&msg, -1);
		MSG_WriteString(&msg, "ping\n");
		break;
	case NETAPI_REQUEST_RULES:
		MSG_WriteLong(&msg, -1);
		MSG_WriteString(&msg, "rules\n");
		break;
	case NETAPI_REQUEST_PLAYERS:
		MSG_WriteLong(&msg, -1);
		MSG_WriteString(&msg, "players\n");
		break;
	case NETAPI_REQUEST_DETAILS:
		MSG_WriteLong(&msg, -1);
		MSG_WriteString(&msg, "details\n");
		break;
	default:
		Con_Printf(const_cast<char*>("Unknown request type:  %i\n"), request);
		break;
	}

	if (msg.cursize > 0)
		NET_SendPacket(NS_CLIENT, msg.cursize, msg.data, g_queries->request);
}

void NET_ParsePlayersResponse(char* returninfo)
{
	returninfo[0] = 0;

	MSG_BeginReading();
	
	MSG_ReadLong();
	MSG_ReadByte();
	
	int n = MSG_ReadByte();
	char sz[256];

	Info_SetValueForKey(returninfo, "players", va(const_cast<char*>("%i"), n), 2048);

	for (int i = 0; i < n; i++)
	{
		int index = MSG_ReadByte();
		char* s = MSG_ReadString();

		if (s == NULL)
			break;

		snprintf(sz, sizeof(sz), "%s", s);
		Info_SetValueForKey(returninfo, va(const_cast<char*>("p%iname"), index), sz, 2048);

		int frags = MSG_ReadLong();
		snprintf(sz, sizeof(sz), "%i", frags);
		Info_SetValueForKey(returninfo, va(const_cast<char*>("p%ifrags"), index), sz, 2048);

		float time = MSG_ReadFloat();
		snprintf(sz, sizeof(sz), "%.2f", time);
		Info_SetValueForKey(returninfo, va(const_cast<char*>("p%itime"), index), sz, 2048);
	}
}

void NET_ParseRulesResponse(char* returninfo)
{
	returninfo[0] = 0;

	MSG_BeginReading();

	MSG_ReadLong();
	MSG_ReadByte();

	int n = MSG_ReadShort();

	Info_SetValueForKey(returninfo, "rules", va(const_cast<char*>("%i"), n), 2048);

	for (int i = 0; i < n; i++)
	{
		char szkey[64];
		char szval[1024];
		char* key, * val;

		key = MSG_ReadString();
		if (key == NULL || key[0] == 0)
			break;

		Q_strncpy(szkey, key, sizeof(szkey) - 1);
		szkey[sizeof(szkey) - 1] = 0;
		
		val = MSG_ReadString();

		if (val == NULL)
			break;

		Q_strncpy(szval, val, sizeof(szval) - 1);
		szval[sizeof(szval) - 1] = 0;

		Info_SetValueForKey(returninfo, szkey, szval, 2048);
	}
}

void NET_ParseServerInfoResponse(char* returninfo)
{
	returninfo[0] = 0;

	MSG_BeginReading();

	MSG_ReadLong();
	MSG_ReadByte();

	MSG_ReadString();

	char* s = MSG_ReadString();
	Info_SetValueForKey(returninfo, "hostname", s, 2048);

	s = MSG_ReadString();
	Info_SetValueForKey(returninfo, "map", s, 2048);

	s = MSG_ReadString();
	Info_SetValueForKey(returninfo, "gamedir", s, 2048);

	s = MSG_ReadString();
	Info_SetValueForKey(returninfo, "gamedesc", s, 2048);

	int n = MSG_ReadLong();
	Info_SetValueForKey(returninfo, "current", va(const_cast<char*>("%i"), n), 2048);

	n = MSG_ReadLong();
	Info_SetValueForKey(returninfo, "max", va(const_cast<char*>("%i"), n), 2048);

	n = MSG_ReadByte();
	Info_SetValueForKey(returninfo, "protocol", va(const_cast<char*>("%i"), n), 2048);

	char c = MSG_ReadByte();
	Info_SetValueForKey(returninfo, "type", va(const_cast<char*>("%c"), c), 2048);

	c = MSG_ReadByte();
	Info_SetValueForKey(returninfo, "os", va(const_cast<char*>("%c"), c), 2048);

	Info_SetValueForKey(returninfo, "pw", const_cast<char*>(MSG_ReadByte() == 0 ? "N" : "Y"), 2048);

	Info_SetValueForKey(returninfo, "secure", const_cast<char*>(MSG_ReadByte() == 0 ? "0" : "1"), 2048);

	if (!MSG_ReadByte())
	{
		Info_SetValueForKey(returninfo, "mod", "N", 2048);
		return;
	}

	Info_SetValueForKey(returninfo, "mod", "Y", 2048);

	s = MSG_ReadString();
	Info_SetValueForKey(returninfo, "mod_url", s, 2048);

	s = MSG_ReadString();
	Info_SetValueForKey(returninfo, "mod_dl", s, 2048);

	MSG_ReadString();

	n = MSG_ReadLong();
	Info_SetValueForKey(returninfo, "mod_ver", va(const_cast<char*>("%i"), n), 2048);

	n = MSG_ReadLong();
	Info_SetValueForKey(returninfo, "mod_size", va(const_cast<char*>("%i"), n), 2048);

	Info_SetValueForKey(returninfo, "svonly", const_cast<char*>(MSG_ReadByte() == 0 ? "N" : "Y"), 2048);
	Info_SetValueForKey(returninfo, "cldll", const_cast<char*>(MSG_ReadByte() == 0 ? "N" : "Y"), 2048);
}

int NET_ParseServerList()
{
	char szadr[128]{};
	byte data[1024];

	MSG_BeginReading();
	MSG_ReadLong();
	MSG_ReadByte();
	MSG_ReadByte();

	int unique = MSG_ReadLong();

	int nNumAddresses = (net_message.cursize - 6) / 6;

	for (int i = 0; i < nNumAddresses; i++)
	{
		byte cIP[4];
		netadr_t adr;
		Q_memset(szadr, 0, sizeof(szadr));

		for (int j = 0; j < 4; j++)
			cIP[i] = MSG_ReadByte();

		int port = MSG_ReadShort();
		port = BigShort(port);

		_snprintf(szadr, sizeof(szadr), "%i.%i.%i.%i:%i", cIP[0], cIP[1], cIP[2], cIP[3], port);

		if (Net_StringToAdr(szadr, &adr))
		{
			net_adrlist_t* p = (net_adrlist_t*)Mem_ZeroMalloc(0x18u);
			p->remote_address = adr;
			p->next = g_addresses;
			g_addresses = p;
		}
	}

	if (unique)
	{
		sizebuf_t msg;
		Q_memset(&msg, 0, sizeof(msg));

		msg.data = data;
		msg.buffername = "NET_ParseServerList";
		msg.cursize = 0;
		msg.maxsize = 1024;

		Net_GetBatchServerList(&msg, unique);

		if (msg.cursize)
		{
			NET_SendPacket(NS_CLIENT, msg.cursize, msg.data, net_from);
		}
		return 0;
	}

	return 1;
}

int Net_APIProcess()
{
	MSG_BeginReading();
	MSG_ReadLong();

	char c = MSG_ReadByte();

	net_response_t r;
	net_api_query_t* p;
	char returninfo[2048];

	returninfo[0] = 0;

	switch (c)
	{
	case S2A_PLAYERS:
		p = Net_APIFind(NETAPI_REQUEST_PLAYERS, &net_from);
		
		if (p == NULL)
			return 0;
		
		if (p->callback == NULL)
			break;
		
		NET_ParsePlayersResponse(returninfo);

		r.context = p->context;
		r.type = p->type;
		r.error = NET_SUCCESS;
		r.remote_address = net_from;
		r.response = returninfo;
		break;
	case S2A_RULES:
		p = Net_APIFind(NETAPI_REQUEST_RULES, &net_from);
		
		if (p == NULL)
			return 0;

		if (p->callback == NULL)
			break;

		NET_ParseRulesResponse(returninfo);

		r.context = p->context;
		r.error = NET_SUCCESS;
		r.type = p->type;
		r.remote_address = net_from;
		r.response = returninfo;
		r.ping = realtime - p->requesttime;
		p->callback(&r);

		break;
	case S2A_SERVERLIST:
		if (!NET_ParseServerList())
			return 0;

		p = Net_APIFind(NETAPI_REQUEST_SERVERLIST, &net_from);

		if (p == NULL)
		{
			Net_KillServerList();
			return 0;
		}
		
		if (p->callback != NULL)
		{
			Q_memset(&r, 0, sizeof(r));
			r.context = p->context;
			r.type = p->type;
			r.error = NET_SUCCESS;
			r.response = g_addresses;
			r.ping = realtime - p->requesttime;
			p->callback(&r);
		}

		if ((p->flags & NETAPI_REQUEST_PING) == 0)
			Net_APIKill(p);

		Net_KillServerList();

		return 1;
	case S2A_PING:
		p = Net_APIFind(NETAPI_REQUEST_PING, &net_from);

		if (p == NULL)
			return 0;

		if (p->callback == NULL)
			break;
		
		r.context = p->context;
		r.error = NET_SUCCESS;
		r.type = p->type;
		r.remote_address = net_from;
		r.response = returninfo;
		r.ping = realtime - p->requesttime;
		p->callback(&r);
		
		break;
	case S2A_INFO_DETAILED:
		p = Net_APIFind(NETAPI_REQUEST_DETAILS, &net_from);
		
		if (p == NULL)
			return 0;

		if (p->callback == NULL)
			break;

		NET_ParseServerInfoResponse(returninfo);
		r.context = p->context;
		r.type = p->type;
		r.error = NET_SUCCESS;
		r.remote_address = net_from;
		r.response = returninfo;
		r.ping = realtime - p->requesttime;
		p->callback(&r);

		break;
	default:
		Con_Printf(const_cast<char*>("Net_APIProcess: Unknown message type %i\n"), c);
		return 0;
	}

	if (!(p->flags & NETAPI_REQUEST_PING))
		Net_APIKill(p);

	return 1;
}

char* Net_AdrToString(netadr_t* a)
{
	return NET_AdrToString(*a);
}

int Net_StringToAdr(char* s, netadr_t* a)
{
	return NET_StringToAdr(s, a);
}

void Net_InitNetworking()
{
	NET_Config(true);
}

int Net_CompareAdr(netadr_t* a, netadr_t* b)
{
	return NET_CompareAdr(*a, *b);
}

int Net_GetPacketLoss(void)
{
	if (cls.state == ca_active)
	{
		if (cls.packet_loss < 0)
			return 0;

		if (cls.packet_loss <= 100)
			return cls.packet_loss;
	}
	return 0;
}

void Net_Status( net_status_t* status )
{
	if (status == NULL)
		return;

	status->connected = cls.state != ca_disconnected;
	status->local_address = net_local_adr;
	status->remote_address = cls.netchan.remote_address;

	if (status->connected)
	{
		status->packet_loss = Net_GetPacketLoss();
		status->latency = g_flLatency;
		status->connection_time = realtime - cls.netchan.connect_time;
	}
	else
	{
		status->packet_loss = 0;
		status->connection_time = 0.0;
		status->latency = 0.0;
	}

	status->rate = rate.value;
}

void Net_CancelRequest( int context )
{
	net_api_query_t* q = Net_APIFindContext(context);
	if (q != NULL)
		Net_APIKill(q);
}

void Net_CancelAllRequests()
{
	Net_APIClearQueries();
}

net_api_t netapi =
{
	&Net_InitNetworking,
	&Net_Status,
	&Net_SendRequest,
	&Net_CancelRequest,
	&Net_CancelAllRequests,
	&Net_AdrToString,
	&Net_CompareAdr,
	&Net_StringToAdr,
	&Info_ValueForKey,
	&Info_RemoveKey,
	&Info_SetValueForStarKey
};
