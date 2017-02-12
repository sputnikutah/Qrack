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
// gl_rmisc.c

#include "quakedef.h"

qboolean	r_loadq3player = false;
void CL_CopyPlayerInfo (entity_t *ent, entity_t *player);

char last_mapname[MAX_QPATH] = {0};
mpic_t *last_lvlshot = NULL;
#ifdef USESHADERS
GLuint current_vp = 0;
GLuint current_fp = 0;

void GL_SetVertexProgram (GLuint vp)
{
	if (current_vp != vp)
	{
		qglBindProgramARB (GL_VERTEX_PROGRAM_ARB, vp);
		current_vp = vp;
	}
}

void GL_SetFragmentProgram (GLuint fp)
{
	if (current_fp != fp)
	{
		qglBindProgramARB (GL_FRAGMENT_PROGRAM_ARB, fp);
		current_fp = fp;
	}
}


void GL_SetPrograms (GLuint vp, GLuint fp)
{
	GL_SetVertexProgram (vp);
	GL_SetFragmentProgram (fp);
}


GLuint gl_arb_programs[1024];
int gl_num_arb_programs = 0;


GLuint GL_CreateProgram (GLenum mode, const char *source, qboolean fog)
{
	GLuint progid;
	GLint errPos;
	const GLubyte *errString;
	GLenum errGLErr;
	char *realsource = (char *) scratchbuf;

	if (mode == GL_FRAGMENT_PROGRAM_ARB && fog)
	{
		strcpy (realsource, "!!ARBfp1.0\nOPTION ARB_fog_exp2;\n");
		strcat (realsource, source + 11);
	}
	else strcpy (realsource, source);

	qglGenProgramsARB (1, &progid);
	qglBindProgramARB (mode, progid);

	errGLErr = glGetError ();
	qglProgramStringARB (mode, GL_PROGRAM_FORMAT_ASCII_ARB, strlen (realsource), realsource);
	errGLErr = glGetError ();

	// Find the error position
	glGetIntegerv (GL_PROGRAM_ERROR_POSITION_ARB, &errPos);
	errString = glGetString (GL_PROGRAM_ERROR_STRING_ARB);

	if (errGLErr != GL_NO_ERROR) Con_Printf ("Generic OpenGL Error\n");
	if (errPos != -1) Con_Printf ("Program error at position: %d\n", errPos);
	if (errString && errString[0]) Con_Printf ("Program error: %s\n", errString);

	if ((errPos != -1) || (errString && errString[0]) || (errGLErr != GL_NO_ERROR))
	{
		Con_Printf ("Program:\n%s\n", realsource);
		qglDeleteProgramsARB (1, &progid);
		qglBindProgramARB (mode, 0);
		return 0;
	}
	else
	{
		gl_arb_programs[gl_num_arb_programs] = progid;
		gl_num_arb_programs++;

		qglBindProgramARB (mode, 0);
		return progid;
	}
}

void R_DeleteShaders (void)
{
	int i;

	for (i = 0; i < gl_num_arb_programs; i++)
	{
		qglDeleteProgramsARB (1, &gl_arb_programs[i]);
		gl_arb_programs[i] = 0;
	}

	gl_num_arb_programs = 0;
}

//void GLARB_SetupAliasModel (entity_t *e);
//void GLARB_DrawAliasBatches  (entity_t **ents, int numents);
#endif

