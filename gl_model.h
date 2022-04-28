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

#ifndef __MODEL__
#define __MODEL__

#include "modelgen.h"
#include "spritegn.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/
 
#define ISTURBTEX(x) ((x)[0] == '*' || (x)[0] == '!')
#define ISSKYTEX(x) (((x)[0] == 's' && (x)[1] == 'k' && (x)[2] == 'y')||((x)[0] == 'S' && (x)[1] == 'K' && (x)[2] == 'Y'))
#define ISALPHATEX(x) ((x)[0] == '{')

// hacky flags to get things my way Reckless "dont read":
#define ISLAVATEX(x) ((x)[1] == 'l' && \
(x)[2] == 'a' && \
(x)[3] == 'v' && \
(x)[4] == 'a') || \
((x)[1] == 'b' && \
(x)[2] == 'r' && \
(x)[3] == 'i' && \
(x)[4] == 'm')

#define ISSLIMETEX(x) ((x)[1] == 's' && \
(x)[2] == 'l' && \
(x)[3] == 'i' && \
(x)[4] == 'm' && \
(x)[5] == 'e')

#define ISTELETEX(x) ((x)[1] == 't' && \
(x)[2] == 'e' && \
(x)[3] == 'l' && \
(x)[4] == 'e') || \
((x)[1] == 'r' && \
(x)[2] == 'i' && \
(x)[3] == 'f' && \
(x)[4] == 't') || \
((x)[1] == 'g' && \
(x)[2] == 'a' && \
(x)[3] == 't' && \
(x)[4] == 'e')
// end off hacks "you where warned :)" 

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


// in memory representation
// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vec3_t		position;
} mvertex_t;

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2


// plane_t structure
// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct mplane_s
{
	vec3_t		normal;
	float		dist;
	byte		type;		// for texture axis selection and fast side tests
	byte		signbits;	// signx + signy<<1 + signz<<1
	byte		pad[2];
} mplane_t;

typedef struct gltexture_s
{
	int			texnum;

	byte		*data;
	char		identifier[MAX_QPATH];
	char		*pathname;
	int			width, height;
	int			scaled_width, scaled_height;
	int			texmode;
	unsigned short crc;
	int			bpp;	
} gltexture_t;

typedef struct texture_s
{
	char		name[16];
	unsigned	width, height;
	struct		gltexture_s *gl_texture;
	int			gl_texturenum;
	int			fb_texturenum;					// index of fullbright mask or 0
	struct		msurface_s *texturechain[2];
	struct		msurface_s **texturechain_tail[2];
	int			anim_total;						// total tenths in sequence (0 = no)
	int			anim_min, anim_max;				// time for this frame min <=time< max
	struct		texture_s *anim_next;			// in the animation sequence
	struct		texture_s *alternate_anims;		// bmodels in frame 1 use these
	unsigned	offsets[MIPLEVELS];				// four mip maps stored
	int			isLumaTexture;
} texture_t;

#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
#define SURF_UNDERWATER		0x80
#define SURF_MIRROR			0x100
#define SURF_METAL			0x200
#define SURF_DETAIL			0x400
#define SURF_DRAWALPHA		0x800
#define SURF_DRAWWATER		0x1000
#define SURF_DRAWLAVA		0x2000
#define SURF_DRAWSLIME		0x4000
#define SURF_DRAWTELE		0x8000

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	unsigned int	v[2];//bsp2 
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct
{
	float		vecs[2][4];
	texture_t	*texture;
	int			flags;
} mtexinfo_t;

#define	VERTEXSIZE	9

typedef struct modelMatrix_s
{
	float				x1[3];
	float				x2[3];
	float				x3[3];
	float				o[3];
} modelMatrix_t;

typedef struct modelTan_s
{
	int					numNormals;
	modelMatrix_t		tangentMatrix;
} modelTan_t;

