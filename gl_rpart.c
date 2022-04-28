/*
Copyright (C) 2002-2003, Dr Labman, A. Nourai

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
// gl_rpart.c

//FIXME FRAMETIME INDEPENDENT UPDATES!! ONLY UPDATE PARTICLES AT 72FPS DONT OVERDRAW!~!

#include "quakedef.h"

//#define	DEFAULT_NUM_PARTICLES		8192
#define	ABSOLUTE_MIN_PARTICLES		256
#define	ABSOLUTE_MAX_PARTICLES		32768

typedef	byte	col_t[4];

typedef	enum 
{
	p_spark, 
	p_smoke,
	p_fire, 
	p_fire2, 
	p_bubble, 
	p_lavasplash, 
	p_gunblast, 
	p_chunk, 
	p_shockwave,
	p_inferno_flame, 
	p_inferno_trail, 
	p_sparkray, 
	p_staticbubble, 
	p_trailpart,
	p_dpsmoke, 
	p_dpfire, 
	p_teleflare, 
	p_blood1, 
	p_blood2, 
	p_blood3,
	p_bloodcloud,
	p_flame, 
	p_lavatrail, 
	p_bubble2, 
	p_rain,
	p_streak, 
	p_streaktrail, 
	p_streakwave, 
	p_lightningbeam,
	p_glow, 
	p_alphatrail,//R00k
	p_torch_flame,
	p_flare,
	p_dot,
	p_muzzleflash,
	p_muzzleflash8,
	p_fly,
	num_particletypes
} part_type_t;

typedef	enum 
{
	pm_static, 
	pm_normal,
	pm_bounce,
	pm_die,
	pm_nophysics,
	pm_float,
	pm_rain,
	pm_streak,
	pm_streakwave,
} part_move_t;

typedef	enum 
{
	ptex_none,
	ptex_smoke,
	ptex_bubble,
	ptex_generic,
	ptex_dpsmoke,
	ptex_lava,
	ptex_blueflare,
	ptex_blood1,
	ptex_blood2,
	ptex_blood3,
	ptex_lightning,
	ptex_flame,
	ptex_muzzleflash,
	ptex_muzzleflash8,
	ptex_bloodcloud,
	ptex_fly,
	ptex_chunk,
	num_particletextures
} part_tex_t;

typedef	enum 
{
	pd_spark,
	pd_sparkray,
	pd_billboard,
	pd_hide,
	pd_beam,
} part_draw_t;

typedef struct particle_s 
{
	struct	particle_s	*next;
	vec3_t				org, endorg;
	col_t				color;
	float				growth;
	vec3_t				vel;
	float				rotangle;
	float				rotspeed;
	float				size;
	float				start;
	float				die;
	byte				hit;
	byte				texindex;
	byte				bounces;
} particle_t;

typedef	struct particle_type_s 
{
	particle_t			*start;		
	part_type_t			id;
	part_draw_t			drawtype;
	int					SrcBlend;
	int					DstBlend;
	part_tex_t			texture;
	float				startalpha;
	float				grav;
	float				accel;
	part_move_t 		move;
	float				custom;	
} particle_type_t;

#define	MAX_PTEX_COMPONENTS		8

typedef struct particle_texture_s 
{
	int		texnum;
	int		components;
	float	coords[MAX_PTEX_COMPONENTS][4];
} particle_texture_t;

static	float	sint[7] = {0.000000, 0.781832, 0.974928, 0.433884, -0.433884, -0.974928, -0.781832};
static	float	cost[7] = {1.000000, 0.623490, -0.222521, -0.900969, -0.900969, -0.222521, 0.623490};

static			particle_t			*particles, *free_particles;
static			particle_type_t		particle_types[num_particletypes];//R00k
static	int		particle_type_index[num_particletypes];	
static			particle_texture_t	particle_textures[num_particletextures];

//float			varray_vertex[16];//Vult
void			R_CalcBeamVerts (float *vert, vec3_t org1, vec3_t org2, float width);//Vult

vec3_t	NULLVEC = {0,0,0};//r00k

static	int		r_numparticles;		
static	vec3_t	zerodir = {22, 22, 22};
static	int		particle_count = 0;
static	float	particle_time;
qboolean		qmb_initialized = false;
int				particle_mode = 0;	// 0: classic (default), 1: QMB, 2: mixed

qboolean OnChange_gl_particle_count (cvar_t *var, char *string)
{
	float	f;

	f = bound(ABSOLUTE_MIN_PARTICLES, (atoi(string)), ABSOLUTE_MAX_PARTICLES);
	Cvar_SetValue("gl_particle_count", f);

	QMB_ClearParticles ();		// also re-allocc particles
	
	return true;
}
cvar_t	gl_particle_count	= {"gl_particle_count", "8192", true, false, OnChange_gl_particle_count};

cvar_t	gl_bounceparticles	= {"gl_bounceparticles", "1",true};
cvar_t	gl_flametype		= {"gl_flametype", "0",true};

void R_CalcBeamVerts (float *vert, vec3_t org1, vec3_t org2, float width);
extern cvar_t	cl_gun_offset;

qboolean TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	trace_t	trace;

	memset (&trace, 0, sizeof(trace));
	trace.fraction = 1;
	if (SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, start, end, &trace))
	{
		return false;
	}
	VectorCopy (trace.endpos, impact);

	if (normal)
		VectorCopy (trace.plane.normal, normal);

	return true;
}



static byte *ColorForParticle (part_type_t type)
{
	static	col_t	color;

	switch (type)
	{
	case p_spark:
		color[0] = 224 + (rand() & 31);
		color[1] = 100 + (rand() & 31);
		color[2] = 0;
		break;

	case p_torch_flame:
	case p_glow:
		color[0] = color[1] = color[2] = 255;
		break;

	case p_smoke:
		color[0] = color[1] = color[2] = 128;
		color[3] = 128;
		break;

	case p_fire:
	case p_fire2:
		color[0] = 255;
		color[1] = 142;
		color[2] = 62;
		break;

	case p_bubble:
	case p_bubble2:
	case p_staticbubble:
		color[0] = color[1] = color[2] = 192 + (rand() & 63);
		break;

	case p_teleflare:
	case p_lavasplash:
		color[0] = color[1] = color[2] = 128 + (rand() & 127);
		break;

	case p_gunblast:
		color[0]= 224 + (rand() & 31);
		color[1] = 170 + (rand() & 31);
		color[2] = 0;
		break;

	case p_chunk:
		color[0] = color[1] = color[2] = (32 + (rand() & 127));
		break;

	case p_shockwave:
		color[0] = color[1] = color[2] = 64 + (rand() & 31);
		break;

	case p_inferno_flame:
	case p_inferno_trail:
		color[0] = 255;
		color[1] = 77;
		color[2] = 13;
		break;

	case p_sparkray:
		color[0] = 255;
		color[1] = 102;
		color[2] = 25;
		break;

	case p_dpsmoke:
		color[0] = color[1] = color[2] = 48 + (((rand() & 0xFF) * 48) >> 8);
		break;

	case p_dpfire:
		color[0] = 160 + (((rand() & 0xFF) * 48) >> 8);
		color[1] = 16 + (((rand() & 0xFF) * 148) >> 8);
		color[2] = 16 + (((rand() & 0xFF) * 16) >> 8);
		break;

	case p_blood1:
	case p_blood2:
		color[0] = color[1] = color[2] = 180 + (rand() & 63);
		color[3] = 200;
		break;

	case p_blood3:
		color[0] = (50 + (rand() & 31));
		color[1] = color[2] = 0;
		color[3] = 200;
		break;
	
	case p_flame:
		color[0] = 255;
		color[1] = 100;
		color[2] = 25;
		color[3] = 128;
		break;

	case p_lavatrail:
		color[0] = 255;
		color[1] = 102;
		color[2] = 25;
		color[3] = 255;
		break;

	default:
		color[0] = color[1] = color[2] = 255;//White
		break;
	}

	return color;
}


#define ADD_PARTICLE_TEXTURE(_ptex, _texnum, _texindex, _components, _s1, _t1, _s2, _t2)\
do {											\
	particle_textures[_ptex].texnum = _texnum;					\
	particle_textures[_ptex].components = _components;				\
	particle_textures[_ptex].coords[_texindex][0] = (_s1 + 1) / 256.0;		\
	particle_textures[_ptex].coords[_texindex][1] = (_t1 + 1) / 256.0;		\
	particle_textures[_ptex].coords[_texindex][2] = (_s2 - 1) / 256.0;		\
	particle_textures[_ptex].coords[_texindex][3] = (_t2 - 1) / 256.0;		\
} while(0)

#define ADD_PARTICLE_TYPE(_id, _drawtype, _SrcBlend, _DstBlend, _texture, _startalpha, _grav, _accel, _move, _custom)\
do {\
	particle_types[count].id = (_id);\
	particle_types[count].drawtype = (_drawtype);\
	particle_types[count].SrcBlend = (_SrcBlend);\
	particle_types[count].DstBlend = (_DstBlend);\
	particle_types[count].texture = (_texture);\
	particle_types[count].startalpha = (_startalpha);\
	particle_types[count].grav = 9.8 * (_grav);\
	particle_types[count].accel = (_accel);\
	particle_types[count].move = (_move);\
	particle_types[count].custom = (_custom);\
	particle_type_index[_id] = count;\
	count++;\
} while(0)

void QMB_AllocParticles (void) 
{
	extern cvar_t gl_particle_count;

	r_numparticles = bound(ABSOLUTE_MIN_PARTICLES, gl_particle_count.value, ABSOLUTE_MAX_PARTICLES);

	if (particles || r_numparticles < 1)
		Sys_Error("QMB_AllocParticles: internal error");

	// can't alloc on Hunk, using native memory
	particles = (particle_t *) Q_malloc (r_numparticles * sizeof(particle_t));
	memset (particles, 0, r_numparticles * sizeof(particle_t));//FIXED! 2.011 phew!
}

void QMB_InitParticles (void)
{
	int	i, count = 0, particleimage;

	if (!host_initialized) 
	{
		Cvar_RegisterVariable (&gl_bounceparticles);
		Cvar_RegisterVariable (&gl_flametype);
		Cvar_RegisterVariable (&gl_particle_count);
	}

	if (!(particleimage = GL_LoadTextureImage("textures/particles/particlefont", "qmb:particlefont", 256, 256, TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN)))
		return;

	QMB_AllocParticles ();

	ADD_PARTICLE_TEXTURE(ptex_none, 0, 0, 1, 0, 0, 0, 0);
//	ADD_PARTICLE_TEXTURE(ptex_blood1, particleimage, 0, 1, 0, 0, 64, 64);
//	ADD_PARTICLE_TEXTURE(ptex_blood2, particleimage, 0, 1, 64, 0, 128, 64);
	ADD_PARTICLE_TEXTURE(ptex_lava, particleimage, 0, 1, 128, 0, 192, 64);
	ADD_PARTICLE_TEXTURE(ptex_blueflare, particleimage, 0, 1, 192, 0, 256, 64);
	ADD_PARTICLE_TEXTURE(ptex_generic, particleimage, 0, 1, 0, 96, 96, 192);
	ADD_PARTICLE_TEXTURE(ptex_smoke, particleimage, 0, 1, 96, 96, 192, 192);
	ADD_PARTICLE_TEXTURE(ptex_blood3, particleimage, 0, 1, 192, 96, 256, 160);
	ADD_PARTICLE_TEXTURE(ptex_bubble, particleimage, 0, 1, 192, 160, 224, 192);
	
	for (i=0 ; i<8 ; i++)
		ADD_PARTICLE_TEXTURE(ptex_dpsmoke, particleimage, i, 8, i * 32, 64, (i + 1) * 32, 96);

	// load the rest of the images
	if (!(particleimage = GL_LoadTextureImage("textures/particles/inferno", "qmb:inferno", 0, 0, TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN)))
		return;
	ADD_PARTICLE_TEXTURE(ptex_flame, particleimage, 0, 1, 0, 0, 256, 256);

	if (!(particleimage = GL_LoadTextureImage("textures/particles/zing1", "qmb:lightning", 0, 0, TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN)))
		return;	
	ADD_PARTICLE_TEXTURE(ptex_lightning, particleimage, 0, 1, 0, 0, 256, 128);//R00k changed
	
	if (!(particleimage = GL_LoadTextureImage("textures/muzzleflash", "qmb:muzzleflash", 128, 128, TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN)))
		return;	
	ADD_PARTICLE_TEXTURE(ptex_muzzleflash, particleimage, 0, 1, 0, 0, 256, 256);

	if (!(particleimage = GL_LoadTextureImage("textures/muzzleflash8", "qmb:muzzleflash8", 128, 128, TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN)))
		return;	
	ADD_PARTICLE_TEXTURE(ptex_muzzleflash8, particleimage, 0, 1, 0, 0, 256, 256);

	if (!(particleimage = GL_LoadTextureImage("textures/particles/bloodcloud", "qmb:bloodcloud", 128, 128, TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN)))
		return;	
	ADD_PARTICLE_TEXTURE(ptex_bloodcloud, particleimage, 0, 1, 0, 0, 256, 256);

	if (!(particleimage = GL_LoadTextureImage("textures/particles/blood1", "qmb:blood1", 0, 0, TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN)))
		return;	
	ADD_PARTICLE_TEXTURE(ptex_blood1, particleimage, 0, 1, 0, 0, 256, 256);

	if (!(particleimage = GL_LoadTextureImage("textures/particles/blood2", "qmb:blood2", 0, 0, TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN)))
		return;	
	ADD_PARTICLE_TEXTURE(ptex_blood2, particleimage, 0, 1, 0, 0, 256, 256);

	if (!(particleimage = GL_LoadTextureImage("textures/particles/fly", "qmb:fly", 0, 0, TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN)))
		return;	
	ADD_PARTICLE_TEXTURE(ptex_fly, particleimage, 0, 1, 0, 0, 256, 256);

	if (!(particleimage = GL_LoadTextureImage("textures/particles/dirt", "qmb:dirt", 0, 0, TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN)))
		return;

	ADD_PARTICLE_TEXTURE(ptex_chunk, particleimage, 0, 1, 0, 0, 256, 256);

//	ADD_PARTICLE_TYPE(_id,				_drawtype,			_SrcBlend,		_DstBlend,				_texture,	_startalpha,	_grav, _accel,  _move,			_custom)\	
	ADD_PARTICLE_TYPE(p_spark,			pd_spark,			GL_SRC_ALPHA,	GL_ONE,					ptex_none,			255,	-32,	0,		pm_bounce,		1.3);	
	ADD_PARTICLE_TYPE(p_gunblast,		pd_spark,			GL_SRC_ALPHA,	GL_ONE,					ptex_none,			255,	-16,	0,		pm_bounce,		1.3);	
	ADD_PARTICLE_TYPE(p_sparkray,		pd_sparkray,		GL_SRC_ALPHA,	GL_ONE,					ptex_none,			255,	-0,		0,		pm_nophysics,	0);
	ADD_PARTICLE_TYPE(p_fire,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_generic,		204,	0,		-2.95,	pm_die,			0);
	ADD_PARTICLE_TYPE(p_fire2,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_dpsmoke,		204,	0,		-2.95,	pm_die,			0);
	ADD_PARTICLE_TYPE(p_chunk,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE_MINUS_SRC_ALPHA,	ptex_chunk	,		255,	-16,	0,		pm_bounce,		1.475);
	ADD_PARTICLE_TYPE(p_shockwave,		pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_generic,		255,	0,		-4.85,	pm_nophysics,	0);
	ADD_PARTICLE_TYPE(p_inferno_flame,	pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_generic,		153,	0,		0,		pm_static,		0);
	ADD_PARTICLE_TYPE(p_inferno_trail,	pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_generic,		204,	0,		0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_trailpart,		pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_generic,		230,	0,		0,		pm_static,		0);
	ADD_PARTICLE_TYPE(p_smoke,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_smoke,			140,	3,		0,		pm_normal,		0);
	ADD_PARTICLE_TYPE(p_dpfire,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_dpsmoke,		144,	0,		0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_dpsmoke,		pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_dpsmoke,		85,		3,		0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_teleflare,		pd_billboard,		GL_ONE,			GL_ONE,					ptex_blueflare,		255,	0,		0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_flare,			pd_billboard,		GL_ONE,			GL_ONE,					ptex_bubble,		255,	0,		0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_fly,			pd_billboard,		GL_ZERO,		GL_ONE_MINUS_SRC_COLOR,	ptex_fly,			255,	0,		0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_dot,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_generic,		255,	0,		0,		pm_static,		0);
	ADD_PARTICLE_TYPE(p_blood1,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE_MINUS_SRC_ALPHA,	ptex_blood1,		255,	-20,	0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_blood2,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE_MINUS_SRC_ALPHA,	ptex_blood2,		255,	-20,	0,		pm_die,			0.018);
	ADD_PARTICLE_TYPE(p_blood3,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE_MINUS_SRC_ALPHA, ptex_blood3,		255,	-30,	0,		pm_normal,		0);
	ADD_PARTICLE_TYPE(p_lavasplash,		pd_billboard,		GL_SRC_ALPHA,	GL_ONE_MINUS_SRC_ALPHA, ptex_lava,			170,	0,		0,		pm_nophysics,	0);
	ADD_PARTICLE_TYPE(p_bubble,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE_MINUS_SRC_ALPHA, ptex_bubble,		204,	8,		0,		pm_float,		0);
	ADD_PARTICLE_TYPE(p_staticbubble,	pd_billboard,		GL_SRC_ALPHA,	GL_ONE_MINUS_SRC_ALPHA, ptex_bubble,		204,	0,		0,		pm_static,		0);
	ADD_PARTICLE_TYPE(p_flame,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_generic,		200,	10,		0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_lavatrail,		pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_dpsmoke,		255,	3,		0,		pm_normal,		0);//R00k
	ADD_PARTICLE_TYPE(p_bubble2,		pd_billboard,		GL_SRC_ALPHA,	GL_ONE_MINUS_SRC_ALPHA, ptex_bubble,		204,	1,		0,		pm_float,		0);
	ADD_PARTICLE_TYPE(p_glow,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_generic,		204,	0,		0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_alphatrail,		pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_generic,		100,	0,		0,		pm_static,		0);
	ADD_PARTICLE_TYPE(p_torch_flame,	pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_flame,			255,	12,		0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_streak,			pd_hide,			GL_SRC_ALPHA,	GL_ONE,					ptex_none,			255,	-64,	0,		pm_streak,		1.5);
	ADD_PARTICLE_TYPE(p_streakwave,		pd_hide,			GL_SRC_ALPHA,	GL_ONE,					ptex_none,			255,	0,		0,		pm_streakwave,	0);
	ADD_PARTICLE_TYPE(p_streaktrail,	pd_beam,			GL_SRC_ALPHA,	GL_ONE,					ptex_none,			255,	0,		0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_lightningbeam,	pd_beam,			GL_SRC_ALPHA,	GL_ONE,					ptex_lightning,		255,	0,		0,		pm_die,			0);
	ADD_PARTICLE_TYPE(p_muzzleflash,	pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_muzzleflash,	255,	0,		0,		pm_static,		0);
	ADD_PARTICLE_TYPE(p_muzzleflash8,	pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_muzzleflash8,	220,	0,		0,		pm_static,		0);
	ADD_PARTICLE_TYPE(p_rain,			pd_billboard,		GL_SRC_ALPHA,	GL_ONE,					ptex_generic,		255,	-16,	0,		pm_rain,		0);
	ADD_PARTICLE_TYPE(p_bloodcloud,		pd_billboard,		GL_SRC_ALPHA,	GL_ONE_MINUS_SRC_ALPHA,	ptex_bloodcloud,	255,	-10,	0,		pm_normal,		0);
	qmb_initialized = true;
}

#define	INIT_NEW_PARTICLE(_pt, _p, _color, _size, _time)	\
	_p = free_particles;					\
	free_particles = _p->next;				\
	_p->next = _pt->start;					\
	_pt->start = _p;					\
	_p->size = _size;					\
	_p->hit = 0;						\
	_p->start = cl.time;					\
	_p->die = _p->start + _time;				\
	_p->growth = 0;						\
	_p->rotspeed = 0;					\
	_p->texindex = (rand() % particle_textures[_pt->texture].components);	\
	_p->bounces = 0;					\
	VectorCopy(_color, _p->color);

__inline static void AddParticle (part_type_t type, vec3_t org, int count, float size, float time, col_t col, vec3_t dir)
{
	byte			*color;
	int				i, j;
	float			tempSize;
	particle_t		*p;
	particle_type_t	*pt;

	if (!qmb_initialized)
		Sys_Error ("QMB particle added without initialization");

	if (size <= 0)
		return;

	if (time <= 0)
		return;


	if (type < 0 || type >= num_particletypes)
		Sys_Error ("AddParticle: Invalid type (%d)", type);

	pt = &particle_types[particle_type_index[type]];

	for (i=0 ; i < count && free_particles ; i++)
	{
		color = col ? col : ColorForParticle (type);

		INIT_NEW_PARTICLE(pt, p, color, size, time);
				
		switch (type)
		{
		case p_spark:
			p->size = 1.175;
			VectorCopy (org, p->org);
			tempSize = size * 2;
			p->vel[0] = (rand() % (int)tempSize) - ((int)tempSize / 2);
			p->vel[1] = (rand() % (int)tempSize) - ((int)tempSize / 2);
			p->vel[2] = (rand() % (int)tempSize) - ((int)tempSize / 3);
			break;

		case p_smoke:		
			for (j=0 ; j<3 ; j++)
				p->org[j] = org[j] + ((rand() & 31) - 16) / 2.0;
			for (j=0 ; j<3 ; j++)
				p->vel[j] = ((rand() % 10) - 5) / 20.0;
			p->growth = 4.5;
			break;

		case p_fire:
		case p_fire2:
			p->rotspeed = (rand() & 30);
			VectorCopy (org, p->org);
			for (j=0 ; j<3 ; j++)
				p->vel[j] = ((rand() % 160) - 80) * (size / 25.0);
			break;

		case p_bubble:
			p->start += (rand() & 15) / 36.0;
			p->org[0] = org[0] + ((rand() & 31) - 16);
			p->org[1] = org[1] + ((rand() & 31) - 16);
			p->org[2] = org[2] + ((rand() & 63) - 32);
			VectorClear (p->vel);
			break;

		case p_lavasplash:
			VectorCopy (org, p->org);
			VectorCopy (dir, p->vel);
			break;

		case p_gunblast:
			p->size = 1;
			VectorCopy (org, p->org);
			p->vel[0] = (rand() & 127) - 64;
			p->vel[1] = (rand() & 127) - 64;
			p->vel[2] = (rand() & 127) - 10;
			break;

		case p_chunk:
			VectorCopy (org, p->org);
			p->vel[0] = (rand() % 40) - 20;
			p->vel[1] = (rand() % 40) - 20;
			p->vel[2] = (rand() % 40) - 5;			
			break;

		case p_rain:
			VectorCopy(org, p->org);
			p->vel[0] = (rand() % 180) - 90;
			p->vel[1] = (rand() % 180) - 90;
			p->vel[2] = ((rand() % -100) - 1200);
			break;
/*
		case p_snow:
			VectorCopy(org, p->org);
			p->vel[0] = (rand() % 180) - 90;
			p->vel[1] = (rand() % 180) - 90;
			p->vel[2] = ((rand() % -10));
			break;
*/
		case p_streak:
		case p_streakwave:
		case p_shockwave:
			VectorCopy (org, p->org);
			VectorCopy (dir, p->vel);	
			break;

		case p_inferno_trail:
			for (j=0 ; j<3 ; j++)
				p->org[j] = org[j] + (rand() & 15) - 8;
			for (j=0 ; j<3 ; j++)
				p->vel[j] = (rand() & 3) - 2;
			p->growth = -1.5;
			break;

		case p_inferno_flame:
			VectorCopy (org, p->org);
			VectorClear (p->vel);
			p->growth = -30;
			break;

		case p_sparkray:
			VectorCopy (org, p->endorg);
			VectorCopy (dir, p->org);	
			for (j=0 ; j<3 ; j++)
				p->vel[j] = (rand() & 127) - 64;
			p->growth = -16;
			break;

		case p_bloodcloud:
		case p_staticbubble:
			VectorCopy (org, p->org);
			VectorClear (p->vel);
			break;

		case p_muzzleflash:
		case p_muzzleflash8:		
			VectorCopy (org, p->org);			
			p->rotspeed = (rand() & 45) - 90;
			p->size = size * (rand() % 6) / 4;//r00k
			break;

		case p_teleflare:
		case p_flare:
		case p_fly:
			VectorCopy (org, p->org);
			VectorCopy (dir, p->vel);	
			p->growth = 1.75;
			break;
	
		case p_blood1:
		case p_blood2:
			p->size = size * (rand() % 2) + 0.50;//r00k
			for (j=0 ; j<3 ; j++)
				p->org[j] = org[j] + (rand() & 15) - 8;
			for (j=0 ; j<3 ; j++)
				p->vel[j] = (rand() & 63) - 32;
			break;		

		case p_blood3:
			p->size = size * (rand() % 20) / 5.0;
			VectorCopy (org, p->org);
			for (j=0 ; j<3 ; j++)
				p->vel[j] = (rand() % 40) - 20;
			break;

		case p_flame:
			VectorCopy (org, p->org);
			p->growth = -p->size / 2;
			VectorClear (p->vel);
			for (j=0 ; j<2 ; j++)
				p->vel[j] = (rand() % 6) - 3;
			break;

		case p_torch_flame:
			for (j=0 ; j<3 ; j++)
				p->org[j] = org[j] + (rand() & 3) - 2;
			p->vel[0] = rand() % 15 - 8;
			p->vel[1] = rand() % 15 - 8;
			p->vel[2] = rand() % 15;
			p->rotspeed = (rand() & 31) + 32;
			break;

		case p_dot:
		case p_glow:
			VectorCopy (org, p->org);
			VectorCopy (dir, p->vel);	
			p->growth = -1.5;
			break;

		case p_streaktrail:
		case p_lightningbeam:
			VectorCopy(org, p->org);			
			VectorCopy(dir, p->endorg);
			VectorClear(p->vel);
			p->growth = -p->size/time;
			p->bounces = color[3];
			break;

		default:
			break;
		}
	}
}

