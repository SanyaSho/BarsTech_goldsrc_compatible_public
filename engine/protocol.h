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
#ifndef ENGINE_PROTOCOL_H
#define ENGINE_PROTOCOL_H

typedef struct entity_state_s entity_state_t;

/**
*	@file
*
*	communications protocols
*/

#ifndef NET_H
#include "net.h"
#endif

#define	PROTOCOL_VERSION 48

#define DELTAFRAME_NUMBITS	8
#define DELTAFRAME_MASK		((1 << DELTAFRAME_NUMBITS) - 1)

// Max number of history commands to send ( 2 by default ) in case of dropped packets
#define NUM_BACKUP_COMMAND_BITS		3
#define MAX_BACKUP_COMMANDS			(1 << NUM_BACKUP_COMMAND_BITS)

// This is used, unless overridden in the registry
#define VALVE_MASTER_ADDRESS "half-life.east.won.net:27010"

#define PORT_RCON			27015	// defualt RCON port, TCP
#define	PORT_MASTER			27011	// Default master port, UDP
#define PORT_CLIENT			"27005"	// Default client port, UDP/TCP
#define PORT_SERVER			"27015"	// Default server port, UDP/TCP
#define PORT_HLTV			27020	// Default hltv port
#define PORT_MATCHMAKING	"27025"	// Default matchmaking port
#define PORT_SYSTEMLINK		27030	// Default system link port
#define PORT_RPT			27035	// default RPT (remote perf testing) port, TCP
#define PORT_RPT_LISTEN		27036	// RPT connection listener (remote perf testing) port, TCP

// out of band message id bytes

// M = master, S = server, C = client, A = any
// the second character will allways be \n if the message isn't a single
// byte long (?? not true anymore?)


// Requesting for full server list from Server Master
#define	A2M_GET_SERVERS			'c'	// no params

// Master response with full server list
#define	M2A_SERVERS				'd'	// + 6 byte IP/Port list.

// Request for full server list from Server Master done in batches
#define A2M_GET_SERVERS_BATCH	'e' // + in532 uniqueID ( -1 for first batch )

// Master response with server list for channel
#define M2A_SERVER_BATCH		'f' // + int32 next uniqueID( -1 for last batch ) + 6 byte IP/Port list.

// Request for MOTD from Server Master  (Message of the Day)
#define	A2M_GET_MOTD			'g'	// no params

// MOTD response Server Master
#define	M2A_MOTD				'h'	// + string 

// Generic Ping Request
#define	A2A_PING				'i'	// respond with an A2A_ACK

// Generic Ack
#define	A2A_ACK					'j'	// general acknowledgement without info

#define C2S_CONNECT				'k'	// client requests to connect

// Print to client console.
#define	A2A_PRINT				'l'	// print a message on client

// info request
#define S2A_INFO_DETAILED		'm'	// New Query protocol, returns dedicated or not, + other performance info.

// Another user is requesting a challenge value from this machine
// NOTE: this is currently duplicated in SteamClient.dll but for a different purpose,
// so these can safely diverge anytime. SteamClient will be using a different protocol
// to update the master servers anyway.
#define A2S_GETCHALLENGE		'q'	// Request challenge # from another machine

#define A2S_RCON				'r'	// client rcon command

#define A2A_CUSTOM				't'	// a custom command, follow by a string for 3rd party tools


// A user is requesting the list of master servers, auth servers, and titan dir servers from the Client Master server
#define A2M_GETMASTERSERVERS	'v' // + byte (type of request, TYPE_CLIENT_MASTER or TYPE_SERVER_MASTER)

// Master server list response
#define M2A_MASTERSERVERS		'w'	// + byte type + 6 byte IP/Port List

#define A2M_GETACTIVEMODS		'x' // + string Request to master to provide mod statistics ( current usage ).  "1" for first mod.

#define M2A_ACTIVEMODS			'y' // response:  modname\r\nusers\r\nservers

#define M2M_MSG					'z' // Master peering message

// SERVER TO CLIENT/ANY

// Client connection is initiated by requesting a challenge value
//  the server sends this value back
#define S2C_CHALLENGE			'A' // + challenge value

// Server notification to client to commence signon process using challenge value.
#define	S2C_CONNECTION			'B' // no params

// Response to server info requests

