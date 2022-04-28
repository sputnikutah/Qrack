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
// cl_parse.c -- parse a message received from the server

#include "quakedef.h"

char *svc_strings[] =
{
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",			// [long] server version
	"svc_setview",			// [short] entity number
	"svc_sound",			// <see code>
	"svc_time",				// [float] server time
	"svc_print",			// [string] null terminated string
	"svc_stufftext",		// [string] stuffed into client's console buffer
							// the string should be \n terminated
	"svc_setangle",			// [vec3] set the view angle to this absolute value
	
	"svc_serverinfo",		// [long] version
							// [string] signon string
							// [string]..[0]model cache [string]...[0]sounds cache
							// [string]..[0]item cache
	"svc_lightstyle",		// [byte] [string]
	"svc_updatename",		// [byte] [string]
	"svc_updatefrags",		// [byte] [short]
	"svc_clientdata",		// <shortbits + data>
	"svc_stopsound",		// <see code>
	"svc_updatecolors",		// [byte] [byte]
	"svc_particle",			// [vec3] <variable>
	"svc_damage",			// [byte] impact [byte] blood [vec3] from
	
	"svc_spawnstatic",
	"OBSOLETE svc_spawnbinary",
	"svc_spawnbaseline",
	
	"svc_temp_entity",		// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",			// [string] music [string] text
	"svc_cdtrack",			// [byte] track [byte] looptrack
	"svc_sellscreen",
	"svc_cutscene",
// nehahra support
	"svc_showlmp",			// [string] iconlabel [string] lmpfile [byte] x [byte] y
	"svc_hidelmp",			// [string] iconlabel
	"svc_skybox",			// [string] skyname
	"?",	// 38	
	"?",	// 39
	"?",	// 40
	"svc_fog",				// 41 [byte] density [byte] red [byte] green [byte] blue [float] time
	"svc_spawnbaseline2", //42			// support for large modelindex, large framenum, alpha, using flags
	"svc_spawnstatic2", // 43			// support for large modelindex, large framenum, alpha, using flags
	"svc_spawnstaticsound2", //	44		// [coord3] [short] samp [byte] vol [byte] aten
	"?",	// 45
	"?",	// 46
	"?",	// 47
	"?",	// 48
	"?"	// 49
};

/*
===============
CL_EntityNum

This function checks and tracks the total number of entities
===============
*/
entity_t *CL_EntityNum (int num)
{
	if (num < 0)//johnfitz -- check minimum number too
		Host_Error ("CL_EntityNum: %i is an invalid number", num);

	if (num >= cl.num_entities)
	{
		if (num >= cl_max_edicts) //johnfitz -- no more MAX_EDICTS
			Host_Error ("CL_EntityNum: %i is >= cl_max_edicts. Increase your max_edicts cvar value.", num);

		while (cl.num_entities <= num)
		{
			cl_entities[cl.num_entities].colormap = 0;
			cl_entities[cl.num_entities].lerpflags |= LERP_RESETMOVE|LERP_RESETANIM; //johnfitz
			cl.num_entities++;
		}
	}

	return &cl_entities[num];
}


