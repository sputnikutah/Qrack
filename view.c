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
// view.c -- player eye positioning

#include "quakedef.h"
#include <time.h>	// cl_clock

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boundary.

*/


#ifndef GLQUAKE
cvar_t	lcd_x = {"lcd_x","0"};
cvar_t	lcd_yaw = {"lcd_yaw","0"};
#endif

cvar_t	scr_ofsx = {"scr_ofsx","0", false};
cvar_t	scr_ofsy = {"scr_ofsy","0", false};
cvar_t	scr_ofsz = {"scr_ofsz","0", false};

cvar_t	scr_clock	= {"cl_clock", "0"};
cvar_t	scr_clock_x = {"cl_clock_x", "0"};
cvar_t	scr_clock_y = {"cl_clock_y", "-1"};

cvar_t	show_speed	= {"show_speed", "0"};

cvar_t	show_fps	= {"show_fps","0"};
cvar_t	show_fps_x = {"show_fps_x", "-5"};
cvar_t	show_fps_y = {"show_fps_y", "-1"};

cvar_t	show_healthbar = {"show_healthbar","0"};
cvar_t	show_healthbar_x = {"show_healthbar_x", "0"};
cvar_t	show_healthbar_y = {"show_healthbar_y", "0"};

cvar_t	show_armorbar	= {"show_armorbar","0"};
cvar_t	show_armorbar_x = {"show_armorbar_x", "0"};
cvar_t	show_armorbar_y = {"show_armorbar_y", "0"};

cvar_t	show_ammobar   = {"show_ammobar","0"};
cvar_t	show_ammobar_x = {"show_ammobar_x", "0"};
cvar_t	show_ammobar_y = {"show_ammobar_y", "0"};

cvar_t	show_rec = {"show_rec","0"};
cvar_t	show_rec_x = {"show_rec_x", "-12"};
cvar_t	show_rec_y = {"show_rec_y", "-1"};

cvar_t	show_ping = {"show_ping","0"};
cvar_t	show_ping_x = {"show_ping_x", "-5"};
cvar_t	show_ping_y = {"show_ping_y", "-1"};

cvar_t	show_locname= {"show_locname","0"};
cvar_t	cl_rollspeed = {"cl_rollspeed", "200"};
cvar_t	cl_rollangle = {"cl_rollangle", "2.0"};
cvar_t	cl_gun_offset = {"cl_gun_offset","0", true};
cvar_t	cl_gun_drift = {"cl_gun_drift","0.2", true};
cvar_t	cl_gun_idle_movement = {"cl_gun_idle_movement","0", true};

cvar_t	cl_viewbob	= {"cl_viewbob","0", false};
cvar_t	cl_bob		= {"cl_bob","0.02", false};
cvar_t	cl_bobcycle = {"cl_bobcycle","0.6", false};
cvar_t	cl_bobup	= {"cl_bobup","0.5", false};
cvar_t	cl_bobfall	= {"cl_bobfall","0", true};
cvar_t	cl_bobfall_scale = {"cl_bobfall_scale","2",true};

cvar_t	v_kicktime	= {"v_kicktime", "0.5", false};
cvar_t	v_kickroll	= {"v_kickroll", "0.6", false};
cvar_t	v_kickpitch = {"v_kickpitch", "0.6", false};
cvar_t	v_gunkick	= {"v_gunkick", "0", true};		// by joe

cvar_t	v_iyaw_cycle	= {"v_iyaw_cycle", "2", false};
cvar_t	v_iroll_cycle	= {"v_iroll_cycle", "0.5", false};
cvar_t	v_ipitch_cycle	= {"v_ipitch_cycle", "1", false};
cvar_t	v_iyaw_level	= {"v_iyaw_level", "0.3", false};
cvar_t	v_iroll_level	= {"v_iroll_level", "0.1", false};
cvar_t	v_ipitch_level	= {"v_ipitch_level", "0.3", false};
cvar_t	v_viewheight	= {"v_viewheight", "0",true};
cvar_t	v_idlescale		= {"v_idlescale", "0", false};

cvar_t	crosshair		= {"crosshair", "1", true};
cvar_t	crosshaircolor	= {"crosshaircolor", "255 255 255", true};//R G B
cvar_t	crosshairsize	= {"crosshairsize", "1", true};
cvar_t	cl_crossx		= {"cl_crossx", "0", true};
cvar_t	cl_crossy		= {"cl_crossy", "0", true};

cvar_t  v_contentblend	= {"v_contentblend", "1"};
cvar_t	v_damagecshift	= {"v_damagecshift", "1"};
cvar_t	v_quadcshift	= {"v_quadcshift", "1"};
cvar_t	v_suitcshift	= {"v_suitcshift", "1"};
cvar_t	v_ringcshift	= {"v_ringcshift", "1"};
cvar_t	v_pentcshift	= {"v_pentcshift", "1"};

#ifdef GLQUAKE
cvar_t	v_dlightcshift = {"v_dlightcshift", "1"};
#endif

cvar_t	v_bonusflash	= {"cl_bonusflash", "1"};

float	v_dmg_time, v_dmg_roll, v_dmg_pitch;

extern	int	in_forward, in_forward2, in_back;

/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
vec3_t	right;

float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	float	sign, side;
	
	AngleVectors (angles, NULL, right, NULL);
	side = DotProduct(velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);
	
	side = (side < cl_rollspeed.value) ? side * cl_rollangle.value / cl_rollspeed.value : cl_rollangle.value;

	return side * sign;	
}


/*
===============
V_CalcBob
===============
*/
float V_CalcBob (void)
{
	static	float	bob;
	float			cycle;
	
	if (cl_bobcycle.value <= 0)
		return 0;

	cycle = cl.time - (cl.time / cl_bobcycle.value) * cl_bobcycle.value;
	cycle /= cl_bobcycle.value;

	if (cycle < cl_bobup.value)
		cycle = M_PI * cycle / cl_bobup.value;
	else
		cycle = M_PI + M_PI * (cycle - cl_bobup.value) / (1.0 - cl_bobup.value);

// bob is proportional to velocity in the xy plane
// (don't count Z, or jumping messes it up)
	bob = sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]) * cl_bob.value;
	bob = bob * 0.3 + bob * 0.7 * sin(cycle);
	bob = bound(-7.0, bob, 4.0);

	return bob;
}

//=============================================================================

cvar_t	v_centermove = {"v_centermove", "0.15", false};
cvar_t	v_centerspeed = {"v_centerspeed","500"};

void V_StartPitchDrift (void)
{
	if (cl.laststop == cl.time)
		return;		// something else is keeping it from drifting

	if (cl.nodrift || !cl.pitchvel)
	{
		cl.pitchvel = v_centerspeed.value;
		cl.nodrift = false;
		cl.driftmove = 0;
	}
}

