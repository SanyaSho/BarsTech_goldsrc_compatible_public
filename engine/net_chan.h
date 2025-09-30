#ifndef ENGINE_NET_CHAN_H
#define ENGINE_NET_CHAN_H

#include "net.h"
#include "enums.h"

/*typedef enum netsrc_s
{
	NS_CLIENT = 0,
	NS_SERVER = 1,
	NS_MULTICAST = 2
} netsrc_t;*/

/*typedef struct netchan_s
{
	//TODO: implement - Solokiller
	
	netsrc_t sock;
	netadr_t remote_address;
	int player_slot;
	float last_received;
	float connect_time;
	double rate;
	double cleartime;
	int incoming_sequence;
	int incoming_acknowledged;
	int incoming_reliable_acknowledged;
	int incoming_reliable_sequence;
	int outgoing_sequence;
	int reliable_sequence;
	int last_reliable_sequence;
	void *connection_status;
	int( *pfnNetchan_Blocksize )( void * );
	sizebuf_t message;
	byte message_buf[ 3990 ];
	int reliable_length;
	byte reliable_buf[ 3990 ];
	fragbufwaiting_t *waitlist[ 2 ];
	int reliable_fragment[ 2 ];
	unsigned int reliable_fragid[ 2 ];
	fragbuf_t *fragbufs[ 2 ];
	int fragbufcount[ 2 ];
	__int16 frag_startpos[ 2 ];
	__int16 frag_length[ 2 ];
	fragbuf_t *incomingbufs[ 2 ];
	qboolean incomingready[ 2 ];
	char incomingfilename[ 260 ];
	void *tempbuffer;
	int tempbuffersize;
	flow_t flow[ 2 ];
	
} netchan_t;*/

enum
{
	FLOW_OUTGOING = 0,
	FLOW_INCOMING,

	MAX_FLOWS
};

// Message data
typedef struct flowstats_s
{
	// Size of message sent/received
	int size;
	// Time that message was sent/received
	double time;
} flowstats_t;

const int MAX_LATENT = 32;

typedef struct flow_s
{
	// Data for last MAX_LATENT messages
	flowstats_t stats[MAX_LATENT];
	// Current message position
	int current;
	// Time when we should recompute k/sec data
	double nextcompute;
	// Average data
	float kbytespersec;
	float avgkbytespersec;
} flow_t;

const int FRAGMENT_C2S_MIN_SIZE = 16;
const int FRAGMENT_S2C_MIN_SIZE = 256;
const int FRAGMENT_S2C_MAX_SIZE = 1024;

const int CLIENT_FRAGMENT_SIZE_ONCONNECT = 128;
const int CUSTOMIZATION_MAX_SIZE = 20480;

// Size of fragmentation rgba internal buffers
const int FRAGMENT_MAX_SIZE = 1400;

const int MAX_FRAGMENTS = 25000;

const int UDP_HEADER_SIZE = 28;
const int MAX_RELIABLE_PAYLOAD = 1200;

#define MAKE_FRAGID(id,count)	((( id & 0xffff) << 16) | (count & 0xffff))
#define FRAG_GETID(fragid)		((fragid >> 16) & 0xffff)
#define FRAG_GETCOUNT(fragid)	(fragid & 0xffff)

// Generic fragment structure
typedef struct fragbuf_s
{
	// Next rgba in chain
	fragbuf_s* next;
	// Id of this rgba
	int bufferid;
	// Message rgba where raw data is stored
	sizebuf_t frag_message;
	// The actual data sits here
	byte frag_message_buf[FRAGMENT_MAX_SIZE];
	// Is this a file rgba?
	qboolean isfile;
	// Is this file rgba from memory ( custom decal, etc. ).
	qboolean isbuffer;
	qboolean iscompressed;
	// Name of the file to save out on remote host
	char filename[MAX_PATH];
	// Offset in file from which to read data
	int foffset;
	// Size of data to read at that offset
	int size;
} fragbuf_t;

// Waiting list of fragbuf chains
typedef struct fragbufwaiting_s
{
	// Next chain in waiting list
	fragbufwaiting_s* next;
	// Number of buffers in this chain
	int fragbufcount;
	// The actual buffers
	fragbuf_t* fragbufs;
} fragbufwaiting_t;