//Auto noise generator by LordHavoc.
void fractalnoise(byte *noise, int size, int startgrid)
{
   int x, y, g, g2, amplitude, min, max, size1 = size - 1, sizepower, gridpower;
   int *noisebuf;
#define n(x,y) noisebuf[((y)&size1)*size+((x)&size1)]

   for (sizepower = 0;(1 << sizepower) < size;sizepower++);
   if (size != (1 << sizepower))
      Sys_Error("fractalnoise: size must be power of 2\n");

   for (gridpower = 0;(1 << gridpower) < startgrid;gridpower++);
   if (startgrid != (1 << gridpower))
      Sys_Error("fractalnoise: grid must be power of 2\n");

   startgrid = bound(0, startgrid, size);

   amplitude = 0xFFFF; // this gets halved before use
   noisebuf = malloc(size*size*sizeof(int));
   memset(noisebuf, 0, size*size*sizeof(int));

   for (g2 = startgrid;g2;g2 >>= 1)
   {
      // brownian motion (at every smaller level there is random behavior)
      amplitude >>= 1;
      for (y = 0;y < size;y += g2)
         for (x = 0;x < size;x += g2)
            n(x,y) += (rand()&amplitude);

      g = g2 >> 1;
      if (g)
      {
         // subdivide, diamond-square algorithm (really this has little to do with squares)
         // diamond
         for (y = 0;y < size;y += g2)
            for (x = 0;x < size;x += g2)
               n(x+g,y+g) = (n(x,y) + n(x+g2,y) + n(x,y+g2) + n(x+g2,y+g2)) >> 2;
         // square
         for (y = 0;y < size;y += g2)
            for (x = 0;x < size;x += g2)
            {
               n(x+g,y) = (n(x,y) + n(x+g2,y) + n(x+g,y-g) + n(x+g,y+g)) >> 2;
               n(x,y+g) = (n(x,y) + n(x,y+g2) + n(x-g,y+g) + n(x+g,y+g)) >> 2;
            }
      }
   }
   // find range of noise values
   min = max = 0;
   for (y = 0;y < size;y++)
      for (x = 0;x < size;x++)
      {
         if (n(x,y) < min) min = n(x,y);
         if (n(x,y) > max) max = n(x,y);
      }
   max -= min;
   max++;
   // normalize noise and copy to output
   for (y = 0;y < size;y++)
      for (x = 0;x < size;x++)
         *noise++ = (byte) (((n(x,y) - min) * 256) / max);
   free(noisebuf);
#undef n
}

void Mod_BuildDetailTexture (void)
{
	int x, y, light;
	float vc[3], vx[3], vy[3], vn[3], lightdir[3];
	#define DETAILRESOLUTION 256
	byte data[DETAILRESOLUTION][DETAILRESOLUTION][4], noise[DETAILRESOLUTION][DETAILRESOLUTION];

	if (detailtexture)
		memset (&detailtexture, 0, sizeof(detailtexture));

	lightdir[0] = 0.5;
	lightdir[1] = 1;
	lightdir[2] = -0.25;
	VectorNormalize(lightdir);

	fractalnoise(&noise[0][0], DETAILRESOLUTION, DETAILRESOLUTION >> 4);
	for (y = 0;y < DETAILRESOLUTION;y++)
	{
	  for (x = 0;x < DETAILRESOLUTION;x++)
	  {
		 vc[0] = x;
		 vc[1] = y;
		 vc[2] = noise[y][x] * (1.0f / 32.0f);
		 vx[0] = x + 1;
		 vx[1] = y;
		 vx[2] = noise[y][(x + 1) % DETAILRESOLUTION] * (1.0f / 32.0f);
		 vy[0] = x;
		 vy[1] = y + 1;
		 vy[2] = noise[(y + 1) % DETAILRESOLUTION][x] * (1.0f / 32.0f);
		 VectorSubtract(vx, vc, vx);
		 VectorSubtract(vy, vc, vy);
		 CrossProduct(vx, vy, vn);
		 VectorNormalize(vn);
		 light = 128 - DotProduct(vn, lightdir) * 128;
		 light = bound(0, light, 255);
		 data[y][x][0] = data[y][x][1] = data[y][x][2] = light;
		 data[y][x][3] = 255;
	  }
	}

   detailtexture = texture_extension_number++;

   GL_Bind (detailtexture);
   GL_Upload32 ((unsigned int *)data, DETAILRESOLUTION, DETAILRESOLUTION, TEX_MIPMAP);
   memset(data, 0, sizeof(data));
}

GLuint celtexture;
GLuint vertextexture = 0;

