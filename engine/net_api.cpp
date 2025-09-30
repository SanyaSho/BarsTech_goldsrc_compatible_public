#include "quakedef.h"
#include "net_api_int.h"
#include "net_ws.h"
#include "client.h"
#include "cl_main.h"
#include "cl_parse.h"
#include "protocol.h"

net_api_query_t *g_queries;
net_adrlist_t *g_addresses;

void Net_InitNetworking()
{
	NET_Config(true);
}

void Net_CancelAllRequests()
{
	net_api_query_t* qr, *cur;

	for (qr = g_queries, cur = qr != NULL ? qr->next : NULL; qr != NULL; qr = cur)
		Mem_Free(qr);

	g_queries = NULL;
}

void Net_KillServerList()
{
	net_adrlist_t* adr, *curadr;

	for (adr = g_addresses, curadr = adr != NULL ? adr->next : NULL; adr != NULL; adr = curadr)
		Mem_Free(adr);

	g_addresses = NULL;
}

void Net_APIShutDown()
{
	Net_CancelAllRequests();
	Net_KillServerList();
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

void Net_GetBatchServerList(sizebuf_t *msg, int batch)
{
	MSG_WriteByte(msg, A2M_GET_SERVERS_BATCH);
	MSG_WriteLong(msg, batch);
}

void Net_SendRequest( int context, int request, int flags, double timeout, netadr_t* remote_address, net_api_response_func_t response )
{
	byte data[1024];
	sizebuf_t msg{};
	net_api_query_t *qr;

	msg.data = data;
	msg.cursize = 0;
	msg.buffername = "Net_SendRequest";
	msg.maxsize = 1024;

	qr = g_queries;

	g_queries = (net_api_query_t *)Mem_ZeroMalloc(sizeof(net_api_query_t));
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

net_api_query_t* Net_APIFindContext(int context)
{
	net_api_query_t *q;

	for (q = g_queries; q != NULL && q->context != context; q = q->next)
		;

	return q;
}

void Net_APIKill(net_api_query_t *kill)
{
	net_api_query_t* qr, *save;

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

void Net_CancelRequest( int context )
{
	net_api_query_t* q = Net_APIFindContext(context);
	if (q != NULL)
		Net_APIKill(q);
}

void Net_APICancelAllRequests()
{
	Net_CancelAllRequests();
}

char* Net_AdrToString( netadr_t* a )
{
	return NET_AdrToString(*a);
}

int Net_CompareAdr( netadr_t* a, netadr_t* b )
{
	return NET_CompareAdr(*a, *b);
}

int Net_StringToAdr( char* s, netadr_t* a )
{
	return NET_StringToAdr(s, a);
}

void Net_APIInit()
{
	g_queries = NULL;
	g_addresses = NULL;
}

void Net_APICheckTimeouts()
{
	net_api_query_t* qr, *save, *trash;
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
				resp.error = 1;
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

net_api_t netapi =
{
	&Net_InitNetworking,
	&Net_Status,
	&Net_SendRequest,
	&Net_CancelRequest,
	&Net_APICancelAllRequests,
	&Net_AdrToString,
	&Net_CompareAdr,
	&Net_StringToAdr,
	&Info_ValueForKey,
	&Info_RemoveKey,
	&Info_SetValueForStarKey
};
