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
// gl_model.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "quakedef.h"

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

void Mod_LoadSpriteModel (model_t *mod, void *buffer); 
void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qboolean crash);
void Mod_LoadQ3Model(model_t *mod, void *buffer);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	2048 //johnfitz -- was 512

model_t	mod_known[MAX_MOD_KNOWN];

int	mod_numknown;

cvar_t	gl_subdivide_size = {"gl_subdivide_size", "128", true};
float	map_fallbackalpha;
/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	Cvar_RegisterVariable (&gl_subdivide_size);
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

/*
===============
Mod_Extradata

Caches the data if needed
===============
*/
void *Mod_Extradata (model_t *mod)
{
	void	*r;

	r = Cache_Check (&mod->cache);

	if (r)
	{
		return r;
	}

	Mod_LoadModel (mod, true);

	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed");

	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	
	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;

	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct (p,node->plane->normal)) < node->plane->dist];
		if (node->contents < 0)
			return (mleaf_t *)node;
	}
	
	return NULL;	// never reached
}
/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, model_t *model)
{
	static	byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->numleafs + 7) >> 3;
	out = decompressed;

	if (!in || r_novis.value == 2)//BPJ
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
	
	return decompressed;
}

byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;
	

	return Mod_DecompressVis (leaf->compressed_vis, model);
}


/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
{
	int		i;
	model_t		*mod;
	static qboolean NoFree, Done;
	extern	void GL_FreeTextures (void);

	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (mod->type != mod_alias && mod->type != mod_md3)
			mod->needload = true;
	
		if (mod->name[0] != '*')//RMQ
		{
			// '*' models share their data with the world and so should not free it
			// everything else can safely free
			SAFE_FREE (mod->visdata);
			SAFE_FREE (mod->lightdata);
			SAFE_FREE (mod->entities);
			SAFE_FREE (mod->vertexes);
			SAFE_FREE (mod->edges);
			SAFE_FREE (mod->texinfo);
			SAFE_FREE (mod->surfaces);
			SAFE_FREE (mod->clipnodes);
			SAFE_FREE (mod->marksurfaces);
			SAFE_FREE (mod->surfedges);
			SAFE_FREE (mod->planes);
			SAFE_FREE (mod->submodels);
			SAFE_FREE (mod->hulls[0].clipnodes);
		}
		else
		{
			// in case this model slot gets reused and tries to free in the future...
			mod->visdata = NULL;
			mod->lightdata = NULL;
			mod->entities = NULL;
			mod->vertexes = NULL;
			mod->edges = NULL;
			mod->texinfo = NULL;
			mod->surfaces = NULL;
			mod->clipnodes = NULL;
			mod->marksurfaces = NULL;
			mod->surfedges = NULL;
			mod->planes = NULL;
			mod->submodels = NULL;
			mod->hulls[0].clipnodes = NULL;
		}

		if (mod->type == mod_sprite) 
		{
			mod->cache.data = NULL;
		}
	}

	if (!Done)
	{
		// Some 3dfx miniGLs don't support glDeleteTextures (i.e. do nothing)
		NoFree = isDedicated || COM_CheckParm ("-nofreetex");
		Done = true;
	}

	if (!NoFree)
		GL_FreeTextures ();
}

/*
==================
Mod_FindName
==================
*/
model_t *Mod_FindName (char *name)
{
	int	i;
	model_t	*mod;


	if (!name[0])
		Sys_Error ("Mod_FindName: NULL name");

// search the currently loaded models
	for (i = 0, mod = mod_known ; i < mod_numknown ; i++, mod++)
		if (!strcmp(mod->name, name))
			break;

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error ("Mod_FindName: mod_numknown == MAX_MOD_KNOWN (%d)", MAX_MOD_KNOWN);

		Q_strncpyz (mod->name, name, sizeof(mod->name));
		mod->needload = true;
		mod_numknown++;
	}

	return mod;
}

/*
==================
Mod_TouchModel
==================
*/
void Mod_TouchModel (char *name)
{
	model_t	*mod;
	mod = Mod_FindName (name);

	if (!mod->needload && (mod->type == mod_alias || mod->type == mod_md3))
	{
		Cache_Check (&mod->cache);
	}

}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/

model_t *Mod_LoadModel (model_t *mod, qboolean crash)
{
	void		*d;
	unsigned	*buf;
	byte		stackbuf[1024];		// avoid dirtying the cache heap
	char		strip[128];
	char		md3name[128];

	if (!mod->needload)
	{
		if (mod->type == mod_alias || mod->type == mod_md3)
		{
			d = Cache_Check (&mod->cache);
			if (d)
				return mod;
		}
		else
		{
			return mod;		// not cached at all
		}
	}

// because the world is so huge, load it one piece at a time
	if (!crash)
	{

	}
// load the file
	if (gl_loadq3models.value)
	{
		COM_StripExtension(mod->name, &strip[0]);
		sprintf (&md3name[0], "%s.md3", &strip[0]);

		buf = (unsigned *)COM_LoadStackFile (md3name, stackbuf, sizeof(stackbuf));
		if (!buf)
		{
			buf = (unsigned *)COM_LoadStackFile (mod->name, stackbuf, sizeof(stackbuf));
			if (!buf)
			{
				// If last added mod_known, get rid of it
				if (mod == &mod_known[mod_numknown - 1])
					--mod_numknown;
				if (crash)
					Host_Error ("Mod_LoadModel: %s not found", mod->name);
				return NULL;
			}
		}
	}
	else
	{
		buf = (unsigned *)COM_LoadStackFile (mod->name, stackbuf, sizeof(stackbuf));
		if (!buf)
		{
			//BPJ: If last added mod_known, get rid of it
			if (mod == &mod_known[mod_numknown - 1])
				--mod_numknown;

			if (crash)
				Host_Error ("Mod_LoadModel: %s not found", mod->name);
			return NULL;
		}
	}

// allocate a new model
	COM_FileBase (mod->name, loadname);

	loadmodel = mod;

// fill it in

// call the apropriate loader
	mod->needload = false;

	switch (LittleLong(*(unsigned *)buf))
	{
		case IDPOLYHEADER:
			Mod_LoadAliasModel (mod, buf);
			break;

		case IDMD3HEADER:
			Mod_LoadQ3Model (mod, buf);
			break;

		case IDSPRITEHEADER:
			Mod_LoadSpriteModel (mod, buf);
			break;

		default:
			Mod_LoadBrushModel (mod, buf);
			break;
	}

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;
	
	mod = Mod_FindName (name);
	
	return Mod_LoadModel (mod, crash);
}

qboolean Img_HasFullbrights (byte *pixels, int size)
{
	int	i;

	for (i=0 ; i<size ; i++)
		if (pixels[i] >= 224) //Fullbright color pallete 224-255
			return true;

	return false;
}


/*
===============================================================================

				BRUSHMODEL LOADING

===============================================================================
*/

byte	*mod_base;

/*
=================
Mod_LoadBrushModelTexture
=================
*/
int Mod_LoadBrushModelTexture (texture_t *tx, int flags)
{
	char	*name, *mapname;

	if (isDedicated)
		return 0;

	if (loadmodel->isworldmodel)
	{
		if (!gl_externaltextures_world.value)
			return 0;
	}
	else
	{
		if (!gl_externaltextures_bmodels.value)
			return 0;
	}

	name = tx->name;
	mapname = sv_mapname.string;
	
	if ((loadmodel->isworldmodel) && (!ISTURBTEX(name)))
		flags |= TEX_WORLD;

	if (loadmodel->isworldmodel)
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage(va("textures/%s/%s", mapname, name), name, 0, 0, flags)))
		{
			if (!ISTURBTEX(name))
			{
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s/%s_luma", mapname, name), va("@fb_%s", name), 0, 0, flags | TEX_LUMA);
			}

		}
	}
	else
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage(va("textures/bmodels/%s", name), name, 0, 0, flags)))
		{
			if (!ISTURBTEX(name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/bmodels/%s_luma", name), va("@fb_%s", name), 0, 0, flags | TEX_LUMA);
		}
	}

	if (!tx->gl_texturenum)
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage(va("textures/%s", name), name, 0, 0, flags)))
		{
			if (!ISTURBTEX(name))
			{
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s_luma", name), va("@fb_%s", name), 0, 0, flags | TEX_LUMA);	
			}
		}
	}

	if (tx->fb_texturenum)
		tx->isLumaTexture = true;

	return tx->gl_texturenum;
}

