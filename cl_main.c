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
// cl_main.c -- client main loop

#include "quakedef.h"
#include "curl.h"

extern int in_impulse;
// these two are not intended to be set directly
cvar_t	cl_name				= {"_cl_name", "player", true};
cvar_t	cl_color			= {"_cl_color", "0", true};
cvar_t	cl_echo_qversion	= {"cl_echo_qversion", "1", true};
cvar_t	pingplreport		= {"pingplreport", "0", false};//R00k Not implemented yet *IGNORE SPAMMING CONSOLE*
cvar_t	cl_shownet			= {"cl_shownet","0"};	// can be 0, 1, or 2
cvar_t	cl_nolerp			= {"cl_nolerp","0"};
cvar_t	lookspring			= {"lookspring","0", true};
cvar_t	lookstrafe			= {"lookstrafe","0", true};
cvar_t	sensitivity			= {"sensitivity","3", true};
cvar_t	cl_autofocus		= {"cl_autofocus","1", true};//R00k: if alt-tabbed away and a Match is about to start, bring us back into the game!
// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.
cvar_t	m_pitch				= {"m_pitch","0.022", true};
cvar_t	m_yaw				= {"m_yaw","0.022", true};
cvar_t	m_forward			= {"m_forward","1", true};
cvar_t	m_side				= {"m_side","0.8", true};

//PROQUAKE ADDITIONS _START
// JPG - added these for %r formatting
cvar_t	pq_needrl			= {"pq_needrl", "I need RL", true};
cvar_t	pq_haverl			= {"pq_haverl", "I have RL", true};
cvar_t	pq_needrox			= {"pq_needrox", "I need rockets", true};

// JPG - added these for %p formatting
cvar_t	pq_quad				= {"pq_quad", "quad", true};
cvar_t	pq_pent				= {"pq_pent", "pent", true};
cvar_t	pq_ring				= {"pq_ring", "eyes", true};

// R00k: added for %a formatting
cvar_t	pq_armor			= {"pq_armor", "[G]:[Y]:[R]", true};

// JPG 3.00 - added these for %w formatting
cvar_t	pq_weapons			= {"pq_weapons", "SSG:NG:SNG:GL:RL:LG", true};
cvar_t	pq_noweapons		= {"pq_noweapons", "no weapons", true};

// JPG 1.05 - translate +jump to +moveup under water
cvar_t	pq_moveup			= {"pq_moveup", "1", true};

// JPG 3.00 - added this by request
cvar_t	pq_smoothcam		= {"pq_smoothcam", "1", true};
//PROQUAKE ADDITIONS _END

cvar_t	cl_truelightning	= {"cl_truelightning", "0",true};
cvar_t	cl_sbar				= {"cl_sbar", "0", true};
cvar_t	cl_sbar_style		= {"cl_sbar_style", "0", true};
cvar_t	cl_sbar_drawface	= {"cl_sbar_drawface","1",true};
cvar_t	cl_rocket2grenade	= {"cl_r2g", "0"};
cvar_t	cl_levelshots		= {"cl_levelshots", "1",true};//R00k
cvar_t	cl_muzzleflash		= {"cl_muzzleflash", "1",true};
cvar_t	r_glowlg			= {"r_glowlg", "0",true};//R00k
cvar_t	r_powerupglow		= {"r_powerupglow", "1",true};//0=off, 1=colored, 2= white
cvar_t	r_explosiontype		= {"r_explosiontype", "1",true};
cvar_t	r_explosionlight	= {"r_explosionlight", "1",true};
cvar_t	r_rocketlight		= {"r_rocketlight", "1",true};
#ifdef GLQUAKE
cvar_t	r_explosionlightcolor	= {"r_explosionlightcolor", "0"};
cvar_t	r_rocketlightcolor	= {"r_rocketlightcolor", "0"};
#endif
cvar_t	r_rockettrail		= {"r_rockettrail", "3",true};
cvar_t	r_grenadetrail		= {"r_grenadetrail", "1",true};

cvar_t	cl_itembob			= {"cl_itembob", "0",true};
cvar_t	cl_demospeed		= {"cl_demospeed", "1"};
cvar_t	cl_deadbodyfilter	= {"cl_deadbodyfilter", "0",true};
cvar_t	cl_gibfilter		= {"cl_gibfilter", "0",true};
cvar_t	cl_maxfps			= {"cl_maxfps", "75", true, true};
cvar_t	cl_advancedcompletion = {"cl_advancedcompletion", "1"};

cvar_t	scr_nocenterprint	= {"scr_nocenterprint", "0",false};
cvar_t	cl_nomessageprint	= {"cl_nomessageprint", "0",false};//R00k: suppress mm1/2 output. Used for making demos.
cvar_t	chase_transparent	= {"chase_transparent", "1",true};
cvar_t	cl_basespeedkey		= {"cl_basespeedkey", "1",true};

cvar_t	cl_footsteps		= {"cl_footsteps", "0", false};//Dont save this to the config, require the player to force it on so mods like Arcane Dimensions' footsteps take priority.

cvar_t	cl_lightning_zadjust = {"cl_lightning_zadjust", "16", true};

cvar_t	cl_web_download		= {"cl_web_download", "1", true};
cvar_t	cl_web_download_url	= {"cl_web_download_url", "downloads.quake-1.com/", true};
cvar_t	cl_checkForUpdate	= {"cl_checkForUpdate", "1", true};
cvar_t	cl_proquake			= {"cl_proquake", "1", true}; //R00k: ProQuake server connection compatibility.

cvar_t	cl_netfps			= {"cl_netfps", "40", true};//From DarkPlaces, caps the rate the client sends input to the server.

cvar_t	slist_filter_noplayers = {"slist_filter_noplayers", "0", true};
/*
if CL_LOADMAPCFG is 1
	a.> config.cfg is not automatically updated when you close qrack
	b.> config.cfg is re-loaded at every map change, autoexec.cfg is loaded (if present), then {mapname}.cfg is loaded (if present)
	c.> writeconfig config.cfg will update the config.cfg with current settings.
*/
cvar_t	cl_loadmapcfg		= {"cl_loadmapcfg", "0"};

client_static_t		cls;
client_state_t		cl;
// FIXME: put these on hunk?
efrag_t				cl_efrags[MAX_EFRAGS];

entity_t			*cl_entities; //johnfitz -- was a static array, now on hunk
int					cl_max_edicts; //johnfitz -- only changes when new map loads

entity_t			cl_static_entities[MAX_STATIC_ENTITIES];

lightstyle_t		cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t			cl_dlights[MAX_DLIGHTS];

int					cl_numvisedicts;
entity_t			*cl_visedicts[MAX_VISEDICTS];
modelindex_t		cl_modelindex[NUM_MODELINDEX];
char				*cl_modelnames[NUM_MODELINDEX];

tagentity_t			q3player_body, q3player_head;

unsigned CheckModel (char *mdl);


