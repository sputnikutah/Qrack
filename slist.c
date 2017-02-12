/*
Copyright (C) 1999-2000, contributors of the QuakeForge project

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
// slist.c -- serverlist addressbook

#include "quakedef.h"

server_entry_t	slist[MAX_SERVER_LIST];

void SList_Init (void)
{
	memset (&slist, 0, sizeof(slist));	
}

void SList_Save (FILE *f)
{
	int	i;

	for (i=0 ; i<MAX_SERVER_LIST ; i++)
	{
		if (!slist[i].server)
			break;

		fprintf (f, "%s\t%s\n", slist[i].server, slist[i].description);
	}
}

void SList_Shutdown (void)
{  
	FILE	*f;

	if (!(f = fopen (va("%s/qrack/servers.txt", com_basedir), "wt")))
	{
		Con_DPrintf (1,"Couldn't open servers.txt\n");
		return;
	}
	SList_Save (f);
	fclose (f);
}

void SList_Set (int i, char *addr, char *desc, char *players, char *map, char *mod)
{
	if (i >= MAX_SERVER_LIST || i < 0)
		Sys_Error ("SList_Set: Bad index %d", i);

	if (slist[i].players)
		Z_Free (slist[i].players);

	if (slist[i].server)
		Z_Free (slist[i].server);

	if (slist[i].description)
		Z_Free (slist[i].description);

	if (slist[i].map)
		Z_Free (slist[i].map);

	if (slist[i].mod)
		Z_Free (slist[i].mod);

	slist[i].server		 = CopyString (addr);
	slist[i].players	 = CopyString (players);
	slist[i].description = CopyString (desc);
	slist[i].map		 = CopyString (map);
	slist[i].mod		 = CopyString (mod);
}

void SList_Reset_NoFree (int i)
{ 
	if (i >= MAX_SERVER_LIST || i < 0)
		Sys_Error ("SList_Reset_NoFree: Bad index %d", i);

	slist[i].server		 = 
	slist[i].players	 = 
	slist[i].description = 
	slist[i].map		 = 
	slist[i].mod		 = NULL;

}

void SList_Reset (int i)
{
	if (i >= MAX_SERVER_LIST || i < 0)
		Sys_Error ("SList_Reset: Bad index %d", i);

	if (slist[i].server)
	{
		Z_Free (slist[i].server);
		slist[i].server = NULL;
	}

	if (slist[i].description)
	{
		Z_Free (slist[i].description);
		slist[i].description = NULL;
	}
	
	if (slist[i].players)
	{		
		Z_Free (slist[i].players);
		slist[i].players = NULL;
	}

	if (slist[i].map)
	{
		Z_Free (slist[i].map);
		slist[i].map = NULL;
	}

	if (slist[i].mod)
	{
		Z_Free (slist[i].mod);
		slist[i].mod = NULL;
	}
}

void SList_Switch (int a, int b)
{
	server_entry_t	temp;

	if (a >= MAX_SERVER_LIST || a < 0)
		Sys_Error ("SList_Switch: Bad index %d", a);
	if (b >= MAX_SERVER_LIST || b < 0)
		Sys_Error ("SList_Switch: Bad index %d", b);

	memcpy (&temp, &slist[a], sizeof(temp));
	memcpy (&slist[a], &slist[b], sizeof(temp));
	memcpy (&slist[b], &temp, sizeof(temp));
}

int SList_Length (void)
{
	int	count;

	for (count = 0 ; count < MAX_SERVER_LIST && slist[count].server ; count++)
		;

	return count;
}

void SList_Load (void)
{
	int	c, len, argc, count, i;
	char	line[128], *desc, *addr, *plyrs, *country, *temp, *map, *mod;
	FILE	*f;
	extern cvar_t slist_filter_noplayers;
	
	if (!(f = fopen(va("%s/qrack/servers.txt",com_basedir), "rt")))		
		return;

	i = count = len = 0;

	while ((c = getc(f)))
	{
		if (c == '\n' || c == '\r' || c == EOF)
		{
			if (c == '\r' && (c = getc(f)) != '\n' && c != EOF)
				ungetc (c, f);

			line[len] = 0;
			len = 0;
			Cmd_TokenizeString (line);

			if ((argc = Cmd_Argc()) >= 1)
			{				
				for (i = 0; i <= Cmd_Argc(); i++)//find the token for players
				{
					if (strstr(Cmd_Argv(i),"/"))
					{
						plyrs = Cmd_Argv(i);
						map = Cmd_Argv(i + 1);
						mod = Cmd_Argv(i + 2);
						break;
					}
				}

				if (slist_filter_noplayers.value)//Skip entries with no players
				{
					if (plyrs)
						if(strstr(plyrs,"00/"))
							continue;
				}
								
				{
					if (plyrs)
						if(strstr(plyrs,"/00")) //dead server
							continue;
				}

				addr = Cmd_Argv(0);//Fill in the actual ip/dns
				country = Cmd_Argv(1);//Grab the country code even if we are not printing it
				temp = (argc >= 2) ? Cmd_Args() : "Unknown";//copy the rest of the line
				
				for (i = 0;i < strlen(country);i++)//move our position forward past the country 
					temp++;				
				
				desc = temp;

				Q_strncpyz (desc, temp, 19);//servers.quakeone.com only copies 19 characters over for the hostname :(

				SList_Set (count, addr, temp, plyrs, map, mod);//Write out the line to the file.

				if (++count == MAX_SERVER_LIST)
					break;				
			}
			if (c == EOF)
				break;	//just in case an EOF follows a '\r'
		}
		else
		{
			if (len + 1 < sizeof(line))
				line[len++] = c;
		}
	}
	fclose (f);
}

