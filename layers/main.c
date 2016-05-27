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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/file.h>
#include <fcntl.h>
#ifdef USE_UNISTD
#include <unistd.h>
#endif
#ifdef USE_TTYDEV
#include <sys/ttydev.h>
#endif
#include <stdio.h>
#include <signal.h>

#include "sys/jioctl.h"

#include "proto.h"
#include "packets.h"
#ifdef USE_TERMIOS_H
#include <sys/termios.h>
#endif
#ifdef USE_TERMIO_H
#include <sys/termio.h>
#endif
#if (defined(FAST) && !defined(USE_NICE))
#include <sys/resource.h>
#endif

char * Version = "pseudo-tty Keith Muller + Dave Dykstra 2/11/98";

extern struct layer layer[];
extern int fdtolayer[];

extern int Loadtype;
extern int regularxtarg;
extern int maxpktarg;
extern int highwaterarg;
extern int numpcbufsarg;
extern int nestedlayers;

extern int DebugLevel;
extern FILE *DebugFile;

static int Ppstatistics = 0;
static int Pptrace = 0;

extern char *getenv();

#ifndef DEFDMDSYS
#define DEFDMDSYS "/usr"
#endif

main(argc, argv)
int argc;
char **argv;
{
	register int i;
	extern void catchsig();
	extern char *ttyname();
	extern char asockname[];
	extern char fakespeed;
	extern int booted;
	char *tty;
#	ifdef FAST
	extern int oldpriority;
	extern int fast;

#ifdef USE_NICE
	if (nice(fast) != -1)
		oldpriority = -fast;
	else
		/* nice fails if fast is negative and not super-user */
		oldpriority = 0;
#else
	oldpriority = getpriority(PRIO_PROCESS, 0);
	(void) setpriority(PRIO_PROCESS, 0, oldpriority + fast);
#endif
#	endif

	setuid(getuid()); /* no more reason to be setuid-root */
	setgid(getgid());

	if (parse(argc, argv))
		exit(1);

	if (!nestedlayers && (getenv(AGENV) != NULL)) {
		fprintf(stderr,
			"Layers: attempting to invoke layers within a layer");
		fprintf(stderr, " ($%s is set).\n", AGENV);
		fprintf(stderr, " Use '-l' if that is what you really want.\n");
		exit(1);
	}

#ifdef USE_SYSV_SIGNALS
	sighold (SIGHUP);
	sighold (SIGTERM);
#ifdef SIGTSTP
	sighold (SIGTSTP);
#endif
#else
	(void) sigblock(sigmask(SIGHUP)|sigmask(SIGTERM)|sigmask(SIGTSTP));
#endif
	
	layer[0].flags = NONETFLOW;
	for (i = 0; i < MAXPCHAN; i++) {
		layer[i].fd = -1;
		layer[i].pgrp = -1;
	}
	fdtolayer[0] = 0;
	for (i = 1; i < MAXFD; i++)
		fdtolayer[i] = -1;

	if ((tty = ttyname(0)) == (char *)0) {
		fprintf(stderr, "Layers: stdin is not a tty.\n");
		exit(1);
	}

	initutmp(tty);

	savemode(0);

	if (initagent(tty)) {
		fprintf(stderr, "Layers: Problems starting agent system.\n");
		exit(1);
	}

	moveutmp(1);

	setcntrl(0, tty);
#if defined(USE_SYSV_SIGNALS) || defined(USE_SYSV_WAIT)
	(void) sigset(SIGCHLD, SIG_DFL);
#else
	(void) signal(SIGCHLD, SIG_IGN);
#endif
	if (boot(0)) {
		resetcntrl(0, tty);
		(void) unlink(asockname);
		fprintf(stderr, "Layers: Terminal boot failed, Exiting.\n");
		moveutmp(0);
		exit(1);
	}

	/* Wait until after boot to set agent environment because boot */
	/* could call 32ld and we don't want it to think that it is in layers */
	initagentenv();

	mysleep(3); /* give time for terminal to initialize */
		  /* 630 only needs 1 second but 5620 needs 3 */

	booted = 1;

	setcntrl(0, tty);
#ifndef USE_SYSV_SIGNALS
	(void) signal(SIGHUP, catchsig);
	(void) signal(SIGTERM, catchsig);
	(void) signal(SIGCHLD, catchsig);
	(void) signal(SIGTSTP, SIG_IGN);
	(void) signal(SIGPIPE, SIG_IGN);
#else
	(void) sigset(SIGHUP, catchsig);
	(void) sigset(SIGTERM, catchsig);
	(void) sigset(SIGCHLD, catchsig);
	(void) sigset(SIGPIPE, SIG_IGN);
#endif

	if (protocolinit(0)) {
		(void) sendioctl(0, JTERM_CHAR);
		resetcntrl(0, tty);
		(void) unlink(asockname);
		fprintf(stderr, "Layers: Cannot start protocol, exiting.\n");
		exit(1);
	}
#ifdef USE_SYSV_SIGNALS
	sigrelse (SIGHUP);
	sigrelse (SIGTERM);
#else
	(void) sigsetmask(0);
#endif
	multiplex();
	quit(0);
}

