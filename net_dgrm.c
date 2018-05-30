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
// net_dgrm.c


#include "quakedef.h"
#include "net_dgrm.h"


#include <stdio.h>
#include <Windows.h>
#include <Iphlpapi.h>
#include <Assert.h>
#pragma comment(lib, "iphlpapi.lib")


// these two macros are to make the code more readable
#define sfunc	net_landrivers[sock->landriver]
#define dfunc	net_landrivers[net_landriverlevel]

int net_landriverlevel;

/* statistic counters */

int	packetsSent = 0, packetsReSent = 0, packetsReceived = 0;
int	receivedDuplicateCount = 0, shortPacketCount = 0;
int	droppedDatagrams;

int	myDriverLevel;

struct
{
	unsigned int	length;
	unsigned int	sequence;
	byte			data[MAX_DATAGRAM];
} packetBuffer;

extern int	m_return_state;
extern int	m_state;
extern qboolean	m_return_onerror;
extern char	m_return_reason[32];
extern cvar_t pq_password;			// JPG 3.00 - password protection

char *StrAddr (struct qsockaddr *addr)
{
	static	char	buf[34];
	byte	*p = (byte *)addr;
	int	n;

	for (n=0 ; n<16 ; n++)
		sprintf (buf + n * 2, "%02x", *p++);

	return buf;
}


