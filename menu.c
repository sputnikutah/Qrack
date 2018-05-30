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
// menu.c

#include "quakedef.h"
#include "winquake.h"

qboolean vid_windowedmouse = true;

void (*vid_menucmdfn)(void); //johnfitz
void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

enum
{
		m_none, 
		m_main, 
		m_singleplayer, 
		m_load, 
		m_save, 
		m_multiplayer,
		m_setup, 
		m_namemaker,//JQ 1.5dev
		m_net, 
		m_options, 
		m_video,
		m_videooptions, 
#ifdef GLQUAKE
		m_particles,
#endif
		m_keys, 
		m_maps, 
		m_demos, 
		m_help,
		m_quit,
		m_update,
		/*
		m_serialconfig, 
		m_modemconfig,
		*/
		m_lanconfig, 
		m_gameoptions, 
		m_search, 
		m_servers, 
		m_slist, 
		m_sedit,
		m_stest,
} m_state;

void M_Menu_Main_f (void);
void M_Menu_SinglePlayer_f (void);
void M_Menu_Load_f (void);
void M_Menu_Save_f (void);
void M_Menu_Help_f (void);
void M_Menu_MultiPlayer_f (void);
void M_Menu_ServerList_f (void);//QuakeForge	
void M_Menu_SEdit_f (void);		//QuakeForge	
void M_ServerList_Test_f (void);//R00k
void M_Menu_Setup_f (void);
void M_Menu_NameMaker_f (void);//JQ1.5dev
void M_Menu_Net_f (void);
void M_Menu_Options_f (void);
void M_Menu_Keys_f (void);
void M_Menu_Video_f (void);
void M_Menu_VideoOptions_f (void);
#ifdef GLQUAKE
void M_Menu_Particles_f (void);
#endif

void M_Menu_Maps_f (void);
void M_Menu_Demos_f (void);
void M_Menu_Quit_f (void);
//void M_Menu_SerialConfig_f (void);
//void M_Menu_ModemConfig_f (void);
void M_Menu_ServerList_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_Search_f (void);
void M_Menu_FoundServers_f (void);

void M_Main_Draw (void);
void M_SinglePlayer_Draw (void);
void M_Load_Draw (void);
void M_Save_Draw (void);
void M_Help_Draw (void);
void M_MultiPlayer_Draw (void);
void M_ServerList_Draw (void);
void M_SEdit_Draw (void);
void M_STest_Draw (void);
void M_Setup_Draw (void);
void M_NameMaker_Draw (void);//JQ1.5Dev
void M_Net_Draw (void);
void M_Options_Draw (void);
void M_Keys_Draw (void);
//void M_VideoModes_Draw (void);
void M_Video_Draw (void);
void M_VideoOptions_Draw (void);
#ifdef GLQUAKE
void M_Particles_Draw (void);
#endif

void M_Maps_Draw (void);
void M_Demos_Draw (void);
void M_Quit_Draw (void);
//void M_SerialConfig_Draw (void);
//	void M_ModemConfig_Draw (void);
void M_LanConfig_Draw (void);
void M_GameOptions_Draw (void);
void M_Search_Draw (void);
void M_ServerList_Draw (void);
void M_FoundServers_Draw (void);

void M_Main_Key (int key);
void M_SinglePlayer_Key (int key);
void M_Load_Key (int key);
void M_Save_Key (int key);
void M_Help_Key (int key);
void M_MultiPlayer_Key (int key);
void M_ServerList_Key (int key);
void M_SEdit_Key (int key);
void M_STest_Key (int key);
void M_Setup_Key (int key);
void M_NameMaker_Key (int key);
void M_Net_Key (int key);
void M_Options_Key (int key);
void M_Keys_Key (int key);
//void M_VideoModes_Key (int key);
void M_Video_Key (int key);
void M_VideoOptions_Key (int key);
#ifdef GLQUAKE
void M_Particles_Key (int key);
#endif

void M_Maps_Key (int key);
void M_Demos_Key (int key);
void M_Quit_Key (int key);
//void M_SerialConfig_Key (int key);
//	void M_ModemConfig_Key (int key);
void M_LanConfig_Key (int key);
void M_GameOptions_Key (int key);
void M_Search_Key (int key);
void M_ServerList_Key (int key);
void M_FoundServers_Key (int key);

qboolean	m_entersound;		// play after drawing a frame, so caching won't disrupt the sound
qboolean	m_recursiveDraw;
int			m_return_state;
qboolean	m_return_onerror;
char		m_return_reason[32];

#define StartingGame	(m_multiplayer_cursor == 2)
#define JoiningGame	(m_multiplayer_cursor == 1)
//#define SerialConfig	(m_net_cursor == 0)
//#define DirectConfig	(m_net_cursor == 1)
#define	IPXConfig	(m_net_cursor == 2)
#define	TCPIPConfig	(m_net_cursor == 3)

void M_ConfigureNetSubsystem(void);

#ifdef GLQUAKE
cvar_t	scr_scalemenu = {"scr_scalemenu", "1"};

int	menuwidth = 320;	
int	menuheight = 240;

#else
#define	menuwidth	vid.width
#define	menuheight	vid.height
#endif

cvar_t	scr_centermenu = {"scr_centermenu", "1"};
int	m_yofs = 0;

extern cvar_t	gl_bounceparticles;
extern cvar_t	gl_flametype;
extern cvar_t	gl_decal_blood;
extern cvar_t	gl_decaltime;
extern cvar_t	gl_decal_bullets;
extern cvar_t	gl_decal_sparks;
extern cvar_t	gl_decal_explosions;
extern cvar_t	gl_decal_viewdistance;
extern cvar_t	pq_smoothcam;
extern cvar_t	gl_rain;
extern cvar_t	scr_fov;
/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
void M_DrawCharacter (int cx, int line, int num)
{
	Draw_Character (cx + ((menuwidth - 320) >> 1), line + m_yofs, num, false);
}

void M_Print (int cx, int cy, char *str)
{
	Draw_Alt_String (cx + ((menuwidth - 320) >> 1), cy + m_yofs, str, false);
}

void M_PrintWhite (int cx, int cy, char *str)
{
	Draw_String (cx + ((menuwidth - 320) >> 1), cy + m_yofs, str, true);
}

void M_PrintScale (int cx, int cy, char *str, int scale )
{
	Draw_String (cx + ((menuwidth - 320) >> 1), cy + m_yofs, str, scale);
}

void M_DrawTransPic (int x, int y, mpic_t *pic)
{
	Draw_TransPic (x + ((menuwidth - 320) >> 1), y + m_yofs, pic);
}

void M_DrawPic (int x, int y, mpic_t *pic)
{
	Draw_Pic (x + ((menuwidth - 320) >> 1), y + m_yofs, pic);
}

void M_DrawCrossHairPic (int x, int y)
{
	Draw_CrossHairPic (x + ((menuwidth - 320) >> 1), y + m_yofs);
}

byte	identityTable[256], translationTable[256];

void M_BuildTranslationTable (int top, int bottom)
{
	int	j;
	byte	*dest, *source;

	for (j=0 ; j<256 ; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	if (top < 128)	// the artists made some backward ranges. sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[TOP_RANGE+j] = source[top+15-j];

	if (bottom < 128)// the artists made some backward ranges. sigh.
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[BOTTOM_RANGE+j] = source[bottom+15-j];
}


void M_DrawTransPicTranslate (int x, int y, mpic_t *pic)
{
	Draw_TransPicTranslate (x + ((menuwidth - 320) >> 1), y + m_yofs, pic, translationTable);
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	Draw_TextBox (x + ((menuwidth - 320) >> 1), y + m_yofs, width, lines);
}

//=============================================================================

int	m_save_demonum;

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f ();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		return;
	}
	if (key_dest == key_console)
	{
		Con_ToggleConsole_f ();
	}
	else
	{
		M_Menu_Main_f ();
	}
}


//=============================================================================
/* MAIN MENU */

int	m_main_cursor;
int	MAIN_ITEMS = 6;

void M_Menu_Main_f (void)
{
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;
}

void M_Main_Draw (void)
{
	int	f;
	mpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ((320-p->width)>>1, 4, p);

	M_DrawTransPic (72, 32, Draw_CachePic("gfx/mainmenu.lmp"));

	f = (int)(host_time*10) % 6;

	M_DrawTransPic (54, 32 + m_main_cursor*20, Draw_CachePic(va("gfx/menudot%i.lmp", f+1)));
}


void M_Main_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
		key_dest = key_game;
		m_state = m_none;
		cls.demonum = m_save_demonum;
// joe: automatic startdemos sequence removed
//		if (cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
//			CL_NextDemo ();
		break;

	case K_MWHEELDOWN:
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_MWHEELUP:
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

// by joe - START
	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		m_main_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		m_main_cursor = MAIN_ITEMS - 1;
		break;
// by joe - END
	
	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		m_entersound = true;

		switch (m_main_cursor)
			{
			case 0:
				M_Menu_SinglePlayer_f ();
				break;

			case 1:
				M_Menu_MultiPlayer_f ();
				break;

			case 2:
				M_Menu_Options_f ();
				break;

			case 3:
				M_Menu_Maps_f ();
				break;

			case 4:
				M_Menu_Demos_f ();
				break;

			case 5:
				M_Menu_Quit_f ();
				break;
			}
	}
}

//=============================================================================
/* SINGLE PLAYER MENU */

qboolean	m_singleplayer_confirm;
int	m_singleplayer_cursor;
#define	SINGLEPLAYER_ITEMS	3

void M_Menu_SinglePlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
	m_singleplayer_confirm = false;
}


void M_SinglePlayer_Draw (void)
{
	int	f;
	mpic_t	*p;

	if (m_singleplayer_confirm)
	{
		M_PrintWhite (64, 11*8, "Are you sure you want to");
		M_PrintWhite (64, 12*8, "    start a new game?");
		return;
	}

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic("gfx/sp_menu.lmp"));

	f = (int)(realtime*10) % 6;
	M_DrawTransPic (54, 32 + m_singleplayer_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f+1)));
}

static void StartNewGame (void)
{
	key_dest = key_game;
	if (sv.active)
		Cbuf_AddText ("disconnect\n");
	Cbuf_AddText ("maxplayers 1\n");
	Cvar_SetValue ("teamplay", 0);
	Cvar_SetValue ("coop", 0);
	if (nehahra)
		Cbuf_AddText ("map nehstart\n");
	else
		Cbuf_AddText ("map start\n");
}

void M_SinglePlayer_Key (int key)
{
	if (m_singleplayer_confirm)
	{
		if (key == 'n' || key == K_ESCAPE)
		{
			m_singleplayer_confirm = false;
			m_entersound = true;
		}
		else if (key == 'y' || key == K_ENTER || K_MOUSE1)
		{
			StartNewGame ();
		}
		return;
	}
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_MWHEELDOWN:
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		m_singleplayer_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;
	
	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		m_entersound = true;

		switch (m_singleplayer_cursor)
		{
		case 0:
			if (sv.active)
				m_singleplayer_confirm = true;
			else
				StartNewGame ();
			break;
		case 1:
			M_Menu_Load_f ();
			break;

		case 2:
			M_Menu_Save_f ();
			break;
		}
	}
}

//=============================================================================
/* LOAD/SAVE MENU */

int	load_cursor;		// 0 < load_cursor < MAX_SAVEGAMES

#define	MAX_SAVEGAMES	12
char	m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH+1];
int	loadable[MAX_SAVEGAMES];

void M_ScanSaves (void)
{
	int	i, j, version;
	char	name[MAX_OSPATH];
	FILE	*f;

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		sprintf (name, "%s/s%i.sav", com_gamedir, i);
		if (!(f = fopen(name, "r")))
			continue;
		fscanf_s (f, "%i\n", &version);
		fscanf_s (f, "%79s\n", name);
		strncpy (m_filenames[i], name, sizeof(m_filenames[i])-1);

	// change _ back to space
		for (j=0 ; j<SAVEGAME_COMMENT_LENGTH ; j++)
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		loadable[i] = true;
		fclose (f);
	}
}

void M_Menu_Load_f (void)
{
	m_entersound = true;
	m_state = m_load;
	key_dest = key_menu;
	M_ScanSaves ();
}


void M_Menu_Save_f (void)
{
	if (!sv.active || cl.intermission || svs.maxclients != 1)
		return;

	m_entersound = true;
	m_state = m_save;
	key_dest = key_menu;
	M_ScanSaves ();
}


void M_Load_Draw (void)
{
	int	i;
	mpic_t	*p;

	p = Draw_CachePic ("gfx/p_load.lmp");
	M_DrawPic ((320-p->width)>>1, 4, p);

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
		M_Print (16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Save_Draw (void)
{
	int	i;
	mpic_t	*p;

	p = Draw_CachePic ("gfx/p_save.lmp");
	M_DrawPic ((320-p->width)>>1, 4, p);

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
		M_Print (16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Load_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f ();
		break;

	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		if (!loadable[load_cursor])
			return;
		m_state = m_none;
		key_dest = key_game;

	// Host_Loadgame_f can't bring up the loading plaque because too much
	// stack space has been used, so do it now
		SCR_BeginLoadingPlaque ();

	// issue the load command
		Cbuf_AddText (va("load s%i\n", load_cursor));
		return;

	case K_UPARROW:
	case K_MWHEELUP:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_MWHEELDOWN:
	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		load_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		load_cursor = MAX_SAVEGAMES - 1;
		break;
	}
}


void M_Save_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f ();
		break;

	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText (va("save s%i\n", load_cursor));
		return;

	case K_UPARROW:
	case K_MWHEELUP:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES - 1;
		break;

	case K_MWHEELDOWN:
	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		load_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		load_cursor = MAX_SAVEGAMES - 1;
		break;
	}
}

//=============================================================================
/* MULTIPLAYER MENU */

int	m_multiplayer_cursor;
#define	MULTIPLAYER_ITEMS	4


void M_Menu_MultiPlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_multiplayer;
	m_entersound = true;
}


void M_MultiPlayer_Draw (void)
{
	int	f;
	mpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic("gfx/mp_menu.lmp"));

	f = (int)(realtime * 10) % 6;

	M_DrawTransPic (54, 32 + m_multiplayer_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f+1)));

	if (serialAvailable || ipxAvailable || tcpipAvailable)
		return;

	M_PrintWhite ((320/2) - ((27*8)/2), 148, "No Communications Available");
}



void M_MultiPlayer_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_MWHEELDOWN:
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		m_multiplayer_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		m_entersound = true;
		switch (m_multiplayer_cursor)
		{
		case 0:
			M_Menu_ServerList_f ();
			break;

		case 1:
		case 2:
			if (serialAvailable || ipxAvailable || tcpipAvailable)
				M_Menu_Net_f ();
			break;

		case 3:
			M_Menu_Setup_f ();
			break;
		}
	}
}

//=============================================================================
/* SETUP MENU */

extern	qboolean OnChange_gl_crosshairimage (cvar_t *var, char *string);
extern cvar_t	gl_crosshairimage;
extern cvar_t	crosshairsize;
extern cvar_t	crosshaircolor;

int		setup_cursor = 9;
int		setup_cursor_table[] = {40, 56, 80, 104, 128, 144, 152, 160, 178};
char	namemaker_name[16];
char	setup_hostname[16], setup_myname[16];
int		setup_oldtop, setup_oldbottom, setup_top, setup_bottom;
int		setup_chs, setup_chi, setup_chc;

#define	NUM_SETUP_CMDS	9

