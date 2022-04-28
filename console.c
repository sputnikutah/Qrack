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
// console.c

#ifndef _MSC_VER
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "quakedef.h"

int 		con_linewidth;

float		con_cursorspeed = 6;	// joe: increased to make it blink faster

#define		CON_BUFFSIZE	131072 //r00K: Upped from 16384(quake)->65536(proQuake)->131072(DP) 

qboolean 	con_forcedup;		// because no entities to refresh

int		con_totallines;		// total lines in console scrollback
int		con_numlines = 0;	// number of non-blank text lines, used for backscrolling, added by joe
int		con_backscroll;		// lines up from bottom to display
int		con_current;		// where next message will be printed
int		con_x;			// offset in current line for next print
char		*con_text = 0;

cvar_t		con_notifytime = {"con_notifytime","3"};		//seconds
cvar_t		_con_notifylines = {"con_notifylines","4"};
cvar_t		_con_notify_position = {"con_notify_position","0"};
cvar_t		cl_mute = {"cl_mute","0",false};//R00k
cvar_t		con_logcenterprint = {"con_logcenterprint", "0"}; //johnfitz	R00k: default this OFF for fuck sakes!
char		con_lastcenterstring[1024]; //johnfitz

cvar_t		pq_confilter = {"pq_confilter", "0"};	// JPG 1.05 - filter out the "you got" messages
cvar_t		pq_timestamp = {"pq_timestamp", "0"};	// JPG 1.05 - timestamp player binds during a match
cvar_t		pq_removecr = {"pq_removecr", "1"};		// JPG 3.20 - remove \r from console output
cvar_t		con_clear_input_on_toggle = {"con_clear_input_on_toggle","0"};	// R00k, erase console input line when toggling the console
cvar_t		con_clear_key_state = {"con_clear_key_state","0"};				// R00k, Let off keys held down when pressing ~

cvar_t		con_nomsgdupe = {"con_nomsgdupe","0"};//sschm

#define	NUM_CON_TIMES 16
float		con_times[NUM_CON_TIMES];	// realtime time the line was generated
						// for transparent notify lines

int			con_vislines;
int			con_notifylines;		// scan lines to clear for notify lines

qboolean	con_debuglog = false;
qboolean	con_debugconsole = false;

extern	char	key_lines[64][MAXCMDLINE];
extern	int	edit_line;
extern	int	key_linepos;
extern	int	key_insert;
extern void ClearAllStates (void);	
extern qboolean cl_mm2;//R00k added for cl_mute	

qboolean	con_initialized = false;
char logfilename[128];	// JPG - support for different filenames
/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	if (con_clear_input_on_toggle.value)
	{
		key_lines[edit_line][1] = 0;	// clear any typing
		key_linepos = 1;
	}

	if (key_dest == key_console)
	{		
		if (cls.state == ca_connected)
		{
			key_dest = key_game;			
		}
		else
		{
			M_Menu_Main_f ();
		}
	}
	else
	{
		key_dest = key_console;
		con_backscroll = 0; // JPG - don't want to enter console with backscroll
	}
	
	if (con_clear_key_state.value)
	{
		ClearAllStates();//R00k: if a +alias was interrupted by toggleconsole, then execute the -alias. (also bonks the mouse)...
	}

	SCR_EndLoadingPlaque ();
	memset (con_times, 0, sizeof(con_times));
}


/*
================
Con_Dump_f -- johnfitz -- adapted from quake2 source	
================
*/
void Con_Dump_f (void)
{
	int		l, x;
	char	*line;
	FILE	*f;
	char	buffer[1024];
	char	name[MAX_OSPATH];

#if 1
	//johnfitz -- there is a security risk in writing files with an arbitrary filename. so,
	//until stuffcmd is crippled to alleviate this risk, just force the default filename.
	sprintf (name, "%s/condump.txt", com_gamedir);
#else
	if (Cmd_Argc() > 2)
	{
		Con_Printf ("usage: condump <filename>\n");
		return;
	}

	if (Cmd_Argc() > 1)
	{
		if (strstr(Cmd_Argv(1), ".."))
		{
			Con_Printf ("Relative pathnames are not allowed.\n");
			return;
		}
		sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));
		COM_DefaultExtension (name, ".txt");
	}
	else
		sprintf (name, "%s/condump.txt", com_gamedir);
