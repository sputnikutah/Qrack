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
// r_part.c


#include "quakedef.h"

#ifdef USESHADERS
// program.local[0] = up vector
// program.local[1] = right vector
// program.local[2] = -(r_origin * vpn) vector
// program.local[3] = vpn vector
// program.local[4] = scale vector
// program.local[5] ... max = particle positions and colours
#define particle_vp \
	"!!ARBvp1.0\n" \
	"TEMP pscale, ppos;\n" \
	"MAD pscale, vertex.attrib[0], program.local[3], program.local[2];\n" \
	"DPH pscale.w, pscale, program.local[4];\n" \
	"MUL pscale, pscale.w, vertex.attrib[2];\n" \
	"MAD ppos, program.local[0], pscale.x, vertex.attrib[0];\n" \
	"MAD ppos, program.local[1], pscale.y, ppos;\n" \
	TRANSFORMVERTEX ("result.position", "ppos") \
	"MOV result.texcoord[0], vertex.attrib[2];\n" \
	"MOV result.color, vertex.attrib[1];\n" \
	"END\n"

#define particle_fp \
	"!!ARBfp1.0\n" \
	"TEMP col0;\n" \
	"MOV col0, fragment.color;\n" \
	"DP3 col0.w, fragment.texcoord[0], fragment.texcoord[0];\n" \
	"SUB col0.w, 1.0, col0.w;\n" \
	"MUL result.color, col0, program.local[0];\n" \
	"END\n"

GLuint arb_particle_vp = 0;
GLuint arb_particle_fp = 0;
GLuint arb_particle_fp_fog = 0;

void Part_CreateShaders (void)
{
	arb_particle_vp = GL_CreateProgram (GL_VERTEX_PROGRAM_ARB, particle_vp, false);
	arb_particle_fp = GL_CreateProgram (GL_FRAGMENT_PROGRAM_ARB, particle_fp, false);
	arb_particle_fp_fog = GL_CreateProgram (GL_FRAGMENT_PROGRAM_ARB, particle_fp, true);
}
#endif

#ifdef GLQUAKE

typedef enum
{
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2
} ptype_t;

typedef struct particle_s
{
	vec3_t		org;
	float		color;
	vec3_t		vel;
	float		ramp;
	float		die;
	ptype_t		type;
	struct particle_s	*next;
} particle_t;

#else			// software

#include "d_local.h"
#include "r_local.h"

#endif

//#define DEFAULT_NUM_PARTICLES	2048
#define ABSOLUTE_MIN_PARTICLES	512
#define ABSOLUTE_MAX_PARTICLES	8192

int particletexture2;

extern void Classic_InitParticles (void);

qboolean OnChange_r_particle_count (cvar_t *var, char *string)
{
	float	f;

	f = bound(ABSOLUTE_MIN_PARTICLES, (atoi(string)), ABSOLUTE_MAX_PARTICLES);
	Cvar_SetValue("r_particle_count", f);

	Classic_InitParticles();
	return true;
}

cvar_t	r_particle_count = {"r_particle_count", "2048", true, false, OnChange_r_particle_count};
cvar_t	r_particle_type	= {"r_particle_type", "0", true};//R00k particle type determines the shape of the particle. Circle or square. Default oldschool software shape square! :)
cvar_t	r_particle_scale = {"r_particle_scale","0", true};//R00k static particle size.

static	int	ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
static	int	ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
static	int	ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

static	particle_t	*particles, *active_particles, *free_particles;

static	int		r_numparticles;

vec3_t			r_pright, r_pup, r_ppn;

#ifndef GLQUAKE
#if !id386

