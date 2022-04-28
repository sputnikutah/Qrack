/*
Copyright (C) 2001-2002 Charles Hollemeersch

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

Decals are similar to particles currently...
The code to generate them is based on the article "Applying decals to arbitrary surfaces"
by "Eric Lengyel" in "Game Programming Gems 2"
*/

#include "quakedef.h"

float RandomMinMax (float min, float max) 
{
	return min + ((rand() % 10000) / 10000.0) * (max - min);
}

#define DEFAULT_NUM_DECALS		"1024"
#define ABSOLUTE_MIN_DECALS		1
#define ABSOLUTE_MAX_DECALS		32768
#define MAX_DECAL_VERTICES		128
#define MAX_DECAL_TRIANGLES		64

const double decalEpsilon = (0.03125f);
extern void R_ClearDecals (void);
extern int particle_mode;

qboolean OnChange_gl_decals (cvar_t * var, char *value);
cvar_t  gl_decals				= {"gl_decals", "1", true, false, OnChange_gl_decals};
qboolean OnChange_gl_decal_count (cvar_t *var, char *string);
cvar_t  gl_decal_count			= {"gl_decal_count", DEFAULT_NUM_DECALS, true, false, OnChange_gl_decal_count};
cvar_t	gl_decal_blood			= {"gl_decal_blood", "1",true};
cvar_t	gl_decal_bullets		= {"gl_decal_bullets","1",true};
cvar_t	gl_decal_sparks			= {"gl_decal_sparks","1",true};
cvar_t	gl_decal_explosions		= {"gl_decal_explosions","1",true};
cvar_t	gl_decal_quality   		= {"gl_decal_quality","0",true};
cvar_t	gl_decaltime			= {"gl_decaltime", "30",true};
cvar_t	gl_decal_viewdistance	= {"gl_decal_viewdistance","1280",true};
int		decals_enabled		= 0;

qboolean OnChange_gl_decal_count (cvar_t *var, char *string)
{
	float	f;

	f = bound(ABSOLUTE_MIN_DECALS, (atof(string)), ABSOLUTE_MAX_DECALS);
	Cvar_SetValue("gl_decal_count", f);

	R_ClearDecals ();	
	
	return true;
}

qboolean OnChange_gl_decals (cvar_t * var, char *value) 
{
    float val = Q_atof (value);

	decals_enabled = val;

	Cvar_SetValue ("gl_decal_bullets"	, val);
	Cvar_SetValue ("gl_decal_explosions", val);
	Cvar_SetValue ("gl_decal_sparks"	, val);
	Cvar_SetValue ("gl_decal_blood"		, val);
    
	return true;
}

typedef struct decal_s
{
	float		contents;
	vec3_t		origin;
	vec3_t		normal;
	vec3_t		tangent;
	float		radius;

	struct decal_s	*next;
	float		die;
	float		starttime;

	int			srcblend;
	int			dstblend;

	int			texture;
	float		alpha;

	//geometry of decal
	int			vertexCount, triangleCount;
	vec3_t		vertexArray		[MAX_DECAL_VERTICES];
	float		texcoordArray	[MAX_DECAL_VERTICES][2];
	int			triangleArray	[MAX_DECAL_TRIANGLES][3];
} decal_t;

decal_t	*active_decals, *free_decals;
decal_t	*decals;
int		r_max_decals;

void DecalClipLeaf(decal_t *dec, mleaf_t *leaf);
void DecalWalkBsp_R(decal_t *dec, mnode_t *node);
int DecalClipPolygonAgainstPlane(plane_t *plane, int vertexCount, vec3_t *vertex, vec3_t *newVertex);
int DecalClipPolygon(int vertexCount, vec3_t *vertices, vec3_t *newVertex);

void R_InitDecalTextures (void)
{
	int	flags = TEX_MIPMAP | TEX_ALPHA;

	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);

	decal_blood1 = GL_LoadTextureImage ("textures/decals/blood_splat01", NULL, 128, 128, flags | TEX_COMPLAIN);
	decal_blood2 = GL_LoadTextureImage ("textures/decals/blood_splat02", NULL, 128, 128, flags | TEX_COMPLAIN);
	decal_blood3 = GL_LoadTextureImage ("textures/decals/blood_splat03", NULL, 128, 128, flags | TEX_COMPLAIN);
	decal_burn	 = GL_LoadTextureImage ("textures/decals/explo_burn01", NULL, 128, 128, flags | TEX_COMPLAIN);
	decal_mark	 = GL_LoadTextureImage ("textures/decals/particle_burn01", NULL, 64, 64, flags | TEX_COMPLAIN);
	decal_glow	 = GL_LoadTextureImage ("textures/decals/glow2", NULL, 64, 64, flags | TEX_COMPLAIN);
} 