/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket (void)
{
	vec3_t	pos;
	int		i, channel, ent, sound_num, volume, field_mask;
	float	attenuation;  

	field_mask = MSG_ReadByte ();

	if (field_mask & SND_VOLUME)
		volume = MSG_ReadByte ();
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

	if (field_mask & SND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;
	
	//johnfitz -- PROTOCOL_FITZQUAKE
	if (field_mask & SND_LARGEENTITY)
	{
		ent = (unsigned short) MSG_ReadShort ();
		channel = MSG_ReadByte ();
	}
	else
	{
		channel = (unsigned short) MSG_ReadShort ();
		ent = channel >> 3;
		channel &= 7;
	}

	if (field_mask & SND_LARGESOUND)
		sound_num = (unsigned short) MSG_ReadShort ();
	else
		sound_num = MSG_ReadByte ();
	//johnfitz

	//johnfitz -- check soundnum
	if (sound_num >= MAX_SOUNDS)
		Host_Error ("CL_ParseStartSoundPacket: %i > MAX_SOUNDS", sound_num);
	//johnfitz

	if (ent > cl_max_edicts) //johnfitz -- no more MAX_EDICTS
		Host_Error ("CL_ParseStartSoundPacket: cl_max_edicts exceeded @ ent = %i", ent);

	for (i=0 ; i<3 ; i++)
		pos[i] = MSG_ReadCoord ();
	S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}       
/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/
void CL_KeepaliveMessage (void)
{
	float			time;
	static	float	lastmsg;
	int				ret;
	sizebuf_t		old;
	byte			olddata[8192];//NET_MAXMESSAGE ?? Fixme?
	
	if (sv.active)
		return;		// no need if server is local

	if (cls.demoplayback)
		return;

// read messages from server, should just be nops
	old = net_message;
	memcpy (olddata, net_message.data, net_message.cursize);
	
	do
	{
		ret = CL_GetMessage ();
		switch (ret)
		{
		default:
			Host_Error ("CL_KeepaliveMessage: CL_GetMessage failed");			
		case 0:
			break;	// nothing waiting
		case 1:
			Host_Error ("CL_KeepaliveMessage: received a message");
			break;
		case 2:
			if (MSG_ReadByte() != svc_nop)
				Host_Error ("CL_KeepaliveMessage: datagram wasn't a nop");
			break;
		}
	} while (ret);

	net_message = old;
	memcpy (net_message.data, olddata, net_message.cursize);

// check time
	time = Sys_DoubleTime ();
	if (time - lastmsg < 5)
		return;

	lastmsg = time;

// write out a nop
	Con_Printf ("--> client to server keepalive\n");

	MSG_WriteByte (&cls.message, clc_nop);
	NET_SendMessage (cls.netcon, &cls.message);

	SZ_Clear (&cls.message);
}

/*
   =====================
   CL_WebDownloadProgress
   Callback function for webdownloads.
   Since Web_Get only returns once it's done, we have to do various things here:
   Update download percent, handle input, redraw UI and send net packets.
   =====================
*/
static int CL_WebDownloadProgress( double percent )
{
	static double now, oldtime, newtime;

	if (cls.download.percent != percent)
	{
		Con_Printf(".");
	}

	cls.download.percent = percent;	

	CL_KeepaliveMessage();	

	newtime = Sys_DoubleTime ();
	now = newtime - oldtime;

	Host_Frame (now);

	oldtime = newtime;

	return cls.download.disconnect; // abort if disconnect received
}

qboolean CL_WebDownload(char *precache_filename)
{
	char url[1024];
	qboolean success = false;
	char download_tempname[MAX_OSPATH],download_finalname[MAX_OSPATH];
	char folder[MAX_OSPATH];
	char name[MAX_OSPATH];
	extern char server_name[MAX_QPATH];
	extern int net_hostport;
	extern int Web_Get( const char *url, const char *referer, const char *name, int resume, int max_downloading_time, int timeout, int ( *_progress )(double) );
	extern cvar_t cl_web_download;
	extern cvar_t cl_web_download_url;

	//Create the FULL path where the file should be written
	Q_snprintfz (download_tempname, MAX_OSPATH, "%s/%s.tmp", com_gamedir, precache_filename);
	
	//determine the proper folder and create it, the OS will ignore if already exists
	COM_GetFolder(precache_filename,folder);// "progs/","maps/"
	Q_snprintfz (name, sizeof(name), "%s/%s", com_gamedir, folder);		
	
	Sys_mkdir (name);								

	Q_snprintfz( url, sizeof( url ), "%s", cl_web_download_url.string);

	Con_Printf( "Downloading:\nfile: %s\nfrom: %s\n\n", precache_filename, url);

	//assign the url + path + file + extension we want
	Q_snprintfz( url, sizeof( url ), "%s%s", url, precache_filename);

	cls.download.web = true;
	cls.download.disconnect = false;
	cls.download.error = NULL;
	cls.download.percent = 0.0;

	//let libCURL do it's magic!!
	success = Web_Get(url, NULL, download_tempname, false, 600, 30, CL_WebDownloadProgress);				

	cls.download.web = false;

	if (cls.download.disconnect)//if the user types disconnect in the middle of the download
	{
		cls.download.disconnect = false;
		remove (download_tempname);
		CL_Disconnect_f();
		return false;
	}				

	if (success)
	{
		Con_Printf("Download succesfull.\n\n");
		//Rename the .tmp file to the final precache filename
		Q_snprintfz (download_finalname, MAX_OSPATH, "%s/%s", com_gamedir, precache_filename);					
		rename (download_tempname, download_finalname);							
		return true;
	}
	else
	{					
		Con_Printf ("\nThe required file '%s' could not be found at %s.\n", precache_filename, url);
		remove (download_tempname);
		return false;
	}				
}
/*
==================
CL_ParseServerInfo
==================
*/
void CL_ParseServerInfo (void)
{
	char	*str, tempname[MAX_QPATH];
	int		i, nummodels, numsounds;
	char	model_precache[MAX_MODELS][MAX_QPATH];
	char	sound_precache[MAX_SOUNDS][MAX_QPATH];
	char	mapname[MAX_QPATH];

	extern	void R_PreMapLoad (char *);
#ifdef GLQUAKE
	extern qboolean r_loadq3player;
#endif
	extern cvar_t cl_web_download;
	extern cvar_t cl_web_download_url;
	extern cvar_t cl_levelshots;
	extern cvar_t scr_centerprint_levelname;
	extern char		con_lastcenterstring[1024]; //johnfitz

	Con_DPrintf (1,"Serverinfo packet received.\n");

// wipe the client_state_t struct
	CL_ClearState ();

// parse protocol version number
	i = MSG_ReadLong ();

	if ((i != PROTOCOL_NETQUAKE)&&(i != PROTOCOL_FITZQUAKE))
	{
		Con_Printf ("\nServer returned invalid version: %i.", i);
		return;
	}

	cl.protocol = i;

// parse maxclients
	cl.maxclients = MSG_ReadByte ();
	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
	{
		Con_Printf ("Bad maxclients (%u) from server\n", cl.maxclients);
		return;
	}
	cl.scores = Hunk_AllocName (cl.maxclients * sizeof(*cl.scores), "scores");
	cl.teamscores = Hunk_AllocName (14 * sizeof(*cl.teamscores), "teamscores"); // JPG - for teamscore status bar
// parse gametype
	cl.gametype = MSG_ReadByte ();

// parse signon message
	str = MSG_ReadString ();

	strncpy (cl.levelname, str, sizeof(cl.levelname)-1);

// seperate the printfs so the server message can have a color
	Con_Printf ("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_Printf ("%c%s\n", 2, str);

//johnfitz -- tell user which protocol this is
	Con_Printf ("Using protocol %i\n", i);

// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it

// precache models
	for (i=0 ; i<NUM_MODELINDEX ; i++)//Clear out index
		cl_modelindex[i] = -1;

	memset (cl.model_precache, 0, sizeof(cl.model_precache));

	for (nummodels=1 ; ; nummodels++)
	{
		str = MSG_ReadString ();

		if (!str[0])
			break;

		if (nummodels == MAX_MODELS)
		{
			Con_Printf ("Server sent too many model precaches\n");
			return;
		}

		#ifdef GLQUAKE
		if (gl_loadq3models.value)
		{
			if (!strcmp(str, "progs/player.mdl") && 
			    COM_FindFile("progs/player/head.md3") && 
			    COM_FindFile("progs/player/upper.md3") && 
			    COM_FindFile("progs/player/lower.md3"))
			{
				Q_strncpyz (tempname, "progs/player/lower.md3", MAX_QPATH);
				str = tempname;
				cl_modelindex[mi_player] = nummodels;
				r_loadq3player = true;
			}
			else
			{
				COM_StripExtension (str, tempname);
				strcat (tempname, ".md3");

				if (COM_FindFile(tempname))
					str = tempname;
			}
		}
		#endif
		
		Q_strncpyz (model_precache[nummodels], str, sizeof(model_precache[nummodels]));
		Con_DPrintf (1,"Precaching model (%i):%s\n",nummodels,model_precache[nummodels]);

		if (nummodels == 1)
		{
			COM_StripExtension (COM_SkipPath(model_precache[1]), mapname);
			R_PreMapLoad (mapname);	
			SCR_UpdateScreen();//levelshots are handled asap.
		}

		Mod_TouchModel (str);

		if (!strcmp(model_precache[nummodels], "progs/player.mdl"))
			cl_modelindex[mi_player] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/player.md3"))//R00k 1.92
			cl_modelindex[mi_md3_player] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/eyes.mdl"))
			cl_modelindex[mi_eyes] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/missile.mdl"))
			cl_modelindex[mi_rocket] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/grenade.mdl"))
			cl_modelindex[mi_grenade] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/flame.mdl"))
			cl_modelindex[mi_flame1] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/flame.md3"))
			cl_modelindex[mi_md3_flame0] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/flame2.mdl"))
			cl_modelindex[mi_flame2] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/s_expl.spr"))
			cl_modelindex[mi_explo1] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/s_explod.spr"))
			cl_modelindex[mi_explo2] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/s_bubble.spr"))
			cl_modelindex[mi_bubble] = nummodels;

		else if (!strcmp(model_precache[nummodels], "progs/gib1.mdl"))
			cl_modelindex[mi_gib1] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/gib2.mdl"))
			cl_modelindex[mi_gib2] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/gib3.mdl"))
			cl_modelindex[mi_gib3] = nummodels;

		else if (!strcmp(model_precache[nummodels], "progs/fish.mdl"))
			cl_modelindex[mi_fish] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/dog.mdl"))
			cl_modelindex[mi_dog] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/soldier.mdl"))
			cl_modelindex[mi_soldier] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/enforcer.mdl"))
			cl_modelindex[mi_enforcer] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/knight.mdl"))
			cl_modelindex[mi_knight] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/hknight.mdl"))
			cl_modelindex[mi_hknight] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/wizard.mdl"))
			cl_modelindex[mi_scrag] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/ogre.mdl"))
			cl_modelindex[mi_ogre] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/demon.mdl"))
			cl_modelindex[mi_fiend] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/shalrath.mdl"))
			cl_modelindex[mi_vore] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/shambler.mdl"))
			cl_modelindex[mi_shambler] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_dog.mdl"))
			cl_modelindex[mi_h_dog] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_guard.mdl"))
			cl_modelindex[mi_h_soldier] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_mega.mdl"))
			cl_modelindex[mi_h_enforcer] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_knight.mdl"))
			cl_modelindex[mi_h_knight] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_hknight.mdl"))
			cl_modelindex[mi_h_hknight] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_wizard.mdl"))
			cl_modelindex[mi_h_scrag] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_ogre.mdl"))
			cl_modelindex[mi_h_ogre] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_demon.mdl"))
			cl_modelindex[mi_h_fiend] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_shal.mdl"))
			cl_modelindex[mi_h_vore] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_shams.mdl"))
			cl_modelindex[mi_h_shambler] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_zombie.mdl"))
			cl_modelindex[mi_h_zombie] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/zombie.mdl"))
			cl_modelindex[mi_zombie] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/h_player.mdl"))
			cl_modelindex[mi_h_player] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/w_s_key.mdl"))
			cl_modelindex[mi_w_s_key] = nummodels;
		else if (!strcmp(model_precache[nummodels], "progs/w_g_key.mdl"))
			cl_modelindex[mi_w_g_key] = nummodels;
  }

  // load the rest of the q3 player model if possible
#ifdef GLQUAKE
	if (r_loadq3player)
	{
		if (nummodels + 1 >= MAX_MODELS)
		{
			Con_Printf ("Server sent too many model precaches -> can't load Q3 player model\n");
			Q_strncpyz (model_precache[cl_modelindex[mi_player]], cl_modelnames[mi_player], sizeof(model_precache[cl_modelindex[mi_player]]));
		}
		else
		{
			Q_strncpyz (model_precache[nummodels], "progs/player/upper.md3", sizeof(model_precache[nummodels]));
			cl_modelindex[mi_q3torso] = nummodels++;
			Q_strncpyz (model_precache[nummodels], "progs/player/head.md3", sizeof(model_precache[nummodels]));
			cl_modelindex[mi_q3head] = nummodels++;
		}
	}
#endif
 //johnfitz -- check for excessive models
	if (nummodels >= 256)
		Con_Warning ("%i models exceeds standard limit of 256.\n", nummodels);
//johnfitz

// precache sounds
	memset (cl.sound_precache, 0, sizeof(cl.sound_precache));
	for (numsounds=1 ; ; numsounds++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (numsounds == MAX_SOUNDS)
		{
			Con_Printf ("Server sent too many sound precaches.\n");
			return;
		}
		strcpy (sound_precache[numsounds], str);
		S_TouchSound (str);
	}

	//johnfitz -- check for excessive sounds
	if (numsounds >= 256)
		Con_Warning ("%i sounds exceeds standard limit of 256.\n", numsounds);
	//johnfitz

	// now we try to load everything else until a cache allocation fails
	for (i = 1 ; i < nummodels ; i++)
	{
		cl.model_precache[i] = Mod_ForName (model_precache[i], false);
		
		if (cl.model_precache[i] == NULL)
		{
			SCR_EndLoadingPlaque ();

			if (!cls.demoplayback && cl_web_download.value && cl_web_download_url.string)
			{	
				if (CL_WebDownload(model_precache[i]))
				{
					i -= 1;
				}
				else
				{					
					break;
				}				
			}//************************************************************web download end
			else
			{
				Con_Printf("Model %s was not found.\n", model_precache[i]);
				return;
			}
		}
		CL_KeepaliveMessage();
	}
	
	// load the extra "no-flamed-torch" model
	if (cl.model_precache[nummodels] = Mod_ForName ("progs/flame0.mdl", false))
		cl_modelindex[mi_flame0] = nummodels++;
	
	//R00k added ...
	if (cl.model_precache[nummodels] = Mod_ForName ("progs/flag.mdl", false))
		cl_modelindex[mi_flag] = nummodels++;
		
	//R00k added sprite models
//--------------------------------------------------------------------------------
	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_shells.spr", false);
	cl_modelindex[mi_2dshells] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_cells.spr", false);
	cl_modelindex[mi_2dcells] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_rockets.spr", false);
	cl_modelindex[mi_2drockets] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_nails.spr", false);
	cl_modelindex[mi_2dnails] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_invuln.spr", false);
	cl_modelindex[mi_2dpent] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_quad.spr", false);
	cl_modelindex[mi_2dquad] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_suit.spr", false);
	cl_modelindex[mi_2dsuit] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_invis.spr", false);
	cl_modelindex[mi_2dring] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_mega.spr", false);
	cl_modelindex[mi_2dmega] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_health10.spr", false);
	cl_modelindex[mi_2dhealth10] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_health25.spr", false);
	cl_modelindex[mi_2dhealth25] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_armor1.spr", false);
	cl_modelindex[mi_2darmor1] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_armor2.spr", false);
	cl_modelindex[mi_2darmor2] = nummodels++;
	
	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_armor3.spr", false);
	cl_modelindex[mi_2darmor3] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_backpack.spr", false);
	cl_modelindex[mi_2dbackpack] = nummodels++;

	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_g_shot.spr", false);
	cl_modelindex[mi_2dssg] = nummodels++;
	
	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_g_nail.spr", false);
	cl_modelindex[mi_2dng] = nummodels++;
	
	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_g_nail2.spr", false);
	cl_modelindex[mi_2dsng] = nummodels++;
	
	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_g_rock.spr", false);
	cl_modelindex[mi_2dgl] = nummodels++;
	
	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_g_rock2.spr", false);
	cl_modelindex[mi_2drl] = nummodels++;
	
	cl.model_precache[nummodels] = Mod_ForName ("sprites/s_g_light.spr", false);
	cl_modelindex[mi_2dlg] = nummodels++;
	
	for (i=1 ; i<numsounds ; i++)
	{
		cl.sound_precache[i] = S_PrecacheSound (sound_precache[i]);

		if (!cl.sound_precache[i])
		{
			if (!cls.demoplayback && cl_web_download.value && cl_web_download_url.string)
			{				
				if (CL_WebDownload(sound_precache[i]))
				{
					i--;
				}
			}
			else
			{
				Con_Printf("Sound file %s was not found\n", sound_precache[i]);
				return;
			}
		}
		CL_KeepaliveMessage ();
	}

	// local state
	cl_entities[0].model = cl.worldmodel = cl.model_precache[1];

	if (cl.worldmodel)//If a download fails then dont init the map.
	{
		char	filename[MAX_OSPATH];
		
		if (LOC_LoadLocations()== false)	// JPG - read in the location data for the new map
		{
			if ((!cls.demoplayback) && (cl_web_download.value) && (cl_web_download_url.string) && (!sv.active))
			{
				CL_KeepaliveMessage();
				Q_snprintfz (filename, sizeof(filename), "locs/%s", sv_mapname.string);
				COM_ForceExtension (filename, ".loc");

				if (CL_WebDownload(filename))//try to download the locfile if we dont have it locally
				{
					CL_KeepaliveMessage();
					LOC_LoadLocations();
				}
			}
		}

		R_NewMap ();
		//johnfitz -- clear out string; we don't consider identical
		//messages to be duplicates if the map has changed in between
		con_lastcenterstring[0] = 0;
		//johnfitz

		if ((scr_centerprint_levelname.value)||(cl_levelshots.value))
			SCR_CenterPrint (cl.levelname);//Quake64
	}

	Hunk_Check ();			// make sure nothing is hurt
	
	noclip_anglehack = false;	// noclip is turned off at start
}


/*
==================
CL_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked. Other attributes can change without relinking.
==================
*/
int	bitcounts[16];

//extern cvar_t cl_teamskin;

void CL_ParseUpdate (int bits)
{
	int			i, num;
	model_t		*model;
	qboolean	forcelink;
	entity_t	*ent;
	qboolean	isPlayer = false;
	extern cvar_t	r_powerupglow;

#ifdef GLQUAKE
	int		skin;
#endif

	if (cls.signon == SIGNONS - 1)
	{	// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	if (bits & U_MOREBITS)
	{
		i = MSG_ReadByte ();
		bits |= (i<<8);
	}

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol == PROTOCOL_FITZQUAKE)
	{
		if (bits & U_EXTEND1)
			bits |= MSG_ReadByte() << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte() << 24;
	}
	//johnfitz

	num = (bits & U_LONGENTITY) ? MSG_ReadShort() : MSG_ReadByte();

	if ((num > 0 && num <= cl.maxclients))
		isPlayer = true;

	ent = CL_EntityNum (num);

	for (i=0 ; i<16 ; i++)
		if (bits & (1 << i))
			bitcounts[i]++;

	forcelink = (ent->msgtime != cl.mtime[1]) ? true : false;

	//johnfitz -- lerping
	if (ent->msgtime + 0.2 < cl.mtime[0]) //more than 0.2 seconds since the last message (most entities think every 0.1 sec)
		ent->lerpflags |= LERP_RESETANIM; //if we missed a think, we'd be lerping from the wrong frame
	//johnfitz

	ent->msgtime = cl.mtime[0];

	if (bits & U_MODEL)
	{
		ent->modelindex = MSG_ReadByte ();
		if (ent->modelindex >= MAX_MODELS)
			Host_Error ("CL_ParseUpdate: bad modelindex");
	}
	else
	{
		ent->modelindex = ent->baseline.modelindex;
	}

	ent->frame = (bits & U_FRAME) ? MSG_ReadByte() : ent->baseline.frame;

	i = (bits & U_COLORMAP) ? MSG_ReadByte() : ent->baseline.colormap;

	if (i > cl.maxclients)
	{
		//Sys_Error ("MSG_ReadByte: U_COLORMAP >= cl.maxclients");
		i = 0;
	}	

	if ((i > 0 && i <= cl.maxclients) && (ent->model) && (ent->model->modhint == MOD_PLAYER))
	{
		ent->shirtcolor = (cl.scores[i - 1].colors & 0xf0) >> 4;//R00k
		ent->pantscolor = (cl.scores[i - 1].colors & 15);		//R00k
		ent->colormap = i;
	}
	else
	{
		ent->colormap = 0;
	}

#ifdef GLQUAKE
	skin = (bits & U_SKIN) ? MSG_ReadByte() : ent->baseline.skin;

	if (skin != ent->skinnum)
	{
		ent->skinnum = skin;
		if (isPlayer)
		{
			R_TranslatePlayerSkin (num - 1);
		}
	}
#else
	ent->skinnum = (bits & U_SKIN) ? MSG_ReadByte() : ent->baseline.skin;
#endif

	ent->effects = (bits & U_EFFECTS) ? MSG_ReadByte() : ent->baseline.effects;

// shift the known values for interpolation
	VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
	VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);

	ent->msg_origins[0][0] = (bits & U_ORIGIN1) ? MSG_ReadCoord() : ent->baseline.origin[0];
	ent->msg_angles[0][0] = (bits & U_ANGLE1) ? MSG_ReadAngle() : ent->baseline.angles[0];

	ent->msg_origins[0][1] = (bits & U_ORIGIN2) ? MSG_ReadCoord() : ent->baseline.origin[1];
	ent->msg_angles[0][1] = (bits & U_ANGLE2) ? MSG_ReadAngle() : ent->baseline.angles[1];

	ent->msg_origins[0][2] = (bits & U_ORIGIN3) ? MSG_ReadCoord() : ent->baseline.origin[2];
	ent->msg_angles[0][2] = (bits & U_ANGLE3) ? MSG_ReadAngle() : ent->baseline.angles[2];

	//johnfitz -- lerping for movetype_step entities
	if ( bits & U_STEP )
	{
		ent->lerpflags |= LERP_MOVESTEP;
		ent->forcelink = true;
	}
	else
		ent->lerpflags &= ~LERP_MOVESTEP;
	//johnfitz

	//johnfitz -- PROTOCOL_FITZQUAKE and PROTOCOL_NEHAHRA
	if (cl.protocol == PROTOCOL_FITZQUAKE)
	{
		if (bits & U_ALPHA)
			ent->alpha = MSG_ReadByte();
		else
			ent->alpha = ent->baseline.alpha;

		//R00k: transparency was already implemented using floats, so instead of having to go back and rewrite for alpha i'll just decode the alpha to a float and pass it to transparency :(
		ent->transparency = ENTALPHA_DECODE(ent->alpha);

		if (bits & U_FRAME2)
			ent->frame = (ent->frame & 0x00FF) | (MSG_ReadByte() << 8);
		if (bits & U_MODEL2)
			ent->modelindex = (ent->modelindex & 0x00FF) | (MSG_ReadByte() << 8);
		if (bits & U_LERPFINISH)
		{
			ent->lerpfinish = ent->msgtime + ((float)(MSG_ReadByte()) / 255);
			ent->lerpflags |= LERP_FINISH;
		}
		else
			ent->lerpflags &= ~LERP_FINISH;
	}
	else if (cl.protocol == PROTOCOL_NETQUAKE)
	{
		//HACK: if this bit is set, assume this is PROTOCOL_NEHAHRA
		if (bits & U_TRANS)
		{
			int	fullbright, temp;
	
			temp = MSG_ReadFloat ();
			ent->transparency = MSG_ReadFloat ();
	
			if (ent->transparency > 1)
			{
				ent->transparency /= 255;//HalfLife support
			}
	
			if (temp == 2)
				fullbright = MSG_ReadFloat ();
		}
		else
		{
			ent->transparency = 1.0;
			ent->alpha = ent->baseline.alpha;
		}
	}

	//johnfitz -- moved here from above
	model = cl.model_precache[ent->modelindex];

	if (model != ent->model)
	{
		ent->model = model;
	// automatic animation (torches, etc) can be either all together
	// or randomized
		if (model)
		{
			if (model->synctype == ST_RAND)
				ent->syncbase = (float)(rand()&0x7fff) / 0x7fff;
			else
				ent->syncbase = 0.0;
		}
		else
			forcelink = true;	// hack to make null model players work

		if (num > 0 && num <= cl.maxclients)
			R_TranslatePlayerSkin (num - 1); 

		ent->lerpflags |= LERP_RESETANIM; //johnfitz -- don't lerp animation across model changes
	}
	//johnfitz

	if (forcelink)
	{	// didn't have an update last message
		ent->flytime = 0;//R00k: Reset the fly counter	

		ent->lerpflags |= LERP_RESETANIM; //johnfitz -- don't lerp animation across model changes
		VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
		VectorCopy (ent->msg_origins[0], ent->origin);
		VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
		VectorCopy (ent->msg_angles[0], ent->angles);
		ent->forcelink = true;
		ent->frame_start_time = 0;
		ent->frame_interval = 0.1;//default
		ent->pose1 = ent->pose2 = 0;
		ent->translate_start_time = 0;
	}	
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (entity_t *ent, int version) //johnfitz -- added argument
{
	int	i;
	int bits; //johnfitz

	//johnfitz -- PROTOCOL_FITZQUAKE
	bits = (version == 2) ? MSG_ReadByte() : 0;
	ent->baseline.modelindex = (bits & B_LARGEMODEL) ? MSG_ReadShort() : MSG_ReadByte();
	ent->baseline.frame = (bits & B_LARGEFRAME) ? MSG_ReadShort() : MSG_ReadByte();
	//johnfitz

	ent->baseline.colormap = MSG_ReadByte();
	ent->baseline.skin = MSG_ReadByte();
	for (i=0 ; i<3 ; i++)
	{
		ent->baseline.origin[i] = MSG_ReadCoord ();
		ent->baseline.angles[i] = MSG_ReadAngle ();
	}
	ent->baseline.alpha = (bits & B_ALPHA) ? MSG_ReadByte() : ENTALPHA_ONE; //johnfitz -- PROTOCOL_FITZQUAKE
}

/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata (void)
{
	int	i, j;
	int	bits; //johnfitz

	bits = (unsigned short)MSG_ReadShort (); //johnfitz -- read bits here isntead of in CL_ParseServerMessage()

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (bits & SU_EXTEND1)
		bits |= (MSG_ReadByte() << 16);
	if (bits & SU_EXTEND2)
		bits |= (MSG_ReadByte() << 24);
	//johnfitz

	cl.viewheight = (bits & SU_VIEWHEIGHT) ? MSG_ReadChar() : DEFAULT_VIEWHEIGHT;//22

	cl.idealpitch = (bits & SU_IDEALPITCH) ? MSG_ReadChar() : 0;
	
	VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);

	for (i=0 ; i<3 ; i++)
	{
		cl.punchangle[i] = (bits & (SU_PUNCH1 << i)) ? MSG_ReadChar() : 0;
		cl.mvelocity[0][i] = (bits & (SU_VELOCITY1 << i)) ? MSG_ReadChar()*16 : 0;
	}

	i = MSG_ReadLong ();

	if (cl.items != i)
	{	// set flash times
		Sbar_Changed ();
		for (j=0 ; j<32 ; j++)
			if ((i & (1 << j)) && !(cl.items & (1 << j)))
				cl.item_gettime[j] = cl.time;
		cl.items = i;
	}

	cl.onground = (bits & SU_ONGROUND) != 0;
	cl.inwater = (bits & SU_INWATER) != 0;

	cl.stats[STAT_WEAPONFRAME] = (bits & SU_WEAPONFRAME) ? MSG_ReadByte() : 0;

	i = (bits & SU_ARMOR) ? MSG_ReadByte() : 0;
	if (cl.stats[STAT_ARMOR] != i)
	{
		cl.stats[STAT_ARMOR] = i;		
		Sbar_Changed ();
	}

	i = (bits & SU_WEAPON) ? MSG_ReadByte() : 0;
	
	if (cl.stats[STAT_WEAPON] != i)
	{
		cl.stats[STAT_WEAPON] = i;
		Sbar_Changed ();
		//johnfitz -- lerping
		if (cl.viewent.model != cl.model_precache[cl.stats[STAT_WEAPON]])
			cl.viewent.lerpflags |= LERP_RESETANIM; //don't lerp animation across model changes
		//johnfitz
	}
	
	i = MSG_ReadShort ();
	if (cl.stats[STAT_HEALTH] != i)
	{
		if (i <= 0)
		memcpy(cl.death_location, cl_entities[cl.viewentity].origin, sizeof(vec3_t));
		cl.stats[STAT_HEALTH] = i;
		Sbar_Changed ();
	}

	i = MSG_ReadByte ();
	if (cl.stats[STAT_AMMO] != i)
	{
		cl.stats[STAT_AMMO] = i;
		Sbar_Changed ();
	}

	for (i=0 ; i<4 ; i++)
	{
		j = MSG_ReadByte ();
		if (cl.stats[STAT_SHELLS+i] != j)
		{
			cl.stats[STAT_SHELLS+i] = j;
			Sbar_Changed ();
		}
	}

	i = MSG_ReadByte ();

	if (hipnotic || rogue || quoth)
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != (1 << i))
		{
			cl.stats[STAT_ACTIVEWEAPON] = (1 << i);
			Sbar_Changed ();
		}
	}
	else
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != i)
		{
			cl.stats[STAT_ACTIVEWEAPON] = i;
			Sbar_Changed ();
		}
	}
	//johnfitz -- PROTOCOL_FITZQUAKE
	if (bits & SU_WEAPON2)
		cl.stats[STAT_WEAPON] |= (MSG_ReadByte() << 8);
	if (bits & SU_ARMOR2)
		cl.stats[STAT_ARMOR] |= (MSG_ReadByte() << 8);
	if (bits & SU_AMMO2)
		cl.stats[STAT_AMMO] |= (MSG_ReadByte() << 8);
	if (bits & SU_SHELLS2)
		cl.stats[STAT_SHELLS] |= (MSG_ReadByte() << 8);
	if (bits & SU_NAILS2)
		cl.stats[STAT_NAILS] |= (MSG_ReadByte() << 8);
	if (bits & SU_ROCKETS2)
		cl.stats[STAT_ROCKETS] |= (MSG_ReadByte() << 8);
	if (bits & SU_CELLS2)
		cl.stats[STAT_CELLS] |= (MSG_ReadByte() << 8);
	if (bits & SU_WEAPONFRAME2)
		cl.stats[STAT_WEAPONFRAME] |= (MSG_ReadByte() << 8);