/*
==============
D_DrawParticle
==============
*/
void D_DrawParticle (particle_t *pparticle)
{
	vec3_t	local, transformed;
	float	zi;
	byte	*pdest;
	short	*pz;
	int		i, izi, pix, count, u, v;

// transform point
	VectorSubtract (pparticle->org, r_origin, local);

	transformed[0] = DotProduct (local, r_pright);
	transformed[1] = DotProduct (local, r_pup);
	transformed[2] = DotProduct (local, r_ppn);		

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

// project the point
// FIXME: preadjust xcenter and ycenter
	zi = 1.0 / transformed[2];
	u = (int)(xcenter + zi * transformed[0] + 0.5);
	v = (int)(ycenter - zi * transformed[1] + 0.5);

	if ((v > d_vrectbottom_particle) || (u > d_vrectright_particle) || (v < d_vrecty) || (u < d_vrectx))
		return;

	pz = d_pzbuffer + (d_zwidth * v) + u;
	pdest = d_viewbuffer + d_scantable[v] + u;
	izi = (int)(zi * 0x8000);

	pix = izi >> d_pix_shift;

	pix = bound(d_pix_min, pix, d_pix_max);

	switch (pix)
	{
	case 1:
		count = 1 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
				pz[0] = izi;
				pdest[0] = pparticle->color;
			}
		}
		break;

	case 2:
		count = 2 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
				pz[0] = izi;
				pdest[0] = pparticle->color;
			}

			if (pz[1] <= izi)
			{
				pz[1] = izi;
				pdest[1] = pparticle->color;
			}
		}
		break;

	case 3:
		count = 3 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
				pz[0] = izi;
				pdest[0] = pparticle->color;
			}

			if (pz[1] <= izi)
			{
				pz[1] = izi;
				pdest[1] = pparticle->color;
			}

			if (pz[2] <= izi)
			{
				pz[2] = izi;
				pdest[2] = pparticle->color;
			}
		}
		break;

	case 4:
		count = 4 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
				pz[0] = izi;
				pdest[0] = pparticle->color;
			}

			if (pz[1] <= izi)
			{
				pz[1] = izi;
				pdest[1] = pparticle->color;
			}

			if (pz[2] <= izi)
			{
				pz[2] = izi;
				pdest[2] = pparticle->color;
			}

			if (pz[3] <= izi)
			{
				pz[3] = izi;
				pdest[3] = pparticle->color;
			}
		}
		break;

	default:
		count = pix << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			for (i=0 ; i<pix ; i++)
			{
				if (pz[i] <= izi)
				{
					pz[i] = izi;
					pdest[i] = pparticle->color;
				}
			}
		}
		break;
	}
}

#endif	// !id386
#endif	// GLQUAKE

#ifdef GLQUAKE
static void Classic_LoadParticleTextures (void)
{
	int		i, x, y;
	unsigned int	data[32][32];


	particletexture = texture_extension_number++;
	GL_Bind (particletexture);

	// clear to transparent white
	for (i=0 ; i<1024 ; i++)
		((unsigned *)data)[i] = 0x00FFFFFF;

	// draw a circle in the top left corner
	for (x=0 ; x<16 ; x++)
	{
		for (y=0 ; y<16 ; y++)
		{
			if ((x - 7.5) * (x - 7.5) + (y - 7.5) * (y - 7.5) <= 64)
				data[y][x] = 0xFFFFFFFF;	// solid white
		}
	}
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	GL_Upload32 ((unsigned *)data, 32, 32, TEX_MIPMAP | TEX_ALPHA);

	//R00k: added square simple particle (oldschool look!)
	particletexture2 = texture_extension_number++;
	GL_Bind (particletexture2);

	// clear to transparent white
	for (i=0 ; i<1024 ; i++)
		((unsigned *)data)[i] = 0x00FFFFFF;


	// draw a solid block
	for (x=0 ; x<16 ; x++)
	{
		for (y=0 ; y<16 ; y++)
		{
			data[x][y] = 0xFFFFFFFF;
		}
	}

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GL_Upload32 ((unsigned *)data, 32, 32, TEX_MIPMAP | TEX_ALPHA);
}
#endif

/*
===============
Classic_AllocParticles
===============
*/
void Classic_AllocParticles (void) 
{
	r_numparticles = bound(ABSOLUTE_MIN_PARTICLES, r_particle_count.value, ABSOLUTE_MAX_PARTICLES);

	if (particles || r_numparticles < 1) // seems Classic_AllocParticles() called from wrong place
		Sys_Error("Classic_AllocParticles: internal error");

	// can't alloc on Hunk, using native memory
	particles = (particle_t *) Q_malloc (r_numparticles * sizeof(particle_t));	
	memset (particles, 0, r_numparticles * sizeof(particle_t));//FIXED! 2.011 phew!
}

