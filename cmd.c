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
// cmd.c -- Quake script command processing module

#include "quakedef.h"
#include "winquake.h"

// joe: ReadDir()'s stuff
#ifndef _WIN32
#include <glob.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#define	MAX_ALIAS_NAME	32
#define	CMDTEXTSIZE	65536	// space for commands and script files

extern	char	key_lines[64][MAXCMDLINE];
extern	int	edit_line;
extern	int	key_linepos;

typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	char				name[MAX_ALIAS_NAME];
	char				*value;
} cmdalias_t;

static	cmdalias_t	*cmd_alias = NULL;
static	qboolean	cmd_wait;

int		trashtest, *trashspot;

char		*wih, wih_buff[50];

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f (void)
{
	cmd_wait = true;
}

/*
=============================================================================

				COMMAND BUFFER

=============================================================================
*/

sizebuf_t	cmd_text;

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	SZ_Alloc (&cmd_text, CMDTEXTSIZE);		// space for commands and script files
}


/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (char *text)
{
	int		l;
	
	l = strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Con_Printf ("Cbuf_AddText: overflow\n");
		return;
	}

	SZ_Write (&cmd_text, text, strlen(text));
}

/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (char *text)
{
	char	*temp = NULL;
	int		templen = 0;

	// copy off any commands still remaining in the exec buffer
	templen = cmd_text.cursize;

	if (templen)
	{
		temp = (char *)Z_Malloc (templen);
		memcpy (temp, cmd_text.data, templen);
		SZ_Clear (&cmd_text);
	}
	else
	{
		temp = NULL;	// shut up compiler
	}
		
// add the entire text of the file
	Cbuf_AddText (text);
	
// add the copied off data
	if (templen)
	{
		SZ_Write (&cmd_text, temp, templen);
		Z_Free (temp);
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int		i;
	char	*text;
	char	line[1024];
	int		quotes;

	while (cmd_text.cursize)
	{
		// find a \n or; line break
		text = (char *) cmd_text.data;

		quotes = 0;

		for (i = 0; i < cmd_text.cursize; i++)
		{
			if (text[i] == '"') quotes++;

			// don't break if inside a quoted string
			if (!(quotes & 1) && text[i] == ';') break;

			if (text[i] == '\n') break;
		}

		memcpy (line, text, i);
		line[i] = 0;

		// delete the text from the command buffer and move remaining commands down
		// this is necessary because commands (exec, alias) can insert data at the
		// beginning of the text buffer
		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memcpy (text, text + i, cmd_text.cursize);
		}

		// execute the command line
		Cmd_ExecuteString (line, src_command);

		if (cmd_wait)
		{
			// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait = false;
			break;
		}
	}
}


/*
==============================================================================

				SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
void Cmd_StuffCmds_f (void)
{
	int	i, j, s;
	char	*text = NULL, *build = NULL, c;

// build the combined string to parse from
	s = 0;
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		s += strlen (com_argv[i]) + 1;
	}
	if (!s)
		return;

	text = Z_Malloc (s + 1);

	text[0] = 0;

	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		strcat (text, com_argv[i]);
		if (i != com_argc-1)
			strcat (text, " ");
	}

// pull out the commands
	build = Z_Malloc (s + 1);
	build[0] = 0;

	for (i=0 ; i<s-1 ; i++)
	{
		if (text[i] == '+')
		{
			i++;

			for (j=i ; (text[j] != '+') && (text[j] != '-') && (text[j] != 0) ; j++)
				;

			c = text[j];
			text[j] = 0;

			strcat (build, text + i);
			strcat (build, "\n");
			text[j] = c;
			i = j - 1;
		}
	}

	if (build[0])
		Cbuf_InsertText (build);

	Z_Free (text);
	Z_Free (build);
}

/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f (void)
{
	char	*f = NULL, name[MAX_OSPATH];
	int	mark;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	mark = Hunk_LowMark ();
	if (!(f = (char *)COM_LoadHunkFile(name)))
	{
		char	*p;

		p = COM_SkipPath (name);
		if (!strchr(p, '.'))
		{	// no extension, so try the default (.cfg)
			strcat (name, ".cfg");
			f = (char *)COM_LoadHunkFile (name);
		}

		if (!f)
		{
			Con_Printf ("couldn't exec %s\n", name);
			return;
		}
	}
	Con_Printf ("execing %s\n",name);

	Cbuf_InsertText (f);
	Hunk_FreeToLowMark (mark);
}

/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
	int	i;
	
	for (i=1 ; i<Cmd_Argc() ; i++)
		Con_Printf ("%s ", Cmd_Argv(i));
	Con_Printf ("\n");
}

/*
============
Cmd_WriteAliases_f	-- R00k
============
*/

void Cmd_WriteAliases (char *cfgname)
{
	FILE *f;
	cmdalias_t *a;
	char *s,*n;
	int	i;

	n = Cmd_Argv(1);
	s = Cmd_Argv(1);

	for (a = cmd_alias, i = 0; a; a=a->next, i++)
		;

	if (i == 0)
	{
		Con_SafePrintf ("no alias commands found\n");
		return;
	}

	if (!(f = fopen(va("%s/%s", com_gamedir, cfgname), "w")))
	{
		Con_SafePrintf ("Couldn't write %s\n", cfgname);
		return;
	}

	fprintf (f, "//%s\n//Generated by Qrack\n\n",cfgname);
	fprintf (f,"//Cfg: "__TIME__" "__DATE__"\n");
	fprintf (f, "//--[ Alias Commands ]--\n\n");

	for (a = cmd_alias; a; a = a->next)
	{
		if (a->value && a->name)
		{
			sprintf (n,"alias %s \"%s",a->name, a->value);
			strcpy (s, n);
			i = (strlen(n) - 2);// -2 for "\n"
			s[i++] = '\"';
			s[i] = 0;
			fprintf (f,"%s\n",s);
		}
	}
	fclose(f);	
}

