
/*
 *	Copyright (c) 1987 Keith Muller
 *	All Rights Reserved
 *
 *	The author make no claims as to the fitness or correctness of
 *	this software for any use whatsoever, and it is provided ``as is''
 *	without express or implied warranty. Any use of this software is
 *	at the user's own risk.
 *
 *	Enhancements by David Dykstra at AT&T.
 *	This source code may be freely distributed.
 */
#include "common.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/ioctl.h>
#include <stdio.h>

int DebugLevel;
int DebugTimeout;
FILE *DebugFile;
struct layer layer[MAXPCHAN];
int booted;
int deadlayer[MAXPCHAN];
int layerdied;			/* layer shell died, must be cleaned */
int agentblock = 1;		/* agent messages may be waiting to send */
int agent_pend_set;
int rd_enabled;
int fdtolayer[MAXFD];
int exiting;
int ptracing;
char shell[MAXPATHLEN];
char ffile[MAXPATHLEN];
char dmdpath[MAXPATHLEN];
char dmdsyspath[MAXPATHLEN];
char lsys[MAXPATHLEN];
int Loadtype;
int dmdvers;
int dmdtype;
char asockdir[MAXPATHLEN];
char asockname[MAXPATHLEN];
int agentsock = -1;
int autodelete = 1;
int autocurrent = 1;
int active;
char fakespeed;
int numpcbufsarg;
int numpcbufs;
int maxpktarg;
int maxpkt;
int highwaterarg;
int highwater;
int networkxt;
int regularxtarg;
int nestedlayers;
#ifdef FAST
int oldpriority;
int fast = FAST;
#endif

char *dmd_ctrllist[] = {
	"(null msg)",
	"C_SENDCHAR",
	"C_NEW",
	"C_UNBLK",
	"C_DELETE",
	"C_EXIT",
	"C_BRAINDEATH",
	"C_SENDNCHARS",
	"C_RESHAPE",
	"C_JAGENT",
	"C_NOFLOW",
	"C_YESFLOW",
};
int dmd_maxctrl = (sizeof(dmd_ctrllist)/sizeof(char *)) - 1;

char *dmd_agentlist[] = {
	"(unknown agent)",
	"A_NEWLAYER",
	"A_CURRENT",
	"A_DELETE",
	"A_TOP",
	"A_BOTTOM",
	"A_MOVE",
	"A_RESHAPE",
	"A_NEW",
	"A_EXIT",
};
int dmd_maxagent = (sizeof(dmd_agentlist)/sizeof(char *)) - 1;

char *dmd_agentctrllist[] = {
	"A_JZOMBOOT",		/* 60 */
	"A_JBOOT",		/* 61 */
	"A_JTERM",		/* 62 */
	"A_CHAN",		/* 63 */
	"A_CHANGEPROC",		/* 64 */
	"A_RECV",		/* 65 */
	"A_DONE",		/* 66 */
	"A_RELOGIN",		/* 67 */
	"A_JXTPROTO",		/* 68 */
	"A_JTIMO",		/* 69 */
	"A_JTIMOM",		/* 70 */
	"A_XTSTATS",		/* 71 */
	"A_XTTRACE",		/* 72 */
	0
};

char *dmd_jioctllist[] = {
	"(unknown jioctl)",
	"JBOOT",
	"JTERM",
	"JMPX",
	"JTIMO",
	"JWINSIZE",
	"JTIMOM",
	"JZOMBOOT",
	"<08>",
	"JAGENT",
	"JTRUN",
	"JXTPROTO",
};
int dmd_maxjioctl = (sizeof(dmd_jioctllist)/sizeof(char *)) - 1;