void M_Menu_Setup_f (void)
{
	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;

	Q_strncpyz (setup_hostname, hostname.string, sizeof(setup_hostname));
	
	if (!(strlen(setup_myname)))
	Q_strncpyz (setup_myname, cl_name.string, sizeof(setup_myname));//R00k
	
	setup_top = setup_oldtop = ((int)cl_color.value) >> 4;
	setup_bottom = setup_oldbottom = ((int)cl_color.value) & 15;

	setup_chs = crosshairsize.value;
	setup_chc = crosshaircolor.value;
	setup_chi = 0;
}

void M_Setup_Draw (void)
{
	mpic_t	*p;
	
	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320-p->width)>>1, 4, p);

	M_Print (64, 40, "Hostname");
	M_DrawTextBox (160, 32, 16, 1);
	M_Print (168, 40, setup_hostname);

	M_Print (64, 56, "Your name");
	M_DrawTextBox (160, 48, 16, 1);
	M_PrintWhite (168, 56, setup_myname);

	M_Print (64, 80, "Name Maker");

	M_Print (64, 104, "Shirt color");
	M_Print (64, 128, "Pants color");
	M_Print (64, 144, "Crosshair Image");
	M_Print (64, 152, "Crosshair Size");
	M_Print (64, 160, "Crosshair Color");

	M_DrawTextBox (64, 178-8, 14, 1);
	M_Print (72, 178, "Accept Changes");

	M_DrawTextBox (160, 64, 8, 8);
	p = Draw_CachePic ("gfx/menuplyr.lmp");
	M_BuildTranslationTable (setup_top*16, setup_bottom*16);
	M_DrawTransPicTranslate (176, 76, p);

	//R00k crosshair option
	M_DrawTextBox (236, 64, 8, 8);	
	M_DrawCrossHairPic (276, 104);

	M_DrawCharacter (56, setup_cursor_table [setup_cursor], 12+((int)(realtime*4)&1));

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_hostname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));

	if (setup_cursor == 1)
		M_DrawCharacter (168 + 8*strlen(setup_myname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));
}

void M_Setup_Key (int k)
{
	int	l;

	switch (k)
	{
	case K_ESCAPE:
		Q_strncpyz (setup_myname, cl_name.string, sizeof(setup_myname));//R00k
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS - 1;
		break;

	case K_MWHEELDOWN:
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor = NUM_SETUP_CMDS - 1;
		break;

	case K_LEFTARROW:
		if (setup_cursor < 2)
			return;
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 3)
			setup_top = setup_top - 1;
		if (setup_cursor == 4)
			setup_bottom = setup_bottom - 1;
	
		if (setup_cursor == 5)
		{
			setup_chi = setup_chi - 1;
			if (setup_chi < 1)
				setup_chi = 1;
				
			if (setup_chi > 11)
				setup_chi = 11;

			if (OnChange_gl_crosshairimage(&gl_crosshairimage, (va("crosshair%i", setup_chi))))
				Cbuf_AddText (va("crosshairimage crosshair%i\n", setup_chi));
			else
				Cbuf_AddText (va("crosshair %i\n", setup_chi));
		}

		if (setup_cursor == 6)
		{
			setup_chs = setup_chs - 1;
			setup_chs = bound(1, setup_chs, 4);						
			Cbuf_AddText (va("crosshairsize %i\n", setup_chs));
		}

		if (setup_cursor == 7)
		{
			setup_chc = setup_chc - 1;
			setup_chc = bound(1, setup_chc, 255);						
			Cbuf_AddText (va("crosshaircolor %i\n", setup_chc));
		}

		break;
	case K_RIGHTARROW:
		if (setup_cursor < 2)
			return;
forward:
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 3)
			setup_top = setup_top + 1;
		if (setup_cursor == 4)
			setup_bottom = setup_bottom + 1;

		if (setup_cursor == 5)
		{
			setup_chi = setup_chi + 1;

			if (setup_chi < 1)
				setup_chi = 1;
				
			if (setup_chi > 11)
				setup_chi = 11;

			if (OnChange_gl_crosshairimage(&gl_crosshairimage, (va("crosshair%i", setup_chi))))
				Cbuf_AddText (va("crosshairimage crosshair%i\n", setup_chi));
			else
				Cbuf_AddText (va("crosshair %i\n", setup_chi));
		}
		if (setup_cursor == 6)
		{
			setup_chs = setup_chs + 1;

			if (setup_chs < 1)
				setup_chs = 1;
				
			if (setup_chs > 4)
				setup_chs = 4;
			
			Cbuf_AddText (va("crosshairsize %i\n", setup_chs));
		}

		if (setup_cursor == 7)
		{
			setup_chc = setup_chc + 1;
			setup_chc = bound(1, setup_chc, 255);						
			Cbuf_AddText (va("crosshaircolor %i\n", setup_chc));
		}
		break;

	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		if (setup_cursor == 0 || setup_cursor == 1)
			return;

		if (setup_cursor == 3 || setup_cursor == 4)
			goto forward;

		if (setup_cursor == 2)
		{
			m_entersound = true;
			M_Menu_NameMaker_f ();
			break;
		}

		// setup_cursor == 5 (OK)
		if (setup_cursor == 8)
		{
			Cbuf_AddText (va("name \"%s\"\n", setup_myname));
			Cvar_Set ("hostname", setup_hostname);
			Cbuf_AddText (va("color %i %i\n", setup_top, setup_bottom));
			m_entersound = true;
			M_Menu_MultiPlayer_f ();
		}
		break;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (strlen(setup_hostname))
				setup_hostname[strlen(setup_hostname)-1] = 0;
		}

		if (setup_cursor == 1)
		{
			if (strlen(setup_myname))
				setup_myname[strlen(setup_myname)-1] = 0;
		}
		break;

	default:
		if (k < 32 || k > 127)
			break;
		if (setup_cursor == 0)
		{
			l = strlen(setup_hostname);
			if (l < 15)
			{
				setup_hostname[l+1] = 0;
				setup_hostname[l] = k;
			}
		}
		if (setup_cursor == 1)
		{
			l = strlen(setup_myname);
			if (l < 15)
			{
				setup_myname[l+1] = 0;
				setup_myname[l] = k;
			}
		}
	}

	if (setup_top > 13)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = 13;
	if (setup_bottom > 13)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = 13;
}

//=============================================================================
/* NAME MAKER MENU */ //From: JoeQuake 1.5Dev!!
//=============================================================================
int	namemaker_cursor_x, namemaker_cursor_y;
#define	NAMEMAKER_TABLE_SIZE	16

void M_Menu_NameMaker_f (void)
{
	key_dest = key_menu;
	m_state = m_namemaker;
	m_entersound = true;
	Q_strncpyz (namemaker_name, setup_myname, sizeof(namemaker_name));
}

void M_NameMaker_Draw (void)
{
	int	x, y;

	M_Print (48, 16, "Your name");
	M_DrawTextBox (120, 8, 16, 1);
	M_PrintWhite (128, 16, namemaker_name);

	for (y=0 ; y<NAMEMAKER_TABLE_SIZE ; y++)
		for (x=0 ; x<NAMEMAKER_TABLE_SIZE ; x++)
			M_DrawCharacter (32 + (16 * x), 40 + (8 * y), NAMEMAKER_TABLE_SIZE * y + x);

	if (namemaker_cursor_y == NAMEMAKER_TABLE_SIZE)
		M_DrawCharacter (128, 184, 12 + ((int)(realtime*4)&1));
	else
		M_DrawCharacter (24 + 16*namemaker_cursor_x, 40 + 8*namemaker_cursor_y, 12 + ((int)(realtime*4)&1));

	M_DrawTextBox (136, 176, 2, 1);
	M_Print (144, 184, "OK");
}

void Key_Extra (int *key);
void M_NameMaker_Key (int k)
{
	int	l;

	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Setup_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_y--;
		if (namemaker_cursor_y < 0)
			namemaker_cursor_y = NAMEMAKER_TABLE_SIZE;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_y++;
		if (namemaker_cursor_y > NAMEMAKER_TABLE_SIZE)
			namemaker_cursor_y = 0;
		break;

	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_y = 0;
		break;

	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_y = NAMEMAKER_TABLE_SIZE;
		break;

	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_x--;
		if (namemaker_cursor_x < 0)
			namemaker_cursor_x = NAMEMAKER_TABLE_SIZE - 1;
		break;

	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_x++;
		if (namemaker_cursor_x >= NAMEMAKER_TABLE_SIZE)
			namemaker_cursor_x = 0;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_x = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		namemaker_cursor_x = NAMEMAKER_TABLE_SIZE - 1;
		break;

	case K_BACKSPACE:
		if ((l = strlen(namemaker_name)))
			namemaker_name[l-1] = 0;
		break;

	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		if (namemaker_cursor_y == NAMEMAKER_TABLE_SIZE)
		{
			Q_strncpyz (setup_myname, namemaker_name, sizeof(setup_myname));
			M_Menu_Setup_f ();
		}
		else
		{
			l = strlen(namemaker_name);
			if (l < 15)
			{
				namemaker_name[l] = NAMEMAKER_TABLE_SIZE * namemaker_cursor_y + namemaker_cursor_x;
				namemaker_name[l+1] = 0;
			}
		}
		break;

	default:
		if (k < 32 || k > 127)
			break;

		Key_Extra (&k);

		l = strlen(namemaker_name);
		if (l < 15)
		{
			namemaker_name[l] = k;
			namemaker_name[l+1] = 0;
		}
		break;
	}
}
//=============================================================================
/* NET MENU */

int	m_net_cursor, m_net_items, m_net_saveHeight;

char *net_helpMessage[] =
{
/* .........1.........2.... */
  "                        ",
  " Two computers connected",
  "   through two modems.  ",
  "                        ",

  "                        ",
  " Two computers connected",
  " by a null-modem cable. ",
  "                        ",

  " Novell network LANs    ",
  " or Windows 95 DOS-box. ",
  "                        ",
  "(LAN=Local Area Network)",

  " Commonly used to play  ",
  " over the Internet, but ",
  " also used on a Local   ",
  " Area Network.          "
};

void M_Menu_Net_f (void)
{
	key_dest = key_menu;
	m_state = m_net;
	m_entersound = true;
	m_net_items = 4;

	if (m_net_cursor >= m_net_items)
		m_net_cursor = 0;
	m_net_cursor--;
	M_Net_Key (K_DOWNARROW);
}

void M_Net_Draw (void)
{
	int	f;
	mpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320-p->width)/2, 4, p);

	f = 32;
	if (serialAvailable)
	{
		p = Draw_CachePic ("gfx/netmen1.lmp");
	}
	else
	{
#ifdef _WIN32
		p = NULL;
#else
		p = Draw_CachePic ("gfx/dim_modm.lmp");
#endif
	}

	if (p)
		M_DrawTransPic (72, f, p);

	f += 19;
	if (serialAvailable)
	{
		p = Draw_CachePic ("gfx/netmen2.lmp");
	}
	else
	{
#ifdef _WIN32
		p = NULL;
#else
		p = Draw_CachePic ("gfx/dim_drct.lmp");
#endif
	}

	if (p)
		M_DrawTransPic (72, f, p);

	f += 19;
	if (ipxAvailable)
		p = Draw_CachePic ("gfx/netmen3.lmp");
	else
		p = Draw_CachePic ("gfx/dim_ipx.lmp");
	M_DrawTransPic (72, f, p);

	f += 19;
	if (tcpipAvailable)
		p = Draw_CachePic ("gfx/netmen4.lmp");
	else
		p = Draw_CachePic ("gfx/dim_tcp.lmp");
	M_DrawTransPic (72, f, p);

	if (m_net_items == 5)	// JDC, could just be removed
	{
		f += 19;
		p = Draw_CachePic ("gfx/netmen5.lmp");
		M_DrawTransPic (72, f, p);
	}

	f = (320 - 26 * 8) / 2;
	M_DrawTextBox (f, 134, 24, 4);
	f += 8;
	M_Print (f, 142, net_helpMessage[m_net_cursor*4+0]);
	M_Print (f, 150, net_helpMessage[m_net_cursor*4+1]);
	M_Print (f, 158, net_helpMessage[m_net_cursor*4+2]);
	M_Print (f, 166, net_helpMessage[m_net_cursor*4+3]);

	f = (int)(host_time * 10) % 6;
	M_DrawTransPic (54, 32 + m_net_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f+1)));
}

void M_Net_Key (int k)
{
again:
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_MWHEELDOWN:
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_net_cursor >= m_net_items)
			m_net_cursor = 0;
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		if (--m_net_cursor < 0)
			m_net_cursor = m_net_items - 1;
		break;

	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		m_entersound = true;

		switch (m_net_cursor)
		{
//		case 0:
//			M_Menu_SerialConfig_f ();
//			break;

//		case 1:
//			M_Menu_SerialConfig_f ();
//			break;

		case 2:
			M_Menu_LanConfig_f ();
			break;

		case 3:
			M_Menu_LanConfig_f ();
			break;

		case 4:
// multiprotocol
			break;
		}
	}

	if (m_net_cursor == 0 && !serialAvailable)
		goto again;
	if (m_net_cursor == 1 && !serialAvailable)
		goto again;
	if (m_net_cursor == 2 && !ipxAvailable)
		goto again;
	if (m_net_cursor == 3 && !tcpipAvailable)
		goto again;
}

//=============================================================================
/* OPTIONS MENU */

#define	OPTIONS_ITEMS	11

#define	SLIDER_RANGE	10

int	options_cursor;
int	options_cursor_table[] = {32, 40, 48, 56, 64, 72, 80, 88,104,120,128};
void M_Menu_Options_f (void)
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
}

void M_AdjustSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor)
	{
	case 3:	// mouse speed
		sensitivity.value += dir * 0.5;
		if (sensitivity.value < 1)
			sensitivity.value = 1;
		if (sensitivity.value > 11)
			sensitivity.value = 11;
		Cvar_SetValue ("sensitivity", sensitivity.value);
		break;

	case 4:	
		Cvar_SetValue ("m_look", !m_look.value);
		break;

	case 5:   // invert mouse
		Cvar_SetValue ("m_pitch", -m_pitch.value);
		break;

	case 6:// sfx volume
		volume.value += dir * 0.1;
		if (volume.value < 0)
			volume.value = 0;
		if (volume.value > 1)
			volume.value = 1;
		Cvar_SetValue ("volume", volume.value);
		break;

	case 7:	// always run
		if (cl_forwardspeed.value > 200)
		{
			Cvar_Set ("cl_forwardspeed", "200");
			Cvar_Set ("cl_backspeed", "200");
			Cvar_Set ("cl_sidespeed", "200");
			Cvar_Set ("cl_upspeed","200");//R00k oops! forgot this!
		}
		else
		{
			Cvar_Set ("cl_forwardspeed", "320");
			Cvar_Set ("cl_backspeed", "320");
			Cvar_Set ("cl_sidespeed", "320");
			Cvar_Set ("cl_upspeed","320");
		}
		break;
		/*
		#ifdef _WIN32
			if (modestate == MS_WINDOWED)
		#else
			if (vid_windowedmouse)
		#endif
			case 11:
				// _windowed_mouse
				Cvar_SetValue ("_windowed_mouse", !_windowed_mouse.value);
				break;
		*/
	}
}

