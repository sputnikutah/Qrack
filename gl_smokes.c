#include "quakedef.h"

//gl_smokes.c
//
//independant particle render by MHQuake
//adds rising fog particle from lava/slime texture chains
//
//TODO use vertex arrays as this is REALLY slow!

int smoketexture;
float s_realtime;
float s_oldrealtime = 0;
float s_frametime;


// keep this high enough to kill framerate dependency as much as possible
#define NUM_SMOKE_PARTICLES 256//was 1024
#define LAVA_SMOKE_SCALE 12

float PRGB[3][256];

typedef struct sparticle_s
{
	float color;
	vec3_t org;
	vec3_t vel;
	float radius;
	float alpha;
} sparticle_t;


sparticle_t R_SmokeParticles[NUM_SMOKE_PARTICLES];

void R_InitSmokeParticle (sparticle_t *p)
{
	// set the base particle state
	if (rand () & 1)
	p->color = 0 + (rand () % 7);
	else p->color = 96 + (rand () % 7);

	p->org[0] = -2 + (rand () % 5);
	p->org[1] = -2 + (rand () % 5);
	p->org[2] = -2 + (rand () % 5);

	p->vel[0] = p->vel[1] = p->vel[2] = 0;

	p->radius = 3;
	p->alpha = 0.866;
}


void R_UpdateSmokeParticle (sparticle_t *p, float updatetime)
{
	p->org[2] += p->vel[2] * updatetime;
	p->radius += (updatetime * 10);//was 30
	p->vel[2] += (updatetime * 16);//was 12
	p->alpha -= (updatetime / 2);
}


int NewParticle = NUM_SMOKE_PARTICLES - 1;

void R_UpdateSmokeParticles (void)
{
	int i;

	for (i = 0; i < NUM_SMOKE_PARTICLES; i++)
	R_UpdateSmokeParticle (&R_SmokeParticles[i], s_frametime);

	// add a new particle to the list in the correct position
	// done here to ensure that only one new particle is added each frame, otherwise the
	// smoke animation experiences some strange blips every now and then
	R_InitSmokeParticle (&R_SmokeParticles[NewParticle]);

	// find the new correct position for the next frame
	if (--NewParticle < 0) NewParticle = NUM_SMOKE_PARTICLES - 1;
}


void R_InitSmokeParticles (void)
{
	int i;
	int j;
	float updatetime;

	for (i = 0; i < NUM_SMOKE_PARTICLES; i++)
	{
		R_InitSmokeParticle (&R_SmokeParticles[i]);

		// run a fake update to simulate adding an extra particle each frame
		for (j = 0; j < i; j++)
		{
			updatetime = 6 + (rand () % 9);
			updatetime /= 1000.0;

			R_UpdateSmokeParticle (&R_SmokeParticles[j], updatetime);
		}
	}
}