/*
=================
Mod_LoadTextures
=================
*/
void Mod_LoadTextures (lump_t *l)
{
	int				i, j, num, max, altmax, texture_mode;
	miptex_t		*mt;
	texture_t		*tx, *tx2, *txblock, *anims[10], *altanims[10];
	dmiptexlump_t	*m;
	byte			*data;
	extern int GL_LoadTexturePixels (byte *data, char *identifier, int width, int height, int mode);

	if ((isDedicated)||(cls.state == ca_dedicated))
		return;

	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *)(mod_base + l->fileofs);
	m->nummiptex = LittleLong (m->nummiptex);
	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = Hunk_AllocName (m->nummiptex * sizeof(*loadmodel->textures), loadname);

	txblock = Hunk_AllocName (m->nummiptex * sizeof(**loadmodel->textures), loadname);

	texture_mode = TEX_MIPMAP;

	for (i = 0 ; i < m->nummiptex ; i++)
	{
		m->dataofs[i] = LittleLong (m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;

		mt = (miptex_t *)((byte *)m + m->dataofs[i]);
		loadmodel->textures[i] = tx = txblock + i;

		texture_mode |= ISALPHATEX(mt->name) ? TEX_ALPHA : 0;//R00k support for alpha textures on bmodels. Must have "{" prefix in the ORIGINAL miptex filename.

		memcpy (tx->name, mt->name, sizeof(tx->name));

		if (!tx->name[0])
		{
			Q_snprintfz (tx->name, sizeof(tx->name), "unnamed%d", i);
			Con_DPrintf (1,"Warning: unnamed texture in %s, renaming to %s\n", loadmodel->name, tx->name);
		}

		tx->width = mt->width = LittleLong (mt->width);
		tx->height = mt->height = LittleLong (mt->height);

		if ((mt->width & 15) || (mt->height & 15))
			Sys_Error ("Mod_LoadTextures: Texture %s is not 16 aligned", mt->name);

		for (j=0 ; j<MIPLEVELS ; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);		
		
		if (!strcmp(mt->name, "shot1sid") && mt->width == 32 && mt->height == 32 && CRC_Block((byte*)(mt+1), mt->width*mt->height) == 65393)// HACK HACK HACK
		{	// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
			// They are invisible in software, but look really ugly in GL. So we just copy
			// 32 pixels from the bottom to make it look nice.
			memcpy (mt+1, (byte *)(mt+1) + 32*31, 32);
		}

		if (!Q_strncasecmp(tx->name,"sky",3)) 
		{
			if (strstr(tx->name,"sky4")) 
			{
				if (CRC_Block((byte *)(tx+1), tx->width * tx->height)==13039)
				{
					sprintf(tx->name,"sky1");
				}
			}
		}

		if (loadmodel->isworldmodel && ISSKYTEX(tx->name))
		{				
			R_InitSky (mt);			
			continue;
		}

		if (Mod_LoadBrushModelTexture(tx, texture_mode))
		{			
			continue;
		}

		if ((loadmodel->isworldmodel) && (!ISTURBTEX(tx->name)))
			texture_mode |= TEX_WORLD;

		if (mt->offsets[0])
		{
			data = (byte *)mt + mt->offsets[0];
			tx2 = tx;
		}
		else
		{
			data = (byte *)tx2 + tx2->offsets[0];
			tx2 = r_notexture_mip;
		}

		if (strstr(tx->name,"plat_top1")) 
		{
			if (CRC_Block((byte *)(tx+1), tx->width * tx->height)==24428)
				sprintf(tx->name,"plat_top1_cable");
			else
				sprintf(tx->name,"plat_top1_bolt");
		}
		else
		{
			if (strstr(tx->name,"metal5_2")) 
			{
				if (CRC_Block((byte *)(tx+1), tx->width * tx->height)==49173)
					sprintf(tx->name,"metal5_2_x");
				else
					sprintf(tx->name,"metal5_2_arc");
			}
			else
			{
				if (strstr(tx->name,"metal5_4")) 
				{
					if (CRC_Block((byte *)(tx+1), tx->width * tx->height)==20977)
						sprintf(tx->name,"metal5_4_double");
					else
						sprintf(tx->name,"metal5_4_arc");
				}
				else
				{
					if (strstr(tx->name,"metal5_8")) 
					{
						if (CRC_Block((byte *)(tx+1), tx->width * tx->height)==48444)
							sprintf(tx->name,"metal5_8_rune");
						else
							sprintf(tx->name,"metal5_8_back");
					}
					else
					{
						if (strstr(tx->name,"metal5_8")) 
						{
							if (CRC_Block((byte *)(tx+1), tx->width * tx->height)==48444)
								sprintf(tx->name,"metal5_8_rune");
							else
								sprintf(tx->name,"metal5_8_back");
						}
						else
						{
							if (strstr(tx->name,"window03")) 
							{
								if (CRC_Block((byte *)(tx+1), tx->width * tx->height)==63697) // e4m2 variant
									sprintf(tx->name,"window03_e4m2");
							}					
						}
					}
				}
			}
		}

		tx->gl_texturenum = GL_LoadTexture (tx2->name, tx2->width, tx2->height, data, texture_mode, 1);		

		if (!ISTURBTEX(tx->name) && Img_HasFullbrights(data, tx2->width * tx2->height))
			tx->fb_texturenum = GL_LoadTexture (va("@fb_%s", tx2->name), tx2->width, tx2->height, data, texture_mode | TEX_FULLBRIGHT, 1);
	}

// sequence the animations
	for (i=0 ; i<m->nummiptex ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// already sequenced
		
	// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
		{
			Sys_Error ("Bad animating texture %s", tx->name);
		}

		for (j=i+1 ; j<m->nummiptex ; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp(tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
			{
				Sys_Error ("Bad animating texture %s", tx->name);
			}
		}
		
#define	ANIM_CYCLE	2
	// link them all together
		for (j=0 ; j<max ; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error ("Mod_LoadTextures: Missing frame %i of %s", j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[(j+1)%max];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j=0 ; j<altmax ; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error ("Mod_LoadTextures: Missing frame %i of %s", j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[(j+1)%altmax];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

// joe: from FuhQuake
static byte *LoadColoredLighting (char *name, char **litfilename)
{
	byte		*data;
	char		*tmpname;
	extern	cvar_t	gl_loadlitfiles;

	if (!gl_loadlitfiles.value)
		return NULL;

	tmpname = sv_mapname.string;

	if (strcmp(name, va("maps/%s.bsp", tmpname)))
		return NULL;

	*litfilename = va("maps/%s.lit", tmpname);
	data = COM_LoadHunkFile (*litfilename);

	if (!data)
	{
		*litfilename = va("lits/%s.lit", tmpname);
		data = COM_LoadHunkFile (*litfilename);
	}

	return data;
}

/*
=================
Mod_LoadLighting
=================
*/

void Mod_LoadLighting (lump_t *l)
{
	int	i, lit_ver;
	byte	*in, *out, *data, d;
	char	*litfilename;

	loadmodel->lightdata = NULL;
	
	if (!l->filelen)
		return;

	// check for a .lit file
	data = LoadColoredLighting (loadmodel->name, &litfilename);

	if (data)
	{
		if (com_filesize < 8 || strncmp(data, "QLIT", 4))
			Con_Printf ("Corrupt .lit file (%s)...ignoring\n", COM_SkipPath(litfilename));
		else if (l->filelen * 3 + 8 != com_filesize)
			Con_Printf ("Warning: .lit file (%s) has incorrect size\n", COM_SkipPath(litfilename));
		else if ((lit_ver = LittleLong(((int *)data)[1])) != 1)
			Con_Printf ("Unknown .lit file version (v%d)\n", lit_ver);
		else
		{
			Con_DPrintf (1,"Static coloured lighting loaded\n");
			// mh - lit file fix
			i = LittleLong (((int *) data)[1]);

			if (i == 1)
			{
				Con_DPrintf (1,"%s loaded.\n", litfilename);
				loadmodel->lightdata = (byte *) MallocZ (l->filelen * 3);
				memcpy (loadmodel->lightdata, data + 8, l->filelen * 3);
				return;
			}

			loadmodel->lightdata = data + 8;
			in = mod_base + l->fileofs;
			out = loadmodel->lightdata;
			for (i=0 ; i<l->filelen ; i++)
			{
				int	b = max(out[3*i], max(out[3*i+1], out[3*i+2]));

				if (!b)
				{
					out[3*i] = out[3*i+1] = out[3*i+2] = in[i];
				}
				else
				{	// too bright
					float	r = in[i] / (float)b;

					out[3*i+0] = (int)(r * out[3*i+0]);
					out[3*i+1] = (int)(r * out[3*i+1]);
					out[3*i+2] = (int)(r * out[3*i+2]);
				}
			}
			return;
		}
	}

	// no .lit found, expand the white lighting data to color
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}

	loadmodel->lightdata = (byte *) MallocZ (l->filelen * 3);
	// place the file at the end, so it will not be overwritten until the very last write
	in = loadmodel->lightdata + l->filelen * 2;
	out = loadmodel->lightdata;
	memcpy (in, mod_base + l->fileofs, l->filelen);
	for (i = 0 ; i < l->filelen ; i++, out += 3)
	{
		d = *in++;
		out[0] = out[1] = out[2] = d;
	}
}

/*
=================
Mod_LoadVisibility
=================
*/

void Mod_LoadVisibility (lump_t *l)
{	
	if (!l->filelen)
	{		
		loadmodel->visdata = NULL;
		return;
	}
	//loadmodel->visdata = Hunk_AllocName (l->filelen,  va("%s_@visdata", loadmodel->name));
	loadmodel->visdata = (byte *) MallocZ (l->filelen);
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}

/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}
	//loadmodel->entities = Hunk_AllocName (l->filelen,  va("%s_@entities", loadmodel->name));
	loadmodel->entities = (char *) MallocZ (l->filelen);
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int		i, count;

	in = (dvertex_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	//out = (mvertex_t *)Hunk_AllocName (count * sizeof(*out),  va("%s_@mvertex", loadmodel->name));
	out = (mvertex_t *) MallocZ (count * sizeof (mvertex_t));

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in, *out;
	int			i, j, count;

	in = (dmodel_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadSubmodels: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);

	if (count > MAX_MODELS)	
		Host_Error ("Mod_LoadSubmodels: count > MAX_MODELS");


	out = (dmodel_t *) MallocZ (count * sizeof (*out));
	
	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);
		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
	// johnfitz -- check world visleafs -- adapted from bjp
	out = loadmodel->submodels;

	if (out->visleafs > MAX_MAP_LEAFS)
		Host_Error ("Mod_LoadSubmodels: too many visleafs (%d, max = %d) in %s", out->visleafs, MAX_MAP_LEAFS, loadmodel->name);

	if (out->visleafs > 8192)
		Con_Warning ("%i visleafs exceeds standard limit of 8192.\n", out->visleafs);
	//johnfitz
}

/*
=================
Mod_LoadEdges
=================
*/
static void Mod_LoadEdges (lump_t *l, int bsp2)
{
	medge_t *out;
	dledge_t *lin;
	dsedge_t *sin;

	int 	i, count;

	if (bsp2)
	{
		lin = (dledge_t *)(mod_base + l->fileofs);
		sin = NULL;
		if (l->filelen % sizeof(*lin))
			Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

		count = l->filelen / sizeof(*lin);
		//out = (medge_t *) Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);
		out = (medge_t *) MallocZ ((count + 1) * sizeof (*out));//R00k: use system memory for large maps
		loadmodel->edges = out;
		loadmodel->numedges = count;

		for (i=0 ; i<count ; i++, lin++, out++)
		{
			out->v[0] = LittleLong(lin->v[0]);
			out->v[1] = LittleLong(lin->v[1]);
		}
	}
	else
	{
		sin = (dsedge_t *)(mod_base + l->fileofs);
		lin = NULL;
		if (l->filelen % sizeof(*sin))
			Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

		count = l->filelen / sizeof(*sin);
		//out = (medge_t *) Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);
		out = (medge_t *) MallocZ ((count + 1) * sizeof (*out));

		loadmodel->edges = out;
		loadmodel->numedges = count;

		for (i=0 ; i<count ; i++, sin++, out++)
		{
			out->v[0] = (unsigned short)LittleShort(sin->v[0]);
			out->v[1] = (unsigned short)LittleShort(sin->v[1]);
		}
	}
}


/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t	*in;
	mtexinfo_t	*out;
	int 		i, j, count, miptex;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBrushModel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	//out = Hunk_AllocName (count * sizeof(*out), va("%s_@texinfo", loadmodel->name));
	out = (mtexinfo_t *) MallocZ (count * sizeof (*out));

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<8 ; j++)
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);
	
		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Host_Error ("Mod_LoadTexinfo: miptex >= loadmodel->numtextures");

			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip;	// texture not found
				out->flags = 0;
			}
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	float		mins[2], maxs[2], val;
	int		i, j, e, bmins[2], bmaxs[2];
	mvertex_t	*v;
	mtexinfo_t	*tex;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + v->position[1] * tex->vecs[j][1] + v->position[2] * tex->vecs[j][2] + tex->vecs[j][3];

			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = floor (mins[i] / 16);
		bmaxs[i] = ceil (maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;

		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 2028)
			Host_Error ("CalcSurfaceExtents: Bad surface extents");
	}
}


typedef struct
{
	char	*name;
	int		len;
	int		flags;
} sflags_t;

static sflags_t surfaceflags[] =
{
	// mirrors
	{ "window01_", 9, SURF_MIRROR },
	{ "window02_", 9, SURF_MIRROR },
	// Runes
	{ "rune", 4, SURF_METAL },
	// Metal misc
	{ "metal6_1", 8, SURF_METAL },
	{ "metal6_2", 8, SURF_METAL },
	{ "metal6_3", 8, SURF_METAL },
	{ "metal6_4", 8, SURF_METAL },
	{ "switch_1", 8, SURF_METAL },
	{ "wkey02_1", 8, SURF_METAL },
	{ "wkey02_2", 8, SURF_METAL },
	{ "wkey02_3", 8, SURF_METAL },
	{ "+0mtlsw", 7, SURF_METAL },
	{ "+1mtlsw", 7, SURF_METAL },
	{ "+2mtlsw", 7, SURF_METAL },
	{ "+3mtlsw", 7, SURF_METAL },
	{ "+amtlsw", 7, SURF_METAL },
	{ "arrow_m", 7, SURF_METAL },
	{ "azswitch3", 9, SURF_METAL },
	{ "key", 3, SURF_METAL },
	// end of list
	{ (NULL), 0, 0 }
};

static int numsflags = sizeof(surfaceflags) / sizeof(surfaceflags[0]) - 1;