/*
===============
R_ClearParticles
===============
*/
void Classic_ClearParticles (void) 
{
	int		i;

	if (!particles)
		return;

	Q_free (particles);			// free
	Classic_AllocParticles ();	// and alloc again

	free_particles = &particles[0];
	active_particles = 0;

	for (i = 0;i < r_numparticles; i++)
		particles[i].next = &particles[i+1];
	particles[r_numparticles - 1].next = NULL;
}
/*
===============
R_InitParticles
===============
*/
void Classic_InitParticles (void)
{
	if (!particles)
		Classic_AllocParticles ();
	else
		Classic_ClearParticles (); // also re-alloc particles

#ifdef GLQUAKE
	Classic_LoadParticleTextures ();
#endif
}

extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t		avelocities[NUMVERTEXNORMALS];

/*
===============
R_EntityParticles
===============
*/
void Classic_EntityParticles (entity_t *ent)
{
	int		i, count;
	particle_t	*p;
	float		angle, dist, sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	
	dist = 64;
	count = 50;

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

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.01;
		p->color = 0x6f;
		p->type = pt_explode;
		
		p->org[0] = ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*16;
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*16;
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*16;
	}
}


void R_ReadPointFile_f (void)
{
	FILE		*f;
	vec3_t		org;
	int		r, c;
	particle_t	*p;
	char		name[MAX_OSPATH];
	

	sprintf (name, "maps/%s.pts", sv.name);

	Con_Printf ("Reading %s...\n", name);

	COM_FOpenFile (name, &f);

	if (!f)
	{
		Con_Printf ("couldn't open %s\n", name);
		return;
	}
	
	Con_Printf ("Reading %s...\n", name);
	c = 0;
	for ( ; ; )
	{
		r = fscanf_s (f,"%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;
		
		if (!free_particles)
		{
			Con_Printf ("Not enough free particles\n");
			break;
		}
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		
		p->die = 99999;
		p->color = (-c)&15;
		p->type = pt_static;
		VectorCopy (vec3_origin, p->vel);
		VectorCopy (org, p->org);
	}

	fclose (f);
	Con_Printf ("%i points read\n", c);
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect (void)
{
	vec3_t		org, dir;
	int		i, count, color;
	
	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	for (i=0 ; i<3 ; i++)
		dir[i] = MSG_ReadChar () * 0.0625;
	count = MSG_ReadByte ();
	color = MSG_ReadByte ();

// HACK: unfortunately the effect of the exploding barrel is quite poor 
// in the original progs - no light, *sigh* - so I added some extras - joe
	if (count == 255)
	{
		dlight_t	*dl;

		// joe: nehahra has improved features for barrels
		if (nehahra)
			return;

		R_ParticleExplosion (org);

		// the missing light
		if (r_explosionlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (org, dl->origin);
			dl->radius = 150 + 200 * bound (0, r_explosionlight.value, 1);
			dl->die = cl.time + 0.5;
			dl->decay = 300;
#ifdef GLQUAKE
			dl->type = SetDlightColor (r_explosionlightcolor.value, lt_explosion, true);
#endif
		}
	}
	else
	{
		R_RunParticleEffect (org, dir, color, count);
	}
}
	
/*
===============
R_ParticleExplosion
===============
*/
void Classic_ParticleExplosion (vec3_t org)
{
	int		i, j;
	particle_t	*p;

	if (r_explosiontype.value == 5)	// just a sprite
		return;
	
	for (i=0 ; i<1024 ; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 5;
		p->color = ramp1[0];
		p->ramp = rand() & 3;
		p->type = (i & 1) ? pt_explode : pt_explode2;

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand() & 31) - 16);
			p->vel[j] = (rand() & 511) - 256;
		}
	}
}

/*
===============
R_ColorMappedExplosion		// joe: previously R_ParticleExplosion2
===============
*/
void Classic_ColorMappedExplosion (vec3_t org, int colorStart, int colorLength)
{
	int		i, j;
	particle_t	*p;
	int		colorMod = 0;

	if (r_explosiontype.value == 5)
		return;
	
	for (i=0 ; i<512 ; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.3;
		p->color = colorStart + (colorMod++ % colorLength);
		
		p->type = pt_blob;

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand() & 31) - 16);
			p->vel[j] = (rand() & 511) - 256;
		}
	}
}