__inline static void AddParticleTrail (part_type_t type, vec3_t start, vec3_t end, float size, float time, col_t col)
{
	byte			*color;
	int				i, j, num_particles;
	float			count, length;
	vec3_t			point, delta;
	particle_t		*p;
	particle_type_t	*pt;
	static	float	rotangle = 0;

	if (!qmb_initialized)
		Sys_Error ("QMB particle added without initialization");


	if (size <= 0) 
	{
		return;
	}
	
	if (time <= 0)
	{
		return;
	}


	if (type < 0 || type >= num_particletypes)
		Sys_Error ("AddParticle: Invalid type (%d)", type);

	pt = &particle_types[particle_type_index[type]];

	VectorCopy(start, point);
	VectorSubtract(end, start, delta);
	if (!(length = VectorLength(delta)))
		return;

	switch (type)
	{
	case p_alphatrail:
	case p_trailpart:
	case p_lavatrail:
		count = length / 1.1;
		break;

	case p_blood3:
		count = length / 8;
		break;

	case p_bubble:
	case p_bubble2:
		count = length / 8;
		break;

	case p_smoke:
		count = length / 3.8;
		break;

	case p_dpsmoke:
		count = length / 2.5;
		break;

	case p_dpfire:
		count = length / 2.8;
		break;
	
	default:
		break;
	}

	if (!(num_particles = (int)count))
		num_particles = 1;

	VectorScale(delta, 1 / num_particles, delta);

	for (i=0 ; i < num_particles && free_particles ; i++)
	{
		color = col ? col : ColorForParticle (type);
		INIT_NEW_PARTICLE(pt, p, color, size, time);

		switch (type)
		{
		case p_alphatrail:
		case p_trailpart:
			VectorCopy (point, p->org);
			VectorClear (p->vel);
			p->growth = -size / time;
			break;

		case p_blood3:
			VectorCopy (point, p->org);
			for (j=0 ; j<3 ; j++)
				p->org[j] += ((rand() & 15) - 8) / 8.0;

			for (j=0 ; j<3 ; j++)
				p->vel[j] = ((rand() & 15) - 8) / 2.0;

			p->size = size * (rand() % 10) / 5.0;
			p->growth = 2;
			break;

		case p_bubble2:
			VectorCopy(point, p->org);
			for (j=0 ; j<3 ; j++)
				p->vel[j] = (rand() % 10) - 5;
			break;

		//R00k added
		case p_bubble:
			VectorCopy (point, p->org);
			
			for (j=0 ; j<3 ; j++)
				p->org[j] += ((rand() & 15) - 8) / 8.0;
			
			for (j=0 ; j<3 ; j++)
				p->vel[j] = ((rand() & 15) - 8) / 2.0;

			p->size = size * (rand() % 20) / 10.0;			
			p->growth = 1;						
			break;

		case p_smoke:
			VectorCopy (point, p->org);
			for (j=0 ; j<3 ; j++)
				p->org[j] += ((rand() & 7) - 4) / 8.0;
			p->vel[0] = p->vel[1] = 0;
			p->vel[2] = rand() & 3;
			p->growth = 4.5;
			p->rotspeed = (rand() & 63) + 96;
			break;

		case p_dpsmoke:
			VectorCopy (point, p->org);
			for (j=0 ; j<3 ; j++)
				p->vel[j] = (rand() % 10) - 5;
			p->growth = 3;
			p->rotspeed = (rand() & 63) + 96;
			break;

		case p_dpfire:
			VectorCopy (point, p->org);
			for (j=0 ; j<3 ; j++)
				p->vel[j] = (rand() % 40) - 20;
			break;

		case p_lavatrail:
			VectorCopy (point, p->org);
			for (j=0 ; j<3 ; j++)
				p->org[j] += ((rand() & 7) - 4);
			p->vel[0] = p->vel[1] = 0;
			p->vel[2] = rand() & 3;
			break;
		
		default:
			break;
		}

		VectorAdd(point, delta, point);
	}
}