//----------
//Bestweapon
//----------
void Bestweapon_f (void)
{
	int i=1;
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("Use \"BestWeapon X Y Z...\"\n");
		Con_Printf ("where X is the first option and Y the second one\n");
		return;
	}

	for (i=1; i<=Cmd_Argc(); i++)
	{
		switch(atoi(Cmd_Argv(i)))
		{
			case 8:
			if (cl.items & IT_LIGHTNING && cl.stats[STAT_CELLS] > 0) 
			{
				in_impulse = 8;
				i = Cmd_Argc() + 1;
			}
			break;
			case 7:
			if (cl.items & IT_ROCKET_LAUNCHER && cl.stats[STAT_ROCKETS] > 0)
			{
				in_impulse = 7;
				i = Cmd_Argc() + 1;
			}
			break;

			case 6:
			if (cl.items & IT_GRENADE_LAUNCHER && cl.stats[STAT_ROCKETS] > 0)
			{
				in_impulse = 6;
				i = Cmd_Argc() + 1;
			}
			break;
			case 5:
			if (cl.items & IT_SUPER_NAILGUN && cl.stats[STAT_NAILS] > 0)
			{
				in_impulse = 5;
				i = Cmd_Argc() + 1;
			}
			break;
			case 4:
			if (cl.items & IT_NAILGUN && cl.stats[STAT_NAILS] > 0)
			{
				in_impulse = 4;
				i = Cmd_Argc() + 1;
			}
			break;
			case 3:
			if (cl.items & IT_SUPER_SHOTGUN && cl.stats[STAT_SHELLS]> 0)
			{
				in_impulse = 3;
				i = Cmd_Argc() + 1;
			}
			break;
			case 2:
			if (cl.items & IT_SHOTGUN && cl.stats[STAT_SHELLS] > 0)
			{
				in_impulse = 2;
				i = Cmd_Argc() + 1;
			}
			break;
			case 1:
			in_impulse = 1;
			i = Cmd_Argc() + 1;
			break;
		}
	}
}
/*
=====================
CL_ClearState
=====================
*/
#ifdef GLQUAKE
extern int	coronas;
#endif
void CL_ClearState (void)
{
	int	i;

	if (!sv.active) 
		Host_ClearMemory ();

	CL_ClearTEnts ();
	
	// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	SZ_Clear (&cls.message);

	// clear other arrays
	memset (cl_efrags, 0, sizeof(cl_efrags));
	memset (cl_dlights, 0, sizeof(cl_dlights));
	memset (cl_lightstyle, 0, sizeof(cl_lightstyle));

	cl_max_edicts = CLAMP (MIN_EDICTS,(int)max_edicts.value,MAX_EDICTS);
	cl_entities = Hunk_AllocName (cl_max_edicts*sizeof(entity_t), "cl_entities");	//johnfitz

	memset (cl_temp_entities, 0, sizeof(cl_temp_entities));
	memset (&cl_beams, 0, sizeof(cl_beams));
	memset (edicttags,0,sizeof(edicttags));

#ifdef GLQUAKE
	coronas = 0;
#endif
	memset (cl_static_entities, 0, sizeof(cl_static_entities)); 

	// allocate the efrags and chain together into a free list
	cl.free_efrags = cl_efrags;

	for (i = 0; i < MAX_EFRAGS - 1; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i + 1];

	cl.free_efrags[i].entnext = NULL;

	#ifdef GLQUAKE
	if (nehahra)
		SHOWLMP_clear ();
	#endif
}

#ifdef GLQUAKE
// nehahra supported
void Neh_ResetSFX (void)
{
	int	i;

	if (num_sfxorig == 0)
		num_sfxorig = num_sfx;

	num_sfx = num_sfxorig;
	Con_DPrintf (1,"Current SFX: %d\n", num_sfx);

	for (i = num_sfx + 1 ; i < MAX_SFX ; i++)
	{
		Q_strncpyz (known_sfx[i].name, "dfw3t23EWG#@T#@", sizeof(known_sfx[i].name));
		if (known_sfx[i].cache.data)
			Cache_Free (&known_sfx[i].cache);
	}
}
#endif

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
	extern void Movie_Stop (void);
	extern void Movie_StopPlayback (void);
	extern qboolean Movie_IsActive (void);
	extern	void VID_SetWindowText (void);

	// stop sounds (especially looping!)
	S_StopAllSounds (true);
	
	CL_StopPlayback ();
	
	#ifdef GLQUAKE
	//if (nehahra)
		FMOD_Stop_f ();
	#endif

	// We have to shut down webdownloading first
	if( cls.download.web )
	{
		cls.download.disconnect = true;
		return;
	}

	// if running a local server, shut it down
	if (cls.demoplayback)
	{
		CL_StopPlayback ();
	}
	else if (cls.state == ca_connected)
	{
		#if defined(_WIN32)
		if (cls.capturedemo)
			Movie_StopPlayback ();	
		else if (Movie_IsActive())
		{
			Movie_Stop ();		
		}
		#endif

		if (cls.demorecording)
			CL_Stop_f ();

		Con_DPrintf (1,"Sending clc_disconnect\n");
		SZ_Clear (&cls.message);
		MSG_WriteByte (&cls.message, clc_disconnect);
		NET_SendUnreliableMessage (cls.netcon, &cls.message);
		SZ_Clear (&cls.message);
		NET_Close (cls.netcon);

		cls.state = ca_disconnected;
		if (sv.active)
		Host_ShutdownServer (false);
	}

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;
	cl.intermission = 0;// Baker

	#ifdef GLQUAKE
	if (nehahra)
		Neh_ResetSFX ();
	#endif
	VID_SetWindowText();
}

void CL_Disconnect_f (void)
{
	// We have to shut down webdownloading first
	if( cls.download.web )
	{
		cls.download.disconnect = true;
		return;
	}
	
	CL_Disconnect ();

	if (sv.active)
		Host_ShutdownServer (false);
}


/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address to be passed on
=====================
*/
void CL_EstablishConnection (char *host)
{
	extern void CL_KeepaliveMessage (void);

	if (cls.state == ca_dedicated)
		return;

	#ifdef GLQUAKE
	if (nehahra)
		num_sfxorig = num_sfx;
	#endif

	if (cls.demoplayback)
	{
		return;
	}

	CL_Disconnect ();

	cls.netcon = NET_Connect (host);
	
	if (!cls.netcon)
	{
		Con_Printf ("\nsyntax: connect server:port (port is optional)\n");//r00k added
		if (net_hostport != 26000)
			Con_Printf ("\nTry using port 26000\n");//r00k added
		Host_Error ("CL_Connect: connect failed");				
	}

	Con_Printf("%c%cConnected to %s%c\n", 1, 29,host, 31);

	cls.demonum = -1;			// not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;				// need all the signon messages before playing

	CL_KeepaliveMessage();//R00k this fixes a NAT routing connection.
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/

void CL_SignonReply (void)
{
	Con_DPrintf (1,"CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon)
	{
		case 1:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "prespawn");
		break;

		case 2:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("name \"%s\"\n", cl_name.string));

		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("color %i %i\n", ((int)cl_color.value) >> 4, ((int)cl_color.value) & 15));

		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("spawn %s", cls.spawnparms));
		break;

		case 3:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "begin");
		Cache_Report ();		// print remaining memory
		break;

		case 4:
		SCR_EndLoadingPlaque ();	// allow normal screen updates
		if ((cl_autodemo.value == 1) && (!cls.demoplayback))
			Cmd_ExecuteString ("record\n", src_command);
		break;
	}	
}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
int CL_NextDemo (void)
{
	char	str[128];

	if (cls.demonum == -1)
	return 0;		// don't play demos

	SCR_BeginLoadingPlaque ();

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS)
	{
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0])
		{
			Con_Printf ("No demos listed with startdemos\n");
			cls.demonum = -1;

			return 0;
		}
	}

	Q_snprintfz (str, sizeof(str), "playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str);
	cls.demonum++;

	return 1;
}

/*
==============
CL_PrintEntities_f
==============
*/
void CL_PrintEntities_f (void)
{
	entity_t	*ent;
	int			i;

	Con_Printf ("Entities currently not culled:\n");

	for (i = 0, ent = cl_entities ; i < cl.num_entities ; i++, ent++)
	{		
/*R00k: I !really! hate scrolling back up the console just to find a valid entity!
		Con_Printf ("%3i:", i);
		if (!ent->model)
		{
			Con_Printf ("EMPTY\n");
			continue;
		}
*/
		if (!ent->model)//if the entity has no model then the server will not send then ent to the client. *duh!
			continue;

		Con_Printf ("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n", ent->model->name, ent->frame, ent->origin[0], ent->origin[1], ent->origin[2], ent->angles[0], ent->angles[1], ent->angles[2]);
	}
}

dlighttype_t SetDlightColor (float f, dlighttype_t def, qboolean random)
{
	dlighttype_t colors[NUM_DLIGHTTYPES - 4] = {lt_red, lt_blue, lt_redblue, lt_green, lt_yellow, lt_white};

	if ((int) f == 1)
		return lt_red;
	else if ((int) f == 2)
		return lt_blue;
	else if ((int) f == 3) 
		return lt_redblue;
	else if ((int) f == 4)
		return lt_green;
	else if ((int) f == 5)
		return lt_yellow;
	else if ((int) f == 6)
		return lt_white;	
	else if (((int) f == NUM_DLIGHTTYPES - 3) && random)
		return colors[rand() % (NUM_DLIGHTTYPES - 4)];
	else
		return def;
}

