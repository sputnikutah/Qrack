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

// gl_screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#ifdef _WIN32
#include "movie.h"
#endif

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net 
turn off messages option

the refresh is always rendered, unless the console is full screen


console is:
	notify lines
	half
	full

*/

int		glx, gly, glwidth, glheight;

// only the refresh window will be updated unless these variables are flagged 
int		scr_copytop;
int		scr_copyeverything;

float		scr_con_current;
float		scr_conlines;		// lines of console to display

float		oldscreensize;
float		oldfov = 90;
cvar_t		scr_viewsize			= {"viewsize", "100", true};
cvar_t		scr_fov					= {"fov", "90"};				// 10 - 170
cvar_t		scr_fovspeed			= {"fov_speed", "10"};			//R00k
cvar_t		scr_fovcompat			= {"scr_fovcompat","0"};		//MH
cvar_t      scr_consize				= {"scr_consize", "0.5"};		// by joe
cvar_t		scr_conspeed			= {"scr_conspeed", "2000"};
cvar_t		scr_centertime			= {"scr_centertime", "3"};
cvar_t		scr_showram				= {"showram", "0"};
cvar_t		scr_showturtle			= {"showturtle", "0"};
cvar_t		scr_showpause			= {"showpause", "1"};
cvar_t		scr_printspeed			= {"scr_printspeed", "8"};
cvar_t		scr_drawautoids			= {"scr_drawautoids","1"};
#ifdef GLQUAKE
cvar_t		gl_triplebuffer			= {"gl_triplebuffer", "0", true};
cvar_t		png_compression_level	= {"png_compression_level", "1"};
cvar_t		jpeg_compression_level	= {"jpeg_compression_level", "75"};
#endif
cvar_t		scr_sshot_type			= {"scr_sshot_type", "jpg"};

cvar_t		scr_centerprint_levelname = {"scr_centerprint_levelname","1", true};

qboolean	scr_initialized;		// ready to draw

mpic_t          *scr_ram;
mpic_t          *scr_net; 
mpic_t          *scr_turtle;

int			scr_fullupdate;

int			clearconsole;
int			clearnotify;

int			sb_lines;
	
viddef_t	vid;				// global video state

vrect_t		scr_vrect;

qboolean	scr_disabled_for_loading;
qboolean	scr_drawloading;
float		scr_disabled_time;

qboolean	block_drawing;

void SCR_ScreenShot_f (void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char	scr_centerstring[1024];
float	scr_centertime_start;	// for slow victory printing
float	scr_centertime_off;
int		scr_center_lines;
int		scr_erase_lines;
int		scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	if (!str[0]) return; //MH!

	Q_strncpyz (scr_centerstring, str, sizeof(scr_centerstring)); 

	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}


void SCR_DrawCenterString (void)
{
	char	*start;
	int		l, j, x, y, remaining;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height*0.35;
	else
		y = 48;
	

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8) / 2;
		
		for (j=0 ; j<l ; j++, x+=8)
		{
			Draw_Character (x, y, start[j], false);	
			if (!remaining--)
			{
				return;
			}
		}
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

extern qboolean sb_showscores;
void SCR_CheckDrawCenterString (void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;
	
	if (cl.intermission) 
		scr_centertime_off = 1;

	if (scr_centertime_off < 0.001f && !cl.intermission)
		return;

	if (key_dest != key_game)
		return;

	if ((sb_showscores) && (cl.gametype == GAME_DEATHMATCH))
		return;

	if (scr_centertime_off < 1.0f) 
		Draw_Alpha_Start (scr_centertime_off);

	SCR_DrawCenterString ();

	if (scr_centertime_off < 1.0f) 
		Draw_Alpha_End();
}

//=============================================================================

/*
====================
CalcFov
====================
*/
float SCR_CalcFovX (float fov_y, float width, float height)
{
	float   a;
	float   y;

	// bound, don't crash
	if (fov_y < 1) fov_y = 1;
	if (fov_y > 179) fov_y = 179;

	y = height / tan (fov_y / 360 * M_PI);
	a = atan (width / y);
	a = a * 360 / M_PI;

	return a;
}