void M_DrawSlider (int x, int y, float range)
{
	int	i;

	range = bound(0, range, 1);
	M_DrawCharacter (x-8, y, 128);
	for (i=0 ; i<SLIDER_RANGE ; i++)
		M_DrawCharacter (x + i*8, y, 129);
	M_DrawCharacter (x+i*8, y, 130);
	M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

void M_DrawCheckbox (int x, int y, int on)
{
	if (on)
		M_Print (x, y, "on");
	else
		M_Print (x, y, "off");
}

void M_Options_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		m_entersound = true;
		switch (options_cursor)
		{
		case 0:
			M_Menu_Keys_f ();
			break;

		case 1:
			M_Menu_Video_f ();
			break;

		case 2:
			M_Menu_VideoOptions_f ();
			break;

		case 8:
			Cbuf_AddText ("exec default.cfg\n");
			break;

		case 9:
			m_state = m_none;
			key_dest = key_console;
//			Con_ToggleConsole_f ();
			break;

		default:
			M_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS - 1;
		break;

	case K_MWHEELDOWN:
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		options_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		options_cursor = OPTIONS_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders (1);
		break;
	}

// increased numbers, so the cursor fits to the sliders - joe
	if (k == K_UPARROW || k == K_END || k == K_PGDN)
	{
		if (options_cursor == OPTIONS_ITEMS-1)
			options_cursor = OPTIONS_ITEMS - 2;

		if (options_cursor == OPTIONS_ITEMS-2 && !vid_menudrawfn)
			options_cursor = OPTIONS_ITEMS - 3;
	}
	else
	{
		if (options_cursor == OPTIONS_ITEMS-2 && !vid_menudrawfn)
			options_cursor = OPTIONS_ITEMS - 1;

		if (options_cursor == OPTIONS_ITEMS-1)
			options_cursor = 0;
	}
}

void M_Options_Draw (void)
{
	float	r;
	mpic_t	*p;
	int i = 0;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ((320-p->width)/2, 4, p);

	M_Print (18, options_cursor_table[i], "    Customize controls");
	i++;
	if (vid_menudrawfn)
	M_Print (18, options_cursor_table[i],"           Video Setup");
	i++;
	M_Print (18, options_cursor_table[i], "       Graphic Options");
	i++;
	M_Print (18, options_cursor_table[i], "           Mouse Speed");
	r = (sensitivity.value - 1)/10;
	M_DrawSlider (218, options_cursor_table[i], r);
	i++;
	M_Print (18, options_cursor_table[i],"            Mouse Look");
	M_DrawCheckbox (218, options_cursor_table[i], m_look.value);
	i++;
	M_Print (18, options_cursor_table[i],"          Invert mouse");
	M_DrawCheckbox (218, options_cursor_table[i], m_pitch.value < 0);
	i++;
	M_Print (18, options_cursor_table[i], "          Sound Volume");
	r = volume.value;
	M_DrawSlider (218, options_cursor_table[i], r);
	i++;
	M_Print (18, options_cursor_table[i],"            Always Run");
	M_DrawCheckbox (218, options_cursor_table[i], cl_forwardspeed.value > 200);
	i++;
	M_Print (18, options_cursor_table[i], "     Reset to defaults");
	i++;
	M_Print (18, options_cursor_table[i], "         Go to console");
	i++;

	M_DrawCharacter (200, options_cursor_table[options_cursor], 12+((int)(realtime*4)&1));
}

//=============================================================================
/* KEYS MENU */

char *bindnames[][2] =
{
{"+attack", 	"attack"},
{"impulse 10", 	"change weapon"},
{"+jump", 		"jump / swim up"},
{"+forward", 	"walk forward"},
{"+back", 		"backpedal"},
{"+left", 		"turn left"},
{"+right", 		"turn right"},
{"+speed", 		"run"},
{"+moveleft", 	"step left"},
{"+moveright", 	"step right"},
{"+strafe", 	"sidestep"},
{"+lookup", 	"look up"},
{"+lookdown", 	"look down"},
{"centerview", 	"center view"},
{"+klook", 		"keyboard look"},
{"+moveup",		"swim up"},
{"+movedown",	"swim down"}
};

#define	NUMCOMMANDS	(sizeof(bindnames)/sizeof(bindnames[0]))

int	keys_cursor;
int	bind_grab;

void M_Menu_Keys_f (void)
{
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;
}

void M_FindKeysForCommand (char *command, int *twokeys)
{
	int	count, j;//, l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	//l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		//if (!strncmp(b, command, l))
		if (!strcmp(b, command))
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void M_UnbindCommand (char *command)
{
	int	j;//, l;
	char	*b;

	//l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		//if (!strncmp(b, command, l))
		if (!strcmp (b, command))
			Key_SetBinding (j, "");
	}
}

void M_Keys_Draw (void)
{
	int	i, l, keys[2], x, y;
	char	*name;
	mpic_t	*p;

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ((320-p->width)/2, 4, p);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, backspace to clear");

// search for known bindings
	for (i=0 ; i<NUMCOMMANDS ; i++)
	{
		y = 48 + 8*i;

		M_Print (16, y, bindnames[i][1]);

		l = strlen (bindnames[i][0]);

		M_FindKeysForCommand (bindnames[i][0], keys);

		if (keys[0] == -1)
		{
			M_Print (140, y, "???");
		}
		else
		{
			name = Key_KeynumToString (keys[0]);
			M_Print (140, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				M_Print (140 + x + 8, y, "or");
				M_Print (140 + x + 32, y, Key_KeynumToString (keys[1]));
			}
		}
	}

	if (bind_grab)
		M_DrawCharacter (130, 48 + keys_cursor*8, '=');
	else
		M_DrawCharacter (130, 48 + keys_cursor*8, 12+((int)(realtime*4)&1));
}

void M_Keys_Key (int k)
{
	char	cmd[80];
	int	keys[2];

	if (bind_grab)
	{	// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE)
		{
			bind_grab = false;
		}
		else if (k != '`')
		{
			sprintf (cmd, "bind \"%s\" \"%s\"\n", Key_KeynumToString (k), bindnames[keys_cursor][0]);
			Cbuf_InsertText (cmd);
		}

		bind_grab = false;
		return;
	}

	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Options_f ();
		break;

	case K_LEFTARROW:
	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor--;
		if (keys_cursor < 0)
			keys_cursor = NUMCOMMANDS-1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor++;
		if (keys_cursor >= NUMCOMMANDS)
			keys_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor = NUMCOMMANDS - 1;
		break;

	case K_SPACE:
	case K_ENTER:		// go into bind mode
	case K_MOUSE1:
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		S_LocalSound ("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}

//=============================================================================
/* VIDEO OPTIONS MENU */	

void M_Menu_VideoOptions_f (void)
{
	key_dest = key_menu;
	m_state = m_videooptions;
	m_entersound = true;
	#ifdef GLQUAKE
	CheckParticles ();
	#endif
}

#ifdef GLQUAKE

#define	VOM_ITEMS	19

int	vom_cursor = 0;
int	vom_cursor_table[] = {32, 40, 48, 56, 64, 72, 80, 88,96,104,112,120,128,136,144,152,160,176,186};

extern	cvar_t	r_glowlg;
extern	cvar_t	scr_sbaralpha;
extern	cvar_t	r_simpleitems;
extern	cvar_t	gl_lavasmoke;
#ifndef USEFAKEGL
extern	cvar_t	r_bloom;
#endif
extern	int particle_mode;

void R_SetParticles (int val);

void M_AdjustVOMSliders (int dir)
{
	static float v;

	S_LocalSound ("misc/menu3.wav");

	switch (vom_cursor)
	{
	case 1:    // Field Of View
		v = scr_fov.value + dir * 5;
		v = bound(10, v, 140);
		Cvar_SetValue ("fov", v);
		break;

	case 2:	// screen size
		scr_viewsize.value += dir * 10;
		if (scr_viewsize.value < 30)
			scr_viewsize.value = 30;
		if (scr_viewsize.value > 120)
			scr_viewsize.value = 120;
		Cvar_SetValue ("viewsize", scr_viewsize.value);
		break;

	case 4:
		scr_sbaralpha.value += dir * 0.1;
		scr_sbaralpha.value = bound(0, scr_sbaralpha.value, 1);
		Cvar_SetValue ("scr_sbaralpha", scr_sbaralpha.value);
		break;

	case 8:
		r_shadows.value += dir * 0.1;
		r_shadows.value = bound(0, r_shadows.value, 1.10);
		Cvar_SetValue ("r_shadows", r_shadows.value);
		break;

	case 10:
		r_wateralpha.value += dir * 0.1;
		r_wateralpha.value = bound(0, r_wateralpha.value, 1);
		Cvar_SetValue ("r_wateralpha", r_wateralpha.value);
		break;
	}
}

void M_VideoOptions_Draw (void)
{
	mpic_t	*p;
	int		i = 0;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ((320-p->width)/2, 4, p);

	M_Print			(16, vom_cursor_table[i], "        Particle style");
	M_Print	(216, vom_cursor_table[i], !particle_mode ? "classic" : particle_mode == 1 ? "Enhanced" : "mixed");
	i++;
	M_Print			(16, vom_cursor_table[i], "         Field of view");
	M_Print (216, vom_cursor_table[i], va("%i",(int)scr_fov.value));
	i++;
	M_Print			(16, vom_cursor_table[i], "           Screen size");	
	M_Print (216, vom_cursor_table[i], va("%i",(int)scr_viewsize.value));
	i++;
	M_Print			(16, vom_cursor_table[i],"             HUD style");
	M_Print (216, vom_cursor_table[i], cl_sbar.value ? "classic" : "overlay");
	i++;
	M_Print (		16, vom_cursor_table[i],"      HUD transparency");
	M_Print (216, vom_cursor_table[i], va("%1.1f",(float)scr_sbaralpha.value));
	i++;
	M_Print			(16, vom_cursor_table[i], "        Colored lights");
	M_DrawCheckbox (216, vom_cursor_table[i], gl_loadlitfiles.value);
	i++;
	M_Print			(16, vom_cursor_table[i], "        Dynamic lights");
	M_DrawCheckbox (216, vom_cursor_table[i], r_dynamic.value);
	i++;
	M_Print			(16, vom_cursor_table[i],"       Vertex lighting");
	M_DrawCheckbox (216, vom_cursor_table[i], gl_vertexlights.value);
	i++;
	M_Print			(16, vom_cursor_table[i],"      Shadow intensity");
	M_Print (216, vom_cursor_table[i], va("%1.1f",(float)r_shadows.value));
	i++;
	M_Print			(16, vom_cursor_table[i],"        Water caustics");
	M_DrawCheckbox (216, vom_cursor_table[i], gl_caustics.value);
	i++;
	M_Print			(16, vom_cursor_table[i],"           Water alpha");
	M_Print (216, vom_cursor_table[i], va("%1.1f",(float)r_wateralpha.value));
	i++;
	M_Print			(16, vom_cursor_table[i],"          Simple items");
	M_DrawCheckbox (216, vom_cursor_table[i], r_simpleitems.value);
	i++;
	M_Print			(16, vom_cursor_table[i],"          Cel-shading");
	M_DrawCheckbox (216, vom_cursor_table[i], r_celshading.value);
	i++;
	M_Print			(16, vom_cursor_table[i],"            Lava smoke");
	M_DrawCheckbox (216, vom_cursor_table[i], gl_lavasmoke.value);
	i++;
	M_Print			(16, vom_cursor_table[i],"             Q3 models");
	M_DrawCheckbox (216, vom_cursor_table[i], gl_loadq3models.value);
	i++;
#ifndef USEFAKEGL
	M_Print			(16, vom_cursor_table[i],"          Light Blooms");
	M_DrawCheckbox (216, vom_cursor_table[i], r_bloom.value);
#else
	M_Print			(16, vom_cursor_table[i],"          Light Blooms");
	M_PrintWhite	(216, vom_cursor_table[i],"				DISABLED");
#endif
	i++;
	M_Print			(16, vom_cursor_table[i],"      Particle Options");
	i++;
	M_PrintWhite	(16, vom_cursor_table[i],"          Classic mode");
	i++;
	M_PrintWhite	(16, vom_cursor_table[i],"          High quality");
	i++;

	M_DrawCharacter (200, vom_cursor_table[vom_cursor], 12+((int)(realtime*4)&1));
}

void M_VideoOptions_Key (int k)
{
	extern	void R_ToggleParticles (void);

	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		vom_cursor--;
		if (vom_cursor < 0)
			vom_cursor = VOM_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		vom_cursor++;
		if (vom_cursor >= VOM_ITEMS)
			vom_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		vom_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		vom_cursor = VOM_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_AdjustVOMSliders (-1);
		break;

	case K_RIGHTARROW:
		M_AdjustVOMSliders (1);
		break;

	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		switch (vom_cursor)
		{
		case 0:
			CheckParticles();
			R_SetParticles (!particle_mode);
			break;

		case 3:
			Cvar_SetValue ("cl_sbar", !cl_sbar.value);
			break;

		case 5:
			Cvar_SetValue ("gl_loadlitfiles", !gl_loadlitfiles.value);
			break;

		case 6:
			Cvar_SetValue ("r_dynamic", !r_dynamic.value);
			break;

		case 7:
			Cvar_SetValue ("gl_vertexlights", !gl_vertexlights.value);
			break;


		case 9:
			Cvar_SetValue ("gl_caustics", !gl_caustics.value);
			break;

		case 10:
			Cvar_SetValue ("gl_waterfog", !gl_waterfog.value ? 1 : gl_waterfog.value == 1 ? 2 : 0);
			break;

		case 11:
			Cvar_SetValue ("r_simpleitems",!r_simpleitems.value);
			break;

		case 12:
			Cvar_SetValue ("r_celshading",!r_celshading.value);
			break;

		case 13:
			Cvar_SetValue ("gl_lavasmoke", !gl_lavasmoke.value);
			break;

		case 14:
			Cvar_SetValue ("gl_loadq3models", !gl_loadq3models.value);
			break;
#ifndef USEFAKEGL
		case 15:
			Cvar_SetValue ("r_bloom", !r_bloom.value);
			break;
#endif
		case 16:
			M_Menu_Particles_f ();
			break;

		case 17:
			Cvar_Set ("gl_loadlitfiles", "0");
			Cvar_Set ("r_dynamic", "1");
			Cvar_Set ("gl_vertexlights", "0");
			Cvar_Set ("r_shadows", "0");
			if (particle_mode)
				R_SetParticles (0);
			Cvar_Set ("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST");
			Cvar_Set ("gl_picmip", "0");
			Cvar_Set ("gl_caustics", "0");
			Cvar_Set ("gl_waterfog", "0");
			Cvar_Set ("r_wateralpha", "1");
			Cvar_Set ("r_novis", "0");
			Cvar_Set ("gl_anisotropic", "1");
			Cvar_Set ("gl_triplebuffer", "0");
			Cvar_Set ("gl_lavasmoke", "0");
			Cvar_Set ("gl_decal_bullets", "0");
			Cvar_Set ("gl_decal_explosions", "0");
			Cvar_Set ("gl_decal_sparks", "0");
			Cvar_Set ("gl_decal_blood", "0");
			Cvar_Set ("r_bloom", "0");
			Cvar_Set ("gl_shiny", "0");
			Cvar_Set ("gl_externaltextures_bmodels", "0");
			Cvar_Set ("gl_externaltextures_world", "0");
			Cvar_Set ("r_skyscroll", "0");
			Cvar_Set ("gl_motion_blur", "0");
			break;

		case 18:
			Cvar_Set ("gl_loadlitfiles", "1");
			Cvar_Set ("r_dynamic", "1");
			Cvar_Set ("gl_vertexlights", "1");
			Cvar_Set ("r_shadows", "1.10");
			if (!particle_mode)
				R_SetParticles (1);
			Cvar_Set ("gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR");
			Cvar_Set ("gl_externaltextures_bmodels", "1");
			Cvar_Set ("gl_externaltextures_world", "1");
			Cvar_Set ("gl_picmip", "0");
			Cvar_Set ("r_skyscroll", "1");
			Cvar_Set ("gl_caustics", "1");
			Cvar_Set ("gl_waterfog", "1");
			Cvar_Set ("r_wateralpha", "0.5");
			Cvar_Set ("gl_anisotropic", "8");
			Cvar_Set ("gl_triplebuffer", "1");
			Cvar_Set ("gl_lavasmoke", "1");
			Cvar_Set ("gl_decal_bullets", "1");
			Cvar_Set ("gl_decal_explosions", "1");
			Cvar_Set ("gl_decal_sparks", "1");
			Cvar_Set ("gl_decal_blood", "1");
			Cvar_Set ("r_bloom", "1");
			Cvar_Set ("gl_shiny", "1");
			Cvar_Set ("gl_motion_blur", "0.333");
			break;

		default:
			M_AdjustVOMSliders (1);
			break;
		}
	}
}

// joe: particles submenu
#define	PART_ITEMS	20
extern	cvar_t gl_coronas;
int	part_cursor = 0;

void M_Menu_Particles_f (void)
{
	key_dest = key_menu;
	m_state = m_particles;
	m_entersound = true;
}

void M_Particles_Draw (void)
{
	mpic_t	*p;
	float	r;
	
	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ((320-p->width)/2, 4, p);
	
	M_Print (16, 32, "            Explosions");
	M_Print (220, 32, !gl_part_explosions.value ? "classic" : "enhanced");

	M_Print (16, 40, "                Trails");
	M_Print (220, 40, !gl_part_trails.value ? "classic" : "enhanced");

	M_Print (16, 48, "                Sparks");
	M_Print (220, 48, !gl_part_sparks.value ? "off" : "enhanced");

	M_Print (16, 56, "              Gunshots");
	M_Print (220, 56, !gl_part_gunshots.value ? "classic" : "enhanced");

	M_Print (16, 64, "                 Blood");
	M_Print (220, 64, !gl_part_blood.value ? "classic" : "enhanced");

	M_Print (16, 72, "     Teleport splashes");
	M_Print (220, 72, !gl_part_telesplash.value ? "classic" : "enhanced");

	M_Print (16, 80, "      Spawn explosions");
	M_Print (220, 80, !gl_part_blobs.value ? "classic" : "enhanced");

	M_Print (16, 88, "         Lava splashes");
	M_Print (220, 88, !gl_part_lavasplash.value ? "classic" : "enhanced");

	M_Print (16, 96, "                 Flies");
	M_Print (220, 96, !gl_part_flies.value ? "off" : "on");

	M_Print (16, 104, "                Flames");
	M_Print (220, 104, !gl_part_flames.value ? "classic" : "enhanced");

	M_Print (16, 112, "             Lightning");
	M_Print (220, 112, !gl_part_lightning.value ? "classic" : "enhanced");

	M_Print (16, 120, "    Bouncing particles");
	M_DrawCheckbox (220, 120, gl_bounceparticles.value);

	M_Print (16, 128, "   Rainy Day particles");
	M_DrawCheckbox (220, 128, gl_rain.value);

	M_Print (16, 136, "               Coronas");//R00k
	M_DrawCheckbox (219, 136, gl_coronas.value);

	M_Print (16, 144, "          Blood Decals");//R00k
	M_DrawCheckbox (219, 144, gl_decal_blood.value);

	M_Print (16, 152, "         Bullet Decals");//R00k
	M_DrawCheckbox (219, 152, gl_decal_bullets.value);

	M_Print (16, 160, "      Explosion Decals");//R00k
	M_DrawCheckbox (219, 160, gl_decal_explosions.value);

	M_Print (16, 168, "Lightning Spark Decals");//R00k
	M_DrawCheckbox (219, 168, gl_decal_sparks.value);

	M_Print (16, 176, "       Decal Fade Time");
	r = gl_decaltime.value;
	M_DrawSlider (220, 176, (r/60));//max 60 seconds (good enough eh?)

	M_Print (16, 184, "   Decal View Distance");
	r = gl_decal_viewdistance.value;
	M_DrawSlider (220, 184, (r/r_farclip.value));

	// cursor
	M_DrawCharacter (200, 32 + part_cursor*8, 12+((int)(realtime*4)&1));
}

void M_AdjustPartSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav");

	switch (part_cursor)
	{
	case 18:
		gl_decaltime.value += dir;
		gl_decaltime.value = bound(1, gl_decaltime.value, 60);
		Cvar_SetValue ("gl_decaltime", gl_decaltime.value);
		break;

	case 19:
		gl_decal_viewdistance.value += dir * 512;
		gl_decal_viewdistance.value = bound(1, gl_decal_viewdistance.value, r_farclip.value);
		Cvar_SetValue ("gl_decal_viewdistance", gl_decal_viewdistance.value);
		break;
	}

}