static int Mod_FindSurfaceFlags(char *name)
{
	int	i;

	for (i = 0; i < numsflags; i++) {
		if (surfaceflags[i].len > 0) {
			if (!strncmp(name, surfaceflags[i].name, surfaceflags[i].len)) {
				return surfaceflags[i].flags;
			}
		} else {
			if (!strcmp(name, surfaceflags[i].name)) {
				return surfaceflags[i].flags;
			}
		}
	}
	return 0;
}

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l, qboolean bsp2)
{
	dsface_t	*ins;
	dlface_t	*inl;
	msurface_t 	*out;
	int		i, count, surfnum, planenum, side, lofs, texinfon;

	if (bsp2)
	{
		ins = NULL;
		inl = (dlface_t *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*inl))
			Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
		count = l->filelen / sizeof(*inl);
	}
	else
	{
		ins = (dsface_t *)(mod_base + l->fileofs);
		inl = NULL;
		if (l->filelen % sizeof(*ins))
			Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
		count = l->filelen / sizeof(*ins);
	}
	//out = Hunk_AllocName (count * sizeof(*out), va("%s_@surfaces", loadmodel->name));
	out = (msurface_t *) MallocZ (count * sizeof(*out));
	//johnfitz -- warn mappers about exceeding old limits
	if (count > 32767 && !bsp2)
		Con_Warning ("%i faces exceeds standard limit of 32767.\n", count);
	//johnfitz

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum=0 ; surfnum<count ; surfnum++, out++)
	{
		if (bsp2)
		{
			out->firstedge = LittleLong(inl->firstedge);
			out->numedges = LittleLong(inl->numedges);
			planenum = LittleLong(inl->planenum);
			side = LittleLong(inl->side);
			texinfon = LittleLong (inl->texinfo);
			for (i=0 ; i<MAXLIGHTMAPS ; i++)
				out->styles[i] = inl->styles[i];
			lofs = LittleLong(inl->lightofs);
			inl++;
		}
		else
		{
			out->firstedge = LittleLong(ins->firstedge);
			out->numedges = LittleShort(ins->numedges);
			planenum = LittleShort(ins->planenum);
			side = LittleShort(ins->side);
			texinfon = LittleShort (ins->texinfo);
			for (i=0 ; i<MAXLIGHTMAPS ; i++)
				out->styles[i] = ins->styles[i];
			lofs = LittleLong(ins->lightofs);
			ins++;
		}	
		out->flags = 0;

		if (side) out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + texinfon;
		CalcSurfaceExtents (out);

	// lighting info
		if (lofs == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + lofs * 3;

	// set the drawing flags flag
		if (ISSKYTEX(out->texinfo->texture->name))	// sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			GL_SubdivideSurface (out);	// cut up polygon for warps
			continue;
		}

		if (ISTURBTEX(out->texinfo->texture->name)) 
		{ // turbulent
			out->flags |= (SURF_DRAWTURB|SURF_DRAWTILED);

			if (ISLAVATEX(out->texinfo->texture->name))		out->flags |= SURF_DRAWLAVA;
			else 
			if (ISSLIMETEX(out->texinfo->texture->name)) 	out->flags |= SURF_DRAWSLIME;
			else 
			if (ISTELETEX(out->texinfo->texture->name)) 	out->flags |= SURF_DRAWTELE;
			else 
															out->flags |= SURF_DRAWWATER;

			for (i=0 ; i<2 ; i++) 
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
		
			GL_SubdivideSurface (out); // cut up polygon for warps
			continue;
		} 

		if (ISALPHATEX(out->texinfo->texture->name))
			out->flags |= SURF_DRAWALPHA;

		// keep these flags seperate or else !!
		if (!(out->sflags = Mod_FindSurfaceFlags (out->texinfo->texture->name))) 
		{
			out->sflags |= SURF_DETAIL;
		}		
	}
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
static void Mod_LoadNodes_S (lump_t *l)
{
	int			i, j, count, p;
	dsnode_t	*in;
	mnode_t		*out;

	in = (dsnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
//	out = (mnode_t *) Hunk_AllocName ( count*sizeof(*out), loadname);
	out = (mnode_t *) MallocZ (count * sizeof (*out));

	//johnfitz -- warn mappers about exceeding old limits
	if (count > 32767)
		Con_Warning ("%i nodes exceeds standard limit of 32767.\n", count);
	//johnfitz

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = (unsigned short)LittleShort (in->firstface); //johnfitz -- explicit cast as unsigned short
		out->numsurfaces = (unsigned short)LittleShort (in->numfaces); //johnfitz -- explicit cast as unsigned short

		for (j=0 ; j<2 ; j++)
		{
			//johnfitz -- hack to handle nodes > 32k, adapted from darkplaces
			p = (unsigned short)LittleShort(in->children[j]);
			if (p < count)
				out->children[j] = loadmodel->nodes + p;
			else
			{
				p = 0xffff - p; //note this uses 65535 intentionally, -1 is leaf 0
				if (p < loadmodel->numleafs)
					out->children[j] = (mnode_t *)(loadmodel->leafs + p);
				else
				{
					Con_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
					out->children[j] = (mnode_t *)(loadmodel->leafs); //map it to the solid leaf
				}
			}
			//johnfitz
		}
	}
}

static void Mod_LoadNodes_L1 (lump_t *l)
{
	int			i, j, count, p;
	dl1node_t	*in;
	mnode_t		*out;

	in = (dl1node_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadNodes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	
	out = (mnode_t *) MallocZ (count * sizeof (*out));

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleLong (in->firstface); //johnfitz -- explicit cast as unsigned short
		out->numsurfaces = LittleLong (in->numfaces); //johnfitz -- explicit cast as unsigned short

		for (j=0 ; j<2 ; j++)
		{
			//johnfitz -- hack to handle nodes > 32k, adapted from darkplaces
			p = LittleLong(in->children[j]);
			if (p >= 0 && p < count)
				out->children[j] = loadmodel->nodes + p;
			else
			{
				p = 0xffffffff - p; //note this uses 65535 intentionally, -1 is leaf 0
				if (p >= 0 && p < loadmodel->numleafs)
					out->children[j] = (mnode_t *)(loadmodel->leafs + p);
				else
				{
					Con_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
					out->children[j] = (mnode_t *)(loadmodel->leafs); //map it to the solid leaf
				}
			}
			//johnfitz
		}
	}
}

void Mod_LoadNodes_L2 (lump_t *l)
{
	int			i, j, count, p;
	dl2node_t	*in;
	mnode_t		*out;

	in = (dl2node_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadNodes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mnode_t *) MallocZ (count * sizeof (*out));

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleLong (in->firstface); //johnfitz -- explicit cast as unsigned short
		out->numsurfaces = LittleLong (in->numfaces); //johnfitz -- explicit cast as unsigned short

		for (j=0 ; j<2 ; j++)
		{
			//johnfitz -- hack to handle nodes > 32k, adapted from darkplaces
			p = LittleLong(in->children[j]);
			if (p > 0 && p < count)
				out->children[j] = loadmodel->nodes + p;
			else
			{
				p = 0xffffffff - p; //note this uses 65535 intentionally, -1 is leaf 0
				if (p >= 0 && p < loadmodel->numleafs)
					out->children[j] = (mnode_t *)(loadmodel->leafs + p);
				else
				{
					Con_Printf("Mod_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->numleafs);
					out->children[j] = (mnode_t *)(loadmodel->leafs); //map it to the solid leaf
				}
			}
			//johnfitz
		}
	}
}

void Mod_LoadNodes (lump_t *l, int bsp2)
{
	if (bsp2 == 2)
		Mod_LoadNodes_L2(l);
	else if (bsp2)
		Mod_LoadNodes_L1(l);
	else
		Mod_LoadNodes_S(l);

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}


/*
=================
Mod_LoadLeafs
=================
*/
void Mod_ProcessLeafs_S (dsleaf_t *in, int filelen)
{
	mleaf_t		*out;
	int			i, j, count, p;

	if (filelen % sizeof(*in))
		Sys_Error ("Mod_ProcessLeafs: funny lump size in %s", loadmodel->name);
	count = filelen / sizeof(*in);
	//out = (mleaf_t *) Hunk_AllocName ( count*sizeof(*out), loadname);
	out = (mleaf_t *) MallocZ (count * sizeof (*out));
	//johnfitz
	if (count > 32768)
		Con_Warning("Mod_LoadLeafs: %i leafs exceeds limit of 65535.\n", count);
	//johnfitz

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + (unsigned short)LittleShort(in->firstmarksurface); //johnfitz -- unsigned short
		out->nummarksurfaces = (unsigned short)LittleShort(in->nummarksurfaces); //johnfitz -- unsigned short

		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags = NULL;

		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];
		if (!isDedicated)
		{
			if (out && (out->contents != CONTENTS_EMPTY))
			{
				for (j=0 ; j<out->nummarksurfaces ; j++)

					out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
			}
		}



	}
}

void Mod_ProcessLeafs_L1 (dl1leaf_t *in, int filelen)
{
	mleaf_t		*out;
	int			i, j, count, p;

	if (filelen % sizeof(*in))
		Sys_Error ("Mod_ProcessLeafs: funny lump size in %s", loadmodel->name);

	count = filelen / sizeof(*in);

	//out = (mleaf_t *) Hunk_AllocName (count * sizeof(*out), loadname);
	out = (mleaf_t *) MallocZ (count * sizeof (*out));

	if (count > MAX_MAP_LEAFS)
		Host_Error ("Mod_LoadLeafs: %i leafs exceeds limit of %i.\n", count, MAX_MAP_LEAFS);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + LittleLong(in->firstmarksurface); //johnfitz -- unsigned short
		out->nummarksurfaces = LittleLong(in->nummarksurfaces); //johnfitz -- unsigned short

		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags = NULL;

		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];
		if (!isDedicated)
		{
			if (out && (out->contents != CONTENTS_EMPTY))
			{
				for (j=0 ; j<out->nummarksurfaces ; j++)

					out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
			}
		}
	}
}

void Mod_ProcessLeafs_L2 (dl2leaf_t *in, int filelen)
{
	mleaf_t		*out;
	int			i, j, count, p;

	if (filelen % sizeof(*in))
		Sys_Error ("Mod_ProcessLeafs: funny lump size in %s", loadmodel->name);

	count = filelen / sizeof(*in);

	//out = (mleaf_t *) Hunk_AllocName (count * sizeof(*out), loadname);
	out = (mleaf_t *) MallocZ (count * sizeof (*out));

	if (count > MAX_MAP_LEAFS)
		Host_Error ("Mod_LoadLeafs: %i leafs exceeds limit of %i.\n", count, MAX_MAP_LEAFS);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleFloat (in->mins[j]);
			out->minmaxs[3+j] = LittleFloat (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + LittleLong(in->firstmarksurface); //johnfitz -- unsigned short
		out->nummarksurfaces = LittleLong(in->nummarksurfaces); //johnfitz -- unsigned short

		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags = NULL;

		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		if (!isDedicated)
		{
			if (out && (out->contents != CONTENTS_EMPTY))
			{
				for (j=0 ; j<out->nummarksurfaces ; j++)

					out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
			}
		}
	}
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l, int bsp2)
{
	void *in = (void *)(mod_base + l->fileofs);

	if (bsp2 == 2)
		Mod_ProcessLeafs_L2 ((dl2leaf_t *)in, l->filelen);
	else if (bsp2)
		Mod_ProcessLeafs_L1 ((dl1leaf_t *)in, l->filelen);
	else
		Mod_ProcessLeafs_S  ((dsleaf_t *) in, l->filelen);
}

