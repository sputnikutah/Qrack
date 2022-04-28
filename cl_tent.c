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
// cl_tent.c -- client side temporary entities

#include "quakedef.h"

int			num_temp_entities;
entity_t	cl_temp_entities[MAX_TEMP_ENTITIES];
beam_t		cl_beams[MAX_BEAMS];

static	vec3_t	playerbeam_end = {0,0,0};		// added by joe //R00k init to zero

float		ExploColor[3];		// joe: for color mapped explosions

//model_t		*cl_bolt1_mod, *cl_bolt2_mod, *cl_bolt3_mod, *cl_beam_mod;

model_t		*cl_beam_mod;

sfx_t		*cl_sfx_wizhit;
sfx_t		*cl_sfx_knighthit;
sfx_t		*cl_sfx_tink1;
sfx_t		*cl_sfx_ric1;
sfx_t		*cl_sfx_ric2;
sfx_t		*cl_sfx_ric3;
sfx_t		*cl_sfx_r_exp3;
sfx_t		*cl_sfx_thunder;
sfx_t		*cl_sfx_step1;
sfx_t		*cl_sfx_step2;
sfx_t		*cl_sfx_step3;
sfx_t		*cl_sfx_step4;
sfx_t		*cl_sfx_step5;

extern cvar_t	r_glowlg;
/*
=================
CL_InitTEnts
=================
*/
void CL_InitTEnts (void)
{
	cl_sfx_wizhit		= S_PrecacheSound ("wizard/hit.wav");
	cl_sfx_knighthit	= S_PrecacheSound ("hknight/hit.wav");
	cl_sfx_tink1		= S_PrecacheSound ("weapons/tink1.wav");
	cl_sfx_ric1			= S_PrecacheSound ("weapons/ric1.wav");
	cl_sfx_ric2			= S_PrecacheSound ("weapons/ric2.wav");
	cl_sfx_ric3			= S_PrecacheSound ("weapons/ric3.wav");
	cl_sfx_r_exp3		= S_PrecacheSound ("weapons/r_exp3.wav");
	cl_sfx_thunder		= S_PrecacheSound ("ambience/thunder1.wav");	
	cl_sfx_step1		= S_PrecacheSound ("misc/step1.wav");	
	cl_sfx_step2		= S_PrecacheSound ("misc/step2.wav");	
	cl_sfx_step3		= S_PrecacheSound ("misc/step3.wav");		
	cl_sfx_step4		= S_PrecacheSound ("misc/step4.wav");	
	cl_sfx_step5		= S_PrecacheSound ("misc/step5.wav");	
}

/*
=================
CL_ClearTEnts
=================
*/
void CL_ClearTEnts (void)
{
//unused:	cl_bolt1_mod = cl_bolt2_mod = cl_bolt3_mod = 
	cl_beam_mod = NULL;

	memset (&cl_beams, 0, sizeof(cl_beams));
}

void vectoangles(vec3_t vec, vec3_t ang);

#define MAX_LIGHTNINGBEAMS 10

/*
=================
CL_ParseBeam
=================
*/
void CL_ParseBeam (model_t *m)
{
	int	i, ent;
	vec3_t	start, end;
	beam_t	*b;
	ent = MSG_ReadShort ();
	
	start[0] = MSG_ReadCoord ();
	start[1] = MSG_ReadCoord ();
	start[2] = MSG_ReadCoord ();
	
	end[0] = MSG_ReadCoord ();
	end[1] = MSG_ReadCoord ();
	end[2] = MSG_ReadCoord ();

	if (ent == cl.viewentity)
		VectorCopy(end, playerbeam_end);	// for cl_truelightning
	
	// override any beam with the same entity
	for (i = 0, b = cl_beams ; i < MAX_BEAMS ; i++, b++)
	{
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2f;
			VectorCopy(start, b->start);
			VectorCopy(end, b->end);
			return;
		}
	}