void QMB_ClearParticles (void)
{
	int	i;

	if (!qmb_initialized)
		return;

	Q_free (particles);		// free
	QMB_AllocParticles ();	// and alloc again
	particle_count = 0;
	memset (particles, 0, r_numparticles * sizeof(particle_t));
	free_particles = &particles[0];

	for (i=0 ; i+1 < r_numparticles ; i++)
		particles[i].next = &particles[i + 1];
	particles[r_numparticles-1].next = NULL;

	for (i=0 ; i < num_particletypes ; i++)
		particle_types[i].start = NULL;
}

void QMB_RainSplash (vec3_t org)
{	
	AddParticle (p_smoke, org, 1, 1, 0.2, NULL, vec3_origin);//R00k: shh, i know it's a smoke particle...
}

_inline static void QMB_UpdateParticles(void)
{
	int		i, c;
	float	grav, bounce, frametime, distance[3];
	vec3_t	oldorg, stop, normal;
	particle_type_t	*pt;
	particle_t	*p, *kill;
	extern cvar_t r_explosion_alpha;

	if (!qmb_initialized)
		return;

	frametime = fabs(cl.time - cl.oldtime);

	particle_count = 0;
	grav = sv_gravity.value / 800.0;

	for (i=0 ; i<num_particletypes ; i++)
	{
		pt = &particle_types[i];

		if (pt->start)
		{
			p = pt->start;
			while (p && p->next)
			{
				kill = p->next;
				if (kill->die <= particle_time)
				{
					p->next = kill->next;
					kill->next = free_particles;
					free_particles = kill;
				}
				else
				{
					p = p->next;
				}
			}
			if (pt->start->die <= particle_time)
			{
				kill = pt->start;
				pt->start = kill->next;
				kill->next = free_particles;
				free_particles = kill;
			}
		}

		for (p = pt->start ; (p); p = p->next)
		{
			if (particle_time < p->start)
				continue;
			
			particle_count++;
			
			p->size += p->growth * frametime;
			
			if (p->size <= 0)
			{
				p->die = 0;
				continue;
			}

			VectorCopy (p->org, oldorg);
			VectorSubtract(r_refdef.vieworg,oldorg, distance);		

			if (VectorLength(distance) >= r_farclip.value)
				p->die = 0;
			
			switch (pt->id)
			{
				case p_streaktrail://R00k
				case p_lightningbeam:
					p->color[3] = p->bounces * ((p->die - particle_time) / (p->die - p->start));
					break;
				case p_fire:
				case p_fire2:
					p->color[3] = (pt->startalpha * ((p->die - particle_time) / (p->die - p->start)) * r_explosion_alpha.value);
					break;
				default:
					p->color[3] = pt->startalpha * ((p->die - particle_time) / (p->die - p->start));
					break;
			}

			p->rotangle += p->rotspeed * frametime;

			if (p->hit)
				continue;
			
			p->vel[2] += pt->grav * grav * frametime;
			
			VectorScale (p->vel, 1 + pt->accel * frametime, p->vel);

			switch (pt->move)
			{
			case pm_static:
				break;

			case pm_normal:
				VectorCopy (p->org, oldorg);
				VectorMA (p->org, frametime, p->vel, p->org);

				if (CONTENTS_SOLID == TruePointContents(p->org))
				{
					p->hit = 1;	
										
					if ((pt->id == p_blood3)&&(gl_decal_blood.value) && (decals_enabled) && (particle_mode))
					{
						TraceLineN(oldorg, p->org, stop, normal);

						if ((stop != p->org)&&(VectorLength(stop)!=0))
						{
							vec3_t tangent;
							VectorCopy(stop, p->org);
							VectorCopy(normal, p->vel);						
							CrossProduct(normal,p->vel,tangent);	
							R_SpawnDecal(p->org, normal, tangent, decal_blood3, 16, 0, 1);
						}		
						p->die = 0;
					}
					VectorCopy (oldorg, p->org);
					VectorClear (p->vel);
				}			
			break;

			case pm_float:
				VectorMA (p->org, frametime, p->vel, p->org);
				p->org[2] += p->size + 1;		
				if (!ISUNDERWATER(TruePointContents(p->org)))
					p->die = 0;
				p->org[2] -= p->size + 1;
				break;

			case pm_nophysics:
				VectorMA (p->org, frametime, p->vel, p->org);
				break;

			case pm_die:
				VectorCopy (p->org, oldorg);
				VectorMA (p->org, frametime, p->vel, p->org);				

				if (CONTENTS_SOLID == TruePointContents(p->org))
				{		
					if ((decals_enabled) && (particle_mode))
					{
						TraceLineN(oldorg, p->org, stop, normal);

						if ((stop != p->org)&&(VectorLength(stop)!=0))
						{
							vec3_t tangent;							
							
							VectorCopy(stop, p->org);
							VectorCopy(normal, p->vel);						
							CrossProduct(p->org,p->vel,tangent);	

							if (pt->id == p_fire2)
							{
								if (gl_decal_explosions.value)
								{							
									R_SpawnDecal(p->org, normal, tangent, decal_burn, 50, 0, 0.5f);
								}
							}
							else
							{
								if (pt->id == p_blood1)
								{					
									if (gl_decal_blood.value)
										R_SpawnDecal(p->org, normal, tangent, decal_blood1, 16, 0, 1);								
								}
								else
								{
									if (pt->id == p_blood2)
									{					
										if (gl_decal_blood.value)
											R_SpawnDecal(p->org, normal, tangent, decal_blood2, 12, 0, 1);									
									}
								}
							}
						}					
					}					
					VectorCopy (oldorg, p->org);
					p->die = 0;
				}
				break;

			case pm_bounce:
				if (!gl_bounceparticles.value || p->bounces)
				{
					VectorMA(p->org, frametime, p->vel, p->org);
					if (CONTENTS_SOLID == TruePointContents(p->org))
					{
						p->die = 0;
					}
				}
				else
				{
					VectorCopy (p->org, oldorg);
					VectorMA (p->org, frametime, p->vel, p->org);
					if (CONTENTS_SOLID == TruePointContents(p->org))
					{
						if (TraceLineN(oldorg, p->org, stop, normal))
						{
							VectorCopy (stop, p->org);
							bounce = -pt->custom * DotProduct(p->vel, normal);
							VectorMA(p->vel, bounce, normal, p->vel);
							p->bounces++;
						}
					}
				}
				break;

			case pm_streak:
				VectorCopy(p->org, oldorg);
				VectorMA(p->org, frametime, p->vel, p->org);

/*				if (CONTENTS_SOLID == TruePointContents(p->org))
				{
					if (TraceLineN(oldorg, p->org, stop, normal)) 
					{
						VectorCopy(stop, p->org);
						bounce = -pt->custom * DotProduct(p->vel, normal);
						VectorMA(p->vel, bounce, normal, p->vel);
					}
				}				

				AddParticle (p_streaktrail, oldorg, 1, p->size, 0.2, p->color, p->org);
*/				
				
				if (!VectorLength(p->vel))
					p->die = 0;
				break;

			case pm_rain:
				VectorCopy(p->org, oldorg);
				VectorMA(p->org, frametime, p->vel, p->org);

				VectorSubtract(r_refdef.vieworg, oldorg, distance);		

				if (VectorLength(distance) < r_farclip.value)
				{
					c = TruePointContents(p->org);
	
					if ((CONTENTS_SOLID == c) || (ISUNDERWATER(c)))
					{		
						if ((rand()%3) > 1)
							QMB_RainSplash (p->org);

						VectorClear (p->vel);
						p->die = 0;
					}
					else
						AddParticle (p_streaktrail, oldorg, 1, 1, (rand()%1 + 0.03), p->color, p->org);
				}
				break;

			case pm_streakwave:
				VectorCopy(p->org, oldorg);
				VectorMA(p->org, frametime, p->vel, p->org);
				AddParticle (p_streaktrail, oldorg, 1, p->size, 0.5, p->color, p->org);
				p->vel[0] = 19 * p->vel[0] / 20;
				p->vel[1] = 19 * p->vel[1] / 20;
				p->vel[2] = 19 * p->vel[2] / 20;
				break;

			default:
				break;
			}			
		}
	}
}