/*
===============
CL_AllocDlight
===============
*/
dlight_t *CL_AllocDlight (int key)
{
	int		i;
	dlight_t	*dl;

	// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
		{
			if (dl->key == key)
			{
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.ctime)
		{
			memset (dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}


void CL_NewDlight (int key, vec3_t origin, float radius, float time, int type)
{
	dlight_t	*dl;

	dl = CL_AllocDlight (key);
	VectorCopy (origin, dl->origin);
	dl->radius = radius;
	dl->die = cl.time + time;
	#ifdef GLQUAKE
	dl->type = type;
	#endif
}

/*
===============
CL_DecayLights
===============
*/
void CL_DecayLights (void)
{
	int		i;
	dlight_t	*dl;
	double frametime = fabs (cl.ctime - cl.oldtime);
	
	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time || !dl->radius || !dl->decay)
		continue;

		dl->radius -= frametime * dl->decay;
		if (dl->radius < 0)
		dl->radius = 0;
	}
}

/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
float CL_LerpPoint (void)
{
	float	f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if (!f || cl_nolerp.value || cls.timedemo || sv.active)
	{
		cl.ctime = cl.time = cl.mtime[0];
		return 1;
	}

	if (f > 0.1)
	{	// dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1;
		f = 0.1;
	}

	frac = (cl.ctime - cl.mtime[1]) / f;

	if (frac < 0)
	{
		if (frac < -0.01)
		{
			cl.ctime = cl.time = cl.mtime[1];			
		}
		frac = 0;
	}
	else if (frac > 1)
	{
		if (frac > 1.01)
		{
			cl.ctime = cl.time = cl.mtime[0];
		}
		frac = 1;
	}

	return frac;
}

qboolean Model_isDead (int modelindex, int frame)
{
	if (cl_deadbodyfilter.value == 2)
	{
		if ((modelindex == cl_modelindex[mi_fish] && frame >= 18 && frame <= 38) ||
		    (modelindex == cl_modelindex[mi_dog] && frame >= 8 && frame <= 25) ||
		    (modelindex == cl_modelindex[mi_soldier] && frame >= 8 && frame <= 28) ||
		    (modelindex == cl_modelindex[mi_enforcer] && frame >= 41 && frame <= 65) ||
		    (modelindex == cl_modelindex[mi_knight] && frame >= 76 && frame <= 96) ||
		    (modelindex == cl_modelindex[mi_hknight] && frame >= 42 && frame <= 62) ||
		    (modelindex == cl_modelindex[mi_scrag] && frame >= 46 && frame <= 53) ||
		    (modelindex == cl_modelindex[mi_ogre] && frame >= 112 && frame <= 135) ||
		    (modelindex == cl_modelindex[mi_fiend] && frame >= 45 && frame <= 53) ||
		    (modelindex == cl_modelindex[mi_vore] && frame >= 16 && frame <= 22) ||
		    (modelindex == cl_modelindex[mi_shambler] && frame >= 83 && frame <= 93) ||
		    ((modelindex == cl_modelindex[mi_player] || modelindex == cl_modelindex[mi_md3_player]) && frame >= 41 && frame <= 102))
			return true;
	}
	else
	{
		if ((modelindex == cl_modelindex[mi_fish] && frame == 38) ||
		    (modelindex == cl_modelindex[mi_dog] && (frame == 16 || frame == 25)) ||
		    (modelindex == cl_modelindex[mi_soldier] && (frame == 17 || frame == 28)) ||
		    (modelindex == cl_modelindex[mi_enforcer] && (frame == 54 || frame == 65)) ||
		    (modelindex == cl_modelindex[mi_knight] && (frame == 85 || frame == 96)) ||
		    (modelindex == cl_modelindex[mi_hknight] && (frame == 53 || frame == 62)) ||
		    (modelindex == cl_modelindex[mi_scrag] && frame == 53) ||
		    (modelindex == cl_modelindex[mi_ogre] && (frame == 125 || frame == 135)) ||
		    (modelindex == cl_modelindex[mi_fiend] && frame == 53) ||
		    (modelindex == cl_modelindex[mi_vore] && frame == 22) ||
		    (modelindex == cl_modelindex[mi_shambler] && frame == 93) ||
		    ((modelindex == cl_modelindex[mi_player] || modelindex == cl_modelindex[mi_md3_player]) && (frame == 49 || frame == 60 || frame == 69 || 
			frame == 84 || frame == 93 || frame == 102)))
			return true;
	}

	if (modelindex == cl_modelindex[mi_zombie]) //Zombies always draw flies!
		return true;

	return false;
}

qboolean Model_isHead (int modelindex)
{
	if (modelindex == cl_modelindex[mi_h_dog] || modelindex == cl_modelindex[mi_h_soldier] || 
	    modelindex == cl_modelindex[mi_h_enforcer] || modelindex == cl_modelindex[mi_h_knight] || 
	    modelindex == cl_modelindex[mi_h_hknight] || modelindex == cl_modelindex[mi_h_scrag] || 
	    modelindex == cl_modelindex[mi_h_ogre] || modelindex == cl_modelindex[mi_h_fiend] || 
	    modelindex == cl_modelindex[mi_h_vore] || modelindex == cl_modelindex[mi_h_shambler] || 
	    modelindex == cl_modelindex[mi_h_zombie] || modelindex == cl_modelindex[mi_h_player])
		return true;

	return false;
}

char *nodename = "                 ";

int RecursiveNodePoint (mnode_t *node, vec3_t start, vec3_t end)
{
	float	front, back, frac;
	vec3_t	mid;
	void DrawGLPolyHighlight(glpoly_t *p);
	
loc0:
	if (node->contents < 0)
		return false;		// didn't hit anything
	
// calculate mid point
	if (node->plane->type < 3)
	{
		front = start[node->plane->type] - node->plane->dist;
		back = end[node->plane->type] - node->plane->dist;
	}
	else
	{
		front = DotProduct(start, node->plane->normal) - node->plane->dist;
		back = DotProduct(end, node->plane->normal) - node->plane->dist;
	}

	// optimized recursion
	if ((back < 0) == (front < 0))
	{
		node = node->children[front < 0];
		goto loc0;
	}
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;
	
// go down front side
	if (RecursiveNodePoint(node->children[front < 0], start, mid))
	{		
		return true;	// hit something
	}
	else
	{
		int		i, ds, dt;
		msurface_t	*surf;

		surf = cl.worldmodel->surfaces + node->firstsurface;
		for (i = 0 ; i < node->numsurfaces ; i++, surf++)
		{
			ds = (int)((float)DotProduct (mid, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]);
			dt = (int)((float)DotProduct (mid, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]);

			if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
				continue;
			
			ds -= surf->texturemins[0];
			dt -= surf->texturemins[1];
			
			if (ds > surf->extents[0] || dt > surf->extents[1])
				continue;
	
			if (developer.value)
				DrawGLPolyHighlight(surf->polys);

			nodename = (surf->texinfo->texture->name);
			return true;
		}

	// go down back side		
		return RecursiveNodePoint (node->children[front >= 0], mid, end);
	}
}

void GetNodePoint (vec3_t p, vec3_t	end)
{
	RecursiveNodePoint (cl.worldmodel->nodes, p, end);	
	SCR_CenterPrint (nodename);	
}

void CL_TexturePoint (void)
{	
	float frametime	= fabs(cl.time - cl.oldtime);

	vec3_t	dest, start, forward, right,up;
	trace_t	trace;

	if (frametime > 0.008)
	{	
		AngleVectors (r_refdef.viewangles,  forward, right, up);
		VectorCopy(cl_entities[cl.viewentity].origin, start);

		start[2] +=  16;

		VectorMA (start, r_farclip.value, forward, dest);					
				
		memset (&trace, 0, sizeof(trace_t));
		trace.fraction = 1;
		
		SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, start, dest, &trace);
		
		start[2] += cl.crouch;//the above hullcheck resets our start position :(

		if (trace.fraction != 1)
		{					
			GetNodePoint (start, dest);
		}
	}
}