//	if (bits & SU_WEAPONALPHA)
//		cl.viewent.transparency = MSG_ReadFloat();
//	else
//		cl.viewent.transparency = 1;
	//johnfitz
}


/*
=====================
CL_NewTranslation
=====================
*/
void CL_NewTranslation (int slot)
{
	R_TranslatePlayerSkin (slot);

}

/*
=====================
CL_ParseStatic
=====================
*/
void CL_ParseStatic (int version) //johnfitz -- added a parameter
{
	entity_t	*ent;
		
	if (cl.num_statics >= MAX_STATIC_ENTITIES)
		Host_Error ("Too many static entities");

	ent = &cl_static_entities[cl.num_statics];
	cl.num_statics++;
	CL_ParseBaseline (ent, version); //johnfitz -- added second parameter

// copy it to the current state
	ent->model = cl.model_precache[ent->baseline.modelindex];
	ent->lerpflags |= LERP_RESETANIM; //johnfitz -- lerping
	ent->frame = ent->baseline.frame;
	ent->colormap = 0;
	ent->skinnum = ent->baseline.skin;
	ent->effects = ent->baseline.effects;
	ent->alpha = ent->baseline.alpha; //johnfitz -- alpha
	VectorCopy (ent->baseline.origin, ent->origin);
	VectorCopy (ent->baseline.angles, ent->angles);
	R_AddEfrags (ent);
}