//from darkplaces engine - finds which corner of a particle goes where, so I don't have to :D
void R_CalcBeamVerts (float *vert, vec3_t org1, vec3_t org2, float width)
{
	vec3_t right1, right2, diff, normal;

	VectorSubtract (org2, org1, normal);
	VectorNormalize (normal);

	//width = width / 2;
	// calculate 'right' vector for start
	VectorSubtract (r_origin, org1, diff);
	VectorNormalize (diff);
	CrossProduct (normal, diff, right1);

	// calculate 'right' vector for end
	VectorSubtract (r_origin, org2, diff);
	VectorNormalize (diff);
	CrossProduct (normal, diff, right2);

	vert[ 0] = org1[0] + width * right1[0];
	vert[ 1] = org1[1] + width * right1[1];
	vert[ 2] = org1[2] + width * right1[2];
	vert[ 4] = org1[0] - width * right1[0];
	vert[ 5] = org1[1] - width * right1[1];
	vert[ 6] = org1[2] - width * right1[2];
	vert[ 8] = org2[0] - width * right2[0];
	vert[ 9] = org2[1] - width * right2[1];
	vert[10] = org2[2] - width * right2[2];
	vert[12] = org2[0] + width * right2[0];
	vert[13] = org2[1] + width * right2[1];
	vert[14] = org2[2] + width * right2[2];
}

void QMB_DrawParticles (void)
{
	int					i, j, k;
	vec3_t				v, billboard[4], neworg;
	particle_t			*p;
	particle_type_t		*pt;
	particle_texture_t	*ptex;
	float				varray_vertex[16];
	vec3_t				distance;

	if (!qmb_initialized)
		return;

	particle_time = cl.time;

	if (!cl.paused)
		QMB_UpdateParticles ();

	if (gl_fogenable.value)
		glDisable (GL_FOG);

	VectorAdd (vup, vright, billboard[2]);
	VectorSubtract (vright, vup, billboard[3]);
	VectorNegate (billboard[2], billboard[0]);
	VectorNegate (billboard[3], billboard[1]);

	glDepthMask (GL_FALSE);
	glEnable (GL_BLEND);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glShadeModel (GL_SMOOTH);

	for (i = 0 ; i < num_particletypes ; i++)
	{
		pt = &particle_types[i];
		
		if (!pt->start)
			continue;

		glBlendFunc (pt->SrcBlend, pt->DstBlend);

		switch (pt->drawtype)
		{
			case pd_hide:
				break;
			case pd_beam:
				ptex = &particle_textures[pt->texture];

				GL_Bind (ptex->texnum);

				for (p = pt->start ; p ; p = p->next)
				{
					if (particle_time < p->start || particle_time >= p->die)
						continue;

					VectorSubtract(r_refdef.vieworg, p->org, distance);			
					if (VectorLength(distance) > r_farclip.value)
						continue;

					glColor4ubv (p->color);
					R_CalcBeamVerts (varray_vertex, p->org, p->endorg, p->size / 3.0);
					glBegin (GL_QUADS);
						glTexCoord2f (1, 0);
						glVertex3f (varray_vertex[0], varray_vertex[1], varray_vertex[2]);
						glTexCoord2f (1, 1);
						glVertex3f (varray_vertex[4], varray_vertex[5], varray_vertex[6]);
						glTexCoord2f (0, 1);
						glVertex3f (varray_vertex[8], varray_vertex[9], varray_vertex[10]);
						glTexCoord2f (0, 0);
						glVertex3f (varray_vertex[12], varray_vertex[13], varray_vertex[14]);
					glEnd ();
				}
				break;

			case pd_spark:
				glDisable (GL_TEXTURE_2D);
				for (p = pt->start ; p ; p = p->next)
				{
					if (particle_time < p->start || particle_time >= p->die)
						continue;

					VectorSubtract(r_refdef.vieworg, p->org, distance);			
					if (VectorLength(distance) > r_farclip.value)
						continue;

					glBegin (GL_TRIANGLE_FAN);
						glColor4ubv (p->color);
						glVertex3fv (p->org);
						glColor4ub (p->color[0] >> 1, p->color[1] >> 1, p->color[2] >> 1, 0);
						for (j=7 ; j>=0 ; j--)
						{
							for (k=0 ; k<3 ; k++)
								v[k] = p->org[k] - p->vel[k] / 8 + vright[k] * cost[j%7] * p->size + vup[k] * sint[j%7] * p->size;
							glVertex3fv (v);
						}
					glEnd ();
				}
				glEnable (GL_TEXTURE_2D);
				break;

			case pd_sparkray:
				glDisable (GL_TEXTURE_2D);
				for (p = pt->start ; p ; p = p->next) 
				{
					if (particle_time < p->start || particle_time >= p->die)
						continue;

					VectorSubtract(r_refdef.vieworg, p->org, distance);			
					if (VectorLength(distance) > r_farclip.value)
						continue;

					if (!TraceLineN(p->endorg, p->org, neworg, NULLVEC))
						VectorCopy(p->org, neworg);

					glBegin (GL_TRIANGLE_FAN);
						//R00k added -start-
						glEnable (GL_BLEND);
						p->color[3] = bound(0, 0.3, 1) * 255;
						//R00k added -end-
						glColor4ubv (p->color);
						glVertex3fv (p->endorg);
						glColor4ub (p->color[0] >> 1, p->color[1] >> 1, p->color[2] >> 1, 0);
						for (j=7 ; j>=0 ; j--)
						{
							for (k=0 ; k<3 ; k++)
								v[k] = neworg[k] + vright[k] * cost[j%7] * p->size + vup[k] * sint[j%7] * p->size;
							glVertex3fv (v);
						}
						glDisable (GL_BLEND);//R00k
					glEnd ();
				}
				glEnable (GL_TEXTURE_2D);
				break;

			case pd_billboard:
				ptex = &particle_textures[pt->texture];
				
				GL_Bind (ptex->texnum);
				
				for (p = pt->start ; p ; p = p->next)
				{
					if (particle_time < p->start || particle_time >= p->die)
						continue;

					VectorSubtract(r_refdef.vieworg, p->org, distance);			
					if (VectorLength(distance) > r_farclip.value)
						continue;

					glPushMatrix ();									
					glTranslatef (p->org[0], p->org[1], p->org[2]);				
					glScalef (p->size, p->size, p->size);					
											
					if (p->rotspeed)								
						glRotatef (p->rotangle, vpn[0], vpn[1], vpn[2]);			

					glColor4ubv (p->color);																						
					glBegin (GL_QUADS);						
						glTexCoord2f (ptex->coords[p->texindex][0], ptex->coords[p->texindex][3]); glVertex3fv(billboard[0]);	
						glTexCoord2f (ptex->coords[p->texindex][0], ptex->coords[p->texindex][1]); glVertex3fv(billboard[1]);	
						glTexCoord2f (ptex->coords[p->texindex][2], ptex->coords[p->texindex][1]); glVertex3fv(billboard[2]);	
						glTexCoord2f (ptex->coords[p->texindex][2], ptex->coords[p->texindex][3]); glVertex3fv(billboard[3]);	
					 
					glEnd ();																									
					glPopMatrix ();
				}				
				 
			break;
			default:
				break;
		}
	}

	glDepthMask (GL_TRUE);
	glDisable (GL_BLEND);

	if (gl_fogenable.value)
		glEnable (GL_FOG);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glShadeModel (GL_FLAT);
}