/*
===============
Cmd_Alias_f 

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
void Cmd_Alias_f (void)
{
	cmdalias_t	*a;
	char		cmd[1024];
	int			i, c;
	char		*s,*n;

	if (cls.demoplayback)//R00k dont stuff alias commands when watching a demo.
		return;

	switch (Cmd_Argc())
	{
	case 1: //list all aliases
		for (a = cmd_alias, i = 0; a; a=a->next, i++)
			Con_SafePrintf ("   %s: %s", a->name, a->value);
		if (i)
			Con_SafePrintf ("%i alias command(s)\n", i);
		else
			Con_SafePrintf ("no alias commands found\n");
		break;
	case 2: //output current alias string
		n = Cmd_Argv(1);
		for (a = cmd_alias ; a ; a=a->next)
			if (!strcmp(Cmd_Argv(1), a->name))//R00k empty definition argument edits the current value
			{			
				sprintf (n,"alias %s \"%s",a->name, a->value);

				strcpy (key_lines[edit_line]+1, n);
				key_linepos = (strlen(n) - 1);
				key_lines[edit_line][key_linepos++] = '\"';
				key_lines[edit_line][key_linepos] = 0;
			}
		break;
	default: //set alias string
		s = Cmd_Argv(1);
		if (strlen(s) >= MAX_ALIAS_NAME)
		{
			Con_Printf ("Alias name is too long\n");
			return;
		}

		// if the alias allready exists, reuse it
		for (a = cmd_alias ; a ; a=a->next)
		{
			if (!strcmp(s, a->name))
			{
				Z_Free (a->value);
				break;
			}
		}

		if (!a)
		{
			a = Z_Malloc (sizeof(cmdalias_t));
			a->next = cmd_alias;
			cmd_alias = a;
		}
		strcpy (a->name, s);

		// copy the rest of the command line
		cmd[0] = 0;		// start out with a null string
		c = Cmd_Argc();
		for (i=2 ; i< c ; i++)
		{
			strcat (cmd, Cmd_Argv(i));
			if ((i != c))
				strcat (cmd, " ");
		}
		strcat (cmd, "\n");

		a->value = CopyString (cmd);
		break;
	}
}

/*
===============
Cmd_Unalias_f -- johnfitz
===============
*/
void Cmd_Unalias_f (void)
{
	cmdalias_t	*a, *prev;

	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf("unalias <name> : delete alias\n");
		break;
	case 2:
		for (prev = a = cmd_alias; a; a = a->next)
		{
			if (!strcmp(Cmd_Argv(1), a->name))
			{
				prev->next = a->next;
				Z_Free (a->value);
				Z_Free (a);
				prev = a;
				return;
			}
			prev = a;
		}
		break;
	}
}

/*
===============
Cmd_Unaliasall_f -- johnfitz
===============
*/
void Cmd_Unaliasall_f (void)
{
	cmdalias_t	*blah;

	while (cmd_alias)
	{
		blah = cmd_alias->next;

		Z_Free(cmd_alias->value);
		Z_Free(cmd_alias);
		
		cmd_alias = blah;
	}
}

// joe: legacy commands, from FuhQuake
typedef struct legacycmd_s
{
	char	*oldname, *newname;
	struct legacycmd_s *next;
} legacycmd_t;

static	legacycmd_t	*legacycmds = NULL;

void Cmd_AddLegacyCommand (char *oldname, char *newname)
{
	legacycmd_t	*cmd;

	cmd = (legacycmd_t *)Q_malloc (sizeof(legacycmd_t));
	cmd->next = legacycmds;
	legacycmds = cmd;

	cmd->oldname = CopyString (oldname);
	cmd->newname = CopyString (newname);
}

static qboolean Cmd_LegacyCommand (void)
{
	qboolean	recursive = false;
	legacycmd_t	*cmd;
	char		text[1024];

	for (cmd = legacycmds ; cmd ; cmd = cmd->next)
		if (!Q_strcasecmp(cmd->oldname, Cmd_Argv(0)))
			break;

	if (!cmd)
		return false;

	if (!cmd->newname[0])
		return true;		// just ignore this command

	// build new command string
	Q_strncpyz (text, cmd->newname, sizeof(text) - 1);
	strcat (text, " ");
	strncat (text, Cmd_Args(), sizeof(text) - strlen(text) - 1);

	assert (!recursive);
	recursive = true;
	Cmd_ExecuteString (text, src_command);
	recursive = false;

	return true;
}

/*
=============================================================================

				COMMAND EXECUTION

=============================================================================
*/

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char			*name;
	xcommand_t		function;
} cmd_function_t;

static	cmd_function_t	*cmd_functions;		// possible commands to execute

#define	MAX_ARGS	80

static	int	cmd_argc;
static	char	*cmd_argv[MAX_ARGS];
static	char	*cmd_null_string = "";
static	char	*cmd_args = NULL;

cmd_source_t	cmd_source;

/*
============
Cmd_Argc
============
*/
int Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char *Cmd_Argv (int arg)
{
	if ((unsigned)arg >= cmd_argc)
		return cmd_null_string;
	return cmd_argv[arg];	
}

/*
============
Cmd_Args
============
*/
char *Cmd_Args (void)
{
	if (!cmd_args)
		return "";
	return cmd_args;
}

/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
============
*/
void Cmd_TokenizeString (char *text)
{
	int		idx;
	static	char	argv_buf[1024];
	
	idx = 0;

	cmd_argc = 0;
	cmd_args = NULL;
	
	while (1)
	{
	// skip whitespace up to a /n
		while (*text == ' ' || *text == '\t' || *text == '\r')
			text++;
		
		if (*text == '\n')
		{	// a newline seperates commands in the buffer
			text++;
			break;
		}

		if (!*text)
			return;
	
		if (cmd_argc == 1)
			 cmd_args = text;
			
		if (!(text = COM_Parse(text)))
			return;

		if (cmd_argc < MAX_ARGS)
		{
			cmd_argv[cmd_argc] = argv_buf + idx;
			strcpy (cmd_argv[cmd_argc], com_token);
			idx += strlen(com_token) + 1;
			cmd_argc++;
		}
	}
}

/*
============
Cmd_AddCommand
============
*/
void Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
	cmd_function_t	*cmd;
	cmd_function_t	*cursor,*prev; //johnfitz -- sorted list insert
	
	if (host_initialized)	// because hunk allocation would get stomped
		Sys_Error (va("Cmd_AddCommand: %s, after host_initialized", cmd_name));
		
// fail if the command is a variable name
	if (Cvar_VariableString(cmd_name)[0])
	{
		Con_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}
	
// fail if the command already exists
	if (Cmd_Exists(cmd_name))
	{
		Con_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
		return;
	}

	cmd = Hunk_AllocName (sizeof(cmd_function_t),cmd_name);//R00k
	cmd->name = cmd_name;
	cmd->function = function;

	//johnfitz -- insert each entry in alphabetical order
    if (cmd_functions == NULL || strcmp(cmd->name, cmd_functions->name) < 0) //insert at front
	{
		cmd->next = cmd_functions;
		cmd_functions = cmd;
	}
    else //insert later
	{
        prev = cmd_functions;
        cursor = cmd_functions->next;
        while ((cursor != NULL) && (strcmp(cmd->name, cursor->name) > 0))
		{
            prev = cursor;
            cursor = cursor->next;
        }
        cmd->next = prev->next;
        prev->next = cmd;
    }
}