float SCR_CalcFovY (float fov_x, float width, float height)
{
	float   a;
	float   x;

	// bound, don't crash
	if (fov_x < 1) fov_x = 1;
	if (fov_x > 179) fov_x = 179;

	x = width / tan (fov_x / 360 * M_PI);
	a = atan (height / x);
	a = a * 360 / M_PI;

	return a;
}
//R00k: what this allows is, if you used fov 110 on a 4:3 CRT and got a new widescreen LCD, then with scr_fovcompat on your new monitor can use the same fov
//From MH - DirectQ
void SCR_SetFOV (float *fovx, float *fovy, float fovvar, int width, int height)
{
	extern cvar_t vid_force_aspect_ratio;
	float aspect = (float) height / (float) width;
	float baseheight = (cl_sbar.value ? 480.0f : 432.0f);

	// http://www.gamedev.net/topic/431111-perspective-math-calculating-horisontal-fov-from-vertical/
	// horizontalFov = atan( tan(verticalFov) * aspectratio )
	// verticalFov = atan( tan(horizontalFov) / aspectratio )

	if ((scr_fovcompat.value) || aspect > (baseheight / 640.0f))
	{
		// use the same calculation as GLQuake did
		// (horizontal is constant, vertical varies)
		fovx[0] = fovvar;
		fovy[0] = SCR_CalcFovY (fovx[0], width, height);
	}
	else
	{
		// alternate calculation (vertical is constant, horizontal varies)
		// consistent with http://www.emsai.net/projects/widescreen/fovcalc/
		// note that the gun always uses this calculation irrespective of the aspect)
		fovy[0] = SCR_CalcFovY (fovvar, 640.0f, baseheight);
		fovx[0] = SCR_CalcFovX (fovy[0], width, height);
	}
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef (void)
{
	float		size;
	qboolean	full = false;
	
	scr_fullupdate = 0;		// force a background redraw
	vid.recalc_refdef = 0;


// force the status bar to redraw
	Sbar_Changed ();

//========================================
	
// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set ("viewsize", "30");
	if (scr_viewsize.value > 120)
		Cvar_Set ("viewsize", "120");

// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set ("fov", "10");
	if (scr_fov.value > 170)
		Cvar_Set ("fov", "170");

// intermission is always full screen	
	size = cl.intermission ? 120 : scr_viewsize.value;

	if (size >= 120)
		sb_lines = 0;		// no status bar at all
	else if (size >= 110)
		sb_lines = 24;		// no inventory
	else
		sb_lines = 24 + 16 + 8;

	if (scr_viewsize.value >= 100.0)
	{
		full = true;
		size = 100.0;
	}
	else
	{
		size = scr_viewsize.value;
	}

	if (cl.intermission)
	{
		full = true;
		size = 100.0;
		sb_lines = 0;
	}
	size /= 100.0;

	r_refdef.vrect.width = vid.width * size;

	if (r_refdef.vrect.width < 96)
	{
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;	// min for icons
	}

	r_refdef.vrect.height = vid.height * size;

	if (r_refdef.vrect.height > vid.height)
	{
		r_refdef.vrect.height = vid.height;
	}

	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width) / 2;

	if (full)
		r_refdef.vrect.y = 0;
	else 
		r_refdef.vrect.y = (vid.height - r_refdef.vrect.height) / 2;

	SCR_SetFOV (&r_refdef.fov_x, &r_refdef.fov_y, oldfov, vid.width, vid.height);	

	scr_vrect = r_refdef.vrect;
}
/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue ("viewsize", scr_viewsize.value + 10);
	vid.recalc_refdef = 1;
}

/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue ("viewsize", scr_viewsize.value - 10);
	vid.recalc_refdef = 1;
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	Cvar_RegisterVariable (&scr_fov);
	Cvar_RegisterVariable (&scr_fovcompat);
	Cvar_RegisterVariable (&scr_fovspeed);	// R00k
	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_consize);	// by joe
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&scr_showram);
	Cvar_RegisterVariable (&scr_showturtle);
	Cvar_RegisterVariable (&scr_showpause);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&scr_sshot_type);
	Cvar_RegisterVariable (&scr_centerprint_levelname);
	Cvar_RegisterVariable (&scr_drawautoids);

#ifdef GLQUAKE
	Cvar_RegisterVariable (&png_compression_level);
	Cvar_RegisterVariable (&gl_triplebuffer);
	Cvar_RegisterVariable (&jpeg_compression_level);
#endif
// register our commands
	Cmd_AddCommand ("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f);

	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");

#ifdef _WIN32
	Movie_Init ();
#endif

	scr_initialized = true;
}

