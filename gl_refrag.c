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
// r_efrag.c

#include "quakedef.h"

mnode_t	*r_pefragtopnode;

//===========================================================================
// mh - extended efrags - begin
#define EXTRA_EFRAGS   32

void R_GetMoreEfrags (void)
{
   int i;

   cl.free_efrags = (efrag_t *) Hunk_Alloc (EXTRA_EFRAGS * sizeof (efrag_t));

   for (i = 0; i < EXTRA_EFRAGS - 1; i++)
      cl.free_efrags[i].entnext = &cl.free_efrags[i + 1];

   cl.free_efrags[i].entnext = NULL;
}
// mh - extended efrags - end

/*
===============================================================================

			ENTITY FRAGMENT FUNCTIONS

===============================================================================
*/

efrag_t		**lastlink;

vec3_t		r_emins, r_emaxs;

entity_t	*r_addent;


/*
================
R_RemoveEfrags

Call when removing an object from the world or moving it to another position
================
*/
void R_RemoveEfrags (entity_t *ent)
{
	efrag_t		*ef, *old, *walk, **prev;

	ef = ent->efrag;

	while (ef)
	{
		prev = &ef->leaf->efrags;
		while (1)
		{
			walk = *prev;
			if (!walk)
				break;
			if (walk == ef)
			{	// remove this fragment
				*prev = ef->leafnext;
				break;
			}
			else
				prev = &walk->leafnext;
		}

		old = ef;
		ef = ef->entnext;

	// put it on the free list
		old->entnext = cl.free_efrags;
		cl.free_efrags = old;
	}

	ent->efrag = NULL; 
}

/*
===================
R_SplitEntityOnNode
===================
*/
void R_SplitEntityOnNode (mnode_t *node)
{
	int		sides;
	efrag_t		*ef;
	mplane_t	*splitplane;
	mleaf_t		*leaf;

	if (node->contents == CONTENTS_SOLID)
		return;

// add an efrag if the node is a leaf

	if ( node->contents < 0)
	{
		if (!r_pefragtopnode)
			r_pefragtopnode = node;

		leaf = (mleaf_t *)node;

// grab an efrag off the free list
		ef = cl.free_efrags;
		if (!ef)
		{
		 // mh - extended efrags - begin
		 R_GetMoreEfrags ();
		 ef = cl.free_efrags;
		 // mh - extended efrags - end
		}		
		cl.free_efrags = cl.free_efrags->entnext;

		ef->entity = r_addent;

// add the entity link	
		*lastlink = ef;
		lastlink = &ef->entnext;
		ef->entnext = NULL;

// set the leaf links
		ef->leaf = leaf;
		ef->leafnext = leaf->efrags;
		leaf->efrags = ef;

		return;
	}

// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(r_emins, r_emaxs, splitplane);

	if (sides == 3)
	{
	// split on this plane
	// if this is the first splitter of this bmodel, remember it
		if (!r_pefragtopnode)
			r_pefragtopnode = node;
	}

// recurse down the contacted sides
	if (sides & 1)
		R_SplitEntityOnNode (node->children[0]);

	if (sides & 2)
		R_SplitEntityOnNode (node->children[1]);
}

/*
===========
R_AddEfrags
===========
*/
void R_AddEfrags (entity_t *ent)
{
	int	i;
	model_t	*entmodel;

	if (!ent->model)
		return;

	if (ent == &r_worldentity)
		return;		// never add the world

	r_addent = ent;

	lastlink = &ent->efrag;
	r_pefragtopnode = NULL;

	entmodel = ent->model;

	for (i=0 ; i<3 ; i++)
	{
		r_emins[i] = ent->origin[i] + entmodel->mins[i];
		r_emaxs[i] = ent->origin[i] + entmodel->maxs[i];
	}

	R_SplitEntityOnNode (cl.worldmodel->nodes);

	ent->topnode = r_pefragtopnode;
}

/*
================
R_StoreEfrags

// FIXME: a lot of this goes away with edge-based
================
*/
extern	cvar_t	r_drawflame;//R00k
void R_StoreEfrags (efrag_t **ppefrag)
{
	entity_t	*pent;
	model_t		*model;
	efrag_t		*pefrag;

	for (pefrag = *ppefrag ; pefrag ; pefrag = pefrag->leafnext)
	{
		pent = pefrag->entity;
		model = pent->model;

		if (model->modhint == MOD_FLAME && !r_drawflame.value)
			continue;

		switch (model->type)
		{
		case mod_alias:
		case mod_md3:
		case mod_brush:
		case mod_sprite:
		case mod_spr32:
			if (pent->visframe != r_framecount)
			{
				if (cl_numvisedicts < MAX_VISEDICTS)
					cl_visedicts[cl_numvisedicts++] = pent;

				if (!pent->transparency)
					pent->transparency = 1;

			// mark that we've recorded this entity for this frame
				pent->visframe = r_framecount;
			}
			break;

		default:	
			Sys_Error ("R_StoreEfrags: Bad entity type %d\n", model->type);
		}
	}
}