// Network Connection Channel
typedef struct netchan_s
{
	// NS_SERVER or NS_CLIENT, depending on channel.
	netsrc_t sock;

	// Address this channel is talking to.
	netadr_t remote_address;

	int player_slot;
	// For timeouts.  Time last message was received.
	float last_received;
	// Time when channel was connected.
	float connect_time;

	// Bandwidth choke
	// Bytes per second
	double rate;
	// If realtime > cleartime, free to send next packet
	double cleartime;

	// Sequencing variables
	//
	// Increasing count of sequence numbers
	int incoming_sequence;
	// # of last outgoing message that has been ack'd.
	int incoming_acknowledged;
	// Toggles T/F as reliable messages are received.
	int incoming_reliable_acknowledged;
	// single bit, maintained local
	int incoming_reliable_sequence;
	// Message we are sending to remote
	int outgoing_sequence;
	// Whether the message contains reliable payload, single bit
	int reliable_sequence;
	// Outgoing sequence number of last send that had reliable data
	int last_reliable_sequence;

	void* connection_status;
	int (*pfnNetchan_Blocksize)(void*);

	// Staging and holding areas
	sizebuf_t message;
	byte message_buf[MAX_MSGLEN];

	// Reliable message rgba. We keep adding to it until reliable is acknowledged. Then we clear it.
	int reliable_length;
	byte reliable_buf[MAX_MSGLEN];

	// Waiting list of buffered fragments to go onto queue. Multiple outgoing buffers can be queued in succession.
	fragbufwaiting_t* waitlist[MAX_STREAMS];

	// Is reliable waiting buf a fragment?
	int reliable_fragment[MAX_STREAMS];
	// Buffer id for each waiting fragment
	unsigned int reliable_fragid[MAX_STREAMS];

	// The current fragment being set
	fragbuf_t* fragbufs[MAX_STREAMS];
	// The total number of fragments in this stream
	int fragbufcount[MAX_STREAMS];

	// Position in outgoing rgba where frag data starts
	short int frag_startpos[MAX_STREAMS];
	// Length of frag data in the rgba
	short int frag_length[MAX_STREAMS];

	// Incoming fragments are stored here
	fragbuf_t* incomingbufs[MAX_STREAMS];
	// Set to true when incoming data is ready
	qboolean incomingready[MAX_STREAMS];

	// Only referenced by the FRAG_FILE_STREAM component
	// Name of file being downloaded
	char incomingfilename[MAX_PATH];

	void* tempbuffer;
	int tempbuffersize;

	// Incoming and outgoing flow metrics
	flow_t flow[MAX_FLOWS];
} netchan_t;

extern char gDownloadFile[ 256 ];

void Netchan_Init();

void Netchan_Transmit( netchan_t* chan, int length, byte* data );

//void NET_SendPacket( netsrc_t sock, int length, void* data, netadr_t to );

extern char gDownloadFile[256];

extern int net_drop;
extern cvar_t net_log;
extern cvar_t net_showpackets;
extern cvar_t net_showdrop;
extern cvar_t net_drawslider;
extern cvar_t net_chokeloopback;
extern cvar_t sv_filetransfercompression;
extern cvar_t sv_filetransfermaxsize;

void Netchan_UnlinkFragment(fragbuf_t* buf, fragbuf_t** list);
void Netchan_OutOfBand(netsrc_t sock, netadr_t adr, int length, byte* data);
void Netchan_OutOfBandPrint(netsrc_t sock, netadr_t adr, char* format, ...);
void Netchan_ClearFragbufs(fragbuf_t** ppbuf);
void Netchan_ClearFragments(netchan_t* chan);
void Netchan_Clear(netchan_t* chan);
void Netchan_Setup(netsrc_t socketnumber, netchan_t* chan, netadr_t adr, int player_slot, void* connection_status, qboolean(*pfnNetchan_Blocksize)(void*));
qboolean Netchan_CanPacket(netchan_t* chan);
void Netchan_UpdateFlow(netchan_t* chan);
fragbuf_t* Netchan_FindBufferById(fragbuf_t** pplist, int id, qboolean allocate);
void Netchan_CheckForCompletion(netchan_t* chan, int stream, int intotalbuffers);
qboolean Netchan_Validate(netchan_t* chan, qboolean* frag_message, unsigned int* fragid, int* frag_offset, int* frag_length);
qboolean Netchan_Process(netchan_t* chan);
void Netchan_FragSend(netchan_t* chan);
void Netchan_AddBufferToList(fragbuf_t** pplist, fragbuf_t* pbuf);
fragbuf_t* Netchan_AllocFragbuf(void);
void Netchan_AddFragbufToTail(fragbufwaiting_t* wait, fragbuf_t* buf);
void Netchan_CreateFragments_(qboolean server, netchan_t* chan, sizebuf_t* msg);
void Netchan_CreateFragments(qboolean server, netchan_t* chan, sizebuf_t* msg);
void Netchan_CreateFileFragmentsFromBuffer(qboolean server, netchan_t* chan, const char* filename, unsigned char* uncompressed_pbuf, int uncompressed_size);
int Netchan_CreateFileFragments(qboolean server, netchan_t* chan, const char* filename);
void Netchan_FlushIncoming(netchan_t* chan, int stream);
qboolean Netchan_CopyNormalFragments(netchan_t* chan);
qboolean Netchan_CopyFileFragments(netchan_t* chan);
qboolean Netchan_IncomingReady(netchan_t* chan);
void Netchan_UpdateProgress(netchan_t* chan);

#endif //ENGINE_NET_CHAN_H
