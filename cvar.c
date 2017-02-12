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
// cvar.c -- dynamic variable tracking

#include "quakedef.h"

cvar_t	*cvar_vars = NULL;
char	*cvar_null_string = "";

cvar_t	cfg_savevars = {"cfg_savevars", "2",true};

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (char *var_name)
{
	cvar_t	*var;
	
	for (var=cvar_vars ; var ; var=var->next)
		if (!strcmp(var_name, var->name))
			return var;

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue (char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return Q_atof (var->string);
}


/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString ( char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return cvar_null_string;
	return var->string;
}



/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable (char *partial)
{
	cvar_t	*cvar;
	int	len;

	if (!(len = strlen(partial)))
		return NULL;

	// check functions
	for (cvar = cvar_vars ; cvar ; cvar = cvar->next)
		if (!Q_strncasecmp(partial, cvar->name, len))
			return cvar->name;

	return NULL;
}


/*
============
Cvar_CompleteCountPossible
============
*/
int Cvar_CompleteCountPossible (char *partial)
{
	cvar_t	*cvar;
	int	len, c = 0;

	if (!(len = strlen(partial)))
		return 0;

	// check partial match
	for (cvar = cvar_vars ; cvar ; cvar = cvar->next)
		if (!Q_strncasecmp(partial, cvar->name, len))
			c++;

	return c;
}

/*
============
Cvar_Set
============
*/
void Cvar_Set (char *var_name, char *value)
{
	cvar_t		*var;
	qboolean	changed;
	static qboolean	changing = false;
	
	if (!(var = Cvar_FindVar(var_name)))
	{	// there is an error in C code if this happens
		Con_Printf ("Cvar_Set: variable %s not found\n", var_name);
		return;
	}

	changed = strcmp (var->string, value);
	
	if (var->OnChange && !changing)
	{
		changing = true;
		if (var->OnChange(var, value))
		{
			changing = false;
			return;
		}
		changing = false;
	}

	Z_Free (var->string);	// free the old value string
	
	var->string = CopyString (value);
	var->value = Q_atof (var->string);

	if (var->server && changed && sv.active)
	{
			SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", var->name, var->string);
	}

	// joe, from ProQuake: rcon (64 doesn't mean anything special, but we need some extra space because NET_MAXMESSAGE == RCON_BUFF_SIZE)
	if (rcon_active && (rcon_message.cursize < rcon_message.maxsize - strlen(var->name) - strlen(var->string) - 64))
	{
		rcon_message.cursize--;
		MSG_WriteString (&rcon_message, va("\"%s\" set to \"%s\"\n", var->name, var->string));
	}

	if (!strcmp(var_name, "pq_lag"))
	{
		var->value = bound (0, var->value, 400);
		Cbuf_AddText(va("say \"%cping +%d%c\"\n", 157, (int) var->value, 159));
	}
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue (char *var, float value)
{
	char	val[128];
	int	i;
	
	Q_snprintfz (val, sizeof(val), "%f", value);

	for (i = strlen(val) - 1 ; i > 0 && val[i] == '0' ; i--)
		val[i] = 0;
	if (val[i] == '.')
		val[i] = 0;

	Cvar_Set (var, val);
}



/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable (cvar_t *var)
{
	cvar_t	*cursor,*prev; //johnfitz -- sorted list insert
// first check to see if it has already been defined
	if (Cvar_FindVar(var->name))
	{
		Con_Printf ("Can't register variable %s, already defined\n", var->name);
		return;
	}
	
// check for overlap with a command
	if (Cmd_Exists(var->name))
	{
		Con_Printf ("Cvar_RegisterVariable: %s is a command\n", var->name);
		return;
	}
		
	var->defaultvalue = CopyString (var->string);

// copy the value off, because future sets will Free it
	var->string = CopyString (var->string);
	var->value = Q_atof (var->string);
	
// link the variable in

	//johnfitz -- insert each entry in alphabetical order
    if (cvar_vars == NULL || strcmp(var->name, cvar_vars->name) < 0) //insert at front
	{
		var->next = cvar_vars;
		cvar_vars = var;
	}
    else //insert later
	{
        prev = cvar_vars;
        cursor = cvar_vars->next;
        while (cursor && (strcmp(var->name, cursor->name) > 0))
		{
            prev = cursor;
            cursor = cursor->next;
        }
        var->next = prev->next;
        prev->next = var;
    }
	//johnfitz
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command (void)
{
	cvar_t	*var;

// check variables
	if (!(var = Cvar_FindVar(Cmd_Argv(0))))
		return false;
		
// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"%s\" is:\"%s\" default:\"%s\"\n", var->name, var->string, var->defaultvalue);
		return true;
	}

	Cvar_Set (var->name, Cmd_Argv(1));

	return true;
}

/*
============
Cvar_WriteVariables

Writes lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (FILE *f)
{
	cvar_t	*var;
	
	for (var = cvar_vars ; var ; var = var->next)
	{
		if ((!strcmp(var->name, "mapname")))
			continue;

		if ((!strcmp(var->name, "cl_loadmapcfg")))
			continue;

		if ((!strcmp(var->name, "sv_gravity")))
			continue;

		if (var->archive)
		{
			fprintf (f, "%s \"%s\"\n", var->name, var->string);		
		}
		else
		{
			if ((cfg_savevars.value == 1 && strcmp(var->string, var->defaultvalue)) || cfg_savevars.value == 2)
				fprintf (f, "%s \"%s\"\n", var->name, var->string);
		}
	}
}

void Cvar_Toggle_f (void) 
{
	extern cvar_t	cl_nomessageprint;
	cvar_t *var;

	if (Cmd_Argc() != 2) 
	{
		Con_Printf ("toggle <cvar> : toggle a cvar on/off\n");
		return;
	}

	var = Cvar_FindVar (Cmd_Argv(1));
	if (!var) 
	{
		Con_Printf ("Unknown variable \"%s\"\n", Cmd_Argv(1));
		return;
	}

	if (var->value)
	{
		Cvar_Set(Cmd_Argv(1), "0");
		Con_Printf ("%s Off.\n", Cmd_Argv(1));
	}
	else
	{
		Cvar_Set(Cmd_Argv(1), "1");
		Con_Printf ("%s On.\n", Cmd_Argv(1));
	}
}

void Cvar_Dec_f (void)
{
	int		c;
	cvar_t	*var;
	float	delta;

	c = Cmd_Argc();
	if (c != 2 && c != 3) 
	{
		Con_Printf ("dec <cvar> [value]: Subtract value from cvar.\n");
		return;
	}

	var = Cvar_FindVar (Cmd_Argv(1));
	if (!var) 
	{
		Con_Printf ("Unknown variable \"%s\"\n", Cmd_Argv(1));
		return;
	}

	if (c == 3)
		delta = atof (Cmd_Argv(2));
	else
		delta = 1;

	Cvar_SetValue (var->name, var->value - delta);
}

void Cvar_Inc_f (void)
{
	int		c;
	cvar_t	*var;
	float	delta;

	c = Cmd_Argc();
	if (c != 2 && c != 3) 
	{
		Con_Printf ("inc <cvar> [value:]Increment cvar by value.\n");
		return;
	}

	var = Cvar_FindVar (Cmd_Argv(1));
	if (!var) 
	{
		Con_Printf ("Unknown variable \"%s\"\n", Cmd_Argv(1));
		return;
	}

	if (c == 3)
		delta = atof (Cmd_Argv(2));
	else
		delta = 1;

	Cvar_SetValue (var->name, var->value + delta);
}

void Cvar_Init (void)
{
	Cvar_RegisterVariable (&cfg_savevars);
	Cmd_AddCommand ("toggle", Cvar_Toggle_f);
	Cmd_AddCommand ("inc", Cvar_Inc_f);
	Cmd_AddCommand ("dec", Cvar_Dec_f);
}