// find a free beam
	for (i = 0, b = cl_beams ; i < MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;

			b->model = m;
			b->endtime = cl.time + 0.2f;
			VectorCopy(start, b->start);
			VectorCopy(end, b->end);
			return;
		}
	}
	Con_Printf ("beam list overflow!\n");	
}

#define	SetCommonExploStuff						\
	dl = CL_AllocDlight (0);					\
	VectorCopy (pos, dl->origin);					\
	dl->radius = 160 * bound(0, r_explosionlight.value, 1);	\
	dl->die = cl.time + 0.5;					\
	dl->decay = 300;\

/*
=================
CL_ParseTEnt
=================
*/
extern cvar_t	gl_decal_bullets;
extern cvar_t   gl_decal_explosions;

extern void R_SpawnDecalStatic(vec3_t org, int tex, int size);

void CL_ParseTEnt (void)
{
	int		type, rnd, colorStart, colorLength;
	vec3_t		pos;
	dlight_t	*dl;
#ifdef GLQUAKE	// shut up compiler
	byte		*colorByte;
#endif
	float		count;//JT

	if (!cl.worldmodel)
		return;
	
	type = MSG_ReadByte ();
	switch (type)
	{
	case TE_BLOOD:	// blood puff hitting flesh
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		count = MSG_ReadByte (); // amount of particles
		R_RunParticleEffect (pos, vec3_origin, 0, 20);
		break;

	case TE_WIZSPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_RunParticleEffect (pos, vec3_origin, 20, 30);
		S_StartSound (-1, 0, cl_sfx_wizhit, pos, 1, 1);
		break;

	case TE_KNIGHTSPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_RunParticleEffect (pos, vec3_origin, 226, 20);
		S_StartSound (-1, 0, cl_sfx_knighthit, pos, 1, 1);
		break;

	case TE_SPIKE:				// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

			
		//R00k--start
		if (gl_decal_bullets.value)
		{				
			R_SpawnDecalStatic(pos, decal_mark, 8);
		}
		//R00k--end

		// joe: they put the ventillator's wind effect to "10" in Nehahra. sigh.
		if (nehahra)
			R_RunParticleEffect (pos, vec3_origin, 0, 9);
		else
			R_RunParticleEffect (pos, vec3_origin, 0, 10);

		if (rand() % 5)
		{
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		}
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;

	case TE_SUPERSPIKE:			// super spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		//R00k--start
		if (gl_decal_bullets.value)
		{			
			R_SpawnDecalStatic(pos, decal_mark, 10);
		}
		//R00k--end

		R_RunParticleEffect (pos, vec3_origin, 0, 20);
		
		if (rand() % 5)
		{
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		}
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;

	case TE_GUNSHOT:			// bullet hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		//R00k--start
		if (gl_decal_bullets.value)
		{				
			R_SpawnDecalStatic(pos, decal_mark, 8);
		}
		//R00k--end

		R_RunParticleEffect (pos, vec3_origin, 0, 24);//R00k changed to 24 just to be different ;)
		break;

	case TE_EXPLOSION:			// rocket explosion
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_ParticleExplosion (pos);
/*
		if (gl_decal_explosions.value)
		{
			R_SpawnDecalStatic(pos, decal_burn, 100);
		}
*/		

		if (r_explosionlight.value)
		{
			SetCommonExploStuff;
			#ifdef GLQUAKE
			dl->type = SetDlightColor (r_explosionlightcolor.value, lt_explosion, true);
			#endif
		}

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TAREXPLOSION:			// tarbaby explosion
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		R_BlobExplosion (pos);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_LIGHTNING1:				// lightning bolts
		CL_ParseBeam (Mod_ForName("progs/bolt.mdl", true));
		break;

	case TE_LIGHTNING2:				// lightning bolts
		CL_ParseBeam (Mod_ForName("progs/bolt2.mdl", true));
		break;

	case TE_LIGHTNING3:				// lightning bolts
		CL_ParseBeam (Mod_ForName("progs/bolt3.mdl", true));
		break;

	// nehahra support
        case TE_LIGHTNING4:                             // lightning bolts
                CL_ParseBeam (Mod_ForName(MSG_ReadString(), true));
		break;

// PGM 01/21/97 
	case TE_BEAM:				// grappling hook beam
		if (!cl_beam_mod)
			cl_beam_mod = Mod_ForName ("progs/beam.mdl", true);
		CL_ParseBeam (cl_beam_mod);
		break;
// PGM 01/21/97

	case TE_LAVASPLASH:	
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		R_LavaSplash (pos);
		break;

	case TE_TELEPORT:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		R_TeleportSplash (pos);
		break;

	case TE_EXPLOSION2:			// color mapped explosion
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		colorStart = MSG_ReadByte ();
		colorLength = MSG_ReadByte ();

		if (r_explosiontype.value == 4)
		{
			R_RunParticleEffect (pos, vec3_origin, 225, 50);
		}
		else
		{			
			R_ColorMappedExplosion (pos, colorStart, colorLength);
		}
/*
		if (gl_decal_explosions.value)
		{							
			R_SpawnDecalStatic(pos, decal_burn, 100);
		}
*/		
		if (r_explosionlight.value)
		{
			SetCommonExploStuff;
#ifdef GLQUAKE
			colorByte = (byte *)&d_8to24table[colorStart];
			ExploColor[0] = ((float)colorByte[0]) / (2.0 * 255.0);
			ExploColor[1] = ((float)colorByte[1]) / (2.0 * 255.0);
			ExploColor[2] = ((float)colorByte[2]) / (2.0 * 255.0);
			dl->type = lt_explosion2;
#endif
		}

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	// nehahra support
	case TE_EXPLOSION3:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		ExploColor[0] = MSG_ReadCoord () / 2.0;
		ExploColor[1] = MSG_ReadCoord () / 2.0;
		ExploColor[2] = MSG_ReadCoord () / 2.0;

		R_ParticleExplosion (pos);
/*
		if (gl_decal_explosions.value)
		{							
			R_SpawnDecalStatic(pos, decal_burn, 100);
		}
*/
		if (r_explosionlight.value)
		{
			SetCommonExploStuff;
#ifdef GLQUAKE
			dl->type = lt_explosion3;
#endif
		}
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	default:
		Sys_Error ("CL_ParseTEnt: bad type");
	}
}

