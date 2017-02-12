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
// gl_rmain.c

#include "quakedef.h"

entity_t	r_worldentity;
gl_matrix_t	r_world_matrix;
qboolean	r_cache_thrash;		// compatibility

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking
int			cl_hurtblur;		// R00k
mplane_t	frustum[4];

int		c_brush_polys, c_alias_polys, c_md3_polys;

int		particletexture;	// little dot for particles
int		playertextures;
int		fb_playertextures;

int		skyboxtextures;		// by joe
int		underwatertexture, detailtexture;
int		logoTex;
int		chrometexture, glasstexture;//, glosstexture;
int		solidskytexture2, alphaskytexture2;//R00k
int		decal_blood1,decal_blood2,decal_blood3,decal_burn, decal_mark, decal_glow;

#define INTERP_WEAP_MAXNUM		24
#define INTERP_WEAP_MINDIST		5000
#define INTERP_WEAP_MAXDIST		95000
#define	INTERP_MINDIST			70
#define	INTERP_MAXDIST			300

typedef struct interpolated_weapon
{
	char	name[MAX_QPATH];
	int		maxDistance;
} interp_weapon_t;

static	interp_weapon_t	interpolated_weapons[INTERP_WEAP_MAXNUM];
static	int	interp_weap_num = 0;

int DoWeaponInterpolation (void);

// view origin
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

// screen size info
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

static	float	shadescale = 0;

//johnfitz -- struct for passing lerp information to drawing functions
typedef struct 
{
	short pose1;
	short pose2;
	float blend;
	vec3_t origin;
	vec3_t angles;
}lerpdata_t;
//johnfitz

qboolean OnChange_Cache_Flush (cvar_t *var, char *string);

cvar_t	cl_autodemo			= {"cl_autodemo","0",false,false};	//R00k
cvar_t	cl_gun_fovscale		= {"cl_gun_fovscale","1",true};//R00k

cvar_t	cl_teamskin			= {"cl_teamskin","0",true,false, OnChange_Cache_Flush};//R00k

cvar_t	cl_teamflags		= {"cl_teamflags","0",true};//R00k
cvar_t	r_showbboxes 		= {"r_showbboxes", "0",false,false};
cvar_t	r_drawentities		= {"r_drawentities", "1"};
cvar_t	r_drawviewmodel		= {"r_drawviewmodel", "1",true};
cvar_t	r_drawviewmodelsize = {"r_drawviewmodelsize", "1", true};
cvar_t	r_interpolate_light	= {"r_interpolate_light", "0",true};
cvar_t	r_speeds			= {"r_speeds", "0"};
cvar_t	r_fullbright		= {"r_fullbright", "0"};
cvar_t	r_shadows			= {"r_shadows", "0"};
cvar_t	r_shadows_throwdistance = {"r_shadows_throwdistance", "0.275"};
cvar_t	r_wateralpha		= {"r_wateralpha", "1",true};
cvar_t	r_lavaalpha			= {"r_lavaalpha", "1",true};
cvar_t	r_telealpha			= {"r_telealpha", "1",true};
cvar_t	r_turbalpha_distance = {"r_turbalpha_distance", "512",true};
cvar_t	r_explosion_alpha	= {"r_explosion_alpha", "0.25",true};
cvar_t	r_dynamic			= {"r_dynamic", "1",true};
cvar_t	r_novis				= {"r_novis", "0", true};
cvar_t	r_fastsky			= {"r_fastsky", "0",true};
cvar_t	r_fastturb			= {"r_fastturb", "0",true};
cvar_t	r_drawflame			= {"r_drawflame","1",true};
cvar_t	r_drawlocs			= {"r_drawlocs", "0",false};
cvar_t	r_farclip			= {"r_farclip", "4096"};
qboolean OnChange_r_skybox (cvar_t *var, char *string);
cvar_t	r_skybox			= {"r_skybox", "", false, false, OnChange_r_skybox};
cvar_t	r_skyscroll			= {"r_skyscroll", "0",true};
cvar_t	r_skyspeed			= {"r_skyspeed", "2", true};//MHQuake
cvar_t	r_simpleitems		= {"r_simpleitems", "0",true};
cvar_t	r_outline			= {"r_outline", "0",true};	//QMB
cvar_t	r_outline_surf		= {"r_outline_surf", "0",true};	
cvar_t	r_celshading		= {"r_celshading", "0",true};//QMB
cvar_t	r_waterwarp			= {"r_waterwarp", "0"};
cvar_t  r_waterripple		= {"r_waterripple", "0"};
//cvar_t	r_netgraph			= {"r_netgraph","0"};

cvar_t	gl_lavasmoke		= {"gl_lavasmoke", "0",true};	//MHQuake
cvar_t	gl_shiny			= {"gl_shiny", "0",true};
cvar_t	gl_rain				= {"gl_rain", "0",true};

cvar_t	gl_clip_muzzleflash = {"gl_clip_muzzleflash", "1", true, false, OnChange_Cache_Flush};//R00k: eliminate the muzzleflash verts from the v_models. 

cvar_t	gl_powerupshells	 = {"gl_powerupshells", "0", true};
cvar_t	gl_powerupshell_size = {"gl_powerupshell_size", "0", true};
cvar_t	gl_damageshells		 = {"gl_damageshells", "0", true};
cvar_t	scr_bloodsplat		 = {"scr_bloodsplat", "0", true};

cvar_t r_uwfactor = {"r_uwfactor", "512", true};
cvar_t r_uwscale = {"r_uwscale", "2", true};

qboolean r_dowarp = false;

#ifndef USEFAKEGL
extern cvar_t r_bloom;
extern cvar_t r_bloom_alpha;
extern cvar_t r_bloom_color;	
extern cvar_t r_bloom_diamond_size;
extern cvar_t r_bloom_intensity;
extern cvar_t r_bloom_darken;
extern cvar_t r_bloom_sample_size;
extern cvar_t r_bloom_fast_sample;
#endif
extern cvar_t vid_bpp;

cvar_t  gl_interpolate_transform = {"gl_interpolate_transform", "1",true};
cvar_t  gl_interpolate_animation = {"gl_interpolate_animation", "1",true};

cvar_t	gl_clear			= {"gl_clear", "0",false};
cvar_t	gl_cull				= {"gl_cull", "1",false};
cvar_t	gl_smoothmodels		= {"gl_smoothmodels", "1",false};
cvar_t	gl_affinemodels		= {"gl_affinemodels", "0"};
cvar_t	gl_polyblend		= {"gl_polyblend", "1"};
cvar_t	gl_flashblend		= {"gl_flashblend", "0",true};
cvar_t	gl_playermip		= {"gl_playermip", "0"};
cvar_t	gl_nocolors			= {"gl_nocolors", "0"};
cvar_t	gl_finish			= {"gl_finish", "0"};
cvar_t	gl_loadlitfiles		= {"gl_loadlitfiles", "1",true};
cvar_t	gl_loadq3models		= {"gl_loadq3models", "0",true};
cvar_t	gl_doubleeyes		= {"gl_doubleeyes", "1"};
cvar_t	gl_interdist		= {"gl_interpolate_distance", "512"};//was 17000
cvar_t  gl_waterfog			= {"gl_waterfog", "0",true};
cvar_t  gl_waterfog_density = {"gl_waterfog_density", "1",true};
cvar_t	gl_detail			= {"gl_detail", "0",true};
cvar_t	gl_caustics			= {"gl_caustics", "1",true};
cvar_t	gl_ringalpha		= {"gl_ringalpha", "0.4"};
cvar_t	gl_fb_bmodels		= {"gl_fb_bmodels", "1",true};
cvar_t	gl_fb_models		= {"gl_fb_models", "1", true};
cvar_t  gl_vertexlights		= {"gl_vertexlights", "0",true};// 1 = all models 2 = gun model only
cvar_t	gl_laserpoint		= {"gl_laserpoint", "0",true};
cvar_t	gl_part_explosions	= {"gl_part_explosions", "1",true};
cvar_t	gl_part_trails		= {"gl_part_trails", "1",true};
cvar_t	gl_part_sparks		= {"gl_part_sparks", "1",true};
cvar_t	gl_part_gunshots	= {"gl_part_gunshots", "1",true};
cvar_t	gl_part_blood		= {"gl_part_blood", "1",true};
cvar_t	gl_part_telesplash	= {"gl_part_telesplash", "1",true};
cvar_t	gl_part_blobs		= {"gl_part_blobs", "1",true};
cvar_t	gl_part_lavasplash	= {"gl_part_lavasplash", "1",true};
cvar_t	gl_part_flames		= {"gl_part_flames", "1",true};
cvar_t	gl_part_lightning	= {"gl_part_lightning", "1",true};
cvar_t	gl_part_flies		= {"gl_part_flies", "1", true};
cvar_t	gl_part_muzzleflash	= {"gl_part_muzzleflash", "1", true};
cvar_t	gl_overbright		= {"gl_overbright", "0", true}; 
cvar_t	gl_textureless		= {"gl_textureless", "0"}; 
cvar_t	gl_motion_blur		= {"gl_motion_blur", "0", true};//R00k
cvar_t	gl_waterblur		= {"gl_waterblur", "0.5", true};//R00k
cvar_t	gl_nightmare		= {"gl_nightmare", "0", true};//R00k
cvar_t	gl_deathblur		= {"gl_deathblur", "0", true};//R00k
cvar_t	gl_hurtblur			= {"gl_hurtblur", "0", true};//R00k
cvar_t	gl_anisotropic		= {"gl_anisotropic","1", true};
cvar_t  gl_fogenable		= {"gl_fogenable", "0"};
cvar_t	gl_fogdensity		= {"gl_fogdensity", "0.153"};
cvar_t  gl_fogstart			= {"gl_fogstart", "50.0"};
cvar_t  gl_fogend			= {"gl_fogend", "2048.0"};
cvar_t  gl_fogred			= {"gl_fogred", "0.4"};
cvar_t  gl_foggreen			= {"gl_foggreen", "0.4"};
cvar_t  gl_fogblue			= {"gl_fogblue", "0.4"};
cvar_t  gl_fogsky			= {"gl_fogsky", "1"}; 
cvar_t	gl_lightning_alpha	= {"gl_lightning_alpha","1"};
cvar_t	gl_lightning_type	= {"gl_lightning_type","0"};
cvar_t	gl_color_deadbodies = {"gl_color_deadbodies","1"};

float	shadelight, ambientlight;

extern qboolean have_stencil; // Stencil shadows - MrG
void R_MarkLeaves (void);
void R_InitBubble (void);

extern	int	particle_mode;	// off: classic, on: QMB(default)
extern	int	decals_enabled;
extern	cvar_t	scr_fov;

qboolean OnChange_Cache_Flush (cvar_t *var, char *string)
{
	float update;
	update = atof(string);

	if (update == var->value)
		return false;	

	Cvar_SetValue(var->name,atof(string));
	Cache_Flush ();
	return true;
}

#ifdef USESHADERS
//Mh's waterwarp shader from RMQengine
extern void RTTWarp_EmitVertex (float *vert, float x, float y, float *rgba, float s, float t);
#endif	//USESHADERS

//lxndr - QUORE
void R_CalcRGB (float color, float intensity, float *rgb) 
{
    float	c, i;

    if (color == 0.5) 
	{
        rgb[0] = rgb[1] = rgb[2] = intensity;
    } 
	else 
	{
        c = (color * 0.8 + 0.1) * M_PI;
        i = intensity * 1.1 - 1;

        rgb[0] = cos (c / 2) + i;
        rgb[1] = sin (c) + i;
        rgb[2] = sin (c / 2) + i;
    }
}

void GL_DrawSimpleBox(vec3_t org, float minx, float miny, float minz, float maxx, float maxy, float maxz, vec3_t color, qboolean cull)
{	
	int i;
	float verts[8*3] =
	{
		minx, miny, minz,
		maxx, miny, minz,
		maxx, miny, maxz,
		minx, miny, maxz,
		minx, maxy, minz,
		maxx, maxy, minz,
		maxx, maxy, maxz,
		minx, maxy, maxz,
	};
	static const unsigned char inds[6*5] =
	{
		0,  7, 6, 5, 4,
		1,  0, 1, 2, 3,
		2,  1, 5, 6, 2,
		3,  3, 7, 4, 0,
		4,  2, 6, 7, 3,
		5,  0, 4, 5, 1,
	};
	
	const unsigned char* in = inds;
	glColor4f(color[0], color[1], color[2], 0.05f);
	glDepthMask (0);	

	if (!cull)
		glDisable(GL_DEPTH_TEST); 

	if (have_stencil) 
	{
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_EQUAL,1,2);
		glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);
	}

	glPushMatrix ();
	glTranslatef(org[0], org[1], org[2]); 
	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	glDisable (GL_CULL_FACE);

	glBegin(GL_QUADS);

	for (i = 0; i < 6; ++i)
	{
		in++;	
		glVertex3fv(&verts[*in*3]); in++;
		glVertex3fv(&verts[*in*3]); in++;
		glVertex3fv(&verts[*in*3]); in++;
		glVertex3fv(&verts[*in*3]); in++;
	}

	glEnd(); 

	if (have_stencil) 
	{
		glDisable(GL_STENCIL_TEST);
	}
	
	glDepthMask (1);	

	if (!cull)
		glEnable(GL_DEPTH_TEST); 

	glEnable (GL_CULL_FACE);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glPopMatrix(); 				
	glColor4f(1,1,1,1);
}
/*
=================
R_CullBox -- replaced with new function from lordhavoc

Returns true if the box is completely outside the frustum
=================
*/
qboolean R_CullBox (vec3_t emins, vec3_t emaxs)
{
	int i;
	mplane_t *p;

	for (i = 0;i < 4;i++)
	{
		p = frustum + i;
		switch(p->signbits)
		{
		default:
		case 0:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		}
	}
	return false;
}


/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
qboolean R_CullSphere (vec3_t centre, float radius)
{
	int		i;
	mplane_t	*p;

	for (i = 0, p = frustum ; i < 4 ; i++, p++)
	{
		if (PlaneDiff(centre, p) <= -radius)		
			return true;
	}

	return false;
}