/*
============
Cmd_Exists
============
*/
qboolean Cmd_Exists (char *cmd_name)
{
	cmd_function_t	*cmd;

	for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
	{
		if (!strcmp(cmd_name, cmd->name))
			return true;
	}

	return false;
}

/*
============
Cmd_CompleteCommand
============
*/
char *Cmd_CompleteCommand (char *partial)
{
	cmd_function_t	*cmd;
	legacycmd_t	*lcmd;
	int		len;

	if (!(len = strlen(partial)))
		return NULL;

// check functions
	for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
		if (!Q_strncasecmp(partial, cmd->name, len))
			return cmd->name;

	for (lcmd = legacycmds ; lcmd ; lcmd = lcmd->next)
		if (!Q_strncasecmp(partial, lcmd->oldname, len))
			return lcmd->oldname;

	return NULL;
}

/*
============
Cmd_CompleteCountPossible
============
*/
int Cmd_CompleteCountPossible (char *partial)
{
	cmd_function_t	*cmd;
	legacycmd_t	*lcmd;
	int		len, c = 0;

	if (!(len = strlen(partial)))
		return 0;

	for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
		if (!Q_strncasecmp(partial, cmd->name, len))
			c++;

	for (lcmd = legacycmds ; lcmd ; lcmd = lcmd->next)
		if (!Q_strncasecmp(partial, lcmd->oldname, len))
			c++;

	return c;
}

//===================================================================

int		RDFlags = 0;
direntry_t	*filelist = NULL;

static	char	filetype[8] = "file";

static	char	compl_common[MAX_FILELENGTH];
static	int	compl_len;
static	int	compl_clen;

static void FindCommonSubString (char *s)
{
	if (!compl_clen)
	{
		Q_strncpyz (compl_common, s, sizeof(compl_common));
		compl_clen = strlen (compl_common);
	} 
	else
	{
		while (compl_clen > compl_len && Q_strncasecmp(s, compl_common, compl_clen))
			compl_clen--;
	}
}

static void CompareParams (void)
{
	int	i;

	compl_clen = 0;

	for (i=0 ; i<num_files ; i++)
		FindCommonSubString (filelist[i].name);

	if (compl_clen)
		compl_common[compl_clen] = 0;
}

static void PrintEntries (void);
void EraseDirEntries (void);

#define	READDIR_ALL_PATH(p)							\
	for (search = com_searchpaths ; search ; search = search->next)		\
	{									\
		if (!search->pack)						\
		{								\
			RDFlags |= (RD_STRIPEXT | RD_NOERASE);			\
			if (skybox)						\
				RDFlags |= RD_SKYBOX;				\
			ReadDir (va("%s/%s", search->filename, subdir), p);	\
		}								\
	}

