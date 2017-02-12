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


typedef struct
{
	int		s;
	dfunction_t	*f;
} prstack_t;

#define	MAX_STACK_DEPTH		256	//32
prstack_t	pr_stack[MAX_STACK_DEPTH + 1];
int		pr_depth;

#define	LOCALSTACK_SIZE		2048
int		localstack[LOCALSTACK_SIZE];
int		localstack_used;


qboolean	pr_trace;
dfunction_t	*pr_xfunction;
int		pr_xstatement;


int		pr_argc;

char *pr_opnames[] =
{
	"DONE",

	"MUL_F",
	"MUL_V",
	"MUL_FV",
	"MUL_VF",

	"DIV",

	"ADD_F",
	"ADD_V",

	"SUB_F",
	"SUB_V",

	"EQ_F",
	"EQ_V",
	"EQ_S",
	"EQ_E",
	"EQ_FNC",

	"NE_F",
	"NE_V",
	"NE_S",
	"NE_E",
	"NE_FNC",

	"LE",
	"GE",
	"LT",
	"GT",

	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",

	"ADDRESS",

	"STORE_F",
	"STORE_V",
	"STORE_S",
	"STORE_ENT",
	"STORE_FLD",
	"STORE_FNC",

	"STOREP_F",
	"STOREP_V",
	"STOREP_S",
	"STOREP_ENT",
	"STOREP_FLD",
	"STOREP_FNC",

	"RETURN",

	"NOT_F",
	"NOT_V",
	"NOT_S",
	"NOT_ENT",
	"NOT_FNC",

	"IF",
	"IFNOT",

	"CALL0",
	"CALL1",
	"CALL2",
	"CALL3",
	"CALL4",
	"CALL5",
	"CALL6",
	"CALL7",
	"CALL8",

	"STATE",

	"GOTO",

	"AND",
	"OR",

	"BITAND",
	"BITOR"
};

char *PR_GlobalString (int ofs);
char *PR_GlobalStringNoContents (int ofs);


//=============================================================================

/*
=================
PR_PrintStatement
=================
*/
void PR_PrintStatement (dstatement_t *s)
{
	int	i;

	if ((unsigned)s->op < sizeof(pr_opnames)/sizeof(pr_opnames[0]))
	{
		Con_Printf ("%s ",  pr_opnames[s->op]);
		i = strlen(pr_opnames[s->op]);
		for ( ; i<10 ; i++)
			Con_Printf (" ");
	}

	if (s->op == OP_IF || s->op == OP_IFNOT)
	{
		Con_Printf ("%sbranch %i",PR_GlobalString(s->a),s->b);
	}
	else if (s->op == OP_GOTO)
	{
		Con_Printf ("branch %i",s->a);
	}
	else if ( (unsigned)(s->op - OP_STORE_F) < 6)
	{
		Con_Printf ("%s",PR_GlobalString(s->a));
		Con_Printf ("%s", PR_GlobalStringNoContents(s->b));
	}
	else
	{
		if (s->a)
			Con_Printf ("%s",PR_GlobalString(s->a));
		if (s->b)
			Con_Printf ("%s",PR_GlobalString(s->b));
		if (s->c)
			Con_Printf ("%s", PR_GlobalStringNoContents(s->c));
	}
	Con_Printf ("\n");
}

/*
============
PR_StackTrace
============
*/
void PR_StackTrace (void)
{
	dfunction_t	*f;
	int		i;

	if (pr_depth == 0)
	{
		Con_Printf ("<NO STACK>\n");
		return;
	}

	if (pr_depth > MAX_STACK_DEPTH)
		pr_stack[pr_depth].f = pr_xfunction;

	for (i=pr_depth ; i>=0 ; i--)
	{
		f = pr_stack[i].f;

		if (!f)
			Con_Printf ("<NO FUNCTION>\n");
		else
			Con_Printf ("%12s : %s\n", pr_strings + f->s_file, pr_strings + f->s_name);
	}
}