typedef struct glpoly_s
{
	struct glpoly_s	*next;
	struct glpoly_s	*chain;
	struct glpoly_s	*fb_chain;
	struct glpoly_s *luma_chain;		// next luma poly in chain
	struct glpoly_s	*caustics_chain;	// next caustic poly in chain
	struct glpoly_s	*detail_chain;		// next detail poly in chain
	struct glpoly_s	*outline_chain;		// next outline poly in chain
	struct glpoly_s	*chrome_chain;		// next shiny poly in chain
	struct glpoly_s	*glass_chain;		// next shiny glass poly in chain

	int		numverts;
	vec3_t	midpoint;//MHQuake
	float	fxofs;	//MHQuake
	float	verts[4][VERTEXSIZE];	// variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct msurface_s
{
	int					visframe;	// should be drawn when node is crossed

	mplane_t			*plane;
	int					flags;
	int					sflags;
	int					firstedge;	// look up in model->surfedges[], negative numbers
	int					numedges;	// are backwards edges
	
	short				texturemins[2];
	short				extents[2];

	int					light_s, light_t;	// gl lightmap coordinates

	glpoly_t			*polys;			// multiple if warped
	struct	msurface_s	*texturechain;
	mtexinfo_t			*texinfo;

	// lighting info
	int					dlightframe;
	int					dlightbits[(MAX_DLIGHTS + 31) >> 5];
	int					lightmaptexturenum;
	byte				styles[MAXLIGHTMAPS];
	int					cached_light[MAXLIGHTMAPS];	// values currently used in lightmap
	qboolean			cached_dlight;			// true if dynamic light in cache
	byte				*samples;		// [numstyles*surfsize]
	qboolean			overbright;
} msurface_t;

typedef struct mnode_s
{
// common with leaf
	int		contents;		// 0, to differentiate from leafs
	int		visframe;		// node needs to be traversed if current
	
	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// node specific
	mplane_t	*plane;
	struct mnode_s	*children[2];	

	unsigned short	firstsurface;
	unsigned short	numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
// common with node
	int		contents;		// wil be a negative contents number
	int		visframe;		// node needs to be traversed if current

	float	minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// leaf specific
	byte		*compressed_vis;
	efrag_t		*efrags;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
	byte		ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

//johnfitz -- for clipnodes>32k
typedef struct mclipnode_s
{
	int			planenum;
	int			children[2]; // negative numbers are contents
} mclipnode_t;
//johnfitz

// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct
{
	mclipnode_t	*clipnodes; // johnfitz -- was dclipnode_t
	mplane_t	*planes;
	int		firstclipnode;
	int		lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
} hull_t;

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/


// FIXME: shorten these?
typedef struct mspriteframe_s
{
	int		width;
	int		height;
	float	up, down, left, right;
	int		gl_texturenum;
} mspriteframe_t;

typedef struct
{
	int		numframes;
	float		*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct
{
	spriteframetype_t	type;
	mspriteframe_t		*frameptr;
} mspriteframedesc_t;

typedef struct
{
	int			type;
	int			maxwidth;
	int			maxheight;
	int			numframes;
	float			beamlength;		// remove?
	void			*cachespot;		// remove?
	mspriteframedesc_t	frames[1];
} msprite_t;


/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

typedef struct
{
	int			firstpose;
	int			numposes;
	float		interval;
	trivertx_t	bboxmin;
	trivertx_t	bboxmax;
	int			frame;
	char		name[16];
} maliasframedesc_t;

typedef struct
{
	trivertx_t		bboxmin;
	trivertx_t		bboxmax;
	int			frame;
} maliasgroupframedesc_t;

typedef struct
{
	int			numframes;
	int			intervals;
	maliasgroupframedesc_t	frames[1];
} maliasgroup_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct mtriangle_s 
{
	int			facesfront;
	int			vertindex[3];
	int			neighbors[3];//GLQuake stencil shadows : Rich Whitehouse
} mtriangle_t;


#define	MAX_SKINS	32
typedef struct 
{
	int			ident;
	int			version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int			numskins;
	int			skinwidth;
	int			skinheight;
	int			numverts;
	int			numtris;
	int			numframes;
	synctype_t	synctype;
	int			flags;
	float		size;	

	int			numposes;
	int			poseverts;
	int			posedata;		// numposes*poseverts trivert_t

								//GLQuake stencil shadows : Rich Whitehouse
	int			baseposedata;	//original verts for triangles to reference
	int			triangles;		//we need tri data for shadow volumes

	int			commands;		// gl command list with embedded s/t
	int			gl_texturenum[MAX_SKINS][4];
	int			fb_texturenum[MAX_SKINS][4];
	int			gl_texturepants[MAX_SKINS][4];
	int			gl_textureshirt[MAX_SKINS][4];
	qboolean	isLumaSkin[MAX_SKINS][4];
	int			texels[MAX_SKINS];	// only for player skins
	maliasframedesc_t	frames[1];		// variable sized
} aliashdr_t;

#define	MAXALIASVERTS	4096	//R00k -- was 1024
#define	MAXALIASFRAMES	256
#define	MAXALIASTRIS	8192	

extern	aliashdr_t	*pheader;
extern	stvert_t	stverts[MAXALIASVERTS];
extern	mtriangle_t	triangles[MAXALIASTRIS];
extern	trivertx_t	*poseverts[MAXALIASFRAMES];


/*
==============================================================================

				Q3 MODELS

==============================================================================
*/

typedef struct
{
	int		ident;
	int		version;
	char		name[MAX_QPATH];
	int      	flags;
	int		numframes;
	int		numtags;
	int		numsurfs;
	int		numskins;
	int		ofsframes;
	int		ofstags;
	int		ofssurfs;
	int		ofsend;
} md3header_t;

typedef struct
{
	vec3_t		mins, maxs;
	vec3_t		pos;
	float		radius;
	char		name[16];
} md3frame_t;

typedef struct
{
	char		name[MAX_QPATH];
	vec3_t		pos;
	vec3_t		rot[3];
} md3tag_t;

typedef struct
{
	int		ident;
	char		name[MAX_QPATH];
	int		flags;
	int		numframes;
	int		numshaders;
	int		numverts;
	int		numtris;
	int		ofstris;
	int		ofsshaders;
	int		ofstc;
	int		ofsverts;
	int		ofsend;
} md3surface_t;

typedef struct
{
	char		name[MAX_QPATH];
	int		index;
} md3shader_t;

typedef struct
{
	char		name[MAX_QPATH];
	int		index;
	int		gl_texnum, fb_texnum, pt_texnum, st_texnum;
} md3shader_mem_t;

typedef struct
{
	int		indexes[3];
} md3triangle_t;

typedef struct
{
	float		s, t;
} md3tc_t;

typedef struct
{
	short		vec[3];
	unsigned short	normal;
} md3vert_t;

typedef struct
{
	vec3_t		vec;
	vec3_t		normal;
	byte		anorm_pitch, anorm_yaw;
	unsigned short	oldnormal;	// needed for normal lighting
} md3vert_mem_t;

#define	MD3_XYZ_SCALE	(1.0 / 64)
/*
    * Only faces and verticies in a group get saved.
    * The MD3_PATH+file name cannot be longer than 63 characters.  (Textures and model names)
    * The group name and point name cannot be longer than 63 characters.
    * Maximum of 1024 animation frames
    * Maximum of 16 Points (Quake 3 Tags)
    * Maximum of 32 Groups
    * Maximum of 256 Textures per Group
    * Maximum of 8192 Triangles per Group
    * Maximum of 4096 Verticies per Group
*/
#define	MAXMD3FRAMES	1024
#define	MAXMD3TAGS		16
#define	MAXMD3SURFS		32
#define	MAXMD3SHADERS	256
#define	MAXMD3VERTS		4096
#define	MAXMD3TRIS		8192

typedef struct animdata_s
{
	int		offset;
	int		num_frames;
	int		loop_frames;
	float	interval;
} animdata_t;

typedef enum animtype_s
{
	both_death1, both_death2, both_death3, both_dead1, both_dead2, both_dead3,
	torso_attack, torso_attack2, torso_stand, torso_stand2,
	legs_walk,legs_run, legs_idle,
	NUM_ANIMTYPES
} animtype_t;

extern	animdata_t	anims[NUM_ANIMTYPES];
/*
#ifndef __IQM_H__
#define __IQM_H__

#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION 0

struct iqmheader
{
    char magic[16];
    unsigned int version;
    unsigned int filesize;
    unsigned int flags;
    unsigned int num_text, ofs_text;
    unsigned int num_meshes, ofs_meshes;
    unsigned int num_vertexarrays, num_vertexes, ofs_vertexarrays;
    unsigned int num_triangles, ofs_triangles, ofs_adjacency;
    unsigned int num_joints, ofs_joints;
    unsigned int num_poses, ofs_poses;
    unsigned int num_anims, ofs_anims;
    unsigned int num_frames, num_framechannels, ofs_frames, ofs_bounds;
    unsigned int num_comment, ofs_comment;
    unsigned int num_extensions, ofs_extensions;
};

struct iqmmesh
{
    unsigned int name;
    unsigned int material;
    unsigned int first_vertex, num_vertexes;
    unsigned int first_triangle, num_triangles;
};

enum
{
    IQM_POSITION     = 0,
    IQM_TEXCOORD     = 1,
    IQM_NORMAL       = 2,
    IQM_TANGENT      = 3,
    IQM_BLENDINDEXES = 4,
    IQM_BLENDWEIGHTS = 5,
    IQM_COLOR        = 6,
    IQM_CUSTOM       = 0x10
};

enum
{
    IQM_BYTE   = 0,
    IQM_UBYTE  = 1,
    IQM_SHORT  = 2,
    IQM_USHORT = 3,
    IQM_INT    = 4,
    IQM_UINT   = 5,
    IQM_HALF   = 6,
    IQM_FLOAT  = 7,
    IQM_DOUBLE = 8,
};

struct iqmtriangle
{
    unsigned int vertex[3];
};

struct iqmjoint
{
    unsigned int name;
    int parent;
    float translate[3], rotate[3], scale[3];
};

struct iqmpose
{
    int parent;
    unsigned int mask;
    float channeloffset[9];
    float channelscale[9];
};

struct iqmanim
{
    unsigned int name;
    unsigned int first_frame, num_frames;
    float framerate;
    unsigned int flags;
};

enum
{
    IQM_LOOP = 1<<0
};

struct iqmvertexarray
{
    unsigned int type;
    unsigned int flags;
    unsigned int format;
    unsigned int size;
    unsigned int offset;
};

struct iqmbounds
{
    float bbmin[3], bbmax[3];
    float xyradius, radius;
};

#endif
*/

// Whole model
typedef enum 
{
	mod_brush, 
	mod_sprite, 
	mod_spr32,
	mod_alias, 
	mod_md2,
	mod_md3
	//mod_iqm
} modtype_t;

// some models are special
typedef enum 
{
	MOD_NORMAL, 
	MOD_PLAYER, 
	MOD_EYES, 
	MOD_FLAME, 
	MOD_THUNDERBOLT, 
	MOD_WEAPON, 
	MOD_LAVABALL, 
	MOD_SPIKE, 
	MOD_FLAG,
	MOD_SHAMBLER, 
	MOD_SPR, 
	MOD_SPR32,
	MOD_ALPHA,
	MOD_NOLERP,//johnfitz -- extra flags for rendering
	MOD_NOSHADOW,
} modhint_t;

#define	EF_ROCKET	1			// leave a trail
#define	EF_GRENADE	2			// leave a trail
#define	EF_GIB		4			// leave a trail
#define	EF_ROTATE	8			// rotate (bonus items)
#define	EF_TRACER	16			// green split trail
#define	EF_ZOMGIB	32			// small blood trail
#define	EF_TRACER2	64			// orange split trail + rotate
#define	EF_TRACER3	128			// purple trail
//#define EF_NOCOLORMAP 256
#define EF_FULLBRIGHT 512		// R00k: flames, lavaball, lg bolt etc render in fullbright (also no shading)// need to change to Entity Flag not Model Flag...

#define	MF_HOLEY	(1u<<14)	// MarkV/QSS -- make index 255 transparent on mdl's

typedef struct model_s
{
	char		name[MAX_QPATH];
	qboolean	needload;	// bmodels and sprites don't cache normally

	modhint_t	modhint;	// by joe

	modtype_t	type;

	int			numframes;
	
	//int		numskins; R00k : Why dont we have this here?

	int			mflags;
	synctype_t	synctype;
	
	int			flags;

// volume occupied by the model graphics
	vec3_t		mins, maxs;
	vec3_t		ymins, ymaxs; //johnfitz -- bounds for entities with nonzero yaw
	vec3_t		rmins, rmaxs; //johnfitz -- bounds for entities with nonzero pitch or roll

	float		radius;//R00k keeping radius for testing

// brush model
	int		firstmodelsurface, nummodelsurfaces;

	int		numsubmodels;
	dmodel_t	*submodels;

	int		numplanes;
	mplane_t	*planes;

	int		numleafs;	// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int		numvertexes;
	mvertex_t	*vertexes;

	int		numedges;
	medge_t		*edges;

	int		numnodes;
	mnode_t		*nodes;

	int		numtexinfo;
	mtexinfo_t	*texinfo;

	int		numsurfaces;
	msurface_t	*surfaces;

	int		numsurfedges;
	int		*surfedges;

	int		numclipnodes;
	mclipnode_t	*clipnodes;

	int		nummarksurfaces;
	msurface_t	**marksurfaces;

	hull_t		hulls[MAX_MAP_HULLS];

	int		numtextures;
	texture_t	**textures;

	byte		*visdata;
	byte		*lightdata;
	char		*entities;

	int			bspversion;
	int			contentstransparent;	//spike -- added this so we can disable glitchy wateralpha where its not supported.
	qboolean	isworldmodel;

	// additional model data
	cache_user_t	cache;		// only access through Mod_Extradata
} model_t;

//============================================================================

void Mod_Init (void);
void Mod_ClearAll (void);
model_t *Mod_ForName (char *name, qboolean crash);
void *Mod_Extradata (model_t *mod);	// handles caching
void Mod_TouchModel (char *name);

mleaf_t *Mod_PointInLeaf (float *p, model_t *model);
byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model);

#endif	// __MODEL__
