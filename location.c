//------------
// locations.c
//------------
// Original format by JP Grossman for Proquake
// coded for DarkPlaces by LordHavoc
// adapted/modified for Qrack by R00k

#include "quakedef.h"

location_t	*locations, *temploc;

extern	cvar_t	r_drawlocs;

void LOC_Init (void)
{
	temploc = Z_Malloc(sizeof(location_t));
	locations = Z_Malloc(sizeof(location_t));
}

void LOC_Delete(location_t *loc)
{
	location_t **ptr, **next;
	for (ptr = &locations;*ptr;ptr = next)
	{
		next = &(*ptr)->next_loc;
		if (*ptr == loc)
		{
			*ptr = loc->next_loc;
			Z_Free(loc);
		}
	}	
}

void LOC_Clear_f (void)
{
	location_t *l;
	for (l = locations;l;l = l->next_loc)
	{
		LOC_Delete(l);
	}
}

void LOC_SetLoc (vec3_t mins, vec3_t maxs, char *name)
{
	location_t *l, **ptr;
	int namelen;
	float	temp;
	int		n;
	vec_t	sum;

	if (!(name))
		return;

	namelen = strlen(name);
	l = Z_Malloc(sizeof(location_t));

	sum = 0;
	for (n = 0 ; n < 3 ; n++)
	{
		if (mins[n] > maxs[n])
		{
			temp = mins[n];
			mins[n] = maxs[n];
			maxs[n] = temp;
		}
		sum += maxs[n] - mins[n];
	}
	

	l->sum = sum;

	VectorSet(l->mins, mins[0], mins[1], mins[2]-32);	//ProQuake Locs are extended +/- 32 units on the z-plane...
	VectorSet(l->maxs, maxs[0], maxs[1], maxs[2]+32);
	Q_snprintfz (l->name, namelen + 1, name);	

	for (ptr = &locations;*ptr;ptr = &(*ptr)->next_loc);
		*ptr = l;
}
			
void LOC_StartPoint_f (void)
{
	float mins[3];

	if (cls.state != ca_connected || !cl.worldmodel)
	{
		Con_Printf("!error: No map loaded!\n");
		return;
	}

	//temploc = Z_Malloc(sizeof(location_t));
	VectorClear(temploc->mins);

	if (Cmd_Argc() < 2)
	{
		VectorSet(temploc->mins, cl_entities[cl.viewentity].origin[0], cl_entities[cl.viewentity].origin[1], cl_entities[cl.viewentity].origin[2]);
		Con_Printf("startpoint set: %.1f %.1f %.1f\n",temploc->mins[0],temploc->mins[1],temploc->mins[2]);
	}
	else
	{
		mins[0] = atof(Cmd_Argv(1));
		mins[1] = atof(Cmd_Argv(2));
		mins[2] = atof(Cmd_Argv(3));
		VectorSet(temploc->mins, mins[0], mins[1], mins[2]);
		Con_Printf("startpoint set: %.1f %.1f %.1f\n",temploc->mins[0],temploc->mins[1],temploc->mins[2]);
	}
}

void LOC_EndPoint_f (void)
{
	float maxs[3];

	if (cls.state != ca_connected || !cl.worldmodel)
	{
		Con_Printf("!error: No map loaded!\n");
		return;
	}

	if ((Cmd_Argc() != 2) && (Cmd_Argc() != 4))
	{	
			Con_Printf ("syntax: loc_endpoint \"locname\"\n");
			return;
	}
	
	if (Cmd_Argc() == 2)
	{
		Con_Printf ("using current location\n");
		maxs[0] = cl_entities[cl.viewentity].origin[0];
		maxs[1] = cl_entities[cl.viewentity].origin[1];
		maxs[2] = cl_entities[cl.viewentity].origin[2];
			
		LOC_SetLoc(temploc->mins,maxs,Cmd_Argv(1));
		Con_Printf("endpoint %s set: %.1f %.1f %.1i\n",Cmd_Argv(1), maxs[0],maxs[1],maxs[2]);
	}
	else
	{
		maxs[0] = atof(Cmd_Argv(2));
		maxs[1] = atof(Cmd_Argv(3));
		maxs[2] = atof(Cmd_Argv(4));
		LOC_SetLoc(temploc->mins,maxs,Cmd_Argv(1));
		Con_Printf("endpoint %s set: %.1f %.1f %.1i\n",Cmd_Argv(1), maxs[0],maxs[1],maxs[2]);
	}		
	VectorClear(temploc->mins);
}