void R_InitSmokes (void)
{
	byte basedata[32][32] =
	{
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 3, 3, 4, 4, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 3, 4, 5, 7, 6, 6, 6, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 3, 3, 3, 4, 6, 8, 10, 8, 6, 4, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 6, 6, 6, 5, 8, 12, 10, 6, 5, 4, 4, 3, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 1, 3, 5, 8, 11, 11, 11, 12, 12, 11, 7, 7, 6, 5, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 1, 0, 2, 6, 9, 11, 13, 15, 17, 16, 14, 13, 11, 9, 6, 4, 3, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 1, 1, 1, 4, 8, 12, 14, 16, 20, 22, 24, 24, 24, 22, 16, 12, 7, 5, 4, 2, 3, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 1, 3, 3, 2, 5, 8, 12, 12, 18, 26, 29, 32, 35, 33, 28, 23, 18, 11, 6, 3, 2, 3, 1, 1, 1, 0, 0, 0, 0},
		{0, 0, 0, 1, 3, 3, 5, 7, 11, 15, 15, 18, 32, 38, 39, 40, 37, 29, 23, 22, 17, 11, 6, 3, 3, 2, 1, 1, 1, 0, 0, 0},
		{0, 0, 1, 1, 3, 4, 8, 11, 19, 21, 20, 27, 36, 42, 41, 38, 37, 27, 24, 25, 22, 16, 11, 5, 4, 3, 3, 3, 1, 0, 0, 0},
		{0, 0, 1, 2, 5, 7, 11, 17, 23, 21, 22, 34, 39, 46, 43, 36, 31, 27, 22, 21, 20, 15, 11, 8, 5, 4, 6, 5, 3, 1, 0, 0},
		{0, 0, 2, 4, 7, 11, 16, 24, 25, 20, 27, 39, 45, 51, 50, 36, 21, 19, 20, 20, 18, 14, 10, 6, 4, 5, 8, 7, 5, 2, 0, 0},
		{0, 0, 3, 6, 10, 13, 19, 25, 27, 25, 36, 43, 45, 48, 48, 36, 29, 25, 25, 24, 20, 15, 10, 9, 9, 10, 9, 7, 4, 3, 0, 0},
		{0, 0, 2, 5, 8, 14, 18, 23, 27, 27, 37, 44, 43, 43, 41, 41, 40, 36, 32, 28, 22, 17, 17, 17, 16, 13, 9, 6, 5, 3, 1, 0},
		{0, 0, 2, 5, 9, 13, 17, 21, 26, 27, 33, 37, 36, 38, 39, 42, 48, 45, 37, 34, 27, 18, 21, 22, 19, 15, 11, 6, 6, 4, 2, 0},
		{0, 1, 3, 5, 10, 14, 17, 22, 27, 26, 26, 28, 33, 40, 39, 40, 45, 43, 36, 34, 29, 23, 24, 26, 22, 18, 11, 7, 6, 3, 1, 0},
		{0, 0, 2, 6, 11, 15, 17, 24, 26, 21, 21, 23, 31, 34, 37, 36, 41, 41, 31, 34, 28, 24, 26, 26, 21, 18, 11, 6, 3, 2, 0, 0},
		{0, 0, 2, 4, 11, 16, 19, 22, 22, 18, 18, 17, 19, 21, 24, 32, 36, 37, 34, 34, 29, 24, 24, 22, 18, 12, 11, 7, 3, 1, 0, 0},
		{0, 0, 2, 4, 11, 15, 19, 22, 23, 18, 14, 11, 11, 12, 18, 26, 30, 33, 38, 35, 28, 23, 23, 20, 17, 12, 10, 6, 3, 0, 0, 0},
		{0, 0, 2, 5, 10, 13, 17, 19, 19, 15, 13, 8, 7, 11, 13, 19, 23, 24, 28, 31, 27, 24, 20, 17, 13, 10, 9, 6, 2, 0, 0, 0},
		{0, 0, 0, 4, 7, 9, 13, 15, 13, 11, 8, 6, 6, 13, 15, 19, 19, 18, 17, 16, 17, 15, 14, 11, 8, 6, 6, 4, 2, 0, 0, 0},
		{0, 0, 0, 2, 4, 7, 10, 12, 10, 8, 5, 4, 6, 12, 16, 20, 17, 14, 8, 8, 9, 9, 10, 10, 6, 4, 3, 3, 1, 0, 0, 0},
		{0, 0, 0, 0, 2, 5, 7, 8, 9, 8, 5, 6, 9, 12, 16, 19, 14, 8, 8, 6, 7, 8, 10, 9, 6, 3, 3, 1, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 2, 4, 5, 6, 6, 5, 6, 8, 11, 12, 11, 6, 4, 6, 7, 9, 12, 9, 8, 5, 3, 2, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 1, 2, 4, 5, 5, 6, 6, 7, 8, 5, 3, 0, 3, 6, 8, 9, 8, 6, 3, 2, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 3, 3, 3, 4, 4, 3, 2, 0, 2, 6, 7, 7, 6, 3, 2, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 3, 3, 2, 1, 0, 0, 0, 2, 5, 5, 3, 2, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	};

	byte smokedata[32][32][4];
	int x, y;

	for (x = 0; x < 32; x++)
	{
		for (y = 0; y < 32; y++)
		{
			smokedata[x][y][0] = 255;
			smokedata[x][y][1] = 255;
			smokedata[x][y][2] = 255;
			smokedata[x][y][3] = basedata[x][y];
		}
	}

	smoketexture = texture_extension_number++;

	GL_Bind (smoketexture);

	// you'll need to modify this for your own texture loader
	// make sure you mipmap these for more rendering speed
	GL_Upload32 ((unsigned *) smokedata, 32, 32, TEX_MIPMAP | TEX_ALPHA);

	// init the particles and run 512 fake frames to get a good initial state
	R_InitSmokeParticles ();

	for (x = 0; x < 512; x++)
	{
		// generate a fake frametime, keep it random for validity
		s_frametime = 6 + (rand () % 9);
		s_frametime /= 1000.0;

		// run a fake update of the particles
		R_UpdateSmokeParticles ();
	}

	for (x = 0; x < 256; x++)
	{
		PRGB[0][x] = 0.00392156862745 * ((byte *) &d_8to24table[x])[0];
		PRGB[1][x] = 0.00392156862745 * ((byte *) &d_8to24table[x])[1];
		PRGB[2][x] = 0.00392156862745 * ((byte *) &d_8to24table[x])[2];
	}
}



