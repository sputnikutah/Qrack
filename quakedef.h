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

//
// quakedef.h -- primary header for client

//#define	QUAKE_GAME			// as opposed to utilities

#define	SERVER_VERSION			2.020

#define QRACK_VERSION			"(x86_32)"

#define MOD_PROQUAKE_VERSION	3.50 //PQ Compatibility

//define	PARANOID			// speed sapping error checking

#define	GAMENAME	"id1"		// directory to look in by default

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef GLQUAKE			// already included in pngconf.h
#include <setjmp.h>
#else
#include "png.h"

#endif

#include <assert.h>

#if defined(_WIN32) && !defined(WINDED)

void	VID_LockBuffer (void);
void	VID_UnlockBuffer (void);
#endif

#if id386
#define UNALIGNED_OK	1	// set to 0 if unaligned accesses are not supported
#else
#define UNALIGNED_OK	0
#endif

#define UNUSED(x)	(x = x)	// for pesky compiler / lint warnings

#define	MINIMUM_MEMORY	0x550000
#define	MINIMUM_MEMORY_LEVELPAK		(MINIMUM_MEMORY + 0x100000)

#define MAX_NUM_ARGVS	50

// up / down
#define	PITCH		0

// left / right
#define	YAW			1

// fall over
#define	ROLL		2


#define	MAX_QPATH		64			// max length of a quake game pathname
#define	MAX_OSPATH		128			// max length of a filesystem pathname

#define	ON_EPSILON		0.1			// point on plane side epsilon

#define	MAX_MSGLEN			32000	// max length of a reliable message //johnfitz -- was 8000
#define	MAX_DATAGRAM		32000	// max length of unreliable message //johnfitz -- was 1024

#define DATAGRAM_MTU		1400	// johnfitz -- actual limit for unreliable messages to nonlocal clients

//
// per-level limits
//

#define MIN_EDICTS		256			// johnfitz -- lowest allowed value for max_edicts cvar
#define MAX_EDICTS		32000		// johnfitz -- highest allowed value for max_edicts cvar
									// ents past 8192 can't play sounds in the standard protocol
#define	MAX_LIGHTSTYLES 64
#define MAX_DLIGHTS		64		//MH: robust support for increased dynamic lights

#define	MAX_MODELS		2048		// johnfitz -- was 256
#define	MAX_SOUNDS		2048		// johnfitz -- was 256

#define	SAVEGAME_COMMENT_LENGTH	39

#define	MAX_STYLESTRING		64

// stats are integers communicated to the client by the server
#define	MAX_CL_STATS		32
#define	STAT_HEALTH		0
#define	STAT_FRAGS		1
#define	STAT_WEAPON		2
#define	STAT_AMMO		3
#define	STAT_ARMOR		4
#define	STAT_WEAPONFRAME	5
#define	STAT_SHELLS		6
#define	STAT_NAILS		7
#define	STAT_ROCKETS		8
#define	STAT_CELLS		9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13		// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		// bumped by svc_killedmonster
#define STAT_TEMP		15
#define	STAT_ITEMS		15
#define	STAT_VIEWHEIGHT		16

// stock defines

#define	IT_SHOTGUN			1
#define	IT_SUPER_SHOTGUN		2
#define	IT_NAILGUN			4
#define	IT_SUPER_NAILGUN		8
#define	IT_GRENADE_LAUNCHER		16
#define	IT_ROCKET_LAUNCHER		32
#define	IT_LIGHTNING			64
//#define IT_SUPER_LIGHTNING      128
#define IT_HOOK			128//R00k added hook instead
#define IT_SHELLS               256
#define IT_NAILS                512
#define IT_ROCKETS              1024
#define IT_CELLS                2048
#define IT_AXE                  4096
#define IT_ARMOR1               8192
#define IT_ARMOR2               16384
#define IT_ARMOR3               32768
#define IT_SUPERHEALTH          65536
#define IT_KEY1                 131072
#define IT_KEY2                 262144
#define	IT_INVISIBILITY		524288
#define	IT_INVULNERABILITY	1048576
#define	IT_SUIT			2097152
#define	IT_QUAD			4194304
#define IT_SIGIL1               (1<<28)
#define IT_SIGIL2               (1<<29)
#define IT_SIGIL3               (1<<30)
#define IT_SIGIL4               (1<<31)