void GL_PolygonOffset (int offset)
{
	if (offset > 0)
	{
		glEnable (GL_POLYGON_OFFSET_FILL);
		glEnable (GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(1, offset);
	}
	else if (offset < 0)
	{
		glEnable (GL_POLYGON_OFFSET_FILL);
		glEnable (GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(-1, offset);
	}
	else
	{
		glDisable (GL_POLYGON_OFFSET_FILL);
		glDisable (GL_POLYGON_OFFSET_LINE);
	}
}

/*
=============
R_RotateForEntity
=============
*/
void R_RotateForEntity (entity_t *ent, qboolean shadow)
{
	float	lerpfrac, timepassed;
	vec3_t	d, interpolated;
	int	i;
	
	// positional interpolation
	timepassed = cl.ctime - ent->translate_start_time;

	if (ent->translate_start_time == 0 || timepassed > 1)
	{
		ent->translate_start_time = cl.ctime;
		VectorCopy (ent->origin, ent->origin1);
		VectorCopy (ent->origin, ent->origin2);
	}

	if (!VectorCompare(ent->origin, ent->origin2))
	{
		ent->translate_start_time = cl.ctime;
		VectorCopy (ent->origin2, ent->origin1);
		VectorCopy (ent->origin,  ent->origin2);
		lerpfrac = 0;
	}
	else
	{
		lerpfrac =  timepassed / 0.1;
		if (cl.paused || lerpfrac > 1)
			lerpfrac = 1;
	}

	VectorInterpolate (ent->origin1, lerpfrac, ent->origin2, interpolated);

	glTranslatef (interpolated[0], interpolated[1], interpolated[2]);	

	// orientation interpolation (Euler angles, yuck!)
	timepassed = cl.time - ent->rotate_start_time; 

	if (ent->rotate_start_time == 0 || timepassed > 1)
	{
		ent->rotate_start_time = cl.time;
		VectorCopy (ent->angles, ent->angles1);
		VectorCopy (ent->angles, ent->angles2);
	}

	if (!VectorCompare(ent->angles, ent->angles2))
	{
		ent->rotate_start_time = cl.time;
		VectorCopy (ent->angles2, ent->angles1);
		VectorCopy (ent->angles,  ent->angles2);
		lerpfrac = 0;
	}
	else
	{
		lerpfrac =  timepassed / 0.1;
		if (cl.paused || lerpfrac > 1)
			lerpfrac = 1;
	}

	VectorSubtract (ent->angles2, ent->angles1, d);

	// always interpolate along the shortest path
	for (i=0 ; i<3 ; i++)
	{
		if (d[i] > 180)
			d[i] -= 360;
		else if (d[i] < -180)
			d[i] += 360;
	}

	glRotatef (ent->angles1[1] + (lerpfrac * d[1]), 0, 0, 1);

	if (!shadow)
	{
		glRotatef (-ent->angles1[0] + (-lerpfrac * d[0]), 0, 1, 0);
		glRotatef (ent->angles1[2] + (lerpfrac * d[2]), 1, 0, 0);
	}
}
/*
=============
R_RotateForViewEntity
=============
*/
void R_RotateForViewEntity (entity_t *ent)
{
	glTranslatef (ent->origin[0], ent->origin[1], ent->origin[2]);

	glRotatef (ent->angles[1], 0, 0, 1);
	glRotatef (-ent->angles[0], 0, 1, 0);
	glRotatef (ent->angles[2], 1, 0, 0);
}

void GL_FullscreenQuad(int texture, float alpha)
{
	int vwidth = 1, vheight = 1;
	float vs, vt;

//	float *verts = (float *) scratchbuf;

	while (vwidth < glwidth)
	{
		vwidth *= 2;
	}
	while (vheight < glheight)
	{
		vheight *= 2;
	}

	glViewport (glx, gly, glwidth, glheight);

	GL_Bind(texture);

	// go 2d
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity ();
	glOrtho  (0, glwidth, 0, glheight, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity ();

	vs = ((float)glwidth / vwidth);
	vt = ((float)glheight / vheight) ;

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	
	glAlphaFunc(GL_GREATER,0.000f);
	glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask(0);

	glColor4f(1, 1, 1, alpha);

	glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2f(0, 0);
		glTexCoord2f(vs , 0);
		glVertex2f(glwidth, 0);
		glTexCoord2f(vs , vt);
		glVertex2f(glwidth, glheight);
		glTexCoord2f(0, vt);
		glVertex2f(0, glheight);
	glEnd();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(1);
	glDisable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER,0.666f);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor3f (1,1,1);   
    glDisable (GL_BLEND);   
    glEnable (GL_TEXTURE_2D);   
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);   
	//?glEnable(GL_CULL_FACE);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();		
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();		
}
/*
===============================================================================

				SPRITE MODELS

===============================================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t	*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int		i, numframes, frame;
	float		*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_DPrintf (1,"R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

/*
=================
R_DrawSpriteModel
=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	vec3_t	point, forward, right, up, org, offset;
	mspriteframe_t *frame;
	msprite_t *psprite;
	extern void R_SetupLighting (entity_t *ent);
	float out[3], l, alpha = 1;
	extern float shadelight, ambientlight;

	alpha = (e->transparency < 1.0f) ? e->transparency : 1;

	if ((alpha >= 1) && (r_simpleitems.value < 2))
	{
		R_SetupLighting(e);
		out[0]=out[1]=out[2]= 0.99609375;
		l = (shadelight + ambientlight) / 256;
		VectorScale(out, l, out);// lower lightlevel in shade
	}
	else
		l = 255;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{
		// bullet marks on walls
		AngleVectors (currententity->angles, forward, right, up);
	}
	else if (psprite->type == SPR_FACING_UPRIGHT)
	{
		VectorSet (up, 0, 0, 1);
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalize (right);
	}
	else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT)
	{
		VectorSet (up, 0, 0, 1);
		VectorCopy (vright, right);
	}
	else
	{	// normal sprite
		VectorCopy (vup, up);
		VectorCopy (vright, right);
	}

	GL_Bind (frame->gl_texturenum);

   	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if ((alpha < 1) || (r_simpleitems.value >= 2))
	{
		glBlendFunc (GL_SRC_COLOR, GL_SRC_ALPHA);
		glEnable (GL_BLEND);

		if (r_simpleitems.value >= 2)
			alpha = 0.666;

		glColor4f(1, 1, 1, alpha);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}
	else
		glColor4f(out[0], out[1], out[2], alpha);

	VectorCopy(e->origin, org);
	
	//AAS: brush models require some additional centering
	if (e->model->type == mod_brush)
	{
		VectorSubtract(e->model->maxs, e->model->mins, offset);
		offset[2] = 0;
		VectorMA(org, 0.5, offset, org);
	}

	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMA (org, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (org, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (org, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);
	
	glEnd ();

	if ((alpha < 1) || (r_simpleitems.value >= 2))
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glDisable (GL_BLEND);
	}

	glColor4f(1,1,1,1);
}

/*
===============================================================================

				ALIAS MODELS

===============================================================================
*/

//#define NUMVERTEXNORMALS	162

float r_avertexnormals[NUMVERTEXNORMALS][3] = 
#include "anorms.h"
;

vec3_t	shadevector;

qboolean full_light;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16

float	r_avertexnormal_dots[SHADEDOT_QUANT][256] = 
#include "anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

float	apitch, ayaw;

#ifndef USEFAKEGL
//==============================================================================
//GLQuake stencil shadows : Rich Whitehouse

//TODO: add interpolate animation for shadows

float PROJECTION_DISTANCE;
#define MAX_STENCIL_ENTS			1024

static entity_t *g_stencilEnts[MAX_STENCIL_ENTS];
static int g_numStencilEnts = 0;

/*
=============
GL_GetTriNeighbors

Get the neighbors for a given triangle (meaning, said neighbor
shares an edge with this triangle).
-rww
=============
*/
void GL_GetTriNeighbors(aliashdr_t *paliashdr, mtriangle_t *tris, mtriangle_t *tri, int *neighbors)
{
	static mtriangle_t *n;
	int i = 0;
	int numN = 0;
	static int j, k;
	static int vertA1, vertA2, vertB1, vertB2;

	neighbors[0] = neighbors[1] = neighbors[2] = -1;

	while (i < paliashdr->numtris)
	{
		n = &tris[i];

		if (n != tri)
		{
			j = 0;
			while (j < 3)
			{
				vertA1 = tri->vertindex[j];
				vertA2 = tri->vertindex[(j+1)%3];

				k = 0;
				while (k < 3)
				{
					vertB1 = n->vertindex[k];
					vertB2 = n->vertindex[(k+1)%3];

					if ((vertA1 == vertB1 && vertA2 == vertB2) || (vertA1 == vertB2 && vertA2 == vertB1))
					{
						if (neighbors[j] == -1)
						{
							neighbors[j] = i;
						}
						else
						{ //this edge is shared by more than 2 tris apparently.
						  //this seems to actually happen at times, which is why
						  //we are bothering to check for it.
							neighbors[j] = -2;
						}
					}

					k++;
				}

				j++;
			}
		}

		i++;
	}

	j = 0;
	while (j < 3)
	{ //mark any -2 as -1 now since we're done
		if (neighbors[j] == -2)
		{
			neighbors[j] = -1;
		}
		j++;
	}
}

/*
=============
GL_TriPrecalc

Do whatever precalc'ing we can (in this case just get neighbors).
We could also pre-determine planeEq's for each tri*each frame in
here if we desired.
-rww
=============
*/
void GL_TriPrecalc(aliashdr_t *paliashdr, mtriangle_t *tris, trivertx_t *verts)
{
	int i = 0;
	mtriangle_t *tri;

	while (i < paliashdr->numtris)
	{
		tri = &tris[i];
		GL_GetTriNeighbors(paliashdr, tris, tri, tri->neighbors);
		i++;
	}
}

/*
=============
GL_TriFacingLight

Determine of a given triangle is facing the light position. A lot
of this could probably be precalculated, but I am lazy. To store
the planeEq we would have to store one for every frame since the
tri remains the same but the verts change based on the frame.
The calculation is relatively inexpensive so it is at least
somewhat reasonable to perform it in realtime.
-rww
=============
*/
int GL_TriFacingLight(mtriangle_t *tri, trivertx_t *verts, vec3_t lightPosition)
{
	byte *v1;
	byte *v2;
	byte *v3;
	float side;
	float planeEq[4];

	v1 = &verts[tri->vertindex[0]].v[0];
	v2 = &verts[tri->vertindex[1]].v[0];
	v3 = &verts[tri->vertindex[2]].v[0];

	planeEq[0] = v1[1]*(v2[2]-v3[2]) + v2[1]*(v3[2]-v1[2]) + v3[1]*(v1[2]-v2[2]);
	planeEq[1] = v1[2]*(v2[0]-v3[0]) + v2[2]*(v3[0]-v1[0]) + v3[2]*(v1[0]-v2[0]);
	planeEq[2] = v1[0]*(v2[1]-v3[1]) + v2[0]*(v3[1]-v1[1]) + v3[0]*(v1[1]-v2[1]);
	planeEq[3] = -( v1[0]*( v2[1]*v3[2] - v3[1]*v2[2] ) +
				v2[0]*(v3[1]*v1[2] - v1[1]*v3[2]) +
				v3[0]*(v1[1]*v2[2] - v2[1]*v1[2]) );

	side = planeEq[0]*lightPosition[0]+
		planeEq[1]*lightPosition[1]+
		planeEq[2]*lightPosition[2]+
		planeEq[3];

	if ( side > 0 )
	{
		return 1;
	}

	return 0;
}
/*
=============
GL_ShadowPass

Determine and draw our shadow volume
-rww
=============
*/
void GL_ShadowPass(aliashdr_t *paliashdr, int posenum, vec3_t lightPosition)
{
	static byte *v1;
	static byte *v2;
	static vec3_t v3;
	static vec3_t v4;
	static int i;
	static int j;
	static int neighborIndex;
	static trivertx_t *verts;
	static mtriangle_t *tris;

	PROJECTION_DISTANCE = r_shadows_throwdistance.value;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->baseposedata);
	tris = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);
	verts += posenum * paliashdr->numverts;

	//go through all the triangles
	for ( i = 0; i < paliashdr->numtris; i++ )
	{
		mtriangle_t *tri = &tris[i];

		//Only bother if this tri is facing the light pos
		if (GL_TriFacingLight(tri, verts, lightPosition))
		{
			for ( j = 0; j < 3; j++ )
			{
				neighborIndex = tri->neighbors[j];

				//If the tri has no neighbor or the neighbor is not facing the light,
				//then it is an edge
				if (neighborIndex == -1 || !GL_TriFacingLight(&tris[neighborIndex], verts, lightPosition))
				{
					v1 = &verts[tri->vertindex[j]].v[0];
					v2 = &verts[tri->vertindex[( j+1 )%3]].v[0];

					//get positions of v3 and v4 based on the light position
					v3[0] = ( v1[0]-lightPosition[0] )*PROJECTION_DISTANCE;
					v3[1] = ( v1[1]-lightPosition[1] )*PROJECTION_DISTANCE;
					v3[2] = ( v1[2]-lightPosition[2] )*PROJECTION_DISTANCE;

					v4[0] = ( v2[0]-lightPosition[0] )*PROJECTION_DISTANCE;
					v4[1] = ( v2[1]-lightPosition[1] )*PROJECTION_DISTANCE;
					v4[2] = ( v2[2]-lightPosition[2] )*PROJECTION_DISTANCE;

					//Now draw the quad from the two verts to the projected light
					//verts
					glBegin( GL_QUAD_STRIP );
						glVertex3f( v1[0], v1[1], v1[2] );
						glVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
						glVertex3f( v2[0], v2[1], v2[2] );
						glVertex3f( v2[0]+v4[0], v2[1]+v4[1], v2[2]+v4[2] );
					glEnd();
				}
			}

			//draw the front cap for the shadow volume
			glBegin(GL_TRIANGLES);
			for ( j = 0; j < 3; j++ )
			{
				v1 = &verts[tri->vertindex[j]].v[0];
				glVertex3f(v1[0], v1[1], v1[2]);
			}
			glEnd();

			//Now the back cap. We draw it backwards to assure it is drawn
			//facing outward from the shadow volume.
			glBegin(GL_TRIANGLES);
			for (j = 0; j < 3; j++ )
			{
				v1 = &verts[tri->vertindex[2-j]].v[0];
				v3[0] = ( v1[0]-lightPosition[0] )*PROJECTION_DISTANCE;
				v3[1] = ( v1[1]-lightPosition[1] )*PROJECTION_DISTANCE;
				v3[2] = ( v1[2]-lightPosition[2] )*PROJECTION_DISTANCE;

				glVertex3f( v1[0]+v3[0], v1[1]+v3[1], v1[2]+v3[2] );
			}
			glEnd();
		}
	}
}

/*
=============
GL_DrawAliasStencilShadow

Do both of the stencil buffer passes
-rww
=============
*/
void GL_DrawAliasStencilShadow (aliashdr_t *paliashdr, int posenum, vec3_t lightPosition)
{
	glCullFace(GL_FRONT);
	glStencilOp( GL_KEEP, GL_INCR, GL_KEEP );
	GL_ShadowPass( paliashdr, posenum, lightPosition );

	glCullFace(GL_BACK);
	glStencilOp( GL_KEEP, GL_DECR, GL_KEEP );
	GL_ShadowPass( paliashdr, posenum, lightPosition );
}

/*
=================
GL_StencilShadowModel

Do the shadowing for an ent
-rww
=================
*/
void GL_StencilShadowModel(entity_t *e)
{
	int				pose, numposes;

	aliashdr_t		*paliashdr;
	vec3_t			lightPos;

	if (!e->model)
	{
		return;
	}
	
	if (e->noshadow == true) 
		return;

	paliashdr = (aliashdr_t *)Mod_Extradata (e->model);

	if ((e->frame >= paliashdr->numframes) || (e->frame < 0))
	{
		Con_DPrintf (1,"R_AliasSetupFrame: no such frame %d\n", e->frame);
		e->frame = 0;
	}

	pose = paliashdr->frames[e->frame].firstpose;
	numposes = paliashdr->frames[e->frame].numposes;

	//Just some random light position.
	//This is relative to the entity position and angles.
	lightPos[0] = 45;
	lightPos[1] = 0;
	lightPos[2] = 1024;

	glPushMatrix ();
	R_RotateForEntity (e, 0);
	glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	GL_DrawAliasStencilShadow(paliashdr, pose, lightPos);
	glPopMatrix ();
}

