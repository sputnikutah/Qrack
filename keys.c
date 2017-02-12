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
#include "quakedef.h"

/*

key up events are sent even if in console mode

*/

//cvar_t		cl_chatmode = {"cl_chatmode", "1"};
char		key_lines[64][MAXCMDLINE];	// 32 doubled -- joe
int			key_linepos;
int			key_lastpress;
	
int			edit_line = 0;
int			history_line = 0;

keydest_t	key_dest;

int			key_insert = 1;	// On by Default, from [sons]Quake
int			key_count;		// incremented every key event

char		*keybindings[256];
qboolean	consolekeys[256];	// if true, can't be rebound while in console
qboolean	menubound[256];		// if true, can't be rebound while in menu
int			keyshift[256];		// key to map to if shift held down in console
int			key_repeats[256];	// if > 1, it is autorepeating
qboolean	keydown[256];

typedef struct
{
	char	*name;
	int	keynum;
} keyname_t;

keyname_t keynames[] =
{
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},

	{"CAPSLOCK", K_CAPSLOCK},
	{"PRTSCN", K_PRINTSCR},//R00k: this IS what the key has printed on it!
	{"SCRLK", K_SCRLCK},//R00k: this IS what the key has printed on it!
	{"PAUSE", K_PAUSE},

	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"LALT", K_LALT},
	{"RALT", K_RALT},
	{"CTRL", K_CTRL},
	{"LCTRL", K_LCTRL},
	{"RCTRL", K_RCTRL},
	{"SHIFT", K_SHIFT},
	{"LSHIFT", K_LSHIFT},
	{"RSHIFT", K_RSHIFT},

	{"WINKEY", K_WIN},
	{"LWINKEY", K_LWIN},
	{"RWINKEY", K_RWIN},
	{"POPUPMENU", K_MENU},
	
	// keypad keys

	{"NUMLOCK", KP_NUMLOCK},
	{"KP_NUMLCK", KP_NUMLOCK},
	{"KP_NUMLOCK", KP_NUMLOCK},
	{"KP_SLASH", KP_SLASH},
	{"KP_DIVIDE", KP_SLASH},
	{"KP_STAR", KP_STAR},
	{"KP_MULTIPLY", KP_STAR},

	{"KP_MINUS", KP_MINUS},

	{"KP_HOME", KP_HOME},
	{"KP_7", KP_HOME},
	{"KP_UPARROW", KP_UPARROW},
	{"KP_8", KP_UPARROW},
	{"KP_PGUP", KP_PGUP},
	{"KP_9", KP_PGUP},
	{"KP_PLUS", KP_PLUS},

	{"KP_LEFTARROW", KP_LEFTARROW},
	{"KP_4", KP_LEFTARROW},
	{"KP_5", KP_5},
	{"KP_RIGHTARROW", KP_RIGHTARROW},
	{"KP_6", KP_RIGHTARROW},

	{"KP_END", KP_END},
	{"KP_1", KP_END},
	{"KP_DOWNARROW", KP_DOWNARROW},
	{"KP_2", KP_DOWNARROW},
	{"KP_PGDN", KP_PGDN},
	{"KP_3", KP_PGDN},

	{"KP_INS", KP_INS},
	{"KP_0", KP_INS},
	{"KP_DEL", KP_DEL},
	{"KP_ENTER", KP_ENTER},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},
	{"MOUSE6", K_MOUSE6},
	{"MOUSE7", K_MOUSE7},
	{"MOUSE8", K_MOUSE8},

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},
	{"AUX17", K_AUX17},
	{"AUX18", K_AUX18},
	{"AUX19", K_AUX19},
	{"AUX20", K_AUX20},
	{"AUX21", K_AUX21},
	{"AUX22", K_AUX22},
	{"AUX23", K_AUX23},
	{"AUX24", K_AUX24},
	{"AUX25", K_AUX25},
	{"AUX26", K_AUX26},
	{"AUX27", K_AUX27},
	{"AUX28", K_AUX28},
	{"AUX29", K_AUX29},
	{"AUX30", K_AUX30},
	{"AUX31", K_AUX31},
	{"AUX32", K_AUX32},

	{"PAUSE", K_PAUSE},

	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},

	{"SEMICOLON", ';'},	// because a raw semicolon seperates commands

	{NULL, 0}
};

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

