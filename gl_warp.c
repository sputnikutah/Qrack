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
// gl_warp.c -- sky and water polygons

#include "quakedef.h"

extern	model_t	*loadmodel;
extern	msurface_t	*skychain;
extern	msurface_t	**skychain_tail;

int solidskytexture, alphaskytexture;
static	float	speedscale, speedscale2;	// for top sky and bottom sky

static	msurface_t	*warpface;

qboolean	r_skyboxloaded;

extern	cvar_t	gl_subdivide_size;
extern	cvar_t	gl_textureless;
extern	byte *StringToRGB (char *s);

extern	cvar_t	gl_fogenable, gl_fogdensity, gl_fogred, gl_foggreen, gl_fogblue;
extern	cvar_t	r_skyspeed;
/*
=====================================================================================
MH IMPROVED SKY WARP BEGIN
=====================================================================================
*/
float sky_fog[4];
int spherelist = -1;
/*
=====================================================================================
MH IMPROVED SKY WARP END
=====================================================================================
*/

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int	i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
	{
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}

void SubdividePolygon (int numverts, float *verts)
{
	int			i, j, k, f, b;
	vec3_t		mins, maxs, front[64], back[64];
	float		m, *v, dist[64], frac, s, t, subdivide_size;
	glpoly_t	*poly;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	subdivide_size = max(1, gl_subdivide_size.value);

	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = subdivide_size * floor (m/subdivide_size + 0.5);

		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v+=3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v-=i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v+=3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j+1] > 0))
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

//	poly = Hunk_AllocName (sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float),"glpoly_t");
	poly = MallocZ(sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	poly->midpoint[0] = poly->midpoint[1] = poly->midpoint[2] = 0;

	for (i = 0 ; i < numverts ; i++, verts += 3)
	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		// mh - speed up water rendering
		if (subdivide_size == 64)
		{
			s = s / 64;
			t = t / 64;
		}
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
		poly->verts[i][5] = 0;

		poly->midpoint[0] += poly->verts[i][0];
		poly->midpoint[1] += poly->verts[i][1];
		poly->midpoint[2] += poly->verts[i][2];
	}

	poly->midpoint[0] /= (float) numverts;
	poly->midpoint[1] /= (float) numverts;
	poly->midpoint[2] /= (float) numverts;

	poly->fxofs = (rand () % 64) - 64; //This randomly offsets for random raindrops
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface (msurface_t *fa)
{
	vec3_t		verts[64];
	int		numverts, i, lindex;
	float		*vec;

	warpface = fa;

	// convert edges back to a normal polygon
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}
	SubdividePolygon (numverts, verts[0]);
}

//=========================================================

void EmitFlatPoly (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int		i;
	
	if (gl_fogenable.value)
		glEnable(GL_FOG);

	vaEnableVertexArray (3);
	vaEnableTexCoordArray (GL_TEXTURE0, VA_TEXTURE0, 2);
	vaEnableTexCoordArray (GL_TEXTURE1, VA_TEXTURE1, 2);

	for (p = fa->polys ; p ; p = p->next)
	{
		vaBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0] ; i < p->numverts ; i++, v += VERTEXSIZE)
		{
			vaTexCoord2f (0,0);	//stop flickering animated textures			
			vaVertex3fv (v);
		}
		vaDrawArrays();
		vaEnd ();
	}
	vaDisableArrays();

	if (gl_fogenable.value)
		glDisable(GL_FOG);
}
/*
float	turbsin[] =
{
	#include "gl_warp_sin.h"
};
*/
//MH- matches gl_warp_sin.h but with more decimal places precision
float turbsin[256];