/*
=================
GL_StencilShadowing

Go through each frame and draw stencil shadows for appropriate ents
-rww
=================
*/
void GL_StencilShadowing(void)
{
	if (r_shadows.value < 2 || g_numStencilEnts <= 0)
	{
		return;
	}

	//Set us up for drawing our passes onto the stencil buffer
	glPushAttrib( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT | GL_POLYGON_BIT | GL_STENCIL_BUFFER_BIT );
	glDisable (GL_TEXTURE_2D);
	glDepthMask( GL_FALSE );
	glEnable(GL_CULL_FACE);

	if (r_shadows.value == 2)
	{ //> 2 is a debug feature to draw the shadows outside of the stencil buffer
		glEnable( GL_STENCIL_TEST );
		glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	}
	else
	{
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	}

	glStencilFunc(GL_ALWAYS, 0, 255);
	glDepthFunc(GL_LESS);

	while (g_numStencilEnts > 0)
	{ //go through the list and do all the shadows
		g_numStencilEnts--;
		GL_StencilShadowModel(g_stencilEnts[g_numStencilEnts]);
	}

	glCullFace(GL_FRONT);
	glDepthFunc(GL_LEQUAL);

	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

	//draw a full screen rectangle now - wherever we draw
	//is where our "shadows" will actually be seen.
	glColor4f( 0.0f, 0.0f, 0.0f, 0.666f );

	glStencilFunc( GL_NOTEQUAL, 1, 255 );
	glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

	glEnable (GL_BLEND);

	glPushMatrix(); //save the current view matrix

	glLoadIdentity ();

	glRotatef (-90,  1, 0, 0);
	glRotatef (90,  0, 0, 1);

	glBegin (GL_QUADS);
		glVertex3f (10, 100, 100);
		glVertex3f (10, -100, 100);
		glVertex3f (10, -100, -100);
		glVertex3f (10, 100, -100);
	glEnd ();

	glPopMatrix(); //so we aren't stuck with the identity matrix
	glPopAttrib(); //pop back our previous enable/etc. state so we don't have to bother switching it all back
}
#endif
void R_DrawAliasShellFrame (int frame, aliashdr_t *paliashdr, entity_t *ent, int distance, float col[3], float alpha)
{
	int			*order, count, pose, numposes;
	float		lerpfrac;
	trivertx_t	*verts1, *verts2;
	float		scroll[2], v[3], shell_size;

	shell_size = bound (0, gl_powerupshell_size.value, 10);

	if (gl_powerupshell_size.value > 5)
		glCullFace (GL_BACK);//R00k: this makes the model more visible

	if (have_stencil) 
	{
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_EQUAL,1,2);
		glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);
	}

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf (1,"R_DrawAliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += ent->pose1 * paliashdr->poseverts;
	verts2 += ent->pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	GL_Bind (underwatertexture);
	
	glEnable (GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	//rotate the image in a circle clockwise
	scroll[0] = (cos(cl.time)/2);
	scroll[1] = (sin(cl.time)/2);	

	glColor4f (col[0], col[1], col[2], alpha); 

	while ((count = *order++))
	{
		if (count < 0)
		{
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		}
		else
			glBegin(GL_TRIANGLE_STRIP);
		do
		{
			lerpfrac = VectorL2Compare(verts1->v, verts2->v, distance) ? ent->framelerp : 1;

			if (gl_interpolate_animation.value == 0)
				lerpfrac = 1;

			v[0] = r_avertexnormals[verts1->lightnormalindex][0] * shell_size + verts1->v[0];
			v[1] = r_avertexnormals[verts1->lightnormalindex][1] * shell_size + verts1->v[1];
			v[2] = r_avertexnormals[verts1->lightnormalindex][2] * shell_size + verts1->v[2];

			v[0] += lerpfrac * (r_avertexnormals[verts2->lightnormalindex][0] * shell_size + verts2->v[0] - v[0]);
			v[1] += lerpfrac * (r_avertexnormals[verts2->lightnormalindex][1] * shell_size + verts2->v[1] - v[1]);
			v[2] += lerpfrac * (r_avertexnormals[verts2->lightnormalindex][2] * shell_size + verts2->v[2] - v[2]);
			
			glTexCoord2f (((float *) order)[0] * 2.0f + scroll[0], ((float *) order)[1] * 2.0f + scroll[1]);
		
			glVertex3fv (v);				

			order += 2;
			verts1++;
			verts2++;
		} while (--count);
		 
		glEnd();
	}	
	 
	if (gl_powerupshell_size.value > 5)
		glCullFace (GL_FRONT);	

	if (have_stencil) 
	{
		glDisable(GL_STENCIL_TEST);
		//glClearStencil(1);
		//glClear(GL_STENCIL_BUFFER_BIT);
	}
	
	glDisable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	
}

//These colors coincide with the (unofficial) Quake Network Protocol Specs
float teamcolor[16][3] = 
{
	{123, 123, 123},// 0 Newspaper
	{83, 59, 27},	// 1 Brown
	{79, 79, 115},	// 2 Light Blue
	{55, 55, 7},	// 3 Green
	{71, 0, 0},		// 4 Red
	{95, 71, 7},	// 5 Mustard
	{143,67, 51},	// 6 Clay
	{127, 83, 63},	// 7 Flesh
	{87, 55, 67},	// 8 Lavendar
	{95, 51, 63},	// 9 Grape
	{107, 87, 71},	// 10 Biege
	{47, 67, 55},	// 11 Aqua					
	{123, 99, 7},	// 12 Lemon
	{47, 47, 127},	// 13 Blue
	{255, 114, 41},	// 14 Salmon
	{254, 0, 0},	// 15 Fire Engine Red
};

void TeamOutlinColor(int in)
{
	vec3_t out;
	float l;

	l = ((shadelight + ambientlight) / 512) / 1024;
	
	VectorCopy (teamcolor[in], out);
	VectorScale(out, l, out);
	glColor3f (out[0], out[1], out[2]);
}

void ColorMeBad(int in)
{
	vec3_t out;
	float l;

	l = 255;

	if (gl_overbright.value)
		l = ((shadelight + ambientlight) / 224) / 192;//darker
	else
		l = ((shadelight + ambientlight) / 224) / 224;

	VectorCopy (teamcolor[in], out);
	VectorScale(out, l, out);
	glColor4f (out[0], out[1], out[2], 1);
}

void R_DrawAliasCelSkinFrame  (int frame, aliashdr_t *paliashdr, entity_t *ent, int distance, int color, int texture)
{
	int			*order, count, pose, numposes;
	trivertx_t	*verts1, *verts2;
	float		lerpfrac;
	vec3_t		interpolated_verts;

	//cell shading
	extern	GLuint	celtexture, vertextexture;
	extern void vaMultiTexCoord1f (int st_array, float st1);
	float			iblend;
	GLfloat			normal[3];

	if ((ISTRANSPARENT(ent)))  //R00k: no celpass if this ent is transparent!
		return;

	if ((ent->noshadow)||(ent->modelindex == MOD_FLAME))
		return;

	GL_PolygonOffset (-2);

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf (1,"R_DrawAliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1) 
	{
		ent->frame_interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / ent->frame_interval) % numposes;
	}
	else
	{
		ent->frame_interval = 0.1;
	}

	if (ent->pose2 != pose)
	{
		ent->frame_start_time = cl.time;
		ent->pose1 = ent->pose2;
		ent->pose2 = pose;
		ent->framelerp = 0;		
	}
	else
	{
		ent->framelerp = (cl.time - ent->frame_start_time) / ent->frame_interval;		
	}

	if (cl.paused || ent->framelerp > 1)
		ent->framelerp = 1;

	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += ent->pose1 * paliashdr->poseverts;
	verts2 += ent->pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	GL_Bind(texture);

	if ((r_celshading.value) && (gl_mtexable)) 
	{	
		//setup for shading
		iblend = 1.0 - (ent->framelerp);
		GL_SelectTexture(GL_TEXTURE1);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glEnable(GL_TEXTURE_1D);

		if (r_celshading.value > 1)		
			glBindTexture (GL_TEXTURE_1D, vertextexture);
		else
			glBindTexture (GL_TEXTURE_1D, celtexture);		
	}
	else
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);		

	glEnable (GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	
	ColorMeBad (color);

	while ((count = *order++))
	{
		if (count < 0)
		{
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		}
		else
		{
			glBegin(GL_TRIANGLE_STRIP);
		}

		do 
		{		
			if ((r_celshading.value) && (gl_mtexable))
			{				
				normal[0] = (r_avertexnormals[verts1->lightnormalindex][0] * iblend + r_avertexnormals[verts2->lightnormalindex][0] * (ent->framelerp));
				normal[1] = (r_avertexnormals[verts1->lightnormalindex][1] * iblend + r_avertexnormals[verts2->lightnormalindex][1] * (ent->framelerp));
				normal[2] = (r_avertexnormals[verts1->lightnormalindex][2] * iblend + r_avertexnormals[verts2->lightnormalindex][2] * (ent->framelerp));
				
				glNormal3f(normal[0],normal[1],normal[2]);

				// texture coordinates come from the draw list
				qglMultiTexCoord1f(GL_TEXTURE1, bound(0,DotProduct(shadevector,normal),1));
				qglMultiTexCoord2f(GL_TEXTURE0, ((float *)order)[0], ((float *)order)[1]);	
			}
			else
			{			
				glTexCoord2f (((float *)order)[0], ((float *)order)[1]);
			}

			lerpfrac = VectorL2Compare(verts1->v, verts2->v, distance) ? ent->framelerp : 1;
			VectorInterpolate (verts1->v, lerpfrac, verts2->v, interpolated_verts);
			glVertex3fv (interpolated_verts);				

			order += 2;
			verts1++;
			verts2++;
		} while (--count);

		 
		glEnd();
	}	
	 

	if ((r_celshading.value) && (gl_mtexable))
	{
		glDisable(GL_TEXTURE_1D);		
		GL_SelectTexture(GL_TEXTURE0);
	}

	GL_PolygonOffset (0);
	glDisable (GL_BLEND);	
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	
}

/*
=============
R_DrawAliasOutlineFrame
=============
*/

void R_DrawAliasOutlineFrame (int frame, aliashdr_t *paliashdr, entity_t *ent, int distance)
{
	int			*order, count, pose, numposes;
	vec3_t		interpolated_verts;
	float		lerpfrac;	
	trivertx_t	*verts1, *verts2;
	qboolean	lerpmdl = true;
	float		line_width = bound (1,r_outline.value,3);

	if (ent->transparency < 1.0f)
		return;

	if (ent->model->modhint == MOD_EYES)//No outlines on eyes please!
		return;

	if (ent->model->modhint == MOD_FLAME)
	{
		return;
	}


	glCullFace (GL_BACK);
	glPolygonMode (GL_FRONT, GL_LINE);

	glLineWidth (line_width);

	glEnable (GL_LINE_SMOOTH);
	GL_PolygonOffset (-0.7);
	glDisable (GL_TEXTURE_2D);

	if(gl_fogenable.value)//R00k
	{
		glEnable (GL_FOG);
	}

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf (1,"R_DrawAliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += ent->pose1 * paliashdr->poseverts;
	verts2 += ent->pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	while ((count = *order++))
	{		
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;

			glBegin(GL_TRIANGLE_FAN);
		}
		else
		{
			glBegin(GL_TRIANGLE_STRIP);
		}
		do 
		{
			if (gl_interpolate_animation.value == 0)
			{
				glVertex3f (verts1->v[0], verts1->v[1], verts1->v[2]);
			}
			else
			{
				lerpfrac = VectorL2Compare(verts1->v, verts2->v, distance) ? ent->framelerp : 1;
				VectorInterpolate (verts1->v, lerpfrac, verts2->v, interpolated_verts);
				glVertex3fv (interpolated_verts);				
			}
			
			order += 2;
			verts1++;
			verts2++;
		} 
		while (--count);
		
		glEnd();
	}
	glColor4f (1, 1, 1, 1);	
	GL_PolygonOffset (0);
	glPolygonMode (GL_FRONT, GL_FILL);
	glDisable (GL_LINE_SMOOTH);
	glCullFace (GL_FRONT);
	glEnable (GL_TEXTURE_2D);	

	if(gl_fogenable.value)//R00k
	{
		glDisable (GL_FOG);
	}
}

/*
=============
R_DrawAliasFrame
=============
*/
void R_DrawAliasCelFrame (int frame, aliashdr_t *paliashdr, entity_t *ent, int distance)
{
	int			i, *order, count, pose, numposes;
	float		l, lerpfrac;
	vec3_t		lightvec, interpolated_verts;
	trivertx_t	*verts1, *verts2;
	qboolean	lerpmdl = true;

	//cel shading
	extern	GLuint	
				celtexture, 
				vertextexture;

	float		iblend;
	GLfloat		normal[3];
	int			nocelshading;

	nocelshading = ISUNDERWATER(TruePointContents(ent->origin));

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf (1,"R_DrawAliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1) 
	{
		ent->frame_interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / ent->frame_interval) % numposes;
	}
	else
	{
		ent->frame_interval = 0.1;
	}

	//MH
	if (ent->pose2 != pose) 
		lerpmdl = false;			// don't interpolate between different poses

	if (paliashdr->numposes == 1) 
		lerpmdl = false;			// only one pose	

	if (ent != &cl.viewent && r_framecount < 10) 
		lerpmdl = false;			// reliving their dying throes

	if (ent->pose1 == ent->pose2) 
		lerpmdl = false;
	//MH

	if (!lerpmdl)
	{
		ent->frame_start_time = cl.time;
		ent->pose1 = ent->pose2;
		ent->pose2 = pose;
		ent->framelerp = 0;	
	}
	else
	{
		ent->framelerp = (cl.time - ent->frame_start_time) / ent->frame_interval;		
	}

	// weird things start happening if blend passes 1
	if (cl.paused || ent->framelerp > 1) 
		ent->framelerp = 1;

	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += ent->pose1 * paliashdr->poseverts;
	verts2 += ent->pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	if ((r_celshading.value) && (gl_mtexable) && (!nocelshading)) 
	{	
		//QMB: setup for shading
		iblend = 1.0 - (ent->framelerp);
		GL_SelectTexture(GL_TEXTURE1);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glEnable(GL_TEXTURE_1D);

		if (r_celshading.value > 1)		
			glBindTexture (GL_TEXTURE_1D, vertextexture);
		else
			glBindTexture (GL_TEXTURE_1D, celtexture);		
	}

	while ((count = *order++))
	{
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;

			glBegin(GL_TRIANGLE_FAN);
		}
		else
		{
			glBegin(GL_TRIANGLE_STRIP);
		}

		do {
			// normals and vertexes come from the frame list
			// blend the light intensity from the two frames together
			lerpfrac = VectorL2Compare(verts1->v, verts2->v, distance) ? ent->framelerp : 1;
			VectorInterpolate (verts1->v, lerpfrac, verts2->v, interpolated_verts);

			if (gl_vertexlights.value == 1 && !full_light)
			{
				l = R_LerpVertexLight (anorm_pitch[verts1->lightnormalindex], anorm_yaw[verts1->lightnormalindex], anorm_pitch[verts2->lightnormalindex], anorm_yaw[verts2->lightnormalindex], lerpfrac, apitch, ayaw);
				l = min(l, 1);

				for (i=0 ; i<3 ; i++)
					lightvec[i] = lightcolor[i] / 255.0f + l;

				glColor4f (lightvec[0], lightvec[1], lightvec[2], 1);
			}
			else
			{
				l = FloatInterpolate (shadedots[verts1->lightnormalindex], lerpfrac, shadedots[verts2->lightnormalindex]);
				l = (l * shadelight + ambientlight) / 255.0f;
				l = min(l, 1);
				for (i=0 ; i<3 ; i++)
					lightvec[i] = lightcolor[i] / 255.0f + l;

				glColor4f (lightvec[0], lightvec[1], lightvec[2], 1);
			}

			if ((r_celshading.value) && (gl_mtexable) && (!nocelshading)) 
			{				
				normal[0] = (r_avertexnormals[verts1->lightnormalindex][0] * iblend + r_avertexnormals[verts2->lightnormalindex][0] * (ent->framelerp));
				normal[1] = (r_avertexnormals[verts1->lightnormalindex][1] * iblend + r_avertexnormals[verts2->lightnormalindex][1] * (ent->framelerp));
				normal[2] = (r_avertexnormals[verts1->lightnormalindex][2] * iblend + r_avertexnormals[verts2->lightnormalindex][2] * (ent->framelerp));
				
				glNormal3f(normal[0],normal[1],normal[2]);

				// texture coordinates come from the draw list
				qglMultiTexCoord1f(GL_TEXTURE1, bound(0,DotProduct(shadevector,normal),1));
				qglMultiTexCoord2f(GL_TEXTURE0, ((float *)order)[0], ((float *)order)[1]);	
			}
			else
			{			
				if (gl_mtexable)
				{
					qglMultiTexCoord2f (GL_TEXTURE0, ((float *)order)[0], ((float *)order)[1]);
					qglMultiTexCoord2f (GL_TEXTURE1, ((float *)order)[0], ((float *)order)[1]);
				}
				else
				{
					glTexCoord2f (((float *)order)[0], ((float *)order)[1]);
				}
			}

			glVertex3fv (interpolated_verts);				

			order += 2;
			verts1++;
			verts2++;
		} while (--count);
		
		glEnd();
	}

	if ((r_celshading.value) && (gl_mtexable) && (!nocelshading)) 
	{
		glDisable(GL_TEXTURE_1D);		
		GL_SelectTexture(GL_TEXTURE0);
	}
}
void R_DrawAliasSkinFrame  (int frame, aliashdr_t *paliashdr, entity_t *ent, int distance, int color, int texture)
{
	int			*order, count, pose, numposes;
	trivertx_t	*verts1, *verts2;
	float		lerpfrac = 1;
	vec3_t		interpolated_verts;

	float l;

	if ((have_stencil) && (ISTRANSPARENT(ent)))
	{
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_EQUAL,1,2);
		glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);
	}

	GL_PolygonOffset (-1);

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf (1,"R_DrawAliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += ent->pose1 * paliashdr->poseverts;
	verts2 += ent->pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	GL_Bind (texture);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);		
	glEnable (GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	ColorMeBad(color);

	while ((count = *order++))
	{
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
		{
			glBegin (GL_TRIANGLE_STRIP);
		}

		do 
		{
			lerpfrac = VectorL2Compare(verts1->v, verts2->v, distance) ? ent->framelerp : 1;
			
			if ((gl_vertexlights.value == 1 && !full_light)||(gl_vertexlights.value == 2 && ent == &cl.viewent && !full_light))
			{
				l = R_LerpVertexLight (anorm_pitch[verts1->lightnormalindex], anorm_yaw[verts1->lightnormalindex], anorm_pitch[verts2->lightnormalindex], anorm_yaw[verts2->lightnormalindex], lerpfrac, apitch, ayaw);
				l = min(l, 1);
			}
			else
			{
				l = FloatInterpolate (shadedots[verts1->lightnormalindex], lerpfrac, shadedots[verts2->lightnormalindex]);
				l = (l * shadelight + ambientlight) / 255.0f;
				l = min(l, 1);
			}

			glTexCoord2f (((float *)order)[0], ((float *)order)[1]);

			if (gl_interpolate_animation.value == 0)
			{
				glVertex3f (verts2->v[0], verts2->v[1], verts2->v[2]);
			}
			else
			{
				lerpfrac = VectorL2Compare(verts1->v, verts2->v, distance) ? ent->framelerp : 1;
				VectorInterpolate (verts1->v, lerpfrac, verts2->v, interpolated_verts);					
				glVertex3fv (interpolated_verts);
			}
			
			order += 2;
			verts1++;
			verts2++;
		} while (--count);
		 
		glEnd ();
	}	 

	if ((have_stencil) && (ISTRANSPARENT(ent)))
	{		
		glDisable(GL_STENCIL_TEST);
	}

	GL_PolygonOffset (0);
	glDisable (GL_BLEND);	
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	
}

void R_DrawAliasFrame(int frame, aliashdr_t *paliashdr, entity_t *ent, int distance)
{
	int			*order, count, pose, numposes, i;
	trivertx_t	*verts1, *verts2;
	float		l, lerpfrac;
	vec3_t		lightvec, interpolated_verts;


	if ((have_stencil) && (ISTRANSPARENT(ent)))
	{		
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_EQUAL,1,2);
		glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);
		glEnable (GL_BLEND);
	}

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf (1,"R_DrawAliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (gl_interpolate_animation.value)
	{
		if (numposes > 1)
		{
			ent->frame_interval = paliashdr->frames[frame].interval;
			pose += (int)(cl.time / ent->frame_interval) % numposes;
		}
		else
		{
			ent->frame_interval = 0.1;
		}

		if (ent->pose2 != pose)
		{
			ent->frame_start_time = cl.time;
			ent->pose1 = ent->pose2;
			ent->pose2 = pose;
			ent->framelerp = 0;
		}
		else
		{
			ent->framelerp = (cl.time - ent->frame_start_time) / ent->frame_interval;
		}

		if (cl.paused || ent->framelerp > 1)
			ent->framelerp = 1;		
	}
	else	
	{
		ent->frame_interval = 0.1;
		ent->frame_start_time = cl.time;
		ent->pose1 = ent->pose2;
		ent->pose2 = pose;
		ent->framelerp = 0;		
	}

	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += ent->pose1 * paliashdr->poseverts;
	verts2 += ent->pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	while ((count = *order++))
	{
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
		{
			glBegin (GL_TRIANGLE_STRIP);
		}

		do 
		{
			lerpfrac = VectorL2Compare(verts1->v, verts2->v, distance) ? ent->framelerp : 1;
			
			if (gl_vertexlights.value && !full_light)
			{
				l = R_LerpVertexLight (anorm_pitch[verts1->lightnormalindex], anorm_yaw[verts1->lightnormalindex], anorm_pitch[verts2->lightnormalindex], anorm_yaw[verts2->lightnormalindex], lerpfrac, apitch, ayaw);
				l = min(l, 1);

				for (i = 0 ; i < 3 ; i++)
					lightvec[i] = lightcolor[i] / 256 + l;
				glColor4f (lightvec[0], lightvec[1], lightvec[2], ent->transparency);
			}
			else
			{
				l = FloatInterpolate (shadedots[verts1->lightnormalindex], lerpfrac, shadedots[verts2->lightnormalindex]);
				l = (l * shadelight + ambientlight) / 256;
				l = min(l, 1);

				glColor4f (l, l, l, ent->transparency);
			}
			
			if (gl_mtexable)
			{
				qglMultiTexCoord2f (GL_TEXTURE0,((float *)order)[0], ((float *)order)[1]);
				qglMultiTexCoord2f (GL_TEXTURE1,((float *)order)[0], ((float *)order)[1]);//Used for gl_fb_models
			}
			else
			{			
				glTexCoord2f (((float *)order)[0], ((float *)order)[1]);
			}

			order += 2;

			if (gl_interpolate_animation.value == 0)
			{
				glVertex3f (verts2->v[0], verts2->v[1], verts2->v[2]);
			}
			else
			{
				VectorInterpolate (verts1->v, lerpfrac, verts2->v, interpolated_verts);					
				glVertex3fv (interpolated_verts);
				verts1++;
			}			
			verts2++;
		} while (--count);
		glEnd ();
	}

	if ((have_stencil) && (ISTRANSPARENT(ent)))
	{		
		glDisable(GL_STENCIL_TEST);
		glDisable (GL_BLEND);	
	}

}
/*
=============
R_DrawAliasShadow
=============
*/
void R_DrawAliasShadow (aliashdr_t *paliashdr, entity_t *ent, int distance, trace_t downtrace)
{
	int			*order, count;
	float		lheight, lerpfrac, s1, c1;
	vec3_t		point1, point2, interpolated;

	trivertx_t	*verts1, *verts2;
	
	lheight = ent->origin[2] - lightspot[2];

	s1 = sin(ent->angles[1] / 180 * M_PI);
	c1 = cos(ent->angles[1] / 180 * M_PI);

	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += ent->pose1 * paliashdr->poseverts;
	verts2 += ent->pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	if (have_stencil) 
	{
		glStencilFunc(GL_EQUAL,1,2);
		glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);
		glEnable(GL_STENCIL_TEST);
	}

	while ((count = *order++))
	{
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
		{
			glBegin (GL_TRIANGLE_STRIP);
		}

		do {
			order += 2;

			lerpfrac = VectorL2Compare(verts1->v, verts2->v, distance) ? ent->framelerp : 1;

			point1[0] = verts1->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point1[1] = verts1->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point1[2] = verts1->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point1[0] -= shadevector[0] * (point1[2] + lheight);
			point1[1] -= shadevector[1] * (point1[2] + lheight);

			point2[0] = verts2->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point2[1] = verts2->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point2[2] = verts2->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point2[0] -= shadevector[0] * (point2[2] + lheight);
			point2[1] -= shadevector[1] * (point2[2] + lheight);

			VectorInterpolate (point1, lerpfrac, point2, interpolated);

			interpolated[2] = -(ent->origin[2] - downtrace.endpos[2]);

			interpolated[2] += ((interpolated[1] * (s1 * downtrace.plane.normal[0])) - 
						(interpolated[0] * (c1 * downtrace.plane.normal[0])) - 
						(interpolated[0] * (s1 * downtrace.plane.normal[1])) - 
						(interpolated[1] * (c1 * downtrace.plane.normal[1]))) + 
						((1 - downtrace.plane.normal[2]) * 20) + 0.2;

			glVertex3fv (interpolated);
			
			verts1++;
			verts2++;
		} while (--count);
		 
		glEnd ();
	}       

	 

	if (have_stencil)
	{		
		glDisable(GL_STENCIL_TEST);
		//glClearStencil(1);
		//glClear(GL_STENCIL_BUFFER_BIT);
	}
}