void CL_Footsteps(entity_t	*ent, int frame)
{
	extern	sfx_t		*cl_sfx_step1;
	extern	sfx_t		*cl_sfx_step2;
	extern	sfx_t		*cl_sfx_step3;
	extern	sfx_t		*cl_sfx_step4;
	extern	sfx_t		*cl_sfx_step5;

	if (!sv.active)
		return;

	if (!cl_sfx_step1 || !cl_sfx_step2 || !cl_sfx_step3 || !cl_sfx_step4 || !cl_sfx_step5)//no sounds precached!!!
		return;

	if (ent->steptime > cl.time)
		return;

	if (!((ent->modelindex == cl_modelindex[mi_player])||(ent->modelindex == cl_modelindex[mi_md3_player])))//players or frikbots only
		return;

	if (TruePointContents(ent->origin) != CONTENTS_EMPTY)// if in water etc.. no sound
		return;

	if (frame == 2 || frame == 5 ||	frame == 7 || frame == 10)
	{
		vec3_t	dest, forward, right,up;
		trace_t	trace;
		
		int	i, e, f;
		entity_t *p;

		AngleVectors (ent->angles, forward, right, up);
		
		VectorMA (ent->origin, -32, up, dest);					
			
		memset (&trace, 0, sizeof(trace_t));

		trace.fraction = 1;

		SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, ent->origin, dest, &trace);

		if (trace.fraction == 1)
		{			
			ent->steptime = cl.time + 0.1;//R00k:(2.010) Since the player model animates at 10 frames per sec, no need to come back here until time + 1/10...
			return;
		}

		f = (rand()%4)+1;

		if (ent == &cl_entities[cl.viewentity])
		{
			e = (int)cl.viewentity;//play my own footsteps locally
		}
		else
		{
			for (i = 0, p = cl_entities ; i < cl.num_entities ; i++, p++)
			{
				if (p == ent)
				{
					e = i;
				}			
			}
		}

		//GetNodePoint(ent->origin, dest);//SOUNDS BASED ON texture: WATER, GRass, metal, dirt, wood, or tile.

		if (f == 1)
			S_StartSound(e, 0, cl_sfx_step1, ent->origin, 0.78f, 1.0f);
		else if (f == 2)
			S_StartSound(e, 0, cl_sfx_step2, ent->origin, 0.78f, 1.0f);
		else if (f == 3)
			S_StartSound(e, 0, cl_sfx_step3, ent->origin, 0.78f, 1.0f);
		else if (f == 4)
			S_StartSound(e, 0, cl_sfx_step4, ent->origin, 0.78f, 1.0f);
		else
			S_StartSound(e, 0, cl_sfx_step5, ent->origin, 0.78f, 1.0f);

		ent->steptime = cl.time + 0.3;
	}
}

extern void QMB_FlyParticles (vec3_t origin, int count);

void CL_FlyEffect (entity_t *ent, vec3_t origin)
{
    int        n;
    int        count;
    int        starttime;

    if (ent->flytime < cl.time)
    {
        starttime = cl.time;
        ent->flytime = cl.time + 60;
    }
    else
    {
        starttime = ent->flytime - 60;
    }

    n = cl.time - starttime;
    if (n < 20)
        count = n * 162 / 20;
    else
    {
        n = ent->flytime - cl.time;
        if (n < 20000)
            count = n * 162 / 20;
        else
            count = 162;
    }

    QMB_FlyParticles (origin, (int)(count*0.1));
}

extern	cvar_t pq_timer; // JPG - need this for CL_RelinkEntities
extern	cvar_t	scr_fov;
extern	cvar_t	cl_gun_fovscale;

/*
===============
CL_RelinkEntities
===============
*/
void CL_RelinkEntities (void)
{
	entity_t	*ent;
	int			i, j;
	float		frac, f, d, bobjrotate;
	vec3_t		delta, oldorg;
	dlight_t	*dl;
	model_t		*model;
	dlighttype_t dimlightcolor;
	vec3_t		fv, rv, uv;
	vec3_t		smokeorg, smokeorg2;
	extern		cvar_t	v_viewheight;
	float		scale;
	extern cvar_t	v_viewheight;
	extern cvar_t	cl_gun_offset;

	extern void QMB_MuzzleFlash (vec3_t org);
	extern void QMB_MuzzleFlashLG (vec3_t org);
	extern int	particle_mode;

	// determine partial update time
	frac = CL_LerpPoint ();

	// JPG - check to see if we need to update the status bar
	if (pq_timer.value && (cl.ctime != cl.oldtime))
		Sbar_Changed();

	cl_numvisedicts = 0;
//
// interpolate local player info
//
	for (i = 0 ; i < 3 ; i++)
		cl.velocity[i] = cl.mvelocity[1][i] + frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);

	if ((cls.demoplayback || (last_angle_time > host_time && !(in_attack.state & 3)) && pq_smoothcam.value)) // JPG - check for last_angle_time for smooth chasecam!
	{
		// interpolate the angles
		for (j=0 ; j<3 ; j++)
		{
			d = cl.mviewangles[0][j] - cl.mviewangles[1][j];

			if (d > 180) 
			{
				d -= 360;
			}
			else
			{
				if (d < -180) 
					d += 360;
			}

			// JPG - I can't set cl.viewangles anymore since that messes up the demorecording.  So instead,
			// I'll set lerpangles (new variable), and view.c will use that instead.
			cl.lerpangles[j] = cl.mviewangles[1][j] + (frac * d);
		}
	}
	else
	{		
		VectorCopy(cl.viewangles, cl.lerpangles);
	}
	
	bobjrotate = anglemod (100 * cl.time);

	// start on the entity after the world
	for (i = 1, ent = cl_entities + 1 ; i < cl.num_entities ; i++, ent++)
	{
		if (!ent->model)
		{	
			// empty slot
			if (ent->forcelink)
			{
				if	(ent->efrag != NULL)
				{
					Con_Printf("Found ent->efrag without a model\n");
					R_RemoveEfrags (ent);	// just became empty
				}
			}
			continue;
		}

		// if the object wasn't included in the last packet, remove it
		if (ent->msgtime != cl.mtime[0])
		{
			ent->model = NULL;

			// clear it's interpolation data
			ent->frame_start_time     = 0;
			ent->translate_start_time = 0;
			ent->rotate_start_time    = 0;

			//MH: and the poses
			ent->pose1 = ent->pose2 = 0;

			continue;
		}

		VectorCopy (ent->origin, oldorg);	

		if (ent->forcelink)
		{	// the entity was not updated in the last message so move to the final spot
			VectorCopy (ent->msg_origins[0], ent->origin);
			VectorCopy (ent->msg_angles[0], ent->angles);			
		}
		else
		{	// if the delta is large, assume a teleport and don't lerp
			f = frac;

			if (f >= 1)
			{
				ent->translate_start_time = 0;
				ent->rotate_start_time = 0;	
			}

			for (j = 0 ; j < 3 ; j++)
			{
				delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];

				if (delta[j] > 100 || delta[j] < -100)// assume a teleportation, not a motion
				{
					ent->origin[j] = ent->msg_origins[0][j];
					ent->angles[j] = ent->msg_angles[0][j];
				}
				else
				{
					ent->origin[j] = ent->msg_origins[1][j] + (f * delta[j]);

					d = ent->msg_angles[0][j] - ent->msg_angles[1][j];

					if (d > 180) 
					{
						d -= 360;
					}
					else
					{
						if (d < -180) 
							d += 360;
					}
					ent->angles[j] = ent->msg_angles[1][j] + (f * d);
				}
			}
		}

		if (cl_deadbodyfilter.value && (ent->model->type == mod_alias || ent->model->type == mod_md3) && Model_isDead(ent->modelindex, ent->frame))
			continue;

		if (cl_gibfilter.value && (ent->model->type == mod_alias || ent->model->type == mod_md3) && 
			(ent->modelindex == cl_modelindex[mi_gib1] || ent->modelindex == cl_modelindex[mi_gib2] || 
		     ent->modelindex == cl_modelindex[mi_gib3] || Model_isHead(ent->modelindex)))
			continue;

		if (ent->modelindex == cl_modelindex[mi_explo1] || ent->modelindex == cl_modelindex[mi_explo2])
		{
			// software removal of sprites
			if (r_explosiontype.value == 2 || r_explosiontype.value == 3)
				continue;

#ifdef GLQUAKE
			if (qmb_initialized && gl_part_explosions.value)
			{
				R_RocketTrail (oldorg, ent->origin, LAVA_TRAIL);//R00k for flamethrower
				if (gl_part_explosions.value == 1)	//Value 1 eliminates the sprite, > 1 shows both.
					continue; 
			}
#endif
		}