/*
============
Cmd_CompleteParameter	-- by joe

parameter completion for various commands
============
*/
void Cmd_CompleteParameter (char *partial, char *attachment)
{
	char		*s, *param, stay[MAX_QPATH], subdir[MAX_QPATH] = "", param2[MAX_QPATH];
	qboolean	skybox = false;

	Q_strncpyz (stay, partial, sizeof(stay));

// we don't need "<command> + space(s)" included
	param = strrchr (stay, ' ') + 1;
	if (!*param)		// no parameter was written in, so quit
		return;

	compl_len = strlen (param);
	strcat (param, attachment);

	if (!strcmp(attachment, "*.bsp"))
	{
		Q_strncpyz (subdir, "maps/", sizeof(subdir));
	}
	else if (!strcmp(attachment, "*.tga"))
	{
		if (strstr(stay, "loadsky ") == stay || strstr(stay, "r_skybox ") == stay)
		{
			Q_strncpyz (subdir, "env/", sizeof(subdir));
			skybox = true;
		}
		else if (strstr(stay, "loadcharset ") == stay || strstr(stay, "gl_consolefont ") == stay)
		{
			Q_strncpyz (subdir, "textures/charsets/", sizeof(subdir));
		}
		else if (strstr(stay, "crosshairimage ") == stay)
		{
			Q_strncpyz (subdir, "crosshairs/", sizeof(subdir));
		}
	}

	if (strstr(stay, "gamedir ") == stay)
	{
		RDFlags |= RD_GAMEDIR;
		ReadDir (com_basedir, param);

		pak_files = 0;	// so that previous pack searches are cleared
	}
	else if (strstr(stay, "load ") == stay || strstr(stay, "printtxt ") == stay)
	{
		RDFlags |= RD_STRIPEXT;
		ReadDir (com_gamedir, param);

		pak_files = 0;	// same here
	}
	else
	{
		searchpath_t	*search;

		EraseDirEntries ();
		pak_files = 0;

		READDIR_ALL_PATH(param);
		if (!strcmp(param + strlen(param)-3, "tga"))
		{
			Q_strncpyz (param2, param, strlen(param)-3);
			strcat (param2, "png");
			READDIR_ALL_PATH(param2);
			FindFilesInPak (va("%s%s", subdir, param2));
		}
		else if (!strcmp(param + strlen(param)-3, "dem"))
		{
			Q_strncpyz (param2, param, strlen(param)-3);
			strcat (param2, "dz");
			READDIR_ALL_PATH(param2);
			FindFilesInPak (va("%s%s", subdir, param2));
		}
		FindFilesInPak (va("%s%s", subdir, param));
	}

	if (!filelist)
		return;

	s = strchr (partial, ' ') + 1;
// just made this to avoid printing the filename twice when there's only one match
	if (num_files == 1)
	{
		*s = '\0';
		strcat (partial, filelist[0].name);
		key_linepos = strlen(partial) + 1;
		key_lines[edit_line][key_linepos] = 0;
		return;
	}

	CompareParams ();

	Con_Printf ("]%s\n", partial);
	PrintEntries ();

	*s = '\0';
	strcat (partial, compl_common);
	key_linepos = strlen(partial) + 1;
	key_lines[edit_line][key_linepos] = 0;
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void Cmd_ExecuteString (char *text, cmd_source_t src)
{	
	cmd_function_t	*cmd;
	cmdalias_t	*a;

	cmd_source = src;
	Cmd_TokenizeString (text);

// execute the command line
	if (!Cmd_Argc())
		return;		// no tokens

// check functions
	for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
	{
		if (!Q_strcasecmp(cmd_argv[0], cmd->name))
		{
			cmd->function ();
			return;
		}
	}

// check alias
	for (a = cmd_alias ; a ; a = a->next)
	{
		if (!Q_strcasecmp(cmd_argv[0], a->name))
		{
			Cbuf_InsertText (a->value);
			return;
		}
	}

// check cvars
	if (Cvar_Command())
		return;

// joe: check legacy commands
	if (Cmd_LegacyCommand())
		return;

	Con_Printf ("Unknown command \"%s\"\n", Cmd_Argv(0));
}

// from ProQuake
// JPG - added these for %r formatting
extern cvar_t	pq_needrl;
extern cvar_t	pq_haverl;
extern cvar_t	pq_needrox;

// JPG - added these for %p formatting
extern cvar_t	pq_quad;
extern cvar_t	pq_pent;
extern cvar_t	pq_ring;

// JPG 3.00 - %w formatting
extern cvar_t	pq_weapons;
extern cvar_t	pq_noweapons;
// from ProQuake

extern cvar_t	pq_armor;//R00k

/*
===================
Cmd_ForwardToServer

Sends the entire command line over to the server
===================
*/
void Cmd_ForwardToServer (void)
{
	//from ProQuake --start
	char *src, *dst, buff[128];			// JPG - used for say/say_team formatting
	int minutes, seconds, match_time;	// JPG - used for %t
	//from ProQuake --end

	if (cls.state != ca_connected)
	{
		Con_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}
	
	if (cls.demoplayback)
		return;		// not really connected

	MSG_WriteByte (&cls.message, clc_stringcmd);
	
	//----------------------------------------------------------------------
	// JPG - handle say separately for formatting--start
	if ((!Q_strcasecmp(Cmd_Argv(0), "say") || !Q_strcasecmp(Cmd_Argv(0), "say_team")) && Cmd_Argc() > 1)
	{
		SZ_Print (&cls.message, Cmd_Argv(0));
		SZ_Print (&cls.message, " ");

		src = Cmd_Args();
		dst = buff;
		while (*src && dst - buff < 100)
		{
			if (*src == '%')
			{
				switch (*++src)
				{
				case 'h':
					dst += sprintf(dst, "%d", cl.stats[STAT_HEALTH]);
					break;

				case 'a':
					{
						char *ch = pq_armor.string;
						int first = 1;
						int item;

						dst += sprintf(dst, "%d", cl.stats[STAT_ARMOR]);
						//R00k: added to show armor type					
						if (cl.stats[STAT_ARMOR] > 0)
						{
							for (item = IT_ARMOR1 ; item <= IT_ARMOR3 ; item *= 2)
							{
								if (*ch != ':' && (cl.items & item))
								{
									if (!first)
										*dst++ = ',';
									first = 0;
									while (*ch && *ch != ':')
										*dst++ = *ch++;
								}
								while (*ch && *ch != ':')
									*ch++;
								if (*ch)
									*ch++;
								if (!*ch)
									break;
							}
						}
					}
					break;						

				case 'A':
				if (cl.stats[STAT_HEALTH] > 0)
				{					
					switch (cl.stats[STAT_ACTIVEWEAPON])
					{
					case IT_SHOTGUN:		
						if (*++src == 't')
						{
							dst += sprintf(dst, "shells");
						}
						else
						{
							if (cl.stats[STAT_SHELLS] < 5)
								dst += sprintf(dst, "I need shells ");	
							else
								dst += sprintf(dst, "%d shells", cl.stats[STAT_SHELLS]);
						}
						break;

					case IT_SUPER_SHOTGUN:							
						if (*++src == 't')
						{
							dst += sprintf(dst, "shells");
						}
						else
						{
							if (cl.stats[STAT_SHELLS] < 5)
								dst += sprintf(dst, "I need shells ");
							else
								dst += sprintf(dst, "%d shells", cl.stats[STAT_SHELLS]);
						}
						break;

					case IT_NAILGUN:							
						if (*++src == 't')
						{
							dst += sprintf(dst, "nails");
						}
						else
						{
							if (cl.stats[STAT_NAILS] < 5)
								dst += sprintf(dst, "I need nails ");
							else
								dst += sprintf(dst, "%d nails", cl.stats[STAT_NAILS]);
						}
						break;

					case IT_SUPER_NAILGUN:							
						if (*++src == 't')
						{
							dst += sprintf(dst, "nails");
						}
						else
						{
							if (cl.stats[STAT_NAILS] < 5)
								dst += sprintf(dst, "I need nails ");
							else
								dst += sprintf(dst, "%d nails", cl.stats[STAT_NAILS]);
						}
						break;

					case IT_GRENADE_LAUNCHER:							
						if (*++src == 't')
						{
							dst += sprintf(dst, "rockets");
						}
						else
						{
							if (cl.stats[STAT_NAILS] < 5)
								dst += sprintf(dst, "%s", pq_needrox.string);
							else
								dst += sprintf(dst, "%d rockets", cl.stats[STAT_ROCKETS]);
						}
						break;

					case IT_ROCKET_LAUNCHER:														
						if (*++src == 't')
						{
							dst += sprintf(dst, "rockets");
						}
						else
						{
							if (cl.stats[STAT_NAILS] < 5)
								dst += sprintf(dst, "%s", pq_needrox.string);
							else
								dst += sprintf(dst, "%d rockets", cl.stats[STAT_ROCKETS]);
						}
						break;

					case IT_LIGHTNING:							
						if (*++src == 't')
						{
							dst += sprintf(dst, "cells");
						}
						else
						{
							if (cl.stats[STAT_CELLS] < 5)
								dst += sprintf(dst, "I need cells");
							else
								dst += sprintf(dst, "%d cells", cl.stats[STAT_CELLS]);
						}
						break;

					default:
						if (*++src == 't')
						{
							dst += sprintf(dst, "nothing ");
						}
						break;
					}
				}
				else
					dst += sprintf(dst, "%s", pq_needrl.string);
				break;

				case 'r':
					if (cl.stats[STAT_HEALTH] > 0 && (cl.items & IT_ROCKET_LAUNCHER))
					{
						if (cl.stats[STAT_ROCKETS] < 5)
							dst += sprintf(dst, "%s", pq_needrox.string);
						else
							dst += sprintf(dst, "%s", pq_haverl.string);
					}
					else
						dst += sprintf(dst, "%s", pq_needrl.string);
					break;

				case 'l':
					dst += sprintf(dst, "%s", LOC_GetLocation(cl_entities[cl.viewentity].origin));
					break;

				case 'd':
					dst += sprintf(dst, "%s", LOC_GetLocation(cl.death_location));
					break;
				
				case 'D':
					dst += sprintf(dst, "%s", wih);
					break;

				case 'c':
					dst += sprintf(dst, "%d", cl.stats[STAT_CELLS]);
					break;

				case 'x':
					dst += sprintf(dst, "%d", cl.stats[STAT_ROCKETS]);
					break;

				case 'R':					
					if (cl.items & IT_SIGIL1)
					{
						dst += sprintf(dst, "Resistance");							
						break;
					}
					if (cl.items & IT_SIGIL2)
					{
						dst += sprintf(dst, "Strength");
						break;
					}
					if (cl.items & IT_SIGIL3)
					{							
						dst += sprintf(dst, "Haste");
						break;
					}
					if (cl.items & IT_SIGIL4)
					{						
						dst += sprintf(dst, "Regen");
						break;							
					}	
					break;

				case 'F':
					if (cl.items & IT_KEY1)
					{
						dst += sprintf(dst, "Blue Flag");							
						break;
					}
					if (cl.items & IT_KEY2)
					{
						dst += sprintf(dst, "Red Flag");							
						break;
					}
					break;

				case 'p':
					if (cl.stats[STAT_HEALTH] > 0)
					{
						if (cl.items & IT_QUAD)
						{
							dst += sprintf(dst, "%s", pq_quad.string);
							if (cl.items & (IT_INVULNERABILITY | IT_INVISIBILITY))
								*dst++ = ',';
						}
						if (cl.items & IT_INVULNERABILITY)
						{
							dst += sprintf(dst, "%s", pq_pent.string);
							if (cl.items & IT_INVISIBILITY)
								*dst++ = ',';
						}
						if (cl.items & IT_INVISIBILITY)
							dst += sprintf(dst, "%s", pq_ring.string);
					}
					break;

				case 'w':	// JPG 3.00
					{
						int first = 1;
						int item;
						char *ch = pq_weapons.string;
						if (cl.stats[STAT_HEALTH] > 0)
						{
							for (item = IT_SUPER_SHOTGUN ; item <= IT_LIGHTNING ; item *= 2)
							{
								if (*ch != ':' && (cl.items & item))
								{
									if (!first)
										*dst++ = ',';
									first = 0;
									while (*ch && *ch != ':')
										*dst++ = *ch++;
								}
								while (*ch && *ch != ':')
									*ch++;
								if (*ch)
									*ch++;
								if (!*ch)
									break;
							}
						}
						if (first)
							dst += sprintf(dst, "%s", pq_noweapons.string);
					}
					break;
				//R00k added W for weapon in hand based on pq_weapons.string
				case 'W':
					if (cl.stats[STAT_HEALTH] > 0)
					{					
						int		item;
						char	*ch = pq_weapons.string;

						for (item = IT_SUPER_SHOTGUN ; item <= IT_LIGHTNING ; item *= 2)
						{
							if (*ch != ':' && (item == cl.stats[STAT_ACTIVEWEAPON]))
							{
								while (*ch && *ch != ':')
									*dst++ = *ch++;
								break;
							}
							while (*ch && *ch != ':')
								*ch++;
							if (*ch)
								*ch++;
							if (!*ch)
								break;
						}
					}
					else
						dst += sprintf(dst, "%s", pq_noweapons.string);
					break;

				case '%':
					*dst++ = '%';
					break;

				case 't':
					if ((cl.minutes || cl.seconds) && cl.seconds < 128)
					{
						if (cl.match_pause_time)
							match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.match_pause_time - cl.last_match_time));
						else
							match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.time - cl.last_match_time));
						minutes = match_time / 60;
						seconds = match_time - 60 * minutes;
					}
					else
					{
						minutes = cl.time / 60;
						seconds = cl.time - 60 * minutes;
						minutes &= 511;
					}
					dst += sprintf(dst, "%d:%02d", minutes, seconds);
					break;

				default:
					*dst++ = '%';
					*dst++ = *src;
					break;
				}
				if (*src)
					src++;
			}
			else
				*dst++ = *src++;
		}
		*dst = 0;
		
		SZ_Print (&cls.message, buff);
		return;
	}
	// JPG - handle say separately for formatting--end
	//----------------------------------------------------------------------

	if (Q_strcasecmp(Cmd_Argv(0), "cmd") != 0)
	{
		SZ_Print (&cls.message, Cmd_Argv(0));
		SZ_Print (&cls.message, " ");
	}
	if (Cmd_Argc() > 1)
		SZ_Print (&cls.message, Cmd_Args());
	else
		SZ_Print (&cls.message, "\n");
}