vec3_t pUp, pRight, pOrg;


void R_DrawSmokeEffect (vec3_t org, float ofs)
{
	int i;
	int base = abs (lrintf(ofs) % 8);

	// this is just a container to keep things neat for the particle currently being drawn
	sparticle_t p;

	for (i = base; i < NUM_SMOKE_PARTICLES; i += 8)
	{
		// don't add anything with negative alpha
		// (allow ourselves to take some of the particles with negative alpha to get a better height range,
		// we'll add 0.25 to the alpha before drawing)
		if (R_SmokeParticles[i].alpha < -0.25) continue;

		// don't draw if the particle is completely under the lava
		if (org[2] + R_SmokeParticles[i].org[2] * (LAVA_SMOKE_SCALE / 4) + ofs < org[2] - 4) continue;

		p.org[0] = org[0] + R_SmokeParticles[i].org[0] * LAVA_SMOKE_SCALE;
		p.org[1] = org[1] + R_SmokeParticles[i].org[1] * LAVA_SMOKE_SCALE;
		p.org[2] = org[2] + R_SmokeParticles[i].org[2] * (LAVA_SMOKE_SCALE / 4) + ofs;

		// add 0.25 to alpha to compensate for the negative alpha of 0.25 we accepted above
		p.alpha = R_SmokeParticles[i].alpha + 0.25;

		// scale it down to make a "haze" rather than a "smoke" effect
		p.alpha /= 2;

		// clamp the range
		if (p.alpha > 0.9) p.alpha = 0.9;//was 0.5
		if (p.alpha < 0) p.alpha = 0;

		// you'll need to tweak this depending on your engine's colour balance
		// *= 2 is fine for Qrack, don't multiply for MHQuake
		p.alpha *= 2;

		// these particles will be off-center, so correct that by calcing a new origin
		// to offset the verts from, which is half the offset away in each direction
		pOrg[0] = p.org[0] - (pRight[0] + pUp[0]) / 2;
		pOrg[1] = p.org[1] - (pRight[1] + pUp[1]) / 2;
		pOrg[2] = p.org[2] - (pRight[2] + pUp[2]) / 2;

		glColor4f (PRGB[0][(int) R_SmokeParticles[i].color],PRGB[1][(int) R_SmokeParticles[i].color],PRGB[2][(int) R_SmokeParticles[i].color],p.alpha);

		glTexCoord2f (0, 0);
		glVertex3fv (pOrg);

		glTexCoord2f (1, 0);
		glVertex3f (pOrg[0] + pUp[0], pOrg[1] + pUp[1], pOrg[2] + pUp[2]);

		glTexCoord2f (1, 1);
		glVertex3f (pOrg[0] + pRight[0] + pUp[0], pOrg[1] + pRight[1] + pUp[1], pOrg[2] + pRight[2] + pUp[2]);

		glTexCoord2f (0, 1);
		glVertex3f (pOrg[0] + pRight[0], pOrg[1] + pRight[1], pOrg[2] + pRight[2]);
	}
}