void R_AllocDecals (void) 
{
	extern cvar_t gl_decal_count;

	r_max_decals = bound(ABSOLUTE_MIN_DECALS, gl_decal_count.value, ABSOLUTE_MAX_DECALS);

	if (decals || r_max_decals < 1)
		Sys_Error("R_AllocDecals: internal error");

	decals = (decal_t *) Q_malloc (r_max_decals * sizeof(decal_t));	
}

void R_InitDecals (void)
{
	Cvar_RegisterVariable (&gl_decals);
	Cvar_RegisterVariable (&gl_decaltime);
	Cvar_RegisterVariable (&gl_decal_count);
	Cvar_RegisterVariable (&gl_decal_blood);	
	Cvar_RegisterVariable (&gl_decal_bullets);
	Cvar_RegisterVariable (&gl_decal_sparks);
	Cvar_RegisterVariable (&gl_decal_explosions);
	Cvar_RegisterVariable (&gl_decal_viewdistance);
	Cvar_RegisterVariable (&gl_decal_quality);

	R_AllocDecals();
}

void R_ClearDecals (void)
{
	int		i;

	if (!qmb_initialized)
		return;

	Q_free (decals);	// free
	R_AllocDecals ();	// and alloc again

	free_decals = &decals[0];
	active_decals = NULL;

	for (i=0 ;i + 1 < r_max_decals ; i++)
		decals[i].next = &decals[i + 1];

	decals[r_max_decals - 1].next = NULL;	
}

//--------------------------------------------------------------------------------------------->
static plane_t leftPlane, rightPlane, bottomPlane, topPlane, backPlane, frontPlane;

void R_SpawnDecal(vec3_t center, vec3_t normal, vec3_t tangent, int tex, int size, float dtime, float dalpha)
{
	float	width, height, depth, d, one_over_w, one_over_h;
	vec3_t	binormal, decaldist, test = {0.5, 0.5, 0.5};
	decal_t	*dec, *kill;
	int		a, i;
	
	if (!qmb_initialized)
		return;

	if ((!decals_enabled) || (!particle_mode))
		return;

	//allocate decal
	if (!free_decals)
	{
		return;
	}

	if (tex != decal_mark)//Only draw bulletholes underwater...
	{
		if (ISUNDERWATER(TruePointContents(center)))
			return;
	}

	//R00k added distance checking, default 1024			
	VectorSubtract(r_refdef.vieworg, center, decaldist);		

	if (VectorLength(decaldist) > gl_decal_viewdistance.value)
		return;

//R00k: remove oldest decal from list if max decal limit hit

	i = 1;
	kill = active_decals;

	while ((i < r_max_decals - 1)&&(kill))
	{
		i++;
		kill = kill->next;		
	}
	
	if (kill && (i == r_max_decals - 1))
	{
		kill->die = cl.time - 1;
	}

	dec = free_decals;
	free_decals = dec->next;
	dec->next = active_decals;
	active_decals = dec;

	VectorNormalize(test);
	CrossProduct(normal,test,tangent);

	VectorCopy(center, dec->origin);
	VectorCopy(tangent, dec->tangent);
	VectorCopy(normal, dec->normal);
	
	VectorNormalize(tangent);
	VectorNormalize(normal);
	CrossProduct(normal, tangent, binormal);
	VectorNormalize(binormal);	
	
	width = RandomMinMax(size*0.5, size);
	height = width;

	depth = width*0.5;
	dec->radius = max(max(width,height),depth);
	dec->starttime = cl.time;
	
	if (dtime)
		dec->die = cl.time + dtime;
	else
		dec->die = cl.time + max(5, gl_decaltime.value);

	dec->texture = tex;
	dec->alpha = dalpha;

	// Calculate boundary planes
	d = DotProduct(center, tangent);
	VectorCopy(tangent,leftPlane.normal);
	leftPlane.dist = -(width * 0.5 - d);
	VectorInverse(tangent);
	VectorCopy(tangent,rightPlane.normal);
	VectorInverse(tangent);
	rightPlane.dist = -(width * 0.5 + d);
	
	d = DotProduct(center, binormal);
	VectorCopy(binormal,bottomPlane.normal);
	bottomPlane.dist = -(height * 0.5 - d);
	VectorInverse(binormal);
	VectorCopy(binormal,topPlane.normal);
	VectorInverse(binormal);
	topPlane.dist = -(height * 0.5 + d);
	
	d = DotProduct(center, normal);
	VectorCopy(normal,backPlane.normal);
	backPlane.dist = -(depth - d);
	VectorInverse(normal);
	VectorCopy(normal,frontPlane.normal);
	VectorInverse(normal);
	frontPlane.dist = -(depth + d);

	// Begin with empty mesh
	dec->vertexCount = 0;
	dec->triangleCount = 0;
	
	//Clip decal to bsp
	DecalWalkBsp_R(dec, cl.worldmodel->nodes);

	//This happens when a decal is to far from any surface or the surface is to steeply sloped
	if (dec->triangleCount == 0)
	{
		//deallocate decal
		active_decals = dec->next;
		dec->next = free_decals;
		free_decals = dec;	
		return;
	}

	//Assign texture mapping coordinates
	one_over_w  = 1.0f / width;
	one_over_h = 1.0f / height;

	for (a = 0; a < dec->vertexCount; a++)
	{
		float s, t;
		vec3_t v;
		VectorSubtract(dec->vertexArray[a],center,v);
		s = DotProduct(v, tangent) * one_over_w + 0.5f;
		t = DotProduct(v, binormal) * one_over_h + 0.5f;
		dec->texcoordArray[a][0] = s;
		dec->texcoordArray[a][1] = t;
	}
}