/*
============
PR_Profile_f

============
*/
void PR_Profile_f(void)//Ripped from Hexen2
{
	int i, j;
	int max;
	dfunction_t *f, *bestFunc;
	int total;
	int funcCount;
	qboolean byHC;
	char saveName[128];
	FILE *saveFile;
	int currentFile;
	int bestFile;
	int tally;
	char *s;

	byHC = false;
	funcCount = 10;
	*saveName = 0;

	if (!sv.active) 
	{
		Con_Printf ("not running a local server: aborted.\n");
		return;
	}

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("Usage: profile {q} {s} {#}\n");
		Con_Printf ("q: sort by qc files\n");
		Con_Printf ("s: save to profile.txt\n");
		Con_Printf ("#: specify function count\n");
		return;
	}

	for(i = 1; i < Cmd_Argc(); i++)
	{
		s = Cmd_Argv(i);
		if(tolower(*s) == 'q')
		{ // Sort by QC source file
			byHC = true;
		}
		else if(tolower(*s) == 's')
		{ // Save to file
			sprintf(saveName, "%s/profile.txt", com_gamedir);
		}
		else if(isdigit(*s))
		{ // Specify function count
			funcCount = atoi(Cmd_Argv(i));
			if(funcCount < 1)
			{
				funcCount = 1;
			}
		}
	}

	total = 0;
	for(i = 0; i < progs->numfunctions; i++)
	{
		total += pr_functions[i].profile;
	}

	if(*saveName)
	{ // Create the output file
		if((saveFile = fopen(saveName, "w")) == NULL)
		{
			Con_Printf("Could not open %s\n", saveName);
			return;
		}
	}

#ifdef TIMESNAP_ACTIVE
	if(*saveName)
	{
		fprintf(saveFile, "(Timesnap Profile)\n");
	}
	else
	{
		Con_Printf("(Timesnap Profile)\n");
	}
#endif

	if(byHC == false)
	{
		j = 0;
		do
		{
			max = 0;
			bestFunc = NULL;
			for(i = 0; i < progs->numfunctions; i++)
			{
				f = &pr_functions[i];
				if(f->profile > max)
				{
					max = f->profile;
					bestFunc = f;
				}
			}
			if(bestFunc)
			{
				if(j < funcCount)
				{
					if(*saveName)
					{
						fprintf(saveFile, "%05.2f %s\n",
							((float)bestFunc->profile/(float)total)*100.0,
							pr_strings+bestFunc->s_name);
					}
					else
					{
						Con_Printf("%05.2f %s\n",
							((float)bestFunc->profile/(float)total)*100.0,
							pr_strings+bestFunc->s_name);
					}
				}
				j++;
				bestFunc->profile = 0;
			}
		} while(bestFunc);
		if(*saveName)
		{
			fclose(saveFile);
		}
		return;
	}

	currentFile = -1;
	do
	{
		tally = 0;
		bestFile = Q_MAXINT;
		for(i = 0; i < progs->numfunctions; i++)
		{
			if(pr_functions[i].s_file > currentFile
				&& pr_functions[i].s_file < bestFile)
			{
				bestFile = pr_functions[i].s_file;
				tally = pr_functions[i].profile;
				continue;
			}
			if(pr_functions[i].s_file == bestFile)
			{
				tally += pr_functions[i].profile;
			}
		}
		currentFile = bestFile;
		if(tally && currentFile != Q_MAXINT)
		{
			if(*saveName)
			{
				fprintf(saveFile, "\"%s\"\n", pr_strings+currentFile);
			}
			else
			{
				Con_Printf("\"%s\"\n", pr_strings+currentFile);
			}
			j = 0;
			do
			{
				max = 0;
				bestFunc = NULL;
				for(i = 0; i < progs->numfunctions; i++)
				{
					f = &pr_functions[i];
					if(f->s_file == currentFile && f->profile > max)
					{
						max = f->profile;
						bestFunc = f;
					}
				}
				if(bestFunc)
				{
					if(j < funcCount)
					{
						if(*saveName)
						{
							fprintf(saveFile, "   %05.2f %s\n",
								((float)bestFunc->profile
								/(float)total)*100.0,
								pr_strings+bestFunc->s_name);
						}
						else
						{
							Con_Printf("   %05.2f %s\n",
								((float)bestFunc->profile
								/(float)total)*100.0,
								pr_strings+bestFunc->s_name);
						}
					}
					j++;
					bestFunc->profile = 0;
				}
			} while(bestFunc);
		}
	} while(currentFile != Q_MAXINT);
	if(*saveName)
	{
		fclose(saveFile);
	}
}
/*
============
PR_RunError

Aborts the currently executing function
============
*/
void PR_RunError (char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	PR_PrintStatement (pr_statements + pr_xstatement);
	PR_StackTrace ();
	Con_Printf ("%s\n", string);
	
	pr_depth = 0;		// dump the stack so host_error can shutdown functions

	Host_Error ("Program error");
}