void R_InitTurbSin (void)
{
	int i;

	for (i = 0; i < 256; i++)
	{
		float f = i;
		f /= 256;
		f *= 360;
		turbsin[i] = sin (f / 180 * M_PI) * 8;
	}
}
/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys(msurface_t *fa)
{
	glpoly_t	*p;
	float		*v, s, t, os, ot;
	int			i;
	vec3_t		nv;

	if (gl_fogenable.value)
		glEnable(GL_FOG);

	GL_Bind(fa->texinfo->texture->gl_texturenum);

	if ((r_fastturb.value)||(gl_textureless.value))
	{
		EmitFlatPoly(fa);
	}
	else
	{
		vaEnableVertexArray (3);
		vaEnableTexCoordArray (GL_TEXTURE0, VA_TEXTURE0, 2);
		vaEnableTexCoordArray (GL_TEXTURE1, VA_TEXTURE1, 2);

		for (p = fa->polys ; p ; p = p->next)
		{
			vaBegin (GL_POLYGON);
			for (i = 0, v = p->verts[0] ; i < p->numverts ; i++, v += VERTEXSIZE)
			{				
				os = v[3];//todo:cvar scale
				ot = v[4];//cvar scale

				s = os + turbsin[(int)((ot*0.125+cl.time) * TURBSCALE) & 255];
				s *= (1.0/(int)gl_subdivide_size.value);

				t = ot + turbsin[(int)((os*0.125+cl.time) * TURBSCALE) & 255];
				t *= (1.0/(int)gl_subdivide_size.value);		
				
				nv[0] = v[0];
				nv[1] = v[1];
				nv[2] = v[2];
			
				if (r_waterripple.value) 
				{
					nv[2] = v[2] + bound(0, r_waterripple.value, 3)*sin(v[0]*0.02+cl.time)*sin(v[1]*0.02+cl.time)*sin(v[2]*0.02+cl.time);
				}

				//glTexCoord2f (s+(sin(cl.time)/16), t+(cos(cl.time)/16));
				vaTexCoord2f (s, t);				
				vaVertex3fv (nv);
			}			 
			vaDrawArrays();
			vaEnd ();
		}	
		vaDisableArrays();
	}

	if (gl_fogenable.value)
		glDisable(GL_FOG);
}

/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolys (msurface_t *fa, qboolean mtex)
{
	glpoly_t	*p;
	float		*v, s, t, ss, tt, length;
	int		i;
	vec3_t		dir;

	for (p = fa->polys ; p ; p = p->next)
	{
		glBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0] ; i < p->numverts ; i++, v += VERTEXSIZE)
		{
				VectorSubtract (v, r_origin, dir);
				dir[2] *= 3;	// flatten the sphere

				length = VectorLength (dir);
				length = 6 * 63 / length;

				dir[0] *= length;
				dir[1] *= length;

				if (mtex)
				{
					s = (speedscale + dir[0]) * (1.0 / 128);
					t = (speedscale + dir[1]) * (1.0 / 128);

					ss = (speedscale2 + dir[0]) * (1.0 / 128);
					tt = (speedscale2 + dir[1]) * (1.0 / 128);
				}
				else
				{
					s = (speedscale + dir[0]) * (1.0 / 128);
					t = (speedscale + dir[1]) * (1.0 / 128);
				}
			
			if (mtex)
			{
				qglMultiTexCoord2f (GL_TEXTURE0, s, t);
				qglMultiTexCoord2f (GL_TEXTURE1, ss, tt);
			}
			else
			{
				glTexCoord2f (s, t);
			}

			glVertex3fv (v);
		}
		 
		glEnd ();
	}
	 
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain (void)
{
	msurface_t	*fa;
//	byte		*col;
	float speed = (bound(0,r_skyspeed.value,100));

	if (!skychain)
		return;		

	GL_DisableMultitexture ();

	if ((r_fastsky.value) || (gl_textureless.value))
	{
		GL_Bind (alphaskytexture);
		for (fa = skychain ; fa ; fa = fa->texturechain)
			EmitFlatPoly(fa);
	}
	else
	{
		if (gl_mtexable)
		{
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

			GL_Bind (solidskytexture);
			
			GL_EnableMultitexture ();
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			GL_Bind (alphaskytexture);

			if (speed)
			{
				speedscale = cl.time * speed / 2;
				speedscale -= (int)speedscale & ~127;
				speedscale2 = cl.time * speed;
				speedscale2 -= (int)speedscale2 & ~127;
			}

			for (fa = skychain ; fa ; fa = fa->texturechain)
				EmitSkyPolys (fa, true);

			GL_DisableMultitexture ();
		}
		else
		{
			GL_Bind (solidskytexture);
			speedscale = cl.time * 8;
			speedscale -= (int)speedscale & ~127;

			for (fa = skychain ; fa ; fa = fa->texturechain)
				EmitSkyPolys (fa, false);

			glEnable (GL_BLEND);
			GL_Bind (alphaskytexture);
			speedscale = cl.time * 16;
			speedscale -= (int)speedscale & ~127;

			for (fa = skychain ; fa ; fa = fa->texturechain)
				EmitSkyPolys (fa, false);

			glDisable (GL_BLEND);
		}
	}

	skychain = NULL;
	skychain_tail = &skychain;
}