void R_InitOtherTextures (void)
{
	extern int	logoTex;//R00k
	extern int	solidskytexture2, alphaskytexture2;//R00k
	extern int	chrometexture;
	extern int	glasstexture;
	extern void R_InitDecalTextures (void);

	int		i, flags = TEX_MIPMAP | TEX_ALPHA;

	float	cellData[32] = {0.2f,0.2f,0.2f,0.2f,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0};
	float	cellFull[32][3];
	float	vertexFull[32][3];

	for (i=0;i<32;i++)
		cellFull[i][0] = cellFull[i][1] = cellFull[i][2] = cellData[i];

	for (i=0;i<32;i++)
		vertexFull[i][0] = vertexFull[i][1] = vertexFull[i][2] = (((i + 4) / 16.0f));

	if (!celtexture) 
		celtexture = texture_extension_number++;	

	glBindTexture(GL_TEXTURE_1D, celtexture);
	glTexImage1D (GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGB , GL_FLOAT, cellFull);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);	

	if (!vertextexture) 
		vertextexture = texture_extension_number++;		

	glBindTexture (GL_TEXTURE_1D, vertextexture);	
	glTexImage1D (GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGB , GL_FLOAT, vertexFull);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);

	underwatertexture	= GL_LoadTextureImage ("textures/water_caustic", NULL, 0, 0,  flags | TEX_COMPLAIN);
	chrometexture		= GL_LoadTextureImage ("textures/shine_chrome", NULL, 0, 0, flags | TEX_COMPLAIN);
	glasstexture		= GL_LoadTextureImage ("textures/shine_glass", NULL, 0, 0, flags | TEX_COMPLAIN);
	solidskytexture2	= GL_LoadTextureImage ("textures/solidskytexture", NULL, 128, 128, flags | TEX_COMPLAIN);
	alphaskytexture2	= GL_LoadTextureImage ("textures/alphaskytexture", NULL, 128, 128, flags | TEX_COMPLAIN);
	logoTex				= GL_LoadTextureImage ("textures/qracklogo", NULL, 0, 0, TEX_ALPHA);

	R_InitDecalTextures();
	Mod_BuildDetailTexture ();
} 

/*
==================
R_InitTextures
==================
*/
void R_InitTextures (void)
{
	int	x, y, m;
	byte	*dest;

// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;

	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y<(16>>m) ; y++)
			for (x=0 ; x<(16>>m) ; x++)
				*dest++ = ((y < (8 >> m)) ^ (x < (8 >> m))) ? 0 : 0x0e;
	}
}

int	fb_skins[MAX_SCOREBOARD];	// by joe

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
void R_TranslatePlayerSkin (int playernum)
{
	int			top, bottom, i, size, inwidth, inheight;
	byte		translate[256], *original, *translated;
	model_t		*model;
	aliashdr_t	*paliashdr;
	extern qboolean	Img_HasFullbrights (byte *pixels, int size);

	GL_DisableMultitexture ();

	top = cl.scores[playernum].colors & 0xf0;
	bottom = (cl.scores[playernum].colors & 15) << 4;

	for (i=0 ; i<256 ; i++)
		translate[i] = i;

	for (i=0 ; i<16 ; i++)
	{
		// the artists made some backward ranges. sigh.
		translate[TOP_RANGE+i] = (top < 128) ? top + i : top + 15 - i;
		translate[BOTTOM_RANGE+i] = (bottom < 128) ? bottom + i : bottom + 15 - i;
	}

	// locate the original skin pixels
	currententity = &cl_entities[1+playernum];

	if (!(model = currententity->model))
		return;		// player doesn't have a model yet

	if (model->type != mod_alias)
		return;		// only translate skins on alias models

	paliashdr = (aliashdr_t *)Mod_Extradata (model);
	size = paliashdr->skinwidth * paliashdr->skinheight;

	if (currententity->skinnum < 0 || currententity->skinnum >= paliashdr->numskins)
	{
		Con_DPrintf (1,"(%d): Invalid player skin #%d\n", playernum, currententity->skinnum);		
		original = (byte *)paliashdr + paliashdr->texels[0];
	}
	else
	{
		original = (byte *)paliashdr + paliashdr->texels[currententity->skinnum];
	}

	if (size & 3)
		Sys_Error ("R_TranslatePlayerSkin: size & 3");

	inwidth = paliashdr->skinwidth;
	inheight = paliashdr->skinheight;

	GL_Bind (playertextures + playernum);

	translated = Q_malloc (inwidth * inheight);

	for (i = 0 ; i < size ; i++)
		translated[i] = translate[original[i]];

	GL_Upload8 (translated, inwidth, inheight, TEX_MIPMAP);

	fb_skins[playernum] = 0;

	if (Img_HasFullbrights(original, inwidth * inheight))
	{
		fb_skins[playernum] = playertextures + playernum + MAX_SCOREBOARD;

		GL_Bind (fb_skins[playernum]);
		GL_Upload8 (translated, inwidth, inheight, TEX_FULLBRIGHT);
	}
}