/*
===============
R_BlobExplosion
===============
*/
void Classic_BlobExplosion (vec3_t org)
{
	int		i, j;
	particle_t	*p;
	
	for (i=0 ; i<1024 ; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 1 + (rand() & 8) * 0.05;

		if (i & 1)
		{
			p->type = pt_blob;
			p->color = 66 + rand() % 6;
		}
		else
		{
			p->type = pt_blob2;
			p->color = 150 + rand() % 6;
		}

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand() & 31) - 16);
			p->vel[j] = (rand() & 511) - 256;
		}
	}
}

/*
===============
R_RunParticleEffect
===============
*/
void Classic_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int		i, j;
	particle_t	*p;

	for (i=0 ; i<count ; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.1 * (rand() % 5);
		p->color = (color & ~7) + (rand() & 7);
		p->type = pt_grav;

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand() & 15) - 8);
			p->vel[j] = dir[j] * 15;
		}
	}
}

/*
===============
R_LavaSplash
===============
*/
void Classic_LavaSplash (vec3_t org)
{
	int		i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i=-16 ; i<16 ; i++)
	{
		for (j=-16 ; j<16 ; j++)
		{
			for (k=0 ; k<1 ; k++)
			{
				if (!free_particles)
					return;

				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;
		
				p->die = cl.time + 2 + (rand() & 31) * 0.02;
				p->color = 224 + (rand() & 7);
				p->type = pt_grav;
				
				dir[0] = j * 8 + (rand() & 7);
				dir[1] = i * 8 + (rand() & 7);
				dir[2] = 256;
	
				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand() & 63);
	
				VectorNormalize (dir);
				vel = 50 + (rand() & 63);
				VectorScale (dir, vel, p->vel);
			}
		}
	}
}

/*
===============
R_TeleportSplash
===============
*/
void Classic_TeleportSplash (vec3_t org)
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i=-16 ; i<16 ; i+=4)
	{
		for (j=-16 ; j<16 ; j+=4)
		{
			for (k=-24 ; k<32 ; k+=4)
			{
				if (!free_particles)
					return;

				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = cl.time + 0.2 + (rand() & 7) * 0.02;
				p->color = 7 + (rand() & 7);
				p->type = pt_grav;

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				p->org[0] = org[0] + i + (rand() & 3);
				p->org[1] = org[1] + j + (rand() & 3);
				p->org[2] = org[2] + k + (rand() & 3);

				VectorNormalize (dir);
				vel = 50 + (rand() & 63);
				VectorScale (dir, vel, p->vel);
			}
		}
	}
}