#endif

	COM_CreatePath (name);
	f = fopen (name, "w");
	if (!f)
	{
		Con_Printf ("ERROR: couldn't open file.\n", name);
		return;
	}

	// skip initial empty lines
	for (l = con_current - con_totallines + 1 ; l <= con_current ; l++)
	{
		line = con_text + (l%con_totallines)*con_linewidth;
		for (x=0 ; x<con_linewidth ; x++)
			if (line[x] != ' ')
				break;
		if (x != con_linewidth)
			break;
	}

	// write the remaining lines
	buffer[con_linewidth] = 0;
	for ( ; l <= con_current ; l++)
	{
		line = con_text + (l%con_totallines)*con_linewidth;
		strncpy (buffer, line, con_linewidth);
		for (x=con_linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x=0; buffer[x]; x++)
			buffer[x] &= 0x7f;

		fprintf (f, "%s\n", buffer);
	}

	fclose (f);
	Con_Printf ("Dumped console text to %s.\n", name);
}

/*
================
Con_Copy_f -- Baker -- adapted from Con_Dump
================
*/
void Con_Copy_f (void)
{
	char	outstring[CON_BUFFSIZE]="";
	int		l, x;
	char	*line;
	char	buffer[1024];
	extern void Sys_CopyToClipboard(char *text);

	// skip initial empty lines
	for (l = con_current - con_totallines + 1 ; l <= con_current ; l++)
	{
		line = con_text + (l%con_totallines)*con_linewidth;
		for (x=0 ; x<con_linewidth ; x++)
			if (line[x] != ' ')
				break;
		if (x != con_linewidth)
			break;
	}

	// write the remaining lines
	buffer[con_linewidth] = 0;
	for ( ; l <= con_current ; l++)
	{
		line = con_text + (l%con_totallines)*con_linewidth;
		strncpy (buffer, line, con_linewidth);
		for (x=con_linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x=0; buffer[x]; x++)
			buffer[x] &= 0x7f;

		strcat(outstring, va("%s\r\n", buffer));

	}

	Sys_CopyToClipboard(outstring);
	Con_Printf ("Copied console to clipboard\n");
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	if (con_text)
		memset (con_text, ' ', CON_BUFFSIZE);
}

/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;
	
	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con_times[i] = 0;
}

/*
================
Con_MessageMode_f
================
*/
extern qboolean team_message;