//spike
void Mod_CheckWaterVis(void)
{
	mleaf_t		*leaf, *other;
	int i, j, k;
	int numclusters = loadmodel->submodels[0].visleafs;
	int contentfound = 0;
	int contenttransparent = 0;
	int contenttype;

	if (r_novis.value)
	{	//all can be
		loadmodel->contentstransparent = (SURF_DRAWWATER|SURF_DRAWTELE|SURF_DRAWSLIME|SURF_DRAWLAVA);
		return;
	}

	//pvs is 1-based. leaf 0 sees all (the solid leaf).
	//leaf 0 has no pvs, and does not appear in other leafs either, so watch out for the biases.
	for (i=0,leaf=loadmodel->leafs+1 ; i<numclusters ; i++, leaf++)
	{
		byte *vis;
		if (leaf->contents == CONTENTS_WATER)
		{
			if ((contenttransparent & (SURF_DRAWWATER|SURF_DRAWTELE))==(SURF_DRAWWATER|SURF_DRAWTELE))
				continue;
			//this check is somewhat risky, but we should be able to get away with it.
			for (contenttype = 0, i = 0; i < leaf->nummarksurfaces; i++)
				if (leaf->firstmarksurface[i]->flags & (SURF_DRAWWATER|SURF_DRAWTELE))///fixme ENGINE CRASHES ON MAP CHANGE HERE
				{
					contenttype = leaf->firstmarksurface[i]->flags & (SURF_DRAWWATER|SURF_DRAWTELE);
					break;
				}
			//its possible that this leaf has absolutely no surfaces in it, turb or otherwise.
			if (contenttype == 0)
				continue;
		}
		else if (leaf->contents == CONTENTS_SLIME)
			contenttype = SURF_DRAWSLIME;
		else if (leaf->contents == CONTENTS_LAVA)
			contenttype = SURF_DRAWLAVA;
		//fixme: tele
		else
			continue;
		if (contenttransparent & contenttype)
		{
			nextleaf:
			continue;	//found one of this type already
		}
		contentfound |= contenttype;
		vis = Mod_DecompressVis(leaf->compressed_vis, loadmodel);
		for (j = 0; j < (numclusters+7)/8; j++)
		{
			if (vis[j])
			{
				for (k = 0; k < 8; k++)
				{
					if (vis[j] & (1u<<k))
					{
						other = &loadmodel->leafs[(j<<3)+k+1];
						if (leaf->contents != other->contents)
						{
//							Con_Printf("%p:%i sees %p:%i\n", leaf, leaf->contents, other, other->contents);
							contenttransparent |= contenttype;
							goto nextleaf;
						}
					}
				}
			}
		}
	}

	if (!contenttransparent)
	{
		if (loadmodel->isworldmodel)
			Con_Printf("%s is not watervised\n", loadmodel->name);
		Cvar_SetValue("r_wateralpha", 1.0);
	}
	else
	{
		if (loadmodel->isworldmodel)
			Con_Printf("%s is vised for transparent", loadmodel->name);
		/*
		if (contenttransparent & SURF_DRAWWATER)
			Con_Printf(" water");
		if (contenttransparent & SURF_DRAWTELE)
			Con_Printf(" tele");
		if (contenttransparent & SURF_DRAWLAVA)
			Con_Printf(" lava");
		if (contenttransparent & SURF_DRAWSLIME)
			Con_Printf(" slime");
		Con_Printf("\n");*/
		Cvar_SetValue("r_wateralpha", map_fallbackalpha);		
	}

	//any types that we didn't find are assumed to be transparent.
	//this allows submodels to work okay (eg: ad uses func_illusionary teleporters for some reason).
	loadmodel->contentstransparent = contenttransparent | (~contentfound & (SURF_DRAWWATER|SURF_DRAWTELE|SURF_DRAWSLIME|SURF_DRAWLAVA));
}

/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes (lump_t *l, qboolean bsp2)
{
	dsclipnode_t *ins;
	dlclipnode_t *inl;

	mclipnode_t *out; //johnfitz -- was dclipnode_t
	int			i, count;
	hull_t		*hull;

	if (bsp2)
	{
		ins = NULL;
		inl = (dlclipnode_t *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*inl))
			Host_Error ("Mod_LoadClipnodes: funny lump size in %s",loadmodel->name);

		count = l->filelen / sizeof(*inl);
	}
	else
	{
		ins = (dsclipnode_t *)(mod_base + l->fileofs);
		inl = NULL;
		if (l->filelen % sizeof(*ins))
			Host_Error ("Mod_LoadClipnodes: funny lump size in %s",loadmodel->name);

		count = l->filelen / sizeof(*ins);
	}

	out = (mclipnode_t *) MallocZ (count * sizeof (*out));

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;

	if (bsp2)
	{
		for (i=0 ; i<count ; i++, out++, inl++)
		{
			out->planenum = LittleLong(inl->planenum);

			//johnfitz -- bounds check
			if (out->planenum < 0 || out->planenum >= loadmodel->numplanes)
				Host_Error ("Mod_LoadClipnodes: planenum out of bounds");
			//johnfitz

			out->children[0] = LittleLong(inl->children[0]);
			out->children[1] = LittleLong(inl->children[1]);

		}
	}
	else
	{
		for (i=0 ; i<count ; i++, out++, ins++)
		{
			out->planenum = LittleLong(ins->planenum);

		//johnfitz -- bounds check
		if (out->planenum < 0 || out->planenum >= loadmodel->numplanes)
			Host_Error ("Mod_LoadClipnodes: planenum out of bounds");
		//johnfitz

			//johnfitz -- support clipnodes > 32k
			out->children[0] = (unsigned short)LittleShort(ins->children[0]);
			out->children[1] = (unsigned short)LittleShort(ins->children[1]);

			if (out->children[0] >= count)
				out->children[0] -= 65536;
			if (out->children[1] >= count)
				out->children[1] -= 65536;
			//johnfitz
		}
	}
}

/*
=================
Mod_MakeHull0

Duplicate the drawing hull structure as a clipping hull
=================
*/
void Mod_MakeHull0 (void)
{
	mnode_t		*in, *child;
	mclipnode_t *out; //johnfitz -- was dclipnode_t
	int		i, j, count;
	hull_t		*hull;
	
	hull = &loadmodel->hulls[0];	
	
	in = loadmodel->nodes;
	count = loadmodel->numnodes;

	out = (mclipnode_t *) MallocZ (count * sizeof (*out));

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j=0 ; j<2 ; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l, int bsp2)
{
	int		i, j, count;
	msurface_t **out;

	if (bsp2)
	{
		unsigned int *in = (unsigned int *)(mod_base + l->fileofs);

		if (l->filelen % sizeof(*in))
			Host_Error ("Mod_LoadMarksurfaces: funny lump size in %s",loadmodel->name);

		count = l->filelen / sizeof(*in);
		out = (msurface_t **) MallocZ (count * sizeof (*out));

		loadmodel->marksurfaces = out;
		loadmodel->nummarksurfaces = count;

		for (i=0 ; i<count ; i++)
		{
			j = LittleLong(in[i]);
			if (j >= loadmodel->numsurfaces)
				Host_Error ("Mod_LoadMarksurfaces: bad surface number");
			out[i] = loadmodel->surfaces + j;
		}
	}
	else
	{
		short *in = (short *)(mod_base + l->fileofs);

		if (l->filelen % sizeof(*in))
			Host_Error ("Mod_LoadMarksurfaces: funny lump size in %s",loadmodel->name);

		count = l->filelen / sizeof(*in);
		out = (msurface_t **) MallocZ (count * sizeof (*out));

		loadmodel->marksurfaces = out;
		loadmodel->nummarksurfaces = count;

		for (i=0 ; i<count ; i++)
		{
			j = (unsigned short)LittleShort(in[i]); //johnfitz -- explicit cast as unsigned short
			if (j >= loadmodel->numsurfaces)
				Host_Error ("Mod_LoadMarksurfaces: bad surface number");
			out[i] = loadmodel->surfaces + j;
		}
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{	
	int	i, count, *in, *out;
	
	in = (int *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadSurfedges: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	//out = Hunk_AllocName (count * sizeof(*out), va("%s_@surfedges", loadmodel->name));
	out = (int *) MallocZ (count * sizeof (*out));

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int		i, j, count, bits;
	mplane_t	*out;
	dplane_t 	*in;
	
	in = (dplane_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadPlanes: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	//out = Hunk_AllocName (count * 2 * sizeof(*out), va("%s_@planes", loadmodel->name));
	out = (mplane_t *) MallocZ (count * sizeof (*out));	
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}
 /*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t		corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength (corner);
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i, j;
	int			bsp2;
	dheader_t	*header;
	dmodel_t 	*bm;

	loadmodel->type = mod_brush;

	header = (dheader_t *)buffer;

	mod->bspversion = LittleLong (header->version);

	switch(mod->bspversion)
	{
	case BSPVERSION:
		bsp2 = false;
		break;
	case BSP2VERSION_2PSB:
		bsp2 = 1;	//first iteration
		break;
	case BSP2VERSION_BSP2:
		bsp2 = 2;	//sanitised revision
		break;
	default:
		Host_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, mod->bspversion, BSPVERSION);
		break;
	}

	loadmodel->isworldmodel = !strcmp (loadmodel->name, va("maps/%s.bsp", sv_mapname.string));
// swap all the lumps
	mod_base = (byte *)header;

	for (i = 0; i < (int) sizeof(dheader_t) / 4; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

// load into heap
	if(!isDedicated) 
	{
		Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
		Mod_LoadEdges (&header->lumps[LUMP_EDGES], bsp2);
		Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
		Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
		Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	}
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	if(!isDedicated) 
	{
		Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
		Mod_LoadFaces (&header->lumps[LUMP_FACES], bsp2);
		Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES], bsp2);

	}
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS], bsp2);
	Mod_LoadNodes (&header->lumps[LUMP_NODES], bsp2);
	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES], bsp2);
	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();

	mod->numframes = 2;		// regular and alternate animation

//
// set up the submodels (FIXME: this is confusing)
//

	// johnfitz -- okay, so that i stop getting confused every time i look at this loop, here's how it works:
	// we're looping through the submodels starting at 0.  Submodel 0 is the main model, so we don't have to
	// worry about clobbering data the first time through, since it's the same data.  At the end of the loop,
	// we create a new copy of the data to use the next time through.
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);
		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[10];

			_snprintf (name, sizeof(name), "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
//	Mod_CheckWaterVis(); (FIXME)
}

/*
==============================================================================

				ALIAS MODELS

==============================================================================
*/

aliashdr_t	*pheader;

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes. a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int		posenum;

byte aliasbboxmins[3], aliasbboxmaxs[3];

/*
=================
Mod_LoadAliasFrame
=================
*/
void *Mod_LoadAliasFrame (void *pin, maliasframedesc_t *frame)
{
	trivertx_t	*pinframe;
	int		i;
	daliasframe_t	*pdaliasframe;
	
	pdaliasframe = (daliasframe_t *)pin;

	strcpy (frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];

		aliasbboxmins[i] = min(aliasbboxmins[i], frame->bboxmin.v[i]);
		aliasbboxmaxs[i] = max(aliasbboxmaxs[i], frame->bboxmax.v[i]);
	}

	pinframe = (trivertx_t *)(pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return (void *)pinframe;
}


/*
=================
Mod_LoadAliasGroup
=================
*/
void *Mod_LoadAliasGroup (void *pin,  maliasframedesc_t *frame)
{
	daliasgroup_t		*pingroup;
	int			i, numframes;
	daliasinterval_t	*pin_intervals;
	void			*ptemp;
	
	pingroup = (daliasgroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];

		aliasbboxmins[i] = min(aliasbboxmins[i], frame->bboxmin.v[i]);
		aliasbboxmaxs[i] = max(aliasbboxmaxs[i], frame->bboxmax.v[i]);
	}

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	frame->interval = LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		poseverts[posenum] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		posenum++;

		ptemp = (trivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

//=========================================================

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct
{
	short	x, y;
} floodfill_t;

// must be a power of 2
#define	FLOODFILL_FIFO_SIZE	0x1000
#define	FLOODFILL_FIFO_MASK	(FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP(off, dx, dy)					\
{									\
	if (pos[off] == fillcolor)					\
	{								\
		pos[off] = 255;						\
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy);	\
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;		\
	}								\
	else if (pos[off] != 255) fdc = pos[off];			\
}

void Mod_FloodFillSkin (byte *skin, int skinwidth, int skinheight)
{
	byte		fillcolor = *skin;	// assume this is the pixel to fill
	floodfill_t	fifo[FLOODFILL_FIFO_SIZE];
	int		i, inpt = 0, outpt = 0, filledcolor = -1;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i=0 ; i<256 ; ++i)
		{
			if (d_8to24table[i] == (255 << 0))	// alpha 1.0
			{
				filledcolor = i;
				break;
			}
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
		return;

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int	x = fifo[outpt].x, y = fifo[outpt].y;
		int	fdc = filledcolor;
		byte	*pos = &skin[x+skinwidth*y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)
			FLOODFILL_STEP(-1, -1, 0);
		if (x < skinwidth - 1)
			FLOODFILL_STEP(1, 1, 0);
		if (y > 0)
			FLOODFILL_STEP(-skinwidth, 0, -1);
		if (y < skinheight - 1)
			FLOODFILL_STEP(skinwidth, 0, 1);
		skin[x+skinwidth*y] = fdc;
	}
}

/*
=================
Mod_LoadAliasModelTexture
=================
*/
//R00k: todo: add a cvar for custom path

void Mod_LoadAliasModelTexture (char *identifier, int picmip_flag, int *gl_texnum, int *fb_texnum, int *gl_pants, int *gl_shirt)
{
	char	loadpath[64];
	extern cvar_t cl_teamskin;

	if ((!gl_externaltextures_models.value)&&((loadmodel->modhint != MOD_PLAYER)))
		return;

	if ((loadmodel->modhint == MOD_PLAYER)&&(!cl_teamskin.value))
		return;

	Q_snprintfz (loadpath, sizeof(loadpath), "progs/%s", identifier);
	*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, picmip_flag | TEX_BLEND);
	
	if (*gl_texnum)
	{
		*gl_pants = GL_LoadTextureImage (va("%s_pants", loadpath), va("@pants_%s", identifier), 0, 0, picmip_flag | TEX_BLEND);
		*gl_shirt = GL_LoadTextureImage (va("%s_shirt", loadpath), va("@shirt_%s", identifier), 0, 0, picmip_flag | TEX_BLEND);
		*fb_texnum = GL_LoadTextureImage (va("%s_luma", loadpath), va("@fb_%s", identifier), 0, 0, picmip_flag | TEX_LUMA);
		return;
	}

	if (!*gl_texnum)
	{
		Q_snprintfz (loadpath, sizeof(loadpath), "textures/%s", identifier);
		*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, picmip_flag | TEX_BLEND);
		if (*gl_texnum)
		{
			*gl_pants = GL_LoadTextureImage (va("%s_pants", loadpath), va("@pants_%s", identifier), 0, 0, picmip_flag | TEX_BLEND);
			*gl_shirt = GL_LoadTextureImage (va("%s_shirt", loadpath), va("@shirt_%s", identifier), 0, 0, picmip_flag | TEX_BLEND);
			*fb_texnum = GL_LoadTextureImage (va("%s_luma", loadpath), va("@fb_%s", identifier), 0, 0, picmip_flag | TEX_LUMA);
			return;
		}
	}

	if (!*gl_texnum)
	{
		Q_snprintfz (loadpath, sizeof(loadpath), "textures/models/%s", identifier);//R00k: this is the least common setup...moved down here.
		*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, picmip_flag | TEX_BLEND);
		if (*gl_texnum)
		{
			*gl_pants = GL_LoadTextureImage (va("%s_pants", loadpath), va("@pants_%s", identifier), 0, 0, picmip_flag | TEX_BLEND);
			*gl_shirt = GL_LoadTextureImage (va("%s_shirt", loadpath), va("@shirt_%s", identifier), 0, 0, picmip_flag | TEX_BLEND);
			*fb_texnum = GL_LoadTextureImage (va("%s_luma", loadpath), va("@fb_%s", identifier), 0, 0, picmip_flag | TEX_LUMA);
		}
	}
}

