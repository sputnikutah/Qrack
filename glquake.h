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
// disable data conversion warnings

#pragma warning (disable : 4244)     // MIPS
#pragma warning (disable : 4136)     // X86
#pragma warning (disable : 4051)     // ALPHA
  
#ifdef _WIN32
#include <windows.h>
#endif

#ifndef USEFAKEGL
		#include <gl/gl.h>
		#include <GL/glu.h>
		#include <gl_ext.h>
		#pragma comment (lib, "opengl32.lib")
		#pragma comment (lib, "glu32.lib")
#else
		#include "fakegl.h"
#endif

typedef int qboolean;

//MHGLQFIX: polygons are not native to GPUs so use triangle fans instead
#undef GL_POLYGON
#define GL_POLYGON GL_TRIANGLE_FAN

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

extern qboolean	WinNT, Win2K, WinXP, Win2K3, WinVISTA, Win7;

extern HMONITOR VID_GetCurrentMonitor();
extern MONITORINFOEX VID_GetCurrentMonitorInfo(HMONITOR monitor);
extern HMONITOR prevMonitor;
extern MONITORINFOEX prevMonInfo;
extern HMONITOR	hCurrMon;
extern MONITORINFOEX currMonInfo;
extern qboolean	gl_vbo;
extern	int	texture_extension_number;

extern	float	gldepthmin, gldepthmax;

// added by joe
#define	TEX_COMPLAIN		1
#define TEX_MIPMAP			2
#define TEX_ALPHA			4
#define TEX_LUMA			8
#define TEX_FULLBRIGHT		16
#define	TEX_BLEND			32
#define	TEX_WORLD			64//R00k

//#define	MAX_GLTEXTURES 1024
#define	MAX_GLTEXTURES 4096

void GL_SelectTexture (GLenum target);
void GL_DisableMultitexture (void);
void GL_EnableMultitexture (void);
void GL_EnableTMU (GLenum target);
void GL_DisableTMU (GLenum target);

void GL_Upload32 (unsigned *data, int width, int height, int mode);
void GL_Upload8 (byte *data, int width, int height, int mode);
int GL_LoadTexture (char *identifier, int width, int height, byte *data, int mode, int bytesperpixel);
byte *GL_LoadImagePixels (char* filename, int matchwidth, int matchheight, int mode);
int GL_LoadTextureImage (char *filename, char *identifier, int matchwidth, int matchheight, int mode);
mpic_t *GL_LoadPicImage (char *filename, char *id, int matchwidth, int matchheight, int mode);
int GL_LoadCharsetImage (char *filename, char *identifier);

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;

extern glvert_t	glv;

extern	int	glx, gly, glwidth, glheight;

// r_local.h -- private refresh defs

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle
#define	MAX_LBM_HEIGHT		480

#define SKYSHIFT		7
#define	SKYSIZE			(1 << SKYSHIFT)
#define SKYMASK			(SKYSIZE - 1)

#define BACKFACE_EPSILON	0.01

#define ISTRANSPARENT(ent)	(((ent)->transparency > 0 && (ent)->transparency < 1)||((ent)->alpha > 0 && (ent)->alpha < 1))
extern qboolean Model_isDead (int modelindex, int frame);
void R_TimeRefresh_f (void);
void R_ReadPointFile_f (void);
texture_t *R_TextureAnimation (texture_t *base);

//MHGLQFIX: this mirrors the layout of a D3DMATRIX struct which is more convenient to work with
typedef struct gl_matrix_s
{
	union
	{
		struct
		{
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};

		float m4x4[4][4];
		float m16[16];
	};
} gl_matrix_t;

extern	gl_matrix_t	r_world_matrix;