/*
===============
R_RocketTrail
===============
*/
void Classic_RocketTrail (vec3_t start, vec3_t end, int type)
{
	vec3_t		vec;
	float		len;
	int		j, dec;
	particle_t	*p;
	static int	tracercount;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	if (type < 128)
	{
		dec = 3;
	}
	else
	{
		dec = 1;
		type -= 128;
	}

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		
		VectorCopy (vec3_origin, p->vel);
		p->die = cl.time + 2;

		switch (type)
		{
			case GRENADE_TRAIL:
				p->ramp = (rand() & 3) + 2;
				p->color = ramp3[(int)p->ramp];
				p->type = pt_fire;
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand() % 6) - 3);
				break;

			case BLOOD_TRAIL:
				p->type = pt_grav;
				p->color = 67 + (rand() & 3);
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand() % 6) - 3);
				break;

			case TRACER1_TRAIL:
			case TRACER2_TRAIL:
				p->die = cl.time + 0.5;
				p->type = pt_static;
				p->color = (type == 3) ? 52 + ((tracercount & 4) << 1) : 230 + ((tracercount & 4) << 1);
				tracercount++;

				VectorCopy (start, p->org);
				if (tracercount & 1)
				{
					p->vel[0] = 30 * vec[1];
					p->vel[1] = 30 * -vec[0];
				}
				else
				{
					p->vel[0] = 30 * -vec[1];
					p->vel[1] = 30 * vec[0];
				}
				break;

			case SLIGHT_BLOOD_TRAIL:
				p->type = pt_grav;
				p->color = 67 + (rand() & 3);
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand() % 6) - 3);
				len -= 3;
				break;

			case VOOR_TRAIL:
				p->color = 9*16 + 8 + (rand() & 3);
				p->type = pt_static;
				p->die = cl.time + 0.3;
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand() & 15) - 8);
				break;

			case NEHAHRA_SMOKE:
				// nehahra smoke tracer
				p->color = 12 + (rand() & 3);
				p->type = pt_fire;
				p->die = cl.time + 1;
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand() & 3) - 2);
				break;

			case ALT_ROCKET_TRAIL:
			case ROCKET_TRAIL:
			case LAVA_TRAIL:
			default:
				p->ramp = (rand() & 3);
				p->color = ramp3[(int)p->ramp];
				p->type = pt_fire;
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand() % 6) - 3);
				break;
		}		
		VectorAdd (start, vec, start);
	}
}
#ifdef USESHADERS
/*
===============
CL_RunParticles -- johnfitz -- all the particle behavior, separated from R_DrawParticles
===============

void CL_RunParticlesForType (particle_type_t *pt)
{
	particle_t		*p, *kill;
	int				i;
	float			time1, time2, time3, dvel, frametime, grav;
	extern	cvar_t	sv_gravity;

	// nothing to run
	if (!pt->numparticles) return;
	if (!pt->particles) return;

	frametime = fabs (cl.ctime - cl.oldtime);
	time3 = frametime * 15;
	time2 = frametime * 10;
	time1 = frametime * 5;
	grav = frametime * sv_gravity.value * 0.05;
	dvel = 4 * frametime;

	for (;;)
	{
		kill = pt->particles;

		if (kill && kill->die < cl.time)
		{
			pt->particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			r_numparticles--;
			pt->numparticles--;
			continue;
		}

		break;
	}

	for (p = pt->particles; p; p = p->next)
	{
		for (;;)
		{
			kill = p->next;

			if (kill && kill->die < cl.time)
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				r_numparticles--;
				pt->numparticles--;
				continue;
			}

			break;
		}

		p->org[0] += p->vel[0] * frametime;
		p->org[1] += p->vel[1] * frametime;
		p->org[2] += p->vel[2] * frametime;

		p->alpha -= p->alphadec * frametime;

		if (p->alpha <= 0) p->die = -1;

		switch (p->type)
		{
		case pt_static:
			break;

		case pt_fire:
			p->ramp += time1;

			if (p->ramp >= 6)
				p->die = -1;
			else
				p->color = ramp3[(int) p->ramp];

			p->vel[2] += grav;
			break;

		case pt_explode:
			p->ramp += time2;

			if (p->ramp >= 8)
				p->die = -1;
			else
				p->color = ramp1[(int) p->ramp];

			for (i = 0; i < 3; i++)
				p->vel[i] += p->vel[i] * dvel;

			p->vel[2] -= grav;
			break;

		case pt_explode2:
			p->ramp += time3;

			if (p->ramp >= 8)
				p->die = -1;
			else
				p->color = ramp2[(int) p->ramp];

			for (i = 0; i < 3; i++)
				p->vel[i] -= p->vel[i] * frametime;

			p->vel[2] -= grav;
			break;

		case pt_blob:
			for (i = 0; i < 3; i++)
				p->vel[i] += p->vel[i] * dvel;

			p->vel[2] -= grav;
			break;

		case pt_blob2:

			for (i = 0; i < 2; i++)
				p->vel[i] -= p->vel[i] * dvel;

			p->vel[2] -= grav;
			break;

		case pt_grav:
		case pt_slowgrav:
			p->vel[2] -= grav;
			break;
		}
	}
}


void CL_RunParticles (void)
{
	particle_type_t	*pt, *kill;

	if (!active_particle_types) return;
	if (cls.state != ca_connected) return;

	// remove from the head of the list
	for (;;)
	{
		kill = active_particle_types;

		if (kill && !kill->particles)
		{
			// return to the free list
			active_particle_types = kill->next;
			kill->next = free_particle_types;
			kill->numparticles = 0;
			free_particle_types = kill;

			continue;
		}

		break;
	}

	// no types currently active
	if (!active_particle_types) return;

	for (pt = active_particle_types; pt; pt = pt->next)
	{
		// remove from a mid-point in the list
		for (;;)
		{
			kill = pt->next;

			if (kill && !kill->particles)
			{
				pt->next = kill->next;
				kill->next = free_particle_types;
				kill->numparticles = 0;
				free_particle_types = kill;

				continue;
			}

			break;
		}

		CL_RunParticlesForType (pt);
	}
}


/*
===============
R_DrawParticles -- johnfitz -- moved all non-drawing code to CL_RunParticles
===============


int r_particleframe = -1;

typedef struct r_particlevert_s
{
	float xyz[3];

	union
	{
		byte color[4];
		unsigned int rgba;
	};

	float st[2];
} r_particlevert_t;

r_particlevert_t *r_particleverts;
int r_numparticleverts;
int r_maxparticleverts = SCRATCHBUF_SIZE / (sizeof (r_particlevert_t) * 4);

void R_ParticlesBegin (void)
{
#define PARTICLE_SCALE	(0.666f * r_particlesize.value)
#define HACK_SCALE_UP	(0.004f * r_particlescale.value)

	GL_SetVertexProgram (arb_particle_vp);

	if (Fog_GetDensity () > 0)
		GL_SetFragmentProgram (arb_particle_fp_fog);
	else GL_SetFragmentProgram (arb_particle_fp);

	qglProgramLocalParameter4fARB (GL_VERTEX_PROGRAM_ARB, 0, vup[0] * PARTICLE_SCALE, vup[1] * PARTICLE_SCALE, vup[2] * PARTICLE_SCALE, 0);
	qglProgramLocalParameter4fARB (GL_VERTEX_PROGRAM_ARB, 1, vright[0] * PARTICLE_SCALE, vright[1] * PARTICLE_SCALE, vright[2] * PARTICLE_SCALE, 0);
	qglProgramLocalParameter4fARB (GL_VERTEX_PROGRAM_ARB, 2, -(r_origin[0] * vpn[0]), -(r_origin[1] * vpn[1]), -(r_origin[2] * vpn[2]), 0);
	qglProgramLocalParameter4fARB (GL_VERTEX_PROGRAM_ARB, 3, vpn[0], vpn[1], vpn[2], 0);

	// hack a scale up to prevent particles from disappearing
	qglProgramLocalParameter4fARB (GL_VERTEX_PROGRAM_ARB, 4, HACK_SCALE_UP, HACK_SCALE_UP, HACK_SCALE_UP, 1.0f);

	// boost alpha to make particles look nicer
	qglProgramLocalParameter4fARB (GL_FRAGMENT_PROGRAM_ARB, 0, 1, 1, 1, 1.5f);

	r_particleverts = (r_particlevert_t *) scratchbuf;
	r_numparticleverts = 0;

	// ... and this is our real deal ...
	GL_EnableVertexArrays (VAA_0 | VAA_1 | VAA_2);
	GL_VertexArrayPointer (0, 0, 3, GL_FLOAT, GL_FALSE, sizeof (r_particlevert_t), r_particleverts->xyz);
	GL_VertexArrayPointer (0, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof (r_particlevert_t), r_particleverts->color);
	GL_VertexArrayPointer (0, 2, 2, GL_FLOAT, GL_FALSE, sizeof (r_particlevert_t), r_particleverts->st);
}


void R_ParticlesEnd (void)
{
	// draw anything left over
	if (r_numparticleverts)
	{
		GL_DrawArrays (GL_QUADS, 0, r_numparticleverts);
		r_numparticleverts = 0;
	}
}


#define R_ParticleVertex(vert, s, t) \
	(vert).xyz[0] = p->org[0]; \
	(vert).xyz[1] = p->org[1]; \
	(vert).xyz[2] = p->org[2]; \
	(vert).rgba = d_8to24table_rgba[(int) p->color]; \
	(vert).st[0] = (s); \
	(vert).st[1] = (t);

void R_DrawParticlesForType (particle_type_t *pt)
{
	particle_t *p;

	for (p = pt->particles; p; p = p->next)
	{
		// don't crash (255 is alpha)
		if (p->color < 0 || p->color > 254) continue;
		if (p->alpha < 0) continue;

		if (r_numparticleverts >= r_maxparticleverts)
		{
			GL_DrawArrays (GL_QUADS, 0, r_numparticleverts);
			r_particleverts = (r_particlevert_t *) scratchbuf;
			r_numparticleverts = 0;
		}

		R_ParticleVertex (r_particleverts[0], -1, -1);
		R_ParticleVertex (r_particleverts[1], 1, -1);
		R_ParticleVertex (r_particleverts[2], 1, 1);
		R_ParticleVertex (r_particleverts[3], -1, 1);

		r_particleverts += 4;
		r_numparticleverts += 4;
	}
}


void R_AddParticlesToAlphaList (particle_type_t *pt);

void R_DrawParticles (void)
{
	particle_type_t *pt;
	extern	cvar_t	r_particles; // johnfitz

	if (!r_particles.value) return;
	if (!active_particle_types) return;

	for (pt = active_particle_types; pt; pt = pt->next)
	{
		if (!pt->particles) continue;

		R_AddParticlesToAlphaList (pt);
	}
}


void CL_WipeParticles (void)
{
	// these need to be wiped immediately on going to a new server
	active_particle_types = NULL;
	free_particle_types = NULL;
	free_particles = NULL;
}
*/
#endif