void V_StopPitchDrift (void)
{
	cl.laststop = cl.time;
	cl.nodrift = true;
	cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when 
===============
*/
void V_DriftPitch (void)
{
	float	delta, move;

	if (noclip_anglehack || !cl.onground || cls.demoplayback)
	{
		cl.driftmove = 0;
		cl.pitchvel = 0;
		return;
	}

// don't count small mouse motion
	if (cl.nodrift)
	{
		if (fabs(cl.cmd.forwardmove) < cl_forwardspeed.value)
			cl.driftmove = 0;
		else
			cl.driftmove += host_frametime;
	
		if (cl.driftmove > v_centermove.value)
				V_StartPitchDrift ();

		return;
	}
	
	delta = cl.idealpitch - cl.viewangles[PITCH];

	if (!delta)
	{
		cl.pitchvel = 0;
		return;
	}

	move = host_frametime * cl.pitchvel;
	cl.pitchvel += host_frametime * v_centerspeed.value;
	
//Con_Printf ("move: %f (%f)\n", move, host_frametime);

	if (delta > 0)
	{
		if (move > delta)
		{
			cl.pitchvel = 0;
			move = delta;
		}
		cl.viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			cl.pitchvel = 0;
			move = -delta;
		}
		cl.viewangles[PITCH] -= move;
	}
}



/*
============================================================================== 
 
				PALETTE FLASHES 
 
============================================================================== 
*/ 
 
 
cshift_t	cshift_empty = {{130, 80, 50}, 0};
cshift_t	cshift_water = {{130, 80, 50}, 128};
cshift_t	cshift_slime = {{0, 25, 5}, 150};
cshift_t	cshift_lava = {{255, 80, 0}, 150};

#ifdef	GLQUAKE

cvar_t		gl_cshiftpercent	= {"gl_cshiftpercent", "100"};
//cvar_t		gl_hwblend			= {"gl_hwblend", "0"};
float		v_blend[4];		// rgba 0.0 - 1.0
cvar_t		v_gamma				= {"gl_gamma", "0.85", true};
cvar_t		v_contrast			= {"gl_contrast", "2", true};
unsigned short	ramps[3][256];

#else

byte		gammatable[256];	// palette is sent through this
byte		current_pal[768];	// Tonik: used for screenshots
cvar_t		v_gamma				= {"gamma", "0.5", true};
cvar_t		v_contrast			= {"contrast", "2", true};

#endif

#ifndef GLQUAKE
void BuildGammaTable (float g, float c)
{
	int	i, inf;

	g = bound(0.3, g, 3);
	c = bound(1, c, 3);

	if (g == 1 && c == 1)
	{
		for (i=0 ; i<256 ; i++)
			gammatable[i] = i;
		return;
	}

	for (i=0 ; i<256 ; i++)
	{
		inf = 255 * pow((i + 0.5) / 255.5 * c, g) + 0.5;
		gammatable[i] = bound(0, inf, 255);
	}
}

/*
=================
V_CheckGamma
=================
*/
qboolean V_CheckGamma (void)
{
	static	float	old_gamma, old_contrast;

	if (v_gamma.value == old_gamma && v_contrast.value == old_contrast)
		return false;

	old_gamma = v_gamma.value;
	old_contrast = v_contrast.value;

	BuildGammaTable (v_gamma.value, v_contrast.value);
	vid.recalc_refdef = 1;			// force a surface cache flush

	return true;
}
#endif // GLQUAKE

/*
===============
V_ParseDamage
===============
*/
extern cvar_t	gl_hurtblur;
extern int		cl_hurtblur;//R00k

float	/*damagetime = 0,*/ damagecount;

void V_ParseDamage (void)
{
	int		i, armor, blood;
	vec3_t		from, forward, right, up;
	entity_t	*ent;
	float		side, fraction;
	
	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();

	for (i=0 ; i<3 ; i++)
		from[i] = MSG_ReadCoord ();

	damagecount = blood * 0.5 + armor * 0.5;

	if (damagecount < 10)
		damagecount = 10;

	cl.faceanimtime = cl.time + 0.2;		// put sbar face into pain frame
	
//R00k
	if (gl_hurtblur.value)
	{
		cl.hurtblur = cl.time + (damagecount/24);//R00k
	}
	else
	{
		cl.hurtblur = 0;
	}

	cl.cshifts[CSHIFT_DAMAGE].percent += 3*damagecount;
	if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
		cl.cshifts[CSHIFT_DAMAGE].percent = 150;

	fraction = bound(0, v_damagecshift.value, 1);
	cl.cshifts[CSHIFT_DAMAGE].percent *= fraction;

	if (armor > blood)		
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	}
	else if (armor)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	}
	else
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

// calculate view angle kicks
	if ((v_kicktime.value)&&(v_kickroll.value || v_kickpitch.value))//R00k skip if disabled
	{				
		ent = &cl_entities[cl.viewentity];
					
		VectorSubtract (from, ent->origin, from);
		VectorNormalize (from);
					
		AngleVectors (ent->angles, forward, right, up);

		side = DotProduct (from, right);
		v_dmg_roll = damagecount*side*v_kickroll.value;
					
		side = DotProduct (from, forward);
		v_dmg_pitch = damagecount*side*v_kickpitch.value;
		v_dmg_time = v_kicktime.value;
	}
}

/*
==================
V_cshift_f
==================
*/
void V_cshift_f (void)
{
	cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
	cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
	cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
	cshift_empty.percent = atoi(Cmd_Argv(4));
}

