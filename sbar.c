
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
// sbar.c -- status bar code

#include "quakedef.h"

cvar_t	pq_teamscores		= {"pq_teamscores", "1",true}; // JPG - show teamscores
cvar_t	pq_timer			= {"pq_timer", "1",true}; // JPG - show timer
cvar_t	pq_scoreboard_pings = {"pq_scoreboard_pings", "1",true};	// JPG - show ping times in the scoreboard

int		sb_updates;		// if >= vid.numpages, no update needed

#define STAT_MINUS		10	// num frame for '-' stats digit
mpic_t		*sb_nums[2][11];
mpic_t		*sb_colon, *sb_slash;
mpic_t		*sb_ibar;
//mpic_t		*sb_ibar_ammo_icons;
mpic_t		*sb_sbar;
mpic_t		*sb_scorebar;

mpic_t		*sb_weapons[7][8];	// 0 is active, 1 is owned, 2-5 are flashes
mpic_t		*sb_ammo[4];
mpic_t		*sb_sigil[4];
mpic_t		*sb_armor[3];
mpic_t		*sb_items[32];

mpic_t	*sb_faces[5][2];		// 0 is dead, 1-4 are alive
							// 0 is static, 1 is temporary animation
mpic_t	*sb_face_invis;
mpic_t	*sb_face_quad;
mpic_t	*sb_face_invuln;
mpic_t	*sb_face_invis_invuln;

qboolean	sb_showscores;

int			sb_lines;			// scan lines to draw

mpic_t      *rsb_invbar[2];
mpic_t      *rsb_weapons[5];
mpic_t      *rsb_items[2];
mpic_t      *rsb_ammo[3];
mpic_t      *rsb_teambord;		// PGM 01/19/97 - team color border

//MED 01/04/97 added two more weapons + 3 alternates for grenade launcher
mpic_t	*hsb_weapons[7][5];   // 0 is active, 1 is owned, 2-5 are flashes
//MED 01/04/97 added array to simplify weapon parsing
int	hipweapons[4] = {HIT_LASER_CANNON_BIT, HIT_MJOLNIR_BIT, 4, HIT_PROXIMITY_GUN_BIT};
//MED 01/04/97 added hipnotic items array
mpic_t	*hsb_items[2];

void Sbar_MiniDeathmatchOverlay (void);
void Sbar_IntermissionNumber (int x, int y, int num, int digits, int color);
void Sbar_DeathmatchOverlay (void);

// by joe
int	sbar_xofs;
cvar_t	scr_centersbar			= {"scr_centersbar", "0",true};
cvar_t	scr_sbaralpha			= {"scr_sbaralpha",	"0.5",true};
cvar_t	scr_printstats			= {"scr_printstats", "0",true};
cvar_t	scr_printstats_style	= {"scr_printstats_style", "0",true};
cvar_t	scr_printstats_length	= {"scr_printstats_length", "0.5",true};
cvar_t	scr_scoreboard_fillalpha = {"scr_scoreboard_fillalpha", "0.8"};//R00k
//cvar_t	scr_scoreboard_fillcolor = {"scr_scoreboard_fillcolor", "0xffffff"};//R00k

extern	cvar_t gl_conalpha;

/*
===============
Sbar_ShowScores

Tab key down
===============
*/
void Sbar_ShowScores (void)
{
	if (sb_showscores)
		return;
	sb_showscores = true;
	sb_updates = 0;
}

/*
===============
Sbar_DontShowScores

Tab key up
===============
*/
void Sbar_DontShowScores (void)
{
	sb_showscores = false;
	sb_updates = 0;
}

/*
===============
Sbar_Changed
===============
*/
void Sbar_Changed (void)
{
	sb_updates = 0;	// update next frame
}

void Sbar_Hipnotic_Init (void)
{
	int	i;

	hsb_weapons[0][0] = Draw_PicFromWad ("inv_laser");
	hsb_weapons[0][1] = Draw_PicFromWad ("inv_mjolnir");
	hsb_weapons[0][2] = Draw_PicFromWad ("inv_gren_prox");
	hsb_weapons[0][3] = Draw_PicFromWad ("inv_prox_gren");
	hsb_weapons[0][4] = Draw_PicFromWad ("inv_prox");

	hsb_weapons[1][0] = Draw_PicFromWad ("inv2_laser");
	hsb_weapons[1][1] = Draw_PicFromWad ("inv2_mjolnir");
	hsb_weapons[1][2] = Draw_PicFromWad ("inv2_gren_prox");
	hsb_weapons[1][3] = Draw_PicFromWad ("inv2_prox_gren");
	hsb_weapons[1][4] = Draw_PicFromWad ("inv2_prox");

	for (i=0 ; i<5 ; i++)
	{
		hsb_weapons[2+i][0] = Draw_PicFromWad (va("inva%i_laser", i+1));
		hsb_weapons[2+i][1] = Draw_PicFromWad (va("inva%i_mjolnir", i+1));
		hsb_weapons[2+i][2] = Draw_PicFromWad (va("inva%i_gren_prox", i+1));
		hsb_weapons[2+i][3] = Draw_PicFromWad (va("inva%i_prox_gren", i+1));
		hsb_weapons[2+i][4] = Draw_PicFromWad (va("inva%i_prox", i+1));
	}

	hsb_items[0] = Draw_PicFromWad ("sb_wsuit");
	hsb_items[1] = Draw_PicFromWad ("sb_eshld");

// joe: better reload these, coz they might look different
	if (hipnotic)
	{
		sb_weapons[0][4] = Draw_PicFromWad ("inv_rlaunch");
		sb_weapons[1][4] = Draw_PicFromWad ("inv2_rlaunch");
		sb_items[0] = Draw_PicFromWad ("sb_key1");
		sb_items[1] = Draw_PicFromWad ("sb_key2");
	}
}

void Sbar_Rogue_Init (void)
{
	rsb_invbar[0] = Draw_PicFromWad ("r_invbar1");
	rsb_invbar[1] = Draw_PicFromWad ("r_invbar2");

	rsb_weapons[0] = Draw_PicFromWad ("r_lava");
	rsb_weapons[1] = Draw_PicFromWad ("r_superlava");
	rsb_weapons[2] = Draw_PicFromWad ("r_gren");
	rsb_weapons[3] = Draw_PicFromWad ("r_multirock");
	rsb_weapons[4] = Draw_PicFromWad ("r_plasma");

	rsb_items[0] = Draw_PicFromWad ("r_shield1");
	rsb_items[1] = Draw_PicFromWad ("r_agrav1");

// PGM 01/19/97 - team color border
	rsb_teambord = Draw_PicFromWad ("r_teambord");
// PGM 01/19/97 - team color border

	rsb_ammo[0] = Draw_PicFromWad ("r_ammolava");
	rsb_ammo[1] = Draw_PicFromWad ("r_ammomulti");
	rsb_ammo[2] = Draw_PicFromWad ("r_ammoplasma");
}

