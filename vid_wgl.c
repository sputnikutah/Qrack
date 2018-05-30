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
// vid_wgl.c -- Windows 9x/NT OpenGL driver

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"
#include <commctrl.h>

#ifdef USESHADERS
gl_brokendrivers_t gl_brokendrivers;
#endif

#define MAX_MODE_LIST	600
#define VID_ROW_SIZE	3
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

#define MODE_WINDOWED		0
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

qboolean have_stencil = false; // Stencil shadows - MrG

typedef struct 
{
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			refreshrate; //johnfitz
	int			halfscreen;
	char		modedesc[17];
} vmode_t;

typedef struct 
{
	int		width;
	int		height;
} lmode_t;

lmode_t	lowresmodes[] = 
{
	{320, 200},
	{320, 240},
	{400, 300},
	{512, 384},
};

qboolean	DDActive;
qboolean	scr_skipupdate;
static		vmode_t	modelist[MAX_MODE_LIST];

static		int	nummodes;
//static	vmode_t	*pcurrentmode;
static		vmode_t	badmode;

static	DEVMODE		gdevmode;
static	qboolean	vid_initialized = false;
static	qboolean	windowed, leavecurrentmode;
static	qboolean	vid_canalttab = false;
static	qboolean	vid_wassuspended = false;
static	int			windowed_mouse;
extern	qboolean	mouseactive;	// from in_win.c
static	HICON		hIcon;

qboolean	gl_add_ext = false;
qboolean	g_canUseTexComp = false;
qboolean	gl_filtering_anisotropic = false;

int			DIBWidth, DIBHeight;
RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;

HWND		mainwindow, dibwindow;

int			vid_modenum = NO_MODE;
int			vid_default = MODE_WINDOWED;
qboolean	vid_locked = false; //johnfitz
static int	windowed_default;
unsigned char	vid_curpal[256*3];
qboolean	fullsbardraw = false;

HDC		maindc;

glvert_t	glv;

unsigned short	*currentgammaramp = NULL;
static unsigned short systemgammaramp[3][256];

qboolean	vid_gammaworks = false;
qboolean	vid_hwgamma_enabled = false;
qboolean	customgamma = false;

void RestoreHWGamma (void);

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);
#ifdef USESHADERS
PFNGLBINDBUFFERPROC qglBindBuffer = NULL;
PFNGLDELETEBUFFERSPROC qglDeleteBuffers = NULL;
PFNGLGENBUFFERSPROC qglGenBuffers = NULL;
PFNGLISBUFFERPROC qglIsBuffer = NULL;
PFNGLBUFFERDATAPROC qglBufferData = NULL;
PFNGLBUFFERSUBDATAPROC qglBufferSubData = NULL;
PFNGLGETBUFFERSUBDATAPROC qglGetBufferSubData = NULL;
PFNGLMAPBUFFERPROC qglMapBuffer = NULL;
PFNGLUNMAPBUFFERPROC qglUnmapBuffer = NULL;
PFNGLGETBUFFERPARAMETERIVPROC qglGetBufferParameteriv = NULL;
PFNGLGETBUFFERPOINTERVPROC qglGetBufferPointerv = NULL;
// vertex/fragment program
PFNGLBINDPROGRAMARBPROC qglBindProgramARB = NULL;
PFNGLDELETEPROGRAMSARBPROC qglDeleteProgramsARB = NULL;
PFNGLGENPROGRAMSARBPROC qglGenProgramsARB = NULL;
PFNGLGETPROGRAMENVPARAMETERDVARBPROC qglGetProgramEnvParameterdvARB = NULL;
PFNGLGETPROGRAMENVPARAMETERFVARBPROC qglGetProgramEnvParameterfvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC qglGetProgramLocalParameterdvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC qglGetProgramLocalParameterfvARB = NULL;
PFNGLGETPROGRAMSTRINGARBPROC qglGetProgramStringARB = NULL;
PFNGLGETPROGRAMIVARBPROC qglGetProgramivARB = NULL;
PFNGLISPROGRAMARBPROC qglIsProgramARB = NULL;
PFNGLPROGRAMENVPARAMETER4DARBPROC qglProgramEnvParameter4dARB = NULL;
PFNGLPROGRAMENVPARAMETER4DVARBPROC qglProgramEnvParameter4dvARB = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC qglProgramEnvParameter4fARB = NULL;
PFNGLPROGRAMENVPARAMETER4FVARBPROC qglProgramEnvParameter4fvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DARBPROC qglProgramLocalParameter4dARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DVARBPROC qglProgramLocalParameter4dvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC qglProgramLocalParameter4fARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC qglProgramLocalParameter4fvARB = NULL;
PFNGLPROGRAMSTRINGARBPROC qglProgramStringARB = NULL;

PFNGLVERTEXATTRIBPOINTERARBPROC qglVertexAttribPointerARB = NULL;
PFNGLENABLEVERTEXATTRIBARRAYARBPROC qglEnableVertexAttribArrayARB = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC qglDisableVertexAttribArrayARB = NULL;

PFNGLDRAWRANGEELEMENTSPROC qglDrawRangeElements = NULL;
PFNGLLOCKARRAYSEXTPROC qglLockArrays = NULL;
PFNGLUNLOCKARRAYSEXTPROC qglUnlockArrays = NULL;
PFNGLMULTIDRAWARRAYSPROC qglMultiDrawArrays = NULL;

qboolean gl_ext_compiled_vertex_array = false;
#endif
modestate_t	modestate = MS_UNINIT;

void VID_Menu_Init (void); //johnfitz
void VID_Menu_f (void); //johnfitz
void VID_MenuDraw (void);
void VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate (BOOL fActive, BOOL minimize);
char *VID_GetModeDescription (int mode);

void VID_UpdateWindowStatus (void);
void GL_SetupState (void);
void GL_Init (void);
//====================================

cvar_t	vid_fullscreen			= {"vid_fullscreen", "1", true};
cvar_t	vid_width				= {"vid_width", "640", true};
cvar_t	vid_height				= {"vid_height", "480", true};
cvar_t	vid_bpp					= {"vid_bpp", "32", true};
cvar_t  vid_borderless			= {"vid_borderless","0",true};
cvar_t	vid_refreshrate			= {"vid_refreshrate", "0",true};		// R00k used internally
cvar_t	vid_mode				= {"vid_mode","0", false};				// Note that 0 is MODE_WINDOWED
cvar_t	_vid_default_mode		= {"_vid_default_mode","0", true};		// Note that 3 is MODE_FULLSCREEN_DEFAULT
cvar_t	_vid_default_mode_win	= {"_vid_default_mode_win","3", true};
cvar_t	vid_force_aspect_ratio	= {"vid_force_aspect_ratio","1", true}; // force 2d images to conform to the monitor's native aspect
cvar_t	vid_config_x			= {"vid_config_x","800", true};
cvar_t	vid_config_y			= {"vid_config_y","600", true};
cvar_t	vid_stretch_by_2		= {"vid_stretch_by_2","1", true};
cvar_t	_windowed_mouse			= {"_windowed_mouse","1", true};

qboolean OnChange_con_textsize (cvar_t *var, char *string);
cvar_t	con_textsize = {"con_textsize", "8",true, false, OnChange_con_textsize};

static	qboolean update_vsync = true;
qboolean gl_swap_control = false; //johnfitz

qboolean OnChange_vid_vsync(cvar_t *var, char *string) 
{
	update_vsync = true;
	Cbuf_AddText ("vid_restart\n");
	return false;
}
cvar_t	vid_vsync = {"vid_vsync", "0",true,false,OnChange_vid_vsync};//R00k set to 0 default to reduce DIB errors


//R00k realtime console resizing
extern mpic_t *conback;
qboolean OnChange_vid_conheight (cvar_t *var, char *string)
{	
	vid.height = bound(200, (atoi(string)), vid_height.value);
	//vid.height &= 0xfff8;// make it a multiple of eight
	Cvar_SetValue("vid_conheight", vid.height);
	conback->height = vid.conheight = vid.height;	
		
	vid.recalc_refdef = 1;

	return true;
}
cvar_t	vid_conheight = {"vid_conheight", "480", true, false, OnChange_vid_conheight};

qboolean OnChange_vid_conwidth (cvar_t *var, char *string)
{	
	extern cvar_t vid_conwidth;

	vid.width = bound(320, (atoi(string)), vid_width.value);
	vid.width &= 0xfff8;// make it a multiple of eight
	Cvar_SetValue("vid_conwidth", vid.width);

	//pick a conheight that matches the correct aspect
	if ((vid_force_aspect_ratio.value)&&(!windowed))
		vid.height = (vid.width * vid.nativeaspect);//Set aspect to the monitor's native resolution not current mode.
	else
		vid.height = (vid.width * vid.aspect);

	vid.height = bound(200, vid.height, vid_height.value);
	
	Cvar_SetValue("vid_conheight", vid.height);

	conback->width = vid.conwidth = vid.width;
	conback->height = vid.conheight = vid.height;

	vid.recalc_refdef = 1;
	return true;
}
cvar_t	vid_conwidth = {"vid_conwidth", "640", true, false, OnChange_vid_conwidth};
//R00k realtime console resizing--end

cvar_t		vid_hwgammacontrol		= {"vid_hwgammacontrol", "2"};//R00k changed to 2 to support windowed modes!
int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

typedef BOOL (APIENTRY *SETSWAPFUNC) (int); //johnfitz
typedef int (APIENTRY *GETSWAPFUNC) (void); //johnfitz

SETSWAPFUNC wglSwapIntervalEXT		= NULL; //johnfitz
GETSWAPFUNC wglGetSwapIntervalEXT	= NULL; //johnfitz

qboolean CheckExtension (const char *extension);
void CheckVsyncControlExtensions(void) 
{
    if (CheckExtension("WGL_EXT_swap_control") || CheckExtension("GL_EXT_swap_control"))	//WGL_EXT_swap_control
	{
		wglSwapIntervalEXT = (SETSWAPFUNC) wglGetProcAddress("wglSwapIntervalEXT");
		wglGetSwapIntervalEXT = (GETSWAPFUNC) wglGetProcAddress("wglGetSwapIntervalEXT");

		if (wglSwapIntervalEXT && wglGetSwapIntervalEXT)
		{
			if (!wglSwapIntervalEXT(0))
				Con_Printf("Wарнинг: vertical sync not supported (wglSwapIntervalEXT failed)\n");
			else if (wglGetSwapIntervalEXT() == -1)
				Con_Printf("Wарнинг: vertical sync not supported (swap interval is -1.) Make sure you don't have vertical sync disabled in your driver settings.\n");
			else
			{
				Con_Printf("VSYNC Support Enabled\n");
				Cvar_RegisterVariable (&vid_vsync);
				gl_swap_control = true;
			}
		}
		else
			Con_Printf ("арнинг: vertical sync not supported (wglGetProcAddress failed)\n");
	}
//	else
//		Con_Printf ("арнинг: vertical sync not supported (extension not found)\n");
}
#ifdef USESHADERS
#define polyblend_vp \
	"!!ARBvp1.0\n" \
	"MOV result.position, vertex.attrib[0];\n" \
	"MOV result.color, program.local[0];\n" \
	"END\n"