/*
============================================================================
PR_ExecuteProgram

The interpretation main loop
============================================================================
*/

/*
====================
PR_EnterFunction

Returns the new program statement counter
====================
*/
int PR_EnterFunction (dfunction_t *f)
{
	int	i, j, c, o;

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;	
	pr_depth++;
	if (pr_depth >= MAX_STACK_DEPTH)
		PR_RunError ("stack overflow");

// save off any locals that the new function steps on
	c = f->locals;
	if (localstack_used + c > LOCALSTACK_SIZE)
		PR_RunError ("PR_ExecuteProgram: locals stack overflow\n");

	for (i=0 ; i<c ; i++)
		localstack[localstack_used+i] = ((int *)pr_globals)[f->parm_start + i];
	localstack_used += c;

// copy parameters
	o = f->parm_start;
	for (i=0 ; i<f->numparms ; i++)
	{
		for (j=0 ; j<f->parm_size[i] ; j++)
		{
			((int *)pr_globals)[o] = ((int *)pr_globals)[OFS_PARM0+i*3+j];
			o++;
		}
	}

	pr_xfunction = f;
	return f->first_statement - 1;	// offset the s++
}

/*
====================
PR_LeaveFunction
====================
*/
int PR_LeaveFunction (void)
{
	int	i, c;

	if (pr_depth <= 0)
		PR_RunError ("prog stack underflow");

// restore locals from the stack
	c = pr_xfunction->locals;
	localstack_used -= c;
	if (localstack_used < 0)
		PR_RunError ("PR_ExecuteProgram: locals stack underflow\n");

	for (i=0 ; i < c ; i++)
		((int *)pr_globals)[pr_xfunction->parm_start + i] = localstack[localstack_used+i];

// up stack
	pr_depth--;
	pr_xfunction = pr_stack[pr_depth].f;
	return pr_stack[pr_depth].s;
}

#define RUNAWAY_STEP (RUNAWAY / 5)
#define RUNAWAY	     5000000