void Con_MessageMode_f (void)
{
	if (key_dest == key_console)//R00k
	{
		Con_ToggleConsole_f();
	}

	key_dest = key_message;
	team_message = false;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	if (key_dest == key_console)//R00k
	{
		Con_ToggleConsole_f();
	}

	key_dest = key_message;
	team_message = true;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
extern cvar_t	con_textsize;
extern cvar_t vid_conwidth;
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_BUFFSIZE];

	//from DarkPlaces
	width = (int)floor(vid_conwidth.value / con_textsize.value);
	width = bound(1, width, 1024);

	if (width == con_linewidth)
		return;

	if (width < 38)			// video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = CON_BUFFSIZE / con_linewidth;
		memset (con_text, ' ', CON_BUFFSIZE);
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_BUFFSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;
	
		if (con_linewidth < numchars)
			numchars = con_linewidth;

		memcpy (tbuf, con_text, CON_BUFFSIZE);
		memset (con_text, ' ', CON_BUFFSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con_text[(con_totallines - 1 - i) * con_linewidth + j] =
					tbuf[((con_current - i + oldtotallines) % oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}


/*
================
Con_Quakebar -- johnfitz -- returns a bar of the desired length, but never wider than the console

includes a newline, unless len >= con_linewidth.
================
*/
char *Con_Quakebar (int len)
{
	static char bar[42];
	int i;

	len = min(len, sizeof(bar) - 2);
	len = min(len, con_linewidth);

	bar[0] = '\35';
	for (i = 1; i < len - 1; i++)
		bar[i] = '\36';
	bar[len-1] = '\37';

	if (len < con_linewidth)
	{
		bar[len] = '\n';
		bar[len+1] = 0;
	}
	else
		bar[len] = 0;

	return bar;
}

/*
================
Con_Init
================
*/
void Con_Init (void)
{
#define MAXGAMEDIRLEN	1000
	char	temp[MAXGAMEDIRLEN+1];
	// char	*t2 = "/qconsole.log"; // JPG - don't need this
	char	*ch;	// JPG - added this
	int		fd, n;	// JPG - added these
    time_t  ltime;		// JPG - for console log file

	con_debuglog = COM_CheckParm("-condebug");

	if (con_debuglog)
	{
		// JPG - check for different file name
		if ((con_debuglog < com_argc - 1) && (*com_argv[con_debuglog+1] != '-') && (*com_argv[con_debuglog+1] != '+'))
		{
			if ((ch = strchr(com_argv[con_debuglog+1], '%')) && (ch[1] == 'd'))
			{
				n = 0;
				do
				{
					n = n + 1;
					_snprintf(logfilename, sizeof(logfilename), com_argv[con_debuglog+1], n);
					strcat_s (logfilename, sizeof(logfilename), ".log");
					_snprintf (temp, sizeof(temp), va("%s/%s", com_gamedir, logfilename));
					fd = _open(temp, O_CREAT | O_EXCL | O_WRONLY, 0666);
				}
				while (fd == -1);
				_close(fd);
			}
			else
				_snprintf (logfilename, sizeof(logfilename), va("%s.log", com_argv[con_debuglog+1]));
		}
		else
			strcpy(logfilename, "qconsole.log");

		// JPG - changed t2 to logfilename
		if (strlen (com_gamedir) < (MAXGAMEDIRLEN - strlen (logfilename)))
		{			
			Con_DPrintf (1,"JPG: 'changed t2 to logfilename'.\n");
			_snprintf(temp, sizeof(temp), va("%s/%s", com_gamedir, logfilename)); // JPG - added the '/'
			_unlink (temp);
		}

		// JPG - print initial message
		Con_Printf("Log file initialized.\n");
		Con_Printf("%s/%s\n", com_gamedir, logfilename);
		time( &ltime );
		Con_Printf( "%s\n", ctime( &ltime ) );
	}

	con_text = Hunk_AllocName (CON_BUFFSIZE, "context");
	memset (con_text, ' ', CON_BUFFSIZE);
	con_linewidth = -1;
	Con_CheckResize ();

// register our commands
	Cvar_RegisterVariable (&con_notifytime);
	Cvar_RegisterVariable (&_con_notifylines);
	Cvar_RegisterVariable (&_con_notify_position);//R00k
	Cvar_RegisterVariable (&cl_mute);//R00k
	Cvar_RegisterVariable (&con_logcenterprint); //johnfitz
	Cvar_RegisterVariable (&con_clear_input_on_toggle);//R00k
	Cvar_RegisterVariable (&con_clear_key_state);//R00k
	Cvar_RegisterVariable (&con_nomsgdupe);//sschm
	Cvar_RegisterVariable (&pq_confilter);	// JPG 1.05 - make "you got" messages temporary
	Cvar_RegisterVariable (&pq_timestamp);	// JPG 1.05 - timestamp player binds during a match
	Cvar_RegisterVariable (&pq_removecr);	// JPG 3.20 - optionally remove '\r'
	
	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f); //johnfitz

	con_initialized = true;
	
	Con_Printf ("Console initialized\n");
}

/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	con_x = 0;
	con_current++;
	if (con_numlines < con_totallines)	// by joe
		con_numlines++;
	memset (&con_text[(con_current%con_totallines)*con_linewidth], ' ', con_linewidth);
			// JPG - fix backscroll
	if (con_backscroll)
		con_backscroll++;
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
#define DIGIT(x) ((x) >= '0' && (x) <= '9')

void Con_Print (char *txt)
{
	int		y, c, l, mask;
	static int	cr;
	static int fixline = 0;	
	
	//sschm 3/2/2017 3:34PM
	if (con_nomsgdupe.value)
	{		
		if (txt[0])
		{
			static char szOldText[16384];
			if (szOldText[0])
			{
				if (!_stricmp(szOldText, txt))
				{
					return;
				}
				RtlZeroMemory(szOldText, _countof(szOldText));
			}
			strncpy_s(szOldText, _countof(szOldText), txt, _TRUNCATE);
		}
	}	

	// JPG 1.05 - make the "You got" messages temporary
	if (pq_confilter.value)
		fixline |= !strcmp(txt, "You got armor\n") || 
				   !strcmp(txt, "You receive ") || 
				   !strcmp(txt, "You got the ") ||
				   !strcmp(txt, "no weapon.\n") ||
				   !strcmp(txt, "not enough ammo.\n");

	if (txt[0] == 1)
	{
		mask = 128;		// go to colored text
		
		//R00k cl_mute (0 no mute, 1 silent mm1, 2 silent all)
		switch ((int)cl_mute.value)
		{
			case 0:
					if (cl_mm2)					
						S_LocalSound ("misc/talk2.wav");					
					else
						S_LocalSound ("misc/talk.wav");
					break;
			case 1:
					if (cl_mm2)
					{
						S_LocalSound ("misc/talk2.wav");// play talk wav
					}
					break;
		}

		txt++;

		// JPG 1.05 - timestamp player binds during a match (unless the bind already has a time in it)
		if (!cls.demoplayback && (!cls.netcon || *txt == '(') && !con_x && pq_timestamp.value && (cl.minutes || cl.seconds) && cl.seconds < 128)		// JPG 3.30 - fixed old bug hit by some servers
		{
			int minutes, seconds, match_time, msg_time;
			char buff[16];
			char *ch;

			if (cl.match_pause_time)
				match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.match_pause_time - cl.last_match_time));
			else
				match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.time - cl.last_match_time));

			minutes = match_time / 60;
			seconds = match_time - 60 * minutes;

			msg_time = -10;
			ch = txt + 2;
			if (ch[0] && ch[1] && ch[2])
			{
				while (ch[2])
				{
					if (ch[0] == ':' && DIGIT(ch[1]) && DIGIT(ch[2]) && DIGIT(ch[-1]))
					{
						msg_time = 60 * (ch[-1] - '0') + 10 * (ch[1] - '0') + (ch[2] - '0');
						if (DIGIT(ch[-2]))
							msg_time += 600 * (ch[-2] - '0');
						break;
					}
					ch++;
				}
			}
			if (msg_time < match_time - 2 || msg_time > match_time + 2)
			{
				if (pq_timestamp.value == 1)
					sprintf(buff, "%d%c%02d", minutes, 'X' + 128, seconds);
				else
					sprintf(buff, "%02d", seconds);

				if (cr)
				{
					con_current--;
					cr = false;
				}
				Con_Linefeed ();
				if (con_current >= 0)
					con_times[con_current % NUM_CON_TIMES] = realtime;

				y = con_current % con_totallines;
				for (ch = buff ; *ch ; ch++)
					con_text[y*con_linewidth+con_x++] = *ch - 30;
				con_text[y*con_linewidth+con_x++] = ' ';
			}
		}
		//proquake end --JPG
	}
	else if (txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
	{
		mask = 0;
	}

	while ((c = *txt))
	{
	// count word length
		for (l = 0 ; l < con_linewidth ; l++)
			if (txt[l] <= ' ')
				break;

	// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth))
			con_x = 0;

		txt++;

		if (cr)
		{
			con_current--;
			cr = false;
		}

		if (!con_x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current % NUM_CON_TIMES] = realtime;
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			cr = fixline;	// JPG 1.05 - make the "you got" messages temporary
			fixline = 0;	// JPG
			break;

		case '\r':
			if (pq_removecr.value)	// JPG 3.20 - optionally remove '\r'
				c += 128;
			else
			{
				con_x = 0;
				cr = 1;
				break;
			}

		default:	// display character and advance
			y = con_current % con_totallines;
			con_text[y*con_linewidth+con_x] = c | mask;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}
	}
}