#define bbox_vp \
	"!!ARBvp1.0\n" \
	TRANSFORMVERTEX ("result.position", "vertex.attrib[0]") \
	"MOV result.color, program.local[0];\n" \
	"END\n"

#define polyblend_fp \
	"!!ARBfp1.0\n" \
	"MOV result.color, fragment.color;\n" \
	"END\n"

#define showtris_fp \
	"!!ARBfp1.0\n" \
	"MOV result.color, {0.666, 0.666, 0.666, 1};\n" \
	"END\n"

GLuint arb_bbox_vp = 0;
GLuint arb_polyblend_vp = 0;
GLuint arb_polyblend_fp = 0;
GLuint arb_showtris_fp = 0;


#define blur_vp \
	"!!ARBvp1.0\n" \
	"TEMP tc0;\n" \
	"MOV result.position, vertex.attrib[0];\n" \
	"MOV result.color, vertex.attrib[1];\n" \
	"MOV result.texcoord[0], vertex.attrib[2];\n" \
	"MOV result.texcoord[1], program.local[0];\n" \
	"MOV result.texcoord[2], vertex.attrib[0];\n" \
	"END\n"

#define blur_fp \
	"!!ARBfp1.0\n" \
	"TEMP col0;\n" \
	"DP3 col0, fragment.texcoord[2], fragment.texcoord[2];\n" \
	"MUL col0, col0, program.local[0].w;\n" \
	"MIN col0, col0, 0.95;\n" \
	"TEMP tex0;\n" \
	"TEX tex0, fragment.texcoord[0], texture[0], 2D;\n" \
	"MUL tex0, tex0, fragment.color;\n" \
	"MUL tex0.w, tex0.w, col0.w;\n" \
	"LRP result.color, fragment.texcoord[1].a, fragment.texcoord[1], tex0;\n" \
	"END\n"


GLuint arb_blur_vp = 0;
GLuint arb_blur_fp = 0;

#define underwater_vp \
	"!!ARBvp1.0\n" \
	"TEMP tc0;\n" \
	"MOV result.position, vertex.attrib[0];\n" \
	"MAD tc0, vertex.attrib[2], 2.0, program.local[0].x;\n" \
	"MUL result.texcoord[1], tc0, 0.024543;\n" \
	"MOV result.texcoord[0], vertex.attrib[2];\n" \
	"MOV result.color, vertex.attrib[1];\n" \
	"END\n"

#define underwater_fp \
	"!!ARBfp1.0\n" \
	"TEMP tc0, tc1;\n" \
	"SIN tc0.x, fragment.texcoord[1].y;\n" \
	"SIN tc0.y, fragment.texcoord[1].x;\n" \
	"MAD tc1, tc0, program.local[0].z, fragment.texcoord[0];\n" \
	"MUL tc0, tc1, program.local[0].y;\n" \
	"TEX tc1, tc0, texture[0], 2D;\n" \
	"LRP result.color, fragment.color.a, fragment.color, tc1;\n" \
	"END\n"

GLuint arb_underwater_vp = 0;
GLuint arb_underwater_fp = 0;

void Main_CreateShaders (void)
{
	arb_underwater_vp = GL_CreateProgram (GL_VERTEX_PROGRAM_ARB, underwater_vp, false);
	arb_underwater_fp = GL_CreateProgram (GL_FRAGMENT_PROGRAM_ARB, underwater_fp, false);
/*
	arb_blur_vp = GL_CreateProgram (GL_VERTEX_PROGRAM_ARB, blur_vp, false);
	arb_blur_fp = GL_CreateProgram (GL_FRAGMENT_PROGRAM_ARB, blur_fp, false);

	arb_bbox_vp = GL_CreateProgram (GL_VERTEX_PROGRAM_ARB, bbox_vp, false);
*/
	arb_polyblend_vp = GL_CreateProgram (GL_VERTEX_PROGRAM_ARB, polyblend_vp, false);
	arb_polyblend_fp = GL_CreateProgram (GL_FRAGMENT_PROGRAM_ARB, polyblend_fp, false);
/*
	arb_showtris_fp = GL_CreateProgram (GL_FRAGMENT_PROGRAM_ARB, showtris_fp, false);
*/
}

// CVA replacment funcs so that I can keep a consistent API
/*
void APIENTRY glLockArraysRMQ (GLint blah1, GLsizei blah2) {}
void APIENTRY glUnlockArraysRMQ (void) {}

void GL_GetCVAExtensions (void)
{
	if (Q_strstr (gl_extensions, "GL_EXT_compiled_vertex_array "))
	{
		qglLockArrays = (PFNGLLOCKARRAYSEXTPROC) wglGetProcAddress ("glLockArraysEXT");
		qglUnlockArrays = (PFNGLUNLOCKARRAYSEXTPROC) wglGetProcAddress ("glUnlockArraysEXT");

		if (qglLockArrays && qglUnlockArrays)
		{
			gl_ext_compiled_vertex_array = true;
			Con_Printf ("FOUND: EXT_compiled_vertex_array\n");
		}
		else Con_Warning ("GL_EXT_compiled_vertex_array not supported (GetProcAddress failed)\n");
	}
	else Con_Warning ("GL_EXT_compiled_vertex_array not supported\n");

	// set up our replacement CVA functions
	if (!qglLockArrays || !qglUnlockArrays)
	{
		qglLockArrays = glLockArraysRMQ;
		qglUnlockArrays = glUnlockArraysRMQ;
	}
}
*/
void GL_GetBufferExtensions (void)
{
	qglBindBuffer = (PFNGLBINDBUFFERPROC) wglGetProcAddress ("glBindBufferARB");
	qglDeleteBuffers = (PFNGLDELETEBUFFERSPROC) wglGetProcAddress ("glDeleteBuffersARB");
	qglGenBuffers = (PFNGLGENBUFFERSPROC) wglGetProcAddress ("glGenBuffersARB");
	qglIsBuffer = (PFNGLISBUFFERPROC) wglGetProcAddress ("glIsBufferARB");
	qglBufferData = (PFNGLBUFFERDATAPROC) wglGetProcAddress ("glBufferDataARB");
	qglBufferSubData = (PFNGLBUFFERSUBDATAPROC) wglGetProcAddress ("glBufferSubDataARB");
	qglGetBufferSubData = (PFNGLGETBUFFERSUBDATAPROC) wglGetProcAddress ("glGetBufferSubDataARB");
	qglMapBuffer = (PFNGLMAPBUFFERPROC) wglGetProcAddress ("glMapBufferARB");
	qglUnmapBuffer = (PFNGLUNMAPBUFFERPROC) wglGetProcAddress ("glUnmapBufferARB");
	qglGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC) wglGetProcAddress ("glGetBufferParameterivARB");
	qglGetBufferPointerv = (PFNGLGETBUFFERPOINTERVPROC) wglGetProcAddress ("glGetBufferPointervARB");

	if (qglBindBuffer && qglDeleteBuffers && qglGenBuffers && qglIsBuffer &&
		qglBufferData && qglBufferSubData && qglGetBufferSubData && qglMapBuffer &&
		qglUnmapBuffer && qglGetBufferParameteriv && qglGetBufferPointerv)
	{
		Con_Printf ("FOUND: ARB_vertex_buffer_object\n");
		gl_vbo = true;
	}
	else
	{
		gl_vbo = false;
	}
}
qboolean GL_GetGLSLExtensions (void)
{
	extern void R_DeleteShaders (void);
	extern void GL_SetPrograms (GLuint vp, GLuint fp);
	extern void GLSLGamma_CreateShaders (void);
	// let's use the proper OpenGL 2.0 extensions here
	if (!Q_strstr (gl_extensions, "GL_ARB_vertex_program ")) return false;
	if (!Q_strstr (gl_extensions, "GL_ARB_fragment_program ")) return false;

	if (!(qglBindProgramARB = (PFNGLBINDPROGRAMARBPROC) wglGetProcAddress ("glBindProgramARB"))) return false;
	if (!(qglDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC) wglGetProcAddress ("glDeleteProgramsARB"))) return false;
	if (!(qglGenProgramsARB = (PFNGLGENPROGRAMSARBPROC) wglGetProcAddress ("glGenProgramsARB"))) return false;
	if (!(qglGetProgramEnvParameterdvARB = (PFNGLGETPROGRAMENVPARAMETERDVARBPROC) wglGetProcAddress ("glGetProgramEnvParameterdvARB"))) return false;
	if (!(qglGetProgramEnvParameterfvARB = (PFNGLGETPROGRAMENVPARAMETERFVARBPROC) wglGetProcAddress ("glGetProgramEnvParameterfvARB"))) return false;
	if (!(qglGetProgramLocalParameterdvARB = (PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC) wglGetProcAddress ("glGetProgramLocalParameterdvARB"))) return false;
	if (!(qglGetProgramLocalParameterfvARB = (PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC) wglGetProcAddress ("glGetProgramLocalParameterfvARB"))) return false;
	if (!(qglGetProgramStringARB = (PFNGLGETPROGRAMSTRINGARBPROC) wglGetProcAddress ("glGetProgramStringARB"))) return false;
	if (!(qglGetProgramivARB = (PFNGLGETPROGRAMIVARBPROC) wglGetProcAddress ("glGetProgramivARB"))) return false;
	if (!(qglIsProgramARB = (PFNGLISPROGRAMARBPROC) wglGetProcAddress ("glIsProgramARB"))) return false;
	if (!(qglProgramEnvParameter4dARB = (PFNGLPROGRAMENVPARAMETER4DARBPROC) wglGetProcAddress ("glProgramEnvParameter4dARB"))) return false;
	if (!(qglProgramEnvParameter4dvARB = (PFNGLPROGRAMENVPARAMETER4DVARBPROC) wglGetProcAddress ("glProgramEnvParameter4dvARB"))) return false;
	if (!(qglProgramEnvParameter4fARB = (PFNGLPROGRAMENVPARAMETER4FARBPROC) wglGetProcAddress ("glProgramEnvParameter4fARB"))) return false;
	if (!(qglProgramEnvParameter4fvARB = (PFNGLPROGRAMENVPARAMETER4FVARBPROC) wglGetProcAddress ("glProgramEnvParameter4fvARB"))) return false;
	if (!(qglProgramLocalParameter4dARB = (PFNGLPROGRAMLOCALPARAMETER4DARBPROC) wglGetProcAddress ("glProgramLocalParameter4dARB"))) return false;
	if (!(qglProgramLocalParameter4dvARB = (PFNGLPROGRAMLOCALPARAMETER4DVARBPROC) wglGetProcAddress ("glProgramLocalParameter4dvARB"))) return false;
	if (!(qglProgramLocalParameter4fARB = (PFNGLPROGRAMLOCALPARAMETER4FARBPROC) wglGetProcAddress ("glProgramLocalParameter4fARB"))) return false;
	if (!(qglProgramLocalParameter4fvARB = (PFNGLPROGRAMLOCALPARAMETER4FVARBPROC) wglGetProcAddress ("glProgramLocalParameter4fvARB"))) return false;
	if (!(qglProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC) wglGetProcAddress ("glProgramStringARB"))) return false;
	if (!(qglVertexAttribPointerARB = (PFNGLVERTEXATTRIBPOINTERARBPROC) wglGetProcAddress ("glVertexAttribPointerARB"))) return false;
	if (!(qglEnableVertexAttribArrayARB = (PFNGLENABLEVERTEXATTRIBARRAYARBPROC) wglGetProcAddress ("glEnableVertexAttribArrayARB"))) return false;
	if (!(qglDisableVertexAttribArrayARB = (PFNGLDISABLEVERTEXATTRIBARRAYARBPROC) wglGetProcAddress ("glDisableVertexAttribArrayARB"))) return false;

	// these are optional so we don't want to print a warning if they don't exist
	Con_Printf ("FOUND: GL_ARB_vertex_program\n");
	Con_Printf ("FOUND: GL_ARB_fragment_program\n");
	// clear down anything we had
	R_DeleteShaders ();

	// now load them up again