void Sbar_ClearWadPics (void)
{
	int	i, j;

	for (i = 0; i < 7; i++)
		for (j = 0; j < 8; j++)
			sb_weapons[i][j] = NULL;

	for (i = 0; i < 4; i++)
	{
		sb_ammo[i] = NULL;
		sb_sigil[i] = NULL;
	}

	for (i = 0; i < 3; i++)
		sb_armor[i] = NULL;

	for (i = 0; i < 32; i++)
		sb_items[i] = NULL;

	for (i = 0; i < 5; i++)
	{
		sb_faces[i][0] = NULL;
		sb_faces[i][1] = NULL;
	}

	sb_face_invis = sb_face_quad = sb_face_invuln = sb_face_invis_invuln = NULL;
	sb_sbar = sb_ibar = sb_scorebar = NULL;

// Rogue:
	rsb_invbar[0] = rsb_invbar[1] = NULL;
	rsb_items[0] = rsb_items[1] = NULL;
	rsb_ammo[0] = rsb_ammo[1] = rsb_ammo[2] = NULL;
	rsb_teambord = NULL;

	for (i = 0; i < 5; i++)
		rsb_weapons[i] = NULL;

// Hipnotic:
	for (i = 0; i < 7; i++)
		for (j = 0; j < 5; j++)
			hsb_weapons[i][j] = NULL;

	hsb_items[0] = hsb_items[1] = NULL;
}
/*
===============
Sbar_Init
===============
*/
void Sbar_Init (void)
{
	int	i;

	for (i=0 ; i<10 ; i++)
	{
		sb_nums[0][i] = Draw_PicFromWad (va("num_%i", i));
		sb_nums[1][i] = Draw_PicFromWad (va("anum_%i", i));
	}

	sb_nums[0][10] = Draw_PicFromWad ("num_minus");
	sb_nums[1][10] = Draw_PicFromWad ("anum_minus");

	sb_colon = Draw_PicFromWad ("num_colon");
	sb_slash = Draw_PicFromWad ("num_slash");

	sb_weapons[0][0] = Draw_PicFromWad ("inv_shotgun");
	sb_weapons[0][1] = Draw_PicFromWad ("inv_sshotgun");
	sb_weapons[0][2] = Draw_PicFromWad ("inv_nailgun");
	sb_weapons[0][3] = Draw_PicFromWad ("inv_snailgun");
	sb_weapons[0][4] = Draw_PicFromWad ("inv_rlaunch");
	sb_weapons[0][5] = Draw_PicFromWad ("inv_srlaunch");
	sb_weapons[0][6] = Draw_PicFromWad ("inv_lightng");

	sb_weapons[1][0] = Draw_PicFromWad ("inv2_shotgun");
	sb_weapons[1][1] = Draw_PicFromWad ("inv2_sshotgun");
	sb_weapons[1][2] = Draw_PicFromWad ("inv2_nailgun");
	sb_weapons[1][3] = Draw_PicFromWad ("inv2_snailgun");
	sb_weapons[1][4] = Draw_PicFromWad ("inv2_rlaunch");
	sb_weapons[1][5] = Draw_PicFromWad ("inv2_srlaunch");
	sb_weapons[1][6] = Draw_PicFromWad ("inv2_lightng");

	for (i=0 ; i<5 ; i++)
	{
		sb_weapons[2+i][0] = Draw_PicFromWad (va("inva%i_shotgun", i+1));
		sb_weapons[2+i][1] = Draw_PicFromWad (va("inva%i_sshotgun", i+1));
		sb_weapons[2+i][2] = Draw_PicFromWad (va("inva%i_nailgun", i+1));
		sb_weapons[2+i][3] = Draw_PicFromWad (va("inva%i_snailgun", i+1));
		sb_weapons[2+i][4] = Draw_PicFromWad (va("inva%i_rlaunch", i+1));
		sb_weapons[2+i][5] = Draw_PicFromWad (va("inva%i_srlaunch", i+1));
		sb_weapons[2+i][6] = Draw_PicFromWad (va("inva%i_lightng", i+1));
	}

	sb_ammo[0] = Draw_PicFromWad ("sb_shells");
	sb_ammo[1] = Draw_PicFromWad ("sb_nails");
	sb_ammo[2] = Draw_PicFromWad ("sb_rocket");
	sb_ammo[3] = Draw_PicFromWad ("sb_cells");

	sb_armor[0] = Draw_PicFromWad ("sb_armor1");
	sb_armor[1] = Draw_PicFromWad ("sb_armor2");
	sb_armor[2] = Draw_PicFromWad ("sb_armor3");

	sb_items[0] = Draw_PicFromWad ("sb_key1");
	sb_items[1] = Draw_PicFromWad ("sb_key2");
	sb_items[2] = Draw_PicFromWad ("sb_invis");
	sb_items[3] = Draw_PicFromWad ("sb_invuln");
	sb_items[4] = Draw_PicFromWad ("sb_suit");
	sb_items[5] = Draw_PicFromWad ("sb_quad");

	sb_sigil[0] = Draw_PicFromWad ("sb_sigil1");
	sb_sigil[1] = Draw_PicFromWad ("sb_sigil2");
	sb_sigil[2] = Draw_PicFromWad ("sb_sigil3");
	sb_sigil[3] = Draw_PicFromWad ("sb_sigil4");

	sb_faces[4][0] = Draw_PicFromWad ("face1");
	sb_faces[4][1] = Draw_PicFromWad ("face_p1");
	sb_faces[3][0] = Draw_PicFromWad ("face2");
	sb_faces[3][1] = Draw_PicFromWad ("face_p2");
	sb_faces[2][0] = Draw_PicFromWad ("face3");
	sb_faces[2][1] = Draw_PicFromWad ("face_p3");
	sb_faces[1][0] = Draw_PicFromWad ("face4");
	sb_faces[1][1] = Draw_PicFromWad ("face_p4");
	sb_faces[0][0] = Draw_PicFromWad ("face5");
	sb_faces[0][1] = Draw_PicFromWad ("face_p5");

	sb_face_invis = Draw_PicFromWad ("face_invis");
	sb_face_invuln = Draw_PicFromWad ("face_invul2");
	sb_face_invis_invuln = Draw_PicFromWad ("face_inv2");
	sb_face_quad = Draw_PicFromWad ("face_quad");

	
	sb_sbar = Draw_PicFromWad ("sbar");
	sb_ibar = Draw_PicFromWad ("ibar");
//	sb_ibar_ammo_icons = Draw_PicFromWad ("ibar2");
	sb_scorebar = Draw_PicFromWad ("scorebar");

//MED 01/04/97 added new hipnotic weapons
	if (hipnotic)
		Sbar_Hipnotic_Init ();

	if (rogue)
		Sbar_Rogue_Init ();

	if (!host_initialized) 
	{
		Cvar_RegisterVariable (&scr_centersbar);
		Cvar_RegisterVariable (&scr_printstats);
		Cvar_RegisterVariable (&scr_printstats_style);
		Cvar_RegisterVariable (&scr_printstats_length);
		Cvar_RegisterVariable (&scr_scoreboard_fillalpha);
		Cvar_RegisterVariable (&scr_sbaralpha);

		Cmd_AddCommand ("+showscores", Sbar_ShowScores);
		Cmd_AddCommand ("-showscores", Sbar_DontShowScores);

		Cvar_RegisterVariable (&pq_teamscores); // JPG - status bar teamscores
		Cvar_RegisterVariable (&pq_timer); // JPG - status bar timer
		Cvar_RegisterVariable (&pq_scoreboard_pings); // JPG - ping times in the scoreboard
	}
}