/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound (int version) //johnfitz -- added argument
{
	vec3_t	org;
	int	i, sound_num, vol, atten;
	
	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (version == 2)
		sound_num = MSG_ReadShort ();
	else
		sound_num = MSG_ReadByte ();
	//johnfitz

	vol = MSG_ReadByte ();
	atten = MSG_ReadByte ();
	
	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}


#define SHOWNET(x)						\
	if (cl_shownet.value == 2)				\
		Con_Printf ("%3i:%s\n", msg_readcount - 1, x)

// JPG - added this
int MSG_ReadBytePQ (void)
{
	return MSG_ReadByte() * 16 + MSG_ReadByte() - 272;
}

// JPG - added this
int MSG_ReadShortPQ (void)
{	
	return (MSG_ReadBytePQ() * 256 + MSG_ReadBytePQ());
}

qboolean cl_mm2;//R00k added for cl_mute
/* JPG - added this function for ProQuake messages
=======================
CL_ParseProQuakeMessage
=======================
*/
void CL_ParseProQuakeMessage (void)
{
	int cmd, i;
	int team, shirt, frags, ping;
	extern void SCR_DrawPing (int ping);
	
	MSG_ReadByte();
	cmd = MSG_ReadByte();
	
	switch (cmd)
	{
	case pqc_new_team:
		Sbar_Changed ();
		team = MSG_ReadByte() - 16;
		if (team < 0 || team > 15)
			Host_Error ("CL_ParseProQuakeMessage: pqc_new_team invalid team");
		shirt = MSG_ReadByte() - 16;
		cl.teamgame = true;
		cl.teamscores[team].colors = 16 * shirt + team;
		cl.teamscores[team].frags = 0;
		//Con_Printf("pqc_new_team %d %d\n", team, shirt);
		break;

	case pqc_erase_team:
		Sbar_Changed ();
		team = MSG_ReadByte() - 16;
		if (team < 0 || team > 15)
			Host_Error ("CL_ParseProQuakeMessage: pqc_erase_team invalid team");
		cl.teamscores[team].colors = 0;
		cl.teamscores[team].frags = 0;		// JPG 3.20 - added this
		//Con_Printf("pqc_erase_team %d\n", team);
		break;

	case pqc_team_frags:
		Sbar_Changed ();
		cl.teamgame = true;
		team = MSG_ReadByte() - 16;
		if (team < 0 || team > 15)
			Host_Error ("CL_ParseProQuakeMessage: pqc_team_frags invalid team");
		frags = MSG_ReadShortPQ();
		if (frags & 32768)
			frags = (frags - 65536);
		cl.teamscores[team].frags = frags;
		//Con_DPrintf (1,"pqc_team_frags %d %d\n", team, frags);
	break;			

	case pqc_match_time:		
		Sbar_Changed ();
		cl.teamgame = true;
		cl.minutes = MSG_ReadBytePQ();
		cl.seconds = MSG_ReadBytePQ();
		cl.last_match_time = cl.ctime;//R00k: fixed for cl_demorewind
		//Con_Printf("pqc_match_time %d %d\n", cl.minutes, cl.seconds);
		break;

	case pqc_match_reset:
		Sbar_Changed ();
		cl.teamgame = true;
		for (i = 0 ; i < 14 ; i++)
		{
			cl.teamscores[i].colors = 0;
			cl.teamscores[i].frags = 0;		// JPG 3.20 - added this
		}
		//Con_Printf("pqc_match_reset\n");
		break;

	case pqc_ping_times:
		while (ping = MSG_ReadShortPQ())
		{
			if ((ping / 4096) >= cl.maxclients)
			{
				Con_DPrintf (1,"CL_ParseProQuakeMessage: pqc_ping_times > MAX_SCOREBOARD\n");
			}
			else
			{
				cl.scores[ping / 4096].ping = (ping & 4095);
			}
		}
		cl.last_ping_time = cl.time;
		break;		
	}
}