/*
	Warp_CreateShaders ();	
	Draw_CreateShaders ();
	Sky_CreateShaders ();
	Part_CreateShaders ();

	Mesh_CreateShaders ();

	IQM_CreateShaders ();
	Main_CreateShaders ();
	World_CreateShaders ();
	Sprite_CreateShaders ();
	Corona_CreateShaders ();
*/
	Main_CreateShaders ();
	{
		// now enable our shaders
		glEnable (GL_VERTEX_PROGRAM_ARB);
		glEnable (GL_FRAGMENT_PROGRAM_ARB);

		// bind the default shaders (also updates cached state)
		GL_SetPrograms (0,0);

		glDisable (GL_VERTEX_PROGRAM_ARB);
		glDisable (GL_FRAGMENT_PROGRAM_ARB);
	}
	return true;
}
#endif

/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	int	i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
		Key_Event (i, false);

	Key_ClearStates ();
	IN_ClearStates ();
}

/*
===============
VID_Vsync_f -- johnfitz
===============
*/
void VID_Vsync_f (void)
{
	if (gl_swap_control)
	{
		if (vid_vsync.value)
		{
			if (!wglSwapIntervalEXT(1))
				Con_DPrintf (1,"VID_Vsync_f: failed on wglSwapIntervalEXT\n");
		}
		else
		{
			if (!wglSwapIntervalEXT(0))
				Con_DPrintf (1,"VID_Vsync_f: failed on wglSwapIntervalEXT\n");
		}
	}
}


void GL_Init_Win(void) 
{
	gl_add_ext					= CheckExtension ("GL_ARB_texture_env_add");
//	gl_combine					= CheckExtension ("GL_ARB_texture_env_combine");
	gl_filtering_anisotropic	= CheckExtension ("GL_EXT_texture_filter_anisotropic");
	g_canUseTexComp				= ((CheckExtension("GL_ARB_texture_compression")) || (CheckExtension("GL_EXT_texture_compression_s3tc")));

	CheckVsyncControlExtensions();
}

// direct draw software compatability stuff

void VID_HandlePause (qboolean pause)
{
}

void VID_LockBuffer (void)
{
}

void VID_UnlockBuffer (void)
{
}

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

int CenterX, CenterY;
void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
	//int     CenterX, CenterY;
	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY*2)
		CenterX >>= 1;	// dual screens
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0,	SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}


void VID_SetWindowText (void)
{
	char *ver;
	extern char server_name[MAX_QPATH];

	ver = VersionString ();

	if ((cls.state != ca_connected) && (!sv.active)) 
	{
		SetWindowText(dibwindow, va("Qrack v%s", ver));//Just sitting in the console.
	}
	else
	{
		if (cls.demoplayback)
		{
			SetWindowText(dibwindow, va("Qrack v%s Demo: %s Map: %s %s", ver, cls.demoname, cls.mapstring, cl.levelname));
		}
		else
		{
			if (cls.state == ca_connected)
			{
				SetWindowText(dibwindow, va("Qrack v%s Server: %s  Map: %s %s", ver, server_name, cls.mapstring, cl.levelname));
			}
		}
	}
}

qboolean VID_SetWindowedMode (int modenum)
{
	HDC		hdc;
	int		lastmodestate, width, height;
	RECT	rect;
	
	WindowRect.top = WindowRect.left = 0;

	lastmodestate = modestate;	

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	if (vid_borderless.value)
		WindowStyle = WS_OVERLAPPED | WS_CHILD | WS_CLIPCHILDREN | WS_SYSMENU;
	else
		WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx (&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
		 "Qrack",
		 "Qrack",
		 WindowStyle,
		 rect.left, rect.top,
		 width,
		 height,
		 (WindowStyle & WS_CHILD) ? GetDesktopWindow() : NULL, // child require parent window
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	// Center and show the DIB window
	CenterWindow(dibwindow, WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top, false);

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	modestate = MS_WINDOWED;

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	vid.height	= vid.conheight = DIBHeight;
	vid.width	= vid.conwidth = DIBWidth;
	vid.aspect = ((float)vid.height/(float)vid.width);//R00k: using current height/width since it's a window.

	vid.numpages = 2;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);
	VID_SetWindowText();
	return true;
}