//=============================================================================

// drawing routines are relative to the status bar location
/*
=============
Sbar_DrawPic
=============
*/
void Sbar_DrawPic (int x, int y, mpic_t *pic)
{	
	Draw_Pic (x + sbar_xofs, y + (vid.height-SBAR_HEIGHT), pic);
}

void Sbar_DrawPicAlpha (int x, int y, mpic_t *pic)
{	
	Draw_AlphaPic (x + sbar_xofs, y + (vid.height-SBAR_HEIGHT), pic, scr_sbaralpha.value);
}

// by joe, from QW
/*
=============
Sbar_DrawSubPic
=============
JACK: Draws a portion of the picture in the status bar.
*/
void Sbar_DrawSubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height)
{
	Draw_SubPic (x, y + (vid.height-SBAR_HEIGHT), pic, srcx, srcy, width, height);
}


/*
=============
Sbar_DrawTransPic
=============
*/
void Sbar_DrawTransPic (int x, int y, mpic_t *pic)
{
	Draw_TransPic (x + sbar_xofs, y + (vid.height-SBAR_HEIGHT), pic);
}

/*
================
Sbar_DrawCharacter

Draws one solid graphics character
================
*/
void Sbar_DrawCharacter (int x, int y, int num)
{
	Draw_Character (x + sbar_xofs, y + vid.height-SBAR_HEIGHT, num, false);
}

/*
================
Sbar_DrawString
================
*/
void Sbar_DrawString (int x, int y, char *str)
{
	Draw_String (x + sbar_xofs, y + (vid.height-SBAR_HEIGHT), str, 1);
}
/*
=============
Sbar_itoa
=============
*/
int Sbar_itoa (int num, char *buf)
{
	char	*str;
	int		pow10, dig;

	str = buf;

	if (num < 0)
	{
		*str++ = '-';
		num = -num;
	}

	for (pow10 = 10 ; num >= pow10 ; pow10 *= 10)
	;

	do
	{
		pow10 /= 10;
		dig = num/pow10;
		*str++ = '0'+dig;
		num -= dig*pow10;
	} while (pow10 != 1);

	*str = 0;

	return str-buf;
}