/*
=====================================================================================
MH IMPROVED SKY WARP BEGIN
=====================================================================================
*/
#ifndef USEFAKEGL
void R_ClipSky (msurface_t *skychain)
{
	msurface_t	*surf;

	// disable texturing and writes to the color buffer
	glDisable (GL_TEXTURE_2D);
	glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	for (surf = skychain; surf; surf = surf->texturechain)
	{
		EmitFlatPoly (surf);
	}

	// revert
	glEnable (GL_TEXTURE_2D);
	glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// need to reset primary colour to full as well
	// as colormask can set it to black on some implementations
	glColor3f (1, 1, 1);
}

/*
=================
R_DrawSkyChain

Hot damn Jethro, this sho' is purdy!!!
=================
*/
float rotateBack = 0;
float rotateFore = 0;
extern int	solidskytexture2, alphaskytexture2;
void R_DrawSkyChain2 (void)
{
	// sky scaling
	float fullscale;
	float halfscale;
	float reducedfull;
	float reducedhalf;
	extern cvar_t gl_finish;

	// do these calcs even if we're not drawing
	// sky rotation
	// bound rotation speed 0 to 100
	if (r_skyspeed.value < 0) r_skyspeed.value = 0;
	if (r_skyspeed.value > 100) r_skyspeed.value = 100;

	// always rotate even if we're paused!!!
	rotateBack = anglemod (cl.time * (r_skyspeed.value));
	rotateFore = anglemod (cl.time * (3 * r_skyspeed.value));

	if (!skychain)
		return;

	// write the regular sky polys into the depth buffer to get a baseline
	R_ClipSky (skychain);

	// calculate the scales
	fullscale = r_farclip.value / 4096;
	halfscale = r_farclip.value / 8192;
	reducedfull = (r_farclip.value / 4096) * 0.9;
	reducedhalf = (r_farclip.value / 8192) * 0.9;

	// switch the depth func so that the regular polys will prevent sphere polys outside their area reaching the framebuffer
	glDepthFunc (GL_GEQUAL);
	glDepthMask (GL_FALSE);

	skychain = NULL;
	skychain_tail = &skychain;

	// turn on fogging.  fog looks good on the skies - it gives them a more 
	// "airy" far-away look, and has the knock-on effect of preventing the 
	// old "texture distortion at the poles" problem.

	if (!(gl_fogenable.value))
	{
		glFogf (GL_FOG_START, -r_farclip.value);
		// must tweak the fog end too!!!
		glFogf (GL_FOG_END, r_farclip.value);
		glFogi (GL_FOG_MODE, GL_LINEAR);
		glFogfv (GL_FOG_COLOR, sky_fog);
		glEnable (GL_FOG);
	}

	// sky texture scaling
	// previous releases made a tiled version of the sky texture.  here i just shrink it using the
	// texture matrix, for much the same effect
	glMatrixMode (GL_TEXTURE);
	glLoadIdentity ();
	glScalef (2, 1, 1);
	glMatrixMode (GL_MODELVIEW);

	// background
	// ==========
	// go to a new matrix
	glPushMatrix ();

	// center it on the players position
	glTranslatef (r_origin[0], r_origin[1], r_origin[2]);

	// flatten the sphere
	glScalef (fullscale, fullscale, halfscale);

	// orient it so that the poles are unobtrusive
	glRotatef (-90, 1, 0, 0);

	// make it not always at right angles to the player
	glRotatef (-22, 0 ,1, 0);

	// rotate it around the poles
	glRotatef (-rotateBack, 0, 0, 1);

	// solid sky texture
	GL_Bind (solidskytexture2);

	// draw the sphere
	glCallList (spherelist);

	// restore the previous matrix
	glPopMatrix ();
		
	// foreground
	// ==========
	// go to a new matrix
	glPushMatrix ();
	glEnable (GL_ALPHA_TEST);
	// center it on the players position
	glTranslatef (r_origin[0], r_origin[1], r_origin[2]);

	// flatten the sphere and shrink it a little - the reduced scale prevents artefacts appearing when
	// corners on the skysphere may potentially overlap
	glScalef (reducedfull, reducedfull, reducedhalf);

	// orient it so that the poles are unobtrusive
	glRotatef (-90, 1, 0, 0);

	// make it not always at right angles to the player
	glRotatef (-22, 0 ,1, 0);

	// rotate it around the poles
	glRotatef (-rotateFore, 0, 0, 1);

	// blend mode
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_COLOR, GL_SRC_ALPHA);

	// alpha sky texture
	GL_Bind (alphaskytexture2);

	// draw the sphere
	glCallList (spherelist);

	// back to normal mode
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_BLEND);
	glDisable (GL_ALPHA_TEST);

	// restore the previous matrix
	glPopMatrix ();

	// turn off fog
	glDisable (GL_FOG);

	glMatrixMode (GL_TEXTURE);
	glLoadIdentity ();
	glMatrixMode (GL_MODELVIEW);

	// revert the depth func
	glDepthFunc (GL_LEQUAL);
	glDepthMask (GL_TRUE);

	R_ClipSky (skychain);

	// run a pipeline flush
	if (gl_finish.value)//R00k is this required??
		glFinish ();
}