/*
		if ((qmb_initialized) && (gl_part_flames.value && !strcmp(ent->model->name, "progs/fball.mdl")))
		{
				R_RocketTrail (oldorg, ent->origin, LAVA_TRAIL);//R00k for Wyver
				continue;
		}
*/
		if (!(model = cl.model_precache[ent->modelindex]))
			Host_Error ("CL_RelinkEntities: bad modelindex");

		if (ent->modelindex == cl_modelindex[mi_rocket] && cl_rocket2grenade.value && cl_modelindex[mi_grenade] != -1)
			ent->model = cl.model_precache[cl_modelindex[mi_grenade]];

		// rotate binary objects locally
		if (ent->model->flags & EF_ROTATE)
		{
			if ((ent->model != cl.model_precache[cl_modelindex[mi_flag]]) && (ent->model != cl.model_precache[cl_modelindex[mi_w_s_key]]) && (ent->model != cl.model_precache[cl_modelindex[mi_w_g_key]])) //R00k dont rotate flags!
			{
				ent->angles[0] = 0;
				ent->angles[1] = bobjrotate;
				ent->angles[2] = 0;

				if (cl_itembob.value)
					ent->origin[2] += sin(bobjrotate / 90 * M_PI) * 5 + 5;
			}
		}

		// EF_BRIGHTFIELD is not used by original progs
		if (ent->effects & EF_BRIGHTFIELD)
		{
			R_EntityParticles (ent);			
		}

		if ((ent->effects & EF_MUZZLEFLASH) && cl_muzzleflash.value)
		{
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->origin[2] += 16;
			AngleVectors (ent->angles, fv, rv, uv);
			VectorMA (dl->origin, 18, fv, dl->origin);
			dl->radius = 200 + (rand() & 31);
			dl->minlight = 32;
			dl->die = cl.time + 0.1;		
			if (ent == &cl_entities[cl.viewentity])
			{
				ent->translate_start_time = 0;
				ent->rotate_start_time = 0;	
			}

			#ifdef GLQUAKE
				if (ent->modelindex == cl_modelindex[mi_shambler] && qmb_initialized && gl_part_lightning.value)
					dl->type = lt_blue;
				else
				{
					if (ent->modelindex == cl_modelindex[mi_scrag] && qmb_initialized)
						dl->type = lt_green;
					else
						dl->type = lt_muzzleflash;
				}

			// check for progs/player.mdl and if cl.stats[STAT_ACTIVEWEAPON] == whatever
			// and change the muzzle flash colour depending on the weapon
			if ((qmb_initialized) && (gl_part_muzzleflash.value))
			{
				if (i == cl.viewentity)
				{
					AngleVectors (r_refdef.viewangles , fv, rv, uv);
					VectorCopy(cl_entities[cl.viewentity].origin, smokeorg);
					
					smokeorg[2] +=  14;//R00k: lowered down CVAR?

					if (!chase_active.value)
					{
						smokeorg[2] += cl.crouch + bound(-7, v_viewheight.value, 4);

						if (cl_gun_offset.value == 2)//Right-Handed
						{
							VectorMA (smokeorg, 2, rv, smokeorg);	
						}
						else if (cl_gun_offset.value == 1)//Left-Handed
						{
							VectorMA (smokeorg, -2, rv, smokeorg);	
						}
					}
					
					scale = 1.0f;

					if ((cl_gun_fovscale.value) && (scr_fov.value != 0))
					{
						scale = 1.0f / tan( DEG2RAD(scr_fov.value/2));
					}

					VectorMA (smokeorg, (18*scale), fv, smokeorg);

					if (r_drawviewmodel.value > 0)
					{						
						if ((cl.stats[STAT_ACTIVEWEAPON] == IT_NAILGUN)||(cl.stats[STAT_ACTIVEWEAPON] == IT_SUPER_SHOTGUN))
						{
								VectorMA (smokeorg, -3, rv, smokeorg);
								QMB_MuzzleFlash (smokeorg);					
								VectorMA (smokeorg, 6, rv, smokeorg2);
								QMB_MuzzleFlash (smokeorg2);				
						}
						else
						{
							if ((cl.stats[STAT_ACTIVEWEAPON] == IT_SUPER_NAILGUN)||(cl.stats[STAT_ACTIVEWEAPON] == IT_SHOTGUN) || (cl.stats[STAT_ACTIVEWEAPON] == IT_ROCKET_LAUNCHER))
							{
								QMB_MuzzleFlash (smokeorg);
							}
							else
							{
								if (cl.stats[STAT_ACTIVEWEAPON] == IT_LIGHTNING)
								{
									QMB_MuzzleFlashLG(smokeorg);
								}						
							}
						}					
					}
				}
				else
				{
					if ((ent->modelindex == cl_modelindex[mi_player])||(ent->modelindex == cl_modelindex[mi_md3_player]))
					{
						VectorCopy (ent->origin, smokeorg);
						smokeorg[2] += 10;
						AngleVectors (ent->angles, fv, rv, uv);
						VectorMA (smokeorg, 32, fv, smokeorg);
						VectorMA (smokeorg, 12, rv, smokeorg);
						QMB_MuzzleFlash (smokeorg);
					}
				}
			}
			#endif
		}

		if (r_powerupglow.value)
		{
			dimlightcolor = lt_default;

			if (r_powerupglow.value == 2)
			{
				dimlightcolor = SetDlightColor(lt_white, lt_default, false);//Set glow white
			}

			if ((ent->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED))
			{
				if (r_powerupglow.value == 1)
				{
					CL_NewDlight (i, ent->origin, 200, 0.1, lt_redblue);
					CL_NewDlight (i+1, ent->origin, 80, 0.1, dimlightcolor);
				}
				else
					CL_NewDlight (i, ent->origin, 200, 0.1, dimlightcolor);
			}
			else
			{
				if (ent->effects & EF_BLUE)
				{
					if (r_powerupglow.value == 1)
					{
						CL_NewDlight (i, ent->origin, 200, 0.1, lt_blue);
						CL_NewDlight (i+1, ent->origin, 80, 0.1, dimlightcolor);
					}
					else
					CL_NewDlight (i, ent->origin, 200, 0.1, dimlightcolor);
				}			
				else 
				{
					if (ent->effects & EF_RED)
					{
						if (r_powerupglow.value == 1)
						{
							CL_NewDlight (i, ent->origin, 200, 0.1, lt_red);
							CL_NewDlight (i+1, ent->origin, 80, 0.1, dimlightcolor);
						}
						else
							CL_NewDlight (i, ent->origin, 200, 0.1, dimlightcolor);
					}					
					else
					{					
/*						if (ent->effects & EF_GREEN)
						{
							if (r_powerupglow.value == 1)
							{
								CL_NewDlight (i, ent->origin, 200, 0.1, lt_green);
								CL_NewDlight (i+1, ent->origin, 80, 0.1, dimlightcolor);
							}
							else
							CL_NewDlight (i, ent->origin, 200, 0.1, dimlightcolor);
						}			
						else 
						{
							if ((ent->effects & EF_YELLOW)||(ent->effects & EF_BRIGHTFIELD))
							{
								if (r_powerupglow.value == 1)
								{
									CL_NewDlight (i, ent->origin, 200, 0.1, lt_yellow);
									CL_NewDlight (i+1, ent->origin, 80, 0.1, dimlightcolor);
								}
								else
									CL_NewDlight (i, ent->origin, 200, 0.1, dimlightcolor);
							}										//
							else
							{*/
								if (ent->effects & EF_BRIGHTLIGHT)
								{
									vec3_t	tmp;
									VectorCopy (ent->origin, tmp);
									tmp[2] += 16;
									CL_NewDlight (i, tmp, 400 + (rand() & 31), 0.1, dimlightcolor);
								}
								else
								{			
									if (ent->effects & EF_DIMLIGHT)
									{
										vec3_t	tmp;
										VectorCopy (ent->origin, tmp);
										tmp[2] += 16;
										CL_NewDlight (i, tmp, 200 + (rand() & 31), 0.1, dimlightcolor);
									}							
								}
//							}
//						}
					}
				}
			}
		}

		if (!strcmp(ent->model->name, "progs/s_light.spr"))//R00k: Fire Ball! SPEEEDBALL! ;)
		{
		  if (qmb_initialized && gl_part_trails.value)
				R_RocketTrail (oldorg, ent->origin, LAVA_TRAIL);
		}
		
		if (!strcmp(ent->model->name, "progs/flame2.mdl"))			
		{	
			#ifdef GLQUAKE
			if (qmb_initialized && gl_part_flames.value)
			{
				//QMB_BigTorchFlame (ent->origin);					
				if (qmb_initialized && gl_part_trails.value)
					R_RocketTrail (oldorg, ent->origin, LAVA_TRAIL);				
			}
			#endif
		}

		//R00k added for nailtrails
		if (ent->model->modhint == MOD_SPIKE)//v1.92 MD3 supported
		{	
			#ifdef GLQUAKE
			if (qmb_initialized && gl_part_trails.value)
			{
				R_RocketTrail (oldorg, ent->origin, NAIL_TRAIL);
			}
			#endif
		}
								
		if (cl_footsteps.value)
			CL_Footsteps(ent,ent->frame);

		if ((Model_isDead(ent->modelindex, ent->frame)))
		{	
			if (qmb_initialized && gl_part_flies.value)
			{
				if (!(ISUNDERWATER(TruePointContents(ent->origin))))
					CL_FlyEffect (ent, ent->origin);
			}
		}

		if (model->flags)
		{
			if (model->flags & EF_GIB)
			R_RocketTrail (oldorg, ent->origin, BLOOD_TRAIL);
			else if (model->flags & EF_ZOMGIB)
			R_RocketTrail (oldorg, ent->origin, SLIGHT_BLOOD_TRAIL);
			else if (model->flags & EF_TRACER)
			R_RocketTrail (oldorg, ent->origin, TRACER1_TRAIL);
			else if (model->flags & EF_TRACER2)
			R_RocketTrail (oldorg, ent->origin, TRACER2_TRAIL);
			else if (model->flags & EF_ROCKET)
			{
				if (model->modhint == MOD_LAVABALL)
				{
					R_RocketTrail (oldorg, ent->origin, LAVA_TRAIL);

					dl = CL_AllocDlight (i);
					VectorCopy (ent->origin, dl->origin);
					dl->radius = 100 * (1 + bound(0, r_rocketlight.value, 1));
					dl->die = cl.time + 0.1;
					#ifdef GLQUAKE
					dl->type = lt_rocket;
					#endif
				}
				else
				{
					if (r_rockettrail.value == 2)
					R_RocketTrail (oldorg, ent->origin, GRENADE_TRAIL);
					else if (r_rockettrail.value == 3)
					R_RocketTrail (oldorg, ent->origin, ALT_ROCKET_TRAIL);
					else if (r_rockettrail.value)
					R_RocketTrail (oldorg, ent->origin, ROCKET_TRAIL);

					if (r_rocketlight.value)
					{
						dl = CL_AllocDlight (i);
						VectorCopy (ent->origin, dl->origin);
						dl->radius = 100 * (1 + bound(0, r_rocketlight.value, 1));
						dl->die = cl.time + 0.1;
						#ifdef GLQUAKE
						dl->type = SetDlightColor (r_rocketlightcolor.value, lt_rocket, false);
						#endif
					}
				}
			}
			else if ((model->flags & EF_GRENADE) && r_grenadetrail.value)
			{
				// Nehahra dem compatibility
				if (ent->transparency == -1)
				{
					if (cl.time >= ent->smokepuff_time)
					{
						R_RocketTrail (oldorg, ent->origin, NEHAHRA_SMOKE);
						ent->smokepuff_time = cl.time + 0.14;
					}
				}
				else
				{
					R_RocketTrail (oldorg, ent->origin, GRENADE_TRAIL);
				}
			}
			else 
			if (model->flags & EF_TRACER3)
				R_RocketTrail (oldorg, ent->origin, VOOR_TRAIL);			
		}

		ent->forcelink = false;

		if (i == cl.viewentity && !chase_active.value)
			continue;

		// nehahra supported
		if (ent->effects & EF_NODRAW)
			continue;

		#ifdef GLQUAKE
		if (qmb_initialized)
		{
			if (ent->modelindex == cl_modelindex[mi_bubble])
			{
				if ((!cl.paused && cl.oldtime != cl.time)&&(particle_mode))
				{
					QMB_StaticBubble (ent);
					continue;
				}
			}
			else if (gl_part_lightning.value && ent->modelindex == cl_modelindex[mi_shambler] &&
			ent->frame >= 65 && ent->frame <= 68)
			{
				vec3_t	liteorg;

				VectorCopy(ent->origin, liteorg);
				liteorg[2] += 32;
				QMB_ShamblerCharge (liteorg);
			}						
		}
		#endif

		if (cl_numvisedicts < MAX_VISEDICTS)
		{
			cl_visedicts[cl_numvisedicts++] = ent;
		}
	}
};