/*
================
Cmd_CheckParm

Returns the position (1 to argc-1) in the command's argument list
where the given parameter apears, or 0 if not present
================
*/
int Cmd_CheckParm (char *parm)
{
	int	i;
	
	if (!parm)
		Sys_Error ("Cmd_CheckParm: NULL");

	for (i=1 ; i<Cmd_Argc() ; i++)
		if (!Q_strcasecmp(parm, Cmd_Argv(i)))
			return i;
			
	return 0;
}

/*
====================
Cmd_CmdList_f

List all console commands
====================
*/
void Cmd_CmdList_f (void)
{
	cmd_function_t	*cmd;
	int		counter;
	
	if (cmd_source != src_command)
		return;

	for (counter = 0, cmd = cmd_functions ; cmd ; cmd = cmd->next, counter++)
		Con_Printf ("%s\n", cmd->name);

	Con_Printf ("------------\n%d commands\n", counter);
}

/*
====================
Cmd_CvarList_f

List all console variables
====================
*/
void Cmd_CvarList_f (void)
{
	cvar_t	*var;
	int	counter;
	
	if (cmd_source != src_command)	
		return;

	for (counter = 0, var = cvar_vars ; var ; var = var->next, counter++)
	{		
		Con_Printf ("%s %s [default:%s]\n", var->name, var->string, var->defaultvalue);
	}

	Con_Printf ("------------\n%d variables\n", counter);
}

/*
====================
Cmd_Dir_f

List all files in the mod's directory	-- by joe
====================
*/
void Cmd_Dir_f (void)
{
	char	myarg[MAX_FILELENGTH];

	if (cmd_source != src_command)
		return;

	if (!strcmp(Cmd_Argv(1), cmd_null_string))
	{
		Q_strncpyz (myarg, "*", sizeof(myarg));
		Q_strncpyz (filetype, "file", sizeof(filetype));
	}
	else
	{
		Q_strncpyz (myarg, Cmd_Argv(1), sizeof(myarg));
		// first two are exceptional cases
		if (strstr(myarg, "*"))
			Q_strncpyz (filetype, "file", sizeof(filetype));
		else if (strstr(myarg, "*.dem"))
			Q_strncpyz (filetype, "demo", sizeof(filetype));
		else
		{
			if (strchr(myarg, '.'))
			{
				Q_strncpyz (filetype, COM_FileExtension(myarg), sizeof(filetype));
				filetype[strlen(filetype)] = 0x60;	// right-shadowed apostrophe
			}
			else
			{
				strcat (myarg, "*");
				Q_strncpyz (filetype, "file", sizeof(filetype));
			}
		}
	}

	RDFlags |= RD_COMPLAIN;
	ReadDir (com_gamedir, myarg);
	if (!filelist)
		return;

	Con_Printf ("\x02" "%ss in current folder are:\n", filetype);
	PrintEntries ();
}