// frustum culling
#define FRUSTUM_OUTSIDE			0
#define FRUSTUM_INSIDE			1
#define FRUSTUM_INTERSECT		2
//-----------------------------------------------------
void QMB_InitParticles (void);
void QMB_ClearParticles (void);
void QMB_DrawParticles (void);
void QMB_Q3TorchFlame (vec3_t org, float size);
void QMB_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void QMB_RocketTrail (vec3_t start, vec3_t end, trail_type_t type);
void QMB_BlobExplosion (vec3_t org);
void QMB_ParticleExplosion (vec3_t org);
void QMB_LavaSplash (vec3_t org);
void QMB_TeleportSplash (vec3_t org);
void QMB_InfernoFlame (vec3_t org);
void QMB_StaticBubble (entity_t *ent);
void QMB_ColorMappedExplosion (vec3_t org, int colorStart, int colorLength);
void QMB_TorchFlame (vec3_t org);
void QMB_BigTorchFlame (vec3_t org);
void QMB_ShamblerCharge (vec3_t org);
void QMB_LightningBeam (vec3_t start, vec3_t end);
//void QMB_GenSparks (vec3_t org, byte col[3], float count, float size, float life);
void QMB_EntityParticles (entity_t *ent);

extern	qboolean	qmb_initialized;

void CheckParticles (void);

//====================================================


extern	entity_t	r_worldentity;
extern	qboolean	r_cache_thrash;		// compatability
extern	vec3_t		modelorg, r_entorigin;
extern	entity_t	*currententity;
extern	int		r_visframecount;	// ??? what difs?
extern	int		r_framecount;
extern	mplane_t	frustum[4];
extern	int		c_brush_polys, c_alias_polys, c_md3_polys;


// view origin
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

// screen size info
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	mleaf_t		*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack
extern	texture_t	*r_notexture_mip;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	int	currenttexture;
extern	int	particletexture;
extern	int	playertextures;
extern	int	skyboxtextures;
extern	int	underwatertexture, detailtexture;
extern  int chrometexture;
extern  int glasstexture;

extern	int	gl_max_size_default;

extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_viewmodelsize;
extern	cvar_t	r_speeds;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_shadows;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_lavaalpha;
extern	cvar_t	r_telealpha;
extern	cvar_t	r_turbalpha_distance;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_fastsky;
extern	cvar_t	r_fastturb;
extern	cvar_t	r_skybox;
extern	cvar_t	r_farclip;
extern	cvar_t	r_outline;
extern	cvar_t	r_outline_surf;
extern	cvar_t	r_celshading;

extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_poly;
extern	cvar_t	gl_smoothmodels;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_polyblend;
extern	cvar_t	gl_flashblend;
extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_loadlitfiles;
extern	cvar_t	gl_doubleeyes;
extern	cvar_t	gl_interdist;
extern  cvar_t  gl_waterfog;		
extern  cvar_t  gl_waterfog_density;
extern  cvar_t  gl_detail;
extern  cvar_t  gl_caustics;
extern	cvar_t	gl_fb_bmodels;
extern	cvar_t	gl_fb_models;
extern  cvar_t  gl_vertexlights;
extern  cvar_t  gl_loadq3models;
extern  cvar_t	gl_part_explosions;
extern  cvar_t	gl_part_trails;
extern  cvar_t	gl_part_sparks;
extern  cvar_t	gl_part_gunshots;
extern  cvar_t	gl_part_blood;
extern  cvar_t	gl_part_telesplash;
extern  cvar_t	gl_part_blobs;
extern  cvar_t	gl_part_lavasplash;
extern	cvar_t	gl_part_flames;
extern	cvar_t	gl_part_lightning;
extern	cvar_t	gl_part_muzzleflash;
extern	cvar_t	gl_part_flies;
extern	cvar_t	gl_particle_count;
extern	cvar_t	gl_bounceparticles;

extern	cvar_t	gl_externaltextures_world;
extern	cvar_t	gl_externaltextures_bmodels;
extern	cvar_t	gl_externaltextures_models;
extern  cvar_t  gl_shiny;
extern	cvar_t	gl_anisotropic;
//extern	cvar_t	gl_lightmode;//R00k

//extern	int	lightmode;