/*
==============
SCR_DrawRam
==============
*/
void SCR_DrawRam (void)
{
	if (!scr_showram.value)
		return;

	if (!r_cache_thrash)
		return;

	Draw_Pic (scr_vrect.x+32, scr_vrect.y, scr_ram);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static	int	count;
	
	if (!scr_showturtle.value)
		return;

	if (host_frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (scr_vrect.x, scr_vrect.y, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;

	if (cls.demoplayback)
		return;

	Draw_Pic (scr_vrect.x+64, scr_vrect.y, scr_net);
}

/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	mpic_t	*pic;

	if (!scr_showpause.value)		// turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ((vid.width - pic->width) / 2, (vid.height - 48 - pic->height) / 2, pic);
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	mpic_t	*pic;

	if (!scr_drawloading)
		return;
		
	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ((vid.width - pic->width) / 2, (vid.height - 48 - pic->height) / 2, pic);		
}

//=============================================================================

/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	Con_CheckResize ();

	if (scr_drawloading)
		return;		// never a console with loading plaque

// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)		// by joe
	{
		scr_conlines = vid.height * scr_consize.value;
		if (scr_conlines < 30)
			scr_conlines = 30;
		if (scr_conlines > vid.height - 10)
			scr_conlines = vid.height - 10;
	}
	else
	{
		scr_conlines = 0;			// none visible
	}

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value * host_frametime * vid.height / 320;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value * host_frametime * vid.height / 320;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
		Sbar_Changed ();
	else if (clearnotify++ < vid.numpages)
	{
	}
	else
		con_notifylines = 0;
}
	
/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	if (scr_con_current)
	{
		scr_copyeverything = 1;
		Con_DrawConsole (scr_con_current, true);
		clearconsole = 0;
		if (con_backscroll)
			Con_DrawNotify ();
	}
	else
	{
//		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}


/* 
============================================================================== 
 
				SCREEN SHOTS 
 
============================================================================== 
*/ 

// stuff added from FuhQuake - joe
extern	unsigned short	ramps[3][256];

void ApplyGamma (byte *buffer, int size)
{
	int	i;

	if (!vid_hwgamma_enabled)
		return;

	for (i=0 ; i<size ; i+=3)
	{
		buffer[i+0] = ramps[0][buffer[i+0]] >> 8;
		buffer[i+1] = ramps[1][buffer[i+1]] >> 8;
		buffer[i+2] = ramps[2][buffer[i+2]] >> 8;
	}
}

int SCR_ScreenShot (char *name)
{
	qboolean	ok = false;
	int		buffersize = glwidth * glheight * 3;
	byte		*buffer;
	char		*ext;

	ext = COM_FileExtension (name);

	buffer = Q_malloc (buffersize);
	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	ApplyGamma (buffer, buffersize);

	if (!Q_strcasecmp(ext, "jpg"))
		ok = Image_WriteJPEG (name, jpeg_compression_level.value, buffer + buffersize - 3 * glwidth, -glwidth, glheight);
	else if (!Q_strcasecmp(ext, "png"))
		ok = Image_WritePNG (name, png_compression_level.value, buffer + buffersize - 3 * glwidth, -glwidth, glheight);
	else
		ok = Image_WriteTGA (name, buffer, glwidth, glheight);

	Q_free (buffer);

	return ok;
}

/* 
================== 
SCR_ScreenShot_f
================== 
*/  
void SCR_ScreenShot_f (void) 
{
	int	i, success;
	char	name[MAX_OSPATH], ext[4], *sshot_dir = "qrack/screenshots";

	if (Cmd_Argc() == 2)
	{
		Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	}
	else if (Cmd_Argc() == 1)
	{
		// find a file name to save it to 
		if (!Q_strcasecmp(scr_sshot_type.string, "jpg") || !Q_strcasecmp(scr_sshot_type.string, "jpeg"))
			Q_strncpyz (ext, "jpg", 4);
		else if (!Q_strcasecmp(scr_sshot_type.string, "png"))
			Q_strncpyz (ext, "png", 4);
		else
			Q_strncpyz (ext, "tga", 4);

		for (i=0 ; i<999 ; i++) 
		{ 
			Q_snprintfz (name, sizeof(name), "qrack%03i.%s", i, ext);
			if (Sys_FileTime(va("%s/%s/%s", com_basedir, sshot_dir, name)) == -1)
				break;	// file doesn't exist
		} 

		if (i > 1000)
		{
			Con_Printf ("ERROR: Cannot create more than 1000 screenshots\n");
			return;
		}
	}
	else
	{
		Con_Printf ("Usage: %s [filename]", Cmd_Argv(0));
		return;
	}

	success = SCR_ScreenShot (va("%s/%s", sshot_dir, name));
	Con_Printf ("%s %s\n", success ? "Wrote" : "Couldn't write", name);
} 


//=============================================================================


/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (true);

	if (cls.state != ca_connected || cls.signon != SIGNONS)
		return;

// redraw with no console and the loading plaque
	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	scr_fullupdate = 0;
	Sbar_Changed ();
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
	scr_fullupdate = 0;
}