static void Q_toLower (char* str)		// for strings
{
	char	*s;
	int	i;

	i = 0;
	s = str;

	while (*s)
	{
		if (*s >= 'A' && *s <= 'Z')
			*(str + i) = *s + 32;
		i++;
		s++;
	}
}

static void AddNewEntry (char *fname, int ftype, long fsize)
{
	int	i, pos;

	filelist = Q_realloc (filelist, (num_files + 1) * sizeof(direntry_t));

#ifdef _WIN32
	Q_toLower (fname);
	// else don't convert, linux is case sensitive
#endif

	// inclusion sort
	for (i=0 ; i<num_files ; i++)
	{
		if (ftype < filelist[i].type)
			continue;
		else if (ftype > filelist[i].type)
			break;

		if (strcmp(fname, filelist[i].name) < 0)
			break;
	}
	pos = i;
	for (i=num_files ; i>pos ; i--)
		filelist[i] = filelist[i-1];

        filelist[i].name = Q_strdup (fname);
	filelist[i].type = ftype;
	filelist[i].size = fsize;

	num_files++;
}

static void AddNewEntry_unsorted (char *fname, int ftype, long fsize)
{
	filelist = Q_realloc (filelist, (num_files + 1) * sizeof(direntry_t));

#ifdef _WIN32
	Q_toLower (fname);
	// else don't convert, linux is case sensitive
#endif
        filelist[num_files].name = Q_strdup (fname);
	filelist[num_files].type = ftype;
	filelist[num_files].size = fsize;

	num_files++;
}

void EraseDirEntries (void)
{
	if (filelist)
	{
		Q_free (filelist);
		filelist = NULL;
		num_files = 0;
	}
}

static qboolean CheckEntryName (char *ename)
{
	int	i;

	for (i=0 ; i<num_files ; i++)
		if (!Q_strcasecmp(ename, filelist[i].name))
			return true;

	return false;
}

#define SLASHJMP(x, y)			\
	if (!(x = strrchr(y, '/')))	\
		x = y;			\
	else				\
		*++x

/*
=================
ReadDir			-- by joe
=================
*/
int	num_files = 0;
void ReadDir (char *path, char *the_arg)
{
#ifdef _WIN32
	HANDLE		h;
	WIN32_FIND_DATA	fd;
#else
	int		h, i = 0;
	glob_t		fd;
	char		*p;
	struct	stat	fileinfo;
#endif

	if (path[strlen(path)-1] == '/')
		path[strlen(path)-1] = 0;

	if (!(RDFlags & RD_NOERASE))
		EraseDirEntries ();

#ifdef _WIN32
	h = FindFirstFile (va("%s/%s", path, the_arg), &fd);
	if (h == INVALID_HANDLE_VALUE)
#else
	h = glob (va("%s/%s", path, the_arg), 0, NULL, &fd);
	if (h == GLOB_ABORTED)
#endif
	{
		if (RDFlags & RD_MENU_DEMOS)
		{
			AddNewEntry ("Error reading directory", 3, 0);
			num_files = 1;
		}
		else if (RDFlags & RD_COMPLAIN)
		{
			Con_Printf ("No such file\n");
		}
		goto end;
	}

	if (RDFlags & RD_MENU_DEMOS && !(RDFlags & RD_MENU_DEMOS_MAIN))
	{
		AddNewEntry ("..", 2, 0);
		num_files = 1;
	}

	do {
		int	fdtype;
		long	fdsize;
		char	filename[MAX_FILELENGTH];

#ifdef _WIN32
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!(RDFlags & (RD_MENU_DEMOS | RD_GAMEDIR)) || !strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
				continue;

			fdtype = 1;
			fdsize = 0;
			Q_strncpyz (filename, fd.cFileName, sizeof(filename));
		}
		else
		{
			char	ext[8];

			if (RDFlags & RD_GAMEDIR)
				continue;

			Q_strncpyz (ext, COM_FileExtension(fd.cFileName), sizeof(ext));

			if (RDFlags & RD_MENU_DEMOS && Q_strcasecmp(ext, "dem") && Q_strcasecmp(ext, "dz"))
				continue;

			fdtype = 0;
			fdsize = fd.nFileSizeLow;
			if (Q_strcasecmp(ext, "dz") && RDFlags & (RD_STRIPEXT | RD_MENU_DEMOS))
			{
				COM_StripExtension (fd.cFileName, filename);
				if (RDFlags & RD_SKYBOX)
					filename[strlen(filename)-2] = 0;	// cut off skybox_ext
			}
			else
			{
				Q_strncpyz (filename, fd.cFileName, sizeof(filename));
			}

			if (CheckEntryName(filename))
				continue;	// file already on list
		}
#else
		if (h == GLOB_NOMATCH || !fd.gl_pathc)
			break;

		SLASHJMP(p, fd.gl_pathv[i]);
		stat (fd.gl_pathv[i], &fileinfo);

		if (S_ISDIR(fileinfo.st_mode))
		{
			if (!(RDFlags & (RD_MENU_DEMOS | RD_GAMEDIR)))
				continue;

			fdtype = 1;
			fdsize = 0;
			Q_strncpyz (filename, p, sizeof(filename));
		}
		else
		{
			char	ext[8];

			if (RDFlags & RD_GAMEDIR)
				continue;

			Q_strncpyz (ext, COM_FileExtension(p), sizeof(ext));

			if (RDFlags & RD_MENU_DEMOS && Q_strcasecmp(ext, "dem") && Q_strcasecmp(ext, "dz"))
				continue;

			fdtype = 0;
			fdsize = fileinfo.st_size;
			if (Q_strcasecmp(ext, "dz") && RDFlags & (RD_STRIPEXT | RD_MENU_DEMOS))
			{
				COM_StripExtension (p, filename);
				if (RDFlags & RD_SKYBOX)
					filename[strlen(filename)-2] = 0;	// cut off skybox_ext
			}
			else
			{
				Q_strncpyz (filename, p, sizeof(filename));
			}

			if (CheckEntryName(filename))
				continue;	// file already on list
		}
#endif
		AddNewEntry (filename, fdtype, fdsize);
	}
#ifdef _WIN32
	while (FindNextFile(h, &fd));
	FindClose (h);
#else
	while (++i < fd.gl_pathc);
	globfree (&fd);