/*
=============
R_SetupLighting
=============
*/
void R_SetupLighting (entity_t *ent)
{
	int		i, lnum;
	float	add, radiusmax = 0.0;

	vec3_t	dist, dlight_color;
	model_t	*clmodel = ent->model;

	extern corona_t *R_AddDefaultCorona (entity_t *e);	
	extern 	cvar_t	r_glowlg;

	if (clmodel->flags & EF_FULLBRIGHT)
	{
		ambientlight = 255;
		shadelight = 0;
		full_light = ent->noshadow = true;
	}

	// make thunderbolt and torches full light
	if (clmodel->modhint == MOD_THUNDERBOLT)
	{
		ent->transparency = bound(0.4, gl_lightning_alpha.value,1);
		
		ambientlight = 255;
		shadelight = 0;
		full_light = ent->noshadow = true;
		return;
	}

	if (clmodel->modhint == MOD_FLAME)
	{
		ambientlight = 255;
		shadelight = 0;
		full_light = ent->noshadow = true;
		if(!ent->corona)
		{
			corona_t *c;
			if((c = (void*)R_AddDefaultCorona(currententity)))
			{
				c->offset[2] = 6;
				if(!strcmp (clmodel->name, "progs/flame2.mdl"))
					c->offset[2] -= 2;
			}
		}
		return;
	}

	
	// normal lighting
	ambientlight = shadelight = R_LightPoint (ent->origin);
	full_light = false;
	ent->noshadow = false;

	// R00k: dont muddle through the dlights if r_dynamic is off!
	if (r_dynamic.value)
	{
		for (lnum = 0 ; lnum < MAX_DLIGHTS ; lnum++)
		{
			if (cl_dlights[lnum].die < cl.time || !cl_dlights[lnum].radius)
				continue;

			VectorSubtract (ent->origin, cl_dlights[lnum].origin, dist);
			add = cl_dlights[lnum].radius - VectorLength (dist);

			if (add > 0)
			{
				ambientlight += add;
				shadelight += add;
				VectorCopy (bubblecolor[cl_dlights[lnum].type], dlight_color);
				for (i=0 ; i<3 ; i++)
				{
					lightcolor[i] = lightcolor[i] + (dlight_color[i] * add) * 2;
					if (lightcolor[i] > 256)
					{
						switch (i)
						{
						case 0:
							lightcolor[1] = lightcolor[1] - (lightcolor[1]/3); 
							lightcolor[2] = lightcolor[2] - (lightcolor[2]/3); 
							break;

						case 1:
							lightcolor[0] = lightcolor[0] - (lightcolor[0]/3); 
							lightcolor[2] = lightcolor[2] - (lightcolor[2]/3); 
							break;

						case 2:
							lightcolor[1] = lightcolor[1] - (lightcolor[1]/3); 
							lightcolor[0] = lightcolor[0] - (lightcolor[0]/3); 
							break;
						}
					}
				}
			}
		}
	}

	// calculate pitch and yaw for vertex lighting
	if (gl_vertexlights.value)
	{
		apitch = -ent->angles[0];//R00k Quake has this backwards...
		ayaw = ent->angles[1];
	}

	// clamp lighting so it doesn't overbright as much

	ambientlight = min(128, ambientlight);

	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	if (ent == &cl.viewent)
	{
		ent->noshadow = true;
		// always give the gun some light
		if (ambientlight < 8)
			ambientlight = shadelight = 8;//R00k: reduced to 8, equal to the player model.
	}
	else
	{
		if (ambientlight < 8)	//R00k: nothing should be so dark you cant see; it's a waste rendering it for nothing...
			ambientlight = shadelight = 8;
	}
}

/*
=============
R_SetupInterpolateDistance
=============
*/
void R_SetupInterpolateDistance (entity_t *ent, int *distance)
{
	*distance = INTERP_MAXDIST;

	if (ent->model->modhint == MOD_FLAME)
		*distance = 0;

	else if (ent->model->modhint == MOD_WEAPON)
	{
		if ((*distance = DoWeaponInterpolation()) == -1)
			*distance = (int)gl_interdist.value;
	}
	else if ((ent->modelindex == cl_modelindex[mi_player])||(ent->modelindex == cl_modelindex[mi_md3_player]))
	{
		// nailatt
		if (ent->frame == 103)
			*distance = 0;
		else if (ent->frame == 104)
			*distance = 59;
		// rockatt
		else if (ent->frame == 107)
			*distance = 0;
		else if (ent->frame == 108)
			*distance = 115;
		// shotatt
		else if (ent->frame == 113)
			*distance = 76;
		else if (ent->frame == 115)
			*distance = 79;
	}
	else if (ent->modelindex == cl_modelindex[mi_soldier])
	{
		if (ent->frame == 84)
			*distance = 63;
		else if (ent->frame == 85)
			*distance = 49;
		else if (ent->frame == 86)
			*distance = 106;
	}
	else if (ent->modelindex == cl_modelindex[mi_enforcer])
	{
		if (ent->frame == 36)
			*distance = 115;
		else if (ent->frame == 37)
			*distance = 125;
	}
}

extern	corona_t	*R_AddDefaultCorona (entity_t *e);
extern	cvar_t		r_powerupglow;