void M_Particles_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		CheckParticles ();
		M_Menu_VideoOptions_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		part_cursor--;
		if (part_cursor < 0)
			part_cursor = PART_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		part_cursor++;
		if (part_cursor >= PART_ITEMS)
			part_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		part_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		part_cursor = PART_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_AdjustPartSliders (-1);
		break;

	case K_RIGHTARROW:
	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		switch (part_cursor)
		{
		case 0:
			Cvar_SetValue ("gl_part_explosions", !gl_part_explosions.value);
			break;

		case 1:
			Cvar_SetValue ("gl_part_trails", !gl_part_trails.value);
			break;

		case 2:
			Cvar_SetValue ("gl_part_sparks", !gl_part_sparks.value);
			break;

		case 3:
			Cvar_SetValue ("gl_part_gunshots", !gl_part_gunshots.value);
			break;

		case 4:
			Cvar_SetValue ("gl_part_blood", !gl_part_blood.value);
			break;

		case 5:
			Cvar_SetValue ("gl_part_telesplash", !gl_part_telesplash.value);
			break;

		case 6:
			Cvar_SetValue ("gl_part_blobs", !gl_part_blobs.value);
			break;

		case 7:
			Cvar_SetValue ("gl_part_lavasplash", !gl_part_lavasplash.value);
			break;

		case 8:
			Cvar_SetValue ("gl_part_flies", !gl_part_flies.value);
			break;

		case 9:
			Cvar_SetValue ("gl_part_flames", !gl_part_flames.value);
			break;

		case 10:
			Cvar_SetValue ("gl_part_lightning", !gl_part_lightning.value);
			break;

		case 11:
			Cvar_SetValue ("gl_bounceparticles", !gl_bounceparticles.value);
			break;

		case 12:
			Cvar_SetValue ("gl_rain", !gl_rain.value);
			break;

		case 13:
			Cvar_SetValue ("gl_coronas", !gl_coronas.value);
			break;
		case 14:
			Cvar_SetValue ("gl_decal_blood", !gl_decal_blood.value);
			break;

		case 15:
			Cvar_SetValue ("gl_decal_bullets", !gl_decal_bullets.value);
			break;

		case 16:
			Cvar_SetValue ("gl_decal_explosions", !gl_decal_explosions.value);
			break;

		case 17:
			Cvar_SetValue ("gl_decal_sparks", !gl_decal_sparks.value);
			break;

		default:
			M_AdjustPartSliders (1);
			break;
		}
	}
}

#endif

//=============================================================================
/* VIDEO MENU */

void M_Menu_Video_f (void)
{
	(*vid_menucmdfn) (); //johnfitz	
}


void M_Video_Draw (void)
{
	(*vid_menudrawfn) ();
}


void M_Video_Key (int key)
{
	(*vid_menukeyfn) (key);
}

//=============================================================================
/* COMMON STUFF FOR MAPS AND DEMOS MENUS */

// NOTE: 320x200 res can only handle no more than 17 lines +2 for file
// searching. In GL I use 1 more line, though 320x200 is also available
// under GL too, but I force _nobody_ using that, but 320x240 instead!

#ifndef GLQUAKE
#define	MAXLINES	17	// maximum number of files visible on screen
#else
#define	MAXLINES	18
#endif

char	demodir[MAX_QPATH] = "";
char	prevdir[MAX_QPATH] = "";
char	searchfile[MAX_FILELENGTH] = "";

static	int	demo_cursor = 0, demo_base = 0, globctr = 0;
static qboolean	searchbox = false;

void PrintSortedFiles (void)
{
	int	i;

	demo_base = demo_cursor = 0;

	// TODO: position demo cursor
	if (prevdir)
	{
		for (i=0 ; i<num_files ; i++)
		{
			if (!strcmp(filelist[i].name, prevdir))
			{
				demo_cursor = i;
				if (demo_cursor >= MAXLINES)
				{
					demo_base += demo_cursor - (MAXLINES-1);
					demo_cursor = MAXLINES-1;
				}
				*prevdir = 0;
			}
		}
	}
}

static char *toYellow (char *s)
{
	static	char	buf[20];

	Q_strncpyz (buf, s, sizeof(buf));
	for (s = buf ; *s ; s++)
		if (*s >= '0' && *s <= '9')
			*s = *s - '0' + 18;

	return buf;
}

extern	int	key_insert;

void M_Files_Draw (char *title)
{
	int		i, y;
	direntry_t	*d;
	char		str[29];

	M_Print (140, 8, title);
	M_Print (8, 24, "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1f");

	d = filelist + demo_base;
	for (i = 0, y = 32 ; i < num_files - demo_base && i < MAXLINES ; i++, y += 8, d++)
	{
		Q_strncpyz (str, d->name, sizeof(str));
		if (d->type)
			M_PrintWhite (24, y, str);
		else
			M_Print (24, y, str);

		if (d->type == 1)
			M_PrintWhite (256, y, "folder");
		else if (d->type == 2)
			M_PrintWhite (256, y, "  up  ");
		else if (d->type == 0)
			M_Print (256, y, toYellow(va("%5ik", d->size >> 10)));
	}
	
	M_DrawCharacter (8, 32 + demo_cursor*8, 12+((int)(realtime*4)&1));

	if (searchbox)
	{
		M_PrintWhite (24, 48 + 8*MAXLINES, "search: ");
		M_DrawTextBox (80, 40 + 8*MAXLINES, 16, 1);
		M_PrintWhite (88, 48 + 8*MAXLINES, searchfile);

		M_DrawCharacter (88 + 8*strlen(searchfile), 48 + 8*MAXLINES, ((int)(realtime*4)&1) ? 11+(84*key_insert) : 10);
	}
}

static void KillSearchBox (void)
{
	searchbox = false;
	memset (searchfile, 0, sizeof(searchfile));
	globctr = 0;
}

void M_Files_Key (int k)
{
	int		i;
	qboolean	worx;

	switch (k)
	{
	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		if (demo_cursor > 0)
			demo_cursor--;
		else if (demo_base > 0)
			demo_base--;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		if (demo_cursor+demo_base < num_files-1)
		{
			if (demo_cursor < MAXLINES-1)
				demo_cursor++;
			else
				demo_base++;
		}
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor = 0;
		demo_base = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		if (num_files > MAXLINES)
		{
			demo_cursor = MAXLINES-1;
			demo_base = num_files - demo_cursor - 1;
		}
		else
		{
			demo_base = 0;
			demo_cursor = num_files-1;
		}
		break;

	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor -= MAXLINES-1;
		if (demo_cursor < 0)
		{
			demo_base += demo_cursor;
			if (demo_base < 0)
				demo_base = 0;
			demo_cursor = 0;
		}
		break;

	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor += MAXLINES-1;
		if (demo_base + demo_cursor >= num_files)
			demo_cursor = num_files - demo_base - 1;
		if (demo_cursor >= MAXLINES)
		{
			demo_base += demo_cursor - (MAXLINES-1);
			demo_cursor = MAXLINES-1;
			if (demo_base + demo_cursor >= num_files)
				demo_base = num_files - demo_cursor - 1;
		}
		break;

	case K_BACKSPACE:
		if (strcmp(searchfile, ""))
			searchfile[--globctr] = 0;
		break;

	default:
		if (k < 32 || k > 127)
			break;
		searchbox = true;
		searchfile[globctr++] = k;
		worx = false;
		for (i=0 ; i<num_files ; i++)
		{
			if (strstr(filelist[i].name, searchfile) == filelist[i].name)
			{
				worx = true;
				S_LocalSound ("misc/menu1.wav");
				demo_base = i - 10;
				if (demo_base < 0)
				{
					demo_base = 0;
					demo_cursor = i;
				}
				else if (demo_base > (num_files - MAXLINES))
				{
					demo_base = num_files - MAXLINES;
					demo_cursor = MAXLINES - (num_files - i);
				}
				else
					demo_cursor = 10;
				break;
			}
		}
		if (!worx)
			searchfile[--globctr] = 0;
		break;
	}
}

//=============================================================================
/* MAPS MENU */

void PrintSortedMaps (void)
{
	searchpath_t	*search;
	extern	void EraseDirEntries (void);

	EraseDirEntries ();
	pak_files = 0;

	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (!search->pack)
		{
			RDFlags |= (RD_STRIPEXT | RD_NOERASE);
			ReadDir (va("%s/maps", search->filename), "*.bsp");
		}
	}
	FindFilesInPak ("maps/*.bsp");

	PrintSortedFiles ();
}

void M_Menu_Maps_f (void)
{
	key_dest = key_menu;
	m_state = m_maps;
	m_entersound = true;

	PrintSortedMaps ();
}

void M_Maps_Draw (void)
{
	M_Files_Draw ("MAPS");
}

void M_Maps_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		if (searchbox)
			KillSearchBox ();
		else
			M_Menu_Main_f ();
		return;

	case K_ENTER:
	case K_MOUSE1:
		if (!num_files || filelist[demo_cursor+demo_base].type == 3)
			return;

		key_dest = key_game;
		m_state = m_none;
		if (sv.active)
			Cbuf_AddText (va("changelevel %s\n", filelist[demo_cursor+demo_base].name));
		else
			Cbuf_AddText (va("map %s\n", filelist[demo_cursor+demo_base].name));
		Q_strncpyz (prevdir, filelist[demo_cursor+demo_base].name, sizeof(prevdir));

		if (searchbox)
			KillSearchBox ();
		return;
	}

	M_Files_Key (k);
}

//=============================================================================
/* DEMOS MENU */

void PrintSortedDemos (void)
{
	RDFlags |= RD_MENU_DEMOS;

	if (!demodir[0])	
	{
		RDFlags |= RD_MENU_DEMOS_MAIN;
		ReadDir (com_basedir, "*");
	}
	else
	{
		ReadDir (va("%s%s", com_basedir, demodir), "*");
	}

	PrintSortedFiles ();
}

void M_Menu_Demos_f (void)
{
	key_dest = key_menu;
	m_state = m_demos;
	m_entersound = true;

	PrintSortedDemos ();
}

void M_Demos_Draw (void)
{
	M_Print (16, 16, demodir);
	M_Files_Draw ("DEMOS");
}

void M_Demos_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		if (searchbox)
		{
			KillSearchBox ();
		}
		else
		{
			Q_strncpyz (prevdir, filelist[demo_cursor+demo_base].name, sizeof(prevdir));
			M_Menu_Main_f ();
		}
		return;

	case K_ENTER:
	case K_MOUSE1:
		if (!num_files || filelist[demo_cursor+demo_base].type == 3)
			return;

		if (filelist[demo_cursor+demo_base].type)
		{
			if (filelist[demo_cursor+demo_base].type == 2)
			{
				char	*p;

				if ((p = strrchr(demodir, '/')))
				{
					Q_strncpyz (prevdir, p + 1, sizeof(prevdir));
					*p = 0;
				}
			}
			else
			{
				strncat (demodir, va("/%s", filelist[demo_cursor+demo_base].name), sizeof(demodir)-1);
			}
			PrintSortedDemos ();
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
			Cbuf_AddText (va("playdemo \"..%s/%s\"\n", demodir, filelist[demo_cursor+demo_base].name));
			Q_strncpyz (prevdir, filelist[demo_cursor+demo_base].name, sizeof(prevdir));
		}

		if (searchbox)
			KillSearchBox ();
		return;
	}

	M_Files_Key (k);
}