quit(reset)
int reset;
{
	register int n;
	extern void flush_write();

#ifdef USE_SYSV_SIGNALS
	sighold (SIGHUP);
	sighold (SIGTERM);
#ifdef SIGTSTP
	sighold (SIGTSTP);
#endif
#else
	(void)sigblock(sigmask(SIGHUP)|sigmask(SIGTERM)|sigmask(SIGTSTP));
#endif

	for (n = 0; n < MAXPCHAN; n++)
		deletehost(n);

	exitagent();
	if (!reset)
		(void) sendioctl(0, JTERM_CHAR);
	flush_write();
	alarm(0);
	resetcntrl(0, ttyname(0));
	moveutmp(0);
	if (DebugLevel > 0)
		fpsummary(DebugFile);
	if (Pptrace)
		fpdumphist(stderr);
	if (Ppstatistics)
		fpsummary(stderr);
	exit(0);
}

usage()
{
fprintf(stderr,
"Usage: layers [-V] [-xrstl] [-m max-pkt] [-h high-water] [-b baud] \\\n");
fprintf(stderr,
"		[-f startupfile] [-d debuglevel] [-o debugfile] [lsys]\n");
fprintf(stderr,
" -V : print out version number and exit\n");
fprintf(stderr,
" -x : use hex encoding regular XT (same as DMDLOAD=hex)\n");
fprintf(stderr,
" -r : use regular XT, not network XT (same as DMDLOAD=regular)\n");
fprintf(stderr,
" -s : send protocol summary to stderr on exit (similar to xts)\n");
fprintf(stderr,
" -t : send protocol trace to stderr on exit (similar to xtt)\n");
fprintf(stderr,
" -l : attempt running layers even in a layers window (most terminals don't)\n");
fprintf(stderr,
" -m max-pkt: max packet size for regular XT (same as DMDMAXPKT=max-pkt)\n");
fprintf(stderr,
" -h high-water: high water for network XT (same as DMDHIGHWATER=high-water)\n");
fprintf(stderr,
" -b baud: baud rate to use for determining regular XT timeouts\n");
fprintf(stderr,
" -f startupfile: layers startup file (default $HOME/.layersrc)\n");
fprintf(stderr,
" -d debuglevel : enable debugging. debuglevel is the OR of the following:\n");
fprintf(stderr,
"	1 : non-protocol errors\n");
fprintf(stderr,
"	2 : low-volume information messages\n");
fprintf(stderr,
"	4 : protocol errors\n");
fprintf(stderr,
"	8 : protocol trace\n");
fprintf(stderr,
"	16 : raw bytes to/from terminal\n");
fprintf(stderr,
"	32 : high-volume information messages\n");
fprintf(stderr,
" -o debugfile : file to use for debug messages (default \"layers.debug\")\n");
fprintf(stderr,
" lsys : layersys file to download before starting protocol.  5620 only.\n");
}