void R_TorchSmoke (vec3_t org, float ofs)
{
	int i;

	// this is just a container to keep things neat for the particle currently being drawn
	sparticle_t p;

	for (i = 0; i < NUM_SMOKE_PARTICLES; i++)
	{
		// don't add anything with negative alpha
		// (allow ourselves to take some of the particles with negative alpha to get a better height range,
		// we'll add 0.25 to the alpha before drawing)
		if (R_SmokeParticles[i].alpha < 0) continue;

		p.org[0] = org[0] + R_SmokeParticles[i].org[0];
		p.org[1] = org[1] + R_SmokeParticles[i].org[1];
		p.org[2] = org[2] + R_SmokeParticles[i].org[2] + ofs;

		// calculate the scaled vertex offset factors
		VectorScale (vup, R_SmokeParticles[i].radius, pUp);
		VectorScale (vright, R_SmokeParticles[i].radius, pRight);

		// these particles will be off-center, so correct that by calcing a new origin
		// to offset the verts from, which is half the offset away in each direction

		pOrg[0] = p.org[0] - (pRight[0] + pUp[0]) / 2;
		pOrg[1] = p.org[1] - (pRight[1] + pUp[1]) / 2;
		pOrg[2] = p.org[2] - (pRight[2] + pUp[2]) / 2;

		glColor4f (PRGB[0][(int) R_SmokeParticles[i].color], PRGB[1][(int) R_SmokeParticles[i].color], PRGB[2][(int) R_SmokeParticles[i].color], R_SmokeParticles[i].alpha * 1.5);

		glTexCoord2f (0, 0);
		glVertex3fv (pOrg);

		glTexCoord2f (1, 0);
		glVertex3f (pOrg[0] + pUp[0], pOrg[1] + pUp[1], pOrg[2] + pUp[2]);

		glTexCoord2f (1, 1);
		glVertex3f (pOrg[0] + pRight[0] + pUp[0], pOrg[1] + pRight[1] + pUp[1], pOrg[2] + pRight[2] + pUp[2]);

		glTexCoord2f (0, 1);
		glVertex3f (pOrg[0] + pRight[0], pOrg[1] + pRight[1], pOrg[2] + pRight[2]);
	}
}

extern int particle_mode;
void R_SmokeFrame (void)
{
	int i;
	msurface_t *surf;
	glpoly_t *p;
	char *name;
	vec3_t	distance;

	if (!particle_mode)
	return;

	s_realtime = realtime;
	s_frametime = s_realtime - s_oldrealtime;
	s_oldrealtime = s_realtime;

	// calculate the scaled vertex offset factors
	VectorScale (vup, 64, pUp);
	VectorScale (vright, 64, pRight);

	glDepthMask (GL_FALSE);
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA,	GL_ONE_MINUS_SRC_ALPHA); //GL_DST_ALPHA);
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	GL_DisableMultitexture ();
	GL_Bind (smoketexture);
	glBegin (GL_QUADS);

	// do lava smoke/haze
	for (i = 0, surf = cl.worldmodel->surfaces; i < cl.worldmodel->numsurfaces; i++, surf++)
	{
		if (!surf->flags & SURF_DRAWTURB) continue;
		if (surf->visframe != r_framecount) continue;

		// some major optimization potential here...
		name = surf->texinfo->texture->name;

		// don't check 0 cos some texture loaders may change it!!!
		if ((!(name[1] == 'l' && name[2] == 'a' && name[3] == 'v' && name[4] == 'a')))//lava only
			continue;

		// ok, we have a lava surf which is visible
		for (p = surf->polys; p; p = p->next)
		{
			//R00k added distance checking, default 512
			VectorSubtract(r_refdef.vieworg,p->midpoint, distance);

			if (VectorLength(distance) > r_farclip.value)
			continue;

			R_DrawSmokeEffect (p->midpoint, p->fxofs);
		}
	}

	// now do torch smoke
	for (i = 0; i < cl_numvisedicts; i++)
	{
		if ((cl_visedicts[i]->model->type != mod_alias) && (cl_visedicts[i]->model->type != mod_md3))
		continue;

		name = cl_visedicts[i]->model->name;

		// is it a "flame" model?
		if (!(name[6] == 'f' && name[7] == 'l' && name[8] == 'a' && name[9] == 'm' && name[10] == 'e'))
		continue;

		R_TorchSmoke (cl_visedicts[i]->origin, 14);
	}

	glEnd ();

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glColor4f (1, 1, 1, 1);

	glDisable (GL_BLEND);
	glDepthMask (GL_TRUE);
	if (!cl.paused)
	R_UpdateSmokeParticles ();
}