extern	int	gl_lightmap_format;
extern	int	gl_solid_format;
extern	int	gl_alpha_format;

extern	cvar_t	gl_max_size;
extern	cvar_t	gl_playermip;
extern	cvar_t	gl_picmip;
extern  cvar_t  r_waterripple;
/*
extern	qboolean	mirror;
extern	mplane_t	*mirror_plane;
*/
extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

void GL_Bind (int texnum);

//johnfitz -- polygon offset
#define OFFSET_BMODEL 1
#define OFFSET_NONE 0
#define OFFSET_DECAL -1
#define OFFSET_FOG -2
#define OFFSET_SHOWTRIS -3
void GL_PolygonOffset (int);
//johnfitz

typedef void (APIENTRY *lp1DMTexFUNC) (GLenum, GLfloat);
typedef void (APIENTRY *lpMTexFUNC)(GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *lpSelTexFUNC)(GLenum);

extern lpMTexFUNC	qglMultiTexCoord2f;
extern lp1DMTexFUNC qglMultiTexCoord1f;
extern lpSelTexFUNC qglActiveTexture;
extern lpSelTexFUNC glClientActiveTexture;

extern qboolean gl_mtexable;
extern int gl_textureunits;

void GL_DisableMultitexture (void);
void GL_EnableMultitexture (void);

// vid_common_gl.c
void Check_Gamma (unsigned char *pal);
void VID_SetPalette (unsigned char *palette);
void GL_Init (void);
extern qboolean	gl_add_ext;
//extern qboolean gl_combine;
extern qboolean gl_shader;

// gl_image.c
extern	int	image_width;
extern	int	image_height;
byte *Image_LoadTGA (FILE *fin, char *filename, int matchwidth, int matchheight);
int Image_WriteTGA (char *filename, byte *pixels, int width, int height);
byte *Image_LoadPNG (FILE *fin, char *filename, int matchwidth, int matchheight);
int Image_WritePNG (char *filename, int compression, byte *pixels, int width, int height);
byte *Image_LoadJPEG (FILE *fin, char *filename, int matchwidth, int matchheight);
int Image_WriteJPEG (char *filename, int compression, byte *pixels, int width, int height);

// gl_warp.c
void GL_SubdivideSurface (msurface_t *fa);
void EmitWaterPolys (msurface_t *fa);
void EmitSkyPolys (msurface_t *fa, qboolean mtex);
void CalcCausticTexCoords(float *v, float *s, float *t);
void EmitCausticsPolys (void);
void EmitChromePolys (void);
void EmitGlassPolys (void);
void R_DrawSkyChain (void);
void R_AddSkyBoxSurface (msurface_t *fa);
void R_ClearSkyBox (void);
void R_DrawSkyBox (void);
extern qboolean	r_skyboxloaded;
int R_SetSky (char *skyname);

// gl_draw.c
void GL_Set2D (void);

// gl_rmain.c
qboolean R_CullBox (vec3_t mins, vec3_t maxs);
qboolean R_CullSphere (vec3_t centre, float radius);

void R_RotateForEntity (entity_t *ent, qboolean shadow);

float R_GetVertexLightValue (byte ppitch, byte pyaw, float apitch, float ayaw);
float R_LerpVertexLight (byte ppitch1, byte pyaw1, byte ppitch2, byte pyaw2, float ilerp, float apitch, float ayaw);

void R_PolyBlend (void);
void R_BrightenScreen (void);

#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];

// gl_rlight.c
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);
void R_AnimateLight (void);
void R_RenderDlights (void);
int R_LightPoint (vec3_t p);

void R_InitVertexLights (void);
extern	float	bubblecolor[NUM_DLIGHTTYPES][4];
extern	vec3_t	lightspot, lightcolor;
extern	float	vlight_pitch, vlight_yaw;
extern	byte	anorm_pitch[NUMVERTEXNORMALS], anorm_yaw[NUMVERTEXNORMALS];

// gl_refrag.c
void R_StoreEfrags (efrag_t **ppefrag);

