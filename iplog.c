//
// iplog.c
//
// JPG 1.05
//
// This entire file is from proquake 1.05.  It is used for player IP logging.
//

#include "quakedef.h"



iplog_t	*iplogs;
iplog_t *iplog_head;

int iplog_size;
int iplog_next;
int iplog_full;

#define DEFAULT_IPLOGSIZE	0x10000

/*
====================
IPLog_Init
====================
*/
void IPLog_Init (void)
{
	//int p;
	FILE *f;
	iplog_t temp;

	// Allocate space for the IP logs
	iplog_size = 0;
//	p = COM_CheckParm ("-iplog");
//	if (!p)
//		return;

//	if (p < com_argc - 1)
//		iplog_size = Q_atoi(com_argv[p+1]) * 1024 / sizeof(iplog_t);

//	if (!iplog_size)
		iplog_size = DEFAULT_IPLOGSIZE;

	iplogs = (iplog_t *) Hunk_AllocName(iplog_size * sizeof(iplog_t), "iplog");
	iplogs = (iplog_t *) Q_malloc (iplog_size * sizeof(iplog_t));
	iplog_next = 0;
	iplog_head = NULL;
	iplog_full = 0;

	// Attempt to load log data from iplog.dat
//	Sys_GetLock();
	f = fopen(va("%s/id1/iplog.dat",com_basedir), "r");
	if (f)
	{
		while(fread(&temp, 20, 1, f))
			IPLog_Add(temp.addr, temp.name);
		fclose(f);
	}
//	Sys_ReleaseLock();
}

/*
====================
IPLog_Import
====================
*/
void IPLog_Import (void)
{
	FILE *f;
	iplog_t temp;

	if (!iplog_size)
	{
		Con_Printf("IP logging not available\nUse -iplog command line option\n");
		return;
	}

	if (Cmd_Argc() < 2)
	{
		Con_Printf("Usage: ipmerge <filename>\n");
		return;
	}
	f = fopen(va("%s", Cmd_Argv(1)), "r");
	if (f)
	{
		while(fread(&temp, 20, 1, f))
			IPLog_Add(temp.addr, temp.name);
		fclose(f);
		Con_Printf("Merged %s\n", Cmd_Argv(1));
	}
	else
		Con_Printf("Could not open %s\n", Cmd_Argv(1));
}

/*
====================
IPLog_WriteLog
====================
*/
void IPLog_WriteLog (void)
{
	FILE *f;
	int i;
	iplog_t temp;

	if (!iplog_size)
		return;

//	Sys_GetLock();

	// first merge
	f = fopen(va("%s/id1/iplog.dat",com_basedir), "r");
	if (f)
	{
		while(fread(&temp, 20, 1, f))
			IPLog_Add(temp.addr, temp.name);
		fclose(f);
	}

	// then write
	f = fopen(va("%s/id1/iplog.dat",com_basedir), "w");
	if (f)
	{
		if (iplog_full)
		{
			for (i = iplog_next + 1 ; i < iplog_size ; i++)
				fwrite(&iplogs[i], 20, 1, f);
		}
		for (i = 0 ; i < iplog_next ; i++)
			fwrite(&iplogs[i], 20, 1, f);

		fclose(f);
	}	
	else
		Con_Printf("Could not write iplog.dat\n");

//	Sys_ReleaseLock();
}

#define MAX_REPITITION	64