qboolean VID_SetFullDIBMode (int modenum)
{
	HDC	hdc;
	int	lastmodestate, width, height;
	RECT	rect;

	if (!leavecurrentmode)
	{
		gdevmode.dmSize				= sizeof (gdevmode);
		gdevmode.dmFields			= DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
		gdevmode.dmBitsPerPel		= modelist[modenum].bpp;
		gdevmode.dmPelsWidth		= modelist[modenum].width << modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight		= modelist[modenum].height;		
		gdevmode.dmDisplayFrequency = modelist[modenum].refreshrate; 
		gdevmode.dmFields |= DM_DISPLAYFREQUENCY;				
		if (ChangeDisplaySettings(&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		{
			Sys_Error ("Couldn't set fullscreen DIB mode\n Use -current in commandline.");				
		}
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;
	
	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

// Create the DIB window
	dibwindow = CreateWindowEx (
	ExWindowStyle,
	"Qrack",
	"Qrack",
	WindowStyle,
	rect.left, 
	rect.top,
	width,
	height,
	NULL,
	NULL,
	global_hInstance,
	NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop),
	// we clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	vid.height	= DIBHeight;
	vid.width	= DIBWidth;
	vid.aspect = ((float)vid.height / (float)vid.width);
	conback->width = vid.conwidth = vid.width;
	conback->height = vid.conheight = vid.height;

	vid.numpages = 2;

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);
	return true;
}

int VID_SetMode (int modenum, unsigned char *palette)
{
	int		original_mode, temp;
	qboolean	stat;
	MSG		msg;

	if ((windowed && modenum) || (!windowed && (modenum < 1)) || (!windowed && (modenum >= nummodes)))
		Sys_Error ("Bad video mode");

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED)
	{
		if (_windowed_mouse.value && key_dest == key_game)
		{
			stat = VID_SetWindowedMode(modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode(modenum);
		}
	}
	else if (modelist[modenum].type == MS_FULLDIB)
	{
		stat = VID_SetFullDIBMode(modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	}
	else
	{
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
	}

	if (!stat)
		Sys_Error ("Couldn't set video mode");

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;


// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	VID_SetPalette (palette);//R00k needed here?
	vid_modenum = modenum;
	Cvar_SetValue ("vid_mode", (float)vid_modenum);

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}
	
	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates();

	if (!msg_suppress_1)
		Con_SafePrintf ("Video mode %s initialized\n", VID_GetModeDescription (vid_modenum));
	
	VID_SetPalette (palette);

	vid.recalc_refdef = 1;

	//R00k mouse died on mode change
	IN_Shutdown ();
	IN_StartupMouse();
	IN_ActivateMouse ();
	IN_HideMouse ();

	return true;
}
/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{
	RECT	monitor_rect;

	// Get the current monitor and info about it.
	hCurrMon = VID_GetCurrentMonitor();
	currMonInfo = VID_GetCurrentMonitorInfo(hCurrMon);

	monitor_rect = currMonInfo.rcMonitor;
	GetWindowRect( mainwindow, &window_rect);

	if (window_rect.left < monitor_rect.left)
		window_rect.left = monitor_rect.left;
	if (window_rect.top < monitor_rect.top)
		window_rect.top = monitor_rect.top;
	if (window_rect.right >= monitor_rect.right)
		window_rect.right = monitor_rect.right - 1;
	if (window_rect.bottom >= monitor_rect.bottom)
		window_rect.bottom = monitor_rect.bottom - 1;
	
	window_center_x = (window_rect.right + window_rect.left) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	window_rect = monitor_rect;//R00k
	
	IN_UpdateClipCursor ();
}




//=================================================================

/*
=================
GL_BeginRendering
=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = WindowRect.right - WindowRect.left;
	*height = WindowRect.bottom - WindowRect.top;
}

/*
=================
GL_EndRendering
=================
*/
void GL_EndRendering (void)
{
	static qboolean	old_hwgamma_enabled;

	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks && ActiveApp && !Minimized;
	vid_hwgamma_enabled = vid_hwgamma_enabled && (modestate == MS_FULLDIB || vid_hwgammacontrol.value == 2);
	
	if (vid_hwgamma_enabled != old_hwgamma_enabled)
	{
		old_hwgamma_enabled = vid_hwgamma_enabled;
	
		if (vid_hwgamma_enabled && currentgammaramp)
		{
			VID_SetDeviceGammaRamp (currentgammaramp);
		}
		else
		{
			RestoreHWGamma ();			
		}
	}

	if (!scr_skipupdate || block_drawing)
	{
		if (wglSwapIntervalEXT && update_vsync && vid_vsync.string[0])//From FUHQuake
			wglSwapIntervalEXT(vid_vsync.value ? 1 : 0);

		update_vsync = false;

#ifdef USEFAKEGL
		FakeSwapBuffers ();
#else
		SwapBuffers(maindc);
#endif

	}
	// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if (!_windowed_mouse.value)
		{
			if (windowed_mouse)
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		}
		else
		{
			windowed_mouse = true;
			if (key_dest == key_game && !mouseactive && ActiveApp)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
			else if (mouseactive && key_dest != key_game)
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
	}

	if (fullsbardraw)
		Sbar_Changed ();
}

void VID_SetDefaultMode (void)
{
	IN_DeactivateMouse ();
}

void VID_ShiftPalette (unsigned char *palette)
{
}

/*
======================
VID_SetDeviceGammaRamp

Note: ramps must point to a static array
======================
*/
void VID_SetDeviceGammaRamp (unsigned short *ramps)
{
	HDC hdc;
	if (vid_gammaworks)
	{
		currentgammaramp = ramps;
		if (vid_hwgamma_enabled)
		{
			hdc = wglGetCurrentDC();
			SetDeviceGammaRamp (hdc, ramps);
			customgamma = true;
		}
	}
}

void InitHWGamma (void)
{
	if (COM_CheckParm("-nohwgamma"))
		return;

	vid_gammaworks = GetDeviceGammaRamp (maindc, systemgammaramp);
}

void RestoreHWGamma (void)
{
	if (vid_gammaworks && customgamma)
	{
		customgamma = false;
		SetDeviceGammaRamp (maindc, systemgammaramp);
	}
}

/*
================
VID_Gamma_Restore -- restore system gamma
================
*/
void VID_Gamma_Restore (void)
{
	if (maindc)
	{
		if (vid_gammaworks)
			if (SetDeviceGammaRamp(maindc, systemgammaramp) == false)
				Con_Printf ("VID_Gamma_Restore: failed on SetDeviceGammaRamp\n");
	}
}

/*
================
VID_Gamma_Shutdown -- called on exit
================
*/
void VID_Gamma_Shutdown (void)
{
	VID_Gamma_Restore ();
}

BOOL bSetupPixelFormat (HDC hDC)
{
	static PIXELFORMATDESCRIPTOR pfd = 
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,						// version number
		PFD_DRAW_TO_WINDOW 		// support window
		| PFD_SUPPORT_OPENGL 	// support OpenGL
		| PFD_DOUBLEBUFFER ,	// double buffered
		PFD_TYPE_RGBA,			// RGBA type
		24,						// 24-bit color depth
		0, 0, 0, 0, 0, 0,		// color bits ignored
		0,						// no alpha buffer
		0,						// shift bit ignored
		0,						// no accumulation buffer
		0, 0, 0, 0, 			// accum bits ignored
		24,						// 24-bit z-buffer	
		8,						// 8-bit stencil buffer
		0,						// no auxiliary buffer
		PFD_MAIN_PLANE,			// main layer
		0,						// reserved
		0, 0, 0					// layer masks ignored
	};
	int	pixelformat;

	if (!(pixelformat = ChoosePixelFormat(hDC, &pfd)))
	{
		MessageBox (NULL, "ChoosePixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	if (!SetPixelFormat(hDC, pixelformat, &pfd))
	{
		MessageBox (NULL, "SetPixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	if (&pfd.cStencilBits)//MH -  if you also have a stencil buffer, even if you don't actually use it, you should always clear it at the same time as you clear your depth buffer. Otherwise your performance will suffer quite a huge dropoff. 
	{
		have_stencil = true; // Stencil shadows - MrG
	}

	memset (&pfd, 0, sizeof (PIXELFORMATDESCRIPTOR));
	DescribePixelFormat (hDC, pixelformat, sizeof (PIXELFORMATDESCRIPTOR), &pfd);
	return TRUE;
}

/*
===================
VID_Restart -- johnfitz -- change video modes on the fly
===================
*/
void VID_Restart (void)
{
	HDC			hdc;
#ifndef USEFAKEGL
	HGLRC		hrc;
#endif
	int			i;
	qboolean	mode_changed = false;
	vmode_t		oldmode;

	if (vid_locked)
		return;
//
// check cvars against current mode
//
	if (vid_fullscreen.value)
	{
		if (modelist[vid_default].type == MS_WINDOWED)
			mode_changed = true;
		else if (modelist[vid_default].bpp != (int)vid_bpp.value)
			mode_changed = true;
		else if (modelist[vid_default].refreshrate != (int)vid_refreshrate.value)
			mode_changed = true;
	}
	else
	if (modelist[vid_default].type != MS_WINDOWED)
	{
		mode_changed = true;
		/*
		todo add cvars for windowed size
		Cvar_Set ("vid_width", va("%i", 640));
		Cvar_Set ("vid_height", va("%i",480));
		*/
	}

	if (modelist[vid_default].width != (int)vid_width.value || modelist[vid_default].height != (int)vid_height.value)
		mode_changed = true;

	if (mode_changed)
	{
//
// decide which mode to set
//
		oldmode = modelist[vid_default];

		if (vid_fullscreen.value)
		{
			for (i=1; i<nummodes; i++)
			{
				if (modelist[i].width == (int)vid_width.value &&
					modelist[i].height == (int)vid_height.value &&
					modelist[i].bpp == (int)vid_bpp.value &&
					modelist[i].refreshrate == (int)vid_refreshrate.value)
				{
					break;
				}
			}

			if (i == nummodes)
			{
				Con_Printf ("%dx%dx%d %dHz is not a valid fullscreen mode\n",
							(int)vid_width.value,
							(int)vid_height.value,
							(int)vid_bpp.value,
							(int)vid_refreshrate.value);
				return;
			}

			windowed = false;
			vid_default = i;
		}
		else //not fullscreen
		{
			hdc = GetDC (NULL);
			if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
			{
				Con_Printf ("Can't run windowed on non-RGB desktop\n");
				ReleaseDC (NULL, hdc);
				return;
			}

			ReleaseDC (NULL, hdc);

			if (vid_width.value < 320)
			{
				Con_Printf ("Window width can't be less than 320\n");
				return;
			}

			if (vid_height.value < 200)
			{
				Con_Printf ("Window height can't be less than 200\n");
				return;
			}

			modelist[0].width = (int)vid_width.value;
			modelist[0].height = (int)vid_height.value;

			modelist[0].refreshrate = (int)vid_refreshrate.value;
			sprintf (modelist[0].modedesc, "%dx%dx%d %dHz",	 modelist[0].width,	 modelist[0].height, modelist[0].bpp, modelist[0].refreshrate);

			windowed = true;
			vid_default = 0;
		}
//
// destroy current window
//
#ifdef USEFAKEGL
		// instead of destroying the window, context, etc we just need to resize the window and reset the device for D3D
		// we need this too
		vid_canalttab = false;

		// and reset the mode
		D3D_ResetMode (modelist[vid_default].width, modelist[vid_default].height, modelist[vid_default].bpp, windowed);

		// now fill in all the ugly globals that Quake stores the same data in over and over again
		// (this will be different for different engines)
		DIBWidth = window_width = WindowRect.right = modelist[vid_default].width;
		DIBHeight = window_height = WindowRect.bottom = modelist[vid_default].height;

		// these also needed
		VID_UpdateWindowStatus ();
		vid.recalc_refdef = 1;
#else
		hrc = wglGetCurrentContext();
		hdc = wglGetCurrentDC();
		wglMakeCurrent(NULL, NULL);

		vid_canalttab = false;

		if (hdc && dibwindow)
			ReleaseDC (dibwindow, hdc);
		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);
		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);
		maindc = NULL;
		if (dibwindow)
			DestroyWindow (dibwindow);		

//
// set new mode
//
		Cvar_SetValue ("vid_mode", _vid_default_mode_win.value);

		VID_SetMode (vid_default, host_basepal);
		maindc = GetDC(mainwindow);
		bSetupPixelFormat(maindc);

		vid.aspect = ((float)vid_height.value / (float)vid_width.value);
		
		// if bpp changes, recreate render context and reload textures
		if (modelist[vid_default].bpp != oldmode.bpp)
		{
			wglDeleteContext (hrc);
			hrc = wglCreateContext (maindc);
			if (!wglMakeCurrent (maindc, hrc))
				Sys_Error ("VID_Restart: wglMakeCurrent failed");
			GL_SetupState ();
		}
		else
			if (!wglMakeCurrent (maindc, hrc))
			{
				char szBuf[80];
				LPVOID lpMsgBuf;
				DWORD dw = GetLastError();
				FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL );
				sprintf(szBuf, "VID_Restart: wglMakeCurrent failed with error %d: %s", dw, (char *)lpMsgBuf);
 				Sys_Error (szBuf);
			}
#endif
		vid_canalttab = true;

		//swapcontrol settings were lost when previous window was destroyed
		VID_Vsync_f ();

		Cvar_SetValue("vid_conwidth", vid_conwidth.value);//Refresh the conback
	}
//
// keep cvars in line with actual mode
//
	Cvar_Set ("vid_width", va("%i", modelist[vid_default].width));
	Cvar_Set ("vid_height", va("%i", modelist[vid_default].height));
	Cvar_Set ("vid_bpp", va("%i", modelist[vid_default].bpp));
	Cvar_Set ("vid_refreshrate", va("%i", modelist[vid_default].refreshrate));
	Cvar_Set ("vid_fullscreen", (windowed) ? "0" : "1");
}

/*
================
VID_Test -- johnfitz -- like vid_restart, but asks for confirmation after switching modes
================
*/
void VID_Test (void)
{
	vmode_t oldmode;
	qboolean	mode_changed = false;

	if (vid_locked)
		return;
//
// check cvars against current mode
//
	if (vid_fullscreen.value)
	{
		if (modelist[vid_default].type == MS_WINDOWED)
			mode_changed = true;
		else if (modelist[vid_default].bpp != (int)vid_bpp.value)
			mode_changed = true;
		else if (modelist[vid_default].refreshrate != (int)vid_refreshrate.value)
			mode_changed = true;
	}
	else
		if (modelist[vid_default].type != MS_WINDOWED)
			mode_changed = true;

	if (modelist[vid_default].width != (int)vid_width.value ||
		modelist[vid_default].height != (int)vid_height.value)
		mode_changed = true;

	if (!mode_changed)
		return;
//
// now try the switch
//
	oldmode = modelist[vid_default];

	VID_Restart ();

	//pop up confirmation dialoge
	if (!SCR_ModalMessage("Would you like to keep this\nvideo mode? (y/n)\n"))
	{
		//revert cvars and mode
		Cvar_Set ("vid_width", va("%i", oldmode.width));
		Cvar_Set ("vid_height", va("%i", oldmode.height));
		Cvar_Set ("vid_bpp", va("%i", oldmode.bpp));
		Cvar_Set ("vid_refreshrate", va("%i", oldmode.refreshrate));
		Cvar_Set ("vid_fullscreen", (oldmode.type == MS_WINDOWED) ? "0" : "1");
		VID_Restart ();
	}
}

/*
================
VID_Unlock -- johnfitz
================
*/
void VID_Unlock (void)
{
	vid_locked = false;

	//sync up cvars in case they were changed during the lock
	Cvar_Set ("vid_width", va("%i", modelist[vid_default].width));
	Cvar_Set ("vid_height", va("%i", modelist[vid_default].height));
	Cvar_Set ("vid_bpp", va("%i", modelist[vid_default].bpp));
	Cvar_Set ("vid_refreshrate", va("%i", modelist[vid_default].refreshrate));
	Cvar_Set ("vid_fullscreen", (windowed) ? "0" : "1");
}

//=================================================================

void VID_Shutdown (void)
{
   	HGLRC	hRC;
   	HDC	hDC;
	extern void SwitchMonitors (void);

	if (vid_initialized)
	{
		RestoreHWGamma ();

		vid_canalttab = false;
		hRC = wglGetCurrentContext ();
		hDC = wglGetCurrentDC ();

		wglMakeCurrent (NULL, NULL);

		if (hRC)
			wglDeleteContext (hRC);

		VID_Gamma_Shutdown (); //johnfitz

		if (COM_CheckParm("-monitor2"))
		{		
			SwitchMonitors();
		}

		if (hDC && dibwindow)
			ReleaseDC (dibwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);

		AppActivate (false, false);
	}
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

void AppActivate (BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
	static BOOL	sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive)
	{		
		if (modestate == MS_FULLDIB)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();

			if (vid_canalttab && !Minimized && currentgammaramp && vid_hwgammacontrol.value)
			{			
				VID_SetDeviceGammaRamp (currentgammaramp);
			}
			if (vid_canalttab && vid_wassuspended)
			{
				vid_wassuspended = false;
				if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
					Sys_Error ("Couldn't set fullscreen DIB mode");
		
				ShowWindow (mainwindow, SW_SHOWNORMAL);
				
				// Fix for alt-tab bug in NVidia drivers
				MoveWindow (mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false);
				ClearAllStates();//needed?v2.011
				VID_UpdateWindowStatus();//R00k required for multi-mon?
				Sbar_Changed ();
				if (currentgammaramp && vid_hwgammacontrol.value)
					VID_SetDeviceGammaRamp (currentgammaramp);
			}
		}
		else if (modestate == MS_WINDOWED && Minimized)
			ShowWindow (mainwindow, SW_RESTORE);
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value && key_dest == key_game)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
			if (currentgammaramp && vid_hwgammacontrol.value)
				VID_SetDeviceGammaRamp (currentgammaramp);
		}
	}

	if (!fActive)
	{
		if (modestate == MS_FULLDIB || vid_hwgammacontrol.value == 2)
			SetDeviceGammaRamp (maindc, systemgammaramp);//FIXME: if multiple instances then jumping from client to client we get all messed up.

		if (modestate == MS_FULLDIB)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			
			if (vid_canalttab) 
			{ 
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}

LONG CDAudio_MessageHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int IN_MapKey (int key);

/*
=============
Main Window procedure
=============
*/
LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG	lRet = 0;//MH-GLQFIX
	int		fActive, fMinimized;
	extern unsigned	int uiWheelMessage;
	extern void IN_RawInput_MouseRead(HANDLE in_device_handle);
	extern cvar_t  m_raw_input;
	extern cvar_t	m_dinput;
	extern unsigned short	ramps[3][256];


	if (uMsg == uiWheelMessage)
	{
		uMsg = WM_MOUSEWHEEL;
		wParam <<= 16;
	}

	if (uMsg == WM_QUERYENDSESSION)
	{
		Sys_Quit ();
	}

	switch (uMsg)
	{
		if ((m_raw_input.value) && (!m_dinput.value))
		{
			case WM_INPUT:
			{
				// raw input handling
				IN_RawInput_MouseRead((HANDLE)lParam);
				break;
			}
		}

		case WM_KILLFOCUS:
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			Key_Event (IN_MapKey(lParam), true);
			break;
			

		case WM_KEYUP:
		case WM_SYSKEYUP:
			Key_Event (IN_MapKey(lParam), false);
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			{
				int i, temp = 0;
				int mbuttons[] = { MK_LBUTTON, MK_RBUTTON, MK_MBUTTON, MK_XBUTTON1, MK_XBUTTON2, MK_XBUTTON3, MK_XBUTTON4, MK_XBUTTON5 };//R00k 8 mouse button support

				for( i = 0; i < mouse_buttons; i++ )
					if( wParam & mbuttons[i] )
						temp |= ( 1<<i );

				IN_MouseEvent (temp);
			}
			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL: 
			if ((short) HIWORD(wParam) > 0) 
			{
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			} else {
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
			}
			break;

		case WM_SIZE:
		{/*	//R00k TODO resize-able!
			int nWidth  = LOWORD(lParam); 
			int nHeight = HIWORD(lParam);
			glViewport(0, 0, nWidth, nHeight);

			glMatrixMode( GL_PROJECTION );
			glLoadIdentity();
			gluPerspective( 45.0, (GLdouble)nWidth / (GLdouble)nHeight, 0.1, 100.0);
		*/
		}
		break;

		case WM_CLOSE:
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Sys_Quit ();
			}

		        break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);
		
			ClearAllStates();// fix the leftover Key_Alt from any Alt-Tab or the like that switched us away
			VID_SetDeviceGammaRamp ((unsigned short *)ramps);//v2.012
			//R00k re-init the mouse after alt-tabbing... (v1.9)
			if ((!mouseactive)&&(fActive))//RESETMOUSE
			{	
				IN_Shutdown ();
				IN_StartupMouse();
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
			break;

		case WM_DESTROY:
			if (dibwindow)
			{
				DestroyWindow (dibwindow);
			}

			PostQuitMessage (0);

			break;

		case MM_MCINOTIFY:
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;


		case WM_ERASEBKGND:
			lRet = 1;
			break;

		default:
		// pass all unhandled messages to DefWindowProc
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
			break;
	}

	// return 1 if handled message, 0 if not
	return lRet;
}