/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f (void)
{
	if (!v_bonusflash.value)
		return;

	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	cl.cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void V_SetContentsColor (int contents)
{
	if ((!v_contentblend.value) || (!gl_polyblend.value))
	{
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
#ifdef GLQUAKE
		cl.cshifts[CSHIFT_CONTENTS].percent *= 100;
#endif
		return;
	}

	switch (contents)
	{
	case CONTENTS_EMPTY:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		break;
	case CONTENTS_LAVA:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
		break;
	case CONTENTS_SOLID:
	case CONTENTS_SLIME:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
		break;
	default:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
	}

	if (v_contentblend.value > 0 && v_contentblend.value < 1 && contents != CONTENTS_EMPTY)
	{
		cl.cshifts[CSHIFT_CONTENTS].percent *= v_contentblend.value;
	}

	#ifdef GLQUAKE
	if (contents != CONTENTS_EMPTY)
	{
		if (!gl_polyblend.value)
			cl.cshifts[CSHIFT_CONTENTS].percent = 0;
		else
			cl.cshifts[CSHIFT_CONTENTS].percent *= gl_cshiftpercent.value;
	}
	else
	{
		cl.cshifts[CSHIFT_CONTENTS].percent *= 100;
	}
	#endif
}

/*
=============
V_AddWaterFog

Fog in liquids, from FuhQuake
=============
*/
#ifdef GLQUAKE
void V_AddWaterfog (int contents)
{
	float	*colors;
	float	lava[4] = {1.0f, 0.314f, 0.0f, 1};
	float	slime[4] = {0.039f, 0.738f, 0.333f, 1};
	float	water[4] = {0.05f, 0.05f, 0.030f, 1};

	if (!gl_waterfog.value || contents == CONTENTS_EMPTY || contents == CONTENTS_SOLID)
	{
		glDisable (GL_FOG);
		return;
	}

	switch (contents)
	{
	case CONTENTS_LAVA:
		colors = lava;
		break;

	case CONTENTS_SLIME:
		colors = slime;
		break;

	default:
		colors = water;
		break;
	}
	
	glFogfv (GL_FOG_COLOR, colors);
	if (((int)gl_waterfog.value) == 2)
	{
		glFogf (GL_FOG_DENSITY, 0.0002 + (0.0009 - 0.0002) * bound(0, gl_waterfog_density.value, 1));
		glFogi (GL_FOG_MODE, GL_EXP);
	}
	else
	{
		glFogi (GL_FOG_MODE, GL_LINEAR);
		glFogf (GL_FOG_START, 150.0f);	
		glFogf (GL_FOG_END, 4250.0f - (4250.0f - 1536.0f) * bound(0, gl_waterfog_density.value, 1));
	}
	glEnable(GL_FOG);
}
#endif

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift (void)
{
	float	fraction;

	if (cl.stats[STAT_HEALTH] > 0)//R00k
	{
		if (cl.items & IT_QUAD)
		{
			cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
			cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
			cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
			fraction = bound(0, v_quadcshift.value, 1);
			cl.cshifts[CSHIFT_POWERUP].percent = 30 * fraction;
		}
		else if (cl.items & IT_SUIT)
		{
			cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
			cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
			cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
			fraction = bound(0, v_suitcshift.value, 1);
			cl.cshifts[CSHIFT_POWERUP].percent = 20 * fraction;
		}
		else if (cl.items & IT_INVISIBILITY)
		{
			cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
			cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
			cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
			fraction = bound(0, v_ringcshift.value, 1);
			cl.cshifts[CSHIFT_POWERUP].percent = 100 * fraction;
		}
		else if (cl.items & IT_INVULNERABILITY)
		{
			cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
			cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
			cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
			fraction = bound(0, v_pentcshift.value, 1);
			cl.cshifts[CSHIFT_POWERUP].percent = 30 * fraction;
		}
		else
		{
			cl.cshifts[CSHIFT_POWERUP].percent = 0;
		}
	}
	else if (cl.stats[STAT_HEALTH] <= 0)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 245;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 50;
	}
}

/*
=============
V_CalcBlend
=============
*/
#ifdef	GLQUAKE
void V_CalcBlend (void)
{
	float	r, g, b, a, a2;
	int		j;

	r = g = b = a = 0;

	if (cls.state != ca_connected)
	{
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
	}
	else
	{
		V_CalcPowerupCshift ();
	}

		// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= (cl.time - cl.oldtime) * 150;//MH 
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

	// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= (cl.time - cl.oldtime) * 100;//MH
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;


	for (j=0 ; j<NUM_CSHIFTS ; j++)	
	{
		if ((!gl_cshiftpercent.value || !gl_polyblend.value) && j != CSHIFT_CONTENTS)
			continue;

		if (j == CSHIFT_CONTENTS)
			a2 = cl.cshifts[j].percent / 100.0 / 255.0;
		else
			a2 = ((cl.cshifts[j].percent * gl_cshiftpercent.value) / 100.0) / 255.0;

		if (!a2)
			continue;
		a = a + a2 * (1 - a);

		a2 /= a;
		r = r * (1 - a2) + cl.cshifts[j].destcolor[0] * a2;
		g = g * (1 - a2) + cl.cshifts[j].destcolor[1] * a2;
		b = b * (1 - a2) + cl.cshifts[j].destcolor[2] * a2;
	}

	v_blend[0] = r / 255.0;
	v_blend[1] = g / 255.0;
	v_blend[2] = b / 255.0;
	v_blend[3] = bound(0, a, 1);
}

void V_AddLightBlend (float r, float g, float b, float a2)
{
	float	a;

	if (!gl_polyblend.value || !gl_cshiftpercent.value || !v_dlightcshift.value)
		return;

	a2 = a2 * bound(0, v_dlightcshift.value, 1) * gl_cshiftpercent.value / 100.0;

	v_blend[3] = a = v_blend[3] + a2 * (1 - v_blend[3]);

	if (!a)
		return;

	a2 /= a;

	v_blend[0] = v_blend[0] * (1 - a2) + r * a2;
	v_blend[1] = v_blend[1] * (1 - a2) + g * a2;
	v_blend[2] = v_blend[2] * (1 - a2) + b * a2;
}
#endif