//  Q_VERSION

int MHz,Mem;

#pragma warning(disable: 4035)

static __forceinline __int64 rdtsc64 () { _asm rdtsc }
//R00k: TODO: REPLACE with function to QueryProcessCycleTime
void GetMHz(void)
{
	__int64 q0, q1, t0, t1, qfrq;

	QueryPerformanceFrequency((LARGE_INTEGER *)&qfrq);

	QueryPerformanceCounter((LARGE_INTEGER *)&q0);
	t0 = rdtsc64();

	while (1) //Force at least a 1/32 second delay (use longer for more accuracy)
	{
		QueryPerformanceCounter((LARGE_INTEGER *)&q1);
		if (q1-q0 >= (qfrq>>5)) 
			break;
	}

	QueryPerformanceCounter((LARGE_INTEGER *)&q1);
	t1 = rdtsc64();

	MHz= (((double)qfrq) * ((double)(t1-t0)) / ((double)(q1-q0)) / 1000000);
}

void GetMem()
{
	MEMORYSTATUS ms;
	ms.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus(&ms);
	Mem = ms.dwTotalPhys >> 10;
}

char *VID_GetModeDescription (int mode);
extern int vid_modenum;

void Q_Version(char *s)
{
	extern cvar_t cl_echo_qversion;
	static float qv_time = 0;
	static float qi_time = 0;
	char *t;
	int l = 0, n = 0;

	if (cl_echo_qversion.value == 0)
		return;

	t = s;

	while (*t != ':')//skip name
		t++;

	l = strlen(t);

	while (n < l)
	{					
		if (!strncmp(t, "q_version", 9))
		{
			if ((qv_time > 0) && (qv_time > realtime))
			{
				Con_Printf ("q_version spam active; try again in %02.0f seconds.\n",(qv_time - realtime));
				return;
			}

			if (cl_mm2)
				Cbuf_AddText (va("say_team Qrack %s\n", VersionString()));
			else
				Cbuf_AddText (va("say Qrack %s\n", VersionString()));

			Cbuf_Execute ();
			qv_time = realtime + 20;		
			break; // Baker: only do once per string
		}
		if (!strncmp(t, "q_sysinfo", 9))
		{
			if ((qi_time > 0) && (qi_time > realtime))
			{
				Con_Printf ("q_sysinfo spam active; try again in %02.0f seconds.\n",(qv_time - realtime));
				return;
			}

			GetMem();
			GetMHz();

			if (cl_mm2)
			{
				Cbuf_AddText(va("say_team %iMHz %iMB %s\n\n", MHz, (int)(Mem/1024), gl_renderer));
				Cbuf_AddText(va("say_team Video mode %s \n", VID_GetModeDescription (vid_modenum)));
				#ifdef WINVER
				Cbuf_AddText(va("say_team Windows Version %x \n", WINVER));
				#endif
			}
			else
			{
				Cbuf_AddText(va("say %iMHz %iMB %s\n\n", MHz, (int)(Mem/1024), gl_renderer));
				Cbuf_AddText(va("say Video mode %s \n", VID_GetModeDescription (vid_modenum)));
				#ifdef WINVER
				Cbuf_AddText(va("say Windows Version %x \n", WINVER));
				#endif
			}
			qi_time = realtime + 20;
			Cbuf_Execute ();
			break; // Baker: only do once per string
		}		
		n++; t++;
	}
}