/*
===============
CL_CalcCrouch

Smooth out stair step ups
===============
*/
void CL_CalcCrouch (void)
{
	qboolean	teleported;
	entity_t	*ent;
	static	vec3_t	oldorigin = {0, 0, 0};
	static	float	oldz = 0, extracrouch = 0, crouchspeed = 100;
	
	ent = &cl_entities[cl.viewentity];

	if (!ent)//Seems we need this here, because connecting to a server, from the server browser menu, before/without previously loading a map, would crash the engine on the 'teleported =' line of code.
		return;//this is a total hack, until i find why entity_t hasnt been enabled.

	teleported = !VectorL2Compare(ent->origin, oldorigin, 48);
	VectorCopy(ent->origin, oldorigin);

	if (teleported)
	{
		// possibly teleported or respawned
		oldz = ent->origin[2];
		extracrouch = 0;
		crouchspeed = 100;
		cl.crouch = 0;
		return;
	}

	if (cl.onground && ent->origin[2] - oldz > 0)
	{
		if (ent->origin[2] - oldz > 20)
		{
			// if on steep stairs, increase speed
			if (crouchspeed < 160)
			{
				extracrouch = ent->origin[2] - oldz - host_frametime * 200 - 15;
				extracrouch = min(extracrouch, 5);
			}
			crouchspeed = 160;
		}

		oldz += host_frametime * crouchspeed;

		if (oldz > ent->origin[2])
			oldz = ent->origin[2];

		if (ent->origin[2] - oldz > 15 + extracrouch)
			oldz = ent->origin[2] - 15 - extracrouch;

		extracrouch -= host_frametime * 200;
		extracrouch = max(extracrouch, 0);

		cl.crouch = oldz - ent->origin[2];
	}
	else
	{
		// in air or moving down
		oldz = ent->origin[2];
		cl.crouch += host_frametime * 150;

		if (cl.crouch > 0)
			cl.crouch = 0;

		crouchspeed = 100;
		extracrouch = 0;
	}
}