/*
===============
R_DrawParticles
===============
*/
extern	cvar_t	sv_gravity;

void Classic_DrawParticles (void)
{
	particle_t	*p, *kill;
	int		i;
	float		grav, time1, time2, time3, dvel, frametime;
#ifdef GLQUAKE
	unsigned char	*at, theAlpha;
	vec3_t		up, right;
	float		dist, scale, r_partscale;
#endif

	if (!active_particles)
		return;

#ifdef GLQUAKE
	r_partscale = 0.004 * tan(r_refdef.fov_x * (M_PI / 180) * 0.5f);

	if (!(particletexture))
		Classic_LoadParticleTextures();

	if (r_particle_type.value)
		GL_Bind (particletexture);//circle
	else
		GL_Bind (particletexture2);//square (default)

	glEnable (GL_BLEND);
	glDepthMask (GL_FALSE);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBegin (GL_TRIANGLES);
	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);
#else
	VectorScale (vright, xscaleshrink, r_pright);
	VectorScale (vup, yscaleshrink, r_pup);
	VectorCopy (vpn, r_ppn);
#endif
	frametime = fabs(cl.ctime - cl.oldtime);
	if (cl.paused)		// joe: pace from FuhQuake
		frametime = 0;

	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = frametime * sv_gravity.value * 0.05;
	dvel = 4 * frametime;

	for ( ; ; ) 
	{
		kill = active_particles;
		if (kill && kill->die < cl.time)
		{
			active_particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}
		break;
	}

	for (p = active_particles ; p ; p = p->next)
	{
		for ( ; ; )
		{
			kill = p->next;
			if (kill && kill->die < cl.time)
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}
#ifdef GLQUAKE
		// hack a scale up to keep particles from disapearing
		if (r_particle_scale.value <= 1)//R00k: cvar'd this
		{
			dist = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1] + (p->org[2] - r_origin[2])*vpn[2];
			scale = 1 + dist * r_partscale;
		}
		else
		{
			scale = CLAMP(2, r_particle_scale.value, 6);
		}

		at = (byte *)&d_8to24table[(int)p->color];
		theAlpha = (p->type == pt_fire) ? 255 * (6 - p->ramp) / 6 : 255;
		glColor4ub (*at, *(at + 1), *(at + 2), theAlpha);
		glTexCoord2f (0,0);
		glVertex3fv (p->org);
		glTexCoord2f (1,0);
		glVertex3f (p->org[0] + up[0]*scale, p->org[1] + up[1]*scale, p->org[2] + up[2]*scale);
		glTexCoord2f (0,1);
		glVertex3f (p->org[0] + right[0]*scale, p->org[1] + right[1]*scale, p->org[2] + right[2]*scale);