void R_SpawnDecalStatic(vec3_t org, int tex, int size)
{
	int i;
	float bestorg[3], bestnormal[3];
	float v[3], normal[3], org2[3];
	vec3_t tangent;
	extern qboolean TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);
	qboolean foundimpact;

	if (!qmb_initialized)
		return;

	if ((!decals_enabled) || (!particle_mode))
		return;

	if (!free_decals)
	{
		return;
	}	

	if (tex != decal_mark)//Only draw bulletholes underwater...
	{
		if (ISUNDERWATER(TruePointContents(org)))
			return;
	}
	
	VectorClear (bestorg);
	VectorClear (bestnormal);

	for (i = 0 ; i < 22 ; i++)
	{
		if (i == 0)
		{
			org2[0] = 1;
			org2[1] = 0;
			org2[2] = 0;
		}
		else if (i == 1)
		{
			org2[0] = -1;
			org2[1] = 0;
			org2[2] = 0;
		}
		else if (i == 2)
		{
			org2[0] = 0;
			org2[1] = 1;
			org2[2] = 0;
		}
		else if (i == 3)
		{
			org2[0] = 0;
			org2[1] = -1;
			org2[2] = 0;
		}
		else if (i == 4)
		{
			org2[0] = 0;
			org2[1] = 0;
			org2[2] = 1;
		}
		else if (i == 5)
		{
			org2[0] = 0;
			org2[1] = 0;
			org2[2] = -1;
		}
		else if (i == 6)
		{
			org2[0] = 1;
			org2[1] = 1;
			org2[2] = 0;
		}
		else if (i == 7)
		{
			org2[0] = -1;
			org2[1] = 1;
			org2[2] = 0;
		}
		else if (i == 8)
		{
			org2[0] = 1;
			org2[1] = -1;
			org2[2] = 0;
		}
		else if (i == 9)
		{
			org2[0] = -1;
			org2[1] = -1;
			org2[2] = 0;
		}
		else if (i == 10)
		{
			org2[0] = 1;
			org2[1] = 0;
			org2[2] = 1;
		}
		else if (i == 11)
		{
			org2[0] = 1;
			org2[1] = 0;
			org2[2] = -1;
		}
		else if (i == 12)
		{
			org2[0] = -1;
			org2[1] = 0;
			org2[2] = 1;
		}
		else if (i == 13)
		{
			org2[0] = -1;
			org2[1] = 0;
			org2[2] = -1;
		}
		else if (i == 14)
		{
			org2[0] = 1;
			org2[1] = 1;
			org2[2] = 1;
		}
		else if (i == 15)
		{
			org2[0] = -1;
			org2[1] = 1;
			org2[2] = 1;
		}
		else if (i == 16)
		{
			org2[0] = 1;
			org2[1] = -1;
			org2[2] = 1;
		}
		else if (i == 17)
		{
			org2[0] = 1;
			org2[1] = 1;
			org2[2] = -1;
		}
		else if (i == 18)
		{
			org2[0] = -1;
			org2[1] = -1;
			org2[2] = 1;
		}
		else if (i == 19)
		{
			org2[0] = -1;
			org2[1] = 1;
			org2[2] = -1;
		}
		else if (i == 20)
		{
			org2[0] = 1;
			org2[1] = -1;
			org2[2] = -1;
		}
		else if (i == 21)
		{
			org2[0] = -1;
			org2[1] = -1;
			org2[2] = -1;
		}

		if (tex == decal_burn)	// explosion burn
		{
			VectorMA (org, 10, org2, org2);
		}
		else if (tex == decal_mark)	// gun shots, spike shots
		{
			VectorMA (org, 5, org2, org2);
		}
		else if (tex == decal_glow)	// lightning burn
		{
			VectorMA (org, 1, org2, org2);
		}
		foundimpact = TraceLineN (org, org2, v, normal);
		if (!foundimpact)
		{
			continue;
		}
		VectorCopy (v, bestorg);
		VectorCopy (normal, bestnormal);
		CrossProduct (normal, bestnormal, tangent);
		break;
	}
	R_SpawnDecal (bestorg, bestnormal, tangent, tex, size, 0, 1);
}