extern cvar_t pq_scoreboard_pings; // JPG - need this for CL_ParseProQuakeString
void CL_ParseString (char *string)
{
	static int checkping = -1;
	int i,a,b,c;
	static int checkip = -1;	// player whose IP address we're expecting
	// JPG 1.05 - for ip logging
	static int remove_status = 0;
	static int begin_status = 0;
	static int playercount = 0;
	extern	HWND	mainwindow;

	const char *t, *s;//R00k
	extern cvar_t cl_autofocus;

	if (!strcmp(string, "Client ping times:\n") && pq_scoreboard_pings.value)
	{		
		cl.last_ping_time = cl.time;
		checkping = 0;
		if (!cl.console_ping)
			*string = 0;
	}
	else if (checkping >= 0)
	{		
		t = string;
		while (*t == ' ')
			t++;
		if (*t >= '0' && *t <= '9')
		{
			int ping = atoi(t);
			while (*t >= '0' && *t <= '9')
				t++;
			if (*t == ' ')
			{
				int charindex = 0;
				t++;
				for (charindex = 0;cl.scores[checkping].name[charindex] == t[charindex];charindex++);
				if (cl.scores[checkping].name[charindex] == 0 && t[charindex] == '\n')
				{
					cl.scores[checkping].ping = bound(0, ping, 9999);
					for (checkping++;checkping < cl.maxclients && !cl.scores[checkping].name[0];checkping++);
				}
				if (!cl.console_ping)
					*string = 0;
				if (checkping == cl.maxclients)
					checkping = -1;
			}
			else
				checkping = -1;
		}
		else
			checkping = -1;
		cl.console_ping = cl.console_ping && (checkping >= 0);	// JPG 1.05 cl.sbar_ping -> cl.console_ping		
	}

	//R00k added for cl_mute
	if (string[1] == '(')
	{
		cl_mm2 = true;
	}	
	else
	{
		cl_mm2 = false;
	}

	// check for match time
	if (!strncmp(string, "Match ends in ", 14))
	{
		s = string + 14;
		if ((*s != 'T') && strchr(s, 'm'))
		{
			sscanf(s, "%d", &cl.minutes);
			cl.seconds = 0;
			cl.last_match_time = cl.time;
		}
	}
	else 
	{
		if (!strcmp(string, "Match paused\n"))
		{
			//TODO:R00k add a pause for demo if recording...(SVC_SETPAUSE ?)
			cl.match_pause_time = cl.time;	
		}
		else
		{
			if (!strcmp(string, "Match unpaused\n"))
			{
				cl.last_match_time += (cl.time - cl.match_pause_time);
				cl.match_pause_time = 0;
			}
			else 
			{
				if (!strcmp(string, "The match is over\n"))
				{
					cl.minutes = 255;
					if ((cl_autodemo.value == 2) && (cls.demorecording))
						Cmd_ExecuteString ("stop\n", src_command);
				}
				else 
				{					
					if (!strncmp(string, "Match begins in", 15)) 
					{
						if (cl_autofocus.value == 2)
						{							
							if (GetForegroundWindow() != mainwindow)
							{
								FlashWindow(mainwindow, true);								
							}
						}
						else
						{
							if (cl_autofocus.value == 1)
							{							
								if (GetForegroundWindow() != mainwindow)
								{									
									SetForegroundWindow (mainwindow);
								}
							}
						}
						cl.minutes = 255;
					}
					else
					{
						if ((cl_autodemo.value == 2) && ((!cls.demoplayback)&&(!cls.demorecording)))
						{
							if ( (!strncmp(string, "The match has begun!", 20)) || (!strncmp(string, "minutes remaining", 17)) )//crmod doesnt say "begun" so catch the 1st instance of minutes remain, makes the demos miss initial spawn though :(
							{
								Cmd_ExecuteString ("record\n", src_command);
							}
						}
						else 
						{
							if (checkping < 0)
							{
								s = string;
								i = 0;
								while (*s >= '0' && *s <= '9')
									i = 10 * i + *s++ - '0';
								if (!strcmp(s, " minutes remaining\n"))
								{
									cl.minutes = i;
									cl.seconds = 0;
									cl.last_match_time = cl.time;
								}
							}						
						}
					}
				}
			}
		}
	}

	// JPG 1.05 check for IP information
	if (iplog_size)
	{
		if (!strncmp(string, "host:    ", 9))
		{
			begin_status = 1;
			if (!cl.console_status)
				remove_status = 1;
		}
		if (begin_status && !strncmp(string, "players: ", 9))
		{
			begin_status = 0;
			remove_status = 0;
			if (sscanf(string + 9, "%d", &playercount))
			{
				if (!cl.console_status)
					*string = 0;
			}
			else
				playercount = 0;
		}
		else if (playercount && string[0] == '#')
		{
			if (!sscanf(string, "#%d", &checkip) || --checkip < 0 || checkip >= cl.maxclients)
				checkip = -1;
			if (!cl.console_status)
				*string = 0;
			remove_status = 0;
		}
		else if (checkip != -1)
		{
			if (sscanf(string, "   %d.%d.%d", &a, &b, &c) == 3)
			{
				cl.scores[checkip].addr = (a << 16) | (b << 8) | c;
				IPLog_Add(cl.scores[checkip].addr, cl.scores[checkip].name);
			}
			checkip = -1;
			if (!cl.console_status)
				*string = 0;
			remove_status = 0;

			if (!--playercount)
				cl.console_status = 0;
		}
		else 
		{
			playercount = 0;
			if (remove_status)			
				*string = 0;
		}
	}
	Q_Version(string);//R00k: look for "q_version" requests
}