parse(argc, argv)
int argc;
char **argv;
{
	register int retval;
	register char *p;
	char *debugfname = "layers.debug";
	extern char fakespeed;
	extern char ffile[];
	extern char dmdpath[];
	extern char dmdsyspath[];
	extern char shell[];
	extern char lsys[];
	extern char *optarg;
	extern int optind;
	extern char *strncpy();
	extern int ptracing;

	Loadtype = UNKNOWN_LOAD;
	if ((p = getenv("DMDLOAD")) != (char *)0) {
		if (strncmp(p, "hex", 3) == 0)
			Loadtype = HEX_LOAD;
		else if (strncmp(p, "regular", 7) == 0)
			regularxtarg = 1;
	}

	if ((p = getenv("DMDMAXPKT")) != (char *)0) {
		if((p[0] < '0') || (p[0] > '9') ||
			((maxpktarg = atoi(p)) < MAXPKTDSIZE) ||
					(maxpktarg > MAXOUTDSIZE)) {
			fprintf(stderr,
			      "Invalid DMDMAXPKT %d; should be %d to %d\n",
				maxpktarg, MAXPKTDSIZE, MAXOUTDSIZE);
			exit(1);
		}
	}

	if ((p = getenv("DMDHIGHWATER")) != (char *)0) {
		if((p[0] < '0') || (p[0] > '9') ||
			((highwaterarg = atoi(p)) < MINNETHIWAT) ||
					(highwaterarg > MAXNETHIWAT)) {
			fprintf(stderr,
			      "Invalid DMDHIGHWATER %d; should be %d to %d\n",
				highwaterarg, MINNETHIWAT, MAXNETHIWAT);
			exit(1);
		}
	}
	while ((retval = getopt(argc, argv, "Vxrstlf:b:h:m:n:d:o:")) != EOF) {
		switch (retval) {
		case 'V':
			fprintf(stderr, "layers version: %s\n", Version);
			exit(0);
			break;
		case 'x':
			/* encoding mode forced on */
			Loadtype = HEX_LOAD;
			break;
		case 'r':
			/* regular XT (network xt forced off */
			regularxtarg = 1;
			break;
		case 's':
			/* print protocol statistics on exit */
			Ppstatistics++;
			break;
		case 't':
			/* print protocol trace on exit */
			Pptrace++;
			ptracing = 1;
			break;
		case 'f':
			(void) strncpy(ffile, optarg, (MAXPATHLEN-1));
			break;
		case 'b':
			switch (atoi(optarg)) {
			case 19200:
				fakespeed = EXTA;
				break;
			case 9600:
				fakespeed = B9600;
				break;
			case 4800:
				fakespeed = B4800;
				break;
			case 2400:
				fakespeed = B2400;
				break;
			case 1200:
				fakespeed = B1200;
				break;
			case 300:
				fakespeed = B300;
				break;
			default:
				fakespeed = 0;
				break;
			}
			break;
		case 'h':
			if((optarg[0] < '0') || (optarg[0] > '9') ||
				((highwaterarg = atoi(optarg)) < MINNETHIWAT) ||
					    (highwaterarg > MAXNETHIWAT)) {
				fprintf(stderr,
				  "Invalid high water %d; should be %d to %d\n",
					highwaterarg, MINNETHIWAT, MAXNETHIWAT);
				exit(1);
			}
			break;
		case 'm':
			if((optarg[0] < '0') || (optarg[0] > '9') ||
				((maxpktarg = atoi(optarg)) < MAXPKTDSIZE) ||
						(maxpktarg > MAXOUTDSIZE)) {
				fprintf(stderr,
				      "Invalid maxpkt %d; should be %d to %d\n",
					maxpktarg, MAXPKTDSIZE, MAXOUTDSIZE);
				exit(1);
			}
			break;
		case 'n':
			/* 
			 * This is not documented because it is dangerous;
			 * if 4 is chosen and the max pkt size > (252/2) and
			 *   the terminal is not modified to support the
			 *   extra buffers, it will cause many NAKs.
			 */
			if((optarg[0] < '0') || (optarg[0] > '9') ||
				((numpcbufsarg = atoi(optarg)) < NPCBUFS) ||
						(numpcbufsarg > MAXNPCBUFS)) {
				fprintf(stderr,
				      "Invalid numpcbufs %d; should be %d to %d\n",
					numpcbufsarg, NPCBUFS, MAXNPCBUFS);
				exit(1);
			}
			break;

		case 'l':
			nestedlayers++;
			break;

		case 'd':
			DebugLevel = atoi(optarg);
			break;
		
		case 'o':
			debugfname = optarg;
			break;
			
		default:
			usage();
	    		return(-1);
		}
	}

	DebugInit(debugfname);

	if ((DebugLevel & (DEBPTRACE|DEBPERROR)) != 0)
		ptracing = 1;

	if (optind < argc)
		(void) strncpy(lsys, argv[optind], (MAXPATHLEN-1));
	else
		lsys[0] = '\0';
	if ((p = getenv("SHELL")) == (char *)0 || *p == '\0')
		p = DSHELL;
	(void) strncpy(shell, p, (MAXPATHLEN-1));
	if ((p = getenv("DMD")) == (char *)0 || *p == '\0')
		p = DEFDMD;
	(void) strncpy(dmdpath, p, (MAXPATHLEN-1));
	if ((p = getenv("DMDSYS")) == (char *)0 || *p == '\0')
		p = DEFDMDSYS;
	(void) strncpy(dmdsyspath, p, (MAXPATHLEN-1));
	return(0);
}