// joe: added from FuhQuake
void R_PreMapLoad (char *mapname)
{
	Cvar_Set ("mapname",mapname);
	strcpy (cls.mapstring, mapname);
	strcpy (sv_mapname.string, mapname);
}

/*
=============
Fog_ParseServerMessage

handle an SVC_FOG message from server
=============
*/
void Fog_ParseServerMessage (void)
{
	float density, red, green, blue, time;

	density = MSG_ReadByte() / 100.0;
	red = MSG_ReadByte() / 255.0;
	green = MSG_ReadByte() / 255.0;
	blue = MSG_ReadByte() / 255.0;
	time = max(0.0, MSG_ReadShort() / 100.0);

	Cvar_SetValue("gl_fogdensity",density);
	Cvar_SetValue("gl_fogred",red);
	Cvar_SetValue("gl_foggreen", green);
	Cvar_SetValue("gl_fogblue",blue);
	if (gl_fogenable.value < 2)	
		Cvar_SetValue("gl_fogenable",1);
}

/*
=============
Fog_ParseWorldspawn

called at map load
=============
*/
void Fog_ParseWorldspawn (void)
{
	char key[128], value[4096];
	char *data;
	float fog_density;
	float fog_red;
	float fog_green;
	float fog_blue;

	if (gl_fogenable.value < 2)	
		Cvar_SetValue("gl_fogenable",0);

	data = COM_Parse(cl.worldmodel->entities);
	
	if (!data)
		return; // error
	
	if (com_token[0] != '{')
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

		if (!strcmp("fog", key))
		{
			sscanf(value, "%f %f %f %f", &fog_density, &fog_red, &fog_green, &fog_blue);
			Cvar_SetValue("gl_fogdensity",fog_density);
			Cvar_SetValue("gl_fogred",fog_red);
			Cvar_SetValue("gl_foggreen", fog_green);
			Cvar_SetValue("gl_fogblue",fog_blue);
			if (gl_fogenable.value < 2)	
				Cvar_SetValue("gl_fogenable",1);
		}
	}
}

extern qboolean config_lock;

void LoadMapConfig (void)
{
	char *mapname = "";

	if (COM_FindFile("config.cfg"))
	{
		Cbuf_AddText ("exec config.cfg\n");
		Cbuf_Execute ();
	}

	if (COM_FindFile("autoexec.cfg"))
	{
		Cbuf_AddText ("exec autoexec.cfg\n");
		Cbuf_Execute ();
	}

	config_lock = false;

	// if (cl_loadmapconfig.value) {
	mapname = sv_mapname.string;

	Con_DPrintf (1,"loading cfg for %s\n", mapname);
	
	if (COM_FindFile(va("%s.cfg",mapname)))
	{
		config_lock = true;	
		Cbuf_AddText (va("exec %s.cfg\n", mapname));	
		Cbuf_Execute ();
	}
}