#endif

	if (!num_files)
	{
		if (RDFlags & RD_MENU_DEMOS)
		{
			AddNewEntry ("[ no files ]", 3, 0);
			num_files = 1;
		}
		else if (RDFlags & RD_COMPLAIN)
		{
			Con_Printf ("No such file\n");
		}
	}

end:
	RDFlags = 0;
}

static qboolean CheckRealBSP (char *bspname)
{
	if (!strcmp(bspname, "b_batt0.bsp") ||
	    !strcmp(bspname, "b_batt1.bsp") ||
	    !strcmp(bspname, "b_bh10.bsp") ||
	    !strcmp(bspname, "b_bh100.bsp") ||
	    !strcmp(bspname, "b_bh25.bsp") ||
	    !strcmp(bspname, "b_explob.bsp") ||
	    !strcmp(bspname, "b_nail0.bsp") ||
	    !strcmp(bspname, "b_nail1.bsp") ||
	    !strcmp(bspname, "b_rock0.bsp") ||
	    !strcmp(bspname, "b_rock1.bsp") ||
	    !strcmp(bspname, "b_shell0.bsp") ||
	    !strcmp(bspname, "b_shell1.bsp") ||
	    !strcmp(bspname, "b_exbox2.bsp"))
		return false;

	return true;
}

/*
=================
FindFilesInPak

Search for files inside a PAK file		-- by joe
=================
*/
int	pak_files = 0;
void FindFilesInPak (char *the_arg)
{
	int		i;
	searchpath_t	*search;
	pack_t		*pak;
	char		*myarg;

	SLASHJMP(myarg, the_arg);
	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (search->pack)
		{
			char	*s, *p, ext[8], filename[MAX_FILELENGTH];

			// look through all the pak file elements
			pak = search->pack;
			for (i=0 ; i<pak->numfiles ; i++)
			{
				s = pak->files[i].name;
				Q_strncpyz (ext, COM_FileExtension(s), sizeof(ext));
				if (!Q_strcasecmp(ext, COM_FileExtension(myarg)))
				{
					SLASHJMP(p, s);
					if (!Q_strcasecmp(ext, "bsp") && !CheckRealBSP(p))
						continue;
					if (!Q_strncasecmp(s, the_arg, strlen(the_arg)-5) || 
					    (*myarg == '*' && !Q_strncasecmp(s, the_arg, strlen(the_arg)-5-compl_len)))
					{
						COM_StripExtension (p, filename);
						if (CheckEntryName(filename))
							continue;
						AddNewEntry_unsorted (filename, 0, pak->files[i].filelen);
						pak_files++;
					}
				}
			}
		}
	}
}

/*
==================
PaddedPrint
==================
*/
#define	COLUMNWIDTH	20
#define	MINCOLUMNWIDTH	18	// the last column may be slightly smaller

extern	int	con_x;

static void PaddedPrint (char *s)
{
	extern	int	con_linewidth;
	int		nextcolx = 0;

	if (con_x)
		nextcolx = (int)((con_x + COLUMNWIDTH) / COLUMNWIDTH) * COLUMNWIDTH;

	if (nextcolx > con_linewidth - MINCOLUMNWIDTH
		|| (con_x && nextcolx + strlen(s) >= con_linewidth))
		Con_Printf ("\n");

	if (con_x)
		Con_Printf (" ");
	while (con_x % COLUMNWIDTH)
		Con_Printf (" ");
	Con_Printf ("%s", s);
}

static void PrintEntries (void)
{
	int	i, filectr;

	filectr = pak_files ? (num_files - pak_files) : 0;

	for (i=0 ; i<num_files ; i++)
	{
		if (!filectr-- && pak_files)
		{
			if (con_x)
			{
				Con_Printf ("\n");
				Con_Printf ("\x02" "inside pack file:\n");
			}
			else
			{
				Con_Printf ("\x02" "inside pack file:\n");
			}
		}
		PaddedPrint (filelist[i].name);
	}

	if (con_x)
		Con_Printf ("\n");
}

/*
==================
CompleteCommand

Advanced command completion

Main body and many features imported from ZQuake	-- joe
==================
*/
void CompleteCommand (void)
{
	int	c, v;
	char	*s, *cmd;

	s = key_lines[edit_line] + 1;
	if (!(compl_len = strlen(s)))
		return;
	compl_clen = 0;

	c = Cmd_CompleteCountPossible (s);
	v = Cvar_CompleteCountPossible (s);

	if (c + v > 1)
	{
		Con_Printf ("\n");

		if (c)
		{
			cmd_function_t	*cmd;
			legacycmd_t	*lcmd;

			Con_Printf ("\x02" "commands:\n");
			// check commands
			for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
			{
				if (!Q_strncasecmp(s, cmd->name, compl_len))
				{
					PaddedPrint (cmd->name);
					FindCommonSubString (cmd->name);
				}
			}

			// joe: check for legacy commands also
			for (lcmd = legacycmds ; lcmd ; lcmd = lcmd->next)
			{
				if (!Q_strncasecmp(s, lcmd->oldname, compl_len))
				{
					PaddedPrint (lcmd->oldname);
					FindCommonSubString (lcmd->oldname);
				}
			}

			if (con_x)
				Con_Printf ("\n");
		}

		if (v)
		{
			cvar_t		*var;

			Con_Printf ("\x02" "variables:\n");
			// check variables
			for (var = cvar_vars ; var ; var = var->next)
			{
				if (!Q_strncasecmp(s, var->name, compl_len))
				{
					PaddedPrint (var->name);
					FindCommonSubString (var->name);
				}
			}

			if (con_x)
				Con_Printf ("\n");
		}
	}

	if (c + v == 1)
	{
		if (!(cmd = Cmd_CompleteCommand(s)))
			cmd = Cvar_CompleteVariable (s);
	}
	else if (compl_clen)
	{
		compl_common[compl_clen] = 0;
		cmd = compl_common;
	}
	else
		return;

	strcpy (key_lines[edit_line]+1, cmd);
	key_linepos = strlen(cmd) + 1;
	if (c + v == 1)
		key_lines[edit_line][key_linepos++] = ' ';
	key_lines[edit_line][key_linepos] = 0;
}