//Malice defines
/*
#define IT_44              = 4096;
#define IT_SHOT            = 1;
#define IT_SHOT2           = 2;
#define IT_UZI             = 4;
#define IT_MINI            = 8;
#define IT_MORTER          = 16;
#define IT_ROCKET          = 32;
#define IT_PUNISHER        = 64;
*/

//===========================================
//rogue changed and added defines

#define RIT_SHELLS              128
#define RIT_NAILS               256
#define RIT_ROCKETS             512
#define RIT_CELLS               1024
#define RIT_AXE                 2048
#define RIT_LAVA_NAILGUN        4096
#define RIT_LAVA_SUPER_NAILGUN  8192
#define RIT_MULTI_GRENADE       16384
#define RIT_MULTI_ROCKET        32768
#define RIT_PLASMA_GUN          65536
#define RIT_ARMOR1              8388608
#define RIT_ARMOR2              16777216
#define RIT_ARMOR3              33554432
#define RIT_LAVA_NAILS          67108864
#define RIT_PLASMA_AMMO         134217728
#define RIT_MULTI_ROCKETS       268435456
#define RIT_SHIELD              536870912
#define RIT_ANTIGRAV            1073741824
#define RIT_SUPERHEALTH         2147483648

//===========================================
//MED 01/04/97 added hipnotic defines

#define	HIT_PROXIMITY_GUN_BIT	16
#define	HIT_MJOLNIR_BIT			7
#define	HIT_LASER_CANNON_BIT	23
#define	HIT_PROXIMITY_GUN		(1<<HIT_PROXIMITY_GUN_BIT)
#define	HIT_MJOLNIR				(1<<HIT_MJOLNIR_BIT)
#define	HIT_LASER_CANNON		(1<<HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT				(1 << (23 + 1))// corrected wetsuit define
#define HIT_EMPATHY_SHIELDS		(1 << (23 + 2))// corrected empathy shield define

//===========================================

#define	MAX_SCOREBOARD		16
#define	MAX_SCOREBOARDNAME	32

#define	SOUND_CHANNELS		8

#include "common.h"
#include "bspfile.h"
#include "vid.h"
#include "sys.h"
#include "zone.h"
#include "mathlib.h"

typedef struct
{
	vec3_t	origin;
	vec3_t	angles;
	unsigned short 	modelindex; //johnfitz -- was int
	unsigned short 	frame; //johnfitz -- was int
	unsigned char 	colormap; //johnfitz -- was int
	unsigned char	transparency; //johnfitz -- added
	unsigned char	alpha; //johnfitz -- added
	int		skin;//teamskins
	int		effects;
//	int		v_weapon;//Demo support for vweap
} entity_state_t;

#include "wad.h"
#include "draw.h"
#include "cvar.h"
#include "screen.h"
#include "net.h"
#include "protocol.h"
#include "cmd.h"
#include "sbar.h"
#include "sound.h"
#include "render.h"
#include "client.h"
#include "progs.h"
#include "server.h"

#ifdef GLQUAKE
#include "gl_model.h"
#else
#include "r_model.h"
#include "d_iface.h"
#endif

#include "input.h"
#include "version.h"//R00k
#include "world.h"
#include "keys.h"
#include "console.h"
#include "view.h"
#include "menu.h"
#include "crc.h"
#include "cdaudio.h"

#ifdef GLQUAKE
#include "glquake.h"
#include "nehahra.h"
#endif
#include "location.h"	// JPG - for %l formatting speficier (from ProQuake)
#include "iplog.h"		// JPG 1.05 - ip address logging
#include "slist.h"
//=============================================================================

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