// gl_mesh.c
void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr);

// gl_rsurf.c
void EmitDetailPolys (void);
void R_DrawBrushModel (entity_t *e);
void R_DrawWorld (void);
void GL_BuildLightmaps (void);

// gl_rmisc
void R_InitOtherTextures (void);

void Mod_LoadQ3Model(model_t *mod, void *buffer);
void R_DrawQ3Model(entity_t *e);
//void R_Q3DamageDraw (void);

//extern cvar_t	gl_hwblend;

void CheckDecals (void);
extern int decals_enabled;
typedef	byte	col_t[4];
extern int decal_blood1,decal_blood2,decal_blood3, decal_burn, decal_mark, decal_glow;

extern void CL_NewDlight (int key, vec3_t origin, float radius, float time, int type);

#define	TURBSINSIZE	128
#define	TURBSCALE	((float)TURBSINSIZE / (2 * M_PI))

extern cvar_t gl_decal_count;
extern cvar_t gl_decal_blood;
extern cvar_t gl_decal_bullets;
extern cvar_t gl_decal_sparks;
extern cvar_t gl_decal_explosions;
extern cvar_t gl_decal_quality;
extern cvar_t gl_decaltime;
extern cvar_t gl_decal_viewdistance;

void R_SpawnDecalStatic(vec3_t org, int tex, int size);
void R_SpawnDecal(vec3_t center, vec3_t normal, vec3_t tangent, int tex, int size, float dtime, float dalpha);
#define ISDEAD(i) ((i) >= 41 && (i) <= 102)

//R00k: This is a crude FIRST attempt at a realtime server list.
#define SERVERLIST_URL "http://servers.quakeone.com/"
#define SERVERLIST_FILE "index.php?format=proquake"

//New stuff from RMQengine	--TESTING--

#define SAFE_FREE(xxxx) {if (xxxx) {free (xxxx); (xxxx) = NULL;}}
// vbo/pbo
#ifdef USESHADERS
extern PFNGLBINDBUFFERPROC qglBindBuffer;
extern PFNGLDELETEBUFFERSPROC qglDeleteBuffers;
extern PFNGLGENBUFFERSPROC qglGenBuffers;
extern PFNGLISBUFFERPROC qglIsBuffer;
extern PFNGLBUFFERDATAPROC qglBufferData;
extern PFNGLBUFFERSUBDATAPROC qglBufferSubData;
extern PFNGLGETBUFFERSUBDATAPROC qglGetBufferSubData;
extern PFNGLMAPBUFFERPROC qglMapBuffer;
extern PFNGLUNMAPBUFFERPROC qglUnmapBuffer;
extern PFNGLGETBUFFERPARAMETERIVPROC qglGetBufferParameteriv;
extern PFNGLGETBUFFERPOINTERVPROC qglGetBufferPointerv;

// vertex/fragment program
extern PFNGLBINDPROGRAMARBPROC qglBindProgramARB;
extern PFNGLDELETEPROGRAMSARBPROC qglDeleteProgramsARB;
extern PFNGLGENPROGRAMSARBPROC qglGenProgramsARB;
extern PFNGLGETPROGRAMENVPARAMETERDVARBPROC qglGetProgramEnvParameterdvARB;
extern PFNGLGETPROGRAMENVPARAMETERFVARBPROC qglGetProgramEnvParameterfvARB;
extern PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC qglGetProgramLocalParameterdvARB;
extern PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC qglGetProgramLocalParameterfvARB;
extern PFNGLGETPROGRAMSTRINGARBPROC qglGetProgramStringARB;
extern PFNGLGETPROGRAMIVARBPROC qglGetProgramivARB;
extern PFNGLISPROGRAMARBPROC qglIsProgramARB;
extern PFNGLPROGRAMENVPARAMETER4DARBPROC qglProgramEnvParameter4dARB;
extern PFNGLPROGRAMENVPARAMETER4DVARBPROC qglProgramEnvParameter4dvARB;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC qglProgramEnvParameter4fARB;
extern PFNGLPROGRAMENVPARAMETER4FVARBPROC qglProgramEnvParameter4fvARB;
extern PFNGLPROGRAMLOCALPARAMETER4DARBPROC qglProgramLocalParameter4dARB;
extern PFNGLPROGRAMLOCALPARAMETER4DVARBPROC qglProgramLocalParameter4dvARB;
extern PFNGLPROGRAMLOCALPARAMETER4FARBPROC qglProgramLocalParameter4fARB;
extern PFNGLPROGRAMLOCALPARAMETER4FVARBPROC qglProgramLocalParameter4fvARB;
extern PFNGLPROGRAMSTRINGARBPROC qglProgramStringARB;