// Request for detailed server/rule information.
#define S2A_INFO_GOLDSRC		'm' // Reserved for use by goldsrc servers
// Proxy sends multicast IP or 0.0.0.0 if he's not broadcasting the game
#define S2C_LISTEN				'G'	// + IP x.x.x.x:port
#define P2P_STATUS				'I'	// Inter-Proxy status message
#define S2M_GETFILE				'J'	// request module from master
#define M2S_SENDFILE			'K'	// send module to server

#define S2C_REDIRECT			'L'	// + IP x.x.x.x:port, redirect client to other server/proxy 

#define	C2M_CHECKMD5			'M'	// player client asks secure master if Module MD5 is valid
#define M2C_ISVALIDMD5			'N'	// secure servers answer to C2M_CHECKMD5

// MASTER TO SERVER
#define M2A_ACTIVEMODS3			'P' // response:  keyvalues struct of mods
#define A2M_GETACTIVEMODS3		'Q' // get a list of mods and the stats about them

#define S2A_LOGSTRING			'R'	// send a log string
#define S2A_LOGKEY				'S'	// send a log event as key value

#define A2S_INFO				'T'

#define A2S_SERVERQUERY_GETCHALLENGE		'W'	// Request challenge # from another machine

#define A2S_KEY_STRING		"Source Engine Query" // required postfix to a A2S_INFO query

#define A2M_GET_SERVERS_BATCH2	'1' // New style server query

#define A2M_GETACTIVEMODS2		'2' // New style mod info query

#define C2S_AUTHREQUEST1        '3' // 
#define S2C_AUTHCHALLENGE1      '4' //
#define C2S_AUTHCHALLENGE2      '5' //
#define S2C_AUTHCOMPLETE        '6'
#define C2S_AUTHCONNECT         '7'  // Unused, signals that the client has
// authenticated the server
#define S2C_BADPASSWORD			'8' // Special protocol for bad passwords.
#define S2C_CONNREJECT			'9'  // Special protocol for rejected connections.

// The new S2A_INFO_SRC packet has a byte at the end that has these bits in it, telling
// which data follows.
#define S2A_EXTRA_DATA_HAS_GAME_PORT				0x80	// Next 2 bytes include the game port.
#define S2A_EXTRA_DATA_HAS_SPECTATOR_DATA			0x40	// Next 2 bytes include the spectator port, then the spectator server name.
#define S2A_EXTRA_DATA_HAS_GAMETAG_DATA				0x20	// Next bytes are the game tag string
#define S2A_EXTRA_DATA_HAS_STEAMID					0x10	// Next 8 bytes are the steamID
#define S2A_EXTRA_DATA_GAMEID						0x01	// Next 8 bytes are the gameID of the server

//
#define PORT_MODMASTER	27011		// Default mod-master port

#define PROTOCOL_AUTHCERTIFICATE 0x01	// Connection from client is using a WON authenticated certificate
#define PROTOCOL_HASHEDCDKEY	 0x02	// Connection from client is using hashed CD key because WON comm. channel was unreachable
#define PROTOCOL_STEAM			 0x03	// Steam certificates
#define PROTOCOL_POKEMON		 0x04
#define PROTOCOL_LASTVALID       0x04	// Last valid protocol

#define SIGNED_GUID_LEN 32 // Hashed CD Key (32 hex alphabetic chars + 0 terminator )

#define PROXYFLAG_SETPORT (1<<7)
#define PROXYFLAG_SETSPECTATOR (1<<6)
#define PROXYFLAG_SETNAME (1<<5)
#define PROXYFLAG_BIT4 (1<<4)
#define PROXYFLAG_BIT0 (1<<0)

//
// server to client
//
#define	svc_bad					0
#define	svc_nop					1
#define	svc_disconnect			2
#define	svc_event				3		// 
#define	svc_version				4		// [long] server version
#define	svc_setview				5		// [short] entity number
#define	svc_sound				6		// <see code>
#define	svc_time				7		// [float] server time
#define	svc_print				8		// [string] null terminated string
#define	svc_stufftext			9		// [string] stuffed into client's console rgba
											// the string should be \n terminated
#define	svc_setangle			10		// [angle3] set the view angle to this absolute value
#define	svc_serverinfo			11		// [long] version
											// [string] signon string
											// [string]..[0]model cache
											// [string]...[0]sounds cache