void QMB_Shockwave_Splash(vec3_t org, int radius)
{
	float	theta;
	vec3_t	angle;

	angle[2] = 0;

  //for (theta = 0; theta < 2 * M_PI; theta += 2 * M_PI / 90) 
	for (theta = 0; theta < 6.283185307179586476925286766559; theta += 0.069813170079773183076947630739545)
	{
		angle[0] = cos(theta) * radius;
		angle[1] = sin(theta) * radius;		
		AddParticle(p_shockwave, org, 1, 2, 0.625f, NULL, angle);
	}
}

void QMB_LetItRain(void)
{
	int				i;
	msurface_t		*surf;
	glpoly_t		*p;
	col_t			color;
	float			distance[3], point[3];
	float			frametime = fabs(cl.ctime - cl.oldtime);
	extern sfx_t	*cl_sfx_thunder;

	if (!qmb_initialized)
		return;

	if (!particle_mode)
		return;

	if (cl.time < cl.rain_time)	
	{
		return;
	}

	cl.rain_time = cl.time + 0.05;//change to ticrate
	
	color[0] =  color[1] = color[2] = + (rand() % 25 + 15);
	color[2] += 5;

	if (gl_fogenable.value)
	{
		gl_fogred.value = gl_foggreen.value = gl_fogblue.value = 0.2;
	}

	for (i = 0, surf = cl.worldmodel->surfaces; i < cl.worldmodel->numsurfaces; i++, surf++)
	{
		if (!(surf->flags & SURF_DRAWSKY))
			continue;

		for (p = surf->polys;  p; p = p->next)
		{	
			VectorSubtract(r_refdef.vieworg,p->midpoint, distance);		

			if ((VectorLength(distance) > r_farclip.value))
				continue;

			//R00k let's make some thunder...
			if (cl.time > cl.thundertime)
			{
				S_StartSound (-1, 0, cl_sfx_thunder, p->midpoint, 1, 0);
				cl.thundertime = cl.time + (rand()% 60 + 1);
			}

			point[0] = p->midpoint[0] + (rand() % 80 - 40);
			point[1] = p->midpoint[1] + (rand() % 80 - 40);
			point[2] = p->midpoint[2] + p->fxofs;
			
			AddParticle(p_rain, point, 1, 0.035, 5, color, zerodir);		
		}		
	}
};

//R00k: revamped to coincide with classic particle style...

void QMB_ParticleExplosion (vec3_t org)
{
	if (r_explosiontype.value == 2)//no explosion what so ever
		return;

	if (ISUNDERWATER(TruePointContents(org)))
	{
		AddParticle (p_fire, org, 8, 8, 0.8, NULL, zerodir);
		AddParticle (p_bubble, org, 50, 1.0, 0.825f + ((rand() % 10) - 5) / 40.0, NULL, zerodir);
	}
	else
	{
		if (r_explosiontype.value != 3)		
		{
			if (gl_flametype.value < 1)//R00k: default 0
			{
				AddParticle (p_fire2, org, 24, 18, 1, NULL, zerodir);			
			}
			else
			{
				AddParticle (p_fire, org, 16, 18, 1, NULL, zerodir);
			}
		}

		if ((r_explosiontype.value == 0) || (r_explosiontype.value == 1) || (r_explosiontype.value == 3))
		{
			AddParticle (p_spark, org, 25, 150, 0.825f, NULL, zerodir);
		}
	}
}

void d8to24col (col_t colourv, int colour)
{
	byte	*colourByte;

	colourByte = (byte *)&d_8to24table[colour];
	colourv[0] = colourByte[0];
	colourv[1] = colourByte[1];
	colourv[2] = colourByte[2];
}

__inline static void AddColoredParticle (part_type_t type, vec3_t org, int count, float size, float time, int colorStart, int colorLength, vec3_t dir)
{
	col_t		color;
	int		i, j, colorMod = 0;
	float		tempSize;
	particle_t	*p;
	particle_type_t	*pt;


	if (!qmb_initialized)
		Sys_Error ("QMB particle added without initialization");

	if (size < 0) 
	{
		return;
	}
	
	if (time < 0)
	{
		return;
	}

	if (type < 0 || type >= num_particletypes)
		Sys_Error ("AddColoredParticle: Invalid type (%d)", type);

	pt = &particle_types[particle_type_index[type]];

	for (i=0 ; i < count && free_particles ; i++)
	{
		d8to24col (color, colorStart + (colorMod % colorLength));
		colorMod++;
		INIT_NEW_PARTICLE(pt, p, color, size, time);

		switch (type)
		{
		case p_spark:
			p->size = 1.175;
			VectorCopy (org, p->org);
			tempSize = size * 2;
			p->vel[0] = (rand() % (int)tempSize) - ((int)tempSize / 4);
			p->vel[1] = (rand() % (int)tempSize) - ((int)tempSize / 4);
			p->vel[2] = (rand() % (int)tempSize) - ((int)tempSize / 6);
			break;

		case p_fire:
			VectorCopy (org, p->org);
			for (j=0 ; j<3 ; j++)
				p->vel[j] = ((rand() % 160) - 80) * (size / 25.0);
			break;

		default:
			//assert (!"AddColoredParticle: unexpected type");
			break;
		}
	}
}

void QMB_ColorMappedExplosion (vec3_t org, int colorStart, int colorLength)
{
	if (ISUNDERWATER(TruePointContents(org)))
	{
		//AddColoredParticle (p_fire, org, 16, 18, 1, colorStart, colorLength, zerodir);
		AddParticle (p_bubble, org, 6, 3.0, 2.5, NULL, zerodir);
		AddParticle (p_bubble, org, 4, 2.35, 2.5, NULL, zerodir);
		if (r_explosiontype.value != 2)
		{
			AddColoredParticle (p_spark, org, 50, 100, 0.5, colorStart, colorLength, zerodir);
			AddColoredParticle (p_spark, org, 25, 60, 0.5, colorStart, colorLength, zerodir);
		}
	}
	else
	{
		AddColoredParticle (p_fire, org, 16, 18, 1, colorStart, colorLength, zerodir);

		if (r_explosiontype.value < 2)
		{			
			AddColoredParticle (p_spark, org, 50, 250, 0.625f, colorStart, colorLength, zerodir);
			if (r_explosiontype.value < 1)
				AddColoredParticle (p_spark, org, 25, 150, 0.625f, colorStart, colorLength, zerodir);
		}
	}
}

void QMB_Blood_Splat(part_type_t type, vec3_t org) //R00k :)
{
	int			j;
	col_t		color;
	vec3_t		neworg, angle;
	
	VectorClear (angle);

	color[0]=100;
	color[1]=0;
	color[2]=0;
	color[3]=255;

	AddParticle(p_bloodcloud, org, 1, 6, 0.5, color, zerodir);

	for (j=0 ; j<4 ; j++)
	{
		AngleVectors (angle, NULLVEC, NULLVEC, neworg);
		VectorMA (org, 70, neworg, neworg);

		AddParticle (type, org, 5, 1, 2, color, neworg);
		angle[1] += 360 / 4;
	}
} 