//=============================================================================
/* QUIT MENU */

int		m_quit_prevstate;
qboolean	wasInMenus;

void M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
}

void M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
		}
		break;

	case K_ENTER:
	case K_MOUSE1:
	case 'Y':
	case 'y':
		key_dest = key_console;
		Host_Quit ();
		break;

	default:
		break;
	}

}

void doLogo (int left, int top, float alpha);
void M_Quit_Draw (void)
{
	static char *quitmsg[] = 
	{
		"0",	
		"0",
		"0",
		"0",
		"0",
		"0",
		"0",
		"0",
		"0",
		"0",
		"1http://www.quakeone.com/qrack",
		"0",
		"0",
		"1",
		"1Based on the GPL open source",
		"1release of Quake(r)",
		"1(c)1996 Id Software,Inc.",
		"1All rights reserved.",
		"0",
		"0Press 'Y' to quit.",
		"0",
		NULL
	};

	char	**p;
	int	x, y;
#ifdef GLQUAKE
	if (scr_scalemenu.value)
	{
		menuwidth = 320;
		menuheight = max(menuwidth * vid.aspect,200);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity ();
		glOrtho  (0, menuwidth, menuheight, 0, -99999, 99999);
	} 
#endif
	M_DrawTextBox (0, 4, 38, 22);
	y = 16;
	for (p = quitmsg ; *p ; p++, y += 8)
	{
		char	*c = *p;

		x = 16 + (36 - (strlen(c + 1))) * 4;
		if (*c == '2')
		{
			c++;
			while (*c != '0' && *c != '1')
			{
				M_DrawCharacter (x, y, *c++ | 128);
				x += 8;
			}
		}

		if (*c == '0')
			M_PrintWhite (x, y, c + 1);
		else
			M_Print (x, y, c + 1);
	}
	doLogo (32, 30, 1);//R00k: todo, if not using 24bit gfx then display 8bit logo.
#ifdef GLQUAKE
	if (scr_scalemenu.value)
	{
		glMatrixMode (GL_PROJECTION);
		glLoadIdentity ();
		glOrtho (0, vid.width, vid.height, 0, -99999, 99999);
	}
#endif
}

void M_Menu_Update_f (void)
{
	if (m_state == m_update)
		return;

	key_dest = key_menu;
	m_state = m_update;
	m_entersound = true;
}

void M_Update_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	default:
		break;
	}
}

void M_Update_Draw (void)
{
	static char *updatemsg[] = 
	{
		"0",	
		"0",
		"0",
		"0",
		"0",
		"0",
		"0",
		"0",
		"0",
		"1http://www.quakeone.com/qrack",
		"1",
		"0",
		"0A new version is available",
		"0",
		"0",
		"0",
		"0Press 'esc' to continue.",
		"0",
		NULL
	};

	char	**p;
	int	x, y;
#ifdef GLQUAKE
	if (!(scr_scalemenu.value) )
	{
		menuwidth = 320;
		menuheight = max(menuwidth * vid.aspect,200);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity ();
		glOrtho  (0, menuwidth, menuheight, 0, -99999, 99999);
	} 
#endif
	M_DrawTextBox (0, 4, 38, 18);
	y = 16;
	for (p = updatemsg ; *p ; p++, y += 8)
	{
		char	*c = *p;

		x = 16 + (36 - (strlen(c + 1))) * 4;
		if (*c == '2')
		{
			c++;
			while (*c != '0' && *c != '1')
			{
				M_DrawCharacter (x, y, *c++ | 128);
				x += 8;
			}
		}

		if (*c == '0')
			M_PrintWhite (x, y, c + 1);
		else
			M_Print (x, y, c + 1);
	}
	doLogo (32, 30, 1);
#ifdef GLQUAKE
	if (!(scr_scalemenu.value))
	{
		glMatrixMode (GL_PROJECTION);
		glLoadIdentity ();
		glOrtho (0, vid.width, vid.height, 0, -99999, 99999);
	}
#endif
}

//=============================================================================
/* SERIAL CONFIG MENU */
/*
int	serialConfig_cursor;
int	serialConfig_cursor_table[] = {48, 64, 80, 96, 112, 132};
#define	NUM_SERIALCONFIG_CMDS	6

static	int	ISA_uarts[] = {0x3f8, 0x2f8, 0x3e8, 0x2e8};
static	int	ISA_IRQs[] = {4, 3, 4, 3};
int		serialConfig_baudrate[] = {9600, 14400, 19200, 28800, 38400, 57600};

int	serialConfig_comport;
int	serialConfig_irq ;
int	serialConfig_baud;
char	serialConfig_phone[16];

void M_Menu_SerialConfig_f (void)
{
	int		n, port, baudrate;
	qboolean	useModem;

	key_dest = key_menu;
	m_state = m_serialconfig;
	m_entersound = true;
//	if (JoiningGame && SerialConfig)
//		serialConfig_cursor = 4;
//	else
///		serialConfig_cursor = 5;

//	(*GetComPortConfig)(0, &port, &serialConfig_irq, &baudrate, &useModem);

	// map uart's port to COMx
	for (n=0 ; n<4 ; n++)
		if (ISA_uarts[n] == port)
			break;
	if (n == 4)
	{
		n = 0;
		serialConfig_irq = 4;
	}
	serialConfig_comport = n + 1;

	// map baudrate to index
	for (n=0 ; n<6 ; n++)
		if (serialConfig_baudrate[n] == baudrate)
			break;
	if (n == 6)
		n = 5;
	serialConfig_baud = n;

	m_return_onerror = false;
	m_return_reason[0] = 0;
}

void M_SerialConfig_Draw (void)
{
	mpic_t	*p;
	int	basex;
	char	*startJoin, *directModem;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	if (SerialConfig)
		directModem = "Modem";
	else
		directModem = "Direct Connect";
	M_Print (basex, 32, va("%s - %s", startJoin, directModem));
	basex += 8;

	M_Print (basex, serialConfig_cursor_table[0], "Port");
	M_DrawTextBox (160, 40, 4, 1);
	M_Print (168, serialConfig_cursor_table[0], va("COM%u", serialConfig_comport));

	M_Print (basex, serialConfig_cursor_table[1], "IRQ");
	M_DrawTextBox (160, serialConfig_cursor_table[1]-8, 1, 1);
	M_Print (168, serialConfig_cursor_table[1], va("%u", serialConfig_irq));

	M_Print (basex, serialConfig_cursor_table[2], "Baud");
	M_DrawTextBox (160, serialConfig_cursor_table[2]-8, 5, 1);
	M_Print (168, serialConfig_cursor_table[2], va("%u", serialConfig_baudrate[serialConfig_baud]));

	if (SerialConfig)
	{
		M_Print (basex, serialConfig_cursor_table[3], "Modem Setup...");
		if (JoiningGame)
		{
			M_Print (basex, serialConfig_cursor_table[4], "Phone number");
			M_DrawTextBox (160, serialConfig_cursor_table[4]-8, 16, 1);
			M_Print (168, serialConfig_cursor_table[4], serialConfig_phone);
		}
	}

	if (JoiningGame)
	{
		M_DrawTextBox (basex, serialConfig_cursor_table[5]-8, 7, 1);
		M_Print (basex+8, serialConfig_cursor_table[5], "Connect");
	}
	else
	{
		M_DrawTextBox (basex, serialConfig_cursor_table[5]-8, 2, 1);
		M_Print (basex+8, serialConfig_cursor_table[5], "OK");
	}

	M_DrawCharacter (basex-8, serialConfig_cursor_table [serialConfig_cursor], 12+((int)(realtime*4)&1));

	if (serialConfig_cursor == 4)
		M_DrawCharacter (168 + 8*strlen(serialConfig_phone), serialConfig_cursor_table [serialConfig_cursor], 10+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (basex, 148, m_return_reason);
}

void M_SerialConfig_Key (int key)
{
	int	l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		serialConfig_cursor--;
		if (serialConfig_cursor < 0)
			serialConfig_cursor = NUM_SERIALCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		serialConfig_cursor++;
		if (serialConfig_cursor >= NUM_SERIALCONFIG_CMDS)
			serialConfig_cursor = 0;
		break;

	case K_LEFTARROW:
		if (serialConfig_cursor > 2)
			break;
		S_LocalSound ("misc/menu3.wav");

		if (serialConfig_cursor == 0)
		{
			serialConfig_comport--;
			if (serialConfig_comport == 0)
				serialConfig_comport = 4;
			serialConfig_irq = ISA_IRQs[serialConfig_comport-1];
		}

		if (serialConfig_cursor == 1)
		{
			serialConfig_irq--;
			if (serialConfig_irq == 6)
				serialConfig_irq = 5;
			if (serialConfig_irq == 1)
				serialConfig_irq = 7;
		}

		if (serialConfig_cursor == 2)
		{
			serialConfig_baud--;
			if (serialConfig_baud < 0)
				serialConfig_baud = 5;
		}

		break;

	case K_RIGHTARROW:
		if (serialConfig_cursor > 2)
			break;
forward:
		S_LocalSound ("misc/menu3.wav");

		if (serialConfig_cursor == 0)
		{
			serialConfig_comport++;
			if (serialConfig_comport > 4)
				serialConfig_comport = 1;
			serialConfig_irq = ISA_IRQs[serialConfig_comport-1];
		}

		if (serialConfig_cursor == 1)
		{
			serialConfig_irq++;
			if (serialConfig_irq == 6)
				serialConfig_irq = 7;
			if (serialConfig_irq == 8)
				serialConfig_irq = 2;
		}

		if (serialConfig_cursor == 2)
		{
			serialConfig_baud++;
			if (serialConfig_baud > 5)
				serialConfig_baud = 0;
		}

		break;

	case K_ENTER:
		if (serialConfig_cursor < 3)
			goto forward;

		m_entersound = true;

		if (serialConfig_cursor == 3)
		{
			(*SetComPortConfig) (0, ISA_uarts[serialConfig_comport-1], serialConfig_irq, serialConfig_baudrate[serialConfig_baud], SerialConfig);

			M_Menu_ModemConfig_f ();
			break;
		}

		if (serialConfig_cursor == 4)
		{
			serialConfig_cursor = 5;
			break;
		}

		// serialConfig_cursor == 5 (OK/CONNECT)
		(*SetComPortConfig) (0, ISA_uarts[serialConfig_comport-1], serialConfig_irq, serialConfig_baudrate[serialConfig_baud], SerialConfig);

		M_ConfigureNetSubsystem ();

		if (StartingGame)
		{
			M_Menu_GameOptions_f ();
			break;
		}

		m_return_state = m_state;
		m_return_onerror = true;
		key_dest = key_game;
		m_state = m_none;

		if (SerialConfig)
			Cbuf_AddText (va ("connect \"%s\"\n", serialConfig_phone));
		else
			Cbuf_AddText ("connect\n");
		break;

	case K_BACKSPACE:
		if (serialConfig_cursor == 4)
		{
			if (strlen(serialConfig_phone))
				serialConfig_phone[strlen(serialConfig_phone)-1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;
		if (serialConfig_cursor == 4)
		{
			l = strlen(serialConfig_phone);
			if (l < 15)
			{
				serialConfig_phone[l+1] = 0;
				serialConfig_phone[l] = key;
			}
		}
	}

	if (DirectConfig && (serialConfig_cursor == 3 || serialConfig_cursor == 4))
		serialConfig_cursor = (key == K_UPARROW) ? 2 : 5;

	if (SerialConfig && StartingGame && serialConfig_cursor == 4)
		serialConfig_cursor = (key == K_UPARROW) ? 3 : 5;
}
*/
//=============================================================================
/* MODEM CONFIG MENU */
/*
int	modemConfig_cursor;
int	modemConfig_cursor_table [] = {40, 56, 88, 120, 156};
#define NUM_MODEMCONFIG_CMDS	5

char	modemConfig_dialing;
char	modemConfig_clear [16];
char	modemConfig_init [32];
char	modemConfig_hangup [16];

void M_Menu_ModemConfig_f (void)
{
	key_dest = key_menu;
	m_state = m_modemconfig;
	m_entersound = true;
	(*GetModemConfig) (0, &modemConfig_dialing, modemConfig_clear, modemConfig_init, modemConfig_hangup);
}

void M_ModemConfig_Draw (void)
{
	mpic_t	*p;
	int	basex;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, p);
	basex += 8;

	if (modemConfig_dialing == 'P')
		M_Print (basex, modemConfig_cursor_table[0], "Pulse Dialing");
	else
		M_Print (basex, modemConfig_cursor_table[0], "Touch Tone Dialing");

	M_Print (basex, modemConfig_cursor_table[1], "Clear");
	M_DrawTextBox (basex, modemConfig_cursor_table[1]+4, 16, 1);
	M_Print (basex+8, modemConfig_cursor_table[1]+12, modemConfig_clear);
	if (modemConfig_cursor == 1)
		M_DrawCharacter (basex+8 + 8*strlen(modemConfig_clear), modemConfig_cursor_table[1]+12, 10+((int)(realtime*4)&1));

	M_Print (basex, modemConfig_cursor_table[2], "Init");
	M_DrawTextBox (basex, modemConfig_cursor_table[2]+4, 30, 1);
	M_Print (basex+8, modemConfig_cursor_table[2]+12, modemConfig_init);
	if (modemConfig_cursor == 2)
		M_DrawCharacter (basex+8 + 8*strlen(modemConfig_init), modemConfig_cursor_table[2]+12, 10+((int)(realtime*4)&1));

	M_Print (basex, modemConfig_cursor_table[3], "Hangup");
	M_DrawTextBox (basex, modemConfig_cursor_table[3]+4, 16, 1);
	M_Print (basex+8, modemConfig_cursor_table[3]+12, modemConfig_hangup);
	if (modemConfig_cursor == 3)
		M_DrawCharacter (basex+8 + 8*strlen(modemConfig_hangup), modemConfig_cursor_table[3]+12, 10+((int)(realtime*4)&1));

	M_DrawTextBox (basex, modemConfig_cursor_table[4]-8, 2, 1);
	M_Print (basex+8, modemConfig_cursor_table[4], "OK");

	M_DrawCharacter (basex-8, modemConfig_cursor_table [modemConfig_cursor], 12+((int)(realtime*4)&1));
}

void M_ModemConfig_Key (int key)
{
	int	l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_SerialConfig_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		modemConfig_cursor--;
		if (modemConfig_cursor < 0)
			modemConfig_cursor = NUM_MODEMCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		modemConfig_cursor++;
		if (modemConfig_cursor >= NUM_MODEMCONFIG_CMDS)
			modemConfig_cursor = 0;
		break;

	case K_LEFTARROW:
	case K_RIGHTARROW:
		if (modemConfig_cursor == 0)
		{
			if (modemConfig_dialing == 'P')
				modemConfig_dialing = 'T';
			else
				modemConfig_dialing = 'P';
			S_LocalSound ("misc/menu1.wav");
		}
		break;

	case K_ENTER:
		if (modemConfig_cursor == 0)
		{
			if (modemConfig_dialing == 'P')
				modemConfig_dialing = 'T';
			else
				modemConfig_dialing = 'P';
			m_entersound = true;
		}

		if (modemConfig_cursor == 4)
		{
			(*SetModemConfig) (0, va ("%c", modemConfig_dialing), modemConfig_clear, modemConfig_init, modemConfig_hangup);
			m_entersound = true;
			M_Menu_SerialConfig_f ();
		}
		break;

	case K_BACKSPACE:
		if (modemConfig_cursor == 1)
		{
			if (strlen(modemConfig_clear))
				modemConfig_clear[strlen(modemConfig_clear)-1] = 0;
		}

		if (modemConfig_cursor == 2)
		{
			if (strlen(modemConfig_init))
				modemConfig_init[strlen(modemConfig_init)-1] = 0;
		}

		if (modemConfig_cursor == 3)
		{
			if (strlen(modemConfig_hangup))
				modemConfig_hangup[strlen(modemConfig_hangup)-1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (modemConfig_cursor == 1)
		{
			l = strlen(modemConfig_clear);
			if (l < 15)
			{
				modemConfig_clear[l+1] = 0;
				modemConfig_clear[l] = key;
			}
		}

		if (modemConfig_cursor == 2)
		{
			l = strlen(modemConfig_init);
			if (l < 29)
			{
				modemConfig_init[l+1] = 0;
				modemConfig_init[l] = key;
			}
		}

		if (modemConfig_cursor == 3)
		{
			l = strlen(modemConfig_hangup);
			if (l < 15)
			{
				modemConfig_hangup[l+1] = 0;
				modemConfig_hangup[l] = key;
			}
		}
	}
}
*/
//=============================================================================
/* LAN CONFIG MENU */