extern cvar_t	scr_nocenterprint;
/*
=====================
CL_ParseServerMessage
=====================
*/
void Con_LogCenterPrint (char *str);
void Fog_ParseServerMessage (void);

void CL_ParseServerMessage (void)
{
	int		cmd, i;
	extern	float	printstats_limit;		//
	extern	cvar_t	scr_printstats;			// by joe
	extern	cvar_t	scr_printstats_length;	//
	extern	cvar_t	cl_nomessageprint;
	char		*str; //johnfitz
	char		*s;	//PROQUAKE
	extern qboolean fmod_loaded;
	extern void FMOD_PlayTrack (byte track);


// if recording demos, copy the message out
	if (cl_shownet.value == 1)
		Con_Printf ("%i ", net_message.cursize);
	else if (cl_shownet.value == 2)
		Con_Printf ("------------------\n");
	
	cl.onground = false;	// unless the server says otherwise	

// parse the message
	MSG_BeginReading ();
	
	while (1)
	{
		if (msg_badread)
			Host_Error ("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte ();
		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			return;		// end of message
		}

	// if the high bit of the command byte is set, it is a fast update
		if (cmd & U_SIGNAL) //johnfitz -- was 128, changed for clarity
		{
			SHOWNET("fast update");
			CL_ParseUpdate (cmd & 127);
			continue;
		}

		SHOWNET(svc_strings[cmd]);
	
	// other commands
		switch (cmd)
		{
		default:
			Host_Error ("CL_ParseServerMessage: Illegible server message, %i", cmd);
			break;
			
		case svc_nop:
			break;
			
		case svc_time:
			cl.mtime[1] = cl.mtime[0];
			cl.mtime[0] = MSG_ReadFloat ();
			break;

		case svc_clientdata:
			CL_ParseClientdata ();
			break;
		
		case svc_version:
			i = MSG_ReadLong ();
			if (i != PROTOCOL_NETQUAKE)
				Host_Error ("CL_ParseServerMessage: Server is protocol %i instead of %i", i, PROTOCOL_NETQUAKE);
			cl.protocol = i;
			Con_Printf("using protocol %i\n",i);
			break;
			
		case svc_disconnect:
			Host_EndGame ("Server disconnected\n");
			break;

		case svc_print:
			// JPG - check to see if the message contains useful information
			s = MSG_ReadString();
			CL_ParseString(s);
			
			if (cl_nomessageprint.value == 0)
			{
				Con_Printf ("%s", s);
			}

			if (scr_printstats.value == 2)
				printstats_limit = cl.time + scr_printstats_length.value;
			break;
		
		case svc_centerprint:			
			str = MSG_ReadString ();
			if (scr_nocenterprint.value == 0)
			{				
				SCR_CenterPrint (str);									
			}
			Con_LogCenterPrint (str);//johnfitz -- log centerprints to console
			break;
			
		case svc_stufftext:
			// JPG - check for ProQuake message
			if (MSG_PeekByte() == MOD_PROQUAKE)
				CL_ParseProQuakeMessage();

			// Still want to add text, even on ProQuake messages.  This guarantees compatibility;
			// unrecognized messages will essentially be ignored but there will be no parse errors
			Cbuf_AddText (MSG_ReadString ());
			break;
			
		case svc_damage:
			V_ParseDamage ();

			if (scr_printstats.value == 2)
				printstats_limit = cl.time + scr_printstats_length.value;
			break;
			
		case svc_serverinfo:
			CL_ParseServerInfo ();
			vid.recalc_refdef = true;	// leave intermission full screen
			break;
			
		case svc_setangle:
			if(cls.demoplayback && freelooking.value)
			{
				MSG_ReadAngle ();
				MSG_ReadAngle ();
				MSG_ReadAngle ();
			}
			else
			{
				for (i=0 ; i<3 ; i++)
					cl.viewangles[i] = MSG_ReadAngle ();
			}
		
			if (!cls.demoplayback)
			{
				VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);

				// JPG - hack with last_angle_time to autodetect continuous svc_setangles
				if (last_angle_time > host_time - 0.3)
					last_angle_time = host_time + 0.3;
				else if (last_angle_time > host_time - 0.6)
					last_angle_time = host_time;
				else
					last_angle_time = host_time - 0.3;

				for (i=0 ; i<3 ; i++)
					cl.mviewangles[0][i] = cl.viewangles[i];
			}
			break;			

		case svc_setview:			
				cl.viewentity = MSG_ReadShort ();
			break;
					
		case svc_lightstyle:
			i = MSG_ReadByte ();

			if (i >= MAX_LIGHTSTYLES)
			{
				if (i <= 256)				//	R00k: I had MAX_LIGHTSTYLES set to 256 for Halflife map support. I dont really see a need for those maps beyond novelty. 
					MSG_ReadString();		//			Though, old demos were recorded with up to 256 lightstyles. So, now i have to just read past them so engine doesnt complain.
				else
				Sys_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
			}
			else
			{
				Q_strncpyz (cl_lightstyle[i].map, MSG_ReadString(), sizeof(cl_lightstyle[i].map));
				cl_lightstyle[i].length = strlen(cl_lightstyle[i].map);
			}
			break;
			
		case svc_sound:
			CL_ParseStartSoundPacket ();
			break;
			
		case svc_stopsound:
			i = MSG_ReadShort ();
			S_StopSound (i>>3, i&7);
			break;
		
		case svc_updatename:
			Sbar_Changed ();
			i = MSG_ReadByte ();

			if (i >= cl.maxclients)
				Host_Error (va("CL_ParseServerMessage: svc_updatename (%i) > maxclients (%i)"),i,cl.maxclients);//R00k changed to be more legible.

			Q_strncpyz (cl.scores[i].name, MSG_ReadString(), sizeof(cl.scores[i].name));
			break;
			
		case svc_updatefrags:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags > MAX_SCOREBOARD");
			cl.scores[i].frags = MSG_ReadShort ();
			break;			

		case svc_updatecolors:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatecolors > MAX_SCOREBOARD");
			cl.scores[i].colors = MSG_ReadByte ();
			CL_NewTranslation (i);
			break;
			
		case svc_particle:
			R_ParseParticleEffect ();
			break;

		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline (CL_EntityNum(i), 1); // johnfitz -- added second parameter
			break;

		case svc_spawnstatic:
			CL_ParseStatic (1);
			break;			

		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_setpause:
			if ((cl.paused = MSG_ReadByte()))
			{
				CDAudio_Pause ();
#ifdef _WIN32
				VID_HandlePause (true);
#endif
			}
			else
			{
				CDAudio_Resume ();
#ifdef _WIN32
				VID_HandlePause (false);
#endif
			}
			break;
			
		case svc_signonnum:
			i = MSG_ReadByte ();
			if (i <= cls.signon)
				Host_Error ("Received signon %i when at %i", i, cls.signon);
			cls.signon = i;
			//johnfitz -- if signonnum==2, signon packet has been fully parsed, so check for excessive static ents and efrags
			if (i == 2)
			{
				if (cl.num_statics > 128)
					Con_DPrintf(1,"%i static entities exceeds standard limit of 128.\n", cl.num_statics);
			}
			//johnfitz
			CL_SignonReply ();
			break;

		case svc_killedmonster:
			if (cls.demoplayback && cl_demorewind.value)
				cl.stats[STAT_MONSTERS]--;
			else
				cl.stats[STAT_MONSTERS]++;
			if (scr_printstats.value == 2)
				printstats_limit = cl.time + scr_printstats_length.value;
			break;

		case svc_foundsecret:
			if (cls.demoplayback && cl_demorewind.value)
				cl.stats[STAT_SECRETS]--;
			else
				cl.stats[STAT_SECRETS]++;
			if (scr_printstats.value == 2)
				printstats_limit = cl.time + scr_printstats_length.value;
			break;

		case svc_updatestat:
			i = MSG_ReadByte ();
			if (i < 0 || i >= MAX_CL_STATS)
			{
				Con_DPrintf (1,"CL_ParseServerMessage: svc_updatestat: %i is invalid\n", i);

				//MH: keep message state consistent
				MSG_ReadLong ();
				break;
			}
			cl.stats[i] = MSG_ReadLong ();
			break;
			
		case svc_spawnstaticsound:
			CL_ParseStaticSound (1);
			break;

		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte ();
			cl.looptrack = MSG_ReadByte ();
			
			if (fmod_loaded)
			{	
				FMOD_PlayTrack (cl.cdtrack);
				break;
			}

			if ((cls.demoplayback || cls.demorecording) && (cls.forcetrack != -1))
				CDAudio_Play ((byte)cls.forcetrack, true);
			else
				CDAudio_Play ((byte)cl.cdtrack, true);
			break;

		case svc_intermission:
			if (cls.demoplayback && cl_demorewind.value)
				cl.intermission = 0;
			else
			{
				cl.intermission = 1;			
				// intermission bugfix -- by joe
				cl.completed_time = cl.mtime[0];
			}

			vid.recalc_refdef = true;	// go to full screen
			break;

		case svc_finale:
			if (cls.demoplayback && cl_demorewind.value)
				cl.intermission = 0;
			else
			{
				cl.intermission = 2;
				cl.completed_time = cl.mtime[0];
				vid.recalc_refdef = true;	// go to full screen
				//johnfitz -- log centerprints to console
				str = MSG_ReadString ();
				Con_DPrintf (1,"svc_finale: cl.intermission = 2\n");
				SCR_CenterPrint (str);
				Con_LogCenterPrint (str);
				//johnfitz
			}
			break;

		case svc_cutscene:
			if (cls.demoplayback && cl_demorewind.value)
				cl.intermission = 0;
			else
			{
				cl.intermission = 3;
				cl.completed_time = cl.mtime[0];
				vid.recalc_refdef = true;	// go to full screen
				//johnfitz -- log centerprints to console
				str = MSG_ReadString ();
				SCR_CenterPrint (str);
				Con_LogCenterPrint (str);
				//johnfitz
			}
			break;

		case svc_sellscreen:
			Cmd_ExecuteString ("help", src_command);
			break;

		// nehahra support
		case svc_hidelmp:
			SHOWLMP_decodehide ();
			break;

		case svc_showlmp:
			SHOWLMP_decodeshow ();
			break;

		case svc_skybox:
			Cvar_Set ("r_skybox", MSG_ReadString());
			break;

		case svc_fog:
			Fog_ParseServerMessage ();
			break;

		case svc_spawnbaseline2: //PROTOCOL_FITZQUAKE
			i = MSG_ReadShort ();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline (CL_EntityNum(i), 2);
			break;

		case svc_spawnstatic2: //PROTOCOL_FITZQUAKE
			CL_ParseStatic (2);
			break;

		case svc_spawnstaticsound2: //PROTOCOL_FITZQUAKE
			CL_ParseStaticSound (2);
			break;
		//johnfitz
		}
	}
}