#ifdef GLQUAKE
void CL_CopyPlayerInfo (entity_t *ent, entity_t *player)
{
	memcpy (&ent->baseline, &player->baseline, sizeof(entity_state_t));

	ent->msgtime = player->msgtime;
	memcpy (ent->msg_origins, player->msg_origins, sizeof(ent->msg_origins));
	VectorCopy (player->origin, ent->origin);
	memcpy (ent->msg_angles, player->msg_angles, sizeof(ent->msg_angles));
	VectorCopy (player->angles, ent->angles);

	ent->model = (ent == &q3player_body.ent) ? cl.model_precache[cl_modelindex[mi_q3torso]] : cl.model_precache[cl_modelindex[mi_q3head]];
	ent->efrag = player->efrag;
	ent->shirtcolor = player->shirtcolor;
	ent->pantscolor = player->pantscolor;
	ent->frame = player->frame;
	ent->syncbase = player->syncbase;
	ent->colormap = player->colormap;
	ent->effects = player->effects;
	ent->skinnum = player->skinnum;
	ent->visframe = player->visframe;
//	ent->dlightframe = player->dlightframe;
//	ent->dlightbits = player->dlightbits;

	ent->trivial_accept = player->trivial_accept;
	ent->topnode = player->topnode;

	ent->modelindex = (ent == &q3player_body.ent) ? cl_modelindex[mi_q3torso] : cl_modelindex[mi_q3head];

	ent->noshadow = player->noshadow;

	ent->transparency = player->transparency;
	ent->smokepuff_time = player->smokepuff_time;
}
#endif

/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
int CL_ReadFromServer (void)
{
	int	ret;

	cl.oldtime = cl.ctime;
	cl.time += host_frametime;

	if (!cl_demorewind.value || !cls.demoplayback)	// by joe
	{
		cl.ctime += host_frametime;
	}
	else
	{
		cl.ctime -= host_frametime;
	}

	do 
	{
		ret = CL_GetMessage ();

		if (ret == -1)
			Host_Error ("CL_ReadFromServer: lost server connection");

		if (!ret)
			break;
		
		cl.last_received_message = realtime;
		CL_ParseServerMessage ();
	} 
	while (ret && cls.state == ca_connected);

	if (cl_shownet.value)
		Con_Printf ("\n");
	
	if (cls.state != ca_connected)
		Host_Error ("CL_ReadFromServer: lost server connection");
	CL_RelinkEntities ();
	CL_CalcCrouch ();
	CL_UpdateTEnts ();

	// bring the links up to date
	return 0;
}

/*
=================
CL_SendCmd
=================
*/