/*
=============
Sbar_DrawNum
=============
*/
void Sbar_DrawNum (int x, int y, int num, int digits, int color)
{
	char	str[12];
	char	*ptr;
	int		l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);
	if (l < digits)
		x += (digits-l)*24;

	while (*ptr)
	{
		frame = (*ptr == '-') ? STAT_MINUS : *ptr -'0';

		Sbar_DrawTransPic (x, y, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

//=============================================================================

int		fragsort[MAX_SCOREBOARD];

char	scoreboardtext[MAX_SCOREBOARD][20];
int		scoreboardtop[MAX_SCOREBOARD];
int		scoreboardbottom[MAX_SCOREBOARD];
int		scoreboardcount[MAX_SCOREBOARD];
int		scoreboardlines;

/*
===============
Sbar_SortFrags
===============
*/
void Sbar_SortFrags (void)
{
	int		i, j, k;

// sort by frags
	scoreboardlines = 0;
	for (i=0 ; i<cl.maxclients ; i++)
	{
		if (cl.scores[i].name[0])
		{
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i=0 ; i<scoreboardlines ; i++)
		for (j=0 ; j<scoreboardlines-1-i ; j++)
			if (cl.scores[fragsort[j]].frags < cl.scores[fragsort[j+1]].frags)
			{
				k = fragsort[j];
				fragsort[j] = fragsort[j+1];
				fragsort[j+1] = k;
			}
}

/* JPG - added this for teamscores in status bar
==================
Sbar_SortTeamFrags
==================
*/
void Sbar_SortTeamFrags (void)
{
	int		i, j, k;

// sort by frags
	scoreboardlines = 0;
	for (i=0 ; i<14 ; i++)
	{
		if (cl.teamscores[i].colors)
		{
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i=0 ; i<scoreboardlines ; i++)
		for (j=0 ; j<scoreboardlines-1-i ; j++)
			if (cl.teamscores[fragsort[j]].frags < cl.teamscores[fragsort[j+1]].frags)
			{
				k = fragsort[j];
				fragsort[j] = fragsort[j+1];
				fragsort[j+1] = k;
			}
}

int Sbar_ColorForMap (int m)
{
	return m < 128 ? m + 8 : m + 8;
}

/*
===============
Sbar_UpdateScoreboard
===============
*/
void Sbar_UpdateScoreboard (void)
{
	int		i, k;
	int		top, bottom;
	scoreboard_t	*s;

	Sbar_SortFrags ();

// draw the text
	memset (scoreboardtext, 0, sizeof(scoreboardtext));

	for (i=0 ; i<scoreboardlines; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		sprintf (&scoreboardtext[i][1], "%3i %s", s->frags, s->name);

		top = s->colors & 0xf0;
		bottom = (s->colors & 15) <<4;
		scoreboardtop[i] = Sbar_ColorForMap (top);
		scoreboardbottom[i] = Sbar_ColorForMap (bottom);
	}
}

/*
===============
Sbar_SoloScoreboard
===============
*/
void Sbar_SoloScoreboard (void)
{
	char	str[80];
	int		minutes, seconds, tens, units, len;

	sprintf (str,"Monsters:%3i /%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	Sbar_DrawString (8, 4, str);

	sprintf (str,"Secrets :%3i /%3i", cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
	Sbar_DrawString (8, 12, str);

// time
	minutes = cl.ctime / 60;
	seconds = cl.ctime - 60*minutes;
	tens = seconds / 10;
	units = seconds - 10*tens;
	sprintf (str,"Time :%3i:%i%i", minutes, tens, units);
	Sbar_DrawString (184, 4, str);

// draw level name
	len = strlen (cl.levelname);

	Sbar_DrawString (232 - len*4, 12, cl.levelname);
}

/*
===============
Sbar_DrawScoreboard
===============
*/
void Sbar_DrawScoreboard (void)
{
	Sbar_SoloScoreboard ();
	
	if (cl.gametype == GAME_DEATHMATCH || cl.maxclients > 1)//coop support
		Sbar_DeathmatchOverlay ();
}

//=============================================================================
void Sbar_DrawInventory (void)
{
	int			i, j, flashon, ystart;
	char		num[6];
	float		time;
	qboolean	headsup;	// joe
	mpic_t		*invbar, *icon;

	// by joe
	headsup = !(cl_sbar.value || scr_viewsize.value < 100);

	if (rogue)
	{
		if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN)
			invbar = rsb_invbar[0];
		else
			invbar = rsb_invbar[1];		// secondary ammo hilited
	}
	else
	{
		invbar = sb_ibar;
	}

	if (!headsup)
	{
		Sbar_DrawPicAlpha(0, -24, invbar);
	}
	else
	{
		if (cl_sbar_style.value == 0)
		{
			if (scr_sbaralpha.value > 0)
			{					
				Sbar_DrawPicAlpha(0, -24, invbar);
			}
		}
	}

// weapons
	ystart = (hipnotic) ? -100 : -68;
	for (i=0 ; i<7 ; i++)
	{
		if (cl.items & (IT_SHOTGUN << i))
		{
			time = cl.item_gettime[i];
			flashon = (int)((cl.time - time)*10);
			if (flashon < 0)
				flashon = 0;
			if (flashon >= 10)
				flashon = (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i)) ? 1 : 0;
			else
				flashon = (flashon % 5) + 2;

			icon = sb_weapons[flashon][i];
			if (headsup)
			{
				if (cl_sbar_style.value == 0)
				{
					Sbar_DrawPic (i*24, -16, icon);
				}
				else
				{
					if (i || vid.height>200)
						Sbar_DrawSubPic ((vid.width-30), ystart - (7-i)*16, icon, 0, 0, 24, 16);
				}
			}
			else
			{
				Sbar_DrawPic (i*24, -16, icon);
			}

			if (flashon > 1)
				sb_updates = 0;		// force update to remove flash
		}
	}

// MED 01/04/97
// hipnotic weapons
	if (hipnotic)
	{
		int	grenadeflashing = 0, left, top;

		for (i=0 ; i<4 ; i++)
		{
			if (cl.items & (1 << hipweapons[i]))
			{
				time = cl.item_gettime[hipweapons[i]];
				flashon = (int)((cl.time - time) * 10);
				if (flashon < 0)
					flashon = 0;
				if (flashon >= 10)
					flashon = (cl.stats[STAT_ACTIVEWEAPON] == (1 << hipweapons[i])) ? 1 : 0;
				else
					flashon = (flashon % 5) + 2;

			// check grenade launcher
				if (i == 2)
				{
					if (!(cl.items & HIT_PROXIMITY_GUN) || !flashon)
						continue;

					grenadeflashing = 1;
					icon = hsb_weapons[flashon][2];
					left = 96;
					top = ystart - 48;
				}
				else if (i == 3)
				{
					if (cl.items & (IT_SHOTGUN << 4))		// standard grenade launcher
					{
						if (grenadeflashing)
							continue;		// icon already drawn

						icon = hsb_weapons[flashon][3];
					}
					else
					{
						icon = hsb_weapons[flashon][4];
					}

					left = 96;
					top = ystart - 48;
				}
				else		// laser cannon or hammer
				{
					icon = hsb_weapons[flashon][i];
					left = 176 + (i*24);
					top = ystart + (i*16);
				}

				if (headsup)
				{
					if (cl_sbar_style.value == 0)
					{
						Sbar_DrawPic (left, -16, icon);
					}
					else
					{
						Sbar_DrawSubPic ((vid.width-30), top, icon, 0, 0, 24, 16);
					}
				}
				else
				{
					Sbar_DrawPic (left, -16, icon);
				}

				if (flashon > 1)
					sb_updates = 0;      // force update to remove flash
			}
		}
	}

// rogue weapons
	if (rogue)
	{	// check for alternate weapons
		// JDH: draw icon for non-selected weapons if they are newly acquired (flashing),
		//		or if player doesn't have the equivalent id weapon

		for (i=0 ; i<5 ; i++)
		{
			if (cl.items & (RIT_LAVA_NAILGUN << i))
			{
				time = cl.item_gettime[12+i];		// RIT_LAVA_NAILGUN = 4096 = 1<<12
				flashon = (int)((cl.time - time)*10);
				if (flashon < 0)
					flashon = 0;
				if (flashon >= 10)
					flashon = (cl.stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i)) ? 1 : 0;
				else
					flashon = (flashon % 5) + 2;

				if (flashon == 1)	// active weapon
				{
					icon = rsb_weapons[i];
				}
				else
				{
				// if player has equivalent id weapon, icon has already been drawn
					if ((flashon == 0) && (cl.items & (IT_SHOTGUN << (i+2))))
						continue;

					icon = sb_weapons[flashon][i+2];		// rogue has no custom icons
				}

				if (headsup)		// JDH: this was missing from Joe's code
				{
					if (cl_sbar_style.value == 0)
					{
						Sbar_DrawPic ((i+2)*24, -16, icon);
					}
					else
					{
						if (vid.height>200)
							Sbar_DrawSubPic ((vid.width-30), ystart - (5-i)*16, icon, 0, 0, 24, 16);
					}
				}
				else
				{
					Sbar_DrawPic ((i+2)*24, -16, icon);
				}

				if (flashon > 1)
					sb_updates = 0;		// force update to remove flash
			}
		}
	}

// ammo counts
	for (i=0 ; i<4 ; i++)
	{
		sprintf (num, "%3i", cl.stats[STAT_SHELLS+i]);

		if (headsup)
		{
			if (cl_sbar_style.value == 0)
			{
				for (j = 0; j < 3; j++)
				{
					if (num[j] != ' ')
						Sbar_DrawCharacter ((6*i+1+j)*8 - 2, -24, 18 + num[j] - '0');
				}
			}
			else
			{
				Sbar_DrawSubPic ((vid.width-48), -24 - (4-i)*11, invbar, 3+(i*48), 0, 42, 11);
				for (j = 0; j < 3; j++)
				{
					if (num[j] != ' ')
						Draw_Character ((vid.width - 41 + j*8), vid.height-SBAR_HEIGHT-24 - (4-i)*11, 18 + num[j] - '0',0);
				}
			}
		}
		else
		{
			for (j = 0; j < 3; j++)
			{
				if (num[j] != ' ')
					Sbar_DrawCharacter ((6*i+1+j)*8 - 2, -24, 18 + num[j] - '0');
			}
		}
	}

	flashon = 0;

// items
	for (i=0 ; i<6 ; i++)
	{
		if (cl.items & (1<<(17+i)))
		{
			time = cl.item_gettime[17+i];
			if (time && time > cl.time - 2 && flashon)
			{	// flash frame
				sb_updates = 0;
			}
			else
			{	//MED 01/04/97 changed keys
				if (!hipnotic || (i>1))
					Sbar_DrawPic (192 + i*16, -16, sb_items[i]);
			}
			if (time && time > cl.time - 2)
			sb_updates = 0;
		}
	}

//MED 01/04/97 added hipnotic items
// hipnotic items
	if (hipnotic)
	{
		for (i=0 ; i<2 ; i++)
		{
			if (cl.items & (1<<(24+i)))
			{
				time = cl.item_gettime[24+i];
				if (time && time > cl.time - 2 && flashon)	// flash frame
					sb_updates = 0;
				else
					Sbar_DrawPic (288 + i*16, -16, hsb_items[i]);
				if (time && time > cl.time - 2)
					sb_updates = 0;
			}
		}
	}

// rogue items
	if (rogue)
	{
	// new rogue items
		for (i=0 ; i<2 ; i++)
		{
			if (cl.items & (1<<(29+i)))
			{
				time = cl.item_gettime[29+i];
				if (time && time > cl.time - 2 && flashon)	// flash frame
					sb_updates = 0;
				else
					Sbar_DrawPic (288 + i*16, -16, rsb_items[i]);
				if (time && time > cl.time - 2)
					sb_updates = 0;
			}
		}
	}
	else
	{
	// sigils
		for (i=0 ; i<4 ; i++)
		{
			if (cl.items & (1<<(28+i)))
			{
				time = cl.item_gettime[28+i];
				if (time && time > cl.time - 2 && flashon)	// flash frame
					sb_updates = 0;
				else
					Sbar_DrawPic (320-32 + i*8, -16, sb_sigil[i]);
				if (time && time > cl.time - 2)
					sb_updates = 0;
			}
		}
	}
}

//=============================================================================
/*
===============
Sbar_DrawFrags
===============
*/
void Sbar_DrawFrags (void)
{
	int				i, k, l;
	int				top, bottom;
	int				x, y, f;
	int				xofs;
	char			num[12];
	scoreboard_t	*s;
	int				teamscores, colors, ent, minutes, seconds, mask; // JPG - added these
	int				match_time; // JPG - added this

	// JPG - check to see if we should sort teamscores instead
	teamscores = pq_teamscores.value && cl.teamgame;

	if (teamscores)
		Sbar_SortTeamFrags();
	else
		Sbar_SortFrags ();

// draw the text
	l = scoreboardlines <= 4 ? scoreboardlines : 4;

	x = 23;
	if (cl.gametype == GAME_DEATHMATCH)
	{
		if (scr_centersbar.value)
			xofs = (vid.width - 320)>>1;
		else
			xofs = 0;
	}
	else
		xofs = (vid.width - 320)>>1;
	y = vid.height - SBAR_HEIGHT - 23;

	// JPG - check to see if we need to draw the timer
	if (pq_timer.value && (cl.minutes != 255))
	{
		if (l > 2)
			l = 2;
		mask = 0;
		if (cl.minutes == 254)
		{
			strcpy(num, "    SD");
			mask = 128;
		}
		else if (cl.minutes || cl.seconds)
		{
			if (cl.seconds >= 128)
				sprintf (num, " -0:%02d", cl.seconds - 128);
			else
			{
				if (cl.match_pause_time)
					match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.match_pause_time - cl.last_match_time));
				else
					match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.ctime - cl.last_match_time));
				minutes = match_time / 60;
				seconds = match_time - 60 * minutes;
				sprintf (num, "%3d:%02d", minutes, seconds);
				if (!minutes)
					mask = 128;
			}
		}
		else
		{
			minutes = cl.ctime / 60;
			seconds = cl.ctime - 60 * minutes;
			minutes = minutes & 511;
			sprintf (num, "%3d:%02d", minutes, seconds);
		}

		for (i = 0 ; i < 6 ; i++)
			Sbar_DrawCharacter ((x+9+i)*8, -24, num[i] + mask);
	}

	for (i=0 ; i<l ; i++)
	{
		k = fragsort[i];

		// JPG - check for teamscores
		if (teamscores)
		{
			colors = cl.teamscores[k].colors;
			f = cl.teamscores[k].frags;
		}
		else
		{
			s = &cl.scores[k];
			if (!s->name[0])
				continue;
			colors = s->colors;
			f = s->frags;
		}

		// draw background
		if (teamscores)
		{
			top = (colors & 15)<<4;
			bottom = (colors & 15)<<4;
		}
		else
		{
			top = colors & 0xf0;
			bottom = (colors & 15)<<4;
		}
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill (xofs + x * 8 + 8, y  , 30, 4, top);
		Draw_Fill (xofs + x * 8 + 8, y+4, 30, 3, bottom);

		// draw number
		sprintf (num, "%3i",f);		
		Sbar_DrawCharacter ((x+1)*8 , -24, num[0]);
		Sbar_DrawCharacter ((x+2)*8 , -24, num[1]);
		Sbar_DrawCharacter ((x+3)*8 , -24, num[2]);

		// JPG - check for self's team
		ent = cl.viewentity - 1;

		if ((teamscores && (colors & 15)<<4 == (cl.scores[ent].colors & 15)<<4) || (!teamscores && (k == ent)))
		{
			Sbar_DrawCharacter ((x)*8 + 2, -24, 16);
			Sbar_DrawCharacter ((x+4)*8, -24, 17);
		}

		x+=4;
	}
}
//=============================================================================