/*
=============
V_UpdatePalette
=============
*/
#ifdef	GLQUAKE
void V_UpdatePalette (void)
{
	int				i, j, c;
	qboolean		new;
	static float	prev_blend[4];
	float			a, rgb[3], gamma, contrast;
	static float	old_gamma, old_contrast;//, old_hwblend;
	extern float	vid_gamma;

	new = false;


	for (i=0 ; i<4 ; i++)
	{
		if (v_blend[i] != prev_blend[i])
		{
			new = true;
			prev_blend[i] = v_blend[i];
		}
	}

	gamma = bound(0.3, v_gamma.value, 3);

	if (gamma != old_gamma)
	{
		old_gamma = gamma;
		new = true;
	}

	contrast = bound(1, v_contrast.value, 3);
	if (contrast != old_contrast)
	{
		old_contrast = contrast;
		new = true;
	}

/*	R00k: removed these as gl_hwblend seems useless...
	if (gl_hwblend.value != old_hwblend)
	{
		new = true;
		old_hwblend = gl_hwblend.value;
	}
*/
	if (!new)
		return;

	a = v_blend[3];

//	if (!vid_hwgamma_enabled || !gl_hwblend.value)
//		a = 0;

	rgb[0] = 255 * v_blend[0] * a;
	rgb[1] = 255 * v_blend[1] * a;
	rgb[2] = 255 * v_blend[2] * a;

	a = 1 - a;

	if (vid_gamma != 1.0)
	{
		contrast = pow (contrast, vid_gamma);
		gamma /= vid_gamma;
	}

	for (i = 0 ; i < 256 ; i++)
	{
		for (j = 0 ; j < 3 ; j++)
		{
			// apply blend and contrast
			c = (i*a + rgb[j]) * contrast;
			if (c > 255)
				c = 255;
			// apply gamma
			c = 255 * pow((c + 0.5)/255.5, gamma) + 0.5;
			c = bound(0, c, 255);
			ramps[j][i] = c << 8;
		}
	}

	VID_SetDeviceGammaRamp ((unsigned short *)ramps);
}
#else	// !GLQUAKE
void V_UpdatePalette (void)
{
	int		i, j, r, g, b;
	qboolean	new, force;
	byte		*basepal, *newpal;
	static cshift_t	prev_cshifts[NUM_CSHIFTS];

	if (cls.state != ca_connected)
	{
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
	}
	else
	{
		V_CalcPowerupCshift ();
	}

	new = false;

	for (i=0 ; i<NUM_CSHIFTS ; i++)
	{
		if (cl.cshifts[i].percent != prev_cshifts[i].percent)
		{
			new = true;
			prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j=0 ; j<3 ; j++)
		{
			if (cl.cshifts[i].destcolor[j] != prev_cshifts[i].destcolor[j])
			{
				new = true;
				prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
		}
	}

	// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime * 150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

	// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= host_frametime * 100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	force = V_CheckGamma ();

	if (!new && !force)
		return;

	basepal = host_basepal;
	newpal = current_pal;	// Tonik: so we can use current_pal for screenshots

	for (i=0 ; i<256 ; i++)
	{
		r = basepal[0];
		g = basepal[1];
		b = basepal[2];
		basepal += 3;

		for (j=0 ; j<NUM_CSHIFTS ; j++)	
		{
			r += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[0]-r)) >> 8;
			g += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[1]-g)) >> 8;
			b += (cl.cshifts[j].percent*(cl.cshifts[j].destcolor[2]-b)) >> 8;
		}

		newpal[0] = gammatable[r];
		newpal[1] = gammatable[g];
		newpal[2] = gammatable[b];
		newpal += 3;
	}

	VID_ShiftPalette (current_pal);
}
#endif	// !GLQUAKE

/* 
============================================================================== 
 
				VIEW RENDERING 
 
============================================================================== 
*/ 