#define	svc_lightstyle			12		// [byte] [string]
#define	svc_updateuserinfo		13		// [byte] slot [long] uid [string] userinfo
#define svc_deltadescription	14
#define	svc_clientdata			15		// <shortbits + data>
#define	svc_stopsound			16		// <see code>
#define	svc_pings				17
#define	svc_particle			18		// [vec3] <variable>
#define	svc_damage				19
#define	svc_spawnstatic			20
#define	svc_event_reliable		21
#define svc_spawnbaseline		22
#define svc_tempentity			23
#define	svc_setpause			24		// [byte] on / off
#define	svc_signonnum			25		// [byte]  used for the signon sequence
#define	svc_centerprint			26		// [string] to put in center of the screen
#define	svc_killedmonster		27
#define	svc_foundsecret			28
#define	svc_spawnstaticsound	29		// [coord3] [byte] samp [byte] vol [byte] aten
#define	svc_intermission		30		// [string] music
#define	svc_finale				31		// [string] music [string] text
#define	svc_cdtrack				32		// [byte] track [byte] looptrack
#define svc_restore				33
#define svc_cutscene			34
#define svc_weaponanim			35
#define svc_decalname			36		// [byte] index [string] name
#define	svc_roomtype			37		// [byte] roomtype (dsp effect)
#define	svc_addangle			38		// [angle3] set the view angle to this absolute value
#define svc_newusermsg			39
#define	svc_packetentities		40		// [...]  Non-delta compressed entities
#define	svc_deltapacketentities	41		// [...]  Delta compressed entities
#define	svc_choke   			42		// # of packets held back on channel because too much data was flowing.
#define svc_resourcelist		43
#define svc_newmovevars			44
#define svc_resourcerequest		45
#define svc_customization		46
#define	svc_crosshairangle		47		// [char] pitch * 5 [char] yaw * 5
#define svc_soundfade			48      // char percent, char holdtime, char fadeouttime, char fadeintime
#define svc_filetxferfailed		49
#define svc_hltv				50
#define svc_director			51
#define svc_voiceinit			52		// Initialize voice stuff.
#define svc_voicedata			53		// Voicestream data from the server
#define	svc_sendextrainfo		54
#define svc_timescale			55
#define svc_resourcelocation	56
#define svc_sendcvarvalue		57
#define svc_sendcvarvalue2		58
#define svc_exec				59
#define SVC_LASTMSG				59

//
// client to server
//
#define	clc_bad			0
#define	clc_nop 		1
#define	clc_move		2		// [usercmd_t]
#define	clc_stringcmd	3		// [string] message
#define	clc_delta				4       // [byte] request for delta compressed entity packet, 
										//  delta is from last incoming sequence [byte].
#define	clc_resourcelist		5
#define	clc_tmove				6
#define	clc_fileconsistency		7
#define	clc_voicedata			8       // Voicestream data from a client
#define	clc_hltv				9
#define	clc_cvarvalue			10
#define	clc_cvarvalue2			11

#define clc_endoflist			255		// the last one

// елементи не комунікуються через інтернет

#define SND_SPAWNING		(1<<8)		// duplicated in protocol.h we're spawing, used in some cases for ambients 
#define SND_STOP			(1<<5)		// duplicated in protocol.h stop sound
#define SND_CHANGE_VOL		(1<<6)		// duplicated in protocol.h change sound vol
#define SND_CHANGE_PITCH	(1<<7)		// duplicated in protocol.h change sound pitch
#define SND_SENTENCE		(1<<4)		// set if sound num is actually a sentence num
#define SND_PITCH			(1<<3)
#define SND_NUMBER			(1<<2)
#define SND_ATTENUATION		(1<<1)
#define SND_VOLUME			(1<<0)


// How many data slots to use when in multiplayer (must be power of 2)
#define MULTIPLAYER_BACKUP		(1<<6)
// Same for single player
#define SINGLEPLAYER_BACKUP		(1<<3)

extern int SV_UPDATE_BACKUP;	// Copies of entity_state_t to keep buffered
extern int SV_UPDATE_MASK;		// must be power of two.  Increasing this number costs a lot of RAM.
extern int CL_UPDATE_BACKUP;
extern int CL_UPDATE_MASK;

/*
==========================================================

ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

/**
*	Default minimum number of clients for multiplayer servers
*/
#define MP_MIN_CLIENTS 6

//See com_model.h for MAX_CLIENTS

//#define	MAX_PACKET_ENTITIES	64	// doesn't count nails
struct packet_entities_t
{
	int num_entities;

#ifdef HL25
	byte flags[ 128 ];
#else
	byte flags[ 32 ];
#endif
	entity_state_t* entities;
};

#endif //ENGINE_PROTOCOL_H