/*
===============
Mod_LoadAllSkins
===============
*/
void *Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype)
{
	int		i, j, k, size, groupskins, gl_pants, gl_shirt, gl_texnum, fb_texnum, texture_mode;
	char	basename[64], identifier[64];
	byte	*texels;
	extern	cvar_t	cl_teamskin;
	daliasskingroup_t		*pinskingroup;
	daliasskininterval_t	*pinskinintervals;

	if (numskins < 1 || numskins > MAX_SKINS)
		Sys_Error ("Mod_LoadAllSkins: Invalid # of skins: %d\n", numskins);

	size = pheader->skinwidth * pheader->skinheight;

	COM_StripExtension (COM_SkipPath(loadmodel->name), basename);

	texture_mode = TEX_ALPHA | TEX_MIPMAP;

	for (i=0 ; i<numskins ; i++)
	{
		if (pskintype->type == ALIAS_SKIN_SINGLE)
		{
			Mod_FloodFillSkin ((byte *) (pskintype + 1), pheader->skinwidth, pheader->skinheight);//MH: What will happen here is that for single skins, only the first skin in the model will get flood-filled;

			// save 8 bit texels for the player model to remap
			if (loadmodel->modhint == MOD_PLAYER)
			{
				texels = Hunk_AllocName (size, va("%s_@texels", loadmodel->name));
				pheader->texels[i] = texels - (byte *)pheader;
				memcpy (texels, (byte *)(pskintype + 1), size);
			}

			Q_snprintfz (identifier, sizeof(identifier), "%s_%i", basename, i);

			gl_pants = gl_shirt = gl_texnum = fb_texnum = 0;

			Mod_LoadAliasModelTexture (identifier, texture_mode, &gl_texnum, &fb_texnum, &gl_pants, &gl_shirt);

			if (fb_texnum)
				pheader->isLumaSkin[i][0] = pheader->isLumaSkin[i][1] =	pheader->isLumaSkin[i][2] = pheader->isLumaSkin[i][3] = true;

			if (!gl_texnum)
			{
				gl_texnum = GL_LoadTexture (identifier, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), texture_mode, 1);

				if (Img_HasFullbrights((byte *)(pskintype + 1),	pheader->skinwidth * pheader->skinheight))
					fb_texnum = GL_LoadTexture (va("@fb_%s", identifier), pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), texture_mode | TEX_FULLBRIGHT, 1);
			}

			pheader->gl_texturenum[i][0] = pheader->gl_texturenum[i][1] = pheader->gl_texturenum[i][2] = pheader->gl_texturenum[i][3] = gl_texnum;
			pheader->fb_texturenum[i][0] = pheader->fb_texturenum[i][1] = pheader->fb_texturenum[i][2] = pheader->fb_texturenum[i][3] = fb_texnum;
			pheader->gl_texturepants[i][0] = pheader->gl_texturepants[i][1] = pheader->gl_texturepants[i][2] = pheader->gl_texturepants[i][3] = gl_pants;
			pheader->gl_textureshirt[i][0] = pheader->gl_textureshirt[i][1] = pheader->gl_textureshirt[i][2] = pheader->gl_textureshirt[i][3] = gl_shirt;

			pskintype = (daliasskintype_t *)((byte *)(pskintype + 1) + size);
		}
		else 
		{
			// animating skin group. yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (void *)(pinskinintervals + groupskins);

			texels = Hunk_AllocName (size, va("%s_@texels", loadmodel->name));
			pheader->texels[i] = texels - (byte *)pheader;
			memcpy (texels, (byte *)pskintype, size);

			for (j=0 ; j<groupskins ; j++)
			{
				Mod_FloodFillSkin ((byte *) (pskintype), pheader->skinwidth, pheader->skinheight);//MH: FIXED: for group skins it's worse - nothing gets filled and instead it scribbles all over the memory for the group skin info (fortunately after reading the necessary info, otherwise it's a potential crasher) - it may also mess up the texel data.
				Q_snprintfz (identifier, sizeof(identifier), "%s_%i_%i", basename, i, j);

				gl_texnum = fb_texnum = 0;
				Mod_LoadAliasModelTexture (identifier, texture_mode, &gl_texnum, &fb_texnum, &gl_pants, &gl_shirt);
				if (fb_texnum)
					pheader->isLumaSkin[i][j&3] = true;

				if (!gl_texnum)
				{
					gl_texnum = GL_LoadTexture (identifier, pheader->skinwidth, pheader->skinheight, (byte *)pskintype, texture_mode, 1);

					if (Img_HasFullbrights((byte *)pskintype, pheader->skinwidth * pheader->skinheight))
						fb_texnum = GL_LoadTexture (va("@fb_%s", identifier), pheader->skinwidth, pheader->skinheight, (byte *)pskintype, texture_mode | TEX_FULLBRIGHT, 1);
				}

				pheader->gl_texturenum[i][j&3] = gl_texnum;
				pheader->fb_texturenum[i][j&3] = fb_texnum;
				pheader->gl_texturepants[i][j&3] = 0; 
				pheader->gl_textureshirt[i][j&3] = 0;

				pskintype = (daliasskintype_t *)((byte *)pskintype + size);
			}

			for (k=j ; j<4 ; j++)
				pheader->gl_texturenum[i][j&3] = pheader->gl_texturenum[i][j-k];
		}
	}

	return (void *)pskintype;
}

//=========================================================================

//=========================================================================//
//	From Twilight. This nice feature makes all that fuss with strncmp 	   //
//	feel lightyears away :)												   //
//	Many Thx. Reckless:Ohh by the way add as many flags here as you see fit//
//=========================================================================//
typedef struct
{
	char	*name;
	int		len;
	int		hints;
} mhints_t;

mhints_t modelhints[] =
{
	// Regular Quake
	{ "progs/wyvflame.mdl",	0, MOD_FLAME },
	{ "progs/fball.mdl",	0, MOD_FLAME },
	{ "progs/flame",		11,MOD_FLAME },
	{ "progs/fire.mdl",		0, MOD_FLAME },
	{ "progs/torch.mdl",	0, MOD_FLAME },
//	{ "progs/candle.mdl",	0, MOD_FLAME },
	{ "progs/bolt",			10,MOD_THUNDERBOLT},
	{ "progs/spike.mdl",	0, MOD_SPIKE },
	{ "progs/s_spike.mdl",	0, MOD_SPIKE },
	{ "progs/player.mdl",	0, MOD_PLAYER },	
	{ "progs/w_player.mdl",	0, MOD_PLAYER },	
	{ "progs/v_spike.mdl",	0, MOD_SPIKE },
	{ "progs/eyes.mdl",		0, MOD_EYES },
	// Dissolution of Eternity
	{ "progs/lavaball.mdl",	0, MOD_LAVABALL },
	{ "progs/fireball.mdl",		0, MOD_LAVABALL },
	{ "progs/lspike.mdl",		0, MOD_SPIKE },
	// Common
	{ "progs/v_shot.mdl",		0, MOD_WEAPON },
	{ "progs/v_shot2.mdl",		0, MOD_WEAPON },
	{ "progs/v_nail.mdl",		0, MOD_WEAPON },
	{ "progs/v_nail2.mdl",		0, MOD_WEAPON},
	{ "progs/v_rock.mdl",		0, MOD_WEAPON },
	{ "progs/v_rock2.mdl",		0, MOD_WEAPON },
	{ "progs/v_light.mdl",		0, MOD_WEAPON },
	// hipnotic weapons
	{ "progs/v_laserg.mdl",		0, MOD_WEAPON },
	{ "progs/v_prox.mdl",		0, MOD_WEAPON },
	// rogue weapons
	{ "progs/v_grpple.mdl",		0, MOD_WEAPON },
	{ "progs/v_lava.mdl",		0, MOD_WEAPON },
	{ "progs/v_lava2.mdl",		0, MOD_WEAPON },
	{ "progs/v_multi.mdl",		0, MOD_WEAPON },
	{ "progs/v_multi2.mdl",		0, MOD_WEAPON },
	{ "progs/v_plasma.mdl",		0, MOD_WEAPON },
	{ "progs/v_star.mdl",		0, MOD_WEAPON },
	{ "progs/v_axe.mdl",		0, MOD_WEAPON},
	{ "progs/v_",				8, MOD_WEAPON },
	{ "progs/flame_pyre.mdl",	0, MOD_WEAPON },
	// CTF
	{ "progs/flag.mdl",			0, MOD_FLAG },
	// end of list
	{ NULL, 0, MOD_NORMAL }
};