/*
===============
Sbar_DrawFace
===============
*/
void Sbar_DrawFace (void)
{
	int		f, anim;
	
// PGM 01/19/97 - team color drawing -start
// PGM 03/02/97 - fixed so color swatch only appears in CTF modes
	if (rogue && (cl.maxclients != 1) && (teamplay.value > 3) && (teamplay.value < 7))
	{
		int		top, bottom, xofs;
		char		num[12];
		scoreboard_t	*s;
		
		s = &cl.scores[cl.viewentity - 1];
		// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15)<<4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		if (cl.gametype == GAME_DEATHMATCH)
			xofs = 113;
		else
			xofs = ((vid.width - 320)>>1) + 113;

		Sbar_DrawPic (112, 0, rsb_teambord);
		Draw_Fill (xofs, vid.height-SBAR_HEIGHT+3, 22, 9, top);
		Draw_Fill (xofs, vid.height-SBAR_HEIGHT+12, 22, 9, bottom);

		// draw number
		f = s->frags;
		sprintf (num, "%3i",f);

		if (top == 8)
		{
			if (num[0] != ' ')
				Sbar_DrawCharacter(109, 3, 18 + num[0] - '0');
			if (num[1] != ' ')
				Sbar_DrawCharacter(116, 3, 18 + num[1] - '0');
			if (num[2] != ' ')
				Sbar_DrawCharacter(123, 3, 18 + num[2] - '0');
		}
		else
		{
			Sbar_DrawCharacter ( 109, 3, num[0]);
			Sbar_DrawCharacter ( 116, 3, num[1]);
			Sbar_DrawCharacter ( 123, 3, num[2]);
		}
		
		return;
	}

	// PGM 01/19/97 - team color drawing -end

	if ((cl.items & (IT_INVISIBILITY | IT_INVULNERABILITY)) == (IT_INVISIBILITY | IT_INVULNERABILITY))
	{
		Sbar_DrawPic (112, 0, sb_face_invis_invuln);
		return;
	}
	if (cl.items & IT_QUAD)
	{			
		Sbar_DrawPic (112, 0, sb_face_quad);
		return;
	}
	if (cl.items & IT_INVISIBILITY)
	{
		Sbar_DrawPic (112, 0, sb_face_invis);
		return;
	}
	if (cl.items & IT_INVULNERABILITY)
	{
		Sbar_DrawPic (112, 0, sb_face_invuln);
		return;
	}

	f = cl.stats[STAT_HEALTH] / 20;
	f = bound (0, f, 4);

	if (cl.ctime <= cl.faceanimtime)
	{
		anim = 1;
		sb_updates = 0;		// make sure the anim gets drawn over
	}
	else
		anim = 0;

	Sbar_DrawPic (112, 0, sb_faces[f][anim]);
}

// added by joe
float	printstats_limit;

/*
===============
SCR_PrintStats
===============
Todo: allow end user to set the position of the output.
*/