int Datagram_SendMessage (qsocket_t *sock, sizebuf_t *data)
{
	unsigned int	packetLen, dataLen, eom;

#if 0	// joe: removed DEBUG
	if (data->cursize == 0)
		Host_Error ("Datagram_SendMessage: zero length message");

	if (data->cursize > NET_MAXMESSAGE)
		Host_Error ("Datagram_SendMessage: message too big %u", data->cursize);

	if (sock->canSend == false)
		Host_Error ("SendMessage: called with canSend == false");
#endif

	memcpy (sock->sendMessage, data->data, data->cursize);
	sock->sendMessageLength = data->cursize;

	if (data->cursize <= MAX_DATAGRAM)
	{
		dataLen = data->cursize;
		eom = NETFLAG_EOM;
	}
	else
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	packetBuffer.length = BigLong (packetLen | (NETFLAG_DATA | eom));
	packetBuffer.sequence = BigLong (sock->sendSequence++);
	memcpy (packetBuffer.data, sock->sendMessage, dataLen);

	sock->canSend = false;

	if (sfunc.Write(sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsSent++;

	return 1;
}

int SendMessageNext (qsocket_t *sock)
{
	unsigned int	packetLen, dataLen, eom;

	if (sock->sendMessageLength <= MAX_DATAGRAM)
	{
		dataLen = sock->sendMessageLength;
		eom = NETFLAG_EOM;
	}
	else
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	packetBuffer.length = BigLong (packetLen | (NETFLAG_DATA | eom));
	packetBuffer.sequence = BigLong (sock->sendSequence++);
	memcpy (packetBuffer.data, sock->sendMessage, dataLen);

	sock->sendNext = false;

	if (sfunc.Write(sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsSent++;

	return 1;
}

int ReSendMessage (qsocket_t *sock)
{
	unsigned int	packetLen, dataLen, eom;

	if (sock->sendMessageLength <= MAX_DATAGRAM)
	{
		dataLen = sock->sendMessageLength;
		eom = NETFLAG_EOM;
	}
	else
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	packetBuffer.length = BigLong (packetLen | (NETFLAG_DATA | eom));
	packetBuffer.sequence = BigLong (sock->sendSequence - 1);
	memcpy (packetBuffer.data, sock->sendMessage, dataLen);

	sock->sendNext = false;

	if (sfunc.Write(sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsReSent++;

	return 1;
}

qboolean Datagram_CanSendMessage (qsocket_t *sock)
{
	if (sock->sendNext)
		SendMessageNext (sock);

	return sock->canSend;
}

qboolean Datagram_CanSendUnreliableMessage (qsocket_t *sock)
{
	return true;
}

int Datagram_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data)
{
	int 	packetLen;

#if 0	// joe: removed DEBUG
	if (data->cursize == 0)
		Host_Error ("Datagram_SendUnreliableMessage: zero length message");

	if (data->cursize > MAX_DATAGRAM)
		Host_Error ("Datagram_SendUnreliableMessage: message too big %u", data->cursize);
#endif

	packetLen = NET_HEADERSIZE + data->cursize;

	packetBuffer.length = BigLong (packetLen | NETFLAG_UNRELIABLE);
	packetBuffer.sequence = BigLong (sock->unreliableSendSequence++);
	memcpy (packetBuffer.data, data->data, data->cursize);

	if (sfunc.Write(sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	packetsSent++;
	return 1;
}

int Datagram_GetMessage (qsocket_t *sock)
{
	unsigned int	length, flags;
	int	ret = 0;
	struct qsockaddr readaddr;
	unsigned int	sequence, count;

	if (!sock->canSend)
		if ((net_time - sock->lastSendTime) > 1.0)
			ReSendMessage (sock);

	while(1)
	{	
		length = sfunc.Read(sock->socket, (byte *)&packetBuffer, NET_DATAGRAMSIZE, &readaddr);

		if (length == 0)
			break;

		if (length == -1)
		{
			Con_DPrintf (1,"Read error\n");//R00k: made to dprintf
			return -1;
		}

		// JPG 3.40 - added !sock->net_wait (NAT fix)
		if (!sock->net_wait && sfunc.AddrCompare(&readaddr, &sock->addr) != 0)
		{
			continue;
		}

		if (length < NET_HEADERSIZE)
		{
			shortPacketCount++;
			continue;
		}

		length = BigLong (packetBuffer.length);
		flags = length & (~NETFLAG_LENGTH_MASK);
		length &= NETFLAG_LENGTH_MASK;

		// joe, from ProQuake: fix for attack that crashes server
		if (length > NET_DATAGRAMSIZE)
		{
			Con_DPrintf (1,"Invalid length\n");
			return -1;
		}

		if (flags & NETFLAG_CTL)
			continue;

		sequence = BigLong (packetBuffer.sequence);
		packetsReceived++;

		// JPG 3.40 - NAT fix
		if (sock->net_wait)
		{
			sock->addr = readaddr;
			strcpy(sock->address, sfunc.AddrToString(&readaddr));
			sock->net_wait = false;
		}

		if (flags & NETFLAG_UNRELIABLE)
		{
			if (sequence < sock->unreliableReceiveSequence)
			{
				Con_DPrintf (1,"Got a stale datagram\n");
				ret = 0;
				break;
			}
			if (sequence != sock->unreliableReceiveSequence)
			{
				count = sequence - sock->unreliableReceiveSequence;
				droppedDatagrams += count;
				Con_DPrintf (1,"Dropped %u datagram(s)\n", count);
			}
			sock->unreliableReceiveSequence = sequence + 1;

			length -= NET_HEADERSIZE;

			SZ_Clear (&net_message);
			SZ_Write (&net_message, packetBuffer.data, length);

			ret = 2;
			break;
		}

		if (flags & NETFLAG_ACK)
		{
			if (sequence != (sock->sendSequence - 1))
			{
				Con_DPrintf (1,"Stale ACK received\n");
				continue;
			}
			if (sequence == sock->ackSequence)
			{
				sock->ackSequence++;
				if (sock->ackSequence != sock->sendSequence)
					Con_DPrintf (1,"ack sequencing error\n");
			}
			else
			{
				Con_DPrintf (1,"Duplicate ACK received\n");
				continue;
			}
			sock->sendMessageLength -= MAX_DATAGRAM;
			if (sock->sendMessageLength > 0)
			{
				memcpy (sock->sendMessage, sock->sendMessage + MAX_DATAGRAM, sock->sendMessageLength);
				sock->sendNext = true;
			}
			else
			{
				sock->sendMessageLength = 0;
				sock->canSend = true;
			}
			continue;
		}

		if (flags & NETFLAG_DATA)
		{
			packetBuffer.length = BigLong (NET_HEADERSIZE | NETFLAG_ACK);
			packetBuffer.sequence = BigLong (sequence);
			sfunc.Write (sock->socket, (byte *)&packetBuffer, NET_HEADERSIZE, &readaddr);

			if (sequence != sock->receiveSequence)
			{
				receivedDuplicateCount++;
				continue;
			}
			sock->receiveSequence++;

			length -= NET_HEADERSIZE;

			if (flags & NETFLAG_EOM)
			{
				SZ_Clear (&net_message);
				SZ_Write (&net_message, sock->receiveMessage, sock->receiveMessageLength);
				SZ_Write (&net_message, packetBuffer.data, length);
				sock->receiveMessageLength = 0;

				ret = 1;
				break;
			}

			memcpy (sock->receiveMessage + sock->receiveMessageLength, packetBuffer.data, length);
			sock->receiveMessageLength += length;
			continue;
		}
	}

	if (sock->sendNext)
		SendMessageNext (sock);

	return ret;
}

void PrintStats (qsocket_t *s)
{
	Con_Printf ("canSend = %4u   \n", s->canSend);
	Con_Printf ("sendSeq = %4u   ", s->sendSequence);
	Con_Printf ("recvSeq = %4u   \n", s->receiveSequence);
	Con_Printf ("\n");
}

void getMAC(void)
{
	PIP_ADAPTER_INFO AdapterInfo;
	DWORD dwBufLen = sizeof(AdapterInfo);
	char *mac_addr = (char*)malloc(17);

	AdapterInfo = (IP_ADAPTER_INFO *) malloc(sizeof(IP_ADAPTER_INFO));

	if (AdapterInfo == NULL) 
	{
	// printf("Error allocating memory needed to call GetAdaptersinfo\n");
		return;
	}

	// Make an initial call to GetAdaptersInfo to get the necessary size into the dwBufLen variable
	if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == ERROR_BUFFER_OVERFLOW) 
	{
		free(AdapterInfo);
		AdapterInfo = (IP_ADAPTER_INFO *) malloc(dwBufLen);
		if (AdapterInfo == NULL) 
		{
			//printf("Error allocating memory needed to call GetAdaptersinfo\n");
			return;
		}
	}

	if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == NO_ERROR) 
	{
		PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;// Contains pointer to current adapter info
		do 
		{
			sprintf(mac_addr, "%02X:%02X:%02X:%02X:%02X:%02X",
			pAdapterInfo->Address[0], pAdapterInfo->Address[1],
			pAdapterInfo->Address[2], pAdapterInfo->Address[3],
			pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
			Con_Printf("Address: %s, mac: %s", pAdapterInfo->IpAddressList.IpAddress.String, mac_addr);
			Con_Printf("\n");
			pAdapterInfo = pAdapterInfo->Next;        
		}
		while(pAdapterInfo);                        
	 }
	 free(AdapterInfo);
	 return;
}				

void NET_Stats_f (void)
{
	qsocket_t	*s;

	getMAC();

	if (Cmd_Argc () == 1)
	{
		Con_Printf ("unreliable messages sent   = %i\n", unreliableMessagesSent);
		Con_Printf ("unreliable messages recv   = %i\n", unreliableMessagesReceived);
		Con_Printf ("reliable messages sent     = %i\n", messagesSent);
		Con_Printf ("reliable messages received = %i\n", messagesReceived);
		Con_Printf ("packetsSent                = %i\n", packetsSent);
		Con_Printf ("packetsReSent              = %i\n", packetsReSent);
		Con_Printf ("packetsReceived            = %i\n", packetsReceived);
		Con_Printf ("receivedDuplicateCount     = %i\n", receivedDuplicateCount);
		Con_Printf ("shortPacketCount           = %i\n", shortPacketCount);
		Con_Printf ("droppedDatagrams           = %i\n", droppedDatagrams);
	}
	else if (!strcmp(Cmd_Argv(1), "*"))
	{
		for (s = net_activeSockets ; s ; s = s->next)
			PrintStats (s);

		for (s = net_freeSockets ; s ; s = s->next)
			PrintStats (s);
	}
	else
	{
		for (s = net_activeSockets; s; s = s->next)
			if (Q_strcasecmp(Cmd_Argv(1), s->address) == 0)
				break;
		if (s == NULL)
			for (s = net_freeSockets; s; s = s->next)
				if (Q_strcasecmp(Cmd_Argv(1), s->address) == 0)
					break;
		if (s == NULL)
			return;
		PrintStats(s);
	}
}

//ProQuake: recognize ip:port
void Strip_Port (char *ch)
{
	if ((ch = strchr(ch, ':')))
	{
		int	old_port = net_hostport;
		sscanf (ch+1, "%d", &net_hostport);
		for ( ; ch[-1] == ' ' ; ch--);
		*ch = 0;
		if (net_hostport != old_port)
			Con_Printf ("Setting port to %d\n", net_hostport);
	}
	else //R00k if not specifying port then use default port
	{
		net_hostport = DEFAULTnet_hostport;
		
		if (key_dest == key_console)
			Con_Printf ("Using port %d\n", net_hostport);
	}
}

qboolean testInProgress = false;
int		testPollCount;
int		testDriver;
int		testSocket;

static void Test_Poll (void);
PollProcedure	testPollProcedure = {NULL, 0.0, Test_Poll};

static void Test_Poll (void)
{
	struct qsockaddr clientaddr;
	int		control, len;
	char	name[32], address[64];
	int		colors, frags, connectTime;
	byte	playerNumber;

	net_landriverlevel = testDriver;

	while (1)
	{
		len = dfunc.Read(testSocket, net_message.data, net_message.maxsize, &clientaddr);
		if (len < sizeof(int))
			break;

		net_message.cursize = len;

		MSG_BeginReading ();
		control = BigLong (*((int *)net_message.data));
		MSG_ReadLong ();
		if (control == -1)
			break;
		if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
			break;
		if ((control & NETFLAG_LENGTH_MASK) != len)
			break;

		if (MSG_ReadByte() != CCREP_PLAYER_INFO)
		{	// joe: changed from Sys_Error to Con_Printf from ProQuake
			Con_Printf ("Unexpected response to Player Info request\n");
			break;
		}

		playerNumber = MSG_ReadByte ();
		strcpy (name, MSG_ReadString());
		colors = MSG_ReadLong ();
		frags = MSG_ReadLong ();
		connectTime = MSG_ReadLong ();
		strcpy (address, MSG_ReadString());

		Con_Printf ("%s\n  frags:%3i  colors:%u %u  time:%u\n  %s\n", name, frags, colors >> 4, colors & 0x0f, connectTime / 60, address);
	}

	testPollCount--;
	if (testPollCount)
	{
		SchedulePollProcedure (&testPollProcedure, 0.1);
	}
	else
	{
		dfunc.CloseSocket (testSocket);
		testInProgress = false;
	}
}

static void Test_f (void)
{
	char	*host;
	int		n, max = MAX_SCOREBOARD;
	struct qsockaddr sendaddr;

	if (testInProgress)
	{	// joe: added error message from ProQuake
		Con_Printf ("There is already a test/rcon in progress\n");
		return;
	}

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("Usage: test <host>\n");
		return;
	}

	host = Cmd_Argv (1);
	Strip_Port (host);

	if (host && hostCacheCount)
	{
		for (n=0 ; n<hostCacheCount ; n++)
			if (!Q_strcasecmp(host, hostcache[n].name))
			{
				if (hostcache[n].driver != myDriverLevel)
					continue;
				net_landriverlevel = hostcache[n].ldriver;
				max = hostcache[n].maxusers;
				memcpy (&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				break;
			}
		if (n < hostCacheCount)
			goto JustDoIt;
	}

	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (dfunc.GetAddrFromName(host, &sendaddr) != -1)
			break;
	}
	if (net_landriverlevel == net_numlandrivers)
	{	// joe: added error message from ProQuake
		Con_Printf ("Could not resolve %s\n", host);
		return;
	}

JustDoIt:
	testSocket = dfunc.OpenSocket(0);
	if (testSocket == -1)
	{	// joe: added error message from ProQuake
		Con_Printf ("Could not open socket\n");
		return;
	}

	testInProgress = true;
	testPollCount = 20;
	testDriver = net_landriverlevel;

	for (n=0 ; n<max ; n++)
	{
		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREQ_PLAYER_INFO);
		MSG_WriteByte (&net_message, n);
		*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (testSocket, net_message.data, net_message.cursize, &sendaddr);
	}
	SZ_Clear (&net_message);
	SchedulePollProcedure (&testPollProcedure, 0.1);
}

static void Test2_Poll (void);
PollProcedure	test2PollProcedure = {NULL, 0.0, Test2_Poll};

static void Test2_Poll (void)
{
	struct qsockaddr clientaddr;
	int		control, len;
	char	name[256], value[256];

	net_landriverlevel = testDriver;
	name[0] = 0;

	len = dfunc.Read(testSocket, net_message.data, net_message.maxsize, &clientaddr);
	if (len < sizeof(int))
		goto Reschedule;

	net_message.cursize = len;

	MSG_BeginReading ();
	control = BigLong (*((int *)net_message.data));
	MSG_ReadLong();
	if (control == -1)
		goto Error;
	if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
		goto Error;
	if ((control & NETFLAG_LENGTH_MASK) != len)
		goto Error;

	if (MSG_ReadByte() != CCREP_RULE_INFO)
		goto Error;

	strcpy (name, MSG_ReadString());
	if (name[0] == 0)
		goto Done;
	strcpy (value, MSG_ReadString());

	Con_Printf ("%-16.16s  %-16.16s\n", name, value);

	SZ_Clear (&net_message);
	// save space for the header, filled in later
	MSG_WriteLong (&net_message, 0);
	MSG_WriteByte (&net_message, CCREQ_RULE_INFO);
	MSG_WriteString (&net_message, name);
	*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write(testSocket, net_message.data, net_message.cursize, &clientaddr);
	SZ_Clear (&net_message);

Reschedule:
	// joe: added poll counter from ProQuake
	testPollCount--;
	if (testPollCount)
	{
		SchedulePollProcedure (&test2PollProcedure, 0.05);
		return;
	}
	goto Done;

Error:
	Con_Printf ("Unexpected response to Rule Info request\n");

Done:
	dfunc.CloseSocket (testSocket);
	testInProgress = false;

	return;
}

static void Test2_f (void)
{
	char	*host;
	int		n;
	struct qsockaddr sendaddr;

	if (testInProgress)
	{	// joe: added error message from ProQuake
		Con_Printf ("There is already a test/rcon in progress\n");
		return;
	}

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("Usage: test2 <host>\n");
		return;
	}

	host = Cmd_Argv (1);
	Strip_Port (host);

	if (host && hostCacheCount)
	{
		for (n=0 ; n<hostCacheCount ; n++)
			if (!Q_strcasecmp(host, hostcache[n].name))
			{
				if (hostcache[n].driver != myDriverLevel)
					continue;
				net_landriverlevel = hostcache[n].ldriver;
				memcpy (&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				break;
			}
		if (n < hostCacheCount)
			goto JustDoIt;
	}

	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (dfunc.GetAddrFromName(host, &sendaddr) != -1)
			break;
	}
	if (net_landriverlevel == net_numlandrivers)
	{	// joe: added error message from ProQuake
		Con_Printf ("Could not resolve %s\n", host);
		return;
	}

JustDoIt:
	testSocket = dfunc.OpenSocket(0);
	if (testSocket == -1)
	{	// joe: added error message from ProQuake
		Con_Printf ("Could not open socket\n");
		return;
	}

	testInProgress = true;
	testPollCount = 20;			// joe: added this from ProQuake
	testDriver = net_landriverlevel;

	SZ_Clear (&net_message);
	// save space for the header, filled in later
	MSG_WriteLong (&net_message, 0);
	MSG_WriteByte (&net_message, CCREQ_RULE_INFO);
	MSG_WriteString (&net_message, "");
	*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (testSocket, net_message.data, net_message.cursize, &sendaddr);
	SZ_Clear (&net_message);
	SchedulePollProcedure (&test2PollProcedure, 0.05);
}

static void Rcon_Poll (void);
PollProcedure	rconPollProcedure = {NULL, 0.0, Rcon_Poll};

static void Rcon_Poll (void)
{
	struct qsockaddr clientaddr;
	int		control, len;

	net_landriverlevel = testDriver;

	len = dfunc.Read(testSocket, net_message.data, net_message.maxsize, &clientaddr);
	if (len < sizeof(int))
	{
		testPollCount--;
		if (testPollCount)
		{
			SchedulePollProcedure (&rconPollProcedure, 0.25);
			return;
		}
		Con_Printf ("rcon: no response\n");
		goto Done;
	}

	net_message.cursize = len;

	MSG_BeginReading ();
	control = BigLong(*((int *)net_message.data));
	MSG_ReadLong ();
	if (control == -1)
		goto Error;
	if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
		goto Error;
	if ((control & NETFLAG_LENGTH_MASK) != len)
		goto Error;

	if (MSG_ReadByte() != CCREP_RCON)
		goto Error;

	Con_Printf ("%s\n", MSG_ReadString());

	goto Done;

Error:
	Con_Printf ("Unexpected repsonse to rcon command\n");

Done:
	dfunc.CloseSocket (testSocket);
	testInProgress = false;	
	return;
}

// joe: rcon from ProQuake
extern cvar_t rcon_password;
extern cvar_t rcon_server;
extern char server_name[MAX_QPATH];

static void Rcon_f (void)
{
	char	*host;
	int		n;
	struct qsockaddr sendaddr;

	if (testInProgress)
	{
		Con_Printf ("There is already a test/rcon in progress\n");
		return;
	}

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("Usage: rcon <command>\n");
		return;
	}

	if (!*rcon_password.string)
	{
		Con_Printf ("rcon_password has not been set\n");
		return;
	}

	host = rcon_server.string;

	if (!*rcon_server.string)
	{
		if (cls.state == ca_connected)
		{
			host = server_name;
		}
		else
		{
			Con_Printf ("rcon_server has not been set\n");
			return;
		}
	}

	//Strip_Port (host);//R00k disabled this so we can RCON to a specific port. ie "rcon_server my.quakeserver.com:26666"

	if (host && hostCacheCount)
	{
		for (n=0 ; n<hostCacheCount ; n++)
			if (!Q_strcasecmp(host, hostcache[n].name))
			{
				if (hostcache[n].driver != myDriverLevel)
					continue;
				net_landriverlevel = hostcache[n].ldriver;
				memcpy (&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				break;
			}
		if (n < hostCacheCount)
			goto JustDoIt;
	}

	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (dfunc.GetAddrFromName(host, &sendaddr) != -1)
			break;
	}
	if (net_landriverlevel == net_numlandrivers)
	{
		Con_Printf ("Could not resolve %s\n", host);
		return;
	}

JustDoIt:
	testSocket = dfunc.OpenSocket(0);
	if (testSocket == -1)			
	{
		Con_Printf ("Could not open socket\n");
		return;
	}

	testInProgress = true;
	testPollCount = 20;
	testDriver = net_landriverlevel;

	SZ_Clear (&net_message);
	// save space for the header, filled in later
	MSG_WriteLong (&net_message, 0);
	MSG_WriteByte (&net_message, CCREQ_RCON);
	MSG_WriteString (&net_message, rcon_password.string);
	MSG_WriteString (&net_message, Cmd_Args());
	*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write(testSocket, net_message.data, net_message.cursize, &sendaddr);
	SZ_Clear (&net_message);
	SchedulePollProcedure (&rconPollProcedure, 0.05);
}

int Datagram_Init (void)
{
	int	i, csock;

	myDriverLevel = net_driverlevel;
	Cmd_AddCommand ("net_stats", NET_Stats_f);

	if (COM_CheckParm("-nolan"))
		return -1;

	for (i=0 ; i<net_numlandrivers ; i++)
		{
			csock = net_landrivers[i].Init();
			if (csock == -1)
				continue;
			net_landrivers[i].initialized = true;
			net_landrivers[i].controlSock = csock;
		}

#ifdef BAN_TEST
	Cmd_AddCommand ("ban", NET_Ban_f);
#endif
	Cmd_AddCommand ("test", Test_f);
	Cmd_AddCommand ("test2", Test2_f);
	Cmd_AddCommand ("rcon", Rcon_f);	// joe: rcon from ProQuake

	return 0;
}

void Datagram_Shutdown (void)
{
	int	i;

// shutdown the lan drivers
	for (i=0 ; i<net_numlandrivers ; i++)
	{
		if (net_landrivers[i].initialized)
		{
			net_landrivers[i].Shutdown ();
			net_landrivers[i].initialized = false;
		}
	}
}

void Datagram_Close (qsocket_t *sock)
{
	sfunc.CloseSocket(sock->socket);
	//R00k clear the net_stats of this socket...
	unreliableMessagesSent = 0;
	unreliableMessagesReceived = 0;
	messagesSent = 0;
	messagesReceived = 0;
	packetsSent = 0;
	packetsReSent = 0;
	packetsReceived = 0;
	receivedDuplicateCount = 0;
	shortPacketCount = 0;
	droppedDatagrams = 0;
}

void Datagram_Listen (qboolean state)
{
	int	i;

	for (i=0 ; i<net_numlandrivers ; i++)
		if (net_landrivers[i].initialized)
			net_landrivers[i].Listen (state);
}

qsocket_t *Datagram_Reject (char *message, int acceptsock, struct qsockaddr *addr)
{
	SZ_Clear (&net_message);
	// save space for the header, filled in later
	MSG_WriteLong (&net_message, 0);
	MSG_WriteByte (&net_message, CCREP_REJECT);
	MSG_WriteString (&net_message, message);
	*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (acceptsock, net_message.data, net_message.cursize, addr);
	SZ_Clear (&net_message);

	return NULL;
}
/*
#define	MAX_BANNED_ADDR	1024

qsockaddr	bannedlist[MAX_BANNED_ADDR];
int			numbanned;

void SV_AddIP_f (void) 
{
	int i;

	for (i = 0; i < numbanned; i++)
		if (bannedlist[i].compare == 0xffffffff)
			break;		// free spot
	if (i == numbanned) 
	{
		if (numbanned == MAX_BANNED_ADDR) 
		{
			Com_Printf ("IP filter list is full\n");
			return;
		}
		numbanned++;
	}

	if (!StringToFilter (Cmd_Argv(1), &bannedlist[i]))
		bannedlist[i].compare = 0xffffffff;
}

void SV_RemoveIP_f (void) 
{
	ipfilter_t f;
	int i, j;

	if (!StringToFilter (Cmd_Argv(1), &f))
		return;
	for (i = 0; i < numbanned; i++) 
	{
		if (bannedlist[i].mask == f.mask && bannedlist[i].compare == f.compare) 
		{
			for (j = i + 1; j < numbanned; j++)
				bannedlist[j - 1] = bannedlist[j];
			numbanned--;
			Com_Printf ("Removed.\n");
			return;
		}
	}
	Com_Printf ("Didn't find %s.\n", Cmd_Argv(1));
}

void SV_ListIP_f (void) 
{
	int i;
	byte b[4];

	Com_Printf ("Filter list:\n");
	for (i = 0; i < numbanned; i++) 
	{
		*(unsigned *)b = bannedlist[i].compare;
		Com_Printf ("%3i.%3i.%3i.%3i\n", b[0], b[1], b[2], b[3]);
	}
}

void SV_WriteIP_f (void) 
{
	FILE *f;
	char name[MAX_OSPATH];
	byte b[4];
	int i;

	snprintf (name, sizeof(name), "%s/listip.cfg", com_gamedir);

	Com_Printf ("Writing %s.\n", name);

	if (!(f = fopen (name, "wb")))	
	{
		Com_Printf ("Couldn't open %s\n", name);
		return;
	}

	for (i = 0 ; i < numbanned ; i++) 
	{
		*(unsigned *) b = bannedlist[i].compare;
		fprintf (f, "addip %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
	}

	fclose (f);
}

void SV_SendBan (void)
{
	char data[128];

	data[0] = data[1] = data[2] = data[3] = 0xff;
	data[4] = A2C_PRINT;
	data[5] = 0;
	strlcat (data, "\nbanned.\n", sizeof (data));

	NET_SendPacket (NS_SERVER, strlen(data), data, net_from);
}

//Returns true if net_from.ip is banned
qbool SV_FilterPacket (void) 
{
	int	 i;
	unsigned in;

	if (net_from.type == NA_LOOPBACK)
		return false;	// the local client can't be banned

	in = *(unsigned *)net_from.ip;

	for (i = 0; i < numbanned; i++)
		if ( (in & bannedlist[i].mask) == bannedlist[i].compare)
			return filterban.value;

	return !filterban.value;
}
*/


static qsocket_t *_Datagram_CheckNewConnections (void)
{
	struct qsockaddr clientaddr;
	struct qsockaddr newaddr;
	int		newsock, acceptsock;
	qsocket_t	*sock;
	qsocket_t	*s;
	int		len, command, control, ret;
	byte		mod, mod_version;

	acceptsock = dfunc.CheckNewConnections();

	if (acceptsock == -1)
		return NULL;

	SZ_Clear (&net_message);

	len = dfunc.Read (acceptsock, net_message.data, net_message.maxsize, &clientaddr);

	if (len < sizeof(int))
		return NULL;

	net_message.cursize = len;

	MSG_BeginReading ();

	control = BigLong(*((int *)net_message.data));

	MSG_ReadLong ();

	if (control == -1)
		return NULL;

	if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
		return NULL;

	if ((control & NETFLAG_LENGTH_MASK) != len)
		return NULL;

	command = MSG_ReadByte ();

	if (command == CCREQ_SERVER_INFO)
	{
		char name[256];
		strcpy(name, hostname.string);

		if (strcmp(MSG_ReadString(), "QUAKE"))
			return NULL;

		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREP_SERVER_INFO);
		dfunc.GetSocketAddr(acceptsock, &newaddr);
		MSG_WriteString (&net_message, dfunc.AddrToString(&newaddr));
		MSG_WriteString(&net_message, name);	// JPG 3.50 changed hostname.string to name
		MSG_WriteString (&net_message, sv.name);
		MSG_WriteByte (&net_message, net_activeconnections);
		MSG_WriteByte (&net_message, svs.maxclients);
		MSG_WriteByte (&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear (&net_message);

		return NULL;
	}

	if (command == CCREQ_PLAYER_INFO)
	{
		int		playerNumber, activeNumber, clientNumber;
		client_t	*client;
		
		playerNumber = MSG_ReadByte ();
		activeNumber = -1;
		for (clientNumber = 0, client = svs.clients ; clientNumber < svs.maxclients ; clientNumber++, client++)
		{
			if (client->active)
			{
				activeNumber++;
				if (activeNumber == playerNumber)
					break;
			}
		}

		if (clientNumber == svs.maxclients)
			return NULL;

		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREP_PLAYER_INFO);
		MSG_WriteByte (&net_message, playerNumber);
		MSG_WriteString (&net_message, client->name);
		MSG_WriteLong (&net_message, client->colors);
		MSG_WriteLong (&net_message, (int)client->edict->v.frags);
		MSG_WriteLong (&net_message, (int)(net_time - client->netconnection->connecttime));
		MSG_WriteString (&net_message, client->netconnection->address);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear (&net_message);

		return NULL;
	}

	if (command == CCREQ_RULE_INFO)
	{
		char	*prevCvarName;
		cvar_t	*var;

		// find the search start location
		prevCvarName = MSG_ReadString ();
		if (*prevCvarName)
		{
			if (!(var = Cvar_FindVar(prevCvarName)))
				return NULL;
			var = var->next;
		}
		else
			var = cvar_vars;

		// search for the next server cvar
		while (var)
		{
			if (var->server)
				break;
			var = var->next;
		}

		// send the response

		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREP_RULE_INFO);
		if (var)
		{
			MSG_WriteString (&net_message, var->name);
			MSG_WriteString (&net_message, var->string);
		}
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear (&net_message);

		return NULL;
	}

	// joe: rcon from ProQuake
	if (command == CCREQ_RCON)
	{
		// 2048 = largest possible return from MSG_ReadString
		char	pass[2048], cmd[2048];

		strcpy (pass, MSG_ReadString());
		strcpy (cmd, MSG_ReadString());

		SZ_Clear (&rcon_message);
		// save space for the header, filled in later
		MSG_WriteLong (&rcon_message, 0);
		MSG_WriteByte (&rcon_message, CCREP_RCON);

		if (!*rcon_password.string)
			MSG_WriteString (&rcon_message, "rcon is disabled on this server");
		else if (strcmp(pass, rcon_password.string))
			MSG_WriteString (&rcon_message, "incorrect password");
		else
		{
			MSG_WriteString (&rcon_message, "");
			rcon_active = true;
			Cmd_ExecuteString (cmd, src_command);
			rcon_active = false;
		}

		*((int *)rcon_message.data) = BigLong (NETFLAG_CTL | (rcon_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write(acceptsock, rcon_message.data, rcon_message.cursize, &clientaddr);
		SZ_Clear(&rcon_message);

		return NULL;
	}

	if (command != CCREQ_CONNECT)
		return NULL;

	if (strcmp(MSG_ReadString(), "QUAKE"))
		return NULL;

	if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
		return Datagram_Reject ("Incompatible protocol version.\n", acceptsock, &clientaddr);

	// see if this guy is already connected
	for (s = net_activeSockets ; s ; s = s->next)
	{
		if (s->driver != net_driverlevel)
			continue;

		ret = dfunc.AddrCompare(&clientaddr, &s->addr);
		
		if (ret >= 0)
		{
			// is this a duplicate connection request?
			if (ret == 0 && net_time - s->connecttime < 2.0)
			{
				// yes, so send a duplicate reply
				SZ_Clear (&net_message);
				// save space for the header, filled in later
				MSG_WriteLong (&net_message, 0);
				MSG_WriteByte (&net_message, CCREP_ACCEPT);
				dfunc.GetSocketAddr(s->socket, &newaddr);
				s->client_port = dfunc.GetSocketPort(&newaddr);
				MSG_WriteLong (&net_message, s->client_port);
				MSG_WriteByte (&net_message, MOD_PROQUAKE);
				MSG_WriteByte (&net_message, 10 * MOD_PROQUAKE_VERSION);

				*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
				dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
				SZ_Clear (&net_message);
				return NULL;
			}
		}
	}

//TODO add new ban check...
/*		
		if (SV_FilterPacket ()) //Returns true if net_from.ip is banned
		{
			SV_SendBan ();	// tell them we aren't listening...
		}
*/

	//ProQuake/CRmod compatibility -- start
	if (len > 12)
		mod = MSG_ReadByte ();
	else
		mod = MOD_NONE;
	if (len > 13)
		mod_version = MSG_ReadByte ();
	else
		mod_version = 0;
	//ProQuake/CRmod compatibility -- end

//	if (mod != MOD_QSMACK)
	{
		if (pq_password.value && (len <= 18 || pq_password.value != MSG_ReadLong()))
			return Datagram_Reject("Σεςφες ισ πασσχοςδ πςοτεγτεδ\nset pq_password to the server password\n", acceptsock, &clientaddr);
	}

	// allocate a QSocket
	if (!(sock = NET_NewQSocket()))
		return Datagram_Reject ("Server is full.\n", acceptsock, &clientaddr);
	
	if (COM_CheckParm ("-startport"))//Skutarth
		newsock = dfunc.OpenSocket(sock->client_pool);//R00k:testing Skutarth's port forwarding pool code.
	else	
		newsock = dfunc.OpenSocket(0);

	if (newsock == -1)
	{
		NET_FreeQSocket (sock);
		Datagram_Reject ("No open ports available on server!\n", acceptsock, &clientaddr);//R00k
		return NULL;
	}

	// connect to the client
	if (dfunc.Connect(newsock, &clientaddr) == -1)
	{
		dfunc.CloseSocket(newsock);
		NET_FreeQSocket (sock);
		return NULL;
	}

	sock->mod = mod;
	sock->mod_version = mod_version;
	
	if (mod == MOD_PROQUAKE && mod_version >= 34)
		sock->net_wait = true;		// JPG 3.40 - NAT fix
	
	// everything is allocated, just fill in the details	
	sock->socket = newsock;
	sock->landriver = net_landriverlevel;
	sock->addr = clientaddr;
	strcpy (sock->address, dfunc.AddrToString(&clientaddr));

	// send him back the info about the server connection he has been allocated
	SZ_Clear (&net_message);
	// save space for the header, filled in later
	MSG_WriteLong (&net_message, 0);
	MSG_WriteByte (&net_message, CCREP_ACCEPT);
	dfunc.GetSocketAddr (newsock, &newaddr);
	sock->client_port = dfunc.GetSocketPort(&newaddr);
	MSG_WriteLong (&net_message, sock->client_port);

//	if (sv_proquake.value)
	{
		MSG_WriteByte (&net_message, MOD_PROQUAKE);
		MSG_WriteByte (&net_message, 10 * MOD_PROQUAKE_VERSION);
		MSG_WriteByte(&net_message, 0);
	}
	
	*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
	SZ_Clear (&net_message);

	return sock;
}

qsocket_t *Datagram_CheckNewConnections (void)
{
	qsocket_t	*ret = NULL;

	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
		if (net_landrivers[net_landriverlevel].initialized)
			if ((ret = _Datagram_CheckNewConnections ()))
				break;

	return ret;
}

static void _Datagram_SearchForHosts (qboolean xmit)
{
	int	i, n, ret, control;
	struct qsockaddr readaddr;
	struct qsockaddr myaddr;

	dfunc.GetSocketAddr (dfunc.controlSock, &myaddr);
	if (xmit)
	{
		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREQ_SERVER_INFO);
		MSG_WriteString (&net_message, "QUAKE");
		MSG_WriteByte (&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Broadcast(dfunc.controlSock, net_message.data, net_message.cursize);
		SZ_Clear (&net_message);
	}

	while ((ret = dfunc.Read(dfunc.controlSock, net_message.data, net_message.maxsize, &readaddr)) > 0)
	{
		if (ret < sizeof(int))
			continue;
		net_message.cursize = ret;

		// don't answer our own query
		if (dfunc.AddrCompare(&readaddr, &myaddr) >= 0)
			continue;

		// is the cache full?
		if (hostCacheCount == HOSTCACHESIZE)
			continue;

		MSG_BeginReading ();
		control = BigLong (*((int *)net_message.data));
		MSG_ReadLong ();
		if (control == -1)
			continue;
		if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
			continue;
		if ((control & NETFLAG_LENGTH_MASK) != ret)
			continue;

		if (MSG_ReadByte() != CCREP_SERVER_INFO)
			continue;

		dfunc.GetAddrFromName(MSG_ReadString(), &readaddr);

		// search the cache for this server
		for (n=0 ; n<hostCacheCount ; n++)
			if (dfunc.AddrCompare(&readaddr, &hostcache[n].addr) == 0)
				break;

		// is it already there?
		if (n < hostCacheCount)
			continue;

		// add it
		hostCacheCount++;
		strcpy (hostcache[n].name, MSG_ReadString());
		strcpy (hostcache[n].map, MSG_ReadString());
		hostcache[n].users = MSG_ReadByte ();
		hostcache[n].maxusers = MSG_ReadByte ();
		if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
		{
			strcpy (hostcache[n].cname, hostcache[n].name);
			hostcache[n].cname[14] = 0;
			strcpy (hostcache[n].name, "*");
			strcat (hostcache[n].name, hostcache[n].cname);
		}
		memcpy (&hostcache[n].addr, &readaddr, sizeof(struct qsockaddr));
		hostcache[n].driver = net_driverlevel;
		hostcache[n].ldriver = net_landriverlevel;
		strcpy (hostcache[n].cname, dfunc.AddrToString(&readaddr));

		// check for a name conflict
		for (i=0 ; i<hostCacheCount ; i++)
		{
			if (i == n)
				continue;
			if (!Q_strcasecmp(hostcache[n].name, hostcache[i].name))
			{
				i = strlen (hostcache[n].name);
				if (i < 15 && hostcache[n].name[i-1] > '8')
				{
					hostcache[n].name[i] = '0';
					hostcache[n].name[i+1] = 0;
				}
				else
				{
					hostcache[n].name[i-1]++;
				}
				i = -1;
			}
		}
	}
}

void Datagram_SearchForHosts (qboolean xmit)
{
	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
	{
		if (hostCacheCount == HOSTCACHESIZE)
			break;
		if (net_landrivers[net_landriverlevel].initialized)
			_Datagram_SearchForHosts (xmit);
	}
}

//Outgoing request to connect to a server..
static qsocket_t *_Datagram_Connect (char *host)
{
	struct		qsockaddr sendaddr;
	struct		qsockaddr readaddr;
	qsocket_t	*sock;
	int			newsock, ret, len, reps, clientsock;// JPG 3.40 - added clientsock
	double		start_time;
	int			control;
	char		*reason;
	extern int	DEFAULTnet_clientport;
	extern	cvar_t	cl_proquake;


	// see if we can resolve the host name
	if (dfunc.GetAddrFromName(host, &sendaddr) == -1)
	{	// joe: added error message from ProQuake
		Con_Printf ("Could not resolve %s\n", host);
		return NULL;
	}

	newsock = dfunc.OpenSocket(0);

	Con_DPrintf (1,"Using client-port: %i\n", newsock);

	if (newsock == -1)
		return NULL;

	if (!(sock = NET_NewQSocket()))
		goto ErrorReturn2;




	sock->socket = newsock;
	sock->landriver = net_landriverlevel;

	// connect to the host
	if (dfunc.Connect(newsock, &sendaddr) == -1)
		goto ErrorReturn;

	// send the connection request
	Con_Printf ("Trying to connect to %s...\n", host);
	SCR_UpdateScreen ();
	start_time = net_time;

	for (reps=0 ; reps<3 ; reps++)
	{
		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREQ_CONNECT);
		MSG_WriteString (&net_message, "QUAKE");
		MSG_WriteByte (&net_message, NET_PROTOCOL_VERSION);	//R00k: This gets confusing, but ALL netQuake1 servers are NET_PROTOCOL_VERSION 3
		if (cl_proquake.value)
		{
			MSG_WriteByte (&net_message, MOD_PROQUAKE);			//R00k: Imitate a ProQuake client so a PROQUAKE server will handle our angles properly!
			MSG_WriteByte (&net_message, MOD_PROQUAKE_VERSION * 10);
		}
		MSG_WriteByte(&net_message, 0);						// JPG 3.00 - added this (flags)
		MSG_WriteLong(&net_message, pq_password.value);		// JPG 3.00 - password protected servers

		*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (newsock, net_message.data, net_message.cursize, &sendaddr);
		SZ_Clear (&net_message);
		do
		{
			ret = dfunc.Read (newsock, net_message.data, net_message.maxsize, &readaddr);
			// if we got something, validate it
			if (ret > 0)
			{
				// is it from the right place?
				if (sfunc.AddrCompare(&readaddr, &sendaddr) != 0)
				{
#if 0	// joe: removed DEBUG
					Con_Printf ("wrong reply address\n");
					Con_Printf ("Expected: %s\n", StrAddr(&sendaddr));
					Con_Printf ("Received: %s\n", StrAddr(&readaddr));
					SCR_UpdateScreen ();
#endif
					ret = 0;
					continue;
				}

				if (ret < sizeof(int))
				{
					ret = 0;
					continue;
				}

				net_message.cursize = ret;
				MSG_BeginReading ();

				control = BigLong(*((int *)net_message.data));
				MSG_ReadLong ();
				if (control == -1)
				{
					ret = 0;
					continue;
				}
				if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
				{
					ret = 0;
					continue;
				}
				if ((control & NETFLAG_LENGTH_MASK) != ret)
				{
					ret = 0;
					continue;
				}
			}
		} while (ret == 0 && (SetNetTime() - start_time) < 2.5);

		if (ret)
			break;
		Con_Printf ("still trying...\n");
		SCR_UpdateScreen ();
		start_time = SetNetTime ();
	}

	if (ret == 0)
	{
		reason = "No Response";
		Con_Printf ("%s\n", reason);
		strcpy (m_return_reason, reason);

		goto ErrorReturn;
	}

	if (ret == -1)
	{
		reason = "Network Error";
		Con_Printf ("%s\n", reason);
		strcpy (m_return_reason, reason);

		goto ErrorReturn;
	}

	len = ret;
	ret = MSG_ReadByte ();
	if (ret == CCREP_REJECT)
	{
		reason = MSG_ReadString ();
		Con_Printf (reason);
		strncpy (m_return_reason, reason, 31);

		goto ErrorReturn;
	}

	if (ret == CCREP_ACCEPT)
	{
		sock->client_port = MSG_ReadLong ();

		memcpy (&sock->addr, &sendaddr, sizeof(struct qsockaddr));
		dfunc.SetSocketPort (&sock->addr, sock->client_port);

		//R00k: test for a ProQuake server..
		if (len > 9)
			sock->mod = MSG_ReadByte ();
		else
			sock->mod = MOD_NONE;

		if (len > 10)
		{
			sock->mod_version = MSG_ReadByte();
			Con_Printf("\nProQuake v%1.2i compatibility mode enabled.\n\n",sock->mod_version);						
		}
		else
			sock->mod_version = 0;		
	}
	else
	{
		reason = "Bad Response";
		Con_Printf ("%s\n", reason);
		strcpy (m_return_reason, reason);

		goto ErrorReturn;
	}

	dfunc.GetNameFromAddr(&sendaddr, sock->address);

	Con_Printf ("Connection accepted\n");
	sock->lastMessageTime = SetNetTime ();

	// joe, from ProQuake: make NAT work by opening a new socket
	if (sock->mod == MOD_PROQUAKE && sock->mod_version >= 34)
	{
		clientsock = dfunc.OpenSocket(0);
		if (clientsock == -1)
			goto ErrorReturn;
		dfunc.CloseSocket(newsock);
		newsock = clientsock;
		sock->socket = newsock;
	}

	// switch the connection to the specified address
	if (dfunc.Connect(newsock, &sock->addr) == -1)
	{
		reason = "Connect to Game failed";
		Con_Printf ("%s\n", reason);
		strcpy (m_return_reason, reason);

		goto ErrorReturn;
	}

	m_return_onerror = false;
	return sock;

ErrorReturn:
	NET_FreeQSocket (sock);

ErrorReturn2:
	dfunc.CloseSocket(newsock);
	if (m_return_onerror)
	{
		key_dest = key_menu;
		m_state = m_return_state;
		m_return_onerror = false;
	}

	return NULL;
}

qsocket_t *Datagram_Connect (char *host)
{
	qsocket_t *ret = NULL;

	Strip_Port (host);
	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
		if (net_landrivers[net_landriverlevel].initialized)
			if ((ret = _Datagram_Connect(host)) != NULL)
				break;

	return ret;
}