void QMB_RunParticleEffect (vec3_t org, vec3_t dir, int col, int count)
{
	col_t	color;
	vec3_t	neworg, newdir;
	int		i, j, particlecount;
	int		contents;//R00k Added	
	
	if (col == 73)
	{			
		QMB_Blood_Splat(p_blood1, org);
		QMB_Blood_Splat(p_blood2, org);
		return;
	}
	else if (col == 225)
	{		
		QMB_Blood_Splat(p_blood3, org);
		return;
	}
	else if (col == 20 && count == 30)
	{
		color[0] = color[2] = 51;
		color[1] = 255;
		AddParticle (p_chunk, org, 1, (1 * (rand() % 1) + 0.5), 5, NULL, zerodir);
		//AddParticle (p_spark, org, 1, 70, 0.2, NULL, zerodir);					
		return;
	}
	else if (col == 226 && count == 20)
	{
		color[0] = 230;
		color[1] = 204;
		color[2] = 26;
		AddParticle (p_chunk, org, 5, (0.5 * (rand() % 1) + 0.5), 5, NULL, zerodir);
		//AddParticle (p_spark, org, 1, 70, 0.2, NULL, zerodir);			
		return;
	}

	switch (count)
	{
	case 9:
	case 10://nailgun
		if (nehahra && count == 10)	// ventillation's wind
		{
			for (i=0 ; i<count ; i++)
			{
				for (j=0 ; j<3 ; j++)
				{
					neworg[j] = org[j] + ((rand() % 24) - 12);
					newdir[j] = dir[j] * (10 + (rand() % 5));
				}
				d8to24col (color, (col & ~7) + (rand() & 7));
				AddParticle (p_glow, neworg, 1, 3, 0.3 + 0.1 * (rand() % 3), color, newdir);
			}
		}
		else
		{
			color[0] = 200;	color[1] = 200;	color[2] = 125;			

			AddParticle (p_spark, org, 1, 70, 0.2, color, zerodir);			
			AddParticle (p_chunk, org, 1, (0.5 * (rand() % 1) + 0.5), 5, NULL, zerodir);

			contents = TruePointContents (org);//R00k Added
			if (ISUNDERWATER(contents))//R00k
			{							
				AddParticle (p_bubble, org, 1, 2, 0.825f + ((rand() % 20) - 5) / 40.0, NULL, zerodir);
			}
			else
			{	
				AddParticle (p_smoke, org, 1, 4, 0.825f + ((rand() % 20) - 5) / 40.0, NULL, zerodir);
			}
		}
		break;

	case 20://super nailgun
		color[0] = 200;	color[1] = 200;	color[2] = 125;		
		AddParticle (p_spark, org, 2, 85, 0.4, color, zerodir);
		AddParticle (p_chunk, org, 5, (0.5 * (rand() % 1) + 0.5), 5, NULL, zerodir);

		contents = TruePointContents (org);//R00k Added
		if (ISUNDERWATER(contents))//R00k
		{							
			AddParticle (p_bubble, org, 1, 2, 0.825f + ((rand() % 10) - 5) / 40.0, NULL, zerodir);
		}
		else
		{	
			AddParticle (p_smoke, org, 1, 4, 0.825f + ((rand() % 10) - 5) / 40.0, NULL, zerodir);
		}
		break;

	case 24:// gunshot

		particlecount = count >> 1;
		//AddParticle (p_gunblast, org, 1, 1.04, 0.47, NULL, zerodir);
		for (i=0 ; i<particlecount ; i++)
		{			
			for (j=0 ; j<3 ; j++)
				neworg[j] = org[j] + ((rand() & 15) - 8);
			
			contents = TruePointContents (neworg);//R00k Added

			if (ISUNDERWATER(contents))//R00k
			{							
				AddParticle (p_bubble, neworg, 1, 2, 0.825f + ((rand() % 10) - 5) / 40.0, NULL, zerodir);
			}
			else
			{	
				AddParticle (p_smoke, neworg, 1, 2, 0.425f + ((rand() % 10) - 5) / 40.0, NULL, zerodir);
			}
			
			if ((i % particlecount) == 0)
			{
				AddParticle (p_chunk, neworg, 4, (0.5 * (rand() % 1) + 0.5), 3.75, NULL, zerodir);
			}
		}
		break;

	case 30:
		AddParticle (p_chunk, org, 10, (0.5 * (rand() % 1) + 0.5), 4, NULL, zerodir);
		AddParticle (p_spark, org, 1, 70, 0.2, NULL, zerodir);			
		//AddParticle (p_spark, org, 8, 105, 0.9, NULL, zerodir);

		break;

	case 128:	// electric sparks (R00k added from QMB)
		color[2] = 1.0f;
		for (i=0; i<count; i++)
		{
			color[0] = color[1] = 0.4 + ((rand()%90)/255.0);
			AddParticle (p_spark, org, 1, 100, 1.0f,  color, zerodir);//modified
		}		
		break;

	default:
		if (hipnotic)
		{
			switch (count)
			{
			case 1:		//particlefields
			case 2:
			case 3:
			case 4:
				d8to24col (color, (col & ~7) + (rand() & 7));
				AddParticle (p_gunblast, org, 15, 5, 0.1 * (rand() % 5), color, zerodir);
				return;
			}
		}
		else if (nehahra)
		{
			switch (count)
			{
			case 25:	// slime barrel chunks
				return;

			case 244:	// sewer
				for (i=0 ; i<(count>>3) ; i++)
				{
					for (j=0 ; j<3 ; j++)
						neworg[j] = org[j] + ((rand() % 24) - 12);
					newdir[0] = dir[0] * (10 + (rand() % 5));
					newdir[1] = dir[1] * (10 + (rand() % 5));
					newdir[2] = dir[2] * 15;
					d8to24col (color, (col & ~7) + (rand() & 7));
					AddParticle (p_glow, neworg, 1, 3.5, 0.5 + 0.1 * (rand() % 3), color, newdir);
				}
				return;
			}
		}

		particlecount = max(1, count>>1);
		for (i=0 ; i<particlecount ; i++)
		{
			for (j=0 ; j<3 ; j++)
			{
				neworg[j] = org[j] + ((rand() % 20) - 10);
				newdir[j] = dir[j] * (10 + (rand() % 5));
			}
			d8to24col (color, (col & ~7) + (rand() & 7));
			AddParticle (p_glow, neworg, 1, 3, 0.2 + 0.1 * (rand() % 4), color, newdir);
		}
		break;
	}
}

//R00k added particle muzzleflashes
void QMB_MuzzleFlash(vec3_t org)
{
	if (!qmb_initialized)
		return;

	if (!particle_mode)
		return;

	if (!(ISUNDERWATER(TruePointContents(org))))
	{
		AddParticle (p_muzzleflash, org, 1, 8, 0.04, NULL, zerodir);
	}
	else
	{
		AddParticle (p_bubble, org, 12, 1.0, 0.825f + ((rand() % 10) - 5) / 40.0, NULL, zerodir);
	}
}

void QMB_MuzzleFlashLG(vec3_t org)
{
	float	frametime = fabs(cl.ctime - cl.oldtime);
	col_t	color;

	color[0] = color[1] = 20;
	color[2] = 200;

	if (!qmb_initialized)
		return;

	if (!particle_mode)
		return;

	if (!(ISUNDERWATER(TruePointContents(org))))
	{
		AddParticle (p_muzzleflash8, org, 1, 8, 0.4 * frametime, color, zerodir);
	}
}

void QMB_RocketTrail (vec3_t start, vec3_t end, trail_type_t type)
{
	col_t		color;

	switch (type)
	{
	case GRENADE_TRAIL://r00K mODIFIED
		if (ISUNDERWATER(TruePointContents(start)))
		{
			AddParticleTrail (p_bubble, start, end, 1, 0.30, NULL);							
		}
		else
		{				
			if (gl_part_trails.value > 1)
			{			
				color[0] = 15; color[1] = 15; color[2] = 10;
				AddParticleTrail (p_alphatrail, start, end, 8, 1, color);				
			}
			else
				AddParticleTrail (p_smoke, start, end, 1.45, 0.825, NULL);
		}
		break;

	case BLOOD_TRAIL:
	case SLIGHT_BLOOD_TRAIL:
		AddParticleTrail (p_blood3, start, end, type == BLOOD_TRAIL ? 1.35 : 2.4, 2, NULL);
		break;

	case TRACER1_TRAIL:
		color[0] = color[2] = 0;
		color[1] = 124;
		AddParticleTrail (p_trailpart, start, end, 3.75, 0.5, color);
		break;

	case TRACER2_TRAIL:
		color[0] = 255;
		color[1] = 77;
		color[2] = 0;
		AddParticleTrail (p_trailpart, start, end, 1.75, 0.2, color);
		break;

	case VOOR_TRAIL:
		color[0] = 77;
		color[1] = 0;
		color[2] = 255;
		AddParticleTrail (p_trailpart, start, end, 3.75, 0.5, color);
		break;

	case ALT_ROCKET_TRAIL:
		if (ISUNDERWATER(TruePointContents(start)))
		{
			AddParticleTrail (p_bubble, start, end, 1, 2.5, NULL);								
		}
		else
		{
			if (gl_part_trails.value > 1)
			{			
				color[0] = 15; color[1] = 15; color[2] = 10;
				AddParticleTrail (p_alphatrail, start, end, 8, 1, color);				
			}
			else
			{			
				AddParticleTrail (p_dpfire, start, end, 3, 0.26, NULL);
				AddParticleTrail (p_dpsmoke, start, end, 3, 0.825, NULL);
			}
		}
		break;

	case LAVA_TRAIL:
		AddParticleTrail (p_lavatrail, start, end, 5, 0.25, NULL);		
		AddParticleTrail (p_dpsmoke, start, end, 5, 0.825, NULL);
		break;

	case BUBBLE_TRAIL:
		if (ISUNDERWATER(TruePointContents(start)))
			AddParticleTrail (p_bubble2, start, end, 1.5, 0.825, NULL);
		break;

	case NEHAHRA_SMOKE:
		AddParticleTrail (p_smoke, start, end, 0.8, 0.825, NULL);
		break;

	case ROCKET_TRAIL:
	default:
		color[0] = 255;
		color[1] = 56;
		color[2] = 9;
//		AddParticleTrail (p_trailpart, start, end, 6.2, 0.31, color);
		if (ISUNDERWATER(TruePointContents(start)))
		{
			AddParticleTrail (p_trailpart, start, end, 1, 0.30, color);				
			AddParticleTrail (p_bubble, start, end, 1, 2.5, NULL);				
		}
		else
		{
			if (gl_part_trails.value > 1)
			{			
				color[0] = 15; color[1] = 15; color[2] = 10;
				AddParticleTrail (p_alphatrail, start, end, 8, 3, color);				
			}
			else
			{			
				AddParticleTrail (p_trailpart, start, end, 6.2, 0.31, color);
				AddParticleTrail (p_smoke, start, end, 1.8, 0.825, NULL);
			}
		}
		break;
	case NAIL_TRAIL://R00k added
		if (ISUNDERWATER(TruePointContents(start)))
		{
			AddParticleTrail (p_bubble, start, end, 0.25, 0.50, NULL);
		}
		else
		{			
			color[0] = 15; color[1] = 15; color[2] = 15;
			AddParticleTrail (p_alphatrail, start, end, 1, 0.25, color);				
		}
		break;
	}
}
/*
void QMB_AxeSwoosh(vec3_t start)
{
		col_t		color;
		vec3_t		end, mid;

		mid[0] = 15;mid[1] = 15;mid[2] = 15;
		VectorSubtract(start,mid,end);
		
		if (ISUNDERWATER(TruePointContents(start)))
		{
			AddParticleTrail (p_bubble, start, end, 0.25, 0.50, NULL);
		}
		else
		{			
			color[0] = 255; color[1] = 255; color[2] = 255;
			AddParticleTrail (p_alphatrail, start, end, 50, 0.5, color);				
		//	AddParticle (p_spark, start, 50, 250, 0.925f, NULL, zerodir);
		}
};
*/