/*
====================
Cmd_DemDir_f

List all demo files		-- by joe
====================
*/
void Cmd_DemDir_f (void)
{
	char	myarg[MAX_FILELENGTH];

	if (cmd_source != src_command)
		return;

	if (!strcmp(Cmd_Argv(1), cmd_null_string))
	{
		Q_strncpyz (myarg, "*.dem", sizeof(myarg));
	}
	else
	{
		Q_strncpyz (myarg, Cmd_Argv(1), sizeof(myarg));
		if (strchr(myarg, '.'))
		{
			Con_Printf ("You needn`t use dots in demdir parameters\n");
			if (strcmp(COM_FileExtension(myarg), "dem"))
			{
				Con_Printf ("demdir is for demo files only\n");
				return;
			}
		}
		else
		{
			strcat (myarg, "*.dem");
		}
	}

	Q_strncpyz (filetype, "demo", sizeof(filetype));

	RDFlags |= (RD_STRIPEXT | RD_COMPLAIN);
	ReadDir (com_gamedir, myarg);
	if (!filelist)
		return;

	Con_Printf ("\x02" "%ss in current folder are:\n", filetype);
	PrintEntries ();
}

/*
====================
AddTabs

Replaces nasty tab character with spaces	-- by joe
====================
*/
static void AddTabs (char *buf)
{
	unsigned char	*s, tmp[256];
	int		i;

	for (s = buf, i = 0 ; *s ; s++, i++)
	{
		switch (*s)
		{
		case 0xb4:
		case 0x27:
			*s = 0x60;
			break;

		case '\t':
			strcpy (tmp, s + 1);
			while (i++ < 8)
				*s++ = ' ';
			*s-- = '\0';
			strcat (buf, tmp);
			break;
		}

		if (i >= 7)
			i = -1;
	}
}

/*
====================
Cmd_PrintTxt_f

Prints a text file into the console	-- by joe
====================
*/
void Cmd_PrintTxt_f (void)
{
	char	name[MAX_FILELENGTH], buf[256] = {0};
	FILE	*f;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("printtxt <txtfile> : prints a text file\n");
		return;
	}

	Q_strncpyz (name, Cmd_Argv(1), sizeof(name));

	COM_DefaultExtension (name, ".txt");

	Q_strncpyz (buf, va("%s/%s", com_gamedir, name), sizeof(buf));
	if (!(f = fopen(buf, "rt")))
	{
		Con_Printf ("ERROR: couldn't open %s\n", name);
		return;
	}

	Con_Printf ("\n");
	while (fgets(buf, 256, f))
	{
		AddTabs (buf);
		Con_Printf ("%s", buf);
		memset (buf, 0, sizeof(buf));
	}

	Con_Printf ("\n\n");
	fclose (f);
}

/*
====================
Cmd_Describe_f

Prints manual info about given variable or command into the console.
====================
*/
void Cmd_Describe_f (void)
{
	char	name[MAX_FILELENGTH], buf[256] = {0};
	FILE	*f;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("describe <command> : Prints manual info about given variable or command into the console.\n");
		return;
	}

	Q_strncpyz (name, Cmd_Argv(1), sizeof(name));

	COM_DefaultExtension (name, ".man");

	Q_strncpyz (buf, va("%s/%s", com_gamedir, name), sizeof(buf));
	if (!(f = fopen(buf, "rt")))
	{
		Con_Printf ("ERROR: couldn't open %s\n", name);
		return;
	}

	Con_Printf ("\n");
	while (fgets(buf, 256, f))
	{
		AddTabs (buf);
		Con_Printf ("%s", buf);
		memset (buf, 0, sizeof(buf));
	}

	Con_Printf ("\n\n");
	fclose (f);
}

void Cmd_Servers_f (void)
{
	char url[1024];
	qboolean success;
	FILE *f;
	extern int Web_Get( const char *url, const char *referer, const char *name, int resume, int max_downloading_time, int timeout, int ( *_progress )(double) );
	extern char *Con_Quakebar (int len);
	char buf[128] = {0};
//	char name[32] = {0};
	
	Q_snprintfz( url, sizeof( url ), "%s%s", SERVERLIST_URL, SERVERLIST_FILE );
	success = Web_Get( url, NULL, va("%s/qrack/servers.txt", com_basedir), false, 2, 2, NULL );

	if( !success )
		return;

	Q_strncpyz (buf, va("%s/qrack/servers.txt", com_basedir), sizeof(buf));

	if (!(f = fopen(buf, "rt")))
	{
		Con_Printf ("ERROR: couldn't open servers.txt\n");
		return;
	}

	Con_Printf ("reading : servers.quakeone.com\n");
	
	Con_Printf ("ùûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûü\n");
	
	while (fgets(buf, 128, f))
	{
		if (strstr(Cmd_Argv(1),"all"))
		{
		}
		else
		{
			if (strstr(buf,"00/"))
			{
				memset (buf, 0, sizeof(buf));
				continue;
			}
		}

		Con_Printf ("%s", buf);
		memset (buf, 0, sizeof(buf));
	}

	Con_Printf ("\nùûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûûü\n");
	Con_Printf ("\n");
	fclose (f);
}

void Cmd_PrintLoc_f (void)
{
	char loc[24], ang[24];

	sprintf (loc, "'%5.1f %5.1f %5.1f'", cl_entities[cl.viewentity].origin[0], cl_entities[cl.viewentity].origin[1], cl_entities[cl.viewentity].origin[2]);	
	Con_Printf ("origin:%s\n",loc);
	sprintf (ang, "'%5.1f %5.1f %5.1f'", cl_entities[cl.viewentity].angles[0], cl_entities[cl.viewentity].angles[1], cl_entities[cl.viewentity].angles[2]);		
	Con_Printf ("angle :%s\n",ang);
}


/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
// register our commands
	Cmd_AddCommand ("stuffcmds", Cmd_StuffCmds_f);
	Cmd_AddCommand ("exec", Cmd_Exec_f);
	Cmd_AddCommand ("echo", Cmd_Echo_f);
	Cmd_AddCommand ("alias", Cmd_Alias_f);
	Cmd_AddCommand ("cmd", Cmd_ForwardToServer);
	Cmd_AddCommand ("wait", Cmd_Wait_f);

	// by joe
	Cmd_AddCommand ("cmdlist", Cmd_CmdList_f);
	Cmd_AddCommand ("cvarlist", Cmd_CvarList_f);
	Cmd_AddCommand ("unalias", Cmd_Unalias_f); //johnfitz
	Cmd_AddCommand ("unaliasall", Cmd_Unaliasall_f); //johnfitz
	Cmd_AddCommand ("dir", Cmd_Dir_f);
	Cmd_AddCommand ("demdir", Cmd_DemDir_f);
	Cmd_AddCommand ("printtxt", Cmd_PrintTxt_f);
	Cmd_AddCommand ("describe", Cmd_Describe_f);
	Cmd_AddCommand ("spot", Cmd_PrintLoc_f);//R00k
	Cmd_AddCommand ("servers",Cmd_Servers_f);//R00k
}