void DecalWalkBsp_R(decal_t *dec, mnode_t *node)
{
	mplane_t	*plane;
	float		dist;
	mleaf_t		*leaf;

	if (node->contents < 0) 
	{
		//we are in a leaf
		leaf = (mleaf_t *)node;
		DecalClipLeaf(dec, leaf);
		return;
	}

	plane = node->plane;
	dist = DotProduct (dec->origin, plane->normal) - plane->dist;
	
	if (dist > dec->radius)
	{
		DecalWalkBsp_R (dec, node->children[0]);
		return;
	}
	if (dist < -dec->radius)
	{
		DecalWalkBsp_R (dec, node->children[1]);
		return;
	}

	DecalWalkBsp_R (dec, node->children[0]);
	DecalWalkBsp_R (dec, node->children[1]);
}

qboolean DecalAddPolygon(decal_t *dec, int vertcount, vec3_t *vertices)
{
	int *triangle;
	int count;
	int a, b;

	//Con_Printf("AddPolygon %i %i\n",vertcount, dec->vertexCount);
	count = dec->vertexCount;
	if (count + vertcount >= MAX_DECAL_VERTICES)
		return false;
	
	if (dec->triangleCount + vertcount-2 >= MAX_DECAL_TRIANGLES)
		return false;

	// Add polygon as a triangle fan
	triangle = &dec->triangleArray[dec->triangleCount][0];
	for (a = 2; a < vertcount; a++)
	{
		dec->triangleArray[dec->triangleCount][0] = count;
		dec->triangleArray[dec->triangleCount][1] = (count + a - 1);
		dec->triangleArray[dec->triangleCount][2] = (count + a );
		dec->triangleCount++;
	}
	
	// Assign vertex colors
	for (b = 0; b < vertcount; b++)
	{
		VectorCopy(vertices[b],dec->vertexArray[count]);
		count++;
	}
	
	dec->vertexCount = count;
	return true;
}