void SCR_PrintStats (void)
{
	int		mins, secs, tens;
	int		match_time; // R00k
	//---
	int		i, k, l, f, top, bottom, colors, ent;
	extern cvar_t	con_textsize;

	mins = cl.ctime / 60;
	secs = cl.ctime - 60 * mins;
	tens = (int)(cl.ctime * 10) % 10;
	match_time = 0;

	if (cl.gametype == GAME_DEATHMATCH)
	{
		//if (pq_timer.value == 2)
		{
			if ((cl.minutes || cl.seconds) && (cl.minutes != 255))
			{					
				if (cl.match_pause_time)
						match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.match_pause_time - cl.last_match_time));
					else
						match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.ctime - cl.last_match_time));

				mins = match_time / 60;
				secs = match_time - 60 * mins;

				if (cl.match_pause_time)
					tens = (int)(cl.match_pause_time * 10) % 10;
				else
					tens = (int)(cl.ctime * 10) % 10;
				
				if (mins < 0) mins = 0;
				if (secs < 0) secs = 0;
				if (tens < 0) tens = 0;
			}
		}
	}

	if (scr_printstats_style.value == 0)
	{
		if (mins > 0)
		{
			Sbar_IntermissionNumber (vid.width - 140, 0, mins, 2, 0);
			Draw_TransPic (vid.width - 92, 0, sb_colon);

			Draw_TransPic (vid.width - 80, 0, sb_nums[0][secs/10]);
			Draw_TransPic (vid.width - 58, 0, sb_nums[0][secs%10]);

			Draw_TransPic (vid.width - 36, 0, sb_colon);
			Draw_TransPic (vid.width - 24, 0, sb_nums[0][tens]);
		}
		else
		{
			Sbar_IntermissionNumber (vid.width - 140, 0, mins, 2, 1);
			Draw_TransPic (vid.width - 92, 0, sb_colon);

			Draw_TransPic (vid.width - 80, 0, sb_nums[1][secs/10]);
			Draw_TransPic (vid.width - 58, 0, sb_nums[1][secs%10]);

			Draw_TransPic (vid.width - 36, 0, sb_colon);
			Draw_TransPic (vid.width - 24, 0, sb_nums[1][tens]);
		}

		//R00k added
		if ((cl.gametype != GAME_DEATHMATCH))
		{
			Sbar_IntermissionNumber (vid.width - 48, 24, cl.stats[STAT_SECRETS], 2, 0);
			Sbar_IntermissionNumber (vid.width - 72, 48, cl.stats[STAT_MONSTERS], 3, 0);
		}
		else
		{
			if ((cl.teamgame) && (pq_teamscores.value))
			{
				Sbar_SortTeamFrags();
				l = scoreboardlines <= 4 ? scoreboardlines : 4;
				for (i = 0 ; i < l ; i++)
				{
					k = fragsort[i];
					colors = cl.teamscores[k].colors;
					f = cl.teamscores[k].frags;

					//TODO:cvar this as sometimes its buggy
					// draw background
					top = (colors & 0xf0);
					top = Sbar_ColorForMap (top);

					bottom = (colors & 15)<<4;
					bottom = Sbar_ColorForMap (bottom);

					//Draw teamcolor markers
					Draw_AlphaFill (vid.width - 96, 24 + (i*24), 128, 12, bottom, 0.5f);//was top
					Draw_AlphaFill (vid.width - 96, 24 + (i*24) + 12, 128, 12, bottom, 0.5f);

					// draw scores
					Sbar_IntermissionNumber (vid.width - 72, 24 + (i*24), f, 3, 0);
				}				
			}
		}
	}
	else
	{
		Draw_String (vid.width - 64, 0, va("%2i:%02i:%i", mins, secs, tens), 0);

		if ((cl.gametype != GAME_DEATHMATCH))
		{
			Draw_String (vid.width - 16, 8, va("%2i", cl.stats[STAT_SECRETS]), 0);
			Draw_String (vid.width - 24, 16, va("%3i", cl.stats[STAT_MONSTERS]), 0);
		}
		else
		{
			if (cl.teamgame)
			{
				ent = cl.viewentity - 1;
				Sbar_SortTeamFrags();
				l = scoreboardlines <= 4 ? scoreboardlines : 4;
				for (i=0 ; i<l ; i++)
				{
					k = fragsort[i];
					colors = cl.teamscores[k].colors;
					f = cl.teamscores[k].frags;
					// draw background
					top = (colors & 0xf0);
					bottom = (colors & 15)<<4;

					top = Sbar_ColorForMap (top);
					bottom = Sbar_ColorForMap (bottom);

					//Draw teamcolor markers
					Draw_AlphaFill (vid.width - 40, 8 + (i * 8), 40, 4, top, 0.5);
					Draw_AlphaFill (vid.width - 40, 8 + (i * 8) + 4, 40, 4, bottom, 0.5);					

					// draw number				
					Draw_String (vid.width - 32, 8 + (i * 8), va("%3i", f) , 0);
					
					if ((colors & 15)<<4 == (cl.scores[ent].colors & 15)<<4)
					{
						Draw_Character (vid.width - 44,  8 + (i * 8), 16, 0);
						Draw_Character (vid.width - 8, 8 + (i * 8), 17, 0);
					}
				}
			}
		}
	}
}