void QMB_BlobExplosion (vec3_t org)
{
	float	theta;
	col_t	color;
	vec3_t	neworg, vel;

	color[0] = 60;
	color[1] = 100;
	color[2] = 240;
	AddParticle (p_spark, org, 44, 250, 1.15, color, zerodir);

	color[0] = 90;
	color[1] = 47;
	color[2] = 207;
	AddParticle (p_fire, org, 15, 30, 1.4, color, zerodir);

	vel[2] = 0;
	for (theta = 0 ; theta < 6.28318530717958647692528676655901 ; theta += 0.0897597901025655210989326680937001)
	{
		color[0] = (60 + (rand() & 15));
		color[1] = (65 + (rand() & 15));
		color[2] = (200 + (rand() & 15));

		vel[0] = cos(theta) * 125;
		vel[1] = sin(theta) * 125;
		neworg[0] = org[0] + cos(theta) * 6;
		neworg[1] = org[1] + sin(theta) * 6;
		neworg[2] = org[2] + 0 - 10;
		AddParticle (p_shockwave, neworg, 1, 4, 0.8, color, vel);
		neworg[2] = org[2] + 0 + 10;
		AddParticle (p_shockwave, neworg, 1, 4, 0.8, color, vel);

		vel[0] *= 1.15;
		vel[1] *= 1.15;
		neworg[0] = org[0] + cos(theta) * 13;
		neworg[1] = org[1] + sin(theta) * 13;
		neworg[2] = org[2] + 0;
		AddParticle (p_shockwave, neworg, 1, 6, 1.0, color, vel);
	}
}

void QMB_LavaSplash (vec3_t org)
{
	int	i, j;
	float	vel;
	vec3_t	dir, neworg;

	for (i=-16 ; i<16; i++)
	{
		for (j=-16 ; j<16 ; j++)
		{
			dir[0] = j * 8 + (rand() & 7);
			dir[1] = i * 8 + (rand() & 7);
			dir[2] = 256;

			neworg[0] = org[0] + dir[0];
			neworg[1] = org[1] + dir[1];
			neworg[2] = org[2] + (rand() & 63);

			VectorNormalizeFast (dir);
			vel = 50 + (rand() & 63);
			VectorScale (dir, vel, dir);

			AddParticle (p_lavasplash, neworg, 1, 4.5, 2.6 + (rand() & 31) * 0.02, NULL, dir);
		}
	}
}

void QMB_TeleportSplash (vec3_t org)
{
	int		i, j, k;
	vec3_t		neworg, angle;
	col_t		color;

	QMB_Shockwave_Splash(org, 120);
	for (i=-12 ; i<=12 ; i+=6)
	{
		for (j=-12 ; j<=12 ; j+=6)
		{
			for (k=-24 ; k<=32 ; k+=8)
			{
				neworg[0] = org[0] + i + (rand() & 3) - 1;
				neworg[1] = org[1] + j + (rand() & 3) - 1;
				neworg[2] = org[2] + k + (rand() & 3) - 1;
				angle[0] = (rand() & 15) - 7;
				angle[1] = (rand() & 15) - 7;
				angle[2] = (rand() % 160) - 80;
				AddParticle (p_teleflare, neworg, 1, 1.8, 0.30 + (rand() & 7) * 0.02, NULL, angle);
			}
		}
	}

	VectorSet (color, 140, 140, 255);
	VectorClear (angle);
	for (i=0 ; i<5 ; i++)
	{
		angle[2] = 0;
		for (j=0 ; j<5 ; j++)
		{
			AngleVectors (angle, NULLVEC, NULLVEC, neworg);
			VectorMA (org, 70, neworg, neworg);
			AddParticle (p_sparkray, org, 1, 6 + (i & 3), 5, color, neworg);
			angle[2] += 360 / 5;
		}
		angle[0] += 180 / 5;
	}
}
/*
void QMB_InfernoFlame (vec3_t org)
{
	float	frametime = fabs(cl.ctime - cl.oldtime);
	
	if (ISUNDERWATER(TruePointContents(org)))
		return;

	if (frametime)
	{
		if (gl_flametype.value < 1)
		{
			AddParticle (p_torch_flame, org, 1, 5, 0.5, NULL, zerodir);//R00k
		}
		else
		{
			AddParticle (p_inferno_flame, org, 1, 30, 13.125 * frametime, NULL, zerodir);
			AddParticle (p_inferno_trail, org, 2, 1.75, 45.0 * frametime, NULL, zerodir);
			AddParticle (p_inferno_trail, org, 2, 1.0, 52.5 * frametime, NULL, zerodir);
		}
	}
}
*/

void QMB_StaticBubble (entity_t *ent)
{
	AddParticle (p_staticbubble, ent->origin, 1, ent->frame == 1 ? 1.85 : 2.9, 0.001, NULL, zerodir);
}

void QMB_StaticMarker (entity_t *ent)
{
	col_t		color;

	color[0] = 255;color[1] = 255;color[2] = 000;color[3] = 0;
				
	AddParticle (p_dot, ent->origin, 1, 1, 0.01, color, zerodir);
}

void QMB_TorchFlame (vec3_t org) 
{
	float	frametime = fabs (cl.ctime - cl.oldtime);

	if (frametime > 0.001)
	{
		if (gl_flametype.value < 1)
		{
			AddParticle (p_torch_flame, org, 2, 2.5, 0.5, NULL, zerodir);		
		}
		else
		{
			AddParticle (p_flame, org, 1, 7, 0.8, NULL, zerodir);
		}
	}
}

void QMB_BigTorchFlame (vec3_t org) 
{
	float	frametime = fabs (cl.ctime - cl.oldtime);

	if (frametime > 0.001)
	{
		org[2] -= 5;org[0] += 1;
		if (gl_flametype.value < 1)
		{
			AddParticle (p_torch_flame, org, 2, 4, 0.5, NULL, zerodir);//R00k
		}
		else
		{		
			
			AddParticle (p_flame, org, 1, 5, 0.9, NULL, zerodir);
		}		
	}
}

void QMB_ShamblerCharge (vec3_t org)
{
	vec3_t	pos, vec, dir;
	col_t	col = {60, 100, 240, 128};
	float	time, len;
	int	i;

	for (i=0 ; i<5 ; i++)
	{
		VectorClear(vec);
		VectorClear(dir);

		VectorCopy(org, pos);
		pos[0] += (rand() % 200) - 100;
		pos[1] += (rand() % 200) - 100;
		pos[2] += (rand() % 200) - 100;
	
		VectorSubtract(pos, org, vec);
		len = VectorLength (vec);
		VectorNormalize (vec);
		VectorMA(dir, -200, vec, dir);
		time = len / 200;

		AddParticle (p_streakwave, pos, 1, 3, time, col, dir);
	}
}
//R00k: this is the OLD style like HalfLife 1...not used.
/*
void QMB_FlashLight (void)
{	
	float		frametime	= fabs(cl.time - cl.oldtime);
	float		size;

	extern cvar_t	v_viewheight;
	extern cvar_t	cl_gun_offset;
	
	vec3_t	dest, start, forward, right,up, sz;
	trace_t	trace;

	if (frametime > 0.001)
	{	
		if (qmb_initialized)
		{
			AngleVectors (r_refdef.viewangles,  forward, right, up);
			VectorCopy(cl_entities[cl.viewentity].origin, start);

			start[2] +=  16;
			start[2] += cl.crouch + bound(-7, v_viewheight.value, 4);

			VectorMA (start, 1024, forward, dest);					
					
			memset (&trace, 0, sizeof(trace_t));
			trace.fraction = 1;
			
			start[2]+=cl.crouch;

			SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, start, dest, &trace);
			
			VectorSubtract(dest, start, sz);
			
			size = VectorLength(sz);
			size = bound(0,size,100);

			if (trace.fraction != 1)
			{					
				CL_NewDlight (0, trace.endpos, size, 0.02, lt_default);
			}
		} 
	}
}
*/

void QMB_LaserSight (void)
{	
	float		frametime	= fabs(cl.time - cl.oldtime);
	col_t		color;
	int			c;
	double		fpstime;

	extern cvar_t	gl_laserpoint;
	extern cvar_t	chase_right;
	extern cvar_t	v_viewheight;
	extern cvar_t	cl_gun_offset;
	extern cvar_t	cl_maxfps;
	
	vec3_t	dest, start, forward, right, up;
	trace_t	trace;
	float	varray_vertex[16];
	
	if (!particle_mode)
		return;

	if (cl.viewent.model->name == NULL)//R00k: dont show laserpoint when observer!
	{
		return;
	}
	
	fpstime = (1.0 / cl_maxfps.value);

	if (frametime > fpstime)
	{	
		if (qmb_initialized)
		{
			if (cl.time > cl.laser_point_time)
			{
				AngleVectors (r_refdef.viewangles,  forward, right, up);
				VectorCopy(cl_entities[cl.viewentity].origin, start);

				start[2] +=  16;//QuakeC uses + '0 0 16' for gun aim.
				start[2] += cl.crouch;

				if (cl_gun_offset.value == 2)//Right-Handed
				{
					VectorMA (start, 2, right, start);	
				}
				else if (cl_gun_offset.value == 1)//Left-Handed
				{
					VectorMA (start, -2, right, start);	
				}

				VectorMA (start, 4096, forward, dest);					
						
				c = lt_default;

				switch ((int)gl_laserpoint.value)
				{
					case 1:
						color[0] = color[0] = 000;color[1] = 000;color[2] = 255;color[3] = 30;//B
						c = lt_blue;
						break;
					case 2:
						color[0] = 255;color[1] = 000;color[2] = 000;color[3] = 30;//R
						c = lt_red;
						break;
					case 3:
						color[0] = 255;color[1] = 255;color[2] = 000;color[3] = 30;//Y
						c = lt_yellow;
						break;
					case 4:					
						color[0] = 000;color[1] = 255;color[2] = 000;color[3] = 30;//G
						c = lt_green;
						break;
				}			
				
				memset (&trace, 0, sizeof(trace_t));
				trace.fraction = 1;
				SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, start, dest, &trace);
				
				start[2] += cl.crouch;

				R_CalcBeamVerts (varray_vertex, start, trace.endpos, 0.05);

				glDisable (GL_TEXTURE_2D);
				glEnable(GL_BLEND);
				glColor4f(color[0], color[1], color[2], 0.05);

				glBegin(GL_QUADS); 		
					glVertex3f (varray_vertex[0], varray_vertex[1], varray_vertex[2]);
					glVertex3f (varray_vertex[4], varray_vertex[5], varray_vertex[6]);
					glVertex3f (varray_vertex[8], varray_vertex[9], varray_vertex[10]);
					glVertex3f (varray_vertex[12], varray_vertex[13], varray_vertex[14]);
				glEnd ();

				glColor4f(1,1,1,1);
				glDisable(GL_BLEND);
				glEnable (GL_TEXTURE_2D);	

				if (trace.fraction != 1)
				{					
					color[3] = 200;

					AddParticle (p_dot, trace.endpos, 1, 4, fpstime, color, zerodir);//pinpoint on wall

					if (cl.maxclients < 2)
					{					
						CL_NewDlight (0, trace.endpos, (rand() % 10 + 30), fpstime + 0.005, c);
					}
				}
				cl.laser_point_time = cl.time + fpstime;
			}
		} 
	}
}