// by joe, from ZQuake
static void AdjustConsoleHeight (int delta)
{
	extern	cvar_t	scr_consize;
	int		height;

	if (!cl.worldmodel || cls.signon != SIGNONS)
		return;
	height = (scr_consize.value * vid.height + delta + 5) / 10;
	height *= 10;
	if (delta < 0 && height < 30)
		height = 30;
	if (delta > 0 && height > vid.height - 10)
		height = vid.height - 10;
	Cvar_SetValue ("scr_consize", (float)height / vid.height);
}



void Key_Extra (int *key)
{
	if (keydown[K_CTRL])
	{
		if (*key >= '0' && *key <= '9')
		{
			*key = *key - '0' + 0x12;	// yellow number
		}
		else
		{
			switch (*key)
			{
			case '[': *key = 0x10; break;
			case ']': *key = 0x11; break;
			case 'g': *key = 0x86; break;
			case 'r': *key = 0x87; break;
			case 'y': *key = 0x88; break;
			case 'b': *key = 0x89; break;
			case '(': *key = 0x80; break;
			case '=': *key = 0x81; break;
			case ')': *key = 0x82; break;
			case 'a': *key = 0x83; break;
			case '<': *key = 0x1d; break;
			case '-': *key = 0x1e; break;
			case '>': *key = 0x1f; break;
			case ',': *key = 0x1c; break;
			case '.': *key = 0x9c; break;
			case 'B': *key = 0x8b; break;
			case 'C': *key = 0x8d; break;
			}
		}
	}

	if (keydown[K_ALT])
		*key |= 0x80;		// red char
}

