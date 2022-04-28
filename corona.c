#include	"quakedef.h"
#define		MAX_CORONASTEX 2

int		corona_textures[MAX_CORONASTEX];
int		coronatex_num;
int		coronas;
cvar_t gl_coronas={"gl_coronas", "0", true};

extern void TraceLine (vec3_t start, vec3_t end, vec3_t impact);

// This makes coronas to fade in/out
void R_CoronaFade (corona_t *f, qboolean show)
{
   if(show != f->show)
   {
      f->show = show;
      f->begintime = cl.time;
      return;
   }
   if(f->begintime)
   {
      if(!show)
      {
         f->alpha2 = f->alpha - (cl.time - f->begintime)/(f->fadetime*f->alpha);
         if(f->alpha2 < 0)
         {
            f->alpha2 = 0;
            f->begintime = 0;
         }
      }
      else
      {
         f->alpha2 = (cl.time - f->begintime)/(f->fadetime*f->alpha);
         if(f->alpha2 > f->alpha)
         {
            f->alpha2 = f->alpha;
            f->begintime = 0;
         }
      }
   }
}

void R_DrawCorona (entity_t *e)
{
	corona_t   *f;
	vec3_t      up, right, stop, org, point;
	float      radius, alpha, dist;
	vec3_t      v[4];
	int         i;

	if(!coronatex_num)
		return;

	if (gl_coronas.value < 1)
		return;

	f = (corona_t*)e->corona;

	VectorAdd(e->origin, f->offset, org);

	dist = VecLength2(r_refdef.vieworg, org);

	TraceLine(r_refdef.vieworg, org, stop);

	if (VecLength2(r_refdef.vieworg, stop) < dist) 
		R_CoronaFade(f, false);
	else
		R_CoronaFade(f, true);

	if(f->lightstyle)
		alpha = f->alpha2 * (float)((float)d_lightstylevalue[f->lightstyle])/256/8.8;
	else
		alpha = f->alpha2;

	if(f->distfactor && alpha)
	{
		alpha *= 300*f->distfactor/dist;
		if(alpha < 0)
			alpha = 0;
	}

	if(!alpha)
	{
		return;
	}


	if (gl_fogenable.value)
	{
		glDisable(GL_FOG);
	}	

	VectorCopy(vup, up);
	VectorCopy(vright, right);

	glPushMatrix();
	glColor4f (f->color[0], f->color[1], f->color[2], alpha);

	radius = 0.1 * VecLength2(r_refdef.vieworg, org)/f->scalefactor;

	if(radius < f->minradius)
	radius = f->minradius;
	else
	if(radius > f->maxradius)
	 radius = f->maxradius;

	GL_Bind(corona_textures[f->coronatexnum]);

	//glDisable(GL_DEPTH_TEST); //R00k: removed this so coronas are not visible through doors/plats
	glDepthMask (GL_FALSE);	// don't bother writing Z
	glBlendFunc(GL_SRC_ALPHA, GL_ONE); 
	glEnable (GL_BLEND);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBegin (GL_QUADS);
	glTexCoord2f (0, 1);
	VectorMA (org, -radius*f->texscale[1], up, point);
	VectorMA (point, -radius*f->texscale[0], right, v[0]);
	glVertex3fv (v[0]);
	glTexCoord2f (0, 0);
	VectorMA (org, radius*f->texscale[1], up, point);
	VectorMA (point, -radius*f->texscale[0], right, v[1]);
	glVertex3fv (v[1]);
	glTexCoord2f (1, 0);
	VectorMA (org, radius*f->texscale[1], up, point);
	VectorMA (point, radius*f->texscale[0], right, v[2]);
	glVertex3fv (v[2]);
	glTexCoord2f (1, 1);
	VectorMA (org, -radius*f->texscale[1], up, point);
	VectorMA (point, radius*f->texscale[0], right, v[3]);
	glVertex3fv (v[3]);
	glEnd ();

	i = gl_coronas.value;

	while(--i)
	{
		glBegin (GL_QUADS);
		glTexCoord2f (0, 1);
		glVertex3fv (v[0]);
		glTexCoord2f (0, 0);
		glVertex3fv (v[1]);
		glTexCoord2f (1, 0);
		glVertex3fv (v[2]);
		glTexCoord2f (1, 1);
		glVertex3fv (v[3]);
		glEnd ();
	}

	glPopMatrix();

	glEnable (GL_TEXTURE_2D);	
	glDisable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glDepthMask(GL_TRUE);
//	glEnable(GL_DEPTH_TEST);
	glColor4f(1,1,1,1);

	if (gl_fogenable.value)
		glEnable(GL_FOG);
} 

// Draws all coronas
void R_UpdateCoronas ()
{
	int      i;
	entity_t *ent;

	if(!gl_coronas.value)
	  return;

	if(!coronas)
	  return;

	for (i=1,ent=cl_entities+1 ; i<MAX_EDICTS ; i++,ent++)
	{
		if(ent->corona)
		{
			R_DrawCorona(ent);
		}
	}

	for (i=0,ent=cl_static_entities ; i<MAX_STATIC_ENTITIES; i++,ent++)
	{
		if(ent->corona)
		{
			R_DrawCorona(ent);
		}
	}	
}

void R_RemoveCorona (entity_t *e)
{
   if(!e->corona)
      return;
   e->corona = NULL;
   coronas--;
   if(coronas < 0)
      coronas = 0;
}

corona_t *R_AddDefaultCorona (entity_t *e)
{
   corona_t *c;

   if(e->corona)
      return NULL;

   c = Q_malloc (sizeof(corona_t));
   e->corona = (void*)c;
   if(!e->corona)
      return NULL;
   c->alpha = 0.5f;
   VectorSet(c->color, 1, 1, 0.75);//Yellow-ish
   VectorSet(c->offset, 0, 0, 0);
   c->texscale[0] = c->texscale[1] = 1;
   c->distfactor = 1.5;
   c->scalefactor = 1;
   c->fadetime = 0.5;
   c->lightstyle = c->alpha2 = 0;
   c->minradius = 4;
   c->maxradius = 72;
   c->maxdist = 65536;
   c->coronatexnum = 0;
   c->begintime = -1;
   c->show = false;
   coronas++;
   return c;
}

void R_InitCoronas ()
{
	int i;
	for(i=0; i<MAX_CORONASTEX ;i++)

	if(!(corona_textures[i] = GL_LoadTextureImage(va("textures/coronas/corona%i", i),va("qmb:corona%i",i), 128, 128, TEX_ALPHA | TEX_COMPLAIN)))
		return;

	coronas = 0;
	coronatex_num = i;
	Cvar_RegisterVariable(&gl_coronas);
	if(i)
	  Con_Printf("Loaded %d corona textures\n", i);
} 
