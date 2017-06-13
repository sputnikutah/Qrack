/*
Copyright (C) 2002-2003 A Nourai

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
// vid_common_gl.c -- Common code for vid_wgl.c and vid_glx.c

#include "quakedef.h"

#ifdef _WIN32
#define qglGetProcAddress wglGetProcAddress
#else
#define qglGetProcAddress glXGetProcAddressARB
#endif

const	char	*gl_vendor;
const	char	*gl_renderer;
const	char	*gl_version;
const	char	*gl_extensions;

lpMTexFUNC		qglMultiTexCoord2f = NULL;
lp1DMTexFUNC	qglMultiTexCoord1f = NULL;
lpSelTexFUNC	qglActiveTexture = NULL;
lpSelTexFUNC	glClientActiveTexture = NULL;


int			gl_textureunits = 1;
#ifdef USEFAKEGL
qboolean	gl_mtexable = true;
#else
qboolean	gl_mtexable = false;
qboolean	gl_vbo = false;
#endif

float		gldepthmin = 0.005f;
float		gldepthmax = 1.0f;
float		vid_gamma = 1.0;
byte		vid_gamma_table[256];

unsigned	d_8to24table[256];
unsigned	d_8to24table2[256];

extern qboolean	fullsbardraw;

extern	HWND	mainwindow;
extern  LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HMONITOR		hCurrMon;
MONITORINFOEX	currMonInfo;

// Finds out what monitor the window is currently on.
HMONITOR VID_GetCurrentMonitor()
{
	return MonitorFromWindow(mainwindow, MONITOR_DEFAULTTOPRIMARY);
}

// Finds out info about the current monitor.
MONITORINFOEX VID_GetCurrentMonitorInfo(HMONITOR monitor)
{
	MONITORINFOEX inf;
	memset(&inf, 0, sizeof(MONITORINFOEX));
	inf.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(monitor, (LPMONITORINFO)&inf);
	return inf;
}

qboolean CheckExtension (const char *extension)
{
	const	char	*start;
	char		*where, *terminator;

	if (!gl_extensions && !(gl_extensions = glGetString(GL_EXTENSIONS)))
		return false;

	if (!extension || *extension == 0 || strchr(extension, ' '))
		return false;

	for (start = gl_extensions ; where = strstr(start, extension) ; start = terminator)
	{
		terminator = where + strlen(extension);
		if ((where == start || *(where - 1) == ' ') && (*terminator == 0 || *terminator == ' '))
			return true;
	}

	return false;
}
void CheckMultiTextureExtensions (void)
{
	if (!COM_CheckParm("-nomtex") && CheckExtension("GL_ARB_multitexture"))
	{
		if (strstr(gl_renderer, "Savage"))
			return;

		qglMultiTexCoord2f		= (void *)qglGetProcAddress ("glMultiTexCoord2fARB");
		qglMultiTexCoord1f 		= (void *)qglGetProcAddress ("glMultiTexCoord1fARB");
		qglActiveTexture 		= (void *)qglGetProcAddress ("glActiveTextureARB");		
		glClientActiveTexture	= (void *)qglGetProcAddress ("glClientActiveTexture");

		if (!qglMultiTexCoord2f || !qglActiveTexture || !qglMultiTexCoord1f)
		{
			Con_Printf ("▌варнинг▌ Multitexture extensions нот found!\n");
			return;
		}

		Con_Printf ("Multitexture extensions found\n");
		gl_mtexable = true;
	}

	glGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &gl_textureunits);

	gl_textureunits = min(gl_textureunits, 4);

	if (COM_CheckParm("-maxtmu2"))
		gl_textureunits = min(gl_textureunits, 2);

	if (gl_textureunits < 2)
		gl_mtexable = false;

	if (!gl_mtexable)
		gl_textureunits = 1;
	else
		Con_Printf ("Enabled %i texture units on hardware\n", gl_textureunits);
}

/*
===============
GL_SetupState -- johnfitz

does all the stuff from GL_Init that needs to be done every time a new GL render context is created
GL_Init will still do the stuff that only needs to be done once
===============
*/
void GL_SetupState (void)
{
	glClearColor (0.1, 0.1, 0.1, 0);//was 1,0,0,0 (red)
	glCullFace (GL_FRONT);

	glEnable (GL_TEXTURE_2D);

	glEnable (GL_ALPHA_TEST);
	glAlphaFunc (GL_GREATER, 0.666f);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); //johnfitz
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