/*
=================
VID_NumModes
=================
*/
int VID_NumModes (void)
{
	return nummodes;
}
	
/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr (int modenum)
{
	if (modenum == 0)
		modenum = vid_default;//R00k

	if ((modenum > 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}

/*
=================
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription (int mode)
{
	char		*pinfo;
	vmode_t		*pv;
	static	char	temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode)
	{
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;		
	}
	else
	{
		sprintf (temp, "Desktop resolution (%dx%d)",
				 modelist[MODE_FULLSCREEN_DEFAULT].width,
				 modelist[MODE_FULLSCREEN_DEFAULT].height);
		pinfo = temp;
	}

	return pinfo;
}

// KJB: Added this to return the mode driver name in description for console

char *VID_GetExtModeDescription (int mode)
{
	static char	pinfo[40];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);

	if (modelist[mode].type == MS_FULLDIB)
	{
		if (!leavecurrentmode)
		{
			sprintf(pinfo,"%s fullscreen", pv->modedesc);
		}
		else
		{
			sprintf (pinfo, "Desktop resolution (%ix%ix%i)", //johnfitz -- added bpp
					 modelist[MODE_FULLSCREEN_DEFAULT].width,
					 modelist[MODE_FULLSCREEN_DEFAULT].height,
					 modelist[MODE_FULLSCREEN_DEFAULT].bpp); //johnfitz -- added bpp
		}
	}
	else
	{
		if (modestate == MS_WINDOWED)
			sprintf(pinfo, "%s windowed", pv->modedesc);
		else
			sprintf(pinfo, "windowed");
	}

	return pinfo;
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/*
=================
VID_NumModes_f
=================
*/
void VID_NumModes_f (void)
{
	if (nummodes == 1)
		Con_Printf ("%d video mode is available\n", nummodes);
	else
		Con_Printf ("%d video modes are available\n", nummodes);
}

/*
=================
VID_DescribeMode_f
=================
*/
void VID_DescribeMode_f (void)
{
	int	t, modenum;
	
	modenum = Q_atoi (Cmd_Argv(1));

	if (modenum == 0)
		modenum = vid_default;

	t = leavecurrentmode;
	leavecurrentmode = 0;

	Con_Printf ("%s\n", VID_GetExtModeDescription (modenum));

	leavecurrentmode = t;
}

/*
=================
VID_DescribeModes_f -- johnfitz -- changed formatting, and added refresh rates after each mode.
=================
*/
void VID_DescribeModes_f (void)
{
	int			i, lnummodes, t;
//	char		*pinfo;
	vmode_t		*pv;
	int			lastwidth=0, lastheight=0, lastbpp=0, count=0;

	lnummodes = VID_NumModes ();

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i=1 ; i<lnummodes ; i++)
	{
		pv = VID_GetModePtr (i);
		if (lastwidth == pv->width && lastheight == pv->height && lastbpp == pv->bpp)
		{
			Con_SafePrintf (",%i", pv->refreshrate);
		}
		else
		{
			if (count>0)
				Con_SafePrintf ("\n");
			Con_SafePrintf ("   %4i x %4i x %i : %i", pv->width, pv->height, pv->bpp, pv->refreshrate);
			lastwidth = pv->width;
			lastheight = pv->height;
			lastbpp = pv->bpp;
			count++;
		}
	}
	Con_Printf ("\n%i modes\n", count);

	leavecurrentmode = t;
}

void VID_InitDIB (HINSTANCE hInstance)
{
	int			i;
	WNDCLASS	wc;
	HDC			hdc;

	memset(&wc, 0, sizeof(wc));//R00k just incase..

	/* Register the frame class */
	wc.style		 = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = (WNDPROC)MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = 0;
	wc.hCursor       = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = "Qrack";
	
	if (!RegisterClass(&wc))
		Sys_Error ("Couldn't register window class");

	modelist[0].type = MS_WINDOWED;
	
	if ((i = COM_CheckParm("-width")) && i + 1 < com_argc)
	{
		modelist[0].width = Q_atoi(com_argv[COM_CheckParm("-width")+1]);

		if (modelist[0].width < 320)
		{
			modelist[0].width = 320;
		}
	}
	else
	{
		modelist[0].width =	GetSystemMetrics (SM_CXSCREEN);//use current desktop resolution
	}

	if ((i = COM_CheckParm("-height")) && i + 1 < com_argc)
	{
		modelist[0].height= Q_atoi(com_argv[COM_CheckParm("-height")+1]);

		if (modelist[0].height < 200)
		{
			modelist[0].height = 200;
		}
	}
	else
	{
		modelist[0].height = GetSystemMetrics (SM_CYSCREEN);//use current desktop resolution
	}

	if ((i = COM_CheckParm("-refreshrate")) && i + 1 < com_argc)
	{
		Cvar_SetValue ("vid_refreshrate",Q_atoi(com_argv[i+1]));
		modelist[0].refreshrate = vid_refreshrate.value;
	}
	else
	{
		hdc = GetDC(NULL);
		modelist[0].refreshrate = GetDeviceCaps(hdc,VREFRESH);
		Cvar_SetValue ("vid_refreshrate",modelist[0].refreshrate);
		ReleaseDC(NULL, hdc);
	}

	hdc = GetDC(NULL);
	modelist[0].bpp = GetDeviceCaps(hdc, BITSPIXEL);
	ReleaseDC(NULL, hdc);

	sprintf (modelist[0].modedesc, "%dx%dx%d %dHz", modelist[0].width, modelist[0].height,modelist[0].bpp,modelist[0].refreshrate);	//R00k

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
//	modelist[0].bpp = 0;

	nummodes = 1;
}

/*
=================
VID_InitFullDIB
=================
*/
void VID_InitFullDIB (HINSTANCE hInstance)
{
	DEVMODE	devmode;
	int		i, modenum, originalnummodes, existingmode, numlowresmodes;
	int		j, bpp, done;
	BOOL	stat;

// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do
	{
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if ((devmode.dmBitsPerPel >= 15) &&	(devmode.dmPelsWidth <= MAXWIDTH) && (devmode.dmPelsHeight <= MAXHEIGHT) &&	(nummodes < MAX_MODE_LIST))
		{
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY; //johnfitz -- refreshrate;

			if (ChangeDisplaySettings(&devmode, CDS_TEST | CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				modelist[nummodes].refreshrate = devmode.dmDisplayFrequency; //johnfitz -- refreshrate				

				sprintf (modelist[nummodes].modedesc, "%dx%dx%d %dHz", //johnfitz -- refreshrate
						 devmode.dmPelsWidth,
						 devmode.dmPelsHeight,
						 devmode.dmBitsPerPel,
						 devmode.dmDisplayFrequency); //johnfitz -- refreshrate

			// if the width is more than twice the height, reduce it by half because this
			// is probably a dual-screen monitor
				if (!COM_CheckParm("-noadjustaspect"))
				{
					if (modelist[nummodes].width > (modelist[nummodes].height << 1))
					{
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;
						sprintf (modelist[nummodes].modedesc, "%dx%dx%d %dHz", //johnfitz -- refreshrate
								 modelist[nummodes].width,
								 modelist[nummodes].height,
								 modelist[nummodes].bpp,
								 modelist[nummodes].refreshrate); //johnfitz -- refreshrate	
					}
				}

				for (i=originalnummodes, existingmode=0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width) &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp) &&
						(modelist[nummodes].refreshrate == modelist[i].refreshrate)) //johnfitz -- refreshrate
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
					nummodes++;
			}
		}

		modenum++;
	} while (stat);

// see if there are any low-res modes that aren't being reported
	numlowresmodes = sizeof(lowresmodes) / sizeof(lowresmodes[0]);
	bpp = 16;
	done = 0;

	do
	{
		for (j=0 ; j<numlowresmodes && nummodes<MAX_MODE_LIST ; j++)
		{
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = lowresmodes[j].width;
			devmode.dmPelsHeight = lowresmodes[j].height;
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY; //johnfitz -- refreshrate;

			if (ChangeDisplaySettings(&devmode, CDS_TEST | CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				modelist[nummodes].refreshrate = devmode.dmDisplayFrequency; //johnfitz -- refreshrate

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if (		(modelist[nummodes].width		== modelist[i].width) 
							&&	(modelist[nummodes].height		== modelist[i].height) 
							&&	(modelist[nummodes].bpp			== modelist[i].bpp) 
							&&	(modelist[nummodes].refreshrate == modelist[i].refreshrate) //johnfitz -- refreshrate
						)
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
					nummodes++;
			}
		}
		switch (bpp)
		{
			case 16:
				bpp = 32;
				break;

			case 32:
				bpp = 24;
				break;

			case 24:
				done = 1;
				break;
		}
	} while (!done);

	if (nummodes == originalnummodes)
		Con_SafePrintf ("No fullscreen DIB modes found\n");
}

/*
================
VID_SyncCvars -- johnfitz -- set vid cvars to match current video mode
================
*/
void VID_SyncCvars (void)
{
	Cvar_Set ("vid_width", va("%i", modelist[vid_default].width));
	Cvar_Set ("vid_height", va("%i", modelist[vid_default].height));
	Cvar_Set ("vid_bpp", va("%i", modelist[vid_default].bpp));
	Cvar_Set ("vid_refreshrate", va("%i", modelist[vid_default].refreshrate));
	Cvar_Set ("vid_fullscreen", (windowed) ? "0" : "1");
}

void SwitchMonitors (void)
{
	int i;

	DISPLAY_DEVICEA gDisplayDevice;	
	DEVMODEA gdeviceMode;		
	DWORD sx = -1;
	char initial_primary[32], initial_secondary[32];

	ZeroMemory(&gDisplayDevice, sizeof(DISPLAY_DEVICE));
	gDisplayDevice.cb = sizeof(gDisplayDevice);//Required

	for (i = 0; EnumDisplayDevices(NULL, i, &gDisplayDevice, 0); i++)
	{
		if (gDisplayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER || !(gDisplayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
			continue;// Non-monitor

		gdeviceMode.dmSize	= sizeof(DEVMODE);
		gdeviceMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_POSITION | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS;
		
		EnumDisplaySettings(gDisplayDevice.DeviceName, ENUM_CURRENT_SETTINGS, &gdeviceMode);

		if ((gdeviceMode.dmPosition.x == 0) && (gdeviceMode.dmPosition.y == 0)) //Todo fix this!
		{
			strcpy (initial_primary, gDisplayDevice.DeviceName);  
		}
		else
		{
			sx = gdeviceMode.dmPelsWidth;
			strcpy (initial_secondary, gDisplayDevice.DeviceName);  
		}
	}

	//Change primary monitor's settings
	EnumDisplaySettings(initial_primary, ENUM_CURRENT_SETTINGS, &gdeviceMode);
	gdeviceMode.dmPosition.x = sx;
	gdeviceMode.dmPosition.y = 0;
	ChangeDisplaySettingsEx(initial_primary, &gdeviceMode, NULL, (CDS_UPDATEREGISTRY | CDS_NORESET), NULL);

	//Change secondary monitor's settings
	EnumDisplaySettings(initial_secondary, ENUM_CURRENT_SETTINGS, &gdeviceMode);
	gdeviceMode.dmPosition.x = 0;
	gdeviceMode.dmPosition.y = 0;
	ChangeDisplaySettingsEx(initial_secondary, &gdeviceMode, NULL, (CDS_SET_PRIMARY | CDS_UPDATEREGISTRY | CDS_NORESET), NULL);

	//Update settings for both displays
	ChangeDisplaySettingsEx (NULL, NULL, NULL, 0, NULL);

	//Clean up just in case...
	ZeroMemory(&gDisplayDevice, sizeof(DISPLAY_DEVICE));
}

qboolean CheckOpengl32(void) 
{ 
     FILE *f; 
     char ogname[1024]; 
 
     Q_snprintfz (ogname, sizeof(ogname), "%s\\opengl32.dll", com_basedir); 
     if ((f = fopen(ogname, "rb"))) 
	 {
          fclose (f);
          return true;
     }
     
     return false;
}

/*
===================
VID_Init
===================
*/
void VID_Init(unsigned char *palette)
{
	int		i, existingmode;
	int		basenummodes, width, height, bpp, refreshrate, findbpp, done;
	HDC		hdc;
	HGLRC	baseRC;
	DEVMODE	devmode;
	extern void VID_Menu_CalcConTextSize (float cw, float w);

	if (CheckOpengl32() == true)
		Sys_Error ("Please delete the opengl32.dll from the Quake folder.");

	if (COM_CheckParm("-monitor2"))
	{		
		SwitchMonitors();
	}

	memset (&devmode, 0, sizeof(devmode));

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&con_textsize);
	Cvar_RegisterVariable (&_vid_default_mode);
	Cvar_RegisterVariable (&_vid_default_mode_win);
	Cvar_RegisterVariable (&vid_config_x);
	Cvar_RegisterVariable (&vid_config_y);
	Cvar_RegisterVariable (&vid_stretch_by_2);
	Cvar_RegisterVariable (&_windowed_mouse);
	Cvar_RegisterVariable (&vid_hwgammacontrol);
	Cvar_RegisterVariable (&vid_refreshrate);
	Cvar_RegisterVariable (&vid_conheight);
	Cvar_RegisterVariable (&vid_conwidth);

	Cvar_RegisterVariable (&vid_fullscreen); //johnfitz
	Cvar_RegisterVariable (&vid_width); //johnfitz
	Cvar_RegisterVariable (&vid_height); //johnfitz
	Cvar_RegisterVariable (&vid_bpp); //johnfitz
	Cvar_RegisterVariable (&vid_borderless);
	Cvar_RegisterVariable (&vid_force_aspect_ratio);

	Cmd_AddCommand ("vid_unlock", VID_Unlock); //johnfitz
	Cmd_AddCommand ("vid_restart", VID_Restart); //johnfitz
	Cmd_AddCommand ("vid_test", VID_Test); //johnfitz
	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON2));

	VID_InitDIB (global_hInstance);
	basenummodes = nummodes;

	VID_InitFullDIB (global_hInstance);

	//R00k: used for picking correct consize, assuming the desktop is the native resolution
	vid.nativewidth = GetSystemMetrics (SM_CXSCREEN);
	vid.nativeheight = GetSystemMetrics (SM_CYSCREEN);

	vid.nativeaspect = ((float)vid.nativeheight / (float)vid.nativewidth);

	if (COM_CheckParm("-window"))
	{
		hdc = GetDC (NULL);

		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
			Sys_Error ("Can't run in non-RGB mode");

		ReleaseDC (NULL, hdc);

		windowed = true;
		width = 640;
		height = 480;
		vid_default = MODE_WINDOWED;
	}
	else
	{
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if ((i = COM_CheckParm("-mode")) && i + 1 < com_argc)
			vid_default = Q_atoi(com_argv[i+1]);
		else
		{			
			if (COM_CheckParm("-current"))
			{				
				modelist[MODE_FULLSCREEN_DEFAULT].width = vid.nativewidth;
				modelist[MODE_FULLSCREEN_DEFAULT].height = vid.nativeheight;
				hdc = GetDC(NULL);
				modelist[MODE_FULLSCREEN_DEFAULT].refreshrate = GetDeviceCaps(hdc,VREFRESH);			
				modelist[MODE_FULLSCREEN_DEFAULT].bpp = GetDeviceCaps(hdc, BITSPIXEL);					
				ReleaseDC(NULL, hdc);	
				vid_default = MODE_FULLSCREEN_DEFAULT;
				leavecurrentmode = 1;
			}
			else
			{
				if ((i = COM_CheckParm("-width")) && i + 1 < com_argc)
				{
					width = Q_atoi(com_argv[i+1]);
				}
				else
				{
					width = vid.nativewidth;
				}

				if ((i = COM_CheckParm("-bpp")) && i + 1 < com_argc)
				{
					bpp = Q_atoi(com_argv[i+1]);
					findbpp = 0;
				}
				else
				{
					hdc = GetDC(NULL);
					bpp = GetDeviceCaps(hdc, BITSPIXEL);					
					ReleaseDC(NULL, hdc);	
					findbpp = 1;
				}

				if ((i = COM_CheckParm("-height")) && i + 1 < com_argc)
				{
					height = Q_atoi(com_argv[i+1]);
				}
				else
				{
					height = vid.nativeheight;
				}

				if ((i = COM_CheckParm("-refreshrate")) && i + 1 < com_argc)
				{
					Cvar_SetValue ("vid_refreshrate",Q_atoi(com_argv[i+1]));
					refreshrate = vid_refreshrate.value;
				}
				else
				{
					hdc = GetDC(NULL);
					refreshrate = GetDeviceCaps(hdc,VREFRESH);//R00k grab the current refreshrate, as this is at least, supported by lower resolutions 
					Cvar_SetValue ("vid_refreshrate",refreshrate);
					ReleaseDC(NULL, hdc);	
				}

				// if they want to force it, add the specified mode to the list
				if (COM_CheckParm("-force") && (nummodes < MAX_MODE_LIST))
				{
					modelist[nummodes].type = MS_FULLDIB;
					modelist[nummodes].width = width;
					modelist[nummodes].height = height;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].halfscreen = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp = bpp;
					sprintf (modelist[nummodes].modedesc, "%dx%dx%d %dHz", //johnfitz -- refreshrate
							 devmode.dmPelsWidth,
							 devmode.dmPelsHeight,
							 devmode.dmBitsPerPel,
							 devmode.dmDisplayFrequency); //johnfitz -- refreshrate

					for (i=nummodes, existingmode = 0 ; i<nummodes ; i++)
					{
						if ((modelist[nummodes].width == modelist[i].width)   &&
							(modelist[nummodes].height == modelist[i].height) &&
							(modelist[nummodes].bpp == modelist[i].bpp) &&
							(modelist[nummodes].refreshrate == modelist[i].refreshrate)) //johnfitz -- refreshrate
						{
							existingmode = 1;
							break;
						}
					}

					if (!existingmode)
						nummodes++;
				}

				done = 0;

				do {
					if ((i = COM_CheckParm("-height")) && i + 1 < com_argc)
					{
						height = Q_atoi(com_argv[i+1]);
						if (height < 200)
						{
							height = 200;
						}

						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if (	(modelist[i].width == width) 
								&&	(modelist[i].height == height) 
								&&	(modelist[i].bpp == bpp) 
								&&	(modelist[i].refreshrate == refreshrate))
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}
					else
					{
						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) && (modelist[i].bpp == bpp) && (modelist[i].refreshrate == refreshrate))
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}

					if (!done)
					{
						if (findbpp)
						{
							switch (bpp)
							{
							case 15:
								bpp = 16;
								break;
							case 16:
								bpp = 32;
								break;
							case 32:
								bpp = 24;
								break;
							case 24:
								done = 1;
								break;
							}
						}
						else
							done = 1;
					}
				} while (!done);

				if (!vid_default)
				{
					Sys_Error ("Specified video mode not available");
				}
			}
		}
	}

	vid_initialized = true;
	
	if ((i = COM_CheckParm("-conwidth")) && i + 1 < com_argc)
		vid.conwidth = Q_atoi(com_argv[i+1]);
	else
		vid.conwidth = (width * 0.5);//R00k:was 640

	vid.conwidth &= 0xfff8;	// make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	vid.aspect = ((float)height / (float)width);

	//pick a conheight that matches the correct aspect
	vid.conheight = (vid.conwidth * vid.aspect);

	if ((i = COM_CheckParm("-conheight")) && i + 1 < com_argc)
		vid.conheight = Q_atoi(com_argv[i+1]);
	
	if (vid.conheight < 200)
		vid.conheight = 200;

	Cvar_SetValue("vid_conwidth", vid.conwidth);
	Cvar_SetValue("vid_conheight", vid.conheight);
	VID_Menu_CalcConTextSize(vid.conwidth,width);//R00k
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
//	vid.colormap = host_colormap;
//	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	Check_Gamma (palette);
	VID_SetPalette (palette);

	VID_SetMode (vid_default, palette);

	maindc = GetDC (mainwindow);

	if (!bSetupPixelFormat(maindc))
		Sys_Error ("bSetupPixelFormat failed");

	InitHWGamma ();

	if (!(baseRC = wglCreateContext(maindc)))
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you are in 65535 color mode, and try running -window.");

	if (!wglMakeCurrent(maindc, baseRC))
	{
		char szBuf[80];
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL );
		sprintf(szBuf, "VID_Init: wglMakeCurrent failed with error %d: %s", dw, (char *)lpMsgBuf);
 		Sys_Error (szBuf);
	}

