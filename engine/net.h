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
#ifndef ENGINE_NET_H
#define ENGINE_NET_H

#include <cstdlib>

#include "tier0/platform.h"
#include "netadr.h"
#include "enums.h"

/**
*	Maximum size for a fragment rgba.
*/
//TODO: unrelated to actual net constants, remove - Solokiller
const size_t NET_MAX_FRAG_BUFFER = 1400;

//const int PROTOCOL_VERSION = 48;

const int MAX_CHALLENGES = 1024;

// Client connection is initiated by requesting a challenge value
//  the server sends this value back
const char S2C_CHALLENGE = 'A';	// + challenge value

// Send a userid, client remote address, is this server secure and engine build number
const char S2C_CONNECTION = 'B';

// HLMaster rejected a server's connection because the server needs to be updated
const char M2S_REQUESTRESTART = 'O';

// Response details about each player on the server
const char S2A_PLAYERS = 'D';

// Number of rules + string key and string value pairs
const char S2A_RULES = 'E';

// info request
const char S2A_INFO = 'C'; // deprecated goldsrc response

const char S2A_INFO_DETAILED = 'm'; // New Query protocol, returns dedicated or not, + other performance info.

// send a log event as key value
const char S2A_LOGSTRING = 'R';

// Send a log string
const char S2A_LOGKEY = 'S';

// Basic information about the server
const char A2S_INFO = 'T';

// Details about each player on the server
const char A2S_PLAYER = 'U';

// The rules the server is using
const char A2S_RULES = 'V';

// Another user is requesting a challenge value from this machine
const char A2A_GETCHALLENGE = 'W';	// Request challenge # from another machine

// Generic Ping Request
const char A2A_PING = 'i';	// respond with an A2A_ACK

// Generic Ack
const char A2A_ACK = 'j';	// general acknowledgement without info

// Print to client console
const char A2A_PRINT = 'l'; // print a message on client

// Challenge response from master
const char M2A_CHALLENGE = 's';	// + challenge value

// 0 == regular, 1 == file stream
enum
{
	FRAG_NORMAL_STREAM = 0,
	FRAG_FILE_STREAM,

	MAX_STREAMS
};

// Flow control bytes per second limits
const float MAX_RATE = 100000.0f;
const float MIN_RATE = 1000.0f;

// Default data rate
const float DEFAULT_RATE = 30000.0f;

// NETWORKING INFO

// Max size of udp packet payload
// +2k bytes since HL25
const int MAX_UDP_PACKET = 6010; //4010; // 9 bytes SPLITHEADER + 4000 payload?

// Max length of a reliable message
const int MAX_MSGLEN = 5990; // 10 reserved for fragheader?

// Max length of unreliable message
const int MAX_DATAGRAM = 6000;

// This is the packet payload without any header bytes (which are attached for actual sending)
const int NET_MAX_PAYLOAD = 65536;

// This is the payload plus any header info (excluding UDP header)

// Packet header is:
//  4 bytes of outgoing seq
//  4 bytes of incoming seq
//  and for each stream
// {
//  byte (on/off)
//  int (fragment id)
//  short (startpos)
//  short (length)
// }
#define HEADER_BYTES (8 + MAX_STREAMS * 9)

// Pad this to next higher 16 byte boundary
// This is the largest packet that can come in/out over the wire, before processing the header
//  bytes will be stripped by the networking channel layer
//#define NET_MAX_MESSAGE PAD_NUMBER( ( MAX_MSGLEN + HEADER_BYTES ), 16 )
// This is currently used value in the engine. TODO: define above gives 4016, check it why.
const int NET_MAX_MESSAGE = 6037;

typedef enum svc_commands_e
{
	svc_bad,
	svc_nop,
	svc_disconnect,
	svc_event,
	svc_version,
	svc_setview,
	svc_sound,
	svc_time,
	svc_print,
	svc_stufftext,
	svc_setangle,
	svc_serverinfo,
	svc_lightstyle,
	svc_updateuserinfo,
	svc_deltadescription,
	svc_clientdata,
	svc_stopsound,
	svc_pings,
	svc_particle,
	svc_damage,
	svc_spawnstatic,
	svc_event_reliable,
	svc_spawnbaseline,
	svc_temp_entity,
	svc_setpause,
	svc_signonnum,
	svc_centerprint,
	svc_killedmonster,
	svc_foundsecret,
	svc_spawnstaticsound,
	svc_intermission,
	svc_finale,
	svc_cdtrack,
	svc_restore,
	svc_cutscene,
	svc_weaponanim,
	svc_decalname,
	svc_roomtype,
	svc_addangle,
	svc_newusermsg,
	svc_packetentities,
	svc_deltapacketentities,
	svc_choke,
	svc_resourcelist,
	svc_newmovevars,
	svc_resourcerequest,
	svc_customization,
	svc_crosshairangle,
	svc_soundfade,
	svc_filetxferfailed,
	svc_hltv,
	svc_director,
	svc_voiceinit,
	svc_voicedata,
	svc_sendextrainfo,
	svc_timescale,
	svc_resourcelocation,
	svc_sendcvarvalue,
	svc_sendcvarvalue2,
	svc_exec,
	svc_reserve60,
	svc_reserve61,
	svc_reserve62,
	svc_reserve63,
	svc_startofusermessages = svc_exec,
	svc_endoflist = 255,
} svc_commands_t;

typedef enum clc_commands_e
{
	clc_bad,
	clc_nop,
	clc_move,
	clc_stringcmd,
	clc_delta,
	clc_resourcelist,
	clc_tmove,
	clc_fileconsistency,
	clc_voicedata,
	clc_hltv,
	clc_cvarvalue,
	clc_cvarvalue2,
	clc_endoflist = 255,
} clc_commands_t;

extern sizebuf_t net_message;

void NET_Config( qboolean multiplayer );

void NET_Shutdown();

void NET_Init();

#endif //ENGINE_NET_H