/*
===============
Sbar_Draw
===============
*/
void Sbar_Draw (void)
{
	qboolean headsup;	// joe
	extern cvar_t cl_sbar_drawface;
	extern void SCR_DrawHealthBar (void);
	extern void SCR_DrawArmorBar (void);
	extern void SCR_DrawAmmoBar (void);

	headsup = !(cl_sbar.value || scr_viewsize.value < 100);

//	if ((sb_updates >= vid.numpages) && !headsup)
//		return;

	if (scr_con_current == vid.height)
		return;		// console is full screen

	scr_copyeverything = 1;
	sb_updates++;

	sbar_xofs = (scr_centersbar.value) ? (vid.width - 320)>>1 : 0;

	if (sb_showscores)// || (cl.stats[STAT_HEALTH] <= 0 && !cls.demoplayback && !freemoving.value)) 
	{
		if (cl_sbar.value || scr_viewsize.value < 100)
		{
			Sbar_DrawPic (0, 0, sb_scorebar);
		}
		else
		{
			if ((cl_sbar_style.value == 0) && (scr_sbaralpha.value))//R00k
				Sbar_DrawPicAlpha (0, 0, sb_scorebar);
		}
		Sbar_DrawScoreboard ();
		sb_updates = 0;
	}
	else if (sb_lines)
	{
		if (sb_lines > 24)
		{
			Sbar_DrawInventory ();
			// joe
			//if ((!headsup || vid.width<512) && cl.maxclients != 1)
			if (cl.maxclients > 1)
				Sbar_DrawFrags ();
		}

		// by joe
		if (cl_sbar.value || scr_viewsize.value<100)
			Sbar_DrawPic (0, 0, sb_sbar);
		else
		{
			if ((cl_sbar_style.value == 0) && (scr_sbaralpha.value))
				Sbar_DrawPicAlpha (0, 0, sb_sbar);
		}

	// keys (hipnotic only)
		//MED 01/04/97 moved keys here so they would not be overwritten
		if (hipnotic)
		{
			if (cl.items & IT_KEY1)
				Sbar_DrawPic (209, 3, sb_items[0]);
			if (cl.items & IT_KEY2)
				Sbar_DrawPic (209, 12, sb_items[1]);
		}

	// armor
/*
		if (cl.items & IT_INVULNERABILITY)
		{
//R00k this doesnt make any sense. Having a pent icon AND 666 for armorvalue, is just wrong. Players still lose their armorvalue with pent when shot!
//			if (!cl.teamgame)//R00k singleplayer 
//				Sbar_DrawNum (24, 0, 666, 3, 1);
//			else			
			Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
			Sbar_DrawPic (0, 0, draw_disc);
		}
		else
		{
*/			Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);

			if (rogue)
			{
				if (cl.items & RIT_ARMOR3)
					Sbar_DrawPic (0, 0, sb_armor[2]);
				else if (cl.items & RIT_ARMOR2)
					Sbar_DrawPic (0, 0, sb_armor[1]);
				else if (cl.items & RIT_ARMOR1)
					Sbar_DrawPic (0, 0, sb_armor[0]);
			}
			else
			{
				if (cl.items & IT_ARMOR3)
					Sbar_DrawPic (0, 0, sb_armor[2]);
				else if (cl.items & IT_ARMOR2)
					Sbar_DrawPic (0, 0, sb_armor[1]);
				else if (cl.items & IT_ARMOR1)
					Sbar_DrawPic (0, 0, sb_armor[0]);
			}
//		}

	// face
	if (cl_sbar_drawface.value)
		Sbar_DrawFace ();

	// health
		Sbar_DrawNum (136, 0, cl.stats[STAT_HEALTH], 3, 
			cl.stats[STAT_HEALTH] <= 25);

	// ammo icon
		if (rogue)
		{
			if (cl.items & RIT_SHELLS)
				Sbar_DrawPic (224, 0, sb_ammo[0]);
			else if (cl.items & RIT_NAILS)
				Sbar_DrawPic (224, 0, sb_ammo[1]);
			else if (cl.items & RIT_ROCKETS)
				Sbar_DrawPic (224, 0, sb_ammo[2]);
			else if (cl.items & RIT_CELLS)
				Sbar_DrawPic (224, 0, sb_ammo[3]);
			else if (cl.items & RIT_LAVA_NAILS)
				Sbar_DrawPic (224, 0, rsb_ammo[0]);
			else if (cl.items & RIT_PLASMA_AMMO)
				Sbar_DrawPic (224, 0, rsb_ammo[1]);
			else if (cl.items & RIT_MULTI_ROCKETS)
				Sbar_DrawPic (224, 0, rsb_ammo[2]);
		}
		else
		{
			if (cl.items & IT_SHELLS)
				Sbar_DrawPic (224, 0, sb_ammo[0]);
			else if (cl.items & IT_NAILS)
				Sbar_DrawPic (224, 0, sb_ammo[1]);
			else if (cl.items & IT_ROCKETS)
				Sbar_DrawPic (224, 0, sb_ammo[2]);
			else if (cl.items & IT_CELLS)
				Sbar_DrawPic (224, 0, sb_ammo[3]);
		}

		Sbar_DrawNum (248, 0, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
	}

	// time, secrets & kills drawing to the top right corner -- joe
	if (!cl.intermission && (scr_printstats.value == 1 || (scr_printstats.value == 2 && printstats_limit > cl.ctime)))
	{
		SCR_PrintStats ();
	}

//	if (custom_hud.value == 1
	SCR_DrawArmorBar();
	SCR_DrawHealthBar();
	SCR_DrawAmmoBar();

// added by joe
#ifdef GLQUAKE
	// clear unused areas in GL
	if (vid.width > 320 && !headsup)
	{
		// left
		if (scr_centersbar.value)
			Draw_TileClear (0, vid.height - sb_lines, sbar_xofs, sb_lines);
		// right
		Draw_TileClear (320 + sbar_xofs, vid.height - sb_lines, vid.width - (320 + sbar_xofs), sb_lines);
	}
#endif

	if (!scr_centersbar.value)//R00k
	{
		if (cl.gametype == GAME_DEATHMATCH)
			Sbar_MiniDeathmatchOverlay ();
	}
}

//=============================================================================