/*
================
Con_DebugLog
================
*/
void Con_DebugLog( /* char *file, */ char *fmt, ...)
{
	va_list	argptr; 
	static	char	data[16384];
	int		fd;

	va_start (argptr, fmt);
	vsnprintf (data, sizeof(data), fmt, argptr);
	va_end (argptr);
    fd = _open(va("%s/%s", com_gamedir, logfilename), O_WRONLY | O_CREAT | O_APPEND, 0666);
	
	if (fd != -1)//R00k: dynamic-gamedir-changing validity check
	{
		_write (fd, data, strlen(data));
		_close (fd);
	}
}

/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
#define	MAXPRINTMSG	16384
void Con_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	extern cvar_t	cl_nomessageprint;

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);
	
	// also echo to debugging console
	Sys_Printf ("%s", msg);

	// log all messages to file
	if (con_debuglog)
		Con_DebugLog( /* va("%s/qconsole.log",com_gamedir), */ "%s", msg);  // JPG - got rid of filename

	if (!con_initialized)
		return;
		
	if (cls.state == ca_dedicated)
		return;		// no graphics mode

	if (cl_nomessageprint.value)//R00k
		return;

	// write it to the scrollable buffer
	Con_Print (msg);
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf (int l, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
		
	if ((developer.value >= 0) && (developer.value < l))// R00k: print warnings based on level
		return;			

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);
	
	if (con_debugconsole)
		fprintf (stderr, msg);
	else
	{
		if (l > 1)
			Con_Printf ("\x02%s", msg);//R00k: print dev text in red
		else
			Con_Printf ("%s", msg);
	}
}