float angledelta (float a)
{
	a = anglemod(a);
	if (a > 180)
		a -= 360;
	return a;
}
/*
==================
CalcGunAngle
==================
*/
void CalcGunAngle (void)
{	
	float		yaw, pitch, move;
	static	float	oldyaw = 0;
	static	float	oldpitch = 0;
	
	yaw = r_refdef.viewangles[YAW];
	pitch = -r_refdef.viewangles[PITCH];

	yaw = angledelta(yaw - r_refdef.viewangles[YAW]) * 0.4;
	yaw = bound(-10, yaw, 10);

	pitch = angledelta(-pitch - r_refdef.viewangles[PITCH]) * 0.4;
	pitch = bound(-10, pitch, 10);

	move = host_frametime * 20;
	if (yaw > oldyaw)
	{
		if (oldyaw + move < yaw)
			yaw = oldyaw + move;
	}
	else
	{
		if (oldyaw - move > yaw)
			yaw = oldyaw - move;
	}
	
	if (pitch > oldpitch)
	{
		if (oldpitch + move < pitch)
			pitch = oldpitch + move;
	}
	else
	{
		if (oldpitch - move > pitch)
			pitch = oldpitch - move;
	}
	
	oldyaw = yaw;
	oldpitch = pitch;

	cl.viewent.angles[YAW] = r_refdef.viewangles[YAW] + yaw;
	cl.viewent.angles[PITCH] = -(r_refdef.viewangles[PITCH] + pitch);

	// joe: this makes it fix when strafing
	cl.viewent.angles[ROLL] = r_refdef.viewangles[ROLL];

	if (v_idlescale.value)
	{
		cl.viewent.angles[ROLL] -= v_idlescale.value * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
		cl.viewent.angles[PITCH] -= v_idlescale.value * sin(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
		cl.viewent.angles[YAW] -= v_idlescale.value * sin(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
	}
}

/*
==============
V_BoundOffsets
==============
*/
void V_BoundOffsets (void)
{
	entity_t	*ent;

	if(cls.demoplayback && freemoving.value) //R00k - for freemoving
		ent = &cls.demoentity;
	else
		ent = &cl_entities[cl.viewentity];	

	// absolutely bound refresh reletive to entity clipping hull
	// so the view can never be inside a solid wall
	r_refdef.vieworg[0] = max(r_refdef.vieworg[0], ent->origin[0] - 14);
	r_refdef.vieworg[0] = min(r_refdef.vieworg[0], ent->origin[0] + 14);
	r_refdef.vieworg[1] = max(r_refdef.vieworg[1], ent->origin[1] - 14);
	r_refdef.vieworg[1] = min(r_refdef.vieworg[1], ent->origin[1] + 14);
	r_refdef.vieworg[2] = max(r_refdef.vieworg[2], ent->origin[2] - 22);
	r_refdef.vieworg[2] = min(r_refdef.vieworg[2], ent->origin[2] + 30);
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle (void)
{
	if (!(v_idlescale.value))//R00k clear this cvar to disable this behavior...
		return;

	if ((cl.velocity[0] == 0) && (cl.velocity[1] == 0) && (cl.velocity[2] == 0)) //R00k: limit idle movement to only when "idle"...
	{
		r_refdef.viewangles[ROLL]	+= v_idlescale.value * sin(cl.time	* v_iroll_cycle.value)	* v_iroll_level.value;
		r_refdef.viewangles[PITCH]	+= v_idlescale.value * sin(cl.time	* v_ipitch_cycle.value)	* v_ipitch_level.value;
		r_refdef.viewangles[YAW]	+= v_idlescale.value * sin(cl.time	* v_iyaw_cycle.value)	* v_iyaw_level.value;
	}
}

/* 
======
View_ModelDrift -- Eukos from OpenKatana
Adds a delay to the view model (such as a weapon) in the players view.
======
*/
void View_ModelDrift(vec3_t vOrigin,vec3_t vAngles,vec3_t vOldAngles)
{
			int	i;
			float	fScale,fSpeed,fDifference;
	static	vec3_t	svLastFacing;
			vec3_t	vForward, vRight,vUp, vDifference;
	
	AngleVectors(vAngles,vForward,vRight,vUp);

	if (host_frametime != 0.0f)
	{
		VectorSubtract(vForward,svLastFacing,vDifference);
		fSpeed = 6.0f;
		fDifference = VectorLength(vDifference);
		
		if ((fDifference > cl_gun_drift.value ))
			fSpeed *= fScale = fDifference/cl_gun_drift.value;
		
		for (i = 0; i < 3; i++)
			svLastFacing[i] += vDifference[i]*(fSpeed*host_frametime);
			VectorNormalize(svLastFacing);

		for (i = 0; i < 3; i++)
		{
			vOrigin[i] += (vDifference[i]*-1.0f)*5.0f;
			vAngles[ROLL] += vDifference[YAW];
		}
	}
}
/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll (void)
{
	float	side;
		
	side = V_CalcRoll (cl_entities[cl.viewentity].angles, cl.velocity);
	r_refdef.viewangles[ROLL] += side;

	if (v_dmg_time > 0)
	{
		r_refdef.viewangles[ROLL] += v_dmg_time / v_kicktime.value * v_dmg_roll;
		r_refdef.viewangles[PITCH] += v_dmg_time / v_kicktime.value * v_dmg_pitch;
		v_dmg_time -= host_frametime;
	}

	if (cl.stats[STAT_HEALTH] <= 0)
	{
		r_refdef.viewangles[ROLL] = 80;	// dead view angle
	}
	else
	{
		cl.stats[STAT_TEMP] = 0;
	}
}

void V_AddViewWeapon (float bob)
{
	int		i;
	vec3_t		forward, right, up, vOldAngles;
	entity_t	*view;
	
	float bspeed;
	float t;
	double xyspeed, bobfall;

	view = &cl.viewent;

	// angles
	view->angles[YAW] = r_refdef.viewangles[YAW];
	view->angles[PITCH] = -r_refdef.viewangles[PITCH];
	view->angles[ROLL] = r_refdef.viewangles[ROLL];

	// origin
	AngleVectors (r_refdef.viewangles, forward, right, up);

	for (i=0 ; i<3 ; i++)
		r_refdef.vieworg[i] += scr_ofsx.value * forward[i] + scr_ofsy.value * right[i] + scr_ofsz.value * up[i];

	V_BoundOffsets ();

	// set up gun position
	VectorCopy (cl.lerpangles, view->angles); // JPG - viewangles -> lerpangles
	CalcGunAngle ();

	VectorCopy (r_refdef.vieworg, view->origin);

	if ((cl.stats[STAT_ACTIVEWEAPON] != IT_AXE) && (cl_gun_offset.value == 2))//Right-Handed
	{
		if (cl.stats[STAT_ACTIVEWEAPON] == IT_ROCKET_LAUNCHER)
			VectorMA (view->origin, -4, forward, view->origin);			
		else
			VectorMA (view->origin, -2, forward, view->origin);	
		VectorMA (view->origin, 2, right, view->origin);	
		cl.viewent.angles[YAW] += 2;
	}
	else if ((cl.stats[STAT_ACTIVEWEAPON] != IT_AXE) && (cl_gun_offset.value == 1))//Left-Handed
	{
		if (cl.stats[STAT_ACTIVEWEAPON] == IT_ROCKET_LAUNCHER)
			VectorMA (view->origin, -4, forward, view->origin);			
		else
			VectorMA (view->origin, -2, forward, view->origin);	
		VectorMA (view->origin, -2, right, view->origin);	
			cl.viewent.angles[YAW] -= 2;
	}

	if (VectorLength(cl.velocity) > 1)
	{
		if (bob) 
		{
			xyspeed = sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]);
	
			if (cl.onground)
			{
				if (cl.time - cl.hitgroundtime < 0.2)
				{
					//Sajt: just hit the ground, speed the bob back up over the next 0.2 seconds
					t = cl.time - cl.hitgroundtime;
					t = bound(0, t, 0.2);
					t *= 5;
				}
				else
					t = 1;
			}
			else
			{
				//Sajt: recently left the ground, slow the bob down over the next 0.2 seconds
				t = cl.time - cl.lastongroundtime;
				t = 0.2 - bound(0, t, 0.2);
				t *= 5;
			}
	
			bspeed = bound (0, xyspeed, 400) * 0.01f;		
			AngleVectors (cl.viewent.angles, forward, right, up);
			bob = bspeed * 0.1 * sin ((cl.time * 7)) * t;
			VectorMA (cl.viewent.origin, bob * 0.4, forward, cl.viewent.origin);
			bob = bspeed * 0.1 * sin ((cl.time * 7)) * t;
			VectorMA (cl.viewent.origin, bob, right, cl.viewent.origin);
			bob = bspeed * 0.05 * cos ((cl.time * 7) * 2) * t;
			VectorMA (cl.viewent.origin, bob , up, cl.viewent.origin);	

			// fall bobbing code from DarkPlaces
			// causes the view to swing down and back up when touching the ground
			if (cl_bobfall.value && cl_bobfall_scale.value)
			{
				if (!cl.onground)
				{
					cl.bobfall_speed = bound(-300, cl.velocity[2], 0) * ((cl.ctime - cl.oldtime) * cl_bobfall_scale.value);
					
					if (cl.velocity[2] < -200)
						cl.bobfall_swing = 1;
					else
						cl.bobfall_swing = 0;
				}
				else
				{
					cl.bobfall_swing = max(0, cl.bobfall_swing - 3 * (cl.ctime - cl.oldtime));

					bobfall = sin(M_PI * cl.bobfall_swing) * cl.bobfall_speed;
					
					if (cl_viewbob.value)
						r_refdef.vieworg[2] += bobfall * 0.5;

					cl.viewent.origin[2] += bobfall * 0.1;
				}
			}
		}
	}
	else
	{	//R00k wiggle the gun around for idle movement..
		
		if ((cl_gun_idle_movement.value) && (sv.active))//not smooth online FIXME!
		{
			if (cl.onground)
			{
				if (cl.ctime - cl.hitgroundtime < 0.2)
				{
					t = cl.ctime - cl.hitgroundtime;
					t = bound(0, t, 0.2);
					t *= 5;
				}
				else
					t = 1;
			}
			else
			{
				t = cl.ctime - cl.lastongroundtime;
				t = 0.2 - bound(0, t, 0.2);
				t *= 5;
			}


			VectorMA (view->origin, (0.3 * sin ((cl.ctime * 0.3)) * t), forward, view->origin);
			VectorMA (view->origin, (0.3 * cos ((cl.ctime * 0.5)) * t), up, view->origin);
			VectorMA (view->origin, (0.2 * cos ((cl.ctime * 0.3)) * t), right, view->origin);	

			cl.viewent.angles[PITCH] += (0.5 * sin(cl.ctime* 0.5) * t);
			cl.viewent.angles[YAW] += (0.5 * cos(cl.ctime* 0.5) * t);
			cl.viewent.angles[ROLL] += (0.5 * sin(cl.ctime* 0.5) * t);

		}
	}

	// fudge position around to keep amount of weapon visible roughly equal with different FOV
//R00k: removed this. The player can use r_drawviewmodelsize instead, or cl_gun_fovscale 1 to make it automatic.
/*
	if (scr_viewsize.value == 110)
		view->origin[2] += 1;
	else if (scr_viewsize.value == 100)
		view->origin[2] += 2;
	else if (scr_viewsize.value == 90)
		view->origin[2] += 1;
	else if (scr_viewsize.value == 80)
		view->origin[2] += 0.5;
*/
	if (cl_gun_drift.value)
		View_ModelDrift(view->origin,view->angles,vOldAngles);

	view->model = cl.model_precache[cl.stats[STAT_WEAPON]];
	view->frame = cl.stats[STAT_WEAPONFRAME];
	view->colormap = 0;
}

/*
==================
V_CalcIntermissionRefdef
==================
*/
void V_CalcIntermissionRefdef (void)
{
	entity_t	*ent, *view;
	float		old;

	// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
	// view is the weapon model (only visible from inside body)
	view = &cl.viewent;

	VectorCopy (ent->origin, r_refdef.vieworg);
	VectorCopy (ent->angles, r_refdef.viewangles);
	view->model = NULL;

	// always idle in intermission
	old = v_idlescale.value;
	v_idlescale.value = 1;
	V_AddIdle ();
	v_idlescale.value = old;
}

/*
==================
V_CalcRefdefz
==================
*/
void V_CalcRefdef (void)
{
	entity_t		*ent, *view;
	vec3_t			forward;
	float			bob;
	extern	float	punchangle;

	V_DriftPitch ();

	// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];

	// view is the weapon model (only visible from inside body)
	view = &cl.viewent;
	
	// transform the view offset by the model's matrix to get the offset from
	// model origin for the view
	// JPG - viewangles -> lerpangles
	ent->angles[YAW] = cl.lerpangles[YAW];	
	ent->angles[PITCH] = -cl.lerpangles[PITCH];
										
	bob = V_CalcBob ();

	if (cl.onground)
	{
		if (!cl.oldonground)
			cl.hitgroundtime = cl.time;
		cl.lastongroundtime = cl.time;
	}

	cl.oldonground = cl.onground;
	
	// set up the refresh position
	VectorCopy (ent->origin, r_refdef.vieworg);

	if (cl.inwater) //R00k
	{
		// never let it sit exactly on a node line, because a water plane can
		// dissapear when viewed with the eye exactly on it.
		// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
		r_refdef.vieworg[0] += 0.03125;	// Tomaz - Speed
		r_refdef.vieworg[1] += 0.03125;	// Tomaz - Speed
		r_refdef.vieworg[2] += 0.03125;	// Tomaz - Speed
	}

	// add view height
	r_refdef.vieworg[2] += cl.viewheight + (bound (-7, v_viewheight.value, 4));	// normal view height
	
	if (cl_viewbob.value)
		r_refdef.vieworg[2] += bob;

	r_refdef.vieworg[2] += cl.crouch;

	// set up refresh view angles	
	VectorCopy (cl.lerpangles, r_refdef.viewangles); // JPG - viewangles -> lerpangles
	V_CalcViewRoll ();
	V_AddIdle ();

	if (v_gunkick.value)//R00k from eZQuake
	{
		// add weapon kick offset
		AngleVectors (r_refdef.viewangles, forward, NULL, NULL);
		VectorMA (r_refdef.vieworg, punchangle, forward, r_refdef.vieworg);

		// add weapon kick angle
		r_refdef.viewangles[PITCH] += punchangle * 0.5;
	}

	if (cl.stats[STAT_HEALTH] <= 0)
		r_refdef.viewangles[ROLL] = 80;	// dead view angle

	V_AddViewWeapon (bob);

	if (chase_active.value)
	{
		Chase_Update ();
	}
}

/*
==============
SCR_DrawClock
==============
*/

void SCR_DrawLocalTime (void)
{
	int	x, y;
	char	str[80], *format;
	time_t	t;
	struct tm *ptm;

	if (scr_viewsize.value >= 120.0)
		return;

	format = 
		((int)scr_clock.value == 2) ? "%H:%M:%S" : 
		((int)scr_clock.value == 3) ? "%a %b %I:%M:%S %p" : 
		((int)scr_clock.value == 4) ? "%a %b %H:%M:%S" : "%I:%M:%S %p";

	time (&t);

	if ((ptm = localtime(&t)))
		strftime (str, sizeof(str) - 1, format, ptm);
	else
		strcpy(str, "#bad date#");

	x = ELEMENT_X_COORD(scr_clock);
	y = ELEMENT_Y_COORD(scr_clock);
	Draw_String (x, y, str,1);
}

/*
==============
SCR_DrawFPS
==============
*/
void SCR_DrawFPS (void)
{
	int		x, y;
	char	str[80];
	float	t;
	
	static	float	lastfps;
	static	double	lastframetime;
	extern	int		fps_count;

	if (!show_fps.value)
		return;

	if (scr_viewsize.value >= 120.0)
		return;

	t = Sys_DoubleTime ();
	if ((t - lastframetime) >= 1.0)
	{
		lastfps = fps_count / (t - lastframetime);
		fps_count = 0;
		lastframetime = t;
	}

	Q_snprintfz (str, sizeof(str), "%3.1f%s", lastfps + 0.05, show_fps.value == 2 ? " FPS" : "");
	x = ELEMENT_X_COORD(show_fps);
	y = ELEMENT_Y_COORD(show_fps);
	Draw_String (x, y, str, 1);
}

void SCR_DrawREC (void)
{
	int		x, y;
	char	str[7]="[‹rec]";

	if (!show_rec.value)
		return;

	if (scr_viewsize.value >= 120.0)
		return;

	x = ELEMENT_X_COORD(show_rec);
	y = ELEMENT_Y_COORD(show_rec);

	Draw_String (x, y, str, 1);
}

void SCR_DrawLocName (void)
{	
	int		x, y;
	char	st[32], loc[16];

	if (!show_locname.value)
	return;

	if (scr_viewsize.value >= 120.0)
		return;

	if (cl.time - cl.last_loc_time > 1)
	{
		sprintf (st, "%s", LOC_GetLocation (cl_entities[cl.viewentity].origin));
		cl.last_loc_time = cl.time;	
	}
	else
	{	
		sprintf (st, "%s",cl.last_loc_name);
	}
	
	x = (vid.width - strlen(st) * 8) / 2;
	y = vid.height - sb_lines - 8;
	
	Draw_Character(x - 8, y+1, 16,false);//[
	Draw_String (x, y, st,1);	
	Draw_Character(x+(strlen(st)*8), y+1, 17,false);//]	

	if (show_locname.value == 2)
	{
		sprintf (loc, "%5.1f %5.1f %5.1f", cl_entities[cl.viewentity].origin[0], cl_entities[cl.viewentity].origin[1], cl_entities[cl.viewentity].origin[2]);				

		x = (vid.width - strlen(loc) * 8) / 2;
		y = vid.height - sb_lines - 18;

		Draw_Character(x - 8, y+1, 29,false);//(
		Draw_String (x, y, loc,1);	
		Draw_Character(x+(strlen(loc)*8), y+1, 31,false);//)
	}
}


/*
==============
SCR_DrawSpeed
==============
*/
// joe: from [sons]Quake
void SCR_DrawSpeed (void)
{
	int		x = 0, y = 0;
	char	st[8];
	vec3_t	vel;
	float	speed, vspeed, speedunits;
	static	float	maxspeed = 0;
	static	float	display_speed = -1;
	static	double	lastrealtime = 0;

	if (!show_speed.value)
		return;

	if (scr_viewsize.value >= 120.0)
		return;

	if (lastrealtime > realtime)
	{
		lastrealtime = 0;
		display_speed = -1;
		maxspeed = 0;
	}

	VectorCopy (cl.velocity, vel);
	vspeed = vel[2];
	vel[2] = 0;
	speed = VectorLength (vel);

	if (speed > maxspeed)
		maxspeed = speed;

	if (display_speed >= 0)
	{
		sprintf (st, "%3d", (int)display_speed);

		x = vid.width/2 - 80;

		if (scr_viewsize.value >= 120)
			y = vid.height - 16;

		if (scr_viewsize.value < 120)
			y = vid.height - 8*5;

		if (scr_viewsize.value < 110)
			y = vid.height - 8*8;

		if (cl.intermission)
			y = vid.height - 16;

		Draw_Fill (x, y-1, 160, 1, 10);
		Draw_Fill (x, y+9, 160, 1, 10);
		Draw_Fill (x+32, y-2, 1, 13, 10);
		Draw_Fill (x+64, y-2, 1, 13, 10);
		Draw_Fill (x+96, y-2, 1, 13, 10);
		Draw_Fill (x+128, y-2, 1, 13, 10);

		Draw_Fill (x, y, 160, 9, 52);

		speedunits = display_speed;
		if (display_speed <= 500)
			Draw_Fill (x, y, (int)(display_speed/3.125), 9, 100);
		else 
		{   
			while (speedunits > 500)
				speedunits -= 500;
			Draw_Fill (x, y, (int)(speedunits/3.125), 9, 68);
		}
		Draw_String (x + 36 - strlen(st) * 8, y, st,1);
	}

	if (realtime - lastrealtime >= 0.1)
	{
		lastrealtime = realtime;
		display_speed = maxspeed;
		maxspeed = 0;
	}
}

void SCR_DrawHealthBar (void)
{
	int		x, y;
	char	st[8];
	float	health, healthunits;
	static	float	maxhealth = 0;
	static	float	display_health = -1;
	static	double	lastrealtime = 0;

	if (!show_healthbar.value)
		return;

	if (lastrealtime > realtime)
	{
		lastrealtime = 0;
		display_health = -1;
		maxhealth = 0;
	}

	health = cl.stats[STAT_HEALTH];
	health = CLAMP(0, health, 999);

	if (health > maxhealth)
		maxhealth = health;

	if (cl.stats[STAT_HEALTH] > 0)
	{
		sprintf (st, "%3d", (int)cl.stats[STAT_HEALTH]);

		x = vid.width/2 + show_healthbar_x.value;
		y = vid.height/2 + show_healthbar_y.value;

		healthunits = display_health;

		if (health < 256)//observer's health goes to 1000 to display 000 on hud
		{
			Draw_AlphaFill (x, y, (int)(display_health/2.5), 4, 254, (health / 255));
			Draw_String (x, y + 2, st, 1);
		}
	}

	if (realtime - lastrealtime >= 0.1)
	{
		lastrealtime = realtime;
		display_health = maxhealth;
		maxhealth = 0;
	}
}

void SCR_DrawArmorBar (void)
{
	int		x, y;
	char	st[8];
	float	armor, armorunits;
	static	float	maxarmor = 0;
	static	float	display_armor = -1;
	static	double	lastrealtime = 0;

	if (!show_armorbar.value)
		return;

	if (lastrealtime > realtime)
	{
		lastrealtime = 0;
		display_armor = -1;
		maxarmor = 0;
	}

	armor = cl.stats[STAT_ARMOR];

	if (armor > maxarmor)
		maxarmor = armor;

	if (cl.stats[STAT_ARMOR] > 0)
	{
		sprintf (st, "%3d", (int)cl.stats[STAT_ARMOR]);

		x = vid.width / 2 + show_armorbar_x.value;
		y = vid.height/ 2 + show_armorbar_y.value;

		armorunits = display_armor;
/*
		if (cl.items & IT_INVULNERABILITY)
		{
			sprintf (st, "%3d", 666);
			Draw_AlphaFill (x, y, (int)(display_armor/2.5), 4, 251, 1);
		}
		else*/
		{
			if (cl.items & IT_ARMOR3)
				Draw_AlphaFill (x, y, (int)(display_armor/2.5), 4, 251, (armor / 200));
			else if (cl.items & IT_ARMOR2)
				Draw_AlphaFill (x, y, (int)(display_armor/2.5), 4, 192, (armor / 150));
			else if (cl.items & IT_ARMOR1)
				Draw_AlphaFill (x, y, (int)(display_armor/2.5), 4, 63, (armor / 100));
		}
		Draw_String (x, y + 2, st, 1);
	}

	if (realtime - lastrealtime >= 0.1)
	{
		lastrealtime = realtime;
		display_armor = maxarmor;
		maxarmor = 0;
	}
}

void SCR_DrawAmmoBar (void)
{
	int		x, y;
	char	st[8];
	float	ammo, ammounits;
	static	float	maxammo = 0;
	static	float	display_ammo = -1;
	static	double	lastrealtime = 0;

	if (!show_ammobar.value)
		return;

	if (lastrealtime > realtime)
	{
		lastrealtime = 0;
		display_ammo = -1;
		maxammo = 0;
	}

	ammo = cl.stats[STAT_AMMO];

	if (ammo > maxammo)
		maxammo = ammo;

	if (cl.stats[STAT_AMMO] > 0)
	{
		sprintf (st, "%3d", (int)cl.stats[STAT_AMMO]);

		x = vid.width / 2 + show_ammobar_x.value;
		y = vid.height/ 2 + show_ammobar_y.value;

		ammounits = display_ammo;

		Draw_AlphaFill (x, y, (int)(display_ammo/2.5), 4, 63, 0.8);
		Draw_String (x , y + 2, st, 1);
	}

	if (realtime - lastrealtime >= 0.1)
	{
		lastrealtime = realtime;
		display_ammo = maxammo;
		maxammo = 0;
	}
}
/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
void V_RenderView (void)
{
	if (con_forcedup)
		return;

// don't allow cheats in multiplayer
	if ((scr_ofsx.value)||(scr_ofsy.value)||(scr_ofsz.value))//R00k: Don't spam the console buffer!
	{
		if (cl.maxclients > 1)
		{
			Cvar_Set ("scr_ofsx", "0");
			Cvar_Set ("scr_ofsy", "0");
			Cvar_Set ("scr_ofsz", "0");
		}
	}

	if (cls.state != ca_connected)
	{
#ifdef GLQUAKE
		V_CalcBlend ();
#endif
		return;
	}

	if (cl.intermission)	// intermission / finale rendering
	{
		V_CalcIntermissionRefdef ();	
	}
	else
	{
		if (!cl.paused)
			V_CalcRefdef ();
	}

#ifndef GLQUAKE
	if (lcd_x.value)
	{
		// render two interleaved views
		int	i;

		vid.rowbytes <<= 1;
		vid.aspect *= 0.5;

		r_refdef.viewangles[YAW] -= lcd_yaw.value;
		for (i=0 ; i<3 ; i++)
			r_refdef.vieworg[i] -= right[i] * lcd_x.value;
		R_RenderView ();

		vid.buffer += vid.rowbytes >> 1;

		r_refdef.viewangles[YAW] += lcd_yaw.value * 2;
		for (i=0 ; i<3 ; i++)
			r_refdef.vieworg[i] += 2 * right[i] * lcd_x.value;
	
		R_RenderView ();

		vid.buffer -= vid.rowbytes >> 1;

		r_refdef.vrect.height <<= 1;

		vid.rowbytes >>= 1;
		vid.aspect *= 2;
	}
	else
#endif
	R_RenderView ();
}

//============================================================================

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("v_cshift", V_cshift_f);
	Cmd_AddCommand ("bf", V_BonusFlash_f);
	Cmd_AddCommand ("centerview", V_StartPitchDrift);

#ifndef GLQUAKE
	Cvar_RegisterVariable (&lcd_x);
	Cvar_RegisterVariable (&lcd_yaw);
#endif
	Cvar_RegisterVariable (&v_viewheight);
	Cvar_RegisterVariable (&v_centermove);
	Cvar_RegisterVariable (&v_centerspeed);

	Cvar_RegisterVariable (&v_iyaw_cycle);
	Cvar_RegisterVariable (&v_iroll_cycle);
	Cvar_RegisterVariable (&v_ipitch_cycle);
	Cvar_RegisterVariable (&v_iyaw_level);
	Cvar_RegisterVariable (&v_iroll_level);
	Cvar_RegisterVariable (&v_ipitch_level);
	Cvar_RegisterVariable (&v_idlescale);

	Cvar_RegisterVariable (&crosshair);
	Cvar_RegisterVariable (&crosshaircolor);
	Cvar_RegisterVariable (&crosshairsize);
	Cvar_RegisterVariable (&cl_crossx);
	Cvar_RegisterVariable (&cl_crossy);

	Cvar_RegisterVariable (&scr_ofsx);
	Cvar_RegisterVariable (&scr_ofsy);
	Cvar_RegisterVariable (&scr_ofsz);

	Cvar_RegisterVariable (&scr_clock);
	Cvar_RegisterVariable (&scr_clock_x);
	Cvar_RegisterVariable (&scr_clock_y);

	Cvar_RegisterVariable (&show_speed);

	Cvar_RegisterVariable (&show_fps);
	Cvar_RegisterVariable (&show_fps_x);
	Cvar_RegisterVariable (&show_fps_y);

	Cvar_RegisterVariable (&show_healthbar);
	Cvar_RegisterVariable (&show_healthbar_x);
	Cvar_RegisterVariable (&show_healthbar_y);

	Cvar_RegisterVariable (&show_armorbar);
	Cvar_RegisterVariable (&show_armorbar_x);
	Cvar_RegisterVariable (&show_armorbar_y);

	Cvar_RegisterVariable (&show_ammobar);
	Cvar_RegisterVariable (&show_ammobar_x);
	Cvar_RegisterVariable (&show_ammobar_y);

	Cvar_RegisterVariable (&show_rec);
	Cvar_RegisterVariable (&show_rec_x);
	Cvar_RegisterVariable (&show_rec_y);

	Cvar_RegisterVariable (&show_ping);
	Cvar_RegisterVariable (&show_ping_x);
	Cvar_RegisterVariable (&show_ping_y);

	Cvar_RegisterVariable (&show_locname);
	Cvar_RegisterVariable (&cl_rollspeed);
	Cvar_RegisterVariable (&cl_rollangle);
	Cvar_RegisterVariable (&cl_viewbob);
	Cvar_RegisterVariable (&cl_bob);
	Cvar_RegisterVariable (&cl_bobcycle);
	Cvar_RegisterVariable (&cl_bobup);
	Cvar_RegisterVariable (&cl_bobfall);
	Cvar_RegisterVariable (&cl_bobfall_scale);

	Cvar_RegisterVariable (&cl_gun_offset);
	Cvar_RegisterVariable (&cl_gun_idle_movement);
	Cvar_RegisterVariable (&cl_gun_drift);
	Cvar_RegisterVariable (&v_kicktime);
	Cvar_RegisterVariable (&v_kickroll);
	Cvar_RegisterVariable (&v_kickpitch);		
	Cvar_RegisterVariable (&v_gunkick);


	Cvar_RegisterVariable (&v_bonusflash);
	Cvar_RegisterVariable (&v_contentblend);
	Cvar_RegisterVariable (&v_damagecshift);
	Cvar_RegisterVariable (&v_quadcshift);
	Cvar_RegisterVariable (&v_suitcshift);
	Cvar_RegisterVariable (&v_ringcshift);
	Cvar_RegisterVariable (&v_pentcshift);
#ifdef GLQUAKE
	Cvar_RegisterVariable (&v_dlightcshift);
	Cvar_RegisterVariable (&gl_cshiftpercent);
//	Cvar_RegisterVariable (&gl_hwblend);
#endif

	Cvar_RegisterVariable (&v_gamma);
	Cvar_RegisterVariable (&v_contrast);

#ifdef GLQUAKE
	Cmd_AddLegacyCommand ("gamma", v_gamma.name);
	Cmd_AddLegacyCommand ("contrast", v_contrast.name);
#endif

#ifndef GLQUAKE
	BuildGammaTable (v_gamma.value, v_contrast.value);
#endif
}