/*
=================
R_DrawAliasModel
=================
*/
void R_DrawAliasModel (entity_t *ent)
{	
	int				i, anim, skinnum, distance, texture, fb_texture, st_texture, pt_texture;
	vec3_t			mins, maxs;
	aliashdr_t		*paliashdr;
	model_t			*clmodel = ent->model;
	qboolean		isLumaSkin;
	float			scale, color[3], theta;
	extern	int		fb_skins[16];

	if (ent->cullFrame != r_framecount)
	{
		VectorAdd (ent->origin, clmodel->mins, mins);
		VectorAdd (ent->origin, clmodel->maxs, maxs);

		if (ent->angles[0] || ent->angles[1] || ent->angles[2])
			ent->culled = R_CullSphere(ent->origin, clmodel->radius);
		else
			ent->culled = R_CullBox(mins, maxs);

		ent->cullFrame = r_framecount;
	}
#ifndef USEFAKEGL
	//GLQuake stencil shadows : Rich Whitehouse
	//Since we are using stencil shadows and they tend to stretch
	//outside of the viewable area of the entity itself, we want
	//to draw them regardless of whether we pass the cull check.
	if (r_shadows.value >= 2)
	{ //Add this guy to our list.
		if (g_numStencilEnts < MAX_STENCIL_ENTS)
		{
			g_stencilEnts[g_numStencilEnts] = currententity;
			g_numStencilEnts++;
		}
	}
#endif
	if (ent->culled)
	{
		return;
	}

	if (ent->transparency <= 0)
		return;

	color[0] = 0.4;
	color[1] = 0.4;
	color[2] = 0.4;

	if ((r_shadows.value)||(r_celshading.value))
	{
		if (!shadescale)
			shadescale = 1 / sqrt(2);

		theta = -ent->angles[1] / 180 * M_PI;

		VectorSet (shadevector, cos(theta) * shadescale, sin(theta) * shadescale, shadescale);
	}

	VectorCopy (ent->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// get lighting information
	R_SetupLighting (ent);
	R_SetupInterpolateDistance (ent, &distance);

	shadedots = r_avertexnormal_dots[((int)(ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	// locate the proper data
	paliashdr = (aliashdr_t *)Mod_Extradata (clmodel);

	c_alias_polys += paliashdr->numtris;

	// draw all the triangles

	glPushMatrix ();	
	
	if ((ent == &cl.viewent) || (clmodel->modhint == MOD_FLAME) || (gl_interpolate_transform.value == 0) || (!sv.active))
	{
		R_RotateForViewEntity (ent);
	}
	else
	{
		R_RotateForEntity (ent, false);
	}

	if (clmodel->modhint == MOD_EYES && gl_doubleeyes.value)
	{
		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		glScalef (paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	}
	else
	{		
		if ((ent == &cl.viewent) && (cl_gun_fovscale.value) && (scr_fov.value != 0))
		{			
			if (scr_fov.value <= 90)
				scale = 1.0f;
			else
				scale = 1.0f / tan(DEG2RAD(scr_fov.value/2));//Phoenix
			glTranslatef(paliashdr->scale_origin[0]*scale, paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
			glScalef(paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);			
		}
		else
		{
			if ((ent == &cl.viewent) && (r_drawviewmodelsize.value > 0))
			{				
				scale = 0.5 + bound(0, r_drawviewmodelsize.value, 1) / 2;
				glTranslatef(paliashdr->scale_origin[0]*scale, paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
				glScalef(paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
			}
			else
			{		
				glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
				glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);		
			}
		}
	}

	anim = (lrint(cl.time)*10) & 3;
	skinnum = ent->skinnum;

	if ((skinnum >= paliashdr->numskins) || (skinnum < 0))
	{
		Con_DPrintf (1,"R_DrawAliasModel: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	texture	= paliashdr->gl_texturenum[skinnum][anim];

	//R00k: added 24-bit player skins!
	if (clmodel->modhint == MOD_PLAYER && cl_teamskin.value)
	{		
		st_texture = paliashdr->gl_textureshirt[skinnum][anim];
		pt_texture = paliashdr->gl_texturepants[skinnum][anim];
	}
	else
	{
		if (gl_fb_models.value)
		{
			isLumaSkin	= paliashdr->isLumaSkin[skinnum][anim];
			fb_texture	= paliashdr->fb_texturenum[skinnum][anim];
		}
	}

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players. Heads are just uncolored.
	if (ent->colormap && !gl_nocolors.value && !cl_teamskin.value && (ent->model->modhint == MOD_PLAYER))
	{
		i = ent - cl_entities;

		if (i > 0 && i <= cl.maxclients)
		{
			texture = playertextures + (i - 1);
			fb_texture = fb_skins[i-1];
		}
		else
		{
			if (gl_color_deadbodies.value)
			{
				if (ent->colormap > 0)
				{				
					{
						i = (ent->colormap - 1);
						CLAMP(0, i, cl.maxclients);
						texture = (playertextures) + i;//Colored Dead Bodies
					}
//					else
//					texture	= paliashdr->gl_texturenum[skinnum][anim];					
					fb_texture = fb_skins[(ent->colormap - 1)];
				}
			}
		}
	}

	if (full_light || !gl_fb_models.value || r_celshading.value)
		fb_texture = 0;

	if (gl_fogenable.value)//R00k
	{
		glEnable (GL_FOG);
	}

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	if (gl_affinemodels.value == 1)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	
	GL_DisableMultitexture ();
	GL_Bind (texture);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	//*********************************************************************************************************************
	if ((cl_teamskin.value) && (clmodel->modhint == MOD_PLAYER) && (pt_texture) && (st_texture))
	{					
		if (r_celshading.value)//UGH!
			R_DrawAliasCelFrame (ent->frame, paliashdr, ent, distance);
		else
			R_DrawAliasFrame (ent->frame, paliashdr, ent, distance);

		if (r_outline.value)
		{	
			//fix dont outline on flags?
			{
				TeamOutlinColor(ent->pantscolor);			
				R_DrawAliasOutlineFrame (ent->frame, paliashdr, ent, distance);
			}
		}

		if (r_celshading.value)
		{
			R_DrawAliasCelSkinFrame (ent->frame, paliashdr, ent, distance, ent->pantscolor, pt_texture);			
			R_DrawAliasCelSkinFrame (ent->frame, paliashdr, ent, distance, ent->shirtcolor, st_texture);
		}
		else
		{
			R_DrawAliasSkinFrame (ent->frame, paliashdr, ent, distance, ent->pantscolor, pt_texture);
			R_DrawAliasSkinFrame (ent->frame, paliashdr, ent, distance, ent->shirtcolor, st_texture);
		}
	}
//*********************************************************************************************************************
	else
	{
		if (fb_texture && gl_mtexable)
		{
			GL_EnableMultitexture ();

			if (isLumaSkin)
			{
				if (gl_add_ext)
				{
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
					GL_Bind (fb_texture);
				}
			}
			else
			{
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
				GL_Bind (fb_texture);
			}

			if ((r_celshading.value) && (!(ent->model->flags & EF_FULLBRIGHT)))
				R_DrawAliasCelFrame (ent->frame, paliashdr, ent, distance);
			else
				R_DrawAliasFrame (ent->frame, paliashdr, ent, distance);

			if (isLumaSkin && !gl_add_ext)
			{
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
				GL_Bind (fb_texture);

				glDepthMask (GL_FALSE);
				glEnable (GL_BLEND);
				glBlendFunc (GL_ONE, GL_ONE);

				R_DrawAliasFrame (ent->frame, paliashdr, ent, distance);

				glDisable (GL_BLEND);
				glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glDepthMask (GL_TRUE);
			}
			GL_DisableMultitexture ();
		}
		else
		{
			if ((r_celshading.value) && (!(ent->model->flags & EF_FULLBRIGHT)))
				R_DrawAliasCelFrame (ent->frame, paliashdr, ent, distance);
			else
				R_DrawAliasFrame (ent->frame, paliashdr, ent, distance);

			if (fb_texture)
			{
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				GL_Bind (fb_texture);

				glDepthMask (GL_FALSE);
				if (isLumaSkin)
				{
					glEnable (GL_BLEND);
					glBlendFunc (GL_ONE, GL_ONE);
				}
				else
				{
					glEnable (GL_ALPHA_TEST);
				}

				R_DrawAliasFrame (ent->frame, paliashdr, ent, distance);

				if (isLumaSkin)
				{
					glDisable (GL_BLEND);
					glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				}
				else
				{
					glDisable (GL_ALPHA_TEST);
				}			
				glDepthMask (GL_TRUE);//R00k:D3D requires this here!(12/30/2014)
			}			
		}

		if (r_outline.value)
		{					
			if (clmodel->modhint == MOD_PLAYER)
			{
				TeamOutlinColor(ent->pantscolor);
			}
			else
			{			
				glColor4f (0, 0, 0, 1);	
			}
			R_DrawAliasOutlineFrame (ent->frame, paliashdr, ent, distance);
			glColor4f (1, 1, 1, 1);				
		}
	}
	
	//R00k underwater caustics on alias models by MH
	if (gl_caustics.value)
	if (!((cl_teamskin.value) && (clmodel->modhint == MOD_PLAYER)))//teamskins break caustics, skip this pass...
	if ((underwatertexture && gl_mtexable && ISUNDERWATER(TruePointContents(ent->origin))))
	{
		if (!(r_celshading.value))
		{			
			GL_EnableMultitexture ();

			GL_Bind (underwatertexture);

			glMatrixMode (GL_TEXTURE);
			glLoadIdentity ();
			glScalef (0.5, 0.5, 1);
			glRotatef (cl.time * 10, 1, 0, 0);
			glRotatef (cl.time * 10, 0, 1, 0);
			glMatrixMode (GL_MODELVIEW);

			GL_Bind (underwatertexture);

			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);		
			
			glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			glEnable (GL_BLEND);

			R_DrawAliasFrame (ent->frame, paliashdr, ent, distance);

			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_BLEND);			

			GL_SelectTexture(GL_TEXTURE1);
			glTexEnvi (GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
			glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glDisable (GL_TEXTURE_2D);

			glMatrixMode (GL_TEXTURE);
			glLoadIdentity ();
			glMatrixMode (GL_MODELVIEW);
			
			GL_DisableMultitexture ();
		}
	}

	if ((ent==&cl_entities[cl.viewentity]||ent == &cl.viewent)&&(cl.time <= cl.faceanimtime) && (gl_damageshells.value))//If I am hit, Glow deflection shield...
	{
		color[0] = 1.0;color[1] = 0.50;color[2] = 0.0;
		R_DrawAliasShellFrame (ent->frame, paliashdr, ent, distance, color, 0.65);
	}
	else
	if (gl_powerupshells.value)
	{
		 if ((ent->effects & EF_DIMLIGHT) || ((ent->effects & EF_FROZEN)) || ((ent == &cl.viewent)&&(cl.items & IT_QUAD)) || ((ent == &cl.viewent)&&(cl.items & IT_INVULNERABILITY)))
		 {
			if (ent != &cl.viewent)
			{
				if (r_powerupglow.value == 0)
				{
					color[0] = 0.4;
					color[1] = 0.4;
					color[2] = 0.4;
				}
				else				
				{
					if (ent->effects & EF_RED)
					{
						color[0] = 0.50;color[1] = 0.0;color[2] = 0.0;
					}
					else
					{
						if (ent->effects & EF_BLUE)
						{
							color[0] = 0.0;color[1] = 0.0;color[2] = 0.50;
						}
						else
						{
							if (ent->effects & EF_FROZEN)
							{
								color[0] = 0.25;color[1] = 0.25;color[2] = 0.50;
							}
						}
					}
				}				
			}
			else
			{
				if (r_powerupglow.value == 0)
				{
					color[0] = 0.4;
					color[1] = 0.4;
					color[2] = 0.4;
				}
				else				
				{
					if ((ent == &cl.viewent)&&(cl.items & IT_QUAD))
					{
							color[0] = 0;color[1] = 0;color[2] = 0.5;
					}
					
					if ((ent == &cl.viewent)&&(cl.items & IT_INVULNERABILITY))
					{
							color[0] = 0.5;color[1] = 0;color[2] = 0;
					}
				}
			}
			R_DrawAliasShellFrame (ent->frame, paliashdr, ent, distance,color, 0.65);
		 }
	}

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);//reset

	glShadeModel (GL_FLAT);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix ();	

	if ((r_shadows.value && r_shadows.value < 2) && ent->noshadow == false) 
	{
		int				farclip;
		vec3_t			downmove;
		trace_t			downtrace;
		
		farclip = max((int)r_farclip.value, 4096);

		if (TruePointContents(ent->origin) == CONTENTS_SOLID)// R00k: if entity is off the map or in a wall, no shadows. This happens with the ctf flag.
		{
		}
		else
		{
			glPushMatrix ();

			R_RotateForEntity (ent, true);

			VectorCopy (ent->origin, downmove);
			downmove[2] -= farclip;
			memset (&downtrace, 0, sizeof(downtrace));
			downtrace.fraction = 1;
			SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, ent->origin, downmove, &downtrace);

			if (downtrace.fraction < 1)//R00k
			{
				glDepthMask (GL_FALSE);
				glDisable (GL_TEXTURE_2D);
				glEnable (GL_BLEND);
				glColor4f (0, 0, 0,(((shadelight - (mins[2] - downtrace.endpos[2]))*r_shadows.value)*0.0066 ) * ent->transparency);

				R_DrawAliasShadow (paliashdr, ent, distance, downtrace);

				glDepthMask (GL_TRUE);
				glEnable (GL_TEXTURE_2D);
				glDisable (GL_BLEND);
			}
			glPopMatrix ();
		}
	}

	glColor4f(1,1,1,1);
}

void Set_Interpolated_Weapon_f (void)
{
	int	i;
	char	str[MAX_QPATH];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() == 2)
	{
		for (i=0 ; i<interp_weap_num ; i++)
		{
			if (!Q_strcasecmp(Cmd_Argv(1), interpolated_weapons[i].name))
			{
				Con_Printf ("%s`s distance is %d\n", Cmd_Argv(1), interpolated_weapons[i].maxDistance);
				return;
			}
		}
		Con_Printf ("%s`s distance is default (%d)\n", Cmd_Argv(1), (int)gl_interdist.value);
		return;
	}

	if (Cmd_Argc() != 3)
	{
		Con_Printf ("Usage: set_interpolated_weapon <model> <distance>\n");
		return;
	}

	strcpy (str, Cmd_Argv(1));
	for (i=0 ; i<interp_weap_num ; i++)
		if (!Q_strcasecmp(str, interpolated_weapons[i].name))
			break;
	if (i == interp_weap_num)
	{
		if (interp_weap_num == INTERP_WEAP_MAXNUM)
		{
			Con_Printf ("interp_weap_num == INTERP_WEAP_MAXNUM\n");
			return;
		}
		else
		{
			interp_weap_num++;
		}
	}

	strcpy (interpolated_weapons[i].name, str);
	interpolated_weapons[i].maxDistance = (int)Q_atof(Cmd_Argv(2));
}

int DoWeaponInterpolation (void)
{
	int	i;

	if (currententity != &cl.viewent)
		return -1;

	for (i=0 ; i<interp_weap_num ; i++)
	{
		if (!interpolated_weapons[i].name[0])
			return -1;

		if (!Q_strcasecmp(currententity->model->name, va("%s.mdl", interpolated_weapons[i].name)) || 
		    !Q_strcasecmp(currententity->model->name, va("progs/%s.mdl", interpolated_weapons[i].name)))
			return interpolated_weapons[i].maxDistance;
	}

	return -1;
}
/*
===============================================================================

				Q3 MODELS

===============================================================================
*/

void R_RotateForTagEntity (tagentity_t *tagent, md3tag_t *tag, float *m)
{
	int	i;
	float	lerpfrac, timepassed;

	// positional interpolation
	timepassed = cl.time - tagent->tag_translate_start_time;

	if (tagent->tag_translate_start_time == 0 || timepassed > 1)
	{
		tagent->tag_translate_start_time = cl.time;
		VectorCopy (tag->pos, tagent->tag_pos1);
		VectorCopy (tag->pos, tagent->tag_pos2);
	}

	if (!VectorCompare(tag->pos, tagent->tag_pos2))
	{
		tagent->tag_translate_start_time = cl.time;
		VectorCopy (tagent->tag_pos2, tagent->tag_pos1);
		VectorCopy (tag->pos, tagent->tag_pos2);
		lerpfrac = 0;
	}
	else
	{
		lerpfrac = timepassed / 0.1;
		if (cl.paused || lerpfrac > 1)
			lerpfrac = 1;
	}

	VectorInterpolate (tagent->tag_pos1, lerpfrac, tagent->tag_pos2, m + 12);
	m[15] = 1;

	for (i=0 ; i<3 ; i++)
	{
		// orientation interpolation (Euler angles, yuck!)
		timepassed = cl.time - tagent->tag_rotate_start_time[i];

		if (tagent->tag_rotate_start_time[i] == 0 || timepassed > 1)
		{
			tagent->tag_rotate_start_time[i] = cl.time;
			VectorCopy (tag->rot[i], tagent->tag_rot1[i]);
			VectorCopy (tag->rot[i], tagent->tag_rot2[i]);
		}

		if (!VectorCompare(tag->rot[i], tagent->tag_rot2[i]))
		{
			tagent->tag_rotate_start_time[i] = cl.time;
			VectorCopy (tagent->tag_rot2[i], tagent->tag_rot1[i]);
			VectorCopy (tag->rot[i], tagent->tag_rot2[i]);
			lerpfrac = 0;
		}
		else
		{
			lerpfrac = timepassed / 0.1;
			if (cl.paused || lerpfrac > 1)
				lerpfrac = 1;
		}

		VectorInterpolate (tagent->tag_rot1[i], lerpfrac, tagent->tag_rot2[i], m + i*4);
		m[i*4+3] = 0;
	}
}

int		bodyframe = 0, legsframe = 0;
animtype_t	bodyanim, legsanim;

void R_ReplaceQ3Frame (int frame)
{
	animdata_t		*currbodyanim, *currlegsanim;
	static	animtype_t	oldbodyanim, oldlegsanim;
	static	float		bodyanimtime, legsanimtime;
	static	qboolean	deathanim = false;

	if (deathanim)
	{
		bodyanim = oldbodyanim;
		legsanim = oldlegsanim;
	}

	if (frame < 41 || frame > 102)
		deathanim = false;

	if (frame >= 0 && frame <= 5)		// axrun
	{
		bodyanim = torso_stand2;
		legsanim = legs_run;
	}
	else if (frame >= 6 && frame <= 11)	// rockrun
	{
		bodyanim = torso_stand;
		legsanim = legs_run;
	}
	else if ((frame >= 12 && frame <= 16) || (frame >= 35 && frame <= 40))	// stand, pain
	{
		bodyanim = torso_stand;
		legsanim = legs_idle;
	}
	else if ((frame >= 17 && frame <= 28) || (frame >= 29 && frame <= 34))	// axstand, axpain
	{
		bodyanim = torso_stand2;
		legsanim = legs_idle;
	}
	else if (frame >= 41 && frame <= 102 && !deathanim)	// axdeath, deatha, b, c, d, e
	{
		bodyanim = legsanim = both_death1;
		deathanim = true;
	}
	else if (frame > 103 && frame <= 118)	// gun attacks
	{
		bodyanim = torso_attack;
	}
	else if (frame >= 119)			// axe attacks
	{
		bodyanim = torso_attack2;
	}

	currbodyanim = &anims[bodyanim];
	currlegsanim = &anims[legsanim];

	if (bodyanim == oldbodyanim)
	{
		if (cl.time >= bodyanimtime + currbodyanim->interval)
		{
			if (currbodyanim->loop_frames && bodyframe + 1 >= currbodyanim->offset + currbodyanim->loop_frames)
				bodyframe = currbodyanim->offset;
			else if (bodyframe + 1 < currbodyanim->offset + currbodyanim->num_frames)
				bodyframe++;
			bodyanimtime = cl.time;
		}
	}
	else
	{
		bodyframe = currbodyanim->offset;
		bodyanimtime = cl.time;
	}

	if (legsanim == oldlegsanim)
	{
		if (cl.time >= legsanimtime + currlegsanim->interval)
		{
			if (currlegsanim->loop_frames && legsframe + 1 >= currlegsanim->offset + currlegsanim->loop_frames)
				legsframe = currlegsanim->offset;
			else if (legsframe + 1 < currlegsanim->offset + currlegsanim->num_frames)
				legsframe++;
			legsanimtime = cl.time;
		}
	}
	else
	{
		legsframe = currlegsanim->offset;
		legsanimtime = cl.time;
	}

	oldbodyanim = bodyanim;
	oldlegsanim = legsanim;
}

int		multimodel_level;
qboolean	surface_transparent;


void R_DrawQ3SkinFrame (int frame, md3header_t *pmd3hdr, md3surface_t *pmd3surf, entity_t *ent, int distance, int color, int texture)
{
	int		i,numtris, pose, pose1, pose2;
	float		lerpfrac;
	vec3_t		interpolated_verts;
	unsigned int	*tris;
	md3tc_t		*tc;
	md3vert_mem_t	*verts, *v1, *v2;
	model_t		*clmodel = ent->model;

	GL_PolygonOffset (-3);

	if ((frame >= pmd3hdr->numframes) || (frame < 0))
	{
		Con_DPrintf (1,"R_DrawQ3Frame: no such frame %d\n", frame);
		frame = 0;
	}

	if (ent->pose1 >= pmd3hdr->numframes)
		ent->pose1 = 0;

	pose = frame;

	if (!strcmp(clmodel->name, "progs/player/lower.md3"))
		ent->frame_interval = anims[legsanim].interval;
	else if (!strcmp(clmodel->name, "progs/player/upper.md3"))
		ent->frame_interval = anims[bodyanim].interval;
	else
		ent->frame_interval = 0.1;

	if (ent->pose2 != pose)
	{
		ent->frame_start_time = cl.time;
		ent->pose1 = ent->pose2;
		ent->pose2 = pose;
		ent->framelerp = 0;
	}
	else
	{
		ent->framelerp = (cl.time - ent->frame_start_time) / ent->frame_interval;
	}

	// weird things start happening if blend passes 1
	if (cl.paused || ent->framelerp > 1)
		ent->framelerp = 1;

	verts = (md3vert_mem_t *)((byte *)pmd3hdr + pmd3surf->ofsverts);
	tc = (md3tc_t *)((byte *)pmd3surf + pmd3surf->ofstc);
	tris = (unsigned int *)((byte *)pmd3surf + pmd3surf->ofstris);
	numtris = pmd3surf->numtris * 3;
	pose1 = ent->pose1 * pmd3surf->numverts;
	pose2 = ent->pose2 * pmd3surf->numverts;

	if (surface_transparent)
	{
		glDepthMask (GL_FALSE);
		glDisable (GL_CULL_FACE);
	}

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);		
	glEnable (GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	GL_Bind(texture);	
	ColorMeBad(color);	

	glBegin (GL_TRIANGLES);
	for (i=0 ; i<numtris ; i++)
	{
		float	s, t;

		v1 = verts + *tris + pose1;
		v2 = verts + *tris + pose2;

		s = tc[*tris].s, t = tc[*tris].t;

		if (gl_mtexable)
		{
			qglMultiTexCoord2f (GL_TEXTURE0, s, t);
			qglMultiTexCoord2f (GL_TEXTURE1, s, t);
		}
		else
		{
			glTexCoord2f (s, t);
		}

		lerpfrac = VectorL2Compare(v1->vec, v2->vec, distance) ? ent->framelerp : 1;

		VectorInterpolate (v1->vec, lerpfrac, v2->vec, interpolated_verts);
		glVertex3fv (interpolated_verts);

		*tris++;
	}
	glEnd ();

	if (surface_transparent)
	{
		glDepthMask (GL_TRUE);
		glEnable (GL_CULL_FACE);
	}

	GL_PolygonOffset (0);
	glDisable (GL_BLEND);	
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

}

void R_DrawQ3OutlineFrame (int frame, md3header_t *pmd3hdr, md3surface_t *pmd3surf, entity_t *ent, int distance)
{
	int				i, numtris, pose, pose1, pose2;
	float			lerpfrac;
	vec3_t			interpolated_verts;
	unsigned int	*tris;
	md3tc_t			*tc;
	md3vert_mem_t	*verts, *v1, *v2;
	model_t			*clmodel = ent->model;

	if (ISTRANSPARENT(ent))
		return;

	glCullFace (GL_BACK);
	glPolygonMode (GL_FRONT, GL_LINE);

	glLineWidth (2.0f);

	glEnable (GL_LINE_SMOOTH);
	GL_PolygonOffset (-0.7);
	glDisable (GL_TEXTURE_2D);

	if ((frame >= pmd3hdr->numframes) || (frame < 0))
	{
		Con_DPrintf (1,"R_DrawQ3Frame: no such frame %d\n", frame);
		frame = 0;
	}

	if (ent->pose1 >= pmd3hdr->numframes)
		ent->pose1 = 0;

	pose = frame;

	if (!strcmp(clmodel->name, "progs/player/lower.md3"))
		ent->frame_interval = anims[legsanim].interval;
	else if (!strcmp(clmodel->name, "progs/player/upper.md3"))
		ent->frame_interval = anims[bodyanim].interval;
	else
		ent->frame_interval = 0.1;

	if (ent->pose2 != pose)
	{
		ent->frame_start_time = cl.time;
		ent->pose1 = ent->pose2;
		ent->pose2 = pose;
		ent->framelerp = 0;
	}
	else
	{
		ent->framelerp = (cl.time - ent->frame_start_time) / ent->frame_interval;
	}

	// weird things start happening if blend passes 1
	if (cl.paused || ent->framelerp > 1)
		ent->framelerp = 1;

	verts = (md3vert_mem_t *)((byte *)pmd3hdr + pmd3surf->ofsverts);
	tc = (md3tc_t *)((byte *)pmd3surf + pmd3surf->ofstc);
	tris = (unsigned int *)((byte *)pmd3surf + pmd3surf->ofstris);
	numtris = pmd3surf->numtris * 3;
	pose1 = ent->pose1 * pmd3surf->numverts;
	pose2 = ent->pose2 * pmd3surf->numverts;

	glBegin (GL_TRIANGLES);
	for (i=0 ; i<numtris ; i++)
	{
		v1 = verts + *tris + pose1;
		v2 = verts + *tris + pose2;

		lerpfrac = VectorL2Compare(v1->vec, v2->vec, distance) ? ent->framelerp : 1;

		glColor4f (0, 0, 0, ent->transparency);

		VectorInterpolate (v1->vec, lerpfrac, v2->vec, interpolated_verts);
		glVertex3fv (interpolated_verts);

		*tris++;
	}
	glEnd ();

	GL_PolygonOffset (0);
	glPolygonMode (GL_FRONT, GL_FILL);
	glDisable (GL_LINE_SMOOTH);
	glCullFace (GL_FRONT);
	glEnable (GL_TEXTURE_2D);
}

/*
=================
R_DrawQ3Frame
=================
*/
void R_DrawQ3Frame (int frame, md3header_t *pmd3hdr, md3surface_t *pmd3surf, entity_t *ent, int distance)
{
	int				i, j, numtris, pose, pose1, pose2;
	float			l, lerpfrac;
	vec3_t			lightvec, interpolated_verts;
	unsigned int	*tris;
	md3tc_t			*tc;
	md3vert_mem_t	*verts, *v1, *v2;
	model_t			*clmodel = ent->model;

	if ((frame >= pmd3hdr->numframes) || (frame < 0))
	{
		Con_DPrintf (1,"R_DrawQ3Frame: no such frame %d\n", frame);
		frame = 0;
	}

	if (ent->pose1 >= pmd3hdr->numframes)
		ent->pose1 = 0;

	pose = frame;

	if (!strcmp(clmodel->name, "progs/player/lower.md3"))
		ent->frame_interval = anims[legsanim].interval;
	else if (!strcmp(clmodel->name, "progs/player/upper.md3"))
		ent->frame_interval = anims[bodyanim].interval;
	else
		ent->frame_interval = 0.1;

	if (ent->pose2 != pose)
	{
		ent->frame_start_time = cl.time;
		ent->pose1 = ent->pose2;
		ent->pose2 = pose;
		ent->framelerp = 0;
	}
	else
	{
		ent->framelerp = (cl.time - ent->frame_start_time) / ent->frame_interval;
	}

	// weird things start happening if blend passes 1
	if (cl.paused || ent->framelerp > 1)
		ent->framelerp = 1;

	verts = (md3vert_mem_t *)((byte *)pmd3hdr + pmd3surf->ofsverts);
	tc = (md3tc_t *)((byte *)pmd3surf + pmd3surf->ofstc);
	tris = (unsigned int *)((byte *)pmd3surf + pmd3surf->ofstris);
	numtris = pmd3surf->numtris * 3;
	pose1 = ent->pose1 * pmd3surf->numverts;
	pose2 = ent->pose2 * pmd3surf->numverts;

	if (surface_transparent)
	{
		glEnable (GL_BLEND);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
	}
	else 
	if (ISTRANSPARENT(ent))
		glEnable (GL_BLEND);

	glBegin (GL_TRIANGLES);
	for (i=0 ; i<numtris ; i++)
	{
		float	s, t;

		v1 = verts + *tris + pose1;
		v2 = verts + *tris + pose2;

		s = tc[*tris].s, t = tc[*tris].t;


		if (gl_mtexable)
		{
			qglMultiTexCoord2f (GL_TEXTURE0, s, t);
			qglMultiTexCoord2f (GL_TEXTURE1, s, t);
		}
		else
		{
			glTexCoord2f (s, t);
		}

		lerpfrac = VectorL2Compare(v1->vec, v2->vec, distance) ? ent->framelerp : 1;

		if (gl_vertexlights.value && !full_light)
		{
			l = R_LerpVertexLight (v1->anorm_pitch, v1->anorm_yaw, v2->anorm_pitch, v2->anorm_yaw, lerpfrac, apitch, ayaw);
			l = min(l, 1);
		}
		else
		{
			l = FloatInterpolate (shadedots[v1->oldnormal>>8], lerpfrac, shadedots[v2->oldnormal>>8]);
			l = (l * shadelight + ambientlight) / 256;
			l = min(l, 1);
		}

		for (j=0 ; j<3 ; j++)
			lightvec[j] = lightcolor[j] / 256 + l;

		glColor4f (lightvec[0], lightvec[1], lightvec[2], ent->transparency);
		VectorInterpolate (v1->vec, lerpfrac, v2->vec, interpolated_verts);
		glVertex3fv (interpolated_verts);

		*tris++;
	}
	glEnd ();

	if (surface_transparent)
	{
		glCullFace (GL_FRONT);
		glDisable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);		
		glDepthMask (GL_TRUE);
	}
	else 
	if (ISTRANSPARENT(ent))
		glDisable (GL_BLEND);
}

/*
=================
R_DrawQ3Shadow
=================
*/
void R_DrawQ3Shadow (entity_t *ent, float lheight, float s1, float c1, trace_t downtrace)
{
	int		i, j, numtris, pose1, pose2;
	vec3_t		point1, point2, interpolated;
	md3header_t	*pmd3hdr;
	md3surface_t	*pmd3surf;
	unsigned int	*tris;
	md3vert_mem_t	*verts;
	model_t		*clmodel = ent->model;
#if 0
	float		m[16];
	md3tag_t	*tag;
	tagentity_t	*tagent;
#endif

	pmd3hdr = (md3header_t *)Mod_Extradata (clmodel);

	pmd3surf = (md3surface_t *)((byte *)pmd3hdr + pmd3hdr->ofssurfs);
	for (i=0 ; i<pmd3hdr->numsurfs ; i++)
	{
		verts = (md3vert_mem_t *)((byte *)pmd3hdr + pmd3surf->ofsverts);
		tris = (unsigned int *)((byte *)pmd3surf + pmd3surf->ofstris);
		numtris = pmd3surf->numtris * 3;
		pose1 = ent->pose1 * pmd3surf->numverts;
		pose2 = ent->pose2 * pmd3surf->numverts;

		glBegin (GL_TRIANGLES);
		for (j=0 ; j<numtris ; j++)
		{
			// normals and vertexes come from the frame list
			VectorCopy (verts[*tris+pose1].vec, point1);

			point1[0] -= shadevector[0] * (point1[2] + lheight);
			point1[1] -= shadevector[1] * (point1[2] + lheight);

			VectorCopy (verts[*tris+pose2].vec, point2);

			point2[0] -= shadevector[0] * (point2[2] + lheight);
			point2[1] -= shadevector[1] * (point2[2] + lheight);

			VectorInterpolate (point1, ent->framelerp, point2, interpolated);

			interpolated[2] = -(ent->origin[2] - downtrace.endpos[2]);

			interpolated[2] += ((interpolated[1] * (s1 * downtrace.plane.normal[0])) - 
					    (interpolated[0] * (c1 * downtrace.plane.normal[0])) - 
					    (interpolated[0] * (s1 * downtrace.plane.normal[1])) - 
					    (interpolated[1] * (c1 * downtrace.plane.normal[1]))) + 
					    ((1 - downtrace.plane.normal[2]) * 20) + 0.2;

			glVertex3fv (interpolated);

			*tris++;
		}
		glEnd ();

		pmd3surf = (md3surface_t *)((byte *)pmd3surf + pmd3surf->ofsend);
	}

	if (!pmd3hdr->numtags)	// single model, done
		return;

// no multimodel shadow support yet
#if 0
	tag = (md3tag_t *)((byte *)pmd3hdr + pmd3hdr->ofstags);
	tag += ent->pose2 * pmd3hdr->numtags;
	for (i=0 ; i<pmd3hdr->numtags ; i++, tag++)
	{
		if (multimodel_level == 0 && !strcmp(tag->name, "tag_torso"))
		{
			tagent = &q3player_body;
			ent = &q3player_body.ent;
			multimodel_level++;
		}
		else if (multimodel_level == 1 && !strcmp(tag->name, "tag_head"))
		{
			tagent = &q3player_head;
			ent = &q3player_head.ent;
			multimodel_level++;
		}
		else
		{
			continue;
		}

		glPushMatrix ();
		R_RotateForTagEntity (tagent, tag, m);
		glMultMatrixf (m);
		R_DrawQ3Shadow (ent, lheight, s1, c1, downtrace);
		glPopMatrix ();		
	}
#endif
}

#define	ADD_EXTRA_TEXTURE(_texture, _param)				\
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, _param);	\
	GL_Bind (_texture);						\
									\
	glDepthMask (GL_FALSE);						\
	glEnable (GL_BLEND);						\
	glBlendFunc (GL_ONE, GL_ONE);					\
									\
	R_DrawQ3Frame (frame, pmd3hdr, pmd3surf, ent, INTERP_MAXDIST);	\
									\
	glDisable (GL_BLEND);						\
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);		\
	glDepthMask (GL_TRUE);

/*
=================
R_SetupQ3Frame
=================
*/
void R_SetupQ3Frame (entity_t *ent)
{
	int				i, j, frame, shadernum, texture, fb_texture, st_texture, pt_texture, eshirt;
	float			m[16];
	md3header_t		*pmd3hdr;
	md3surface_t	*pmd3surf;
	md3tag_t		*tag;
	model_t			*clmodel = ent->model;
	tagentity_t		*tagent;

	if (!strcmp(clmodel->name, "progs/player/lower.md3"))
		frame = legsframe;
	else if (!strcmp(clmodel->name, "progs/player/upper.md3"))
		frame = bodyframe;
	else
	{
		frame = ent->frame;
	}
	// locate the proper data
	pmd3hdr = (md3header_t *)Mod_Extradata (clmodel);

	// draw all the triangles

	// draw non-transparent surfaces first, then the transparent ones
	for (i=0 ; i<2 ; i++)
	{
		pmd3surf = (md3surface_t *)((byte *)pmd3hdr + pmd3hdr->ofssurfs);
		for (j=0 ; j<pmd3hdr->numsurfs ; j++)
		{
			md3shader_mem_t	*shader;

			surface_transparent =  (strstr(pmd3surf->name, "energy") || 
						strstr(pmd3surf->name, "f_") || 
						strstr(pmd3surf->name, "plasma") || 
						strstr(pmd3surf->name, "flare") || 
						strstr(pmd3surf->name, "flash") || 
						strstr(pmd3surf->name, "Sphere") || 
						strstr(pmd3surf->name, "telep"));

			if ((!i && surface_transparent) || (i && !surface_transparent))
			{
				pmd3surf = (md3surface_t *)((byte *)pmd3surf + pmd3surf->ofsend);
				continue;
			}

			c_md3_polys += pmd3surf->numtris;

			shadernum = ent->skinnum;
			if ((shadernum >= pmd3surf->numshaders) || (shadernum < 0))
			{
				Con_DPrintf (1,"R_SetupQ3Frame: no such skin # %d\n", shadernum);
				shadernum = 0;
			}

			shader = (md3shader_mem_t *)((byte *)pmd3hdr + pmd3surf->ofsshaders);
			
			texture = shader[shadernum].gl_texnum;

			st_texture = shader[shadernum].st_texnum;
			pt_texture = shader[shadernum].pt_texnum;
			fb_texture = shader[shadernum].fb_texnum;
			
			if (r_outline.value)
				R_DrawQ3OutlineFrame (frame, pmd3hdr, pmd3surf, ent, INTERP_MAXDIST);

			if (fb_texture && gl_mtexable)
			{
				GL_DisableMultitexture ();
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				GL_Bind (texture);

				GL_EnableMultitexture ();
				if (gl_add_ext)
				{
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);					
					GL_Bind (fb_texture);
				}
				
				R_DrawQ3Frame (frame, pmd3hdr, pmd3surf, ent, INTERP_MAXDIST);

				if (!gl_add_ext)
				{
					ADD_EXTRA_TEXTURE(fb_texture, GL_DECAL);
				}

				GL_DisableMultitexture ();
			}
			else
			{
				GL_DisableMultitexture ();
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				GL_Bind (texture);

				R_DrawQ3Frame (frame, pmd3hdr, pmd3surf, ent, INTERP_MAXDIST);

				if (fb_texture)
				{
					ADD_EXTRA_TEXTURE(fb_texture, GL_REPLACE);
				}
			}
			//*********************************************************************************************************************
			if ((cl_teamskin.value)&&((ent->modelindex == cl_modelindex[mi_md3_player])||(ent->modelindex == cl_modelindex[mi_q3torso])||(ent->modelindex == cl_modelindex[mi_player])))
			{		
				GL_DisableMultitexture ();
				
				if (ent->modelindex == cl_modelindex[mi_md3_player])
				{			
					if (pt_texture && st_texture)
					{
						R_DrawQ3SkinFrame (frame, pmd3hdr, pmd3surf, ent, INTERP_MAXDIST, ent->pantscolor, pt_texture);
						R_DrawQ3SkinFrame (frame, pmd3hdr, pmd3surf, ent, INTERP_MAXDIST, ent->shirtcolor, st_texture);
					}
				}
				else
				{
					if (!strcmp(clmodel->name, "progs/player/upper.md3"))
					{
						R_DrawQ3SkinFrame (frame, pmd3hdr, pmd3surf, ent, INTERP_MAXDIST, ent->shirtcolor, st_texture);
					}
					else
					{
						if (!strcmp(clmodel->name, "progs/player/lower.md3"))
						{
							R_DrawQ3SkinFrame (frame, pmd3hdr, pmd3surf, ent, INTERP_MAXDIST, ent->pantscolor, pt_texture);
						}
					}
				}
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}	
			pmd3surf = (md3surface_t *)((byte *)pmd3surf + pmd3surf->ofsend);
		}
	}

	if (!pmd3hdr->numtags)	// single model, done
		return;
	
	eshirt = ent->shirtcolor;

	tag = (md3tag_t *)((byte *)pmd3hdr + pmd3hdr->ofstags);
	tag += frame * pmd3hdr->numtags;

	for (i=0 ; i<pmd3hdr->numtags ; i++, tag++)
	{
		if (multimodel_level == 0 && !strcmp(tag->name, "tag_torso"))
		{
			tagent = &q3player_body;
			ent = &q3player_body.ent;
			ent->shirtcolor =  eshirt;
			multimodel_level++;
		}
		else if (multimodel_level == 1 && !strcmp(tag->name, "tag_head"))
		{
			tagent = &q3player_head;
			ent = &q3player_head.ent;
			multimodel_level++;
		}
		else
		{
			continue;
		}

		glPushMatrix ();		
		R_RotateForTagEntity (tagent, tag, m);
		glMultMatrixf (m);
		R_SetupQ3Frame (ent);
		glPopMatrix ();		
	}
}

/*
=================
R_DrawQ3Model
=================
*/
void R_DrawQ3Model (entity_t *ent)
{
	vec3_t		mins, maxs, md3_scale_origin = {0, 0, 0};
	model_t		*clmodel = ent->model;
	float scale;
	
	if (ent->cullFrame != r_framecount)
	{
		VectorAdd (ent->origin, clmodel->mins, mins);
		VectorAdd (ent->origin, clmodel->maxs, maxs);

		if (ent->angles[0] || ent->angles[1] || ent->angles[2])
			ent->culled = R_CullSphere(ent->origin, clmodel->radius);
		else
			ent->culled = R_CullBox(mins, maxs);

		ent->cullFrame = r_framecount;
	}

	if (ent->culled)
	{
		return;
	}

	VectorCopy (ent->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// get lighting information
	R_SetupLighting (ent);

	shadedots = r_avertexnormal_dots[((int)(ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	glPushMatrix ();	

	if (ent == &cl.viewent)
		R_RotateForViewEntity (ent);
	else
	{
		R_RotateForEntity(ent, false);
	}

	if ((ent == &cl.viewent) && (cl_gun_fovscale.value) && (scr_fov.value != 0))
	{
		if (scr_fov.value <= 90)
			scale = 1.0f;
		else
			scale = 1.0f / tan( DEG2RAD(scr_fov.value/2));

		glTranslatef (md3_scale_origin[0]*scale, md3_scale_origin[1], md3_scale_origin[2]);
		glScalef (scale, 1, 1);
	}
	else
	{
		glTranslatef (md3_scale_origin[0], md3_scale_origin[1], md3_scale_origin[2]);
	}

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	if ((!strcmp(ent->model->name, "progs/player/lower.md3"))||(!strcmp(ent->model->name, "progs/player/upper.md3")))
	{
		q3player_body.ent.transparency = q3player_head.ent.transparency = cl_entities[cl.viewentity].transparency;
		R_ReplaceQ3Frame (ent->frame);
		ent->noshadow = true;
	}

	multimodel_level = 0;
	R_SetupQ3Frame (ent);

	glShadeModel (GL_FLAT);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix ();	

	if (r_shadows.value && !ent->noshadow)
	{
		int		farclip;
		float		theta, lheight, s1, c1;
		vec3_t		downmove;
		trace_t		downtrace;

		farclip = max((int)r_farclip.value, 4096);

		if (!shadescale)
			shadescale = 1 / sqrt(2);
		theta = -ent->angles[1] / 180 * M_PI;

		VectorSet (shadevector, cos(theta) * shadescale, sin(theta) * shadescale, shadescale);

		glPushMatrix ();
		
		R_RotateForEntity (ent, true);

		VectorCopy (ent->origin, downmove);
		downmove[2] -= farclip;
		memset (&downtrace, 0, sizeof(downtrace));
		downtrace.fraction = 1;
		SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, ent->origin, downmove, &downtrace);

		lheight = ent->origin[2] - lightspot[2];

		s1 = sin(ent->angles[1] / 180 * M_PI);
		c1 = cos(ent->angles[1] / 180 * M_PI);

		glDepthMask (GL_FALSE);
		glDisable (GL_TEXTURE_2D);
		glEnable (GL_BLEND);
		glColor4f (0, 0, 0,((shadelight - (mins[2] - downtrace.endpos[2]))*r_shadows.value)*0.0066*ent->transparency);//R00k ACK! it works!

		multimodel_level = 0;

		if (have_stencil) 
		{
			glEnable(GL_STENCIL_TEST);
			glStencilFunc(GL_EQUAL,1,2);
			glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);
		}

		R_DrawQ3Shadow (ent, lheight, s1, c1, downtrace);

		if (have_stencil) 
		{
			glDisable(GL_STENCIL_TEST);
			//glClear(GL_STENCIL_BUFFER_BIT);
		}
		glDepthMask (GL_TRUE);
		glEnable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);

		glPopMatrix ();		
	}

	glColor4f(1,1,1,1);
}
//==================================================================================

// joe: from FuhQuake
void R_SetSpritesState(qboolean state)
{
	static qboolean	r_state = false;

	if (r_state == state)
		return;

	r_state = state;

	if (state)
	{
		if (currententity->model->modhint == MOD_SPR32)
		{
			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable (GL_BLEND);
			glDepthMask (GL_FALSE);	// disable zbuffer updates
		}
		else
		{
			GL_DisableMultitexture ();
			glEnable (GL_ALPHA_TEST);
		}
	}
	else
	{
		if (currententity->model->modhint == MOD_SPR32)
		{
			glDisable (GL_BLEND);
			glDepthMask (GL_TRUE);	// enable zbuffer updates
		}
		else
		{
			glDisable (GL_ALPHA_TEST);
		}
	}
}

void SortEntitiesByTransparency (void)
{
	int		i, j;
	entity_t	*tmp;

	for (i = 0 ; i < cl_numvisedicts ; i++)
	{
		if (cl_visedicts[i]->transparency < 1 && cl_visedicts[i]->transparency > 0)
		{
			for (j = cl_numvisedicts - 1 ; j > i ; j--)
			{
				// if not transparent, exchange with transparent
				if (cl_visedicts[j]->transparency == 1)
				{
					tmp = cl_visedicts[i];
					cl_visedicts[i] = cl_visedicts[j];
					cl_visedicts[j] = tmp;
					break;
				}
				else
					continue;
			}
			if (j == i)
				return;
		}
	}
}

float SpriteForMDL(void)
{
	if ((!currententity->model)||(!currententity))
		return 0;

	if ((!strcmp(currententity->model->name, "maps/b_shell0.bsp")) || (!strcmp(currententity->model->name, "maps/b_shell1.bsp")))
	{		
		if (mi_2dshells)
		return mi_2dshells;
	}
	
	if ((!strcmp(currententity->model->name, "maps/b_batt0.bsp")) || (!strcmp(currententity->model->name, "maps/b_batt1.bsp")))
	{	
		if (mi_2dcells)
		return mi_2dcells;
	}
	
	if ((!strcmp(currententity->model->name, "maps/b_rock0.bsp")) || (!strcmp(currententity->model->name, "maps/b_rock1.bsp")))
	{
		return mi_2drockets;
	}
	
	if ((!strcmp(currententity->model->name, "maps/b_nail0.bsp")) || (!strcmp(currententity->model->name, "maps/b_nail1.bsp")))
	{		
		return mi_2dnails;
	}
	
	if (!strcmp(currententity->model->name, "maps/b_bh100.bsp"))
	{
		return mi_2dmega;
	}

	if (!strcmp(currententity->model->name, "maps/b_bh10.bsp"))
	{
		return mi_2dhealth10;
	}
	
	if (!strcmp(currententity->model->name, "maps/b_bh25.bsp"))
	{
		return mi_2dhealth25;
	}

	//------------------------
	if (!strcmp(currententity->model->name, "progs/invulner.mdl"))
	{
		return mi_2dpent;
	}

	if (!strcmp(currententity->model->name, "progs/quaddama.mdl"))
	{		
		return mi_2dquad;
	}

	if (!strcmp(currententity->model->name, "progs/invisibl.mdl"))
	{		
		return mi_2dring;
	}

	if (!strcmp(currententity->model->name, "progs/suit.mdl"))
	{		
		return mi_2dsuit;
	}
	//---------------------------------------------
	if (!strcmp(currententity->model->name, "progs/armor.mdl"))
	{
		if (currententity->skinnum == 0)
			return mi_2darmor1;
		if (currententity->skinnum == 1)
			return mi_2darmor2;
		if (currententity->skinnum == 2)
			return mi_2darmor3;
	}
	
	if(!strcmp(currententity->model->name, "progs/backpack.mdl"))
    {
		return mi_2dbackpack;		
	}

	if(!strcmp(currententity->model->name, "progs/g_rock2.mdl"))
    {
		return mi_2drl;		
	}

	if(!strcmp(currententity->model->name, "progs/g_rock.mdl"))
    {
		return mi_2dgl;		
	}

	if(!strcmp(currententity->model->name, "progs/g_light.mdl"))
    {
		return mi_2dlg;		
	}

	if(!strcmp(currententity->model->name, "progs/g_nail.mdl"))
    {
		return mi_2dng;		
	}
	if(!strcmp(currententity->model->name, "progs/g_nail2.mdl"))
    {
		return mi_2dsng;		
	}
	if(!strcmp(currententity->model->name, "progs/g_shot.mdl"))
    {
		return mi_2dssg;		
	}
	/*
	if(!strcmp(currententity->model->name, "progs/flame.mdl"))
    {
		if (r_drawflame.value)
		{
			if (mi_2dflame)
				return mi_2dflame;		
		}
		return -1;
	}

	if(!strcmp(currententity->model->name, "progs/flame2.mdl"))
    {
		if (r_drawflame.value)
		{
			if (mi_2dflame2)
			return mi_2dflame2;		
		}
		return -1;
	}


	if(!strcmp(currententity->model->name, "progs/h_player.mdl"))
    {
		return mi_2dplayer_h;		
	}

	if(!strcmp(currententity->model->name, "progs/gib1.mdl"))
    {
		return mi_2dgib1;		
	}

	if(!strcmp(currententity->model->name, "progs/gib2.mdl"))
    {
		return mi_2dgib2;		
	}

	if(!strcmp(currententity->model->name, "progs/gib3.mdl"))
    {
		return mi_2dgib3;		
	}
*/
	return -1;
}

int SetFlameModelState(void)
{
	dlight_t	*dl;
	float frametime;

	if (currententity->model->modhint != MOD_FLAME)
		return 0;

	frametime = (fabs(cl.ctime - cl.oldtime)*10);
	
	if (!gl_part_flames.value && (!strcmp(currententity->model->name, "progs/flame0.mdl")))
	{
		if (!r_drawflame.value) //R00k
			return -1;

		currententity->model = cl.model_precache[cl_modelindex[mi_flame1]];

		if ((r_drawflame.value == 2)&&(r_dynamic.value)) //R00k
		{
			dl = CL_AllocDlight (0);					
			VectorCopy (currententity->origin, dl->origin);					
			dl->radius = (rand()%50+1);	
			dl->die = cl.time + frametime;						
			dl->type = lt_explosion;
		}
		return 0;
	}
	else if (gl_part_flames.value)
	{
		vec3_t	liteorg;

		VectorCopy (currententity->origin, liteorg);
		
		if (currententity->baseline.modelindex == cl_modelindex[mi_flame0])
		{
			if (!r_drawflame.value) //R00k
				return -1;

			liteorg[2] += 5.5;
			QMB_TorchFlame(liteorg);
			
			if ((r_drawflame.value == 2)&&(r_dynamic.value)) //R00k
			{
				dl = CL_AllocDlight (0);					
				VectorCopy (liteorg, dl->origin);					
				dl->radius = (rand()%50+1);	
				dl->die = cl.time + frametime;						
				dl->type = lt_explosion;
			}
		}
		else if (currententity->baseline.modelindex == cl_modelindex[mi_flame1])
		{
			if (!r_drawflame.value) //R00k
				return -1;

			liteorg[2] += 5.5;
			QMB_TorchFlame(liteorg);
			
			if (cl_modelindex[mi_flame0])//R00k dont swap models if the precache failed. (invalid pointer)
				currententity->model = cl.model_precache[cl_modelindex[mi_flame0]];

			if ((r_drawflame.value == 2)&&(r_dynamic.value)) //R00k
			{
				dl = CL_AllocDlight (0);					
				VectorCopy (liteorg, dl->origin);					
				dl->radius = (rand()%50+1);	
				dl->die = cl.time + frametime;						
				dl->type = lt_explosion;
			}
		}
		else if (currententity->baseline.modelindex == cl_modelindex[mi_flame2])
		{
			if (!r_drawflame.value) //R00k
				return -1;

			liteorg[2] -= 1;
			QMB_BigTorchFlame(liteorg);

			if ((r_drawflame.value == 2)&&(r_dynamic.value)) //R00k
			{
				dl = CL_AllocDlight (0);					
				VectorCopy (liteorg, dl->origin);					
				dl->radius = (rand()%75+1);	
				dl->die = cl.time + frametime;						
				dl->type = lt_explosion;
			}

			return -1;	//continue;
		}		
		else
		if (!strcmp(currententity->model->name, "progs/wyvflame.mdl"))
		{
			if (!r_drawflame.value) //R00k
				return -1;

			liteorg[2] -= 1;
			QMB_BigTorchFlame(liteorg);
			return -1;	//continue;
		}
		else
		if ((currententity->baseline.modelindex == cl_modelindex[mi_md3_flame0])||(currententity->model->modhint == MOD_FLAME))
		{
			if (!r_drawflame.value) //R00k
				return -1;

			liteorg[2] += 7;
			QMB_TorchFlame(liteorg);
			
			if ((r_drawflame.value == 2)&&(r_dynamic.value)) //R00k
			{
				dl = CL_AllocDlight (0);					
				VectorCopy (liteorg, dl->origin);					
				dl->radius = (rand()%50+1);	
				dl->die = cl.time + frametime;						
				dl->type = lt_explosion;
			}
		}
	}
	return 0;
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList(void)
{
	int	i, idx;

	if (!r_drawentities.value)
		return;

	SortEntitiesByTransparency();//Joe

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		if (r_simpleitems.value)
		{
			idx = SpriteForMDL();
			if (idx >=0)
			{
				currententity->model = cl.model_precache[cl_modelindex[idx]];
			}
		}

		if ((currententity == &cl_entities[cl.viewentity]) && (chase_active.value))
		{
				currententity->angles[PITCH] = bound(-45,currententity->angles[PITCH],45);//R00k: limit the pitch of the player model in chase view.				
		}

		switch (currententity->model->type)
		{
			case mod_alias:

				if (qmb_initialized && SetFlameModelState() == -1)
					continue;
				
				//R00k, replace laser.mdl with particle effect, if using QMB particles
				if (((qmb_initialized) && (particle_mode)) && !strcmp(currententity->model->name, "progs/laser.mdl"))
				{				
					currententity->model->flags |= EF_TRACER2;
					continue;
				}

				//R00k replace keys with flag model for pubCTF -- start 
				if ((cl_teamflags.value)&&(!sv.active))//dont replace on local singleplayer games...
				{	
					if (cl_modelindex[mi_flag] != 0)//Make sure we have the flag.mdl file precached.
					{
						if (currententity->baseline.modelindex == cl_modelindex[mi_w_g_key])
						{	
							currententity->skinnum = 0;
							currententity->model = cl.model_precache[cl_modelindex[mi_flag]];									
							currententity->origin[2] -= 22;
							//R00k dont animate since qc doesnt update frames for the "key" models
							currententity->frame = currententity->baseline.frame;
							currententity->pose1 = currententity->pose2 = 0;
							currententity->noshadow = true;
						}
						else
						{
							if (currententity->baseline.modelindex == cl_modelindex[mi_w_s_key])
							{							
								currententity->model = cl.model_precache[cl_modelindex[mi_flag]];									
								currententity->origin[2] -= 22;
								//R00k dont animate since qc doesnt update frames for the "key" models
								currententity->frame = currententity->baseline.frame;
								currententity->pose1 = currententity->pose2 = 0;
								currententity->skinnum = 1;
								currententity->noshadow = true;
							}
						}
					}
				}//replace keys with flag model for CTF -- end 

				R_DrawAliasModel(currententity);	

			break;
			
			case mod_md3: 
				R_DrawQ3Model (currententity); 
				break;
		
			case mod_brush:
				R_DrawBrushModel (currententity);
				break;
			
			case mod_sprite:					
					R_SetSpritesState (true);
					R_DrawSpriteModel (currententity);
					R_SetSpritesState (false);
				break;

			case mod_spr32:
					R_SetSpritesState (true);
					R_DrawSpriteModel (currententity);
					R_SetSpritesState (false);
				break;

			default:
				break;
		}
	}
}


/*
=============
R_DrawViewModel
=============
*/
extern void QMB_LaserSight (void);	
void R_DrawViewModel(void)
{
	currententity = &cl.viewent;

	if (!r_drawviewmodel.value || chase_active.value || !r_drawentities.value || (cl.stats[STAT_HEALTH] <= 0) || !currententity->model || freemoving.value)
	{
		return;
	}	

	currententity->transparency = (cl.items & IT_INVISIBILITY) ? gl_ringalpha.value : bound(0, r_drawviewmodel.value, 1);
	
	// hack the depth range to prevent view model from poking into walls
	glDepthRange (0.005f, 0.3f);

	switch (currententity->model->type)
	{
		case mod_md3:
			R_DrawQ3Model (currententity);
			break;

		case mod_alias:
			R_DrawAliasModel(currententity);
			break;
	}

	glDepthRange (gldepthmin, gldepthmax);
}
#ifdef USESHADERS
void GL_PolyBlend (void)
{
	extern GLuint arb_polyblend_vp;
	extern GLuint arb_polyblend_fp;

	float blendverts[] = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};

	// whaddaya know - these run faster
	if (!gl_polyblend.value) return;
	if (v_blend[3] <= 0) v_blend[3] = 0.1;//return;
//	if (r_dowarp) return;

	// fixme - need to implement some state filtering here...
	glDepthMask (GL_FALSE);
	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);

	glViewport (0, 0, vid.width, vid.height);

	GL_EnableVertexArrays (VAA_0);
	GL_VertexArrayPointer (0, 0, 3, GL_FLOAT, GL_FALSE, 0, blendverts);

	GL_SetVertexProgram (arb_polyblend_vp);
	GL_SetFragmentProgram (arb_polyblend_fp);
	qglProgramLocalParameter4fvARB (GL_VERTEX_PROGRAM_ARB, 0, v_blend);

	GL_DrawArrays (GL_QUADS, 0, 4);
	glDepthMask(TRUE);
}
#endif