void CL_SendCmd (void)
{
	usercmd_t	cmd;
	
	if (cls.state != ca_connected)
	{
		return;
	}

	if (cls.signon == SIGNONS)
	{		
		CL_BaseMove (&cmd);		

		// allow mice or other external controllers to add to the move
		IN_Move (&cmd);

		CL_SendMove (&cmd);		
	}

	if (cls.demoplayback)
	{
		SZ_Clear (&cls.message);
		return;
	}

	// send the reliable message
	if (!cls.message.cursize)
		return;	// no message at all

	if (!NET_CanSendMessage(cls.netcon))
	{
		Con_DPrintf (1,"CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage(cls.netcon, &cls.message) == -1)
		Host_Error ("CL_WriteToServer: lost server connection");
	
	SZ_Clear (&cls.message);
}

#ifdef GLQUAKE
void CL_Fog_f (void) 
{
	extern cvar_t gl_fogred, gl_foggreen, gl_fogblue, gl_fogenable;
	if (Cmd_Argc () == 1) {
		Con_Printf ("\"fog\" is \"%f %f %f %f\"\n", gl_fogdensity.value, gl_fogred.value, gl_foggreen.value, gl_fogblue.value);
		return;
	}
	gl_fogenable.value = 1;
	gl_fogdensity.value = atof(Cmd_Argv(1));//fitzquake support
	gl_fogred.value     = atof(Cmd_Argv(2));
	gl_foggreen.value   = atof(Cmd_Argv(3));
	gl_fogblue.value    = atof(Cmd_Argv(4));
}
#endif

/*
===============
CL_CheckForUpdate

retrieve a file with the last version number on a web server, compare with current version
display a message box in case the user need to update
===============
*/

#define CHECKUPDATE_URL "http://www.quakeone.com/qrack/"
#define CHECKUPDATE_FILE "qrack_version.txt"

static void CL_CheckForUpdate( void )
{
	char url[1024];
	qboolean success;
	float local_version, net_version;
	FILE *f;
	extern int Web_Get( const char *url, const char *referer, const char *name, int resume, int max_downloading_time, int timeout, int ( *_progress )(double) );
	extern void M_Menu_Update_f (void);

	if (!cl_checkForUpdate.value)
		return;

	// step one get the last version file
	Con_DPrintf (1, "Checking for Qrack update.\n" );

	Q_snprintfz( url, sizeof( url ), "%s%s", CHECKUPDATE_URL, CHECKUPDATE_FILE );
	success = Web_Get( url, NULL, CHECKUPDATE_FILE, false, 2, 2, NULL );

	if( !success )
		return;

	// got the file
	// this looks stupid but is the safe way to do it
	local_version = atof(QRACK_VERSION);

	f = fopen( CHECKUPDATE_FILE, "r" );

	if( f == NULL )
	{
		Con_Printf( "Fail to open last version file.\n" );
		return;
	}

	if( fscanf_s( f, "%f", &net_version ) != 1 )
	{
		// error
		fclose( f );
		Con_Printf( "Fail to parse last version file.\n" );
		return;
	}

	// we have the version
	if( net_version > local_version )
	{
		char net_version_str[16], *s;
//		char my_version_str[16];

		Q_snprintfz( net_version_str, sizeof( net_version_str ), "%4.3f", net_version );
		s = net_version_str + strlen( net_version_str ) - 1;
		while( *s == '0' ) s--;
		net_version_str[s-net_version_str+1] = '\0';

		M_Menu_Update_f();
		// you should update
		Con_Printf( "\n\nQrack version %s is available.\nhttp://www.quakeone.com/qrack\n\n", net_version_str );
	}
	else if( net_version == local_version )
	{
		Con_Printf( "Your Qrack version is up-to-date.\n");
	}

	fclose( f );

	// cleanup
	remove( CHECKUPDATE_FILE );
}

qboolean CL_CullCheck (vec3_t checkpoint, int viewcontents)//Modified from MHQuake
{
	int i;
	vec3_t mins;
	vec3_t maxs;

	// check against world model
	if ((Mod_PointInLeaf (checkpoint, cl.worldmodel))->contents != viewcontents)
		return false;

	for (i = 0; i < cl_numvisedicts; i++)
	{
		// retrieve the current entity
		entity_t *e = cl_visedicts[i];

		// don't check against self
		if (e == &cl_entities[cl.viewentity]) continue;
		
		if ((!e->modelindex)||(!e->model)) continue;

		if (((e->model->type == mod_alias)||(e->model->type == mod_md3)) && (e->model->numframes < 2))//dont clip on item entities
			continue;

		// derive the bbox
		if (e->model->type == mod_brush && (e->angles[0] || e->angles[1] || e->angles[2]))
		{
			mins[0] = e->origin[0] - e->model->radius;
			maxs[0] = e->origin[0] + e->model->radius;
			mins[1] = e->origin[1] - e->model->radius;
			maxs[1] = e->origin[1] + e->model->radius;
			mins[2] = e->origin[2] - e->model->radius;
			maxs[2] = e->origin[2] + e->model->radius;
		}
		else if ((e->model->type == mod_alias)||(e->model->type == mod_md3))
		{
			aliashdr_t *hdr = (aliashdr_t *)Mod_Extradata (e->model);

			// use per-frame bbox clip tests
			VectorAdd (e->origin, hdr->frames[e->frame].bboxmin.v, mins);
			VectorAdd (e->origin, hdr->frames[e->frame].bboxmax.v, maxs);
		}
		else
		{
			VectorAdd (e->origin, e->model->mins, mins);
			VectorAdd (e->origin, e->model->maxs, maxs);
		}

		// check against bbox
		if (checkpoint[0] < mins[0]) continue;
		if (checkpoint[1] < mins[1]) continue;
		if (checkpoint[2] < mins[2]) continue;
		if (checkpoint[0] > maxs[0]) continue;
		if (checkpoint[1] > maxs[1]) continue;
		if (checkpoint[2] > maxs[2]) continue;

		// point inside
		return false;
	}

	// it's good now
	return true;
}

/*
-------------------------------------------------------------------------
	CL_Clip_Test	-- modified code from MHQuake.
-------------------------------------------------------------------------
 Returns TRUE if a trace is clipped from our pov to the entity origin.
-------------------------------------------------------------------------
*/

qboolean CL_Clip_Test(vec3_t org)
{
	int viewcontents = (Mod_PointInLeaf (r_refdef.vieworg, cl.worldmodel))->contents;
	int best,test;
	int num_tests = max(360,(VectorDistance(r_refdef.vieworg, org)));

	int dest_vert[] = {0, 0, 1, 1, 2, 2};
	int dest_offset[] = {4, -4};
	vec3_t testorg;

	for (best = 0; best < num_tests; best++)
	{
		testorg[0] = r_refdef.vieworg[0] + (org[0] - r_refdef.vieworg[0]) * best / num_tests;
		testorg[1] = r_refdef.vieworg[1] + (org[1] - r_refdef.vieworg[1]) * best / num_tests;
		testorg[2] = r_refdef.vieworg[2] + (org[2] - r_refdef.vieworg[2]) * best / num_tests;

		// check for a leaf hit with different contents
		if (!CL_CullCheck (testorg, viewcontents))
		{
			// go back to the previous best as this one is bad
			if (best > 1)
				best--;
			else best = num_tests;
			break;
		}
	}

	// move along path from chase_dest to r_refdef.vieworg
	for (; best >= 0; best--)
	{
		// number of matches
		int nummatches = 0;

		// adjust
		testorg[0] = r_refdef.vieworg[0] + (org[0] - r_refdef.vieworg[0]) * best / num_tests;
		testorg[1] = r_refdef.vieworg[1] + (org[1] - r_refdef.vieworg[1]) * best / num_tests;
		testorg[2] = r_refdef.vieworg[2] + (org[2] - r_refdef.vieworg[2]) * best / num_tests;

		// run 6 tests: -x/+x/-y/+y/-z/+z
		for (test = 0; test < 6; test++)
		{
			// adjust, test and put back.
			testorg[dest_vert[test]] -= dest_offset[test & 1];
			if (!(CL_CullCheck(testorg, viewcontents)))
				return true;
			testorg[dest_vert[test]] += dest_offset[test & 1];
		}

		// test result, if all match we're done in here
		if (nummatches == 6) break;
	}
	return false;
}

extern cvar_t sys_highpriority;
/*
=================
CL_Init
=================
*/
void CL_Init (void)
{
	SZ_Alloc (&cls.message, NET_MAXMESSAGE);

	CL_InitInput ();
	CL_InitTEnts ();

	// register our commands
	Cvar_RegisterVariable (&cl_name);
	Cvar_RegisterVariable (&cl_color);
	Cvar_RegisterVariable (&cl_upspeed);
	Cvar_RegisterVariable (&cl_forwardspeed);
	Cvar_RegisterVariable (&cl_backspeed);
	Cvar_RegisterVariable (&cl_sidespeed);
	Cvar_RegisterVariable (&cl_movespeedkey);
	Cvar_RegisterVariable (&cl_yawspeed);
	Cvar_RegisterVariable (&cl_pitchspeed);
	Cvar_RegisterVariable (&cl_anglespeedkey);
	Cvar_RegisterVariable (&cl_shownet);
	Cvar_RegisterVariable (&cl_nolerp);
	Cvar_RegisterVariable (&lookspring);
	Cvar_RegisterVariable (&lookstrafe);
	Cvar_RegisterVariable (&sensitivity);

	Cvar_RegisterVariable (&m_pitch);
	Cvar_RegisterVariable (&m_yaw);
	Cvar_RegisterVariable (&m_forward);
	Cvar_RegisterVariable (&m_side);

	Cvar_RegisterVariable (&cl_truelightning);
	Cvar_RegisterVariable (&cl_sbar);
	Cvar_RegisterVariable (&cl_sbar_style);
	Cvar_RegisterVariable (&cl_sbar_drawface);
	Cvar_RegisterVariable (&cl_rocket2grenade);
	Cvar_RegisterVariable (&cl_demorewind);
	Cvar_RegisterVariable (&cl_muzzleflash);
	Cvar_RegisterVariable (&r_glowlg);
	Cvar_RegisterVariable (&r_powerupglow);
	Cvar_RegisterVariable (&r_explosiontype);
	Cvar_RegisterVariable (&r_explosionlight);
	Cvar_RegisterVariable (&r_rocketlight);
	#ifdef GLQUAKE
	Cvar_RegisterVariable (&r_explosionlightcolor);
	Cvar_RegisterVariable (&r_rocketlightcolor);
	#endif
	Cvar_RegisterVariable (&r_rockettrail);
	Cvar_RegisterVariable (&r_grenadetrail);

	Cvar_RegisterVariable (&cl_itembob);
	Cvar_RegisterVariable (&cl_demospeed);
	Cvar_RegisterVariable (&cl_deadbodyfilter);
	Cvar_RegisterVariable (&cl_gibfilter);
	Cvar_RegisterVariable (&cl_maxfps);
	Cvar_RegisterVariable (&cl_advancedcompletion);
	Cvar_RegisterVariable (&scr_nocenterprint);
	Cvar_RegisterVariable (&cl_nomessageprint);
	Cvar_RegisterVariable (&chase_transparent);//R00k
	Cvar_RegisterVariable (&cl_basespeedkey);
    Cmd_AddCommand ("bestweapon", Bestweapon_f);

	// JPG - added these for %r formatting
	Cvar_RegisterVariable (&pq_needrl);
	Cvar_RegisterVariable (&pq_haverl);
	Cvar_RegisterVariable (&pq_needrox);

	// JPG - added these for %p formatting
	Cvar_RegisterVariable (&pq_quad);
	Cvar_RegisterVariable (&pq_pent);
	Cvar_RegisterVariable (&pq_ring);

	// R00k - added for %a formatting
	Cvar_RegisterVariable (&pq_armor);

	// JPG 3.00 - %w formatting
	Cvar_RegisterVariable (&pq_weapons);
	Cvar_RegisterVariable (&pq_noweapons);

	// JPG 1.05 - added this for +jump -> +moveup translation
	Cvar_RegisterVariable (&pq_moveup);

	// JPG 3.02 - added this by request
	Cvar_RegisterVariable (&pq_smoothcam);	
	
	Cmd_AddCommand ("entities", CL_PrintEntities_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);

	Cmd_AddCommand ("loc_deletecurrent", LOC_DeleteCurrent_f);
	Cmd_AddCommand ("loc_clear", LOC_Clear_f);
	Cmd_AddCommand ("loc_reload", LOC_LoadLocations);
	Cmd_AddCommand ("loc_renamecurrent", LOC_RenameCurrent_f);
	Cmd_AddCommand ("loc_save", LOC_Save_f);
	Cmd_AddCommand ("loc_startpoint", LOC_StartPoint_f);
	Cmd_AddCommand ("loc_endpoint", LOC_EndPoint_f);
	
	Cvar_RegisterVariable (&pingplreport);
	Cvar_RegisterVariable (&cl_footsteps);	
	Cvar_RegisterVariable (&cl_loadmapcfg);
	Cvar_RegisterVariable (&cl_lightning_zadjust);
	Cvar_RegisterVariable (&cl_web_download);
	Cvar_RegisterVariable (&cl_web_download_url);
	Cvar_RegisterVariable (&cl_checkForUpdate);
	Cvar_RegisterVariable (&cl_levelshots);
	Cvar_RegisterVariable (&cl_echo_qversion);
	Cvar_RegisterVariable (&cl_autofocus);
	Cvar_RegisterVariable (&cl_proquake);//R00k: imitate a proquake client on a proquake server for precise angle support
	Cvar_RegisterVariable (&sys_highpriority);
	Cvar_RegisterVariable (&slist_filter_noplayers);
	Cvar_RegisterVariable (&cl_netfps);//From DarkPlaces

	#ifdef GLQUAKE
	Cmd_AddCommand ("fog",CL_Fog_f);//R00k
	#endif

	CL_CheckForUpdate();
}