qboolean LOC_LoadLocations (void)
{
	char	filename[MAX_OSPATH];
	vec3_t	mins, maxs;	
	char	name[32];

	int		i, linenumber, limit, len;
	char	*filedata, *text, *textend, *linestart, *linetext, *lineend;
	int		filesize;

	if (cls.state != ca_connected || !cl.worldmodel)
	{
		Con_Printf("!LOC_LoadLocations ERROR: No map loaded!\n");
		return false;
	}

	//LOC_Init();

	LOC_Clear_f();

	Q_snprintfz (filename, sizeof(filename), "locs/%s", sv_mapname.string);

	COM_ForceExtension (filename, ".loc");
	
	if (COM_FindFile(filename) == false)
	{
		Con_DPrintf (1,"%s not found.\n", filename);
		return false;
	}

	filedata = (char *)COM_LoadFile (filename, 0);

	if (!filedata)
	{
		Con_Printf("%s contains empty or corrupt data.\n", filename);
		return false;
	}

	filesize = strlen(filedata);

	text = filedata;
	textend = filedata + filesize;
	for (linenumber = 1;text < textend;linenumber++)
	{
		linestart = text;
		for (;text < textend && *text != '\r' && *text != '\n';text++)
			;
		lineend = text;
		if (text + 1 < textend && *text == '\r' && text[1] == '\n')
			text++;
		if (text < textend)
			text++;
		// trim trailing whitespace
		while (lineend > linestart && lineend[-1] <= ' ')
			lineend--;
		// trim leading whitespace
		while (linestart < lineend && *linestart <= ' ')
			linestart++;
		// check if this is a comment
		if (linestart + 2 <= lineend && !strncmp(linestart, "//", 2))
			continue;
		linetext = linestart;
		limit = 3;
		for (i = 0;i < limit;i++)
		{
			if (linetext >= lineend)
				break;
			// note: a missing number is interpreted as 0
			if (i < 3)
				mins[i] = atof(linetext);
			else
				maxs[i - 3] = atof(linetext);
			// now advance past the number
			while (linetext < lineend && *linetext > ' ' && *linetext != ',')
				linetext++;
			// advance through whitespace
			if (linetext < lineend)
			{
				if (*linetext == ',')
				{
					linetext++;
					limit = 6;
					// note: comma can be followed by whitespace
				}
				if (*linetext <= ' ')
				{
					// skip whitespace
					while (linetext < lineend && *linetext <= ' ')
						linetext++;
				}
			}
		}
		// if this is a quoted name, remove the quotes
		if (i == 6)
		{
			if (linetext >= lineend || *linetext != '"')
				continue; // proquake location names are always quoted
			lineend--;
			linetext++;
			len = min(lineend - linetext, (int)sizeof(name) - 1);
			memcpy(name, linetext, len);
			name[len] = 0;
			
			LOC_SetLoc(mins, maxs, name);// add the box to the list
		}
		else
			continue;
	}
	return true;
}

void LOC_Save_f (void)
{  
	FILE	*f;
	location_t *loc;
	char	filename[MAX_OSPATH];

	if (cls.state != ca_connected || !cl.worldmodel)
	{
		Con_Printf("!error: No map loaded!\n");
		return;
	}

	if (!locations)
	{
		Con_Printf("No locations defined.\n");
		return;
	}

	Q_snprintfz (filename, sizeof(filename),"%s/locs/%s.%s", com_gamedir, sv_mapname.string, "loc");

	if (!(f = fopen(filename, "wt")))
	{
		Con_Printf("!error: Cannot write (%s) file!\n",filename);
		return;
	}

	fprintf (f, "// %s.loc Generated by Qrack \n", sv_mapname.string);

	for (loc = locations;loc;loc = loc->next_loc)
	{
		if (loc)
		{
			fprintf (f, "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,\"%s\"\n", loc->mins[0], loc->mins[1], loc->mins[2], loc->maxs[0], loc->maxs[1], loc->maxs[2], loc->name);
		}
	}
	Con_Printf("%s saved.\n",filename);
	fclose (f);
}

char *LOC_GetLocation (vec3_t p)
{
	location_t *loc;
	location_t *bestloc;
	char somewhere[32];
	float dist, bestdist;

	bestloc = NULL;

	bestdist = 999999;

	for (loc = locations;loc;loc = loc->next_loc)
	{
		dist =	fabs(loc->mins[0] - p[0]) + fabs(loc->maxs[0] - p[0]) + 
				fabs(loc->mins[1] - p[1]) + fabs(loc->maxs[1] - p[1]) +
				fabs(loc->mins[2] - p[2]) + fabs(loc->maxs[2] - p[2]) - loc->sum;

		if (dist < .01)
		{
			Q_strncpyz (cl.last_loc_name, loc->name, sizeof(cl.last_loc_name));
			return cl.last_loc_name;
		}

		if (dist < bestdist)
		{
			bestdist = dist;
			bestloc = loc;
		}
	}
	
	if (bestloc)
	{
		Q_strncpyz (cl.last_loc_name, bestloc->name, sizeof(cl.last_loc_name));
		return cl.last_loc_name;
	}
	//R00k: parse compatibilty with DarkPlaces
//	sprintf (somewhere, "LOC=%5.1f:%5.1f:%5.1f", cl_entities[cl.viewentity].origin[0], cl_entities[cl.viewentity].origin[1], cl_entities[cl.viewentity].origin[2]);	
	sprintf (somewhere,"somewhere");
	Q_strncpyz (cl.last_loc_name, somewhere, sizeof(cl.last_loc_name));
	return cl.last_loc_name;
}

