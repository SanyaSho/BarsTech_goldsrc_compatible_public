#ifndef ENGINE_NET_API_INT_H
#define ENGINE_NET_API_INT_H

#include "net_api.h"

typedef struct net_api_query_s
{
	struct net_api_query_s* next;

	// Context ID
	int			context;
	// Type
	int			type;

	int flags;

	double requesttime;
	double timeout;

	netadr_t request;
	net_api_response_func_t callback;
} net_api_query_t;

void Net_InitNetworking();

void Net_APIShutDown();

void Net_Status( net_status_t* status );

void Net_SendRequest( int context, int request, int flags, double timeout, netadr_t* remote_address, net_api_response_func_t response );

void Net_CancelRequest( int context );

void Net_CancelAllRequests();

char* Net_AdrToString( netadr_t* a );

int Net_CompareAdr( netadr_t* a, netadr_t* b );

int Net_StringToAdr( char* s, netadr_t* a );

void Net_APIInit();

void Net_APICheckTimeouts();

#endif //ENGINE_NET_API_INT_H