#else
		D_DrawParticle (p);
#endif
		p->org[0] += p->vel[0] * frametime;
		p->org[1] += p->vel[1] * frametime;
		p->org[2] += p->vel[2] * frametime;
		
		switch (p->type)
		{
		case pt_static:
			break;

		case pt_fire:
			p->ramp += time1;
			if (p->ramp >= 6)
				p->die = -1;
			else
				p->color = ramp3[(int)p->ramp];
			p->vel[2] += grav;
			break;

		case pt_explode:
			p->ramp += time2;
			if (p->ramp >= 8)
				p->die = -1;
			else
				p->color = ramp1[(int)p->ramp];
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]*dvel;
			p->vel[2] -= (r_explosiontype.value == 1) ? grav : grav * 30;
			break;

		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >= 8)
				p->die = -1;
			else
				p->color = ramp2[(int)p->ramp];
			for (i=0 ; i<3 ; i++)
				p->vel[i] -= p->vel[i]*frametime;
			p->vel[2] -= (r_explosiontype.value == 1) ? grav : grav * 30;
			break;

		case pt_blob:
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_blob2:
			for (i=0 ; i<2 ; i++)
				p->vel[i] -= p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_grav:
		case pt_slowgrav:
			p->vel[2] -= grav;
			break;
		}
	}