void QMB_CullTraceHighlight (void)
{	
	float		frametime	= fabs(cl.time - cl.oldtime);
	col_t		color;
	int			c;
	double		fpstime;

	extern cvar_t	gl_laserpoint;
	extern cvar_t	chase_right;
	extern cvar_t	v_viewheight;
	extern cvar_t	cl_gun_offset;
	extern cvar_t	cl_maxfps;
	
	vec3_t	dest, start, forward, right, up;
	trace_t	trace;
	float	varray_vertex[16];
	
	if (!particle_mode)
		return;

	fpstime = (1.0 / cl_maxfps.value);

	if (frametime > fpstime)
	{	
		if (qmb_initialized)
		{
			AngleVectors (r_refdef.viewangles,  forward, right, up);
			VectorCopy(cl_entities[cl.viewentity].origin, start);

			start[2] += 16;//QuakeC uses + '0 0 16' for gun aim.
			start[2] += cl.crouch + bound(-7, v_viewheight.value, 4);

			if (cl_gun_offset.value == 2)//Right-Handed
			{
				VectorMA (start, 2, right, start);	
			}
			else if (cl_gun_offset.value == 1)//Left-Handed
			{
				VectorMA (start, -2, right, start);	
			}

			VectorMA (start, 4096, forward, dest);					
					
			color[0] = 255;color[1] = 000;color[2] = 000;color[3] = 30;//R
			c = lt_red;
			
			memset (&trace, 0, sizeof(trace_t));
			trace.fraction = 1;
			SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, start, dest, &trace);
			
			start[2] += cl.crouch;

			R_CalcBeamVerts (varray_vertex, start, trace.endpos, 0.05);

			glDisable (GL_TEXTURE_2D);
			glEnable(GL_BLEND);
			glColor4f(color[0], color[1], color[2], 0.05);

			glBegin(GL_QUADS); 		
				glVertex3f (varray_vertex[0], varray_vertex[1], varray_vertex[2]);
				glVertex3f (varray_vertex[4], varray_vertex[5], varray_vertex[6]);
				glVertex3f (varray_vertex[8], varray_vertex[9], varray_vertex[10]);
				glVertex3f (varray_vertex[12], varray_vertex[13], varray_vertex[14]);
			glEnd ();

			glColor4f(1,1,1,1);
			glDisable(GL_BLEND);
			glEnable (GL_TEXTURE_2D);	
		}
	}
};

void QMB_Lightning_Splash(vec3_t org)
{
	int      i, j;
	vec3_t   neworg, angle;
	col_t   color;   
	col_t	col2 = {200, 100, 100, 255};

	VectorSet (color, 40, 40, 128);
	VectorClear (angle);

	for (i=0 ; i<5 ; i++)
	{
	  angle[2] = 0;
	  for (j=0 ; j<5 ; j++)
	  {
			AngleVectors (angle, NULLVEC, NULLVEC, neworg);
			VectorMA (org, 20, neworg, neworg);
			AddParticle (p_spark, org, 2, 42, 0.05f, NULL, zerodir);
			AddParticle (p_spark, org, 2, 50, 0.1f, col2, neworg);
			angle[2] += 360 / 5;
	  }
	  angle[0] += 180 / 5;
	}   
	color[0] = 224 + (rand() & 31);
	color[1] = 100 + (rand() & 31);
	color[2] = 0;

	AddParticle (p_spark, org, 1, 35, 0.2, color, zerodir);			
} 

void QMB_LightningBeam (vec3_t start, vec3_t end)
{	
	float		frametime	= fabs(cl.time - cl.oldtime);
	col_t		color		= {255,255,255,255};
	trace_t		trace;
	extern cvar_t	gl_lightning_alpha;

	if (qmb_initialized && gl_part_lightning.value )
	{
		if (qmb_initialized && gl_part_sparks.value)
		{
			memset (&trace, 0, sizeof(trace_t));
			trace.fraction = 1;
			if (!SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, start, end, &trace))
			{
				if (trace.fraction < 1)
				{
					VectorCopy (trace.endpos, end);

					if ((gl_decal_sparks.value) && (particle_mode) && (decals_enabled))
					{							
						R_SpawnDecalStatic(end, decal_glow, 10);
					}
					QMB_Lightning_Splash (end);						
				}
			}			
		}
		color [3] = bound(0.4f, (gl_lightning_alpha.value * 255), 255);	
		//R00k v1.84 moved down here so that the 'p_lightningbeam' particle will stop when it hits something.
		AddParticle(p_lightningbeam, start, 1, 80, host_frametime * 2, color, end);
	} 
}

extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t		avelocities[NUMVERTEXNORMALS];

/*
===============
R_EntityParticles
===============
*/
void QMB_EntityParticles (entity_t *ent)
{
	int			i;
	float		angle, dist, sr, sp, sy, cr, cp, cy;
	vec3_t		forward, org;
	col_t		color = {255,255,0,100};
	
	dist = 64;

	if (!avelocities[0][0])
		for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
			avelocities[0][i] = (rand() & 255) * 0.01;

	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		angle = cl.time * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = cl.time * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);
		angle = cl.time * avelocities[i][2];
		sr = sin(angle);
		cr = cos(angle);
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		org[0] = ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*16;
		org[1] = ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*16;
		org[2] = ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*16;
		AddParticle (p_flare, org, 1, 2,0.005, color, forward);
	}
}
/*
void QMB_ShowerParticles (entity_t *ent)
{
	float		frametime	= fabs(cl.time - cl.oldtime);
    int         i;
    float       angle, sr, sp, sy, cr, cp, cy;
    vec3_t      forward, org;
    float       dist = 64;
	col_t		color = {255,255,255,100};

	if (frametime > 0.001)
	{
		if (!avelocities[0][0])
		{
			for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
				avelocities[0][i] = (rand()&255) * 0.01;
		}

		for (i = 0 ; i < NUMVERTEXNORMALS ; i += 2)
		{
			angle = cl.time * avelocities[i][0];
			sy = sin(angle);
			cy = cos(angle);
			angle = cl.time * avelocities[i][1];
			sp = sin(angle);
			cp = cos(angle);
			angle = cl.time * avelocities[i][2];
			sr = sin(angle);
			cr = cos(angle);
    
			forward[0] = cp*cy;
			forward[1] = cp*sy;
			forward[2] = -sp;

			dist = sin(cl.time + i)*32;

			org[0] = ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*16;
			org[1] = ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*16;
			org[2] = ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*16;

			AddParticle (p_flare, org, 1, 1,0.01, color, forward);
		}
	}
}
*/


//Modified from Quake2
void QMB_FlyParticles (vec3_t origin, int count)
{
	float		frametime	= fabs(cl.time - cl.oldtime);
    int         i;
    float       angle, sr, sp, sy, cr, cp, cy;
    vec3_t      forward, org;
    float       dist = 64;
	col_t		color = {255,255,255,100};

	if (frametime > 0.001)
	{
		if (count > NUMVERTEXNORMALS)
			count = NUMVERTEXNORMALS;

		if (!avelocities[0][0])
		{
			for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
				avelocities[0][i] = (rand()&255) * 0.01;
		}

		for (i=0 ; i<count ; i+=2)
		{
			angle = cl.time * avelocities[i][0];
			sy = sin(angle);
			cy = cos(angle);
			angle = cl.time * avelocities[i][1];
			sp = sin(angle);
			cp = cos(angle);
			angle = cl.time * avelocities[i][2];
			sr = sin(angle);
			cr = cos(angle);
    
			forward[0] = cp*cy;
			forward[1] = cp*sy;
			forward[2] = -sp;

			dist = sin(cl.time + i)*64;
			org[0] = origin[0] + r_avertexnormals[i][0]*dist + forward[0]*32;
			org[1] = origin[1] + r_avertexnormals[i][1]*dist + forward[1]*32;
			org[2] = origin[2] + r_avertexnormals[i][2]*dist + forward[2]*32;

			AddParticle (p_fly, org, 1, 1,0.01, color, forward);
		}
	}
}

void CheckParticles (void)
{
	if (!qmb_initialized)//something is wrong fallback to classic
	{
		particle_mode = 0;
		return;
	}

	if	(
			!gl_part_explosions.value && 
			!gl_part_trails.value && 
			!gl_part_sparks.value && 
			!gl_part_gunshots.value && 
			!gl_part_blood.value && 
			!gl_part_telesplash.value && 
			!gl_part_blobs.value && 
			!gl_part_lavasplash.value && 
			!gl_part_flames.value && 
			!gl_part_lightning.value &&
			!gl_part_flies.value 
		)
		particle_mode = 0;
	else 
	if	(
			gl_part_explosions.value && 
			gl_part_trails.value && 
			gl_part_sparks.value && 
			gl_part_gunshots.value && 
			gl_part_blood.value && 
			gl_part_lavasplash.value && 
			gl_part_flames.value && 
			gl_part_lightning.value &&
			gl_part_flies.value 
		)
		particle_mode = 1;
	else
		particle_mode = 2;
};

void R_SetParticles (int val)
{
	particle_mode = val;

	Cvar_SetValue ("gl_part_explosions", particle_mode);
	Cvar_SetValue ("gl_part_trails", particle_mode);
	Cvar_SetValue ("gl_part_sparks", particle_mode);
	Cvar_SetValue ("gl_part_gunshots", particle_mode);
	Cvar_SetValue ("gl_part_blood", particle_mode);
	Cvar_SetValue ("gl_part_telesplash", particle_mode);
	Cvar_SetValue ("gl_part_blobs", particle_mode);
	Cvar_SetValue ("gl_part_lavasplash", particle_mode);
	Cvar_SetValue ("gl_part_flames", particle_mode);
	Cvar_SetValue ("gl_part_lightning", particle_mode);
	Cvar_SetValue ("gl_part_flies", particle_mode);
}

/*
===============
R_ToggleParticles_f

function that toggles between classic and QMB particles - by joe
===============
*/
void R_ToggleParticles_f (void)
{
	if (cmd_source != src_command)
		return;

	CheckParticles();

	R_SetParticles (!particle_mode);
	
	Con_Printf ("Using %s particles\n", !particle_mode ? "Classic" : "QMB");
}