/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
void Key_Console (int key)
{
	int		i;
	char	*cmd;
	extern cvar_t con_textsize;
	extern cvar_t vid_width;
	extern cvar_t vid_conwidth;

	void VID_Menu_CalcConTextSize (float cw, float w);

	switch (key)
	{
		case K_ENTER:

			Cbuf_AddText (key_lines[edit_line] + 1);	// skip the "]"
			Cbuf_AddText ("\n");
			Con_Printf ("%s\n", key_lines[edit_line]);
		// joe: don't save same commands multiple times
			if (strcmp(key_lines[edit_line-1], key_lines[edit_line]))
				edit_line = (edit_line + 1) & 63;
			history_line = edit_line;
			key_lines[edit_line][0] = ']';
			key_lines[edit_line][1] = 0;
			key_linepos = 1;
			if (cls.state == ca_disconnected)
				SCR_UpdateScreen ();	// force an update, because the command
							// may take some time
			return;

		case K_TAB:
			// various parameter completions -- by joe
			cmd = key_lines[edit_line] + 1;
			if (strstr(cmd, "playdemo ") == cmd || strstr(cmd, "capture_start ") == cmd || 
				strstr(cmd, "capturedemo ") == cmd)
				Cmd_CompleteParameter (cmd, "*.dem");
			else if (strstr(cmd, "printtxt ") == cmd)
				Cmd_CompleteParameter (cmd, "*.txt");
			else if (strstr(cmd, "map ") == cmd)
				Cmd_CompleteParameter (cmd, "*.bsp");
			else if (strstr(cmd, "exec ") == cmd)
				Cmd_CompleteParameter (cmd, "*.cfg");
			else if (strstr(cmd, "load ") == cmd)
				Cmd_CompleteParameter (cmd, "*.sav");
			else if (strstr(cmd, "loadsky ") == cmd || strstr(cmd, "r_skybox ") == cmd || 
				 strstr(cmd, "loadcharset ") == cmd || strstr(cmd, "gl_consolefont ") == cmd || 
				 strstr(cmd, "crosshairimage ") == cmd)
				Cmd_CompleteParameter (cmd, "*.tga");
			else if (strstr(cmd, "gamedir ") == cmd)
				Cmd_CompleteParameter (cmd, "*");
			else
			{	// command completion
				if (cl_advancedcompletion.value)
				{
					CompleteCommand ();
				}
				else
				{
					char	*p;

					if (!(p = Cmd_CompleteCommand(cmd)))
						p = Cvar_CompleteVariable (cmd);

					if (p)
					{
						strcpy (cmd, p);
						key_linepos = strlen(p) + 1;
						key_lines[edit_line][key_linepos++] = ' ';
						key_lines[edit_line][key_linepos] = 0;
					}
				}
			}
			return;
		
		case K_BACKSPACE:
			if ((keydown[K_ALT]) || (keydown[K_CTRL]))
			{
				VID_Menu_CalcConTextSize(vid_conwidth.value, vid_width.value);//R00k: Reset to the dynamic default based on the resolution.
				return;
			}

			if (key_linepos > 1)
			{
				strcpy (key_lines[edit_line] + key_linepos - 1, key_lines[edit_line] + key_linepos);
				key_linepos--;
			}
			return;

		case K_DEL:
			if (key_linepos < strlen(key_lines[edit_line]))
				strcpy (key_lines[edit_line] + key_linepos, key_lines[edit_line] + key_linepos + 1);
			return;

		case K_LEFTARROW:
			if (keydown[K_CTRL])
			{
				// word left
				while (key_linepos > 1 && key_lines[edit_line][key_linepos-1] == ' ')
					key_linepos--;
				while (key_linepos > 1 && key_lines[edit_line][key_linepos-1] != ' ')
					key_linepos--;
				return;
			}
			if (key_linepos > 1)
				key_linepos--;
			return;

		case K_RIGHTARROW:
			if (keydown[K_CTRL])
			{
				// word right
				i = strlen (key_lines[edit_line]);
				while (key_linepos < i && key_lines[edit_line][key_linepos] != ' ')
					key_linepos++;
				while (key_linepos < i && key_lines[edit_line][key_linepos] == ' ')
					key_linepos++;
				return;
			}
			if (key_linepos < strlen(key_lines[edit_line]))
				key_linepos++;
			return;

		case K_INS:
			key_insert ^= 1;
			return;

		case K_UPARROW:
			if (keydown[K_CTRL])
			{
				AdjustConsoleHeight (-10);
				return;
			}

			if (keydown[K_ALT])
			{
				Cvar_SetValue("con_textsize", (con_textsize.value + 1));
				return;
			}

			do {
				history_line = (history_line - 1) & 63;
			} while (history_line != edit_line && !key_lines[history_line][1]);
			if (history_line == edit_line)
				history_line = (edit_line + 1) & 63;

			strcpy (key_lines[edit_line], key_lines[history_line]);
			key_linepos = strlen (key_lines[edit_line]);
			return;

		case K_DOWNARROW:
			if (keydown[K_CTRL])
			{
				AdjustConsoleHeight (10);
				return;
			}
			
			if (keydown[K_ALT])
			{
				Cvar_SetValue("con_textsize", (con_textsize.value - 1));
				return;
			}

			if (history_line == edit_line)
				return;
			do 
			{
				history_line = (history_line + 1) & 63;
			} 
			while (history_line != edit_line && !key_lines[history_line][1]);

			if (history_line == edit_line)
			{
				key_lines[edit_line][0] = ']';
				key_lines[edit_line][1] = 0;
				key_linepos = 1;
			}
			else
			{
				strcpy (key_lines[edit_line], key_lines[history_line]);
				key_linepos = strlen (key_lines[edit_line]);
			}
			return;

		case K_PGUP:
			if (keydown[K_CTRL])
				con_backscroll += ((int)scr_conlines-16)>>3;
			else
				con_backscroll += 2;
			if (con_backscroll > con_numlines)
				con_backscroll = con_numlines;
			return;

		case K_MWHEELUP:
			if (keydown[K_CTRL])
			{
				Cvar_SetValue("con_textsize", (con_textsize.value + 1));
				return;
			}
			else
			{
				con_backscroll += 2;
				if (con_backscroll > con_numlines)
					con_backscroll = con_numlines;
			}
			return;

		case K_PGDN:
			if (keydown[K_CTRL] && key == K_PGDN)
				con_backscroll -= ((int)scr_conlines-16)>>3;
			else
				con_backscroll -= 2;
			if (con_backscroll < 0)
				con_backscroll = 0;
			return;

		case K_MWHEELDOWN:
			if (keydown[K_CTRL])
			{
				Cvar_SetValue("con_textsize", (con_textsize.value - 1));
				return;
			}
			else
			{
				con_backscroll -= 2;
				if (con_backscroll < 0)
					con_backscroll = 0;
			}
			return;

		case K_HOME:
			if (keydown[K_CTRL])
			{
				con_backscroll = ((int)con_numlines - 1);
			}
			else
				key_linepos = 1;
			return;

		case K_END:
			if (keydown[K_CTRL])			
				con_backscroll = 0;			
			else
				key_linepos = strlen (key_lines[edit_line]);
			return;
	}
		
	if ((key == 'V' || key == 'v') && keydown[K_CTRL])
	{
		char	*cliptext;
		int		len;
			
		if ((cliptext = Sys_GetClipboardData()))
		{
			len = strlen (cliptext);
			if (len + strlen(key_lines[edit_line]) > MAXCMDLINE - 1)
				len = MAXCMDLINE - 1 - strlen(key_lines[edit_line]);
			if (len > 0)
			{	// insert the string
				memmove (key_lines[edit_line] + key_linepos + len, key_lines[edit_line] + key_linepos, strlen(key_lines[edit_line]) - key_linepos + 1);
				memcpy (key_lines[edit_line] + key_linepos, cliptext, len);
				key_linepos += len;
			}
		}
		return;
	}

	//R00k: using ctrl-x to erase current line.
	if ((key == 'X' || key == 'x') && keydown[K_CTRL])
	{
		key_lines[edit_line][1] = 0;	// clear any typing
		key_linepos = 1;
		return;
	}

	if (key < 32 || key > 127)
		return;	// non printable

	Key_Extra (&key);

	i = strlen (key_lines[edit_line]);
	if (i >= MAXCMDLINE-1)
		return;

	// This also moves the ending \0
	memmove (key_lines[edit_line]+key_linepos+1, key_lines[edit_line]+key_linepos, i-key_linepos+1);
	key_lines[edit_line][key_linepos] = key;
	key_linepos++;
}