int nummhints = sizeof(modelhints) / sizeof(modelhints[0]) - 1;

int Mod_FindModelHints(char *name)
{
	int	i;

	for (i = 0; i < nummhints; i++)
	{
		if (modelhints[i].len > 0) 
		{
			if (!strncmp(name, modelhints[i].name, modelhints[i].len)) 
			{
				return modelhints[i].hints;
			}
		} 
		else 
		{
			if (!strcmp(name, modelhints[i].name)) 
			{
				return modelhints[i].hints;
			}
		}
	}
	return MOD_NORMAL;
}

/*
=================
Mod_LoadAliasModel -- .mdl model
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i, j, version, numframes, size, start, end, total;
	mdl_t				*pinmodel;
	stvert_t			*pinstverts;
	dtriangle_t			*pintriangles;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;

	start = Hunk_LowMark ();

	pinmodel = (mdl_t *)buffer;

	version = LittleLong (pinmodel->version);

	if (version != ALIAS_VERSION)
	{
		Hunk_FreeToLowMark (start);
		Host_Error ("Mod_LoadAliasModel: %s has wrong version number (%i should be %i)", mod->name, version, ALIAS_VERSION);
		return;
	}
	
	mod->modhint |= Mod_FindModelHints(mod->name);//Reckless

	if (!strcmp (mod->name, "progs/lavaball")) 
	{
		mod->modhint = MOD_LAVABALL;
		mod->flags |= EF_FULLBRIGHT;
	}

// allocate space for a working header, plus all the data except the frames,
// skin and group info
	size = sizeof(aliashdr_t) + (LittleLong (pinmodel->numframes) - 1) * sizeof(pheader->frames[0]);
	pheader = Hunk_AllocName (size, va("%s_@pheader", mod->name));
	
	mod->flags = LittleLong (pinmodel->flags);

	mod->flags &= (0xFF | MF_HOLEY); //only preserve first byte, plus MF_HOLEY

// endian-adjust and copy the data, starting with the alias model header
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);

	pheader->numskins = LittleLong (pinmodel->numskins);

	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	pheader->skinheight = LittleLong (pinmodel->skinheight);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Sys_Error ("Mod_LoadAliasModel: model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);

	pheader->numverts = LittleLong (pinmodel->numverts);

	if (pheader->numverts <= 0)
		Sys_Error ("Mod_LoadAliasModel: model %s has no vertices", mod->name);
	else if (pheader->numverts > MAXALIASVERTS)
		Sys_Error ("Mod_LoadAliasModel: model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong (pinmodel->numtris);

	if (pheader->numtris <= 0)
		Sys_Error ("Mod_LoadAliasModel: model %s has no triangles", mod->name);
	else if (pheader->numtris > MAXALIASTRIS)
		Sys_Error ("Mod_LoadAliasModel: model %s has too many triangles", mod->name);

	pheader->numframes = LittleLong (pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pheader->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}

// load the skins
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_LoadAllSkins (pheader->numskins, pskintype);

// load base s and t vertices
	pinstverts = (stvert_t *)pskintype;

	for (i=0 ; i<pheader->numverts ; i++)
	{
		stverts[i].onseam = LittleLong (pinstverts[i].onseam);
		stverts[i].s = LittleLong (pinstverts[i].s);
		stverts[i].t = LittleLong (pinstverts[i].t);
	}

// load triangle lists
	pintriangles = (dtriangle_t *)&pinstverts[pheader->numverts];

	for (i=0 ; i<pheader->numtris ; i++)
	{
		triangles[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j=0 ; j<3 ; j++)
			triangles[i].vertindex[j] = LittleLong (pintriangles[i].vertindex[j]);
	}

// load the frames
	posenum = 0;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	aliasbboxmins[0] = aliasbboxmins[1] = aliasbboxmins[2] = 99999;
	aliasbboxmaxs[0] = aliasbboxmaxs[1] = aliasbboxmaxs[2] = -99999;

	for (i=0 ; i<numframes ; i++)
	{
		aliasframetype_t	frametype;

		frametype = LittleLong (pframetype->type);

		if (frametype == ALIAS_SINGLE)
			pframetype = (daliasframetype_t *)Mod_LoadAliasFrame (pframetype + 1, &pheader->frames[i]);
		else
			pframetype = (daliasframetype_t *)Mod_LoadAliasGroup (pframetype + 1, &pheader->frames[i]);
	}

	pheader->numposes = posenum;

	mod->type = mod_alias;

	for (i=0 ; i<3 ; i++) 
	{
		mod->mins[i] = aliasbboxmins[i] * pheader->scale[i] + pheader->scale_origin[i];
		mod->maxs[i] = aliasbboxmaxs[i] * pheader->scale[i] + pheader->scale_origin[i];
	}

	mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

// build the draw lists
	GL_MakeAliasModelDisplayLists (mod, pheader);

// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark ();
	total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);

	if (!mod->cache.data)
		return;

	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}

/*
===============================================================================

				Q3 MODELS

===============================================================================
*/
animdata_t anims[NUM_ANIMTYPES];
vec3_t		md3bboxmins, md3bboxmaxs;
void Mod_GetQ3AnimData (char *buf, char *animtype, animdata_t *adata)
{
	int	i, j, data[4];
	char	*token, num[4];

	if ((token = strstr(buf, animtype)))
	{
		while (*token != '\n')
			token--;
		token++;	// so we jump back to the first char
		for (i=0 ; i<4 ; i++)
		{
			memset (num, 0, sizeof(num));
			for (j = 0 ; *token != '\t' ; j++)
				num[j] = *token++;
			data[i] = Q_atoi(num);
			token++;
		}
		adata->offset = data[0];
		adata->num_frames = data[1];
		adata->loop_frames = data[2];
		adata->interval = 1.0 / (float)data[3];
	}
}

void Mod_LoadQ3Animation (void)
{
	int		ofs_legs;
	char		*animdata;
	animdata_t	tmp1, tmp2;

	if (!(animdata = (char *)COM_LoadFile("progs/player/animation.cfg", 0)))
	{
		Con_Printf ("ERROR: Couldn't open animation file\n");
		return;
	}

	memset (anims, 0, sizeof(anims));

	Mod_GetQ3AnimData (animdata, "BOTH_DEATH1", &anims[both_death1]);
	Mod_GetQ3AnimData (animdata, "BOTH_DEATH2", &anims[both_death2]);
	Mod_GetQ3AnimData (animdata, "BOTH_DEATH3", &anims[both_death3]);
	Mod_GetQ3AnimData (animdata, "BOTH_DEAD1", &anims[both_dead1]);
	Mod_GetQ3AnimData (animdata, "BOTH_DEAD2", &anims[both_dead2]);
	Mod_GetQ3AnimData (animdata, "BOTH_DEAD3", &anims[both_dead3]);

	Mod_GetQ3AnimData (animdata, "TORSO_ATTACK", &anims[torso_attack]);
	Mod_GetQ3AnimData (animdata, "TORSO_ATTACK2", &anims[torso_attack2]);
	Mod_GetQ3AnimData (animdata, "TORSO_STAND", &anims[torso_stand]);
	Mod_GetQ3AnimData (animdata, "TORSO_STAND2", &anims[torso_stand2]);

	Mod_GetQ3AnimData (animdata, "TORSO_GESTURE", &tmp1);
	Mod_GetQ3AnimData (animdata, "LEGS_WALKCR", &tmp2);
// we need to subtract the torso-only frames to get the correct indices
	ofs_legs = tmp2.offset - tmp1.offset;
	
	Mod_GetQ3AnimData (animdata, "LEGS_WALK", &anims[legs_walk]);//R00k
	Mod_GetQ3AnimData (animdata, "LEGS_RUN", &anims[legs_run]);
	Mod_GetQ3AnimData (animdata, "LEGS_IDLE", &anims[legs_idle]);
	anims[legs_walk].offset -= ofs_legs;
	anims[legs_run].offset -= ofs_legs;
	anims[legs_idle].offset -= ofs_legs;

	Z_Free (animdata);
}

/*
=================
Mod_LoadQ3ModelTexture
=================
*/
void Mod_LoadQ3ModelTexture (char *identifier, int flags, int *gl_texnum, int *fb_texnum, int *gl_pants, int *gl_shirt)
{
	char	loadpath[64];

	Q_snprintfz (loadpath, sizeof(loadpath), "textures/q3models/%s", identifier);

	*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, flags);

	if (*gl_texnum)
	{
		*gl_pants	= GL_LoadTextureImage (va("%s_pants", loadpath), va("@pants_%s"	, identifier), 0, 0, flags | TEX_BLEND);
		*gl_shirt	= GL_LoadTextureImage (va("%s_shirt", loadpath), va("@shirt_%s"	, identifier), 0, 0, flags | TEX_BLEND);
		*fb_texnum	= GL_LoadTextureImage (va("%s_luma"	, loadpath), va("@fb_%s"	, identifier), 0, 0, flags | TEX_LUMA);
		return;
	}
	else
		Con_DPrintf (1,"Mod_LoadQ3ModelTexture: no such texture %s\n", loadpath);

	if (!*gl_texnum)
	{
		Q_snprintfz (loadpath, sizeof(loadpath), "textures/%s", identifier);
		*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, flags);
		if (*gl_texnum)
		{
			*gl_pants	= GL_LoadTextureImage (va("%s_pants", loadpath), va("@pants_%s", identifier), 0, 0, flags | TEX_BLEND);
			*gl_shirt	= GL_LoadTextureImage (va("%s_shirt", loadpath), va("@shirt_%s", identifier), 0, 0, flags | TEX_BLEND);
			*fb_texnum	= GL_LoadTextureImage (va("%s_luma", loadpath),	 va("@fb_%s", identifier), 0, 0, flags | TEX_LUMA);
		}
		else
			Con_DPrintf (1,"Mod_LoadQ3ModelTexture: no such texture %s\n", loadpath);
	}
	if (!*gl_texnum)
	{
		Q_snprintfz (loadpath, sizeof(loadpath), "progs/%s", identifier);
		*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, flags);
		if (*gl_texnum)
		{
			*gl_pants	= GL_LoadTextureImage (va("%s_pants", loadpath), va("@pants_%s", identifier), 0, 0, flags | TEX_BLEND);
			*gl_shirt	= GL_LoadTextureImage (va("%s_shirt", loadpath), va("@shirt_%s", identifier), 0, 0, flags | TEX_BLEND);
			*fb_texnum	= GL_LoadTextureImage (va("%s_luma", loadpath),	 va("@fb_%s", identifier), 0, 0, flags | TEX_LUMA);
		}
		else
			Con_DPrintf (1,"Mod_LoadQ3ModelTexture: no such texture %s\n", loadpath);
	}
	if (!*gl_texnum)
	{
		Q_snprintfz (loadpath, sizeof(loadpath), "progs/player/%s", identifier);
		*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, flags);
		if (*gl_texnum)
		{
			*gl_pants	= GL_LoadTextureImage (va("%s_pants", loadpath), va("@pants_%s", identifier), 0, 0, flags | TEX_BLEND);
			*gl_shirt	= GL_LoadTextureImage (va("%s_shirt", loadpath), va("@shirt_%s", identifier), 0, 0, flags | TEX_BLEND);
			*fb_texnum	= GL_LoadTextureImage (va("%s_luma", loadpath),	 va("@fb_%s", identifier), 0, 0, flags | TEX_LUMA);
		}
		else
			Con_DPrintf (1,"Mod_LoadQ3ModelTexture: no such texture %s\n", loadpath);
	}
	if (!*gl_texnum)
	{
		Q_snprintfz (loadpath, sizeof(loadpath), "maps/%s", identifier);
		*gl_texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, flags);
		if (*gl_texnum)
		{
			*gl_pants	= GL_LoadTextureImage (va("%s_pants", loadpath), va("@pants_%s", identifier), 0, 0, flags | TEX_BLEND);
			*gl_shirt	= GL_LoadTextureImage (va("%s_shirt", loadpath), va("@shirt_%s", identifier), 0, 0, flags | TEX_BLEND);
			*fb_texnum	= GL_LoadTextureImage (va("%s_luma", loadpath),	 va("@fb_%s", identifier), 0, 0, flags | TEX_LUMA);
		}
		else
			Con_DPrintf (1,"Mod_LoadQ3ModelTexture: no such texture %s\n", loadpath);
	}
}