/*
=================
CL_NewTempEntity
=================
*/
entity_t *CL_NewTempEntity (void)
{
	entity_t	*ent;

	if (cl_numvisedicts == MAX_VISEDICTS || num_temp_entities == MAX_TEMP_ENTITIES)
		return NULL;

	ent = &cl_temp_entities[num_temp_entities];
	memset (ent, 0, sizeof(*ent));
	num_temp_entities++;

	cl_visedicts[cl_numvisedicts++] = ent;

	ent->colormap = 0;
#ifdef GLQUAKE
	if (!ent->transparency)
		ent->transparency = 1;
#endif
	return ent;
}

/*
=================
CL_UpdateTEnts
=================
*/
void CL_UpdateTEnts (void)
{
	int			i;
	beam_t		*b;
	vec3_t		dist, beamstart, org;
	float		d, yaw, pitch, forward;
	entity_t	*ent;

	extern cvar_t	v_viewheight;
	extern cvar_t	cl_lightning_zadjust;
	extern cvar_t	gl_lightning_type;

#ifdef GLQUAKE
	int			j;
	vec3_t		beamend;
//	qboolean	sparks = false;
	extern	void QMB_Lightning_Splash (vec3_t org);
#endif

	if (cl.time < 0.1f) 
		return;

	if (sv.active && sv.paused) 
		return;

	num_temp_entities = 0;

	// update lightning
	for (i = 0, b = cl_beams ; i < MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		// zadjust only controls the end, viewheight controls the start
		if ((b->entity == cl.viewentity)&&(!chase_active.value))
		{
			VectorCopy (cl_entities[cl.viewentity].origin, b->start);

			b->start[2] += cl.crouch;
			b->start[2] += bound(-7, v_viewheight.value, 4);		
			
			if ((cl_truelightning.value) && (!cls.demoplayback))
			{
				vec3_t	forward, v, org, ang;
				float	f, delta;
				trace_t	trace;

				f = max(0, min(1, cl_truelightning.value));

				VectorSubtract (playerbeam_end, cl_entities[cl.viewentity].origin, v);
				v[2] -= cl.crouch; 
				
				vectoangles (v, ang);

				// lerp pitch
				ang[0] = -ang[0];
				if (ang[0] < -180)
					ang[0] += 360;
				ang[0] += (cl.viewangles[0] - ang[0]) * f;

				// lerp yaw
				delta = cl.viewangles[1] - ang[1];
				if (delta > 180)
					delta -= 360;
				if (delta < -180)
					delta += 360;
				ang[1] += delta * f;
				ang[2] = 0;

				AngleVectors (ang, forward, NULLVEC, NULLVEC);
				VectorScale(forward, 600, forward);

				VectorCopy(cl_entities[cl.viewentity].origin, org);
				
				org[2] += cl.crouch; 
				org[2] += bound(0, cl_lightning_zadjust.value, 16);
				org[2] += bound(-7, v_viewheight.value, 4);			
				
				VectorAdd(org, forward, b->end);

				memset (&trace, 0, sizeof(trace_t));

				if (!SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, org, b->end, &trace))
				{
					//Con_DPrintf (1,"CL_UpdateTEnts : SV_RecursiveHullCheck hit world model\n");
					VectorCopy(trace.endpos, b->end);				
				}
			}
		}

		// calculate pitch and yaw
		VectorSubtract(b->end, b->start, dist);
		
		if (!dist[1] && !dist[0])
		{
			yaw = 0;
			pitch = (dist[2] > 0) ? 90 : 270;
		}
		else
		{
			yaw = atan2 (dist[1], dist[0]) * 180 / M_PI;
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = atan2 (dist[2], forward) * 180 / M_PI;
			if (pitch < 0)
				pitch += 360;
		}

		// add new entities for the lightning
		VectorCopy(b->start, org);
		VectorCopy(b->start, beamstart);
		d = VectorNormalize (dist);
		VectorScale (dist, 30, dist);

//		if (key_dest == key_game)							//Only draw if not in the menu or console, ie in game.
		{			
			for ( ; d > 0 ; d -= 30)
			{
				#ifdef GLQUAKE
				if ((r_glowlg.value == 1) && (r_dynamic.value))
						CL_NewDlight (i, beamend, 100, 0.1, lt_blue);//Changed to glow at beamend

				if ((qmb_initialized && gl_part_lightning.value))
				{					
 					if ((!cl.paused) && (key_dest == key_game))
					{
						VectorAdd(org, dist, beamend);	

						if (gl_lightning_type.value)
						{
							beamend[0] += sin((3.14)*(rand() % 4)-2);
							beamend[1] += sin((3.14)*(rand() % 4)-2);
							beamend[2] += cos((3.14)*(rand() % 4)-2);
						}
						else
						{
							for (j = 0 ; j < 3 ; j++)
								beamend[j] += ((rand() % 8) - 4);
						}
	
						QMB_LightningBeam (beamstart, beamend);				
						VectorCopy(beamend, beamstart);				
					}
				}
				else
				#endif
				{
					if (!(ent = CL_NewTempEntity()))
						return;					

					VectorCopy(org, ent->origin);
					
					ent->model = b->model;
					ent->angles[0] = pitch;
					ent->angles[1] = yaw;
					ent->angles[2] = rand () % 360;
				}
				VectorAdd(org, dist, org);
			}
		}
	}
}