void DecalClipLeaf(decal_t *dec, mleaf_t *leaf)
{
	vec3_t		newVertex[64], t3;
	int	c;
	msurface_t **surf;

	//Con_Printf("Clipleaf\n");
	c = leaf->nummarksurfaces;
	surf = leaf->firstmarksurface;

	//for all surfaces in the leaf
	for (c=0; c<leaf->nummarksurfaces; c++, surf++) 
	{
		glpoly_t *poly;
		int	i,count;
		float *v;
		poly = (*surf)->polys;

		v = poly->verts[0];

		for (i=0 ; i<poly->numverts ; i++, v+= VERTEXSIZE)
		{
			newVertex[i][0] = v[0];
			newVertex[i][1] = v[1];
			newVertex[i][2] = v[2];
		}

		VectorCopy((*surf)->plane->normal,t3);		
		
		if ((*surf)->flags & SURF_PLANEBACK) 
		{
			VectorInverse(t3);
		}
		
		if ((*surf)->flags & SURF_DRAWSKY)//R00k dont draw decals on sky
		{
			dec->die = cl.time - 1;
		}
		
		if ((*surf)->texinfo->texture->name[0] =='+')//R00k: Dont draw on animated textures.
		{
			dec->die = cl.time - 1;
		}

		if ((*surf)->flags & SURF_DRAWTURB)
		{
			dec->die = cl.time - 1;
		}

		//avoid backfacing and orthogonal facing faces to recieve decal parts
		if (DotProduct(dec->normal, t3) > decalEpsilon)
		{
			count = DecalClipPolygon(poly->numverts, newVertex, newVertex);
			if ((count != 0) && (!DecalAddPolygon(dec, count, newVertex))) break;
		}
	}
}

int DecalClipPolygon(int vertexCount, vec3_t *vertices, vec3_t *newVertex)
{
	vec3_t		tempVertex[64];
	
	// Clip against all six planes
	int count = DecalClipPolygonAgainstPlane(&leftPlane, vertexCount, vertices, tempVertex);
	if (count != 0)
	{
		count = DecalClipPolygonAgainstPlane(&rightPlane, count, tempVertex, newVertex);
		if (count != 0)
		{
			count = DecalClipPolygonAgainstPlane(&bottomPlane, count, newVertex, tempVertex);
			if (count != 0)
			{
				count = DecalClipPolygonAgainstPlane(&topPlane, count, tempVertex, newVertex);
				if (count != 0)
				{
					count = DecalClipPolygonAgainstPlane(&backPlane, count, newVertex, tempVertex);
					if (count != 0)
					{
						count = DecalClipPolygonAgainstPlane(&frontPlane, count, tempVertex, newVertex);
					}
				}
			}
		}
	}
	
	return count;
}

int DecalClipPolygonAgainstPlane(plane_t *plane, int vertexCount, vec3_t *vertex, vec3_t *newVertex)
{
	qboolean	negative[65];
	int	a, count, b, c;
	float t;
	vec3_t v1, v2;

	// Classify vertices
	int negativeCount = 0;
	
	for (a = 0; a < vertexCount; a++)
	{
		qboolean neg = ((DotProduct(plane->normal, vertex[a]) - plane->dist) < 0.0);
		negative[a] = neg;
		negativeCount += neg;
	}
	
	// Discard this polygon if it's completely culled
	if (negativeCount == vertexCount)
	{
		return (0);
	}

	count = 0;
	
	for (b = 0; b < vertexCount; b++)
	{
		// c is the index of the previous vertex
		c = (b != 0) ? b - 1 : vertexCount - 1;
		
		if (negative[b])
		{
			if (!negative[c])
			{
				// Current vertex is on negative side of plane,
				// but previous vertex is on positive side.
				VectorCopy(vertex[c],v1);
				VectorCopy(vertex[b],v2);

				t = (DotProduct(plane->normal, v1) - plane->dist) / (plane->normal[0] * (v1[0] - v2[0])	+ plane->normal[1] * (v1[1] - v2[1]) + plane->normal[2] * (v1[2] - v2[2]));
				
				VectorScale(v1,(1.0 - t),newVertex[count]);
				VectorMA(newVertex[count],t,v2,newVertex[count]);
				
				count++;
			}
		}
		else
		{
			if (negative[c])
			{
				// Current vertex is on positive side of plane,
				// but previous vertex is on negative side.
				VectorCopy(vertex[b],v1);
				VectorCopy(vertex[c],v2);

				t = (DotProduct(plane->normal, v1) - plane->dist) / (plane->normal[0] * (v1[0] - v2[0])+ plane->normal[1] * (v1[1] - v2[1])+ plane->normal[2] * (v1[2] - v2[2]));
				
				VectorScale(v1,(1.0 - t),newVertex[count]);
				VectorMA(newVertex[count],t,v2,newVertex[count]);
				
				count++;
			}
			
			// Include current vertex
			VectorCopy(vertex[b],newVertex[count]);
			count++;
		}
	}
	
	// Return number of vertices in clipped polygon
	return count;
}

extern	cvar_t	sv_gravity;