/*
=================
Mod_LoadAllQ3Skins

supporting only the default skin yet
=================
*/
void Mod_LoadAllQ3Skins (char *modelname, md3header_t *header)
{
	int		i, j, defaultskin, numskinsfound;
	char		skinname[MAX_QPATH], **skinsfound;
	md3surface_t	*surf;
	md3shader_t	*shader;
	searchpath_t	*search;

	i = strrchr (modelname, '/') - modelname;
	Q_strncpyz (skinname, modelname, i+1);

	EraseDirEntries ();

	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (!search->pack)
		{
			RDFlags |= RD_NOERASE;
			ReadDir (va("%s/%s", search->filename, skinname), va("%s*.skin", loadname));
		}
	}

	numskinsfound = num_files;
	skinsfound = (char **)Q_malloc (numskinsfound * sizeof(char *));
	for (i=0 ; i<numskinsfound ; i++)
	{
		skinsfound[i] = Q_malloc (MAX_QPATH);
		Q_snprintfz (skinsfound[i], MAX_QPATH, "%s/%s", skinname, filelist[i].name);
	}
	// It only works this lame way coz if I snprintf to skinname from skinname
	// then linux's vsnprintf clears out skinname first and it's gonna lost
	Q_strncpyz (skinname, va("%s/%s", skinname, loadname), MAX_QPATH);

	defaultskin = -1;
	for (i=0 ; i<numskinsfound ; i++)
	{
		if (!strcmp(skinsfound[i], va("%s_default.skin", skinname)))
		{
			defaultskin = i;
			break;
		}
	}

	// load default skin if exists
	if (defaultskin != -1)
	{
		int	pos;
		char	*token, *skindata;

		skindata = (char *)COM_LoadFile (skinsfound[defaultskin], 0);

		pos = 0;
		while (pos < com_filesize)
		{
			token = &skindata[pos];
			while (skindata[pos] != ',' && skindata[pos])
				pos++;
			skindata[pos++] = '\0';

			surf = (md3surface_t *)((byte *)header + header->ofssurfs);
			for (j = 0 ; j < header->numsurfs && strcmp(surf->name, token) ; j++)
				surf = (md3surface_t *)((byte *)surf + surf->ofsend);

			token = &skindata[pos];
			while (skindata[pos] != '\n' && skindata[pos])
				pos++;
			skindata[pos++-1] = '\0';	// becoz of \r\n

			if (token[0] && j < header->numsurfs)
			{
				shader = (md3shader_t *)((byte *)surf + surf->ofsshaders);
				for (j = 0 ; j < surf->numshaders ; j++, shader++)
					Q_strncpyz (shader->name, token, MAX_QPATH);
			}
		}

		Z_Free (skindata);
	}

	for (i=0 ; i<numskinsfound ; i++)
		Q_free (skinsfound[i]);
	Q_free (skinsfound);
}

/*
=================
Mod_LoadQ3Model
=================
*/
void Mod_LoadQ3Model (model_t *mod, void *buffer)
{
	int				i, j, size, base, texture_mode, version, gl_texnum, fb_texnum, gl_pants, gl_shirt;
	char			basename[MAX_QPATH];
	float			radiusmax;
	md3header_t		*header;
	md3frame_t		*frame;
	md3tag_t		*tag;
	md3surface_t	*surf;
	md3shader_t		*shader;
	md3triangle_t	*tris;
	md3tc_t			*tc;
	md3vert_t		*vert;
	char			md3name[128];//R00k

	COM_StripExtension(mod->name, &md3name[0]);

	if(!strcmp (md3name, "progs/g_shot") 
	|| !strcmp (md3name, "progs/g_nail") 
	|| !strcmp (md3name, "progs/g_nail2") 
	|| !strcmp (md3name, "progs/g_rock") 
	|| !strcmp (md3name, "progs/g_rock2")
	|| !strcmp (md3name, "progs/g_light") 
	|| !strcmp (md3name, "progs/armor") 
	|| !strcmp (md3name, "progs/backpack") 
	|| !strcmp (md3name, "progs/w_g_key") 
	|| !strcmp (md3name, "progs/w_s_key") 
	|| !strcmp (md3name, "progs/m_g_key") 
	|| !strcmp (md3name, "progs/m_s_key") 
	|| !strcmp (md3name, "progs/b_g_key") 
	|| !strcmp (md3name, "progs/b_s_key") 
	|| !strcmp (md3name, "progs/quaddama") 
	|| !strcmp (md3name, "progs/invisibl") 
	|| !strcmp (md3name, "progs/invulner") 
	|| !strcmp (md3name, "progs/jetpack") 
	|| !strcmp (md3name, "progs/cube") 
	|| !strcmp (md3name, "progs/suit") 
	|| !strcmp (md3name, "progs/boots") 
	|| !strcmp (md3name, "progs/end1") 
	|| !strcmp (md3name, "progs/end2") 
	|| !strcmp (md3name, "progs/end3") 
	|| !strcmp (md3name, "progs/end4")
	|| !strcmp (md3name, "progs/g_hammer")
	|| !strcmp (md3name, "progs/g_laserg")
	|| !strcmp (md3name, "progs/g_prox"))
	{
		mod->flags |= EF_ROTATE; 
	} 
	else
		if (!strcmp (md3name, "progs/missile")) 
	{
		mod->flags |= EF_ROCKET;
	}
	else
	if (!strcmp (md3name, "progs/gib1") || //EF_GIB
		!strcmp (md3name, "progs/gib2") || 
		!strcmp (md3name, "progs/gib3") || 
		!strcmp (md3name, "progs/h_player") || 
		!strcmp (md3name, "progs/h_dog") || 
		!strcmp (md3name, "progs/h_mega") || 
		!strcmp (md3name, "progs/h_guard") || 
		!strcmp (md3name, "progs/h_wizard") || 
		!strcmp (md3name, "progs/h_knight") || 
		!strcmp (md3name, "progs/h_hellkn") || 
		!strcmp (md3name, "progs/h_zombie") || 
		!strcmp (md3name, "progs/h_shams") || 
		!strcmp (md3name, "progs/h_shal") || 
		!strcmp (md3name, "progs/h_ogre") ||
		!strcmp (md3name, "progs/armor") ||
		!strcmp (md3name, "progs/h_demon")) 
	{
		mod->flags |= EF_GIB;
	} 
	else 
	if (!strcmp (md3name, "progs/grenade")) 
	{
		mod->flags |= EF_GRENADE;
	} 
	else 
	if (!strcmp (md3name, "progs/w_spike")) 
	{
		mod->flags |= EF_TRACER;
		mod->modhint |= MOD_SPIKE;
	} 
	else 
	if (!strcmp (md3name, "progs/k_spike")) 
	{
		mod->flags |= EF_TRACER2;
		mod->modhint |= MOD_SPIKE;
	} 
	else 
	if (!strcmp (md3name, "progs/v_spike")) 
	{
		mod->flags |= EF_TRACER3;
		mod->modhint |= MOD_SPIKE;
	} 
	else 
	if (!strcmp (md3name, "progs/zom_gib")) 
	{
		mod->flags |= EF_ZOMGIB;
	}
	else
	if (!strcmp(md3name, "progs/v_shot")	||
		!strcmp(md3name, "progs/v_shot2")	||
		!strcmp(md3name, "progs/v_nail")	||
		!strcmp(md3name, "progs/v_nail2")	||
		!strcmp(md3name, "progs/v_rock")	||
		!strcmp(md3name, "progs/v_rock2"))
	{
		mod->modhint = MOD_WEAPON;
	}
	else 
	if (!strcmp (md3name, "progs/lavaball")) 
	{
		mod->modhint = MOD_LAVABALL;
		mod->flags |= EF_FULLBRIGHT;
	}
	else 
	if (!strcmp (md3name, "progs/player"))
		mod->modhint = MOD_PLAYER;
	else
	if (((!strcmp (md3name, "progs/fball")) ||
		(!strcmp (md3name, "progs/flame"))	||
		(!strcmp (md3name, "progs/flame0"))	||
		(!strcmp (md3name, "progs/flame2"))	||
		(!strcmp (md3name, "progs/torch"))))
	{
		mod->modhint = MOD_FLAME;
		mod->flags |= EF_FULLBRIGHT;
	}

	header = (md3header_t *)buffer;

	version = LittleLong (header->version);
	if (version != MD3_VERSION)
		Host_Error ("Mod_LoadQ3Model: %s has wrong version number (%i should be %i)", md3name, version, MD3_VERSION);

// endian-adjust all data
	header->numframes = LittleLong (header->numframes);
	if (header->numframes < 1)
		Sys_Error ("Mod_LoadQ3Model: model %s has no frames", md3name);
	else if (header->numframes > MAXMD3FRAMES)
		Sys_Error ("Mod_LoadQ3Model: model %s has too many frames", md3name);

	header->numtags = LittleLong (header->numtags);
	if (header->numtags > MAXMD3TAGS)
		Sys_Error ("Mod_LoadQ3Model: model %s has too many tags", md3name);

	header->numsurfs = LittleLong (header->numsurfs);
	if (header->numsurfs < 1)
		Sys_Error ("Mod_LoadQ3Model: model %s has no surfaces", md3name);
	else if (header->numsurfs > MAXMD3SURFS)
		Sys_Error ("Mod_LoadQ3Model: model %s has too many surfaces", md3name);

	header->numskins = LittleLong (header->numskins);
	header->ofsframes = LittleLong (header->ofsframes);
	header->ofstags = LittleLong (header->ofstags);
	header->ofssurfs = LittleLong (header->ofssurfs);
	header->ofsend = LittleLong (header->ofsend);

	// swap all the frames
	frame = (md3frame_t *)((byte *)header + header->ofsframes);
	for (i=0 ; i<header->numframes ; i++)
	{
		frame[i].radius = LittleFloat (frame->radius);
		for (j=0 ; j<3 ; j++)
		{
			frame[i].mins[j] = LittleFloat (frame[i].mins[j]);
			frame[i].maxs[j] = LittleFloat (frame[i].maxs[j]);
			frame[i].pos[j] = LittleFloat (frame[i].pos[j]);
		}
	}

	// swap all the tags
	tag = (md3tag_t *)((byte *)header + header->ofstags);
	for (i=0 ; i<header->numtags ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			tag[i].pos[j] = LittleFloat (tag[i].pos[j]);
			tag[i].rot[0][j] = LittleFloat (tag[i].rot[0][j]);
			tag[i].rot[1][j] = LittleFloat (tag[i].rot[1][j]);
			tag[i].rot[2][j] = LittleFloat (tag[i].rot[2][j]);
		}
	}

	// swap all the surfaces
	surf = (md3surface_t *)((byte *)header + header->ofssurfs);
	for (i=0 ; i<header->numsurfs ; i++)
	{
		surf->ident = LittleLong (surf->ident);
		surf->flags = LittleLong (surf->flags);
		surf->numframes = LittleLong (surf->numframes);
		if (surf->numframes != header->numframes)
			Sys_Error ("Mod_LoadQ3Model: number of frames don't match in %s", md3name);

		surf->numshaders = LittleLong (surf->numshaders);
		if (surf->numshaders <= 0)
			Sys_Error ("Mod_LoadQ3Model: model %s has no shaders", md3name);
		else if (surf->numshaders > MAXMD3SHADERS)
			Sys_Error ("Mod_LoadQ3Model: model %s has too many shaders", md3name);

		surf->numverts = LittleLong (surf->numverts);
		if (surf->numverts <= 0)
			Sys_Error ("Mod_LoadQ3Model: model %s has no vertices", md3name);
		else if (surf->numverts > MAXMD3VERTS)
			Sys_Error ("Mod_LoadQ3Model: model %s has too many vertices", md3name);

		surf->numtris = LittleLong (surf->numtris);
		if (surf->numtris <= 0)
			Sys_Error ("Mod_LoadQ3Model: model %s has no triangles", md3name);
		else if (surf->numtris > MAXMD3TRIS)
			Sys_Error ("Mod_LoadQ3Model: model %s has too many triangles", md3name);

		surf->ofstris = LittleLong (surf->ofstris);
		surf->ofsshaders = LittleLong (surf->ofsshaders);
		surf->ofstc = LittleLong (surf->ofstc);
		surf->ofsverts = LittleLong (surf->ofsverts);
		surf->ofsend = LittleLong (surf->ofsend);
	
		// swap all the shaders
		shader = (md3shader_t *)((byte *)surf + surf->ofsshaders);
		for (j=0 ; j<surf->numshaders ; j++)
			shader[j].index = LittleLong (shader[j].index);

		// swap all the triangles
		tris = (md3triangle_t *)((byte *)surf + surf->ofstris);
		for (j=0 ; j<surf->numtris ; j++)
		{
			tris[j].indexes[0] = LittleLong (tris[j].indexes[0]);
			tris[j].indexes[1] = LittleLong (tris[j].indexes[1]);
			tris[j].indexes[2] = LittleLong (tris[j].indexes[2]);
		}

		// swap all the texture coords
		tc = (md3tc_t *)((byte *)surf + surf->ofstc);
		for (j=0 ; j<surf->numverts ; j++)
		{
			tc[j].s = LittleFloat (tc[j].s);
			tc[j].t = LittleFloat (tc[j].t);
		}

		// swap all the vertices
		vert = (md3vert_t *)((byte *)surf + surf->ofsverts);
		for (j=0 ; j < surf->numverts * surf->numframes ; j++)
		{
			vert[j].vec[0] = LittleShort (vert[j].vec[0]);
			vert[j].vec[1] = LittleShort (vert[j].vec[1]);
			vert[j].vec[2] = LittleShort (vert[j].vec[2]);
			vert[j].normal = LittleShort (vert[j].normal);
		}

		// find the next surface
		surf = (md3surface_t *)((byte *)surf + surf->ofsend);
	}