void R_PolyBlend (void)
{
	if (/*(vid_hwgamma_enabled && gl_hwblend.value) ||*/ !v_blend[3])
		return;
 	
	GL_DisableMultitexture();

	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_TEXTURE_2D);

    glLoadIdentity ();

#ifndef USEFAKEGL
	if (r_bloom.value)
		VectorScale(v_blend,0.10,v_blend);
#endif
	glColor4fv (v_blend);

	glBegin (GL_QUADS);
		glVertex2f (r_refdef.vrect.x, r_refdef.vrect.y);
		glVertex2f (r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y);
		glVertex2f (r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y + r_refdef.vrect.height);
		glVertex2f (r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height);
	 
	glEnd ();

	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);

	glColor4f(1,1,1,1);
}

/*
================
R_BrightenScreen
================
*/
void R_BrightenScreen (void)
{
	extern	float	vid_gamma;
	float		f;

	if (vid_hwgamma_enabled || v_contrast.value <= 1.0)
		return;

	f = min(v_contrast.value, 3);
	f = pow (f, vid_gamma);
	
	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	glBlendFunc (GL_DST_COLOR, GL_ONE);
	glBegin (GL_QUADS);
	while (f > 1)
	{
		if (f >= 2)
			glColor3f (1, 1, 1);
		else
			glColor3f (f - 1, f - 1, f - 1);
		glVertex2f (0, 0);
		glVertex2f (vid.width, 0);
		glVertex2f (vid.width, vid.height);
		glVertex2f (0, vid.height);
		f *= 0.5;
	}
	 
	glEnd ();
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glColor4f(1,1,1,1);
}