/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafePrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[1024];
	int		temp;
		
	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	Con_Printf ("%s", msg);
	scr_disabled_for_loading = temp;
}

/*
================
Con_Warning -- johnfitz -- prints a warning to the console
================
*/
void Con_Warning (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if (!developer.value)
		return;			

	va_start (argptr, fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	Con_SafePrintf ("\x02Warning: ");
	Con_Printf ("%s", msg);
}

/*
================
Con_CenterPrintf -- johnfitz -- pad each line with spaces to make it appear centered
================
*/
void Con_CenterPrintf (int linewidth, char *fmt, ...)
{
	va_list	argptr;
	char	msg[MAXPRINTMSG]; //the original message
	char	line[MAXPRINTMSG]; //one line from the message
	char	spaces[21]; //buffer for spaces
	char	*src, *dst;
	int		len, s;

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	linewidth = min (linewidth, con_linewidth);
	for (src = msg; *src; )
	{
		dst = line;
		while (*src && *src != '\n')
			*dst++ = *src++;
		*dst = 0;
		if (*src == '\n')
			src++;

		len = strlen(line);
		if (len < linewidth)
		{
			s = (linewidth-len)/2;
			memset (spaces, ' ', s);
			spaces[s] = 0;
			Con_Printf ("%s%s\n", spaces, line);
		}
		else
			Con_Printf ("%s\n", line);
	}
}
/*
==================
Con_LogCenterPrint -- johnfitz -- echo centerprint message to the console
==================
*/
void Con_LogCenterPrint (char *str)
{
	if (!strcmp(str, con_lastcenterstring))
		return; //ignore duplicates

	if (cl.gametype == GAME_DEATHMATCH && con_logcenterprint.value != 2)
		return; //don't log in deathmatch

	strcpy(con_lastcenterstring, str);

	if (con_logcenterprint.value)
	{
		Con_Printf (Con_Quakebar(40));
		Con_CenterPrintf (40, "%s\n", str);
		Con_Printf (Con_Quakebar(40));
		Con_ClearNotify ();
	}
}

/*
==============================================================================

DRAWING

==============================================================================
*/

/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
// modified by joe to work as ZQuake
void Con_DrawInput (void)
{
	int		i;
	char	*text;
	char	temp[MAXCMDLINE];

	if (key_dest != key_console && !con_forcedup)
		return;		// don't draw anything

	text = strcpy (temp, key_lines[edit_line]);
	
	// fill out remainder with spaces
	for (i = strlen(text) ; i < MAXCMDLINE ; i++)
		text[i] = ' ';
		
	// add the cursor frame
	if ((int)(realtime * con_cursorspeed) & 1)
		text[key_linepos] = 11 + 84 * key_insert;
	
	// prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;
		
	// draw it
#ifdef GLQUAKE
	Draw_String (8, con_vislines - (con_textsize.value*2), text,8);
#else
	for (i=0 ; i<con_linewidth ; i++)
		Draw_Character ((i+1)<<3, con_vislines - 16, text[i],false);
#endif
}
/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v, i, maxlines;
	char	*text;
	float	time;

	extern	char	chat_buffer[];

	maxlines = bound(0, _con_notifylines.value, NUM_CON_TIMES);

	v = ((con_textsize.value) * (_con_notify_position.value));//R00k

	if (v > (vid.height - (con_textsize.value) * maxlines))
		v = (vid.height - (con_textsize.value) * maxlines);

	for (i = con_current - maxlines + 1 ; i <= con_current ; i++)
	{
		if (i < 0)
			continue;
		
		time = con_times[i%NUM_CON_TIMES];
		
		if (time < 0.001f)
			continue;
		
		time = realtime - time;
		
		if (time > con_notifytime.value)
			continue;

		text = con_text + (i % con_totallines) * con_linewidth;
		
		clearnotify = 0;
		scr_copytop = 1;

		if (con_notifytime.value - time < 1.0f) 
			Draw_Alpha_Start (con_notifytime.value - time);

		for (x = 0 ; x < con_linewidth ; x++)
			Draw_Character ((x+1)<<3, v, text[x],true);

		if (con_notifytime.value - time < 1.0f) 
			Draw_Alpha_End();

		v += lrintf(con_textsize.value);
	}

	if (key_dest == key_message)
	{
		clearnotify = 0;
		scr_copytop = 1;
	
		i = 0;
		
		if (team_message)
		{
			Draw_String (8, v, "(say_team):",8);//R00k changed to say_team, same as the command.
			x = 12;
		}
		else
		{
			Draw_String (8, v, "say:",8);
			x = 5;
		}

		while (chat_buffer[i])
		{
			Draw_Character (x << 3, v, chat_buffer[i],true);
			x++;

			i++;
			if (x > con_linewidth)
			{
				x = team_message ? 12 : 5;
				v += lrintf(con_textsize.value);
			}
		}
		Draw_Character (x << 3, v, 10 + ((lrint(realtime * con_cursorspeed)) & 1),true);
		v += lrintf(con_textsize.value);
	}
	
	if (v > con_notifylines)
		con_notifylines = v;
}