/*
====================
IPLog_Add
====================
*/
void IPLog_Add (int addr, char *name)
{
	iplog_t	*iplog_new;
	iplog_t **ppnew;
	iplog_t *parent;
	char name2[16];
	char *ch;
	int cmatch;		// limit 128 entries per IP
	iplog_t *match[MAX_REPITITION];
	int i;

	if (!iplog_size)
		return;

	// delete trailing spaces
	strncpy(name2, name, 15);
	ch = &name2[15];
	*ch = 0;
	while (ch >= name2 && (*ch == 0 || *ch == ' '))
		*ch-- = 0;
	if (ch < name2)
		return;

	iplog_new = &iplogs[iplog_next];

	cmatch = 0;
	parent = NULL;
	ppnew = &iplog_head;
	while (*ppnew)
	{
		if ((*ppnew)->addr == addr)
		{
			if (!strcmp(name2, (*ppnew)->name))
			{
				return;
			}
			match[cmatch] = *ppnew;
			if (++cmatch == MAX_REPITITION)
			{
				// shift up the names and replace the last one
				for (i = 0 ; i < MAX_REPITITION - 1 ; i++)
					strcpy(match[i]->name, match[i+1]->name);
				strcpy(match[i]->name, name2);
				return;
			}
		}
		parent = *ppnew;
		ppnew = &(*ppnew)->children[addr > (*ppnew)->addr];
	}
	*ppnew = iplog_new;
	strcpy(iplog_new->name, name2);
	iplog_new->addr = addr;
	iplog_new->parent = parent;
	iplog_new->children[0] = NULL;
	iplog_new->children[1] = NULL;

	if (++iplog_next == iplog_size)
	{
		iplog_next = 0;
		iplog_full = 1;
	}
	if (iplog_full)
		IPLog_Delete(&iplogs[iplog_next]);
}

/*
====================
IPLog_Delete
====================
*/
void IPLog_Delete (iplog_t *node)
{
	iplog_t *newlog;

	newlog = IPLog_Merge(node->children[0], node->children[1]);
	if (newlog)
		newlog->parent = node->parent;
	if (node->parent)
		node->parent->children[node->addr > node->parent->addr] = newlog;
	else
		iplog_head = newlog;
}

/*
====================
IPLog_Merge
====================
*/
iplog_t *IPLog_Merge (iplog_t *left, iplog_t *right)
{
	if (!left)
		return right;
	if (!right)
		return left;

	if (rand() & 1)
	{
		left->children[1] = IPLog_Merge(left->children[1], right);
		left->children[1]->parent = left;
		return left;
	}
	right->children[0] = IPLog_Merge(left, right->children[0]);
	right->children[0]->parent = right;
	return right;
}

/*
====================
IPLog_Identify
====================
*/
void IPLog_Identify (int addr)
{
	iplog_t *node;
	int count = 0;

	node = iplog_head;
	while (node)
	{
		if (node->addr == addr)
		{
			Con_Printf("%s\n", node->name);
			count++;
		}
		node = node->children[addr > node->addr];
	}
	Con_Printf("%d %s found\n", count, (count == 1) ? "entry" : "entries");
}

/*
====================
IPLog_DumpTree
====================
*/
void IPLog_DumpTree (iplog_t *root, FILE *f)
{
	char address[16];
	char name[16];
	unsigned char *ch;

	if (!root)
		return;
	IPLog_DumpTree(root->children[0], f);

	sprintf(address, "%d.%d.%d.xxx", root->addr >> 16, (root->addr >> 8) & 0xff, root->addr & 0xff);
	strcpy(name, root->name);

	for (ch = name ; *ch ; ch++)
	{
		*ch = dequake[*ch];
		if (*ch == 10 || *ch == 13)
			*ch = ' ';
	}

	fprintf(f, "%-16s  %s %s\n", address, name);

	IPLog_DumpTree(root->children[1], f);
}

/*
====================
IPLog_Dump
====================
*/
void IPLog_Dump (void)
{
	FILE *f;

	if (!iplog_size)
	{
		Con_Printf("IP logging not available\nUse -iplog command line option\n");
		return;
	}

	f = fopen(va("%s/id1/iplog.txt",com_basedir), "w");
	if (!f)
	{
		Con_Printf ("Couldn't write iplog.txt.\n");
		return;
	}

	IPLog_DumpTree(iplog_head, f);
	fclose(f);
	Con_Printf("Wrote iplog.txt\n");
}