/*
====================
PR_ExecuteProgram
====================
*/
void PR_ExecuteProgram (func_t fnum)
{
	eval_t		*a, *b, *c, *ptr;
	int		i, s, runaway, exitdepth;
	dstatement_t	*st;
	dfunction_t	*f, *newf;
	edict_t		*ed;
#ifdef EBFS
// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  start
	char	*funcname;
	char	*remaphint;
// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  end
#endif
	if (!fnum || fnum >= progs->numfunctions)
	{
		if (pr_global_struct->self)
			ED_Print (PROG_TO_EDICT(pr_global_struct->self));
		Host_Error ("PR_ExecuteProgram: NULL function");
	}
	
	f = &pr_functions[fnum];

	runaway = RUNAWAY; //was 100000;
	pr_trace = false;

// make a stack frame
	exitdepth = pr_depth;

	s = PR_EnterFunction (f);
	
while (1)
{
	s++;	// next statement
	pr_xstatement = s;
	pr_xfunction->profile++;

	st = &pr_statements[s];
	a = (eval_t *)&pr_globals[st->a];
	b = (eval_t *)&pr_globals[st->b];
	c = (eval_t *)&pr_globals[st->c];
	
	if (!--runaway)
		PR_RunError ("runaway loop error");
	
	if (runaway < RUNAWAY - RUNAWAY_STEP + 1 && runaway % RUNAWAY_STEP == 0)
	{
		Con_Printf ("PR_ExecuteProgram: runaway loop %d\n", runaway / RUNAWAY_STEP);
		SCR_UpdateScreen (); // Force screen update
		S_ClearBuffer ();    // Avoid looping sounds
	}
	if (pr_trace)
		PR_PrintStatement (st);
		
	switch (st->op)
	{
	case OP_ADD_F:
		c->_float = a->_float + b->_float;
		break;
	case OP_ADD_V:
		c->vector[0] = a->vector[0] + b->vector[0];
		c->vector[1] = a->vector[1] + b->vector[1];
		c->vector[2] = a->vector[2] + b->vector[2];
		break;
		
	case OP_SUB_F:
		c->_float = a->_float - b->_float;
		break;
	case OP_SUB_V:
		c->vector[0] = a->vector[0] - b->vector[0];
		c->vector[1] = a->vector[1] - b->vector[1];
		c->vector[2] = a->vector[2] - b->vector[2];
		break;

	case OP_MUL_F:
		c->_float = a->_float * b->_float;
		break;
	case OP_MUL_V:
		c->_float = a->vector[0]*b->vector[0]
				+ a->vector[1]*b->vector[1]
				+ a->vector[2]*b->vector[2];
		break;
	case OP_MUL_FV:
		c->vector[0] = a->_float * b->vector[0];
		c->vector[1] = a->_float * b->vector[1];
		c->vector[2] = a->_float * b->vector[2];
		break;
	case OP_MUL_VF:
		c->vector[0] = b->_float * a->vector[0];
		c->vector[1] = b->_float * a->vector[1];
		c->vector[2] = b->_float * a->vector[2];
		break;

	case OP_DIV_F:
		c->_float = a->_float / b->_float;
		break;
	
	case OP_BITAND:
		c->_float = (int)a->_float & (int)b->_float;
		break;
	
	case OP_BITOR:
		c->_float = (int)a->_float | (int)b->_float;
		break;
	
		
	case OP_GE:
		c->_float = a->_float >= b->_float;
		break;
	case OP_LE:
		c->_float = a->_float <= b->_float;
		break;
	case OP_GT:
		c->_float = a->_float > b->_float;
		break;
	case OP_LT:
		c->_float = a->_float < b->_float;
		break;
	case OP_AND:
		c->_float = a->_float && b->_float;
		break;
	case OP_OR:
		c->_float = a->_float || b->_float;
		break;
		
	case OP_NOT_F:
		c->_float = !a->_float;
		break;
	case OP_NOT_V:
		c->_float = !a->vector[0] && !a->vector[1] && !a->vector[2];
		break;
	case OP_NOT_S:
		c->_float = !a->string || !pr_strings[a->string];
		break;
	case OP_NOT_FNC:
		c->_float = !a->function;
		break;
	case OP_NOT_ENT:
		c->_float = (PROG_TO_EDICT(a->edict) == sv.edicts);
		break;

	case OP_EQ_F:
		c->_float = a->_float == b->_float;
		break;
	case OP_EQ_V:
		c->_float = (a->vector[0] == b->vector[0]) &&
					(a->vector[1] == b->vector[1]) &&
					(a->vector[2] == b->vector[2]);
		break;
	case OP_EQ_S:
		c->_float = !strcmp(pr_strings+a->string,pr_strings+b->string);
		break;
	case OP_EQ_E:
		c->_float = a->_int == b->_int;
		break;
	case OP_EQ_FNC:
		c->_float = a->function == b->function;
		break;


	case OP_NE_F:
		c->_float = a->_float != b->_float;
		break;
	case OP_NE_V:
		c->_float = (a->vector[0] != b->vector[0]) ||
					(a->vector[1] != b->vector[1]) ||
					(a->vector[2] != b->vector[2]);
		break;
	case OP_NE_S:
		c->_float = strcmp(pr_strings+a->string,pr_strings+b->string);
		break;
	case OP_NE_E:
		c->_float = a->_int != b->_int;
		break;
	case OP_NE_FNC:
		c->_float = a->function != b->function;
		break;

//==================
	case OP_STORE_F:
	case OP_STORE_ENT:
	case OP_STORE_FLD:		// integers
	case OP_STORE_S:
	case OP_STORE_FNC:		// pointers
		b->_int = a->_int;
		break;
	case OP_STORE_V:
		b->vector[0] = a->vector[0];
		b->vector[1] = a->vector[1];
		b->vector[2] = a->vector[2];
		break;
		
	case OP_STOREP_F:
	case OP_STOREP_ENT:
	case OP_STOREP_FLD:		// integers
	case OP_STOREP_S:
	case OP_STOREP_FNC:		// pointers
		ptr = (eval_t *)((byte *)sv.edicts + b->_int);
		ptr->_int = a->_int;
		break;
	case OP_STOREP_V:
		ptr = (eval_t *)((byte *)sv.edicts + b->_int);
		ptr->vector[0] = a->vector[0];
		ptr->vector[1] = a->vector[1];
		ptr->vector[2] = a->vector[2];
		break;
		
	case OP_ADDRESS:
		ed = PROG_TO_EDICT(a->edict);
		if (ed == (edict_t *)sv.edicts && sv.state == ss_active)
			PR_RunError ("assignment to world entity");
		c->_int = (byte *)((int *)&ed->v + b->_int) - (byte *)sv.edicts;
		break;
		
	case OP_LOAD_F:
	case OP_LOAD_FLD:
	case OP_LOAD_ENT:
	case OP_LOAD_S:
	case OP_LOAD_FNC:
		ed = PROG_TO_EDICT(a->edict);
		a = (eval_t *)((int *)&ed->v + b->_int);
		c->_int = a->_int;
		break;

	case OP_LOAD_V:
		ed = PROG_TO_EDICT(a->edict);
		a = (eval_t *)((int *)&ed->v + b->_int);
		c->vector[0] = a->vector[0];
		c->vector[1] = a->vector[1];
		c->vector[2] = a->vector[2];
		break;
		
//==================

	case OP_IFNOT:
		if (!a->_int)
			s +=  (signed short)st->b - 1;	// offset the s++
		break;
		
	case OP_IF:
		if (a->_int)
			s +=  (signed short)st->b - 1;	// offset the s++
		break;
		
	case OP_GOTO:
		s +=  (signed short)st->a - 1;	// offset the s++
		break;
		
	case OP_CALL0:
	case OP_CALL1:
	case OP_CALL2:
	case OP_CALL3:
	case OP_CALL4:
	case OP_CALL5:
	case OP_CALL6:
	case OP_CALL7:
	case OP_CALL8:
		pr_argc = st->op - OP_CALL0;
		if (!a->function)
			PR_RunError ("NULL function");

		newf = &pr_functions[a->function];

		if (newf->first_statement < 0)
		{	// negative statements are built in functions
			i = -newf->first_statement;

#ifndef EBFS
			if (i >= pr_numbuiltins)
				PR_RunError ("Bad builtin call number");
#else
// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  start

			if ( (i >= pr_numbuiltins)
			||   (pr_builtins[i] == pr_ebfs_builtins[0].function) )
			{
				funcname = pr_strings + newf->s_name;
				if (pr_builtin_remap.value)
				{
					remaphint = NULL;
				}
				else
				{
					remaphint = "Try \"builtin remapping\" by setting PR_BUILTIN_REMAP to 1\n";
				}
				PR_RunError ("Bad builtin call number %i for %s\nPlease contact the PROGS.DAT author\nUse BUILTINLIST to see all assigned builtin functions\n%s", i, funcname, remaphint);
			}
// 2001-09-14 Enhanced BuiltIn Function System (EBFS) by Maddes  end
#endif
			pr_builtins[i] ();
			break;
		}

		s = PR_EnterFunction (newf);
		break;

	case OP_DONE:
	case OP_RETURN:
		pr_globals[OFS_RETURN] = pr_globals[st->a];
		pr_globals[OFS_RETURN+1] = pr_globals[st->a+1];
		pr_globals[OFS_RETURN+2] = pr_globals[st->a+2];
	
		s = PR_LeaveFunction ();
		if (pr_depth == exitdepth)
			return;		// all done
		break;
		
	case OP_STATE:
		ed = PROG_TO_EDICT(pr_global_struct->self);
		ed->v.nextthink = pr_global_struct->time + 0.1;

		if (a->_float != ed->v.frame)
		{
			ed->v.frame = a->_float;
		}
		ed->v.think = b->function;
		break;
		
	default:
		PR_RunError ("Bad opcode %i", st->op);
	}
}

}