/*
================
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque (void)
{
	scr_disabled_for_loading = false;
	scr_fullupdate = 0;
	Con_ClearNotify ();
}

//=============================================================================

char		*scr_notifystring;
qboolean	scr_drawdialog;

void SCR_DrawNotifyString (void)
{
	char	*start;
	int	l, j, x, y;

	start = scr_notifystring;

	y = vid.height*0.35;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
			Draw_Character (x, y, start[j],false);	
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.  
==================
*/
int SCR_ModalMessage (char *text)
{
	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;
 
// draw a fresh screen
	scr_fullupdate = 0;
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;
	
	S_ClearBuffer ();		// so dma doesn't loop current sound

	do
	{
		key_count = -1;		// wait for a key down and up
		Sys_SendKeyEvents ();
	} while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE);

	scr_fullupdate = 0;
	SCR_UpdateScreen ();

	return key_lastpress == 'y';
}


//=============================================================================

void SCR_TileClear (void)
{
	if (r_refdef.vrect.x > 0)
	{
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - sb_lines);
			// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0, 
			vid.width - r_refdef.vrect.x + r_refdef.vrect.width, 
			vid.height - sb_lines);
	}
	if (r_refdef.vrect.y > 0)
	{
		// top
		Draw_TileClear (r_refdef.vrect.x, 0, 
			r_refdef.vrect.x + r_refdef.vrect.width, 
			r_refdef.vrect.y);
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
			r_refdef.vrect.y + r_refdef.vrect.height, 
			r_refdef.vrect.width, 
			vid.height - sb_lines - 
			(r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

int qglProject (float objx, float objy, float objz, float *model, float *proj, int *view, float* winx, float* winy, float* winz) 
{
	float in[4], out[4], dist, scale;
	int i;
	vec3_t org;

	org[0] = objx; org[1] = objy; org[2] = objz;
	in[0] = objx; in[1] = objy; in[2] = objz; in[3] = 1.0;

	if (R_CullSphere (org, 1))
		return 0;

	dist = VectorDistance(r_refdef.vieworg, org);	

	if (dist > r_farclip.value)
		return 0;

	for (i = 0; i < 4; i++)
		out[i] = in[0] * model[0 * 4 + i] + in[1] * model[1 * 4 + i] + in[2] * model[2 * 4 + i] + in[3] * model[3 * 4 + i];

	for (i = 0; i < 4; i++)
		in[i] =	out[0] * proj[0 * 4 + i] + out[1] * proj[1 * 4 + i] + out[2] * proj[2 * 4 + i] + out[3] * proj[3 * 4 + i];

	if (!in[3])
		return 0;

	VectorScale(in, 1 / in[3], in);
	
	*winx = view[0] + (1 + in[0]) * view[2] / 2;
	*winy = view[1] + (1 + in[1]) * view[3] / 2;

	scale = (0.5 / (dist / 128.0f));//this starts out small then gets big then small again....

	if (scale < 0.1)
		return 0;

	scale = bound(0.5f, scale, 2);
	
	*winz = scale;
	return 1;
}

typedef struct player_autoid_s 
{
    float x, y;
    scoreboard_t *player;
	float scale;
} autoid_player_t;

static autoid_player_t autoids[MAX_SCOREBOARD];
static int    autoid_count;

void SCR_DrawAutoIds (void)
{
	int i, x, y;

	extern void Draw_String_Scaled (int x, int y, char *str, float scale);

	float frametime	= fabs(cl.mtime[0] - cl.mtime[1]);

	if (frametime < (1/cl_maxfps.value)/2)//R00k, might help from over drawing
		return;

	for (i = 0; i < autoid_count; i++)
	{
		x =  autoids[i].x * vid.width / glwidth;
		y =  (glheight - autoids[i].y) * vid.height / glheight;

//		Draw_String_Scaled (x - strlen(va("%i",autoids[i].player->frags)) * 4 * (autoids[i].scale * 1), y - 8 * (autoids[i].scale * 1), va("%i",autoids[i].player->frags), (autoids[i].scale * 1));
		Draw_String_Scaled (x - strlen(autoids[i].player->name) * 4 * (autoids[i].scale * 1.2), y + 0  * (autoids[i].scale * 1.2), autoids[i].player->name, (autoids[i].scale * 2));
//		Draw_String_Scaled (x - strlen(va("%i",autoids[i].player->ping)) * 4 * (autoids[i].scale * 0.8), y + 24  * (autoids[i].scale * 0.8), va("%i",autoids[i].player->ping), (autoids[i].scale * 0.8));//pING		
	}
}

void SCR_SetupAutoID (void) 
{
    int				i, view[4];
    float			model[16], project[16];
	vec3_t			origin;
    entity_t		*state;
    autoid_player_t *id;
	float frametime = fabs(cl.mtime[0] - cl.mtime[1]);

	if (frametime < (1/cl_maxfps.value)/2)
		return;

	autoid_count = 0;

    glGetFloatv (GL_MODELVIEW_MATRIX, model);
    glGetFloatv (GL_PROJECTION_MATRIX, project);
    glGetIntegerv (GL_VIEWPORT, view);

    for (i = 0; i < cl.maxclients; i++) 
	{
        state = &cl_entities[1 + i];

		if (state->culled)
			continue;

		if (!state->model)
			continue;

		if (state->modelindex == cl_modelindex[mi_eyes])
			continue;

		if (state->shirtcolor == 0 && state->pantscolor == 0)//R00k: no names on observers!
			continue;

        id = &autoids[autoid_count];
        id->player = &cl.scores[i];

		VectorCopy(state->origin, origin);

		origin[2] += 12;//middle of the player model

        if (qglProject (origin[0], origin[1], origin[2], model, project, view, &id->x, &id->y, &id->scale))
		{
            autoid_count++;
		}
    }
}

void SCR_DrawEdictTags (void)
{
	int i, x, y;

	extern void Draw_String_Scaled (int x, int y, char *str, float scale);
	float frametime	= fabs(cl.time - cl.oldtime);

	if (frametime < (1/cl_maxfps.value)/2)
		return;

	if (!developer.value)
		return;

	for (i = 0; i < ed_tag_count; i++)
	{
		x =  edicttags[i].x * vid.width / glwidth;
		y =  (glheight - edicttags[i].y) * vid.height / glheight;

		if (edicttags[i].classname)
			Draw_String_Scaled (x - strlen(edicttags[i].classname) * 4 * edicttags[i].scale, y - 8 * edicttags[i].scale, edicttags[i].classname, edicttags[i].scale);
		
		if (edicttags[i].origin)
			Draw_String_Scaled (x - strlen(edicttags[i].origin) * 4 * (edicttags[i].scale * 0.8), y + 8 * (edicttags[i].scale * 0.8), edicttags[i].origin, (edicttags[i].scale * 0.8));
		
		if (edicttags[i].modelname)
			Draw_String_Scaled (x - strlen(edicttags[i].modelname) * 4 * (edicttags[i].scale * 0.8), y + 16 * (edicttags[i].scale * 0.8), edicttags[i].modelname, (edicttags[i].scale * 0.8));
	}
}

void SCR_SetupEdictTags (void) 
{
	int view[4];
	float model[16], project[16];
	edict_t	*ed;
	int	i;
	edict_tag_t *id;
	trace_t	trace;
	qboolean result;
	char *name;
	float frametime	= fabs(cl.time - cl.oldtime);
	extern cvar_t	sv_cullentities;
	
//	entity_t *ent;
	vec3_t	mins, maxs, origin;

	if (frametime < (1/cl_maxfps.value)/2)
		return;

	if (!developer.value)
		return;
	
	if (!cl.worldmodel)//Level hasnt loaded yet...
		return;

	if (cl.maxclients > 1 || !r_drawentities.value || !sv.active)//only in singleplayer mode...
		return;

	ed_tag_count = 0;

	glGetFloatv(GL_MODELVIEW_MATRIX, model);
	glGetFloatv(GL_PROJECTION_MATRIX, project);
	glGetIntegerv(GL_VIEWPORT, (GLint *)view);

	for (i = 0, ed = NEXT_EDICT(sv.edicts) ; i < sv.num_edicts ; i++, ed = NEXT_EDICT(ed))
	{		
		if (ed->free)
		{
			continue;
		}

		origin[0] = ed->v.origin[0];
		origin[1] = ed->v.origin[1];
		origin[2] = ed->v.origin[2];

		if (!VectorLength(origin))
		{
			VectorCopy (ed->v.absmin, mins);
			VectorCopy (ed->v.absmax, maxs);	
			LerpVector (mins, maxs, 0.5, origin);
		}

		if (sv_cullentities.value)    
		{
			memset (&trace, 0, sizeof(trace));
			trace.fraction = 1;

			result = SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, origin, &trace);
			
			if (!result)//hit something
			{
				continue;
			}
		}
		
		id = &edicttags[ed_tag_count];

		if (qglProject(origin[0], origin[1], origin[2], model, project, view, &id->x, &id->y, &id->scale))
		{
			if (!strcmp((pr_strings + ed->v.classname),"player"))
			{
				strlcpy(id->classname, (pr_strings + ed->v.netname), 64);
				
				strlcpy(id->modelname, (pr_strings + ed->v.model), 64);
				
				name = va("%3.1f %3.1f %3.1f ", origin[0], origin[1], origin[2]);
				strlcpy(id->origin, name, 64);
			}
			else
			{
				strlcpy(id->classname, (pr_strings + ed->v.classname), 64);

				strlcpy(id->modelname, (pr_strings + ed->v.model), 64);

				name = va("%3.1f %3.1f %3.1f ", origin[0], origin[1], origin[2]);
				strlcpy(id->origin, name, 64);	
			}				
			ed_tag_count++;
		}
	}
/*
	for (i = 0; i < MAX_EDICTS ; i++)
	{
		ent = CL_EntityNum (i);
		origin[0] = ent->origin[0];
		origin[1] = ent->origin[1];
		origin[2] = ent->origin[2];

		if (sv_cullentities.value)    
		{
			memset (&trace, 0, sizeof(trace));
			trace.fraction = 1;

			result = SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, origin, &trace);
			
			if (!result)//hit something
				continue;
		}

		id = &edicttags[ed_tag_count];

		if (qglProject(origin[0], origin[1], origin[2], model, project, view, &id->x, &id->y, &id->scale))
		{
			name = "στατιγ entity_t";
			strlcpy(id->classname, "στατιγ entity_t", 64);

			name = va("%s",(ent->model->name));
			strlcpy(id->modelname, name, 64);

			name = va("%3.1f %3.1f %3.1f ",origin[0], origin[1], origin[2]);
			strlcpy(id->origin, name, 64);
		
			ed_tag_count++;
		}
	}
	*/
}

/*
===================
SCR_SetBrightness

Enables setting of brightness without having to do any mondo fancy shite.
It's assumed that we're in the 2D view for this...

Basically, what it does is multiply framebuffer colours by an incoming constant between 0 and 2
===================

void SCR_SetBrightness (float brightfactor)
{
    // divide by 2 cos the blendfunc will sum src and dst
    const GLfloat brightblendcolour[4] = {0, 0, 0, 0.5f * brightfactor};
    const GLfloat constantwhite[4] = {1, 1, 1, 1};

    // don't trust == with floats, don;t bother if it's 1 cos it does nothing to the end result!!!
    if (brightfactor > 0.99 && brightfactor < 1.01)
    {
        return;
    }

    glColor4fv (constantwhite);
    glEnable (GL_BLEND);
    glDisable (GL_ALPHA_TEST);
    glBlendFunc (GL_DST_COLOR, GL_SRC_COLOR);

    // combine hack...
    // this is weird cos it uses a texture but actually doesn't - the parameters of the
    // combiner function only use the incoming fragment colour and a constant colour...
    // you could actually bind any texture you care to mention and get the very same result...
    // i've decided not to bind any at all...
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_CONSTANT);
    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, brightblendcolour);

    glBegin (GL_QUADS);

    glTexCoord2f (0, 0);
    glVertex2f (0, 0);

    glTexCoord2f (0, 1);
    glVertex2f (0, GL_State.OrthoHeight);

    glTexCoord2f (1, 1);
    glVertex2f (GL_State.OrthoWidth, GL_State.OrthoHeight);

    glTexCoord2f (1, 0);
    glVertex2f (GL_State.OrthoWidth, 0);

    glEnd ();

    // restore combiner function colour to white so as not to mess up texture state
    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constantwhite);

    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glDisable (GL_BLEND);
    glEnable (GL_ALPHA_TEST);
    glColor4fv (constantwhite);
}
*/

float	oldsbar = 0;
/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/

void SCR_UpdateScreen (void)
{
	extern void Sbar_DrawPing ();
	extern void Draw_MapShotBackground (char *lvlname, float alpha);
	extern cvar_t vid_refreshrate;
	extern cvar_t cl_levelshots;
	extern cvar_t	scr_clock;
	float alpha;
	extern	cvar_t developer_tool_show_edict_tags;

	if (isDedicated)//R00k
		return;

	if (block_drawing)
		return;

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time > 60)
			scr_disabled_for_loading = false;
		else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet

#ifdef _WIN32
	{	// don't suck up any cpu if minimized
		extern	int	Minimized;

		if (Minimized)
			return;
	}
#endif

	vid.numpages = 2 + gl_triplebuffer.value;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (oldsbar != cl_sbar.value)
	{
		oldsbar = cl_sbar.value;
		vid.recalc_refdef = 1;
	}

	if (oldfov != scr_fov.value)
	{      
		if (scr_fov.value > oldfov)
		{
			oldfov += min(scr_fovspeed.value,scr_fov.value);
			if (oldfov > scr_fov.value)
				oldfov = scr_fov.value;
		}
		else
		{
			oldfov -= min(scr_fovspeed.value,scr_fov.value);
			if (oldfov < scr_fov.value)
				oldfov = scr_fov.value;
		}
		vid.recalc_refdef = 1;
	} 

	// determine size of refresh window
	if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = 1;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

	if ((v_contrast.value > 1 && !vid_hwgamma_enabled) || gl_clear.value)
		Sbar_Changed ();

// do 3D refresh drawing, and then update the screen
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	
	SCR_SetUpToDrawConsole ();
	
	V_RenderView ();


	GL_Set2D ();

	if (gl_polyblend.value < 2) //R00k: 2 = use shader
		R_PolyBlend ();	// added by joe - IMPORTANT: this _must_ be here so that palette flashes take effect in windowed mode too.

	// draw any areas not covered by the refresh
	SCR_TileClear ();

	if (scr_drawdialog)
	{
		Sbar_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
		scr_copyeverything = true;
	}
	else if (scr_drawloading)
	{
		SCR_DrawLoading ();	
	}
	else if (cl.intermission == 1 && key_dest == key_game)
	{
		Sbar_IntermissionOverlay ();
	}
	else if (cl.intermission == 2 && key_dest == key_game)
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	}
	else if (cl.intermission == 3 && key_dest == key_game)
	{
		SCR_CheckDrawCenterString ();
	}
	else
	{
		Draw_Crosshair ();
		SCR_DrawRam ();
		SCR_DrawNet ();
		SCR_DrawTurtle ();
		SCR_DrawPause ();

		if (nehahra)
			SHOWLMP_drawall ();

		SCR_CheckDrawCenterString ();
	
		if (scr_clock.value)
			SCR_DrawLocalTime ();

		SCR_DrawFPS ();

		if (cls.demorecording)//R00k
		{			
			SCR_DrawREC();
		}

		Sbar_DrawPing ();
		SCR_DrawLocName();
		SCR_DrawSpeed ();
		Sbar_Draw ();
		SCR_DrawConsole ();	
		M_Draw ();

		if ((developer.value && developer_tool_show_edict_tags.value))
		{
			SCR_DrawEdictTags ();
		}

		if ((cls.demoplayback) && (scr_drawautoids.value))
			SCR_DrawAutoIds();
	}

	R_BrightenScreen ();

	V_UpdatePalette ();

	if (cl_levelshots.value)//R00k: v1.92
	{
		if ((cl.lvlshot_time - realtime > 0) && ((realtime - cl.time) > (1 / 60)))
		{
			alpha = cl.lvlshot_time * ((cl.lvlshot_time - realtime) / cl.lvlshot_time);
			if (alpha > 0)
				Draw_MapShotBackground(sv_mapname.string, alpha);	
		}
	}
	
#ifdef _WIN32
	Movie_UpdateScreen ();
#endif

	GL_EndRendering ();
}