int	lanConfig_cursor = -1;
int	lanConfig_cursor_table [] = {72, 92, 124};
#define NUM_LANCONFIG_CMDS	3

int 	lanConfig_port;
char	lanConfig_portname[6];
char	lanConfig_joinname[22];

void M_Menu_LanConfig_f (void)
{
	key_dest = key_menu;
	m_state = m_lanconfig;
	m_entersound = true;
	if (lanConfig_cursor == -1)
	{
		if (JoiningGame && TCPIPConfig)
			lanConfig_cursor = 2;
		else
			lanConfig_cursor = 1;
	}
	if (StartingGame && lanConfig_cursor == 2)
		lanConfig_cursor = 1;
	lanConfig_port = DEFAULTnet_hostport;
	sprintf (lanConfig_portname, "%u", lanConfig_port);

	m_return_onerror = false;
	m_return_reason[0] = 0;
}

void M_LanConfig_Draw (void)
{
	mpic_t	*p;
	int	basex;
	char	*startJoin, *protocol;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	if (IPXConfig)
		protocol = "IPX";
	else
		protocol = "TCP/IP";
	M_Print (basex, 32, va ("%s - %s", startJoin, protocol));
	basex += 8;

	M_Print (basex, 52, "Address:");
	if (IPXConfig)
		M_Print (basex+9*8, 52, my_ipx_address);
	else
		M_Print (basex+9*8, 52, my_tcpip_address);

	M_Print (basex, lanConfig_cursor_table[0], "Port");
	M_DrawTextBox (basex+8*8, lanConfig_cursor_table[0]-8, 6, 1);
	M_Print (basex+9*8, lanConfig_cursor_table[0], lanConfig_portname);

	if (JoiningGame)
	{
		M_Print (basex, lanConfig_cursor_table[1], "Search for local games...");
		M_Print (basex, 108, "Join game at:");
		M_DrawTextBox (basex+8, lanConfig_cursor_table[2]-8, 22, 1);
		M_Print (basex+16, lanConfig_cursor_table[2], lanConfig_joinname);
	}
	else
	{
		M_DrawTextBox (basex, lanConfig_cursor_table[1]-8, 2, 1);
		M_Print (basex+8, lanConfig_cursor_table[1], "OK");
	}

	M_DrawCharacter (basex-8, lanConfig_cursor_table [lanConfig_cursor], 12+((int)(realtime*4)&1));

	if (lanConfig_cursor == 0)
		M_DrawCharacter (basex+9*8 + 8*strlen(lanConfig_portname), lanConfig_cursor_table [0], 10+((int)(realtime*4)&1));

	if (lanConfig_cursor == 2)
		M_DrawCharacter (basex+16 + 8*strlen(lanConfig_joinname), lanConfig_cursor_table [2], 10+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (basex, 148, m_return_reason);
}

void M_LanConfig_Key (int key)
{
	int		l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor--;
		if (lanConfig_cursor < 0)
			lanConfig_cursor = NUM_LANCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor++;
		if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
			lanConfig_cursor = 0;
		break;

	case K_SPACE:
	case K_ENTER:
	case K_MOUSE1:
		if (lanConfig_cursor == 0)
			break;

		m_entersound = true;

		M_ConfigureNetSubsystem ();

		if (lanConfig_cursor == 1)
		{
			if (StartingGame)
			{
				M_Menu_GameOptions_f ();
				break;
			}
			M_Menu_Search_f();
			break;
		}

		if (lanConfig_cursor == 2)
		{
			m_return_state = m_state;
			m_return_onerror = true;
			key_dest = key_game;
			m_state = m_none;
			Cbuf_AddText ( va ("connect \"%s\"\n", lanConfig_joinname) );
			break;
		}

		break;

	case K_BACKSPACE:
		if (lanConfig_cursor == 0)
		{
			if (strlen(lanConfig_portname))
				lanConfig_portname[strlen(lanConfig_portname)-1] = 0;
		}

		if (lanConfig_cursor == 2)
		{
			if (strlen(lanConfig_joinname))
				lanConfig_joinname[strlen(lanConfig_joinname)-1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (lanConfig_cursor == 2)
		{
			l = strlen(lanConfig_joinname);
			if (l < 21)
			{
				lanConfig_joinname[l+1] = 0;
				lanConfig_joinname[l] = key;
			}
		}

		if (key < '0' || key > '9')
			break;
		if (lanConfig_cursor == 0)
		{
			l = strlen(lanConfig_portname);
			if (l < 5)
			{
				lanConfig_portname[l+1] = 0;
				lanConfig_portname[l] = key;
			}
		}
	}

	if (StartingGame && lanConfig_cursor == 2)
		lanConfig_cursor = (key == K_UPARROW) ? 1 : 0;

	l = Q_atoi(lanConfig_portname);
	if (l > 65535)
		l = lanConfig_port;
	else
		lanConfig_port = l;
	sprintf (lanConfig_portname, "%u", lanConfig_port);
}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct
{
	char	*name;
	char	*description;
} level_t;

level_t	levels[] =
{
	{"start", "Entrance"},	// 0

	{"e1m1", "Slipgate Complex"},				// 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"},				// 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"},			// 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"},				// 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"},			// 31

	{"dm1", "Place of Two Deaths"},				// 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}
};

//MED 01/06/97 added hipnotic levels
level_t	hipnoticlevels[] =
{
	{"start", "Command HQ"},	// 0

	{"hip1m1", "The Pumping Station"},		// 1
	{"hip1m2", "Storage Facility"},
	{"hip1m3", "The Lost Mine"},
	{"hip1m4", "Research Facility"},
	{"hip1m5", "Military Complex"},

	{"hip2m1", "Ancient Realms"},			// 6
	{"hip2m2", "The Black Cathedral"},
	{"hip2m3", "The Catacombs"},
	{"hip2m4", "The Crypt"},
	{"hip2m5", "Mortum's Keep"},
	{"hip2m6", "The Gremlin's Domain"},

	{"hip3m1", "Tur Torment"},			// 12
	{"hip3m2", "Pandemonium"},
	{"hip3m3", "Limbo"},
	{"hip3m4", "The Gauntlet"},

	{"hipend", "Armagon's Lair"},			// 16

	{"hipdm1", "The Edge of Oblivion"}		// 17
};

//PGM 01/07/97 added rogue levels
//PGM 03/02/97 added dmatch level
level_t	roguelevels[] =
{
	{"start", "Split Decision"},
	{"r1m1", "Deviant's Domain"},
	{"r1m2", "Dread Portal"},
	{"r1m3", "Judgement Call"},
	{"r1m4", "Cave of Death"},
	{"r1m5", "Towers of Wrath"},
	{"r1m6", "Temple of Pain"},
	{"r1m7", "Tomb of the Overlord"},
	{"r2m1", "Tempus Fugit"},
	{"r2m2", "Elemental Fury I"},
	{"r2m3", "Elemental Fury II"},
	{"r2m4", "Curse of Osiris"},
	{"r2m5", "Wizard's Keep"},
	{"r2m6", "Blood Sacrifice"},
	{"r2m7", "Last Bastion"},
	{"r2m8", "Source of Evil"},
	{"ctf1", "Division of Change"}
};

typedef struct
{
	char	*description;
	int		firstLevel;
	int		levels;
} episode_t;

episode_t episodes[] =
{
	{"Welcome to Quake", 0, 1},
	{"Doomed Dimension", 1, 8},
	{"Realm of Black Magic", 9, 7},
	{"Netherworld", 16, 7},
	{"The Elder World", 23, 8},
	{"Final Level", 31, 1},
	{"Deathmatch Arena", 32, 6}
};

//MED 01/06/97  added hipnotic episodes
episode_t hipnoticepisodes[] =
{
	{"Scourge of Armagon", 0, 1},
	{"Fortress of the Dead", 1, 5},
	{"Dominion of Darkness", 6, 6},
	{"The Rift", 12, 4},
	{"Final Level", 16, 1},
	{"Deathmatch Arena", 17, 1}
};

//PGM 01/07/97 added rogue episodes
//PGM 03/02/97 added dmatch episode
episode_t rogueepisodes[] =
{
	{"Introduction", 0, 1},
	{"Hell's Fortress", 1, 7},
	{"Corridors of Time", 8, 8},
	{"Deathmatch Arena", 16, 1}
};

int	startepisode;
int	startlevel;
int	maxplayers;
qboolean m_serverInfoMessage = false;
double	m_serverInfoMessageTime;

void M_Menu_GameOptions_f (void)
{
	key_dest = key_menu;
	m_state = m_gameoptions;
	m_entersound = true;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = svs.maxclientslimit;
}

int	gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 88, 96, 112, 120};
#define	NUM_GAMEOPTIONS	9
int	gameoptions_cursor;

void M_GameOptions_Draw (void)
{
	mpic_t	*p;
	int	x;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320-p->width)/2, 4, p);

	M_DrawTextBox (152, 32, 10, 1);
	M_Print (160, 40, "begin game");

	M_Print (0, 56, "      Max players");
	M_Print (160, 56, va("%i", maxplayers));

	M_Print (0, 64, "        Game Type");
	if (coop.value)
		M_Print (160, 64, "Cooperative");
	else
		M_Print (160, 64, "Deathmatch");

	M_Print (0, 72, "        Teamplay");
	if (rogue)
	{
		char	*msg;

		switch ((int)teamplay.value)
		{
			case 1: msg = "No Friendly Fire"; break;
			case 2: msg = "Friendly Fire"; break;
			case 3: msg = "Tag"; break;
			case 4: msg = "Capture the Flag"; break;
			case 5: msg = "One Flag CTF"; break;
			case 6: msg = "Three Team CTF"; break;
			default: msg = "Off"; break;
		}
		M_Print (160, 72, msg);
	}
	else
	{
		char	*msg;

		switch ((int)teamplay.value)
		{
			case 1: msg = "No Friendly Fire"; break;
			case 2: msg = "Friendly Fire"; break;
			default: msg = "Off"; break;
		}
		M_Print (160, 72, msg);
	}

	M_Print (0, 80, "            Skill");
	if (skill.value == 0)
		M_Print (160, 80, "Easy difficulty");
	else if (skill.value == 1)
		M_Print (160, 80, "Normal difficulty");
	else if (skill.value == 2)
		M_Print (160, 80, "Hard difficulty");
	else
		M_Print (160, 80, "Nightmare difficulty");

	M_Print (0, 88, "       Frag Limit");
	if (fraglimit.value == 0)
		M_Print (160, 88, "none");
	else
		M_Print (160, 88, va("%i frags", (int)fraglimit.value));

	M_Print (0, 96, "       Time Limit");
	if (timelimit.value == 0)
		M_Print (160, 96, "none");
	else
		M_Print (160, 96, va("%i minutes", (int)timelimit.value));

	M_Print (0, 112, "         Episode");

//MED 01/06/97 added hipnotic episodes
	if (hipnotic)
		M_Print (160, 112, hipnoticepisodes[startepisode].description);
//PGM 01/07/97 added rogue episodes
	else if (rogue)
		M_Print (160, 112, rogueepisodes[startepisode].description);
	else
		M_Print (160, 112, episodes[startepisode].description);

	M_Print (0, 120, "           Level");

//MED 01/06/97 added hipnotic episodes
	if (hipnotic)
	{
		M_Print (160, 120, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].description);
		M_Print (160, 128, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name);
	}
//PGM 01/07/97 added rogue episodes
	else if (rogue)
	{
		M_Print (160, 120, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].description);
		M_Print (160, 128, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name);
	}
	else
	{
		M_Print (160, 120, levels[episodes[startepisode].firstLevel + startlevel].description);
		M_Print (160, 128, levels[episodes[startepisode].firstLevel + startlevel].name);
	}

// line cursor
	M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(realtime*4)&1));

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) < 5.0)
		{
			x = (320-26*8)/2;
			M_DrawTextBox (x, 138, 24, 4);
			x += 8;
			M_Print (x, 146, "  More than 4 players   ");
			M_Print (x, 154, " requires using command ");
			M_Print (x, 162, "line parameters; please ");
			M_Print (x, 170, "   see techinfo.txt.    ");
		}
		else
		{
			m_serverInfoMessage = false;
		}
	}
}

void M_NetStart_Change (int dir)
{
	int	count;

	switch (gameoptions_cursor)
	{
	case 1:
		maxplayers += dir;
		if (maxplayers > svs.maxclientslimit)
		{
			maxplayers = svs.maxclientslimit;
			m_serverInfoMessage = true;
			m_serverInfoMessageTime = realtime;
		}
		if (maxplayers < 2)
			maxplayers = 2;
		break;

	case 2:
		Cvar_SetValue ("coop", coop.value ? 0 : 1);
		break;

	case 3:
		if (rogue)
			count = 6;
		else
			count = 2;

		Cvar_SetValue ("teamplay", teamplay.value + dir);
		if (teamplay.value > count)
			Cvar_Set ("teamplay", "0");
		else if (teamplay.value < 0)
			Cvar_SetValue ("teamplay", count);
		break;

	case 4:
		Cvar_SetValue ("skill", skill.value + dir);
		if (skill.value > 3)
			Cvar_Set ("skill", "0");
		if (skill.value < 0)
			Cvar_Set ("skill", "3");
		break;

	case 5:
		Cvar_SetValue ("fraglimit", fraglimit.value + dir*10);
		if (fraglimit.value > 100)
			Cvar_Set ("fraglimit", "0");
		if (fraglimit.value < 0)
			Cvar_Set ("fraglimit", "100");
		break;

	case 6:
		Cvar_SetValue ("timelimit", timelimit.value + dir*5);
		if (timelimit.value > 60)
			Cvar_Set ("timelimit", "0");
		if (timelimit.value < 0)
			Cvar_Set ("timelimit", "60");
		break;

	case 7:
		startepisode += dir;
	//MED 01/06/97 added hipnotic count
		if (hipnotic)
			count = 6;
	//PGM 01/07/97 added rogue count
	//PGM 03/02/97 added 1 for dmatch episode
		else if (rogue)
			count = 4;
		else if (registered.value)
			count = 7;
		else
			count = 2;

		if (startepisode < 0)
			startepisode = count - 1;

		if (startepisode >= count)
			startepisode = 0;

		startlevel = 0;
		break;

	case 8:
		startlevel += dir;
	//MED 01/06/97 added hipnotic episodes
		if (hipnotic)
			count = hipnoticepisodes[startepisode].levels;
	//PGM added rogue episodes
		else if (rogue)
			count = rogueepisodes[startepisode].levels;
		else
			count = episodes[startepisode].levels;

		if (startlevel < 0)
			startlevel = count - 1;

		if (startlevel >= count)
			startlevel = 0;
		break;
	}
}