//============================================================================

#define	MAX_CHAT_SIZE	45
char		chat_buffer[MAX_CHAT_SIZE];
qboolean	team_message = false;

void Key_Message (int key)
{
	static	int	chat_bufferlen = 0;

	if (key == K_ENTER)
	{
		if (team_message)
			Cbuf_AddText ("say_team \"");
		else
			Cbuf_AddText ("say \"");
		Cbuf_AddText (chat_buffer);
		Cbuf_AddText ("\"\n");

		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key == K_ESCAPE)
	{
		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key < 32 || key > 127)
		return;	// non printable

	if (key == K_BACKSPACE)
	{
		if (chat_bufferlen)
		{
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if (chat_bufferlen == MAX_CHAT_SIZE - (team_message ? 3 : 1))
		return;	// all full

	chat_buffer[chat_bufferlen++] = key;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================

/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int Key_StringToKeynum (char *str)
{
	keyname_t	*kn;
	
	if (!str || !str[0])
		return -1;
	if (!str[1])
		return str[0];

	for (kn=keynames ; kn->name ; kn++)
	{
		if (!Q_strcasecmp(str,kn->name))
			return kn->keynum;
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char *Key_KeynumToString (int keynum)
{
	keyname_t	*kn;	
	static	char	tinystr[2];
	
	if (keynum == -1)
		return "<KEY NOT FOUND>";
	if (keynum > 32 && keynum < 127)
	{	// printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}
	
	for (kn=keynames ; kn->name ; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}

/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding (int keynum, char *binding)
{
	if (keynum == -1)
		return;

	if (keynum == K_CTRL || keynum == K_ALT || keynum == K_SHIFT || keynum == K_WIN)
	{		
		Key_SetBinding (keynum + 1, binding);
		Key_SetBinding (keynum + 2, binding);
		return;
	}

	// free old bindings
	if (keybindings[keynum])
	{
		Z_Free (keybindings[keynum]);
		keybindings[keynum] = NULL;
	}
			
	// allocate memory for new binding
	keybindings[keynum] = CopyString (binding);	
}

/*
===================
Key_Unbind
===================
*/
void Key_Unbind (int keynum)
{
	if (keynum == -1)
		return;

	if (keynum == K_CTRL || keynum == K_ALT || keynum == K_SHIFT || keynum == K_WIN)
	{		
		Key_Unbind (keynum + 1);
		Key_Unbind (keynum + 2);
		return;
	}

	if (keybindings[keynum])
	{
		Z_Free (keybindings[keynum]);
		keybindings[keynum] = NULL;
	}
}

/*
===================
Key_Unbind_f
===================
*/
void Key_Unbind_f (void)
{
	int	b;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage:  %s <key> : remove commands from a key\n", Cmd_Argv(0));
		return;
	}
	
	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b == -1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_Unbind (b);
}

void Key_Unbindall_f (void)
{
	int	i;
	
	for (i=0 ; i<256 ; i++)
		if (keybindings[i])
			Key_Unbind (i);
}

static void Key_PrintBindInfo (int keynum, char *keyname)
{
	char	*n;

	n = Cmd_Argv(0);

	if (!keyname)
		keyname = Key_KeynumToString (keynum);

	if (keynum == -1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", keyname);
		return;
	}
//R00k edit old definition
	if (keybindings[keynum])
	{
		//Con_Printf ("\"%s\" = \"%s\"\n", keyname, keybindings[keynum]);		
		Con_Printf ("* Use UNBIND %s to clear\n", keyname);
		sprintf (n,"bind %s \"%s", keyname, keybindings[keynum]);

		strcpy (key_lines[edit_line]+1, n);
		key_linepos = (strlen(n)+1);
		key_lines[edit_line][key_linepos++] = '\"';
		key_lines[edit_line][key_linepos] = 0;
	}
	else
		Con_Printf ("\"%s\" is not bound\n", keyname);
}

/*
===================
Key_Bind_f
===================
*/
void Key_Bind_f (void)
{
	int	i, c, b;
	char	cmd[1024];
	char	*n;
	
	c = Cmd_Argc();
	n = Cmd_Argv(0);

	if (c != 2 && c != 3)
	{
		Con_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv(1));
	
	if (b == -1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2)
	{
		if ((b == K_CTRL || b == K_ALT || b == K_SHIFT || b == K_WIN) && (keybindings[b+1] || keybindings[b+2]))
		{
			if (keybindings[b+1] && keybindings[b+2] && !strcmp(keybindings[b+1], keybindings[b+2]))
			{
				//Con_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[b+1]);
				Con_Printf ("* Use UNBIND %s to clear\n", Cmd_Argv(1));
				sprintf (n,"bind %s \"%s", Cmd_Argv(1), keybindings[b+1]);

				strcpy (key_lines[edit_line]+1, n);
				key_linepos = (strlen(n)+1);
				key_lines[edit_line][key_linepos++] = '\"';
				key_lines[edit_line][key_linepos] = 0;	
			}
			else
			{
				Key_PrintBindInfo (b + 1, NULL);
				Key_PrintBindInfo (b + 2, NULL);
			}
		}
		else
		{
			// and the following should print "ctrl (etc) is not bound" since K_CTRL cannot be bound
			Key_PrintBindInfo (b, Cmd_Argv(1));
		}
		return;
	}
	
// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	for (i=2 ; i<c ; i++)
	{
		if (i > 2)
			strcat (cmd, " ");
		strcat (cmd, Cmd_Argv(i));
	}

	Key_SetBinding (b, cmd);
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings (FILE *f)
{
	int	i;

	for (i=0 ; i<256 ; i++)
		if (keybindings[i])
			if (*keybindings[i])
				fprintf (f, "bind \"%s\" \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
}

cvar_t freelooking = {"freelooking","0"};//R00k - added to allow freelooking in demos
cvar_t freemoving = {"freemoving", "0"};//R00k - added to allow freemoving in demos

/*
===================
Key_Init
===================
*/
void Key_Init (void)
{
// joe: added stuff from [sons]Quake
	int		i, j;
	FILE		*cmdhist;
	cmdhistory_t	cmdhistory;

	if ((cmdhist = fopen("qrack/cmdhist.dat", "rb")))
	{
		fread (&cmdhistory, sizeof(cmdhistory_t), 1, cmdhist);
		fclose (cmdhist);
		for (i=0 ; i<64 ; i++)
			for (j=0 ; j<MAXCMDLINE ; j++)
				key_lines[i][j] = cmdhistory.key_lines[i][j];
		key_linepos = cmdhistory.key_linepos;
		history_line = cmdhistory.history_line;
		edit_line = cmdhistory.edit_line;
	}
	else
	{
		for (i=0 ; i<64 ; i++)
		{
			key_lines[i][0] = ']';
			key_lines[i][1] = 0;
		}
		key_linepos = 1;
	}
	
// init ascii characters in console mode
	for (i=32 ; i<128 ; i++)
		consolekeys[i] = true;
	consolekeys[K_ENTER] = true;
	consolekeys[K_TAB] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_INS] = true;
	consolekeys[K_DEL] = true;
	consolekeys[K_HOME] = true;
	consolekeys[K_END] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_ALT] = true;
	consolekeys[K_LALT] = true;
	consolekeys[K_RALT] = true;
	consolekeys[K_CTRL] = true;
	consolekeys[K_LCTRL] = true;
	consolekeys[K_RCTRL] = true;
	consolekeys[K_SHIFT] = true;
	consolekeys[K_LSHIFT] = true;
	consolekeys[K_RSHIFT] = true;
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;
	//R00k added these
	consolekeys[K_CAPSLOCK] = true;	
	consolekeys[KP_NUMLOCK] = true;
	consolekeys[KP_SLASH] = true;
	consolekeys[KP_STAR] = true;
	consolekeys[KP_HOME] = true;
	consolekeys[KP_UPARROW] = true;
	consolekeys[KP_PGUP] = true;
	consolekeys[KP_MINUS] = true;
	consolekeys[KP_LEFTARROW] = true;
	consolekeys[KP_5] = true;
	consolekeys[KP_RIGHTARROW] = true;
	consolekeys[KP_PLUS] = true;
	consolekeys[KP_END] = true;
	consolekeys[KP_DOWNARROW] = true;
	consolekeys[KP_PGDN] = true;
	consolekeys[KP_INS] = true;
	consolekeys[KP_DEL] = true;
	consolekeys[KP_ENTER] = true;
	consolekeys['`'] = false;
	consolekeys['~'] = false;

	for (i=0 ; i<256 ; i++)
		keyshift[i] = i;
	for (i='a' ; i<='z' ; i++)
		keyshift[i] = i - 'a' + 'A';
	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';

	menubound[K_ESCAPE] = true;
	for (i=0 ; i<12 ; i++)
		menubound[K_F1+i] = true;

// register our functions
	Cmd_AddCommand ("bind", Key_Bind_f);
	Cmd_AddCommand ("unbind", Key_Unbind_f);
	Cmd_AddCommand ("unbindall", Key_Unbindall_f);
}

/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void Key_Event (int key, qboolean down)
{
	char	*kb, cmd[1024];
	static int oldkey;
//extern cvar_t	chase_back, chase_up, chase_right;

	if (key == K_LALT || key == K_RALT)
		Key_Event (K_ALT, down);
	else if (key == K_LCTRL || key == K_RCTRL)
		Key_Event (K_CTRL, down);
	else if (key == K_LSHIFT || key == K_RSHIFT)
		Key_Event (K_SHIFT, down);
	else 
	if ((key == K_LWIN || key == K_RWIN))
		Key_Event (K_WIN, down);

	keydown[key] = down;

	if (!down)
		key_repeats[key] = 0;

	key_lastpress = key;
	key_count++;
	if (key_count <= 0)
		return;		// just catching keys for Con_NotifyBox

// update auto-repeat status
	if (down)
	{
		key_repeats[key]++;
		if (key_repeats[key] > 1)	// joe: modified to work as ZQuake
		{
			if ((key != K_BACKSPACE && key != K_DEL && key != K_LEFTARROW && 
			     key != K_RIGHTARROW && key != K_UPARROW && key != K_DOWNARROW && 
			     key != K_PGUP && key != K_PGDN && (key < 32 || key > 126 || key == '`')) || 
			    (key_dest == key_game && cls.state == ca_connected))
				return;	// ignore most autorepeats
		}

		if (key >= 200 && !keybindings[key])			
		{
			if (oldkey != key)//R00k:if pressing twice in a row ++ dont spam, but still warn.
			{
				if ((key != K_MWHEELUP) && (key != K_MWHEELDOWN))//Dont spam console, if mousewheel isnt bound and used to scroll!
				Con_Printf ("%s is unbound, hit F4 to set.\n", Key_KeynumToString(key));//this is kinda outdated...
			}
		}
	}

	// switch between fullscreen/windowed when ALT+ENTER is pressed
	if( key == K_ENTER && down && keydown[K_ALT])
	{/*use cvars for user defaults
		Cvar_Set ("vid_width", va("%i", 640));
		Cvar_Set ("vid_height", va("%i",480));
	*/
		Cbuf_AddText("toggle vid_fullscreen;vid_restart;\n");
		Cbuf_Execute();
		return;
	}

	// handle escape specialy, so the user can never unbind it
	if (key == K_ESCAPE)
	{
		if (!down)
			return;
		switch (key_dest)
		{
			case key_message:
				Key_Message (key);
				break;

			case key_menu:
				M_Keydown (key);
				break;

			case key_game:
			case key_console:
				M_ToggleMenu_f ();
				break;

			default:
				Sys_Error ("Bad key_dest");
		}
		return;
	}

// key up events only generate commands if the game key binding is
// a button command (leading + sign).  These will occur even in console mode,
// to keep the character from continuing an action started before a console
// switch.  Button commands include the kenum as a parameter, so multiple
// downs can be matched with ups
	if (!down)
	{
			kb = keybindings[key];

			if (kb && kb[0] == '+')
			{
				Q_snprintfz (cmd, sizeof(cmd), "-%s %i\n", kb+1, key);
				Cbuf_AddText (cmd);
			}

			if (keyshift[key] != key)
			{
				kb = keybindings[keyshift[key]];
				if (kb && kb[0] == '+')
				{
					Q_snprintfz (cmd, sizeof(cmd), "-%s %i\n", kb+1, key);
					Cbuf_AddText (cmd);
				}
			}			
			return;

	}

	if (cls.demoplayback && key==K_MOUSE1 && !freemoving.value)
	{
		if(freelooking.value)
		{
			Con_Printf("Demo Freelook Disabled\n");
			freelooking.value = 0;//Cvar_Set("freelooking","0");
		}
		else
		{
			Con_Printf("Demo Freelook Enabled\n");
			freelooking.value = 1;//Cvar_Set("freelooking","1");
		}
	}
	else if (cls.demoplayback && key==K_MOUSE2) //R00k - added for freemove
	{
		if(freemoving.value)
		{
			Con_Printf("Freemove Disabled\n");
			freemoving.value = 0;//Cvar_Set("freemoving", "0");
			freelooking.value = 0;//Cvar_Set("freelooking", "0");
		}
		else
		{
			Con_Printf("Freemove Enabled\n");
			freemoving.value = 1;//Cvar_Set("freemoving", "1");
			freelooking.value = 1;//Cvar_Set("freelooking", "1");
			VectorCopy(cl_entities[cl.viewentity].origin, cls.demoentity.origin);
		}
	}

// during demo playback, most keys bring up the main menu
// joe: no, thanks :)
/*
	if (cls.demoplayback && down && consolekeys[key] && key_dest == key_game)
	{
		M_ToggleMenu_f ();
		return;
	}
*/

// R00k: Left/right arrowkeys Rewind/FastForward Demo Position :)
	
	if ((cls.demoplayback) && (key_dest == key_game))
	{
		switch (key)
		{	
			case K_LEFTARROW:
				Cvar_Set ("cl_demorewind", "1");				
				break;

			case K_RIGHTARROW:
				Cvar_Set ("cl_demorewind", "0");				
				break;

			case K_UPARROW:				
				Cvar_SetValue ("cl_demospeed", cl_demospeed.value + 0.1);
				break;

			case K_DOWNARROW:
				Cvar_SetValue ("cl_demospeed", cl_demospeed.value - 0.1);
				break;
				
			case K_ENTER:

				Cvar_Set ("cl_demospeed", "1");
				Cvar_Set ("cl_demorewind", "0");				
				break;
		}
	}
	// if not a consolekey, send to the interpreter no matter what mode is
	if ((key_dest == key_menu && menubound[key]) || (key_dest == key_console && !consolekeys[key]) || (key_dest == key_game && (!con_forcedup || !consolekeys[key])))
	{
		if ((kb = keybindings[key]))
		{
			if (kb[0] == '+')
			{	// button commands add keynum as a parm
				sprintf (cmd, "%s %i\n", kb, key);
				Cbuf_AddText (cmd);
			}
			else
			{
				Cbuf_AddText (kb);
				Cbuf_AddText ("\n");
			}
		}
		return;
	}

	if (!down)
		return;		// other systems only care about key down events

	if (keydown[K_SHIFT])
		key = keyshift[key];

	switch (key_dest)
	{
		case key_message:
			Key_Message (key);
			break;

		case key_menu:
			M_Keydown (key);
			break;

		case key_game:
		case key_console:
			Key_Console (key);
			break;

		default:
			Sys_Error ("Bad key_dest");
	}
}

/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates (void)
{
	int		i;

	for (i=0 ; i<256 ; i++)
	{
		keydown[i] = false;
		key_repeats[i] = 0;
	}
}