typedef struct
{
	char	*basedir;
	int		argc;
	char	**argv;
	void	*membase;
	int	memsize;
} quakeparms_t;

//=============================================================================

extern	qboolean	noclip_anglehack;

// host
extern	quakeparms_t	host_parms;

extern	cvar_t		sys_ticrate;
extern	cvar_t		sys_nostdout;
extern	cvar_t		developer;

extern	cvar_t		sv_progs;

extern	qboolean	host_initialized;	// true if into command execution
extern	double		host_frametime;
extern	byte		*host_basepal;
extern	byte		*host_colormap;
extern	int			host_framecount;	// incremented every frame, never reset
extern	double		realtime;			// not bounded in any way, changed at
										// start of every frame, never reset
extern	double		last_angle_time;	// JPG - need this for smooth chasecam (from Proquake)

void Host_ClearMemory (void);
void Host_ServerFrame (void);
void Host_InitCommands (void);
void Host_Init (quakeparms_t *parms);
void Host_Shutdown (void);
void Host_Error (char *error, ...);
void Host_EndGame (char *message, ...);
void Host_Frame (double time);
void Host_Quit (void);
void Host_Quit_f (void);
void Host_ClientCommands (char *fmt, ...);
void Host_ShutdownServer (qboolean crash);

extern qboolean		msg_suppress_1;		// suppresses resolution and cache size console output
										// a fullscreen DIB focus gain/loss
extern int		current_skill;			// skill level for currently loaded level (in case
										// the user changes the cvar while the level is
										// running, this reflects the level actually in use)

extern qboolean		isDedicated;

extern int		minimum_memory;

// chase
extern	cvar_t	chase_active;

void Chase_Init (void);
void Chase_Reset (void);
void Chase_Update (void);
extern char dequake[256];	// JPG 1.05 - dedicated console translation
extern cvar_t pq_dequake;	// JPG 1.05 - dedicated console translation
char *wih, wih_buff[50];

#define NUMVERTEXNORMALS	162

#define	DIST_EPSILON	(0.03125)// 1/32 epsilon to keep floating point happy

extern	char	*argv0;
extern	cvar_t	cl_autodemo;//r00k
extern	cvar_t gl_coronas;
extern	vec3_t	NULLVEC;
#define ISUNDERWATER(x) (((x) == CONTENTS_WATER || (x) == CONTENTS_SLIME) && (x) != CONTENTS_LAVA)

#define TruePointContents(p) SV_HullPointContents(&cl.worldmodel->hulls[0], 0, p)

extern void Host_WriteConfig_f (void);

#define offsetrandom(MIN,MAX) ((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))//LordHavoc

#define	ELEMENT_X_COORD(var)	((var##_x.value < 0) ? vid.width - strlen(str) * 8 + 8 * var##_x.value: 8 * var##_x.value)
#define	ELEMENT_Y_COORD(var)	((var##_y.value < 0) ? vid.height - sb_lines + 8 * var##_y.value : 8 * var##_y.value)

typedef struct edict_tag_s 
{
	float x, y;
	char classname[64];
	char origin[64];
	char modelname[64];
	float scale;
} edict_tag_t;

static edict_tag_t edicttags[MAX_EDICTS];
static int ed_tag_count;


extern	qboolean	con_debugconsole;

void Con_Warning (const char *fmt, ...);

typedef struct tempent_s
{
	entity_t *ent;
	struct tempent_s *next;
} tempent_t;

void *MallocZ (int size);//MH

// mh - handly little buffer for putting any short-term memory requirements into
// avoids the need to malloc an array, then free it after, for a lot of setup and loading code
extern byte scratchbuf[];
#define SCRATCHBUF_SIZE 2097152

#define ISWHITESPACE(ch) (!(ch) || (ch) == ' ' || (ch) == '\t' || (ch) == '\r' || (ch) == '\n')