int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

/*
===============
TurnVector -- johnfitz

turn forward towards side on the plane defined by forward and side
if angle = 90, the result will be equal to side
assumes side and forward are perpendicular, and normalized
to turn away from side, use a negative angle
===============
*/
//#define DEG2RAD( a ) ( (a) * M_PI_DIV_180 )
void TurnVector (vec3_t out, const vec3_t forward, const vec3_t side, float angle)
{
	float scale_forward, scale_side;

	scale_forward = cos( DEG2RAD( angle ) );
	scale_side = sin( DEG2RAD( angle ) );

	out[0] = scale_forward*forward[0] + scale_side*side[0];
	out[1] = scale_forward*forward[1] + scale_side*side[1];
	out[2] = scale_forward*forward[2] + scale_side*side[2];
}

/*
===============
R_SetFrustum 
===============
*/

static void R_SetFrustum (void)
{
	//**Widescreen Support:
	//
	// LordHavoc: note to all quake engine coders, the special case for 90
	// degrees assumed a square view (wrong), so I removed it, Quake2 has it
	// disabled as well.

	// rotate R_VIEWFORWARD right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90 - r_refdef.fov_x / 2));
	frustum[0].dist = DotProduct (r_origin, frustum[0].normal);
	PlaneClassify(&frustum[0]);

	// rotate R_VIEWFORWARD left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, (90 - r_refdef.fov_x / 2));
	frustum[1].dist = DotProduct (r_origin, frustum[1].normal);
	PlaneClassify(&frustum[1]);

	// rotate R_VIEWFORWARD up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, -(90 - r_refdef.fov_y / 2));
	frustum[2].dist = DotProduct (r_origin, frustum[2].normal);
	PlaneClassify(&frustum[2]);

	// rotate R_VIEWFORWARD down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, (90 - r_refdef.fov_y / 2));
	frustum[3].dist = DotProduct (r_origin, frustum[3].normal);
	PlaneClassify(&frustum[3]);
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	vec3_t		testorigin;
	mleaf_t		*leaf;

	if (nehahra)
	{
		if (oldsky.value && r_skybox.string[0])
			Cvar_Set ("r_skybox", "");
		if (!oldsky.value && !r_skybox.string[0])
			Cvar_Set ("r_skybox", prev_skybox);
	}

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;

	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);
	r_viewleaf2 = NULL;

	// check above and below so crossing solid water doesn't draw wrong
	if (r_viewleaf->contents <= CONTENTS_WATER && r_viewleaf->contents >= CONTENTS_LAVA)
	{
		// look up a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] += 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents == CONTENTS_EMPTY)
			r_viewleaf2 = leaf;
	}
	else if (r_viewleaf->contents == CONTENTS_EMPTY)
	{
		// look down a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] -= 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents <= CONTENTS_WATER &&	leaf->contents >= CONTENTS_LAVA)
			r_viewleaf2 = leaf;
	}

	V_SetContentsColor (r_viewleaf->contents);

	V_AddWaterfog (r_viewleaf->contents);	
	
	if ((nehahra) || (gl_fogenable.value))
		Neh_SetupFrame ();

	// (this is the same condition as software Quake used so it's a valid baseline to work from; we add cl.inwater
	// for func_water support but expect some latency between when the server sets it and the client recieves it)
	r_dowarp = r_waterwarp.value && (r_viewleaf->contents <= CONTENTS_WATER);

	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;
}

__inline void MYgluPerspective (GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	GLdouble	xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;
/*
	xmin += -( 2 * 0 ) / zNear;
	xmax += -( 2 * 0 ) / zNear;
*/
	glFrustum (xmin, xmax, ymin, ymax, zNear, zFar);	
}

gltexture_t gl_postproc_texture;

int gl_rtt_width = 0;
int gl_rtt_height = 0;

//From RMQengine--

void R_CreateRTTTextures (int width, int height)
{
	int maxrtt;
	int hunkmark = Hunk_LowMark ();

	// figure the size to create the texture at
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &maxrtt);

	// scale to powers of 2
	for (gl_rtt_width = 1; gl_rtt_width < width; gl_rtt_width <<= 1);
	for (gl_rtt_height = 1; gl_rtt_height < height; gl_rtt_height <<= 1);

	// clamp to max texture size
	if (gl_rtt_width > maxrtt) gl_rtt_width = maxrtt;
	if (gl_rtt_height > maxrtt) gl_rtt_height = maxrtt;

	glGenTextures (1, &(GLuint)gl_postproc_texture.texnum);
	
	glBindTexture (GL_TEXTURE_2D, gl_postproc_texture.texnum);
		
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, gl_rtt_width, gl_rtt_height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, Hunk_Alloc (gl_rtt_width * gl_rtt_height * 4));

	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//Con_Printf ("Created %s at %ix%i\n", "gl_postproc_texture", gl_rtt_width, gl_rtt_height);
	Hunk_FreeToLowMark (hunkmark);
}

void R_SetupGL (void)
{
	float	screenaspect;
	int	x, x2, y2, y, w, h, farclip;

	// set up viewpoint
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight/vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;

	w = x2 - x;
	h = y - y2;

	if (r_dowarp)
	{
		// bound viewport dimensions to the rtt texture size
		if (w > gl_rtt_width) w = gl_rtt_width;
		if (h > gl_rtt_height) h = gl_rtt_height;
	}

	glViewport (glx + x, gly + y2, w, h);

	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	farclip = max((int)r_farclip.value, 4096);
	
	if (chase_active.value)
		MYgluPerspective (r_refdef.fov_y, screenaspect, 1, farclip); 
	else
		MYgluPerspective (r_refdef.fov_y, screenaspect, 4, farclip);

	glCullFace (GL_FRONT);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	glRotatef (-90, 1, 0, 0);	    // put Z going up
	glRotatef (90, 0, 0, 1);	    // put Z going up
	glRotatef (-r_refdef.viewangles[2], 1, 0, 0);
	glRotatef (-r_refdef.viewangles[0], 0, 1, 0);
	glRotatef (-r_refdef.viewangles[1], 0, 0, 1);
	glTranslatef (-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix.m16);

	// set drawing parms
	if (gl_cull.value)
		glEnable (GL_CULL_FACE);
	else
		glDisable (GL_CULL_FACE);

	glDisable (GL_BLEND);
	glDisable (GL_ALPHA_TEST);
	glEnable (GL_DEPTH_TEST);
}

/*
===============
R_Init
===============
*/

extern void R_ToggleParticles_f (void);
extern void R_InitSmokes (void);
extern void R_InitDecals (void);
extern void R_ToggleDecals_f (void);//R00k
#ifndef USEFAKEGL
extern void R_InitBloomTextures( void );
#endif
void R_Init (void)
{	
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);
	Cmd_AddCommand ("toggleparticles", R_ToggleParticles_f);
	Cmd_AddCommand ("set_interpolated_weapon", Set_Interpolated_Weapon_f);
	Cmd_AddCommand ("toggledecals", R_ToggleDecals_f);//R00k

	Cvar_RegisterVariable (&r_interpolate_light);
	Cvar_RegisterVariable (&r_fullbright);
	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_drawviewmodelsize);//R00k
	Cvar_RegisterVariable (&r_shadows);
	Cvar_RegisterVariable (&r_shadows_throwdistance);
	Cvar_RegisterVariable (&r_wateralpha);
	Cvar_RegisterVariable (&r_lavaalpha);
	Cvar_RegisterVariable (&r_telealpha);
	Cvar_RegisterVariable (&r_turbalpha_distance);
	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariable (&r_novis);
	Cvar_RegisterVariable (&r_speeds);
	Cvar_RegisterVariable (&r_fastsky);
	Cvar_RegisterVariable (&r_fastturb);
	Cvar_RegisterVariable (&r_skybox);
	Cvar_RegisterVariable (&r_skyscroll);
	Cvar_RegisterVariable (&r_skyspeed);
	Cvar_RegisterVariable (&r_farclip);
