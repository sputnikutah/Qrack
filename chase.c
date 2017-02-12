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
// chase.c -- chase camera code

#include "quakedef.h"

cvar_t	chase_back	= {"chase_back"	, "100"};
cvar_t	chase_up	= {"chase_up"	, "32"};
cvar_t	chase_right = {"chase_right", "0"};

cvar_t  chase_roll	= {"chase_roll"	, "0"};
cvar_t  chase_yaw 	= {"chase_yaw"	, "180"};
cvar_t  chase_pitch = {"chase_pitch", "45"};

cvar_t	chase_active	= {"chase_active", "0", true};

vec3_t	chase_dest;

void Chase_Init (void)
{
	Cvar_RegisterVariable (&chase_back);
	Cvar_RegisterVariable (&chase_up);
	Cvar_RegisterVariable (&chase_right);
	Cvar_RegisterVariable (&chase_active);
    Cvar_RegisterVariable (&chase_roll);
    Cvar_RegisterVariable (&chase_yaw);
    Cvar_RegisterVariable (&chase_pitch);
}
//R00k unused
/*
void Chase_Reset (void)
{
	// for respawning and teleporting

	int	i;
	vec3_t	forward, up, right;

	AngleVectors (cl.lerpangles, forward, right, up);	

	for (i=0 ; i<3 ; i++)
		chase_dest[i] = r_refdef.vieworg[i] - forward[i]*12;//	start position 12 units behind head

	chase_dest[2] = r_refdef.vieworg[2] + 22;	
}
*/
void TraceLine (vec3_t start, vec3_t end, vec3_t impact)
{
	trace_t	trace;
	qboolean result;

	memset (&trace, 0, sizeof(trace));
	trace.fraction = 1;

	//result is true if end is empty...
	result = SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);
	
	if (!result)//hit something
	{
		VectorCopy (trace.endpos, impact);	
	}
	else
		VectorCopy (end, impact);	
}

void Chase_Update (void)
{
	int		i;
	float	dist, absdist;
	vec3_t	forward, up, right, dest, stop;
	float	alpha = 1, alphadist = 1;
	extern	cvar_t	chase_transparent;
	
	if (chase_active.value == 2)//custom chase camera by frag.machine
	{
        chase_dest[0] = r_refdef.vieworg[0] + chase_back.value;
        chase_dest[1] = r_refdef.vieworg[1] + chase_right.value;
        chase_dest[2] = r_refdef.vieworg[2] + chase_up.value;
	}
	else
	{
		AngleVectors (cl.lerpangles, forward, right, up);
		
		// calc exact destination
		for (i=0 ; i<3 ; i++)
			chase_dest[i] = r_refdef.vieworg[i] - forward[i]*chase_back.value - right[i]*chase_right.value;
		
		chase_dest[2] = r_refdef.vieworg[2] + chase_up.value;
		
		// find the spot the player is looking at
		VectorMA (r_refdef.vieworg, 4096, forward, dest);
		TraceLine (r_refdef.vieworg, dest, stop);
		
		// calculate pitch to look at the same spot from camera
		VectorSubtract (stop, r_refdef.vieworg, stop);
		dist = max(1, DotProduct(stop, forward));
		r_refdef.viewangles[PITCH] = -180 / M_PI * atan2( stop[2], dist );
	}

	TraceLine (r_refdef.vieworg, chase_dest, stop);

	if (VectorLength (stop) != 0)
	{
		VectorCopy (stop, chase_dest);//update the camera destination to where we hit the wall
		alphadist = VecLength2(r_refdef.vieworg, chase_dest);	

		if (chase_transparent.value)
		{
			absdist = abs(chase_back.value);
			alpha = bound(0,(alphadist / absdist), 1);		

			if (alpha < 0.09)
				alpha = 0;

			cl_entities[cl.viewentity].transparency = (min(alpha, chase_transparent.value));
		}
		else
			cl_entities[cl.viewentity].transparency = 1.0;

		//R00k, this prevents the camera from poking into the wall by rounding off the traceline... zNear clipping plane must be set to 1 in MYgluPerspective
		LerpVector (r_refdef.vieworg, chase_dest, 0.9f, chase_dest);
	}

	VectorCopy (chase_dest, r_refdef.vieworg);	
	
	if (chase_active.value == 2)//custom chase camera by frag.machine
	{
		r_refdef.viewangles[ROLL] = chase_roll.value;
		r_refdef.viewangles[YAW] = chase_yaw.value;
		r_refdef.viewangles[PITCH] = chase_pitch.value;
	}
}


//eof