// allocate extra size for structures different in memory
	surf = (md3surface_t *)((byte *)header + header->ofssurfs);
	for (size = 0, i = 0 ; i < header->numsurfs ; i++)
	{
		size += surf->numshaders * sizeof(md3shader_mem_t);		  // shader containing texnum
		size += surf->numverts * surf->numframes * sizeof(md3vert_mem_t); // floating point vertices
		surf = (md3surface_t *)((byte *)surf + surf->ofsend);
	}

	header = Cache_Alloc (&mod->cache, com_filesize + size, loadname);
	if (!mod->cache.data)
		return;

	memcpy (header, buffer, com_filesize);
	base = com_filesize;

	mod->type = mod_md3;
	mod->numframes = header->numframes;

	md3bboxmins[0] = md3bboxmins[1] = md3bboxmins[2] = 99999;
	md3bboxmaxs[0] = md3bboxmaxs[1] = md3bboxmaxs[2] = -99999;
	radiusmax = 0;

	frame = (md3frame_t *)((byte *)header + header->ofsframes);
	for (i=0 ; i<header->numframes ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			md3bboxmins[j] = min(md3bboxmins[j], frame[i].mins[j]);
			md3bboxmaxs[j] = max(md3bboxmaxs[j], frame[i].maxs[j]);
		}
		radiusmax = max(radiusmax, frame[i].radius);
	}
	VectorCopy (md3bboxmins, mod->mins);
	VectorCopy (md3bboxmaxs, mod->maxs);
	mod->radius = radiusmax;

// load the skins

	Mod_LoadAllQ3Skins (mod->name, header);

// load the animation frames if loading the player model
	if (!strcmp(mod->name, "progs/player/lower.md3"))
		Mod_LoadQ3Animation ();

	texture_mode = TEX_MIPMAP;

	surf = (md3surface_t *)((byte *)header + header->ofssurfs);

	for (i=0 ; i<header->numsurfs; i++)
	{
		shader = (md3shader_t *)((byte *)surf + surf->ofsshaders);
		surf->ofsshaders = base;
		size = surf->numshaders;
		for (j=0 ; j<size ; j++)
		{
			md3shader_mem_t	*memshader = (md3shader_mem_t *)((byte *)header + surf->ofsshaders);

			Q_strncpyz (memshader[j].name, shader->name, sizeof(memshader[j].name));
			memshader[j].index = shader->index;

			COM_StripExtension (COM_SkipPath(shader->name), basename);
			
			Con_DPrintf (1,"Looking for skin %s, %s\n", shader->name, basename);

			gl_texnum = fb_texnum = gl_pants = gl_shirt = 0;
			
			Mod_LoadQ3ModelTexture (basename, texture_mode, &gl_texnum, &fb_texnum, &gl_pants, &gl_shirt);

			memshader[j].gl_texnum = gl_texnum;
			memshader[j].fb_texnum = fb_texnum;
			memshader[j].pt_texnum = gl_pants;
			memshader[j].st_texnum = gl_shirt;

			shader++;
		}
		base += size * sizeof(md3shader_mem_t);

		vert = (md3vert_t *)((byte *)surf + surf->ofsverts);
		surf->ofsverts = base;
		size = surf->numverts * surf->numframes;
		for (j=0 ; j<size ; j++)
		{
			float		lat, lng;
			vec3_t		ang;
			md3vert_mem_t	*vertexes = (md3vert_mem_t *)((byte *)header + surf->ofsverts);

			vertexes[j].oldnormal = vert->normal;

			vertexes[j].vec[0] = (float)vert->vec[0] * MD3_XYZ_SCALE;
			vertexes[j].vec[1] = (float)vert->vec[1] * MD3_XYZ_SCALE;
			vertexes[j].vec[2] = (float)vert->vec[2] * MD3_XYZ_SCALE;

			lat = ((vert->normal >> 8) & 0xff) * M_PI / 128.0f;
			lng = (vert->normal & 0xff) * M_PI / 128.0f;
			vertexes[j].normal[0] = cos(lat) * sin(lng);
			vertexes[j].normal[1] = sin(lat) * sin(lng);
			vertexes[j].normal[2] = cos(lng);

			vectoangles (vertexes[j].normal, ang);
			vertexes[j].anorm_pitch = ang[0] * 256 / 360;
			vertexes[j].anorm_yaw = ang[1] * 256 / 360;

			vert++;
		}
		base += size * sizeof(md3vert_mem_t);

		surf = (md3surface_t *)((byte *)surf + surf->ofsend);
	}
}
//=============================================================================

static	int	spr_version;

/*
=================
Mod_LoadSpriteModelTexture
=================
*/
void Mod_LoadSpriteModelTexture (char *sprite_name, dspriteframe_t *pinframe, mspriteframe_t *pspriteframe, int framenum, int width, int height, int texture_mode)
{
	char	name[64], sprite[64], sprite2[64];

	COM_StripExtension (sprite_name, sprite);
	Q_snprintfz (name, sizeof(name), "%s_%i", sprite_name, framenum);
	Q_snprintfz (sprite2, sizeof(sprite2), "textures/sprites/%s_%i", COM_SkipPath(sprite), framenum);
	pspriteframe->gl_texturenum = GL_LoadTextureImage (sprite2, name, 0, 0, texture_mode);

	if (pspriteframe->gl_texturenum == 0)
	{
		sprintf (sprite2, "textures/%s_%i", COM_SkipPath(sprite), framenum);
		pspriteframe->gl_texturenum = GL_LoadTextureImage (sprite2, name, 0, 0, texture_mode);
	}

	if (pspriteframe->gl_texturenum == 0)
	{
		if (spr_version == SPRITE32_VERSION)
			pspriteframe->gl_texturenum = GL_LoadTexture (name, width, height, (byte *)(pinframe + 1), texture_mode, 4);
		else
			pspriteframe->gl_texturenum = GL_LoadTexture (name, width, height, (byte *)(pinframe + 1), texture_mode, 1);
	}
}

/*
=================
Mod_LoadSpriteFrame
=================
*/
void *Mod_LoadSpriteFrame (void *pin, mspriteframe_t **ppframe, int framenum)
{
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	int			width, height, size, origin[2], texture_mode;

	texture_mode = TEX_ALPHA;

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);

	size = (spr_version == SPRITE32_VERSION) ? width * height * 4 : width * height;

	pspriteframe = Hunk_AllocName (sizeof(mspriteframe_t), va("%s_@pheader", loadmodel->name));

	memset (pspriteframe, 0, sizeof(mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	Mod_LoadSpriteModelTexture (loadmodel->name, pinframe, pspriteframe, framenum, width, height, texture_mode);

	return (void *)((byte *)pinframe + sizeof(dspriteframe_t) + size);
}

/*
=================
Mod_LoadSpriteGroup
=================
*/
void *Mod_LoadSpriteGroup (void *pin, mspriteframe_t **ppframe, int framenum)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int				i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Hunk_AllocName (sizeof(mspritegroup_t) + (numframes - 1) * sizeof(pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof(float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error ("Mod_LoadSpriteGroup: interval <= 0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i);

	return ptemp;
}

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i, numframes, size;
	dsprite_t			*pin;
	msprite_t			*psprite;
	dspriteframetype_t	*pframetype;
	
	pin = (dsprite_t *)buffer;

	spr_version = LittleLong (pin->version);

	if (spr_version != SPRITE_VERSION && spr_version != SPRITE32_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i or %i)", mod->name, spr_version, SPRITE_VERSION, SPRITE32_VERSION);

	if (nospr32.value && spr_version == SPRITE32_VERSION)
		return;

	// joe: indicating whether it's a 32bit sprite or not
	mod->modhint = (spr_version == SPRITE32_VERSION) ? MOD_SPR32 : MOD_SPR;

	numframes = LittleLong (pin->numframes);

	size = sizeof(msprite_t) + (numframes - 1) * sizeof(psprite->frames);

	psprite = Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;
	
// load the frames
	if (numframes < 1)
		Sys_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
			pframetype = (dspriteframetype_t *)Mod_LoadSpriteFrame (pframetype + 1, &psprite->frames[i].frameptr, i);
		else
			pframetype = (dspriteframetype_t *)Mod_LoadSpriteGroup (pframetype + 1, &psprite->frames[i].frameptr, i);
	}

	mod->type = (spr_version == SPRITE32_VERSION) ? mod_spr32 : mod_sprite;
}

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print (void)
{
	int		i;
	model_t		*mod;

	Con_Printf ("Cached models:\n");
	for (i = 0, mod = mod_known ; i < mod_numknown ; i++, mod++)
		Con_Printf ("%8p : %s\n", mod->cache.data, mod->name);
}