#ifdef GLQUAKE
	glEnd ();
	glDisable (GL_BLEND);
	glDepthMask (GL_TRUE);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor3f (1, 1, 1);
#endif
}

void R_InitParticles (void)
{
	if (!host_initialized) 
	{
		Cvar_RegisterVariable (&r_particle_count);
		Cvar_RegisterVariable (&r_particle_type);//R00k
		Cvar_RegisterVariable (&r_particle_scale);//R00k
	}

	Classic_InitParticles ();

#ifdef GLQUAKE
	QMB_InitParticles ();
#endif
}

void R_ClearParticles (void)
{
	Classic_ClearParticles ();
#ifdef GLQUAKE
	QMB_ClearParticles ();
#endif
}

void R_DrawParticles (void)
{	
#ifdef GLQUAKE
	QMB_DrawParticles ();
#endif
	Classic_DrawParticles ();//R00k draw classic particles after QMB to fix "triangle particle bug" in particle_mode 2 (ugh!)
}

void R_ColorMappedExplosion (vec3_t org, int colorStart, int colorLength)
{
#ifdef GLQUAKE
	if (qmb_initialized && gl_part_explosions.value)
		QMB_ColorMappedExplosion (org, colorStart, colorLength);
	else
#endif
		Classic_ColorMappedExplosion (org, colorStart, colorLength);
}

#define RunParticleEffect(var, org, dir, color, count)		\
	if (qmb_initialized && gl_part_##var.value)				\
		QMB_RunParticleEffect (org, dir, color, count);		\
	else													\
		Classic_RunParticleEffect (org, dir, color, count);

void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
#ifndef GLQUAKE
	Classic_RunParticleEffect (org, dir, color, count);
#else
	if (color == 73 || color == 225)
	{
		RunParticleEffect(blood, org, dir, color, count);
		return;
	}

	switch (count)
	{
	case 10:
	case 20:
	case 30:
		RunParticleEffect(sparks, org, dir, color, count);
		break;
	default:
		RunParticleEffect(gunshots, org, dir, color, count);
	}
#endif
}

void R_RocketTrail (vec3_t start, vec3_t end, int type)
{
#ifdef GLQUAKE
	if (qmb_initialized && gl_part_trails.value)
		QMB_RocketTrail (start, end, type);
	else
#endif
		Classic_RocketTrail (start, end, type);
}

//R00k: added
void R_EntityParticles (entity_t *ent)
{
#ifdef GLQUAKE
	if (qmb_initialized && gl_part_trails.value)
		QMB_EntityParticles (ent);
	else
#endif
		Classic_EntityParticles (ent);
}

#ifdef GLQUAKE
#define ParticleFunction(var, name)				\
void R_##name (vec3_t org) {					\
	if (qmb_initialized && gl_part_##var.value)	\
		QMB_##name (org);						\
	else										\
		Classic_##name (org);					\
}
#else
#define ParticleFunction(var, name)				\
void R_##name (vec3_t org) {					\
	Classic_##name (org);						\
}
#endif


ParticleFunction(explosions, ParticleExplosion);
ParticleFunction(blobs, BlobExplosion);
ParticleFunction(lavasplash, LavaSplash);
ParticleFunction(telesplash, TeleportSplash);