void M_GameOptions_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (1);
		break;

	case K_SPACE:
	case K_MOUSE1:
	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("listen 0\n");	// so host_netport will be re-examined
			Cbuf_AddText (va("maxplayers %u\n", maxplayers));
			SCR_BeginLoadingPlaque ();

			if (hipnotic)
				Cbuf_AddText (va("map %s\n", hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name));
			else if (rogue)
				Cbuf_AddText (va("map %s\n", roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name));
			else
				Cbuf_AddText (va("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name));

			return;
		}

		M_NetStart_Change (1);
		break;
	}
}

//=============================================================================
/* SEARCH MENU */

qboolean	searchComplete = false;
double		searchCompleteTime;

void M_Menu_Search_f (void)
{
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;
	slistSilent = true;
	slistLocal = false;
	searchComplete = false;
	NET_Slist_f ();
}

void M_Search_Draw (void)
{
	mpic_t	*p;
	int	x;

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	x = (320/2) - ((12*8)/2) + 4;
	M_DrawTextBox (x-8, 32, 12, 1);
	M_Print (x, 40, "Searching...");

	if (slistInProgress)
	{
		NET_Poll ();
		return;
	}

	if (!searchComplete)
	{
		searchComplete = true;
		searchCompleteTime = realtime;
	}

	if (hostCacheCount)
	{
		M_Menu_ServerList_f ();
		return;
	}

	M_PrintWhite ((320/2) - ((22*8)/2), 64, "No Quake servers found");
	if ((realtime - searchCompleteTime) < 3.0)
		return;

	M_Menu_LanConfig_f ();
}

void M_Search_Key (int key)
{
}

int	servers_cursor;
qboolean servers_sorted;

void M_Menu_FoundServers_f (void)
{
	key_dest = key_menu;
	m_state = m_servers;
	m_entersound = true;

	servers_cursor = 0;
	m_return_onerror = false;
	m_return_reason[0] = 0;
	servers_sorted = false;
}

void M_FoundServers_Draw (void)
{
	int	n;
	char	string[64];
	mpic_t	*p;

	if (!servers_sorted)
	{
		if (hostCacheCount > 1)
		{
			int		i, j;
			hostcache_t	temp;

			for (i=0 ; i<hostCacheCount ; i++)
			{
				for (j=i+1 ; j<hostCacheCount ; j++)
				{
					if (strcmp(hostcache[j].name, hostcache[i].name) < 0)
					{
						memcpy (&temp, &hostcache[j], sizeof(hostcache_t));
						memcpy (&hostcache[j], &hostcache[i], sizeof(hostcache_t));
						memcpy (&hostcache[i], &temp, sizeof(hostcache_t));
					}
				}
			}
		}
		servers_sorted = true;
	}

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);
	for (n=0 ; n<hostCacheCount ; n++)
	{
		if (hostcache[n].maxusers)
			sprintf (string, "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
		else
			sprintf (string, "%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
		M_Print (16, 32 + 8*n, string);
	}
	M_DrawCharacter (0, 32 + servers_cursor*8, 12+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (16, 148, m_return_reason);
}

void M_FoundServers_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_LanConfig_f ();
		break;

	case K_SPACE:
		M_Menu_Search_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		servers_cursor--;
		if (servers_cursor < 0)
			servers_cursor = hostCacheCount - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		servers_cursor++;
		if (servers_cursor >= hostCacheCount)
			servers_cursor = 0;
		break;

	case K_ENTER:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		m_return_state = m_state;
		m_return_onerror = true;
		servers_sorted = false;
		key_dest = key_game;
		m_state = m_none;
		Cbuf_AddText (va("connect \"%s\"\n", hostcache[servers_cursor].cname));
		break;

	default:
		break;
	}
}

//=============================================================================
/* SERVER LIST MENU */

#define	MENU_X	50
#define	MENU_Y	21
#define TITLE_Y 4
#define	STAT_Y	166
extern qboolean	keydown[256];
int	slist_cursor = 0, slist_mins = 0, slist_maxs = 15, slist_state;

#define	SERV_X	60
#define	SERV_Y	64
#define	DESC_X	60
#define	DESC_Y	40
#define	SERV_L	23
#define	DESC_L	23

#define	SLIST_BUFSIZE	128

static	char	slist_serv[SLIST_BUFSIZE], slist_desc[SLIST_BUFSIZE];
static	int		slist_serv_max, slist_serv_min, slist_desc_max, slist_desc_min, sedit_state;

playerinfo_t plist[16]; int numPlayers;
//------------------------------------------------------------------

extern	qboolean testInProgress;
extern	int		testPollCount;
extern	int		testDriver;
extern	int		testSocket;
extern	void	Strip_Port (char *ch);
extern	int		myDriverLevel;
extern	int		net_landriverlevel;
void	ServerList_Test_Poll (void);
static double stime;

PollProcedure	sltestPollProcedure = {NULL, 0.0, ServerList_Test_Poll};

void SList_SetPlist (int i, char *name, int frags, int team)
{
	if (i > 16 || i < 0)
		return;

	if (plist[i].name)
		Z_Free (plist[i].name);

	if (name)
	{
		plist[i].name = CopyString (name);
		plist[i].frags = frags;
		plist[i].bottom= team & 0x0f;
		numPlayers +=1;		
	}
}

static void ServerList_Test_Poll (void)
{
	struct qsockaddr clientaddr;
	int		control, len;
	char	name[32], address[64];
	int		colors, frags, connectTime;
	int		playerNumber;

	net_landriverlevel = testDriver;

	while (1)
	{
		len = net_landrivers[net_landriverlevel].Read(testSocket, net_message.data, net_message.maxsize, &clientaddr);
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
		{	
			Con_Printf ("Unexpected response to Player Info request\n");
			break;
		}

		playerNumber = MSG_ReadByte ();
		strcpy (name, MSG_ReadString());
		colors = MSG_ReadLong ();
		frags = MSG_ReadLong ();
		connectTime = MSG_ReadLong ();
		strcpy (address, MSG_ReadString());
		
		SList_SetPlist(playerNumber,name,frags, colors);
	}

	testPollCount--;
	if (testPollCount)
	{
		SchedulePollProcedure (&sltestPollProcedure, 0.1);
	}
	else
	{
		net_landrivers[net_landriverlevel].CloseSocket (testSocket);
		testInProgress = false;
	}
}

static void ServerList_Test_f (char *host)
{
	int		n, max = MAX_SCOREBOARD;
	struct qsockaddr sendaddr;

	if (testInProgress)
	{
		return;
	}

	if (!host)
	{
		return;
	}

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
		if (net_landrivers[net_landriverlevel].GetAddrFromName(host, &sendaddr) != -1)
			break;
	}
	if (net_landriverlevel == net_numlandrivers)
	{	
		return;
	}

JustDoIt:
	testSocket = net_landrivers[net_landriverlevel].OpenSocket(0);
	if (testSocket == -1)
	{	
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
		net_landrivers[net_landriverlevel].Write (testSocket, net_message.data, net_message.cursize, &sendaddr);
	}

	SZ_Clear (&net_message);
	SchedulePollProcedure (&sltestPollProcedure, 0.1);
}

static void M_ServerList_Test_f ()
{
	key_dest = key_menu;
	m_state = m_stest;
	m_entersound = true;		
	
	memset (&plist, 0, sizeof(plist));
	numPlayers = 0;
	
	ServerList_Test_f (slist[slist_cursor].server);	
}

char *get_teamcolor(int c)
{
	if ((0 & 0x0f) == c) 
	{
		return "white  ";
	}
	if ((1 & 0x0f) == c) 
	{
		return "brown  ";
	}
	if ((2 & 0x0f) == c) 
	{
		return "lt blue";
	}
	if ((3 & 0x0f) == c) 
	{
		return "green  ";
	}
	if ((4 & 0x0f) == c) 
	{
		return "red    ";
	}
	if ((5 & 0x0f) == c) 
	{
		return "olive  ";
	}
	if ((6 & 0x0f) == c) 
	{
		return "clay   ";
	}
	if ((7 & 0x0f) == c) 
	{
		return "flesh  ";
	}
	if ((8 & 0x0f) == c) 
	{
		return "grape  ";
	}
	if ((9 & 0x0f) == c) 
	{
		return "violet ";
	}
	if ((10 & 0x0f) == c) 
	{
		return "beige  ";
	}
	if ((11 & 0x0f) == c) 
	{
		return "aqua   ";
	}
	if ((12 & 0x0f) == c) 
	{
		return "yellow ";
	}
	if ((13 & 0x0f) == c) 
	{
		return "blue   ";
	}
	if ((14 & 0x0f) == c) 
	{
		return "salmon ";
	}
	if ((15 & 0x0f) == c) 
	{
		return "fire   ";
	}
	return NULL;
}

void M_STest_Draw (void)
{
	mpic_t	*p;
	char t[12];
	int l, o;
	
	o = 2;
	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);
	M_PrintWhite (MENU_X + 2, STAT_Y-16 ,va("%s",slist[slist_cursor].server));

	if (testInProgress)
	{
		M_PrintWhite (MENU_X + 2, STAT_Y+16 , "searching for players...");	

		NET_Poll ();
		return;
	}
	
	if (numPlayers)
	{
		M_PrintWhite (MENU_X + 2, MENU_Y + 12, "team    name            frags\n");	
		for (l = 0; l < MAX_PLAYERS; l++)
		{
			if (plist[l].name)
			{
				o++;
				sprintf(t, get_teamcolor(plist[l].bottom));
				M_Print (MENU_X + 2,	o * 8 + MENU_Y, va("%s", t));
				M_Print (MENU_X + 66,	o * 8 + MENU_Y, va("%s", plist[l].name));
				M_Print (MENU_X + 208,	o * 8 + MENU_Y, va("%i", plist[l].frags));
			}
		}	
	
		M_PrintWhite (MENU_X + 4, STAT_Y	,	"SPACE‘ to rescan ");
		M_PrintWhite (MENU_X + 4, STAT_Y + 8,	"Enter‘ to connect");
		M_PrintWhite (MENU_X + 4, STAT_Y + 20,	"ESC‘ Back");
	}
	else
	{
		M_Print      (MENU_X + 4, MENU_Y + 12,	"No Quakers found! ");
		M_PrintWhite (MENU_X + 4, STAT_Y	 ,	"SPACE‘ to rescan ");
		M_PrintWhite (MENU_X + 4, STAT_Y + 8 ,	"Enter‘ to connect");
		M_PrintWhite (MENU_X + 4, STAT_Y + 20 ,	"ESC‘ Back");
	}
}

void M_STest_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		m_state = m_slist;
		M_Menu_ServerList_f ();
		break;

	case K_ENTER:
	case K_MOUSE1:
		m_state = m_main;
		M_ToggleMenu_f ();
		Cbuf_AddText (va("connect \"%s\"\n", slist[slist_cursor].server));
		break;

	case K_SPACE:
	case 't':
	case 'T':			
		M_ServerList_Test_f();//Rescan
		break;

	default:
		M_Menu_ServerList_f ();
		break;
	}
}

//-----------------------------------------------------------------------
int M_Menu_UpdateServerList (void)
{
	char url[1024];
	qboolean success;

	extern int Web_Get( const char *url, const char *referer, const char *name, int resume, int max_downloading_time, int timeout, int ( *_progress )(double) );
	
	Q_snprintfz( url, sizeof( url ), "%s%s", SERVERLIST_URL, SERVERLIST_FILE );
	success = Web_Get( url, NULL, va("%s/qrack/servers.txt", com_basedir), false, 2, 2, NULL );

	if( !success )
	{
		Con_Printf("No Response From Master Server\n");
		return 0;
	}
	else
		return 1;
}

void M_Menu_ServerList_f (void)
{	
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;
	stime = 0;//reset
	slist_state = 0;
	
	if (!(SList_Load()))
	{
		if (M_Menu_UpdateServerList())//grab one off the qrack website if we dont have one.
		{
			Con_Printf("Downloading: quakeone.com/qrack/servers.txt\n");
			if (!(SList_Load()))//reload it!
			{
				Con_Printf("Can not find 'servers.txt' there either!\n");
			}
		}
	}
}

void M_ServerList_Draw (void)
{
	int	serv, line;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	M_DrawTextBox (MENU_X, TITLE_Y, 23, 1);
	M_PrintWhite (MENU_X + 60, TITLE_Y + 8, "Server List");

	if (!slist[0].server)
	{
		M_PrintWhite	(84, MENU_Y + 32, "Empty server list");
		M_Print			(60, MENU_Y + 48, "Press INS to add a server");
		M_Print			(64, MENU_Y + 56, "Press E to edit a server");
		return;
	}

	M_DrawTextBox (MENU_X, STAT_Y, 23, 1);
	M_DrawTextBox (MENU_X, MENU_Y, 23, slist_maxs - slist_mins + 1);
	for (serv = slist_mins, line = 1 ; serv <= slist_maxs && serv < MAX_SERVER_LIST && slist[serv].server ; serv++, line++)
		M_Print (MENU_X + 18, line * 8 + MENU_Y, va("%1.21s", slist[serv].description));
	M_Print (MENU_X - 36, STAT_Y - 4, "INS: add E: edit T: test DEL: Delete");
	M_PrintWhite (MENU_X + 18, STAT_Y + 8, va("%1.22s", slist[slist_cursor].server));
	M_DrawCharacter (MENU_X + 8, (slist_cursor - slist_mins + 1) * 8 + MENU_Y, 12+((int)(realtime*4)&1));
}

void M_ServerList_Key (key)
{
	int	slist_length;

	if (!slist[0].server && key != K_ESCAPE && key != K_INS)
		return;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		if (slist_cursor > 0)
		{
			if (keydown[K_CTRL])
				SList_Switch (slist_cursor, slist_cursor - 1);
			slist_cursor--;
		}
		break;

	case K_MWHEELDOWN:
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (keydown[K_CTRL])
		{
			if (slist_cursor != SList_Length() - 1)
			{
				SList_Switch (slist_cursor, slist_cursor + 1);
				slist_cursor++;
			}
		}
		else if (slist_cursor < MAX_SERVER_LIST - 1 && slist[slist_cursor+1].server)
			slist_cursor++;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor = SList_Length() - 1;
		break;
		
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor -= (slist_maxs - slist_mins);
		if (slist_cursor < 0)
			slist_cursor = 0;
		break;

	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor += (slist_maxs - slist_mins);
		if (slist_cursor >= MAX_SERVER_LIST)
			slist_cursor = MAX_SERVER_LIST - 1;
		while (!slist[slist_cursor].server)
			slist_cursor--;
		break;

	case K_ENTER:
	case K_MOUSE1:
		if (keydown[K_CTRL])
		{
			M_Menu_SEdit_f ();
			break;
		}
		m_state = m_main;
		M_ToggleMenu_f ();
		Cbuf_AddText (va("connect \"%s\"\n", slist[slist_cursor].server));
		break;

	case 'e':
	case 'E':
		M_Menu_SEdit_f ();
		break;

	case K_INS:
		S_LocalSound ("misc/menu2.wav");
		if ((slist_length = SList_Length()) < MAX_SERVER_LIST)
		{
			if (keydown[K_CTRL] && slist_length > 0)
			{
				if (slist_cursor < slist_length - 1)
					memmove (&slist[slist_cursor+2], &slist[slist_cursor+1], (slist_length - slist_cursor - 1) * sizeof(slist[0]));
				SList_Reset_NoFree (slist_cursor + 1);
				SList_Set (slist_cursor + 1, "127.0.0.1", "<BLANK>");
				if (slist_length)
					slist_cursor++;
			}
			else
			{
				memmove (&slist[slist_cursor+1], &slist[slist_cursor], (slist_length - slist_cursor) * sizeof(slist[0]));
				SList_Reset_NoFree (slist_cursor);
				SList_Set (slist_cursor, "127.0.0.1", "<BLANK>");
			}
		}
		break;

	case 't':
	case 'T':			
			M_ServerList_Test_f();
		break;

	case K_DEL:
		S_LocalSound("misc/menu2.wav");
		if ((slist_length = SList_Length()) > 0)
		{
			SList_Reset (slist_cursor);
			if (slist_cursor > 0 && slist_length - 1 == slist_cursor)
			{
				slist_cursor--;
			}
			else
			{
				memmove (&slist[slist_cursor], &slist[slist_cursor+1], (slist_length - slist_cursor - 1) * sizeof(slist[0]));
				SList_Reset_NoFree (slist_length - 1);
			}
		}
		break;
	}

	if (slist_cursor < slist_mins)
	{
		slist_maxs -= (slist_mins - slist_cursor);
		slist_mins = slist_cursor;
	}
	if (slist_cursor > slist_maxs)
	{
		slist_mins += (slist_cursor - slist_maxs);
		slist_maxs = slist_cursor;
	}
}