//		Sys_Error ("wglMakeCurrent failed");
	
	GL_Init ();
	GL_Init_Win();
	
	vid_menucmdfn = VID_Menu_f; //johnfitz
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy (badmode.modedesc, "Bad mode");
	vid_canalttab = true;

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;

	VID_Menu_Init(); //johnfitz

	//johnfitz -- command line vid settings should override config file settings.
	//so we have to lock the vid mode from now until after all config files are read.
	if (COM_CheckParm("-width") || COM_CheckParm("-height") || COM_CheckParm("-bpp") || COM_CheckParm("-window"))
	{
		vid_locked = true;
	}
	//johnfitz
}


//==========================================================================
//
//  NEW VIDEO MENU -- johnfitz
//
//==========================================================================

extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, mpic_t *pic);
extern void M_DrawPic (int x, int y, mpic_t *pic);
extern void M_DrawCheckbox (int x, int y, int on);

extern cvar_t gl_texCompression;
extern cvar_t con_textsize;

#define gamma		"gl_gamma"
#define contrast	"gl_contrast"

extern qboolean	m_entersound;

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
#ifdef GLQUAKE
		m_videooptions, 
		m_particles,
#endif
		m_keys, 
		m_maps, 
		m_demos, 
		m_help,
		m_quit, 
		m_lanconfig, 
		m_gameoptions, 
		m_search, 
		m_servers, 
		m_slist, 
		m_sedit,
		m_stest,
} m_state;