/*
===============
GL_CheckExtensions -- johnfitz

mh - multiple changes here

why the fuck don't SGI/ARB/Khronos/whoever just release proper headers and libs with a well-defined API for
easily checking availability of a cap?  This shit is awful.

SIGH - some hardware and OS combinations kick and scream unless you specifically get the -ARB or whatever shit versions
of entrypoints.  OpenGL drivers still suck, my friend.
===============
*/
#ifdef USESHADERS
//void GL_GetCVAExtensions (void);
qboolean GL_GetGLSLExtensions (void);
#endif
void GL_CheckExtensions (void)
{
	if (!gl_extensions)
	{
		Sys_Error ("glGetString (GL_EXTENSIONS) returned NULL!");
		return;
	}
#ifdef USESHADERS

//	GL_GetCVAExtensions ();
	GL_GetBufferExtensions ();

	if (!GL_GetGLSLExtensions ())
	{
		gl_vbo = false;
		Con_Warning("Could not load vertex/fragment program extensions!\n");
	}
#endif
}
/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
	gl_vendor = glGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);
	gl_version = glGetString (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	Con_Print ("\n");

	gl_extensions = glGetString (GL_EXTENSIONS);
	if (COM_CheckParm("-gl_ext"))
		Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	if (!Q_strncasecmp((char *)gl_renderer, "PowerVR", 7))
		fullsbardraw = true;

	CheckMultiTextureExtensions ();	
	
#ifdef USESHADERS
	// assume we have no vendor specific behavior needed (...yet...!)
	gl_brokendrivers.AMDATIBroken = false;
	gl_brokendrivers.NVIDIABroken = false;
	gl_brokendrivers.IntelBroken = false;

	// to do - AMD/ATI string???
	if (Q_strstr (gl_vendor, "Intel")) gl_brokendrivers.IntelBroken = true;
	if (Q_strstr (gl_vendor, "NVIDIA")) gl_brokendrivers.NVIDIABroken = true;
#endif
	GL_CheckExtensions (); // johnfitz

#ifdef USESHADERS
	//From RMQ
	// unbind all vertex and index arrays
	GL_UncacheArrays ();

	// get a batch of vertex buffers
	// we do it this way because our vbos go NUTS on a vid mode switch from windowed to fullscreen (or vice-versa) but oddly enough
	// not on a switch between 2 different modes of the same type.  
	GL_CreateBuffers ();

	// begin vertex buffers
	GL_BeginBuffers ();
#endif

	GL_SetupState();
}

void Check_Gamma (unsigned char *pal)
{
	float		inf;
	unsigned char	palette[768];
	int		i;

	if ((i = COM_CheckParm("-gamma")) != 0 && i+1 < com_argc)
		vid_gamma = bound(0.3, Q_atof(com_argv[i+1]), 1);
	else
		vid_gamma = 1;

	Cvar_SetValue ("gl_gamma", vid_gamma);

	if (vid_gamma != 1)
	{
		for (i=0 ; i<256 ; i++)
		{
			inf = min(255 * pow((i + 0.5) / 255.5, vid_gamma) + 0.5, 255);
			vid_gamma_table[i] = inf;
		}
	}
	else
	{
		for (i=0 ; i<256 ; i++)
			vid_gamma_table[i] = i;
	}

	for (i=0 ; i<768 ; i++)
		palette[i] = vid_gamma_table[pal[i]];

	memcpy (pal, palette, sizeof(palette));
}

void VID_SetPalette (unsigned char *palette)
{
	byte		*pal;
	int		i;
	unsigned	r, g, b, *table;

// 8 8 8 encoding
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = (pal[0]);
		g = (pal[1]);
		b = (pal[2]);

		pal += 3;
		*table++ = (255<<24) + (r<<0) + (g<<8) + (b<<16);
	}
	d_8to24table[255] = 0;	// 255 is transparent
}