/*
==============
R_DrawSphere

Draw a sphere!!!  The verts and texcoords are precalculated for extra efficiency.  The sphere is put into
a display list to reduce overhead even further.
==============
*/
float skytexes[440];
float skyverts[660];


void R_DrawSphere (void)
{
	if (spherelist == -1)
	{
		int i;
		int j;

		int vertspos = 0;
		int texespos = 0;

		// build the sphere display list
		spherelist = glGenLists (1);

		glNewList (spherelist, GL_COMPILE);

		for (i = 0; i < 10; i++)
		{
			glBegin (GL_TRIANGLE_STRIP);

			for (j = 0; j <= 10; j++)
			{
				glTexCoord2fv (&skytexes[texespos]);
				glVertex3fv (&skyverts[vertspos]);

				texespos += 2;
				vertspos += 3;

				glTexCoord2fv (&skytexes[texespos]);
				glVertex3fv (&skyverts[vertspos]);

				texespos += 2;
				vertspos += 3;
			}

			glEnd ();
		}

		glEndList ();
	}
}


/*
==============
R_InitSkyChain

Initialize the sky chain arrays
==============
*/
void R_InitSkyChain (void)
{
	float drho = 0.3141592653589;
	float dtheta = 0.6283185307180;

	float ds = 0.1;
	float dt = 0.1;

	float t = 1.0f;
	float s = 0.0f;

	int i;
	int j;

	int vertspos = 0;
	int texespos = 0;

	for (i = 0; i < 10; i++)
	{
		float rho = (float) i * drho;
		float srho = (float) (sin (rho));
		float crho = (float) (cos (rho));
		float srhodrho = (float) (sin (rho + drho));
		float crhodrho = (float) (cos (rho + drho));

		s = 0.0f;

		for (j = 0; j <= 10; j++)
		{
			float theta = (j == 10) ? 0.0f : j * dtheta;
			float stheta = (float) (-sin( theta));
			float ctheta = (float) (cos (theta));

			skytexes[texespos++] = s * 2;
			skytexes[texespos++] = t * 2;

			skyverts[vertspos++] = stheta * srho * 4096.0;
			skyverts[vertspos++] = ctheta * srho * 4096.0;
			skyverts[vertspos++] = crho * 4096.0;

			skytexes[texespos++] = s * 2;
			skytexes[texespos++] = (t - dt) * 2;

			skyverts[vertspos++] = stheta * srhodrho * 4096.0;
			skyverts[vertspos++] = ctheta * srhodrho * 4096.0;
			skyverts[vertspos++] = crhodrho * 4096.0;

			s += ds;
		}

		t -= dt;
	}
}
#endif