#define VIDEO_OPTIONS_ITEMS 15

int		video_cursor_table[] = {32, 40, 48, 56, 64, 72, 80, 96, 104, 112, 120, 136, 144, 160, 168};
int		video_options_cursor = 0;

typedef struct {int width,height;} vid_menu_mode;
int vid_menu_rwidth;
int vid_menu_rheight;

vid_menu_mode vid_menu_modes[MAX_MODE_LIST];
int vid_menu_nummodes=0;

int vid_menu_rates[20];
int vid_menu_numrates=0;

char *popular_filters[] = 
{
	"GL_NEAREST",
	"GL_LINEAR_MIPMAP_NEAREST",
	"GL_LINEAR_MIPMAP_LINEAR"
};
extern	cvar_t		gl_texturemode;

int con_size;
/*
================
VID_Menu_Init
================
*/
void VID_Menu_Init (void)
{
	int i,j,h,w;

	for (i=1;i<nummodes;i++) //start i at mode 1 because 0 is windowed mode
	{
		w = modelist[i].width;
		h = modelist[i].height;

		for (j=0;j<vid_menu_nummodes;j++)
		{
			if (vid_menu_modes[j].width == w &&
				vid_menu_modes[j].height == h)
				break;
		}

		if (j==vid_menu_nummodes)
		{
			vid_menu_modes[j].width = w;
			vid_menu_modes[j].height = h;
			vid_menu_nummodes++;
		}
	}
	con_size = floor(vid_width.value / vid_conwidth.value);
	con_size = bound (1,con_size,6);
}

/*
================
VID_Menu_RebuildRateList

regenerates rate list based on current vid_width, vid_height and vid_bpp
================
*/
void VID_Menu_RebuildRateList (void)
{
	int i,j,r;

	vid_menu_numrates=0;

	for (i=1;i<nummodes;i++) //start i at mode 1 because 0 is windowed mode
	{
		//rate list is limited to rates available with current width/height/bpp
		if (modelist[i].width != vid_width.value ||
			modelist[i].height != vid_height.value ||
			modelist[i].bpp != vid_bpp.value)
			continue;

		r = modelist[i].refreshrate;

		for (j=0;j<vid_menu_numrates;j++)
		{
			if (vid_menu_rates[j] == r)
				break;
		}

		if (j==vid_menu_numrates)
		{
			vid_menu_rates[j] = r;
			vid_menu_numrates++;
		}
	}

	//if vid_refreshrate is not in the new list, change vid_refreshrate
	for (i=0;i<vid_menu_numrates;i++)
		if (vid_menu_rates[i] == (int)(vid_refreshrate.value))
			break;

	if (i==vid_menu_numrates)
		Cvar_Set ("vid_refreshrate",va("%i",vid_menu_rates[0]));
}