/*
==================
Sbar_IntermissionNumber

==================
*/
void Sbar_IntermissionNumber (int x, int y, int num, int digits, int color)
{
	char	str[12];
	char	*ptr;
	int		l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);
	if (l < digits)
		x += (digits-l)*24;

	while (*ptr)
	{
		frame = (*ptr == '-') ? STAT_MINUS : *ptr -'0';

		Draw_TransPic (x, y, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

/*
==================
Sbar_DeathmatchOverlay
==================
*/
void Sbar_DeathmatchOverlay (void)
{
	mpic_t			*pic;
	int				i, k, l;
	int				colors, top, bottom;
	int				x, y, f;
	int				xofs, yofs;
	char			num[12];
	scoreboard_t	*s;
	int				ping, j; // JPG 

	if (pq_scoreboard_pings.value && (cl.time - cl.last_ping_time > 5))
	{
		MSG_WriteByte (&cls.message, clc_stringcmd);
		SZ_Print (&cls.message, "ping\n");			
		cl.last_ping_time = cl.time;
	}

	// JPG 1.05 - check to see if we should update IP status
	if (iplog_size && (cl.time - cl.last_status_time > 5))
	{
		MSG_WriteByte (&cls.message, clc_stringcmd);
		SZ_Print (&cls.message, "status\n");
		cl.last_status_time = cl.time;
	}

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	xofs = (vid.conwidth - 320) >> 1;
	yofs = (vid.conheight - 200) >> 1;

	pic = Draw_CachePic ("gfx/ranking.lmp");
	Draw_Pic (xofs + 160 - pic->width/2, yofs - pic->height/2, pic);	// by joe | r00k added yofs

	// scores
	Sbar_SortFrags ();

	// draw the text
	l = scoreboardlines;

	x = xofs + 64;
	y = yofs + 40;

	ping = 0;

	Draw_AlphaFill	(x - 64,	y - 11, 328 , 10, 16, 1);	//inside
	Draw_Fill		(x - 64,	y - 12, 329 , 1	, 0);		//Border - Top
	Draw_Fill		(x - 64,	y - 12, 1	, 11, 0);		//Border - Left
	Draw_Fill		(x + 264,	y - 12, 1	, 11, 0);		//Border - Right
	Draw_Fill		(x - 64,	y - 1 , 329 , 1	, 0);		//Border - Bottom
	Draw_Fill		(x - 64,	y - 1,	329 , 1	, 0);		//Border - Top
	
	Draw_String		(x - 64,	y - 10, "  ping  frags   name            status",1);
	
	for (i=0 ; i<l ; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

		if (k == cl.viewentity - 1)
		{
			Draw_AlphaFill (x - 63, y, 328 , 10, 20, scr_scoreboard_fillalpha.value);//TODO: scr_scoreboard_fillcolor
		}
		else
		{
			Draw_AlphaFill (x - 63, y, 328 , 10, 18, scr_scoreboard_fillalpha.value);//TODO: scr_scoreboard_fillcolor
		}
		
		Draw_Fill (x - 64, y, 1, 10, 0);	//Border - Left
		Draw_Fill (x + 264, y, 1, 10, 0);	//Border - Right

		// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15)<<4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill ( x, y+1, 40, 3, top);
		Draw_Fill ( x, y+4, 40, 4, bottom);

		// JPG - draw ping
		if (s->ping && pq_scoreboard_pings.value)
		{
			ping = 1;
			sprintf(num, "%4d", s->ping);
			for (j = 0 ; j < 4 ; j++)
				Draw_Character(x-56+j*8, y, num[j],false);
		}

		// draw number
		f = s->frags;
		sprintf (num, "%3i",f);

		Draw_Character ( x+8 , y+1, num[0],false);
		Draw_Character ( x+16 , y+1, num[1],false);
		Draw_Character ( x+24 , y+1, num[2],false);

		if (k == cl.viewentity - 1)
		{			
			Draw_Character(x, y+1, 16,false);	
			Draw_Character(x+32, y+1, 17,false);	
		}
		/*
		#if 0
		{
			int				total;
			int				n, minutes, tens, units;

			// draw time
				total = cl.completed_time - s->entertime;
				minutes = (int)total/60;
				n = total - minutes*60;
				tens = n/10;
				units = n%10;

				sprintf (num, "%3i:%i%i", minutes, tens, units);

				Draw_String ( x+48 , y, num);
		}
		#endif
		*/
		// draw name
		Draw_String (x+64, y+1, s->name,1);
		//if (i < (l-1))
		y += 10;
	}
	
	Draw_Fill (x - 64, y, 329 , 1, 0);	//Border - Bottom

	//R00k: added teamscores to the scoreboard (tab)
	//one small problem though, with 4+ teams and low hud reolutions the 
	//scores get messed up.
	if ((cl.teamgame)&&(!scr_printstats.value))
	{
		y += 10;

		Sbar_SortTeamFrags();
		
		l = scoreboardlines <= 4 ? scoreboardlines : 4;

		x = (vid.conwidth - (l * 128)) / 2;

		for (i = 0 ; i < l ; i++)
		{			
			k = fragsort[i];

			colors = cl.teamscores[k].colors;
			f = cl.teamscores[k].frags;

//			// draw background
			top = (colors & 15) << 4;//CRMOD/PQ bug cant always rely on valid colors for shirt. :(  was (colors & 0xf0); doesnt matter its TEAMscores!
			bottom = (colors & 15) << 4;

			top = Sbar_ColorForMap (top);
			bottom = Sbar_ColorForMap (bottom);

			//Draw teamcolor markers
			Draw_AlphaFill (x, y  , 120, 12, top, 0.5f);
			Draw_AlphaFill (x, y+12, 120, 12, bottom, 0.5f);

			// draw number				
			Sbar_IntermissionNumber (x, y, f, 3, 0);
			x += 128;
		}
	}
}


//R00k: v1.9 added for sbar ping display
void Sbar_DrawPing ()
{	
	char	str[80];
	int		x, y, i;	
	scoreboard_t	*s;
	extern	cvar_t	show_ping;
	extern	cvar_t	show_ping_x;
	extern	cvar_t	show_ping_y;

	if ((!show_ping.value)||(scr_viewsize.value > 110))
		return;

	if (cl.time - cl.last_ping_time < (0.25))
	{
		return;
	}

	if (cl.time - cl.last_ping_time > 4)//update ping once every 5 seconds
	{
		MSG_WriteByte (&cls.message, clc_stringcmd);
		SZ_Print (&cls.message, "ping\n");			
		cl.last_ping_time = cl.time;
	}	

	for (i=0 ; i<16 ; i++)
	{
		s = &cl.scores[fragsort[i]];

		if (!s->name[0])
			continue;

		if (fragsort[i] == cl.viewentity - 1)
		{
			Q_snprintfz (str, sizeof(str), "%i", s->ping);

			x = ELEMENT_X_COORD(show_ping);
			y = ELEMENT_Y_COORD(show_ping);

			Draw_String (x, y, str, 1);			
			break;
		}
	}
}

/*
==================
Sbar_MiniDeathmatchOverlay

==================
*/
void Sbar_MiniDeathmatchOverlay (void)
{
	int				i, k, l;
	int				top, bottom;
	int				x, y, f;
	char			num[12];
	scoreboard_t	*s;
	int				numlines;

	if (vid.width < 640 || !sb_lines)
		return;

	scr_copyeverything = 1;
	scr_fullupdate = 0;

// scores
	Sbar_SortFrags ();

// draw the text
	l = scoreboardlines;
	y = vid.height - sb_lines;
	numlines = sb_lines/8;
	if (numlines < 3)
		return;

	//find us
	for (i = 0; i < scoreboardlines; i++)
		if (fragsort[i] == cl.viewentity - 1)
			break;

    if (i == scoreboardlines) // we're not there
            i = 0;
    else // figure out start
            i = i - numlines/2;

    if (i > scoreboardlines - numlines)
            i = scoreboardlines - numlines;
    if (i < 0)
            i = 0;

	x = 324;
	for (/* */; i < scoreboardlines && y < vid.height - 8 ; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

	// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15)<<4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill(x, y+1, 40, 3, top);
		Draw_Fill(x, y+4, 40, 4, bottom);

		if (k == cl.viewentity - 1) 
		{
			Draw_Character(x, y, 16,false);	
			Draw_Character(x+32, y, 17,false);
		}

	// draw number
		f = s->frags;
		sprintf (num, "%3i",f);
		
		Draw_Character ( x+8 , y, num[0],false);
		Draw_Character ( x+16 , y, num[1],false);
		Draw_Character ( x+24 , y, num[2],false);


	// draw name
		Draw_String (x+48, y, s->name,1);

		y += 8;
	}
}

/*
==================
Sbar_IntermissionOverlay

==================
*/
void Sbar_IntermissionOverlay (void)
{
	mpic_t	*pic;
	int	dig, num, xofs, yofs;

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	if (cl.gametype == GAME_DEATHMATCH)
	{
		Sbar_DeathmatchOverlay ();
		return;
	}

	xofs = (vid.width - 320) >> 1;
	yofs = (vid.height - 200) >> 1;

	pic = Draw_CachePic ("gfx/complete.lmp");
	Draw_Pic (xofs + 64, yofs + 16, pic);		// yofs was 24?

	pic = Draw_CachePic ("gfx/inter.lmp");
	Draw_TransPic (xofs, yofs + 64, pic);		//56

	// time
	dig = cl.completed_time/60;
	Sbar_IntermissionNumber (xofs + 160, yofs + 70, dig, 3, 0);
	num = cl.completed_time - dig*60;
	Draw_TransPic (xofs + 234, yofs + 70, sb_colon);
	Draw_TransPic (xofs + 246, yofs + 70, sb_nums[0][num/10]);
	Draw_TransPic (xofs + 270, yofs + 70, sb_nums[0][num%10]);

	// secrets
	Sbar_IntermissionNumber (xofs + 160, yofs + 112, cl.stats[STAT_SECRETS], 3, 0);
	Draw_TransPic (xofs + 232, yofs + 112, sb_slash);
	Sbar_IntermissionNumber (xofs + 240, yofs + 112, cl.stats[STAT_TOTALSECRETS], 3, 0);

	// monsters
	Sbar_IntermissionNumber (xofs + 160, yofs + 152, cl.stats[STAT_MONSTERS], 3, 0);
	Draw_TransPic (xofs + 232, yofs + 152, sb_slash);
	Sbar_IntermissionNumber (xofs + 240, yofs + 152, cl.stats[STAT_TOTALMONSTERS], 3, 0);
}


/*
==================
Sbar_FinaleOverlay

==================
*/
void Sbar_FinaleOverlay (void)
{
	mpic_t	*pic;

	scr_copyeverything = 1;

	pic = Draw_CachePic ("gfx/finale.lmp");
	Draw_TransPic ((vid.width-pic->width)/2, 16, pic);
}