//	Cvar_RegisterVariable (&r_showbboxes);
	Cvar_RegisterVariable (&r_drawflame);//R00k	
	Cvar_RegisterVariable (&r_drawlocs);
	Cvar_RegisterVariable (&r_outline);	
	Cvar_RegisterVariable (&r_outline_surf);	
	Cvar_RegisterVariable (&r_celshading);
	Cvar_RegisterVariable (&r_waterwarp);
	Cvar_RegisterVariable (&r_waterripple);
	Cvar_RegisterVariable (&gl_interpolate_transform);	
	Cvar_RegisterVariable (&gl_interpolate_animation);
	Cvar_RegisterVariable (&gl_finish);
	Cvar_RegisterVariable (&gl_clear);
	Cvar_RegisterVariable (&gl_cull);
	Cvar_RegisterVariable (&gl_smoothmodels);
	Cvar_RegisterVariable (&gl_affinemodels);
	Cvar_RegisterVariable (&gl_polyblend);
	Cvar_RegisterVariable (&gl_flashblend);
	Cvar_RegisterVariable (&gl_playermip);
	Cvar_RegisterVariable (&gl_nocolors);
	Cvar_RegisterVariable (&gl_loadlitfiles);
	Cvar_RegisterVariable (&gl_loadq3models);
	Cvar_RegisterVariable (&gl_doubleeyes);
	Cvar_RegisterVariable (&gl_interdist);
	Cvar_RegisterVariable (&gl_waterfog);
	Cvar_RegisterVariable (&gl_waterfog_density);
	Cvar_RegisterVariable (&gl_detail);
	Cvar_RegisterVariable (&gl_caustics);
	Cvar_RegisterVariable (&gl_ringalpha);
	Cvar_RegisterVariable (&gl_fb_bmodels);
	Cvar_RegisterVariable (&gl_fb_models);	
	Cvar_RegisterVariable (&gl_vertexlights);
	Cvar_RegisterVariable (&gl_part_explosions);
	Cvar_RegisterVariable (&gl_part_trails);
	Cvar_RegisterVariable (&gl_part_sparks);
	Cvar_RegisterVariable (&gl_part_gunshots);
	Cvar_RegisterVariable (&gl_part_blood);
	Cvar_RegisterVariable (&gl_part_telesplash);
	Cvar_RegisterVariable (&gl_part_blobs);
	Cvar_RegisterVariable (&gl_part_lavasplash);
	Cvar_RegisterVariable (&gl_part_flames);
	Cvar_RegisterVariable (&gl_part_lightning);
	Cvar_RegisterVariable (&gl_part_flies);
	Cvar_RegisterVariable (&gl_part_muzzleflash);
	Cvar_RegisterVariable (&gl_damageshells);
	Cvar_RegisterVariable (&gl_powerupshells);
	Cvar_RegisterVariable (&gl_powerupshell_size);
	Cvar_RegisterVariable (&gl_anisotropic);		
	Cvar_RegisterVariable (&gl_motion_blur);
	Cvar_RegisterVariable (&gl_waterblur);
	Cvar_RegisterVariable (&gl_nightmare);
	Cvar_RegisterVariable (&gl_deathblur);
	Cvar_RegisterVariable (&gl_hurtblur);
	Cvar_RegisterVariable (&gl_laserpoint);
	Cvar_RegisterVariable (&gl_lavasmoke);
	Cvar_RegisterVariable (&gl_fogenable); 
	Cvar_RegisterVariable (&gl_fogdensity);
	Cvar_RegisterVariable (&gl_fogstart); 
	Cvar_RegisterVariable (&gl_fogend); 
	Cvar_RegisterVariable (&gl_fogsky);
	Cvar_RegisterVariable (&gl_fogred); 
	Cvar_RegisterVariable (&gl_fogblue);
	Cvar_RegisterVariable (&gl_foggreen);
	Cvar_RegisterVariable (&cl_autodemo);
	Cvar_RegisterVariable (&cl_teamskin);
	Cvar_RegisterVariable (&cl_teamflags);
	Cvar_RegisterVariable (&cl_gun_fovscale);
	Cvar_RegisterVariable (&gl_shiny);
	Cvar_RegisterVariable (&gl_rain);//R00k
	Cvar_RegisterVariable (&gl_clip_muzzleflash);
#ifndef USEFAKEGL
	Cvar_RegisterVariable (&r_bloom);
	Cvar_RegisterVariable (&r_bloom_darken);
	Cvar_RegisterVariable (&r_bloom_alpha);
	Cvar_RegisterVariable (&r_bloom_color);
	Cvar_RegisterVariable (&r_bloom_diamond_size);
	Cvar_RegisterVariable (&r_bloom_intensity);
	Cvar_RegisterVariable (&r_bloom_sample_size);
	Cvar_RegisterVariable (&r_bloom_fast_sample);
#endif
	Cvar_RegisterVariable (&r_explosion_alpha);
	Cvar_RegisterVariable (&scr_bloodsplat);
	Cvar_RegisterVariable (&r_simpleitems);
	Cvar_RegisterVariable (&gl_overbright);
	Cvar_RegisterVariable (&gl_textureless);
	Cvar_RegisterVariable (&gl_lightning_alpha);
	Cvar_RegisterVariable (&gl_lightning_type);
	Cvar_RegisterVariable (&gl_color_deadbodies);
	Cvar_RegisterVariable (&r_uwfactor);
	Cvar_RegisterVariable (&r_uwscale);

	Cmd_AddLegacyCommand ("loadsky", "r_skybox");	

	R_InitTextures();
	R_InitOtherTextures();

	R_InitBubble();
	R_InitParticles();
	
	R_InitSmokes();//MHQuake
	R_InitVertexLights();

	R_InitDecals();
#ifndef USEFAKEGL
	R_InitBloomTextures(); 
#endif
	// by joe
	skyboxtextures = texture_extension_number;
	texture_extension_number += 6;

	playertextures = texture_extension_number;
	texture_extension_number += 16;

	// fullbright skins	
	fb_playertextures = texture_extension_number; //R00k: Tried disabling this feature but the below line bugs if commented out.

	texture_extension_number += 16;//FIXME, removing this line screws up the hud images... maybe we aren't increasing texture_extension_number during hud loading?
}

/*
================
R_ShowBBoxes 
================
*/
void R_ShowBoundingBoxes (void)
{
	extern		edict_t *sv_player;
	vec3_t		mins,maxs;
	edict_t		*ed;
	int			i,s;
	vec3_t color;
	qboolean cull;

	extern cvar_t developer_tool_showbboxes;

	if (!developer.value && !developer_tool_showbboxes.value)
		return;

	if (cl.maxclients > 1 || !r_drawentities.value || !sv.active)
		return;

	for (i = 0, ed = NEXT_EDICT(sv.edicts) ; i < sv.num_edicts ; i++, ed = NEXT_EDICT(ed))
	{
		cull = true;

		if (ed == sv_player)
			continue; //don't draw player's own bbox

		//box entity
		VectorCopy (ed->v.mins, mins);
		VectorCopy (ed->v.maxs, maxs);		
		s = (ed->v.solid);
		switch (s)
		{
			case SOLID_NOT:			color[0] = 0;color[1] = 1;color[2] = 0.1; break;//green
			case SOLID_TRIGGER:		color[0] = 1;color[1] = 0;color[2] = 0; break;	//red
			case SOLID_BBOX:		color[0] = 0;color[1] = 1;color[2] = 1; break;	//purple
			case SOLID_SLIDEBOX:	color[0] = 0;color[1] = 0;color[2] = 1; break;	//blue
			case SOLID_BSP: 		color[0] = 1;color[1] = 1;color[2] = 0; break;	//yellow
			default: 				color[0] = 1;color[1] = 1;color[2] = 1; break;	//white
		}
		GL_DrawSimpleBox(ed->v.origin, mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], color, cull);
	}	
}

#ifdef USESHADERS
void RTTWarp_EmitVertex (float *vert, float x, float y, float *rgba, float s, float t)
{
	vert[0] = x;
	vert[1] = y;
	vert[2] = 0;

	vert[3] = rgba[0];
	vert[4] = rgba[1];
	vert[5] = rgba[2];
	vert[6] = rgba[3];

	vert[7] = s;
	vert[8] = t;
}

void R_DrawUnderwaterWarp (void)
{
	extern GLuint arb_underwater_vp;
	extern GLuint arb_underwater_fp;

	float params[4] = {0, 0, 0, 0};

	float sh, th;
	float xl, yl, xh, yh;

	float *verts = (float *) scratchbuf;

	if (!r_dowarp)
	{
		return;
	}

	if (!gl_postproc_texture.texnum) 
	{
		return;
	}

	// this will be replaced by our shader warp...
	glViewport (glx, gly, glwidth, glheight);

	glDepthMask (GL_FALSE);
	glDisable (GL_ALPHA_TEST);
	glDisable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);

	glEnable (GL_VERTEX_PROGRAM_ARB);
	glEnable (GL_FRAGMENT_PROGRAM_ARB);

	GL_SetVertexProgram (arb_underwater_vp);
	GL_SetFragmentProgram (arb_underwater_fp);

	params[0] = cl.ctime * 10.18591625f * 4.0f;
	params[1] = 1.0f / r_uwfactor.value;
	params[2] = r_uwscale.value;

	qglProgramLocalParameter4fvARB (GL_VERTEX_PROGRAM_ARB, 0, params);
	qglProgramLocalParameter4fvARB (GL_FRAGMENT_PROGRAM_ARB, 0, params);

	GL_Bind(gl_postproc_texture.texnum);

	glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0,glwidth, glheight);
	
	sh = ((float) glwidth / (float) gl_rtt_width) * r_uwfactor.value;
	th = ((float) (glheight) / (float) gl_rtt_height) * r_uwfactor.value;

	GL_EnableVertexArrays (VAA_0 | VAA_1 | VAA_2);
	GL_VertexArrayPointer (0, 0, 3, GL_FLOAT, GL_FALSE, sizeof (float) * 9, &verts[0]);
	GL_VertexArrayPointer (0, 1, 4, GL_FLOAT, GL_FALSE, sizeof (float) * 9, &verts[3]);
	GL_VertexArrayPointer (0, 2, 2, GL_FLOAT, GL_FALSE, sizeof (float) * 9, &verts[7]);

	// expand the verts to overcome warp distortion
#define WARP_EXPAND 8.0f

	xl = (-WARP_EXPAND / (glwidth / 2.0f)) - 1.0f;
	xh = ((glwidth + WARP_EXPAND) / (glwidth / 2.0f)) - 1.0f;
	yl = ((-WARP_EXPAND) / (glheight / 2.0f)) - 1.0f;
	yh = ((glheight + WARP_EXPAND) / (glheight / 2.0f)) - 1.0f;

	// get polyblend for free here
	RTTWarp_EmitVertex (&verts[0], xl, yl, v_blend, 0, 0);
	RTTWarp_EmitVertex (&verts[9], xh, yl, v_blend, sh, 0);
	RTTWarp_EmitVertex (&verts[18], xh, yh, v_blend, sh, th);
	RTTWarp_EmitVertex (&verts[27], xl, yh, v_blend, 0, th);

	GL_DrawArrays (GL_QUADS, 0, 4);

	// disable view blending
	v_blend[3] = 0.1;//-1;

	// bind the default shaders (also updates cached state)
	GL_SetPrograms (0,0);
	
	glDisable (GL_VERTEX_PROGRAM_ARB);
	glDisable (GL_FRAGMENT_PROGRAM_ARB);	
	
	glDepthMask(TRUE);//fixed 
}

#endif

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
extern void R_UpdateCoronas ();
extern void R_SmokeFrame (void);
extern void R_DrawDecals (void);
extern void R_DrawLocs (void);
extern void QMB_LetItRain(void);
extern void CL_TexturePoint (void);
extern void SCR_SetupEdictTags(void);
extern void SCR_SetupAutoID (void);
extern cvar_t scr_drawautoids;

extern cvar_t	developer_tool_texture_point;
extern cvar_t	developer_tool_showbboxes;
extern cvar_t	developer_tool_show_edict_tags;

void R_RenderScene (void)
{
	vec3_t		colors;
		
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow
	
	R_DrawEntitiesOnList ();
	
	if ((particle_mode) && (decals_enabled))
		R_DrawDecals();

	R_DrawWaterSurfaces ();

	if (gl_coronas.value)
		R_UpdateCoronas();

	if (gl_lavasmoke.value)
		R_SmokeFrame ();

	if (gl_laserpoint.value)
		QMB_LaserSight ();// R00k

	if (developer_tool_texture_point.value)//R00k
		CL_TexturePoint();

	if (gl_rain.value)
		QMB_LetItRain();//R00k added
	
	GL_DisableMultitexture ();	

#ifndef USEFAKEGL
	GL_StencilShadowing();//GLQuake stencil shadows : Rich Whitehouse
#endif

	if ((gl_fogenable.value) && (gl_fogstart.value >= 0) && (gl_fogstart.value < gl_fogend.value))
	{
		glFogi(GL_FOG_MODE, GL_LINEAR);
		colors[0] = gl_fogred.value;
		colors[1] = gl_foggreen.value;
		colors[2] = gl_fogblue.value; 
		glFogfv(GL_FOG_COLOR, colors); 
		glFogf(GL_FOG_START, gl_fogstart.value); 
		glFogf(GL_FOG_END, gl_fogend.value); 
		glEnable(GL_FOG);
	}
	else
		glDisable(GL_FOG);

	if (r_drawlocs.value)
		R_DrawLocs();

	if (developer.value)
	{
		if (developer_tool_showbboxes.value)
			R_ShowBoundingBoxes();

		if (developer_tool_show_edict_tags.value)
			SCR_SetupEdictTags();
	}
	
	if ((cls.demoplayback) && (scr_drawautoids.value))
		SCR_SetupAutoID();	
}


void R_Clear (void)
{
	unsigned int clearbits;

	glDepthFunc (GL_LEQUAL);
	glDepthRange (gldepthmin, gldepthmax);
	clearbits = GL_DEPTH_BUFFER_BIT;

	if (have_stencil)
	{
		clearbits |= GL_STENCIL_BUFFER_BIT;
		glClearStencil(1);
	}

	if (gl_clear.value)
	{
		glClearColor (0.1, 0.1, 0.1, 1);
		clearbits |= GL_COLOR_BUFFER_BIT;
	}
	else if (!vid_hwgamma_enabled && v_contrast.value > 1)
	{
		glClearColor (0.1, 0.1, 0.1, 1.0f);//DARK grey
		clearbits |= GL_COLOR_BUFFER_BIT;
	}

	if (r_viewleaf && r_viewleaf->contents == CONTENTS_SOLID)
	{
		glClearColor (0.01f, 0.01f, 0.01f, 1.0f);//Close to original dos Quake
		clearbits |= GL_COLOR_BUFFER_BIT;
	}
	glClear (clearbits);
}


void R_RenderSceneBlur (float alpha, GLenum format)
{
	extern int	sceneblur_texture;
	int vwidth = 2, vheight = 2;
	float vs, vt;

	alpha = bound(0.1, alpha, 0.9);

	while (vwidth < glwidth)
	{
		vwidth *= 2;
	}
	while (vheight < glheight)
	{
		vheight *= 2;
	}

	glViewport (glx, gly, glwidth, glheight);

	GL_Bind(sceneblur_texture);

	// go 2d
	glMatrixMode(GL_PROJECTION);	
	glPushMatrix();
	{
		glLoadIdentity ();
		glOrtho  (0, glwidth, 0, glheight, -99999, 99999);
		glMatrixMode(GL_MODELVIEW);

		glPushMatrix();
		{
			glLoadIdentity ();

			vs = ((float)glwidth / vwidth);
			vt = ((float)glheight / vheight);

			glDisable (GL_DEPTH_TEST);
			glDisable (GL_CULL_FACE);
			glEnable(GL_BLEND);

			glColor4f(1, 1, 1, alpha);

			glBegin(GL_QUADS);
				glTexCoord2f(0, 0);
				glVertex2f(0, 0);
				glTexCoord2f(vs , 0);
				glVertex2f(glwidth, 0);
				glTexCoord2f(vs , vt);
				glVertex2f(glwidth, glheight);
				glTexCoord2f(0, vt);
				glVertex2f(0, glheight);
			glEnd();

			glEnable(GL_DEPTH_TEST);
			glDepthMask(1);
			glColor3f (1,1,1);   
			glDisable (GL_BLEND);   

			glMatrixMode(GL_PROJECTION);
		}
		glPopMatrix();		
		glMatrixMode(GL_MODELVIEW);
	}
	glPopMatrix();		

	glCopyTexImage2D(GL_TEXTURE_2D, 0, format, glx, gly, vwidth, vheight, 0);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);	  	
}



/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	extern void R_BloomBlend (int bloom);
	void R_Clear (void);
	double	time1 = 0, time2;
	static float r;
	

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		glFinish ();
		time1 = Sys_DoubleTime ();
		c_brush_polys = c_alias_polys = c_md3_polys = 0;
	}

	if (gl_finish.value)
		glFinish ();

	R_Clear();

	// render normal view
	R_RenderScene();

	R_RenderDlights();	
	
	R_DrawParticles();

	R_DrawViewModel();

#ifdef USESHADERS
	if (gl_polyblend.value == 2)// uses MH's shader, sometimes its glitchy
			GL_PolyBlend ();

	R_DrawUnderwaterWarp();
#endif

	if (r_speeds.value)
	{
		time2 = Sys_DoubleTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly %4i md3poly\n", (int)((time2 - time1) * 1000), c_brush_polys, c_alias_polys, c_md3_polys);
	}
//	R00k: Draw blurs 
	if ((cl.stats[STAT_HEALTH]< 0) && (gl_deathblur.value))
	{
		R_RenderSceneBlur (gl_deathblur.value, GL_RED);
	}
	else
	{
		if ((gl_hurtblur.value) && (cl.hurtblur > cl.time))
		{

			R_RenderSceneBlur (gl_hurtblur.value, GL_RGB);
		}
		else
		{
			if ((cl.items & IT_INVULNERABILITY) && (gl_nightmare.value))
			{
				R_RenderSceneBlur (gl_nightmare.value, GL_RED);
			}
			else
			{
				if ((r_viewleaf->contents <= CONTENTS_WATER) && gl_waterblur.value)
				{
					R_RenderSceneBlur (gl_waterblur.value, GL_RGB);
				}
				else
				{
					if (gl_motion_blur.value)
					{
						R_RenderSceneBlur (gl_motion_blur.value, GL_RGB);
					}
				}
			}
		}
	}

#ifndef USEFAKEGL
	R_BloomBlend(1);//BLOOMS
#endif

	if (scr_bloodsplat.value)
	{
		if ((cl.faceanimtime > cl.time)	&& (key_dest == key_game) && (!sv.paused))
		{
			if (r  < 1)
			{
				r = rand()&3;
			}

			{
				if (r > 2)
				{
					GL_FullscreenQuad(decal_blood1, (0.1 + (cl.faceanimtime - cl.time)));
				}
				else
				{
					if (r > 1)
					{
						GL_FullscreenQuad(decal_blood2, (0.1 + (cl.faceanimtime - cl.time)));
					}
					else
					{
						if (r > 0)
						{
							GL_FullscreenQuad(decal_blood3, (0.1 + (cl.faceanimtime - cl.time)));
						}
					}
				}
				r -= 0.01;
			}
		}
		else
			r = 0;
	}
}