void R_DrawDecals (void)
{
	decal_t		*p, *kill;
	int			i;
	vec3_t		decaldist;
	float		dcolor, scale;

	if (!qmb_initialized)
		return;

	if ((!decals_enabled) || (!particle_mode))
		return;

	glDepthMask(FALSE);
	glEnable(GL_DEPTH_TEST);
	glEnable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER,0.000f);
	glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glShadeModel (GL_SMOOTH);

	if (gl_fogenable.value)//R00k; v2.017)
		glDisable(GL_FOG);
	
	GL_PolygonOffset (-1);

	for ( ;; ) 
	{
		kill = active_decals;

		if (kill && kill->die < cl.time)
		{
			active_decals = kill->next;
			kill->next = free_decals;
			free_decals = kill;
			continue;
		}
		break;
	}

	for (p = active_decals ; p ; p = p->next)
	{
		for ( ;; )
		{
			kill = p->next;
			//XYZ
			if (kill && (kill->die < cl.time))
			{
				p->next = kill->next;
				kill->next = free_decals;
				free_decals = kill;
				continue;
			}
			break;
		}

		//R00k added distance checking, default 1024			
		VectorSubtract(r_refdef.vieworg, p->origin, decaldist);		

		if (VectorLength(decaldist) > gl_decal_viewdistance.value)
			continue;
		
		GL_Bind(p->texture);
	
		dcolor = (1 - (VectorLength(decaldist) / gl_decal_viewdistance.value));		

		if (p->alpha)
		{
			if (gl_fogenable.value)
				dcolor *= (p->alpha * (gl_fogdensity.value / 0.1f));
			else
				dcolor *= p->alpha;
		}

		if (r_wateralpha.value < 1.0f)
		{
			if ((TruePointContents(p->origin)) != (TruePointContents(r_refdef.vieworg)))	//R00k: different waterlevels of the 2 points..
				dcolor *= (1 - r_wateralpha.value);
		}

		scale = (p->die - cl.time) / dcolor;

		glPushMatrix ();

		if ( (p->die - cl.time) < dcolor)
		{
			dcolor *= scale;
			glColor4f(dcolor, dcolor, dcolor, scale);
		} 
		else 
		{	
			glColor4f(dcolor, dcolor, dcolor, dcolor);
		}

		for (i=0; i<p->triangleCount; i++) 
		{
			int i1, i2, i3;

			i1 = p->triangleArray[i][0];
			i2 = p->triangleArray[i][1];
			i3 = p->triangleArray[i][2];

			glBegin(GL_TRIANGLES);

			glTexCoord2fv(&p->texcoordArray[i1][0]);
			glVertex3fv(&p->vertexArray[i1][0]);

			glTexCoord2fv(&p->texcoordArray[i2][0]);
			glVertex3fv(&p->vertexArray[i2][0]);

			glTexCoord2fv(&p->texcoordArray[i3][0]);
			glVertex3fv(&p->vertexArray[i3][0]);
			glEnd();
		}			
		glPopMatrix ();
	}

	GL_PolygonOffset (0);
	
	glEnable(GL_DEPTH_TEST);
	glDepthMask(TRUE);
	glDisable (GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER,0.666f);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel (GL_FLAT);

	if (gl_fogenable.value)
	{
		glEnable(GL_FOG);
	}
}

void CheckDecals (void)
{
	if	(!gl_decal_bullets.value && !gl_decal_explosions.value && !gl_decal_sparks.value && !gl_decal_blood.value)
		decals_enabled = 0;
	else	
		if	(gl_decal_bullets.value && gl_decal_explosions.value && gl_decal_sparks.value && gl_decal_blood.value)
		decals_enabled = 1;
		else
			decals_enabled = 2;
}

void R_SetDecals (int val)
{	
	decals_enabled = val;
	Cvar_SetValue ("gl_decal_bullets", val);
	Cvar_SetValue ("gl_decal_explosions", val);
	Cvar_SetValue ("gl_decal_sparks", val);
	Cvar_SetValue ("gl_decal_blood", val);
}

void R_ToggleDecals_f (void)
{
	if (cmd_source != src_command)
		return;
	CheckDecals ();
	R_SetDecals (!(decals_enabled));	
	Con_Printf ("All Decals are %s.\n", !decals_enabled ? "disabled" : "enabled");
}

//Decals -- END