/*
================
VID_Menu_CalcAspectRatio -- FitzQuake

calculates aspect ratio for current vid_width/vid_height
================
*/
void VID_Menu_CalcAspectRatio (void)
{
	int w,h,f;
	w = vid_width.value;
	h = vid_height.value;
	f = 2;
	while (f < w && f < h)
	{
		if ((w/f)*f == w && (h/f)*f == h)
		{
			w/=f;
			h/=f;
			f=2;
		}
		else
			f++;
	}
	vid_menu_rwidth = w;
	vid_menu_rheight = h;
}

/*
========================
VID_Menu_CalcConTextSize --	R00k

Sets a default value for the console text size
when changing console resolutions, to avoid
instances of micro unreadable text!
========================
*/
void VID_Menu_CalcConTextSize (float cw, float w)
{
	float s;

	s = lrintf((cw / w) * (w / 80));// 640/80 = 8, 8px is the default glQuake size.

	con_textsize.value = bound(2, s, 16);

	Cvar_SetValue ("con_textsize", con_textsize.value);
}

/*
================
VID_Menu_ChooseNextMode

chooses next resolution in order, then updates vid_width and
vid_height cvars, then updates bpp and refreshrate lists
================
*/
void VID_Menu_ChooseNextMode (int dir)
{
	int i;

	for (i=0;i<vid_menu_nummodes;i++)
	{
		if (vid_menu_modes[i].width == vid_width.value && vid_menu_modes[i].height == vid_height.value)
			break;
	}

	if (i==vid_menu_nummodes) //can't find it in list, so it must be a custom windowed res
	{
		i = 0;
	}
	else
	{
		i+=dir;
		if (i>=vid_menu_nummodes)
			i = 0;
		else if (i<0)
			i = vid_menu_nummodes-1;
	}

	Cvar_SetValue ("vid_width",(float)vid_menu_modes[i].width);
	Cvar_SetValue ("vid_height",(float)vid_menu_modes[i].height);
	VID_Menu_RebuildRateList ();
//	VID_Menu_CalcAspectRatio();
	if (vid_conwidth.value > vid_width.value)
		Cvar_SetValue ("vid_conwidth", vid_width.value);	

	VID_Menu_CalcConTextSize(vid_conwidth.value, vid_width.value);
	con_size = 1;
}

/*
================
VID_Menu_ChooseNextRate

chooses next refresh rate in order, then updates vid_refreshrate cvar
================
*/
void VID_Menu_ChooseNextRate (int dir)
{
	int i;

	for (i=0;i<vid_menu_numrates;i++)
	{
		if (vid_menu_rates[i] == vid_refreshrate.value)
			break;
	}

	if (i==vid_menu_numrates) //can't find it in list
	{
		i = 0;
	}
	else
	{
		i+=dir;
		if (i>=vid_menu_numrates)
			i = 0;
		else if (i<0)
			i = vid_menu_numrates-1;
	}

	Cvar_Set ("vid_refreshrate",va("%i",vid_menu_rates[i]));
}

void VID_MenuAdjustSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav");

	switch (video_options_cursor)
	{
		case 4:
			cl_maxfps.value += dir * 1;
			cl_maxfps.value = bound(30, cl_maxfps.value, 999);
			Cvar_SetValue ("cl_maxfps", cl_maxfps.value);
			break;

		case 6:
			con_textsize.value += dir * 1;
			con_textsize.value = bound(2, con_textsize.value, 16);
			Cvar_SetValue ("con_textsize", con_textsize.value);

			break;
		case 7:
			gl_picmip.value -= dir * 1;
			gl_picmip.value = bound(0, gl_picmip.value, 4);
			Cvar_SetValue ("gl_picmip", gl_picmip.value);
			break;

		case 10:
			gl_anisotropic.value += dir * 1;
			gl_anisotropic.value = bound(0, gl_anisotropic.value, 8);
			Cvar_SetValue ("gl_anisotropic", gl_anisotropic.value);
			break;

		case 11:
			v_gamma.value -= dir * 0.1;
			if (v_gamma.value < 0.5)
				v_gamma.value = 0.5;
			if (v_gamma.value > 1)
				v_gamma.value = 1;
			Cvar_SetValue (gamma, v_gamma.value);
			break;

		case 12:	
			v_contrast.value += dir * 0.1;
			if (v_contrast.value < 1)
				v_contrast.value = 1;
			if (v_contrast.value > 2)
				v_contrast.value = 2;
			Cvar_SetValue (contrast, v_contrast.value);
			break;
	}
}
/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	int i;
	float set;

	switch (key)
	{
	case K_ESCAPE:
		VID_SyncCvars (); 
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor--;
		if (video_options_cursor < 0)
			video_options_cursor = VIDEO_OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor++;
		if (video_options_cursor >= VIDEO_OPTIONS_ITEMS)
			video_options_cursor = 0;
		break;

	case K_LEFTARROW:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case 0:
			VID_Menu_ChooseNextMode (-1);
			break;
		case 1:
			Cbuf_AddText ("toggle vid_vsync\n");
			break;
		case 2:
			VID_Menu_ChooseNextRate (-1);
			break;
		case 3:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;

		case 5:
			con_size += 1;
			con_size = bound (1,con_size,6);
			if ((vid_width.value / con_size) < 320)
				con_size -= 1;
			set = (vid_width.value / con_size);			
			Cvar_SetValue ("vid_conwidth", set);
			VID_Menu_CalcConTextSize(vid_conwidth.value, vid_width.value);
			break;

		default:
			VID_MenuAdjustSliders (-1);
			break;
		}
		break;

	case K_RIGHTARROW:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case 0:
			VID_Menu_ChooseNextMode (1);
			break;
		case 1:
			Cbuf_AddText ("toggle vid_vsync\n");
			break;
		case 2:
			VID_Menu_ChooseNextRate (1);
			break;
		case 3:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;

		case 5:
			con_size -= 1;
			con_size = bound (1,con_size,6);				
			if ((vid_width.value / con_size) < 320)
				con_size += 1;
			set = (vid_width.value / con_size);
			Cvar_SetValue ("vid_conwidth", set);
			VID_Menu_CalcConTextSize(vid_conwidth.value, vid_width.value);
			break;

		default:
			VID_MenuAdjustSliders (1);
			break;
		}
		break;

	case K_ENTER:
		m_entersound = true;
		switch (video_options_cursor)
		{
		case 0:
			VID_Menu_ChooseNextMode (1);
			break;
		case 1:
			Cbuf_AddText ("toggle vid_vsync\n");
			break;
		case 2:
			VID_Menu_ChooseNextRate (1);
			break;
		case 3:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;

		case 8:
			for (i=0 ; i<3 ; i++)
				if (!Q_strcasecmp(popular_filters[i], gl_texturemode.string))
					break;
			if (i >= 2)
				i = -1;
			Cvar_Set ("gl_texturemode", popular_filters[i+1]);
			break;		
		case 9:
			Cbuf_AddText ("toggle gl_texCompression\n");
			break;
		case 13:
			Cbuf_AddText ("vid_test\n");
			break;
		case 14:
			Cbuf_AddText ("vid_restart\n");
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	int i = 0;
	mpic_t *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	// options
	M_Print (16, video_cursor_table[i], "            Video Mode");
	M_Print (216, video_cursor_table[i], va("%ix%i", (int)vid_width.value, (int)vid_height.value));
	i++;

	M_Print (16, video_cursor_table[i], "                V-Sync");
	M_DrawCheckbox (216, video_cursor_table[i], (int)vid_vsync.value);
	i++;

	M_Print (16, video_cursor_table[i], "          Refresh Rate");
	M_Print (216, video_cursor_table[i], va("%i Hz", (int)vid_refreshrate.value));
	i++;

	M_Print (16, video_cursor_table[i], "            Fullscreen");
	M_DrawCheckbox (216, video_cursor_table[i], (int)vid_fullscreen.value);
	i++;

	M_Print (16, video_cursor_table[i], "               Max FPS");
	M_Print (216, video_cursor_table[i], va("%i",(int)cl_maxfps.value));
	i++;

	M_Print (16, video_cursor_table[i], "          Console Size");
	M_Print (216, video_cursor_table[i], va("%ix%i", (int)vid_conwidth.value, (int)vid_conheight.value));
	i++;

	M_Print (16, video_cursor_table[i], "     Console Text Size");
	M_Print (216, video_cursor_table[i], va("%i",(int)con_textsize.value));
	i++;

	M_Print (16, video_cursor_table[i], "        Texture MipMap");
	M_Print (216, video_cursor_table[i], va("%i",(int)gl_picmip.value));
	i++;

	M_Print (16, video_cursor_table[i], "        Texture Filter");
	M_Print (216, video_cursor_table[i], !Q_strcasecmp(gl_texturemode.string, "GL_LINEAR_MIPMAP_NEAREST") ? "bilinear" :
			!Q_strcasecmp(gl_texturemode.string, "GL_LINEAR_MIPMAP_LINEAR") ? "trilinear" :
			!Q_strcasecmp(gl_texturemode.string, "GL_NEAREST") ? "off" : gl_texturemode.string);
	i++;

	M_Print (16, video_cursor_table[i], "   Texture Compression");
	M_DrawCheckbox (216, video_cursor_table[i], (int)gl_texCompression.value);
	i++;

	M_Print (16, video_cursor_table[i], "    Anisotropic Filter");
	M_Print (216, video_cursor_table[i], va("%i",(int)gl_anisotropic.value));	
	i++;
	
	M_Print (16, video_cursor_table[i], "                 Gamma");
	M_Print (216, video_cursor_table[i], va("%1.1f",(float)v_gamma.value));
	i++;

	M_Print (16, video_cursor_table[i], "              Contrast");
	M_Print (216, video_cursor_table[i], va("%1.1f",(float)v_contrast.value));
	i++;

	//---------------------------------------------------------------------------------//	
	M_Print (16, video_cursor_table[i], "          Test Changes");
	i++;

	M_Print (16, video_cursor_table[i], "         Apply Changes");

	// cursor
	M_DrawCharacter (200, video_cursor_table[video_options_cursor], 12+((int)(realtime*4)&1));
}

/*
================
VID_Menu_f
================
*/
void VID_Menu_f (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;

	VID_SyncCvars ();
	VID_Menu_RebuildRateList ();
	//VID_Menu_CalcAspectRatio ();
}