/*
================
Con_DrawConsole

Draws the console with the solid background
The typing input line at the bottom should only be drawn if typing is allowed
================
*/
void Con_DrawConsole (int lines, qboolean drawinput)
{
	int		i, j, x, y, rows, stop;
	char		*text;
	
	if (lines <= 0)
		return;

// draw the background
	Draw_ConsoleBackground (lines);

// draw the text
	con_vislines = lines;

	//from DarkPlaces
	rows = (int)ceil((lines/con_textsize.value)-2);		// rows of text to draw
	y = lines - (rows+2)*con_textsize.value;	// may start slightly negative

	stop = con_current;

	for (i = stop - rows + 1 ; i <= stop ; i++, y += lrintf(con_textsize.value))
	{
		j = i - con_backscroll;
		if (j < 0)
			j = 0;

		// added by joe
		if (con_backscroll && i == stop)
		{
			for (x = 0 ; x < con_linewidth ; x += 4)
				Draw_Character ((x+1)<<3, y, '^',true);
			continue;
		}

		text = con_text + (j % con_totallines)*con_linewidth;

		for (x = 0 ; x < con_linewidth ; x++)
			Draw_Character ((x+1)<<3, y, text[x],true);
	}

// draw the input prompt, user text, and cursor if desired
	if (drawinput)
		Con_DrawInput ();
}

/*
==================
Con_NotifyBox
==================
*/
void Con_NotifyBox (char *text)
{
	double		t1, t2;

// during startup for sound / cd warnings
	Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	Con_Printf (text);

	Con_Printf ("Press a key.\n");
	Con_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	key_count = -2;		// wait for a key down and up
	key_dest = key_console;

	do
	{
		t1 = Sys_DoubleTime ();
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();
		t2 = Sys_DoubleTime ();
		realtime += t2-t1;		// make the cursor blink
	} while (key_count < 0);

	Con_Printf ("\n");
	key_dest = key_game;
	realtime = 0;				// put the cursor back to invisible
}