extern PFNGLVERTEXATTRIBPOINTERARBPROC qglVertexAttribPointerARB;
extern PFNGLENABLEVERTEXATTRIBARRAYARBPROC qglEnableVertexAttribArrayARB;
extern PFNGLDISABLEVERTEXATTRIBARRAYARBPROC qglDisableVertexAttribArrayARB;

extern PFNGLDRAWRANGEELEMENTSPROC qglDrawRangeElements;
extern PFNGLLOCKARRAYSEXTPROC qglLockArrays;
extern PFNGLUNLOCKARRAYSEXTPROC qglUnlockArrays;
extern PFNGLMULTIDRAWARRAYSPROC qglMultiDrawArrays;
extern PFNGLSECONDARYCOLORPOINTERPROC qglSecondaryColorPointer;

extern qboolean gl_arb_texture_compression;
extern qboolean gl_ext_compiled_vertex_array;

typedef struct gl_brokendrivers_s
{
	qboolean IntelBroken;
	qboolean AMDATIBroken;
	qboolean NVIDIABroken;
} gl_brokendrivers_t;

extern gl_brokendrivers_t gl_brokendrivers;

#define VAA_0		(1 << 0)
#define VAA_1		(1 << 1)
#define VAA_2		(1 << 2)
#define VAA_3		(1 << 3)
#define VAA_4		(1 << 4)
#define VAA_5		(1 << 5)
#define VAA_6		(1 << 6)
#define VAA_7		(1 << 7)
#define VAA_8		(1 << 8)
#define VAA_9		(1 << 9)
#define VAA_10		(1 << 10)
#define VAA_11		(1 << 11)
#define VAA_12		(1 << 12)
#define VAA_13		(1 << 13)
#define VAA_14		(1 << 14)
#define VAA_15		(1 << 15)

void GL_VertexArrayPointer (GLuint buffer, int varray, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLvoid *pointer);
void GL_EnableVertexArrays (unsigned int arraymask);
void GL_DrawElements (GLuint buffer, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, GLvoid *indices);
void GL_DrawArrays (GLenum mode, GLint first, GLsizei count);
void GL_UncacheArrays (void);

void GL_SetVertexProgram (GLuint vp);
void GL_SetFragmentProgram (GLuint fp);
void GL_SetPrograms (GLuint vp, GLuint fp);
void GL_BeginBuffers (void);
GLuint GL_GetBuffer (void);
void GL_CreateBuffers (void);
void GL_GetBufferExtensions (void);

GLuint GL_CreateProgram (GLenum mode, const char *source, qboolean fog);
// this is our generic vertex transform to keep me from getting RSI from typing this out every time
#define TRANSFORMVERTEX(out, in) \
	"TEMP outtmp;\n" \
	"DP4 outtmp.x, state.matrix.mvp.row[0], "in";\n" \
	"DP4 outtmp.y, state.matrix.mvp.row[1], "in";\n" \
	"DP4 outtmp.z, state.matrix.mvp.row[2], "in";\n" \
	"DP4 outtmp.w, state.matrix.mvp.row[3], "in";\n" \
	"MOV "out", outtmp;\n" \
	"MOV result.fogcoord, outtmp.z;\n"

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

#endif