/*
	
void M_ServerList_Draw (void)
{
	int	serv, line;
	mpic_t	*p;
	int l;
	char *c = NULL;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);

	for (serv = slist_mins, line = 1 ; serv <= slist_maxs && serv < MAX_SERVER_LIST && slist[serv].server ; serv++, line++)
	{
		M_PrintWhite (MENU_X + 2, line * 8 + MENU_Y, va("%s %-2s", slist[serv].description, slist[serv].players));//
	}

	M_DrawCharacter (MENU_X, (slist_cursor - slist_mins + 1) * 8 + MENU_Y, 12+((int)(realtime*4)&1));

	M_PrintWhite ( 16, STAT_Y      , va("Áääòåóó:%s", slist[slist_cursor].server));
	
	c = va("map: %s mod: %s", slist[slist_cursor].map, slist[slist_cursor].mod);
	
	l = (strlen(c));
	
	M_Print		 (((200-l)/2) ,STAT_Y + 8  , va("map: %s mod: %s", slist[slist_cursor].map, slist[slist_cursor].mod));
	M_Print		 ( 8, STAT_Y + 20 ,	   "T‘ to test server Enter‘ to connect");
}

void M_ServerList_Key (key)
{
	if (!slist[0].server && key != K_ESCAPE && key != K_INS)
		return;

	switch (key)
	{
		case K_ESCAPE:
			M_Menu_MultiPlayer_f ();
			break;

		case K_UPARROW:
		case K_MWHEELUP:
			S_LocalSound ("misc/menu1.wav");
			if (slist_cursor > 0)
			{
//				if (keydown[K_CTRL])
//					SList_Switch (slist_cursor, slist_cursor - 1);
				slist_cursor--;
			}
			else
				slist_cursor = SList_Length() - 1;
			break;

		case K_DOWNARROW:
		case K_MWHEELDOWN:
			S_LocalSound ("misc/menu1.wav");
			if (keydown[K_CTRL])
			{
				if (slist_cursor != SList_Length() - 1)
				{
					SList_Switch (slist_cursor, slist_cursor + 1);
					slist_cursor++;
				}
			}
			else
			{
				if (slist_cursor < MAX_SERVER_LIST - 1 && slist[slist_cursor+1].server)
					slist_cursor++;
				else
					slist_cursor = 0;
			}
			break;

		case K_HOME:
			S_LocalSound ("misc/menu1.wav");
			slist_cursor = 0;
			break;

		case K_END:
			S_LocalSound ("misc/menu1.wav");
			slist_cursor = SList_Length() - 1;
			break;
			
		case K_PGUP:
			S_LocalSound ("misc/menu1.wav");
			slist_cursor -= (slist_maxs - slist_mins);
			if (slist_cursor < 0)
				slist_cursor = 0;
			break;

		case K_PGDN:
			S_LocalSound ("misc/menu1.wav");
			slist_cursor += (slist_maxs - slist_mins);
			if (slist_cursor >= MAX_SERVER_LIST)
				slist_cursor = MAX_SERVER_LIST - 1;
			while (!slist[slist_cursor].server)
				slist_cursor--;
			break;

		case K_ENTER:
		case K_MOUSE1:
			if (keydown[K_CTRL])
			{
				M_Menu_SEdit_f ();
				break;
			}
			m_state = m_main;
			M_ToggleMenu_f ();
			Cbuf_AddText (va("connect \"%s\"\n", slist[slist_cursor].server));
			break;

		case 't':
		case 'T':			
				m_state = m_stest;
				M_ServerList_Test_f();
			break;
	}

	if (slist_cursor < slist_mins)
	{
		slist_maxs -= (slist_mins - slist_cursor);
		slist_mins = slist_cursor;
	}
	if (slist_cursor > slist_maxs)
	{
		slist_mins += (slist_cursor - slist_maxs);
		slist_maxs = slist_cursor;
	}
}
*/
void M_Menu_SEdit_f (void)
{
	int	size;

	key_dest = key_menu;
	m_state = m_sedit;
	m_entersound = true;

	sedit_state = 0;
	Q_strncpyz (slist_serv, slist[slist_cursor].server, sizeof(slist_serv));
	Q_strncpyz (slist_desc, slist[slist_cursor].description, sizeof(slist_desc));
	slist_serv_max = (size = strlen(slist_serv)) > SERV_L ? size : SERV_L;
	slist_serv_min = slist_serv_max - SERV_L;
	slist_desc_max = (size = strlen(slist_desc)) > DESC_L ? size : DESC_L;
	slist_desc_min = slist_desc_max - DESC_L;
}

void M_SEdit_Draw (void)
{
	mpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ((320 - p->width) >> 1, 4, p);

	M_DrawTextBox (SERV_X, SERV_Y, 23, 1);
	M_DrawTextBox (DESC_X, DESC_Y, 23, 1);
	M_PrintWhite (SERV_X, SERV_Y - 4, "Hostname/IP:");
	M_PrintWhite (DESC_X, DESC_Y - 4, "Description:");
	M_Print (SERV_X + 9, SERV_Y + 8, va("%1.23s", slist_serv + slist_serv_min));
	M_Print (DESC_X + 9, DESC_Y + 8, va("%1.23s", slist_desc + slist_desc_min));
	if (sedit_state == 0)
		M_DrawCharacter (SERV_X + 9 + 8*(strlen(slist_serv) - slist_serv_min), SERV_Y + 8, 10+((int)(realtime*4)&1));
	else
		M_DrawCharacter (DESC_X + 9 + 8*(strlen(slist_desc) - slist_desc_min), DESC_Y + 8, 10+((int)(realtime*4)&1));
}

void M_SEdit_Key (int key)
{
	int	l;

	switch (key)
	{

	case K_INS:
		if (keydown[K_SHIFT])
		{
			char	*cliptext;
				
			if ((cliptext = Sys_GetClipboardData()))
			{
				memset (slist_serv, 0, sizeof(slist_serv));
				memset (slist_desc, 0, sizeof(slist_desc));
				SList_Set (slist_cursor, cliptext, cliptext);
				SList_Shutdown();
				M_Menu_ServerList_f ();
			}
		}
		break;

	case K_ESCAPE:
	case K_MOUSE2:
		M_Menu_ServerList_f ();
		break;

	case K_ENTER:
	case K_MOUSE1:
		SList_Set (slist_cursor, slist_serv, slist_desc);
		SList_Shutdown();
		M_Menu_ServerList_f ();
		break;

	case K_UPARROW:
	case K_DOWNARROW:
	case K_MWHEELUP:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu1.wav");
		sedit_state = !sedit_state;
		break;

	case K_BACKSPACE:
		if (sedit_state == 0)
		{
			if ((l = strlen(slist_serv)))
				slist_serv[--l] = 0;
			if (strlen(slist_serv) - 6 < slist_serv_min && slist_serv_min)
			{
				slist_serv_min--;
				slist_serv_max--;
			}
		}
		else
		{
			if ((l = strlen(slist_desc)))
				slist_desc[--l] = 0;
			if (strlen(slist_desc) - 6 < slist_desc_min && slist_desc_min)
			{
				slist_desc_min--;
				slist_desc_max--;
			}
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (sedit_state == 0)
		{
			l = strlen (slist_serv);
			if (l < SLIST_BUFSIZE - 1)
			{
				slist_serv[l+1] = 0;
				slist_serv[l] = key;
				l++;
			}
			if (l > slist_serv_max)
			{
				slist_serv_min++;
				slist_serv_max++;
			}
		}
		else
		{
			l = strlen (slist_desc);
			if (l < SLIST_BUFSIZE - 1)
			{
				slist_desc[l+1] = 0;
				slist_desc[l] = key;
				l++;
			}
			if (l > slist_desc_max)
			{
				slist_desc_min++;
				slist_desc_max++;
			}
		}
		break;
	}
}

//=============================================================================
/* HELP MENU */

int		help_page;
#define	NUM_HELP_PAGES	6

void M_Menu_Help_f (void)
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}

void M_Help_Draw (void)
{
	M_DrawPic (0, 0, Draw_CachePic ( va("gfx/help%i.lmp", help_page)) );
	M_Print(138,189,"ESC to EXIT");
}

void M_Help_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_UPARROW:
	case K_MWHEELUP:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
	case K_LEFTARROW:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES - 1;
		break;
	}
}


//=============================================================================
/* Menu Subsystem */

void M_Init (void)
{
	Cvar_RegisterVariable (&scr_centermenu);
#ifdef GLQUAKE
	Cvar_RegisterVariable (&scr_scalemenu);
#endif
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand ("menu_slist", M_Menu_ServerList_f);
	Cmd_AddCommand ("menu_namemaker", M_Menu_NameMaker_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("menu_videooptions", M_Menu_VideoOptions_f);
#ifdef GLQUAKE
	Cmd_AddCommand ("menu_particles", M_Menu_Particles_f);
#endif
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_maps", M_Menu_Maps_f);
	Cmd_AddCommand ("menu_demos", M_Menu_Demos_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}


void M_Draw (void)
{
	if (m_state == m_none || key_dest != key_menu)
	{
		return;
	}

	if (!m_recursiveDraw)
	{
		scr_copyeverything = 1;

		if (scr_con_current == vid.height)
		{
			Draw_ConsoleBackground (scr_con_current);
			VID_UnlockBuffer ();
			S_ExtraUpdate ();
			VID_LockBuffer ();
		}
		else
		{
			Draw_FadeScreen ();
		}

		scr_fullupdate = 0;
	}
	else
	{
		m_recursiveDraw = false;
	}

#ifdef GLQUAKE

	if (scr_scalemenu.value)
	{
		menuwidth = 320;
		menuheight = max(menuwidth * vid.aspect,200);
		glMatrixMode (GL_PROJECTION);
		glLoadIdentity ();
		glOrtho (0, menuwidth, menuheight, 0, -99999, 99999);
	}
	else
	{
		menuwidth = vid.width;
		menuheight = vid.height;
	}
#endif

	m_yofs = scr_centermenu.value ? (menuheight - 200) / 2 : 0;

	switch (m_state)
	{
	case m_none:
		break;

	case m_main:
		M_Main_Draw ();
		break;

	case m_singleplayer:
		M_SinglePlayer_Draw ();
		break;

	case m_load:
		M_Load_Draw ();
		break;

	case m_save:
		M_Save_Draw ();
		break;

	case m_multiplayer:
		M_MultiPlayer_Draw ();
		break;

	case m_setup:
		M_Setup_Draw ();
		break;

	case m_namemaker:
		M_NameMaker_Draw ();
		break;

	case m_net:
		M_Net_Draw ();
		break;

	case m_options:
		M_Options_Draw ();
		break;

	case m_keys:
		M_Keys_Draw ();
		break;

#ifdef GLQUAKE
	case m_videooptions:
		M_VideoOptions_Draw ();
		break;

	case m_particles:
		M_Particles_Draw ();
		break;
#endif

	case m_video:
		M_Video_Draw ();
		break;

	case m_maps:
		M_Maps_Draw ();
		break;

	case m_demos:
		M_Demos_Draw ();
		break;

	case m_help:
		M_Help_Draw ();
		break;

	case m_quit:
		M_Quit_Draw ();
		break;

	case m_update:
		M_Update_Draw ();
		break;

//	case m_serialconfig:
//		M_SerialConfig_Draw ();
//		break;

//	case m_modemconfig:
//		M_ModemConfig_Draw ();
//		break;

	case m_lanconfig:
		M_LanConfig_Draw ();
		break;

	case m_gameoptions:
		M_GameOptions_Draw ();
		break;

	case m_search:
		M_Search_Draw ();
		break;

	case m_servers:
		M_FoundServers_Draw ();
		break;

	case m_slist:
		M_ServerList_Draw ();
		break;

	case m_sedit:
		M_SEdit_Draw ();
		break;

	case m_stest:
		M_STest_Draw ();
		break;
	}

#ifdef GLQUAKE
	if (scr_scalemenu.value)
	{
		glMatrixMode (GL_PROJECTION);
		glLoadIdentity ();
		glOrtho (0, vid.width, vid.height, 0, -99999, 99999);
	}
#endif

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}

void M_Keydown (int key)
{	
	switch (m_state)
	{
	case m_none:
		return;

	case m_main:
		M_Main_Key (key);
		return;

	case m_singleplayer:
		M_SinglePlayer_Key (key);
		return;

	case m_load:
		M_Load_Key (key);
		return;

	case m_save:
		M_Save_Key (key);
		return;

	case m_multiplayer:
		M_MultiPlayer_Key (key);
		return;

	case m_setup:
		M_Setup_Key (key);
		return;

	case m_namemaker:
		M_NameMaker_Key (key);
		return;

	case m_net:
		M_Net_Key (key);
		return;

	case m_options:
		M_Options_Key (key);
		return;

	case m_keys:
		M_Keys_Key (key);
		return;

#ifdef GLQUAKE
	case m_videooptions:
		M_VideoOptions_Key (key);
		return;

	case m_particles:
		M_Particles_Key (key);
		return;
#endif

	case m_video:
		M_Video_Key (key);
		return;
	
	case m_maps:
		M_Maps_Key (key);
		return;

	case m_demos:
		M_Demos_Key (key);
		return;

	case m_help:
		M_Help_Key (key);
		return;

	case m_quit:
		M_Quit_Key (key);
		return;

	case m_update:
		M_Update_Key (key);
		return;
//	case m_serialconfig:
//		M_SerialConfig_Key (key);
//		return;

//	case m_modemconfig:
//		M_ModemConfig_Key (key);
//		return;

	case m_lanconfig:
		M_LanConfig_Key (key);
		return;

	case m_gameoptions:
		M_GameOptions_Key (key);
		return;

	case m_search:
		M_Search_Key (key);
		break;

	case m_servers:
		M_FoundServers_Key (key);
		break;

	case m_slist:
		M_ServerList_Key (key);
		return;

	case m_sedit:
		M_SEdit_Key (key);
		break;

	case m_stest:
		M_STest_Key (key);
		break;

	}
}

void M_ConfigureNetSubsystem (void)
{
// enable/disable net systems to match desired config
	Cbuf_AddText ("stopdemo\n");
//	if (SerialConfig || DirectConfig)
//		Cbuf_AddText ("com1 enable\n");

	if (IPXConfig || TCPIPConfig)
		net_hostport = lanConfig_port;
}


/*
	M_Print (16, 56, "         Smooth Camera");
	M_DrawCheckbox (219, 56, pq_smoothcam.value);

*/