void R_InitSky (miptex_t *mt)
{
	int			i, j, p;
	byte		*src;
	unsigned	transpix;
	int			r, g, b;
	unsigned	*rgba;
	unsigned	topalpha;
	int			div;
	unsigned	*trans;

	if (mt->height >= mt->width)
		return;
	// initialize our pointers
	trans = Q_malloc (128 * 128 * 4);

	// -----------------
	// solid sky texture
	// -----------------
	src = (byte *)mt + mt->offsets[0];

	// make an average value for the back to avoid
	// a fringe on the top level
	p = r = g = b = 0;

	for (i=0 ; i<128 ; i++)
	{
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i*128) + j] = *rgba;

			r += ((byte *)rgba)[0];
			g += ((byte *)rgba)[1];
			b += ((byte *)rgba)[2];
		}
	}

	((byte *)&transpix)[0] = r/(16384);
	((byte *)&transpix)[1] = g/(16384);
	((byte *)&transpix)[2] = b/(16384);
	((byte *)&transpix)[3] = 0;

	if (!solidskytexture) 
		solidskytexture = texture_extension_number++;

	GL_Bind (solidskytexture);

	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, trans);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// -----------------
	// alpha sky texture
	// -----------------
	// get another average cos the bottom layer average can be too dark
	p = div = r = g = b = 0;

	for (i=0 ; i<128 ; i++)
	{
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j];

			if (p == 0)
				topalpha = transpix;
			else
			{
				rgba = &d_8to24table[p];

				r += ((byte *)rgba)[0];
				g += ((byte *)rgba)[1];				
				b += ((byte *)rgba)[2];

				div++;

				topalpha = d_8to24table[p];
			}

			((byte *)&topalpha)[3] = ((byte *)&topalpha)[3] / 2;

			trans[(i*128) + j] = topalpha;
		}
	}

	if (!alphaskytexture) 
		alphaskytexture = texture_extension_number++;

	GL_Bind (alphaskytexture);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, trans);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// get fog colours for the sky - we halve these
	sky_fog[0] = ((float)r / (float)div) / 512.0;
	sky_fog[1] = ((float)g / (float)div) / 512.0;
	sky_fog[2] = ((float)b / (float)div) / 512.0;
	sky_fog[3] = 0.1;

	// free the used memory
	Q_free (trans);
#ifndef USEFAKEGL
	// recalc the sphere display list - delete the original one first!!!
	if (spherelist != -1) glDeleteLists (spherelist, 1);
	spherelist = -1;

	R_InitSkyChain ();
	R_DrawSphere ();
#endif
}

/*
=====================================================================================
MH IMPROVED SKY WARP END
=====================================================================================
*/