void ReadSkyBoxConfig (void)
{
	FILE	*f;
	char	filename[MAX_OSPATH], map[65], sky[65];
	char	out[65];
	int		r, i, c;

	Q_snprintfz (filename, sizeof(filename), "%s/skybox.cfg", com_gamedir);

	if (!(f = fopen (filename, "rt")))
	{
		return;
	}

	while (!feof(f))
	{
		for (i=0 ; i<sizeof(map)-1 ; i++)
		{
			r = fgetc (f);
			if (r == EOF || !r)
			{
				fclose (f);
				return;
			}
			
			map[i] = r;

			if ((r == '\n')||(r == ' '))
			{
				i++;
				break;
			}
		}
		
		map[i] = 0;

		//convert to lowercase
		for (i=0 ; i<sizeof(map)-1; i++)
		{
			c = map[i];
			if (!c)
				break;

			if (c >= 'A' && c <= 'Z')
				c += ('a' - 'A');
			out[i] = c;
		}
		
		COM_Parse (out);

		if (!strcmp(com_token, sv_mapname.string))
		{			
			for (i=0 ; i<sizeof(sky)-1 ; i++)
			{
				r = fgetc (f);
				if (r == EOF || !r)
				{
					fclose (f);
					return;
				}

				sky[i] = r;

				if ((r == '\n')||(r == ' '))
				{
					i++;
					break;
				}
			}

			sky[i] = 0;
			COM_Parse (sky);

			Cbuf_AddText (va("r_skybox %s\n",com_token));	
			Cbuf_Execute ();//clear buffer just incase
			break;
		}
	}
	fclose (f);
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	extern void R_ClearDecals(void);
	extern void Sky_NewMap (void);
	extern cvar_t cl_loadmapcfg;
	extern char	scr_centerstring[1024];
	extern char	con_lastcenterstring[1024]; //johnfitz
	extern void R_CreateRTTTextures (int width, int height);
	extern void R_InitTurbSin (void);

	int	i, waterline;

	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	
	// Level shots init
#ifdef GLQUAKE
	//last_lvlshot = NULL;
	last_mapname[0] = 'dm3';
	cl.lvlshot_time = realtime + 5;//R00k
#endif

	r_worldentity.model = cl.worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	for (i = 0; i < cl.worldmodel->numleafs + 1; i++)
	{
		cl.worldmodel->leafs[i].efrags = NULL;
	}

	r_viewleaf = NULL;
	R_ClearParticles();
	R_ClearDecals();
	R_InitTurbSin();
	GL_BuildLightmaps();

	// reload all of our vertex buffers
#ifdef USESHADERS
	GL_BeginBuffers ();
	R_CreateRTTTextures (glwidth, glheight);
//	RBrush_LoadVertexBuffers (); todo
//	GLMesh_LoadVertexBuffers ();
//	GLIQM_LoadVertexBuffers ();
#endif
	// identify sky texture
	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;

		for (waterline=0 ; waterline<2 ; waterline++)
		{
 			cl.worldmodel->textures[i]->texturechain[waterline] = NULL;
			cl.worldmodel->textures[i]->texturechain_tail[waterline] = &cl.worldmodel->textures[i]->texturechain[waterline];
		}
	}

	Sky_NewMap(); //johnfitz -- skybox in worldspawn

	Fog_ParseWorldspawn();

	if (cl_loadmapcfg.value)
	{		
		LoadMapConfig();
	}

	ReadSkyBoxConfig();
	
	// HACK HACK HACK - create two extra entities if drawing the player's multimodel
	if (r_loadq3player)
	{
		memset (&q3player_body, 0, sizeof(tagentity_t));
		CL_CopyPlayerInfo (&q3player_body.ent, &cl_entities[cl.viewentity]);
		memset (&q3player_head, 0, sizeof(tagentity_t));
		CL_CopyPlayerInfo (&q3player_head.ent, &cl_entities[cl.viewentity]);
	}
	
	//R00k: clear out old centerstring when connecting to a new server/map.
	memset (scr_centerstring, 0, sizeof(scr_centerstring));//R00k
	memset (con_lastcenterstring, 0, sizeof(con_lastcenterstring));	
}
/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f (void)
{
	int	i;
	float	start, stop, time;

	if (cls.state != ca_connected)
		return;

	if (strcmp(gl_vendor, "Intel"))
		glDrawBuffer (GL_FRONT);

	//glFinish ();

	start = Sys_DoubleTime ();
	for (i=0 ; i<128 ; i++)
	{
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		R_RenderView ();
	}

	//glFinish ();
	stop = Sys_DoubleTime ();
	time = stop - start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);

	glDrawBuffer (GL_BACK);
	GL_EndRendering ();
}
/*
void D_FlushCaches (void)
{
}
*/