location_t *LOC_GetLocPointer (vec3_t p)
{
	location_t *loc;
	location_t *bestloc;

	float dist, bestdist;

	bestloc = NULL;

	bestdist = 999999;

	for (loc = locations;loc;loc = loc->next_loc)
	{
		dist =	fabs(loc->mins[0] - p[0]) + fabs(loc->maxs[0] - p[0]) + 
				fabs(loc->mins[1] - p[1]) + fabs(loc->maxs[1] - p[1]) +
				fabs(loc->mins[2] - p[2]) + fabs(loc->maxs[2] - p[2]) - loc->sum;

		if (dist < .01)
		{
			Q_strncpyz (cl.last_loc_name, loc->name, sizeof(cl.last_loc_name));
			return (loc);
		}

		if (dist < bestdist)
		{
			bestdist = dist;
			bestloc = loc;
		}
	}
	if (bestloc)
	{
		Q_strncpyz (cl.last_loc_name, bestloc->name, sizeof(cl.last_loc_name));//R00k
		return bestloc;
	}
	Q_strncpyz (cl.last_loc_name, "somewhere", sizeof(cl.last_loc_name));//R00k
	return NULL;
}

void LOC_DrawBox(float minx, float miny, float minz, float maxx, float maxy, float maxz, const float* col, qboolean wire)
{
	int i;
	float verts[8*3] =
	{
		minx, miny, minz,
		maxx, miny, minz,
		maxx, miny, maxz,
		minx, miny, maxz,
		minx, maxy, minz,
		maxx, maxy, minz,
		maxx, maxy, maxz,
		minx, maxy, maxz,
	};
	static const float dim[6] =
	{
		0.33f, 0.33f, 0.33f, 0.33f, 0.33f, 0.33f
	};
	static const unsigned char inds[6*5] =
	{
		0,  7, 6, 5, 4,
		1,  0, 1, 2, 3,
		2,  1, 5, 6, 2,
		3,  3, 7, 4, 0,
		4,  2, 6, 7, 3,
		5,  0, 4, 5, 1,
	};
	
	const unsigned char* in = inds;
	glPushMatrix(); 
	glTranslatef((6.4f)*vid.aspect, (4.8f)*vid.aspect, -16.0f);
	glDisable (GL_CULL_FACE);
	glDisable (GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glDepthMask (0);	
	if (wire == true)
	{
		glDisable (GL_DEPTH_TEST);
		glBegin(GL_LINE_STRIP);
	}
	else
		glBegin(GL_QUADS);
	
	for (i = 0; i < 6; ++i)
	{
		float d = dim[*in]; in++;
	
		glColor4f(d*col[0],d*col[1],d*col[2], col[3]);

		glVertex3fv(&verts[*in*3]); in++;
		glVertex3fv(&verts[*in*3]); in++;
		glVertex3fv(&verts[*in*3]); in++;
		glVertex3fv(&verts[*in*3]); in++;
	}

	glEnd();
	
	if (wire == true)
		glEnable(GL_DEPTH_TEST);

	glDepthMask (1);	
	glEnable (GL_CULL_FACE);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glPopMatrix(); 				
	glColor4f(1,1,1,1);
}
void R_DrawLocs (void)
{
	location_t *l;		

	static const float col[4] = { 0.25f, 0.75f, 0.25f, 0.5f };
		
	for (l = locations;l;l = l->next_loc)
	{
		if ((r_drawlocs.value == 2) || (!(strcmp(l->name,cl.last_loc_name))))//highlight Current area				
		{			
			LOC_DrawBox(l->mins[0],l->mins[1],l->mins[2],l->maxs[0],l->maxs[1],l->maxs[2], col, false);
			LOC_DrawBox(l->mins[0],l->mins[1],l->mins[2],l->maxs[0],l->maxs[1],l->maxs[2], col, true);
		}
	}
}

void LOC_DeleteCurrent_f (void)
{
	location_t *l;
	l = LOC_GetLocPointer ((cl_entities[cl.viewentity].origin));
	if (l)
		LOC_Delete(l);
}

void LOC_RenameCurrent_f (void)
{
	location_t *l;
	char *name;
	int namelen;
	
	if ((Cmd_Argc() != 2))
	{	
		Con_Printf ("!syntax error: loc_renamecurrent {location name}\n");
		return;
	}

	l = LOC_GetLocPointer ((cl_entities[cl.viewentity].origin));
	
	name = Cmd_Argv(1);
	namelen = strlen(name) + 1;

	if (l)
		Q_snprintfz (l->name, namelen, name);
	else
		Con_Printf ("!error: undefined location.\n");
}