/*
==================
R_SetSky
==================
*/
char	*skybox_ext[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

int R_SetSky (char *skyname)
{
	int	i, error = 0;
	byte	*data[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

	if (strlen(skyname) == 0) 
	{
		error = 1;
		goto cleanup;		
	}

retry:
	for (i=0 ; i<6 ; i++)
	{
		if (!(data[i] = GL_LoadImagePixels(va("env/%s%s", skyname, skybox_ext[i]), 0, 0, 0)) && 
		    !(data[i] = GL_LoadImagePixels(va("gfx/env/%s%s", skyname, skybox_ext[i]), 0, 0, 0)) && 
		    !(data[i] = GL_LoadImagePixels(va("env/%s_%s", skyname, skybox_ext[i]), 0, 0, 0)) && 
		    !(data[i] = GL_LoadImagePixels(va("gfx/env/%s_%s", skyname, skybox_ext[i]), 0, 0, 0)))
		{
			error += 1;

			if (error < 2)
			{
				strcpy(skyname,prev_skybox);//revert to default
				goto retry;
			}
			else
			{
				Con_DPrintf (1,"Couldn't load skybox \"%s\"\n", skyname);//R00k
				goto cleanup;
			}
		}
	}
	for (i=0 ; i<6 ; i++)
	{
		GL_Bind (skyboxtextures + i);
		GL_Upload32 ((unsigned int *)data[i], image_width, image_height, 0);
	}
	r_skyboxloaded = true;

cleanup:
	for (i=0 ; i<6 ; i++)
	{
		if (data[i])
			free (data[i]);
		else
			break;
	}

	return error;
}

qboolean OnChange_r_skybox (cvar_t *var, char *string)
{
	if (!string[0])
	{
		r_skyboxloaded = false;
		return false;
	}

	if (nehahra)
	{
		Cvar_Set ("r_oldsky", "0");
	}
	strcpy (prev_skybox, string);//R00k

	return R_SetSky (string);
}

void Sky_NewMap (void)
{
	char	key[128], value[4096];
	char	*data;

	Cvar_Set ("r_skybox", "");

	data = cl.worldmodel->entities;

	if (!data)
		return;
	
	data = COM_Parse(data);
	
	if (!data) //should never happen
		return; // error

	if (com_token[0] != '{') //should never happen
		return; // error

	while (1)
	{
		data = COM_Parse(data);
		if (!data)
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strcpy(key, com_token + 1);
		else
			strcpy(key, com_token);
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		data = COM_Parse(data);
		if (!data)
			return; // error
		strcpy(value, com_token);

		if (!strcmp("sky", key))
			Cvar_Set ("r_skybox", value);
	}
}

static	vec3_t	skyclip[6] = 
{
	{1, 1, 0},
	{1, -1, 0},
	{0, -1, 1},
	{0, 1, 1},
	{1, 0, 1},
	{-1, 0, 1} 
};

// 1 = s, 2 = t, 3 = 2048
static	int	st_to_vec[6][3] = 
{
	{3, -1, 2},
	{-3, 1, 2},

	{1, 3, 2},
	{-1, -3, 2},

	{-2, -1, 3},		// 0 degrees yaw, look straight up
	{2, -1, -3}		// look straight down
};

// s = [0]/[2], t = [1]/[2]
static	int	vec_to_st[6][3] = {
	{-2, 3, 1},
	{2, 3, -1},

	{1, 3, 2},
	{-1, 3, -2},

	{-2, -1, 3},
	{-2, 1, -3}
};

static	float	skymins[2][6], skymaxs[2][6];

void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int	i, j, axis;
	vec3_t	v, av;
	float	s, t, dv, *vp;

	// decide which face it maps to
	VectorClear (v);
	for (i = 0, vp = vecs ; i < nump ; i++, vp += 3)
		VectorAdd (vp, v, v);
	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
		axis = (v[0] < 0) ? 1 : 0;
	else if (av[1] > av[2] && av[1] > av[0])
		axis = (v[1] < 0) ? 3 : 2;
	else
		axis = (v[2] < 0) ? 5 : 4;

	// project new texture coords
	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		dv = (j > 0) ? vecs[j - 1] : -vecs[-j - 1];

		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		j = vec_to_st[axis][1];
		t = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

#define	MAX_CLIP_VERTS	64
void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	float		*norm, *v, d, e, dists[MAX_CLIP_VERTS];
	qboolean	front, back;
	int		sides[MAX_CLIP_VERTS], newc[2], i, j;
	vec3_t		newv[2][MAX_CLIP_VERTS];

	if (nump > MAX_CLIP_VERTS-2)
		Sys_Error ("ClipSkyPolygon: nump > MAX_CLIP_VERTS - 2");

	if (stage == 6)
	{	// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i = 0, v = vecs ; i < nump ; i++, v += 3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs + (i*3)));
	newc[0] = newc[1] = 0;

	for (i=0, v=vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;

		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;

		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}

/*
=================
R_AddSkyBoxSurface
=================
*/
void R_AddSkyBoxSurface (msurface_t *fa)
{
	int		i;
	vec3_t		verts[MAX_CLIP_VERTS];
	glpoly_t	*p;

	// calculate vertex values for sky box
	for (p = fa->polys ; p ; p = p->next)
	{
		for (i=0 ; i<p->numverts ; i++)
			VectorSubtract (p->verts[i], r_origin, verts[i]);
		ClipSkyPolygon (p->numverts, verts[0], 0);
	}
}

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox (void) 
{
	int	i;

	for (i=0 ; i<6 ; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

void MakeSkyVec (float s, float t, int axis)
{
	vec3_t		v, b;
	int		j, k, farclip;

	farclip = max((int)r_farclip.value, 4096);

	b[0] = s *	(farclip / sqrt(3.0));
	b[1] = t *	(farclip / sqrt(3.0));
	b[2] =		(farclip / sqrt(3.0));

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k-1] : b[k-1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = (s+1) * 0.5;
	t = (t+1) * 0.5;

	if (s < 1.0/512)
		s = 1.0 / 512;
	else if (s > 511.0/512)
		s = 511.0 / 512;
	if (t < 1.0/512)
		t = 1.0 / 512;
	else if (t > 511.0/512)
		t = 511.0 / 512;

	t = 1.0 - t;
	glTexCoord2f (s, t);
	glVertex3fv (v);
}
/*
==============
R_DrawSkyBox
==============
*/
static	int	skytexorder[6] = {0, 2, 1, 3, 4, 5};

void R_DrawSkyBox (void)
{
	int		i;
	msurface_t	*fa;

	if (!skychain)
		return;
	
	R_ClearSkyBox ();

	for (fa = skychain ; fa ; fa = fa->texturechain)
		R_AddSkyBoxSurface (fa);

	GL_DisableMultitexture ();

	if (gl_fogenable.value)
	{
		glDisable (GL_FOG);
	}
	
	for (i=0 ; i<6 ; i++)
	{
		if (skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])
			continue;

		GL_Bind (skyboxtextures + skytexorder[i]);

		glBegin (GL_QUADS);

		MakeSkyVec (skymins[0][i], skymins[1][i], i);
		MakeSkyVec (skymins[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymins[1][i], i);
		 
		glEnd ();
	}

	glDisable(GL_TEXTURE_2D);
	glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ZERO, GL_ONE);

	for (fa = skychain ; fa ; fa = fa->texturechain)
		EmitFlatPoly (fa);

	glEnable (GL_TEXTURE_2D);
	glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable (GL_BLEND); 
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
	glColor3f (1, 1, 1);

	if (gl_fogenable.value)
		glEnable (GL_FOG);	

	skychain = NULL;
	skychain_tail = &skychain;
}

void EmitCausticsPolys (void)
{
	glpoly_t	*p;
	int			i;
	float		os, ot, s, t, *v;
	extern	glpoly_t	*caustics_polys;
	
	GL_Bind (underwatertexture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	glEnable (GL_BLEND);

	for (p = caustics_polys ; p ; p = p->caustics_chain)
	{
		glBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0] ; i < p->numverts ; i++, v += VERTEXSIZE)
		{
			os = v[7];
			ot = v[8];

			s = os/4 + (cl.time*0.015);
			t = ot/4 + (cl.time*0.015);
				
			glTexCoord2f (s+(sin(cl.time)/32), t+(cos(cl.time)/32));
			glVertex3fv (v);			
		}
		 
		glEnd ();
	}
	 

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);

	caustics_polys = NULL;
}

/*
=============
EmitChromePolys
=============
*/
void EmitChromePolys (void)
{
	glpoly_t	*p;
	int			i;
	float		*v;
	extern	glpoly_t	*chrome_polys;

	if (!chrome_polys) return;

	GL_Bind (chrometexture);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

	glEnable (GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);

	for (p = chrome_polys ; p ; p = p->chrome_chain)
	{
		glBegin (GL_POLYGON);
		v = p->verts[0];

		for (i = 0; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			glTexCoord2f(v[7], v[8]);
			glVertex3fv (v);
		}
		 
		glEnd ();
	}
	 

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	chrome_polys = (NULL);

}
/*
void EmitGlossPolys (void)
{
	glpoly_t	*p;
	int			i;
	float		*v;
	extern	glpoly_t	*gloss_polys;

	if (!gloss_polys) return;

	GL_Bind (glosstexture);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
	glEnable (GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);

	for (p = gloss_polys ; p ; p = p->gloss_chain)
	{
		glBegin (GL_POLYGON);

		v = p->verts[0];

		for (i = 0; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			glTexCoord2f(v[7], v[8]);
			glVertex3fv (v);
		}
		 
		glEnd ();
	}
	 

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	gloss_polys = (NULL);
}
*/
/*
=============
EmitGlassPolys
=============
*/
void EmitGlassPolys (void)
{
	glpoly_t	*p;
	int			i;
	float		*v;
	extern	glpoly_t	*glass_polys;
	extern GLuint r_scaleview_texture;

	if (!glass_polys) return;

	GL_Bind (glasstexture);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
	glEnable (GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);

	for (p = glass_polys ; p ; p = p->glass_chain)
	{
		glBegin (GL_POLYGON);

		v = p->verts[0];

		for (i = 0; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			glTexCoord2f(v[7], v[8]);
			glVertex3fv (v);
		}
		 
		glEnd ();
	}
	 

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glass_polys = (NULL);
}
