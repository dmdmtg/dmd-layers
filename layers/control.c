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
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef USE_TERMIOS_H
#include <sys/termios.h>
#endif
#ifdef USE_TERMIO_H
#include <sys/termio.h>
#endif
#include "sys/jioctl.h"
#include "proto.h"
#include "pconfig.h"
#include "packets.h"

extern struct layer layer[];
extern char *dmd_ctrllist[];
extern int dmd_maxctrl;

struct winsize *getwinsize();
extern int numpcbufs;

control(lay, bp, cc)
int lay;
char *bp;
register int cc;
{
	extern struct layer layer[];
	register struct layer *lp = &layer[lay];
	register int i;
	char *cmd;
	extern int fdtolayer[];
	extern int rd_enabled;
	extern int exiting;
	extern char *agcmd();
	extern int active;
	extern int autodelete;

	Debug(DEBDETAIL,"control chan:%d bytes:%d cmd: ",lay,cc);
	if (*bp > dmd_maxctrl)
		Debug(DEBDETAIL, "%d\n", *bp);
	else
		Debug(DEBDETAIL, "%s\n", dmd_ctrllist[*bp]);

	switch (*bp++) {

	case C_SENDCHAR:
	case C_SENDNCHARS:
		--cc;
		if (lp->fd != -1) {
			check_write(lp->fd, bp, cc);
			break;
		}
		break;

	case C_NEW:
		if (allocchanpty(lay) == -1) {
			char *msg = "Error allocating pty";
			(void) psend(lay, msg, strlen(msg));
			break;
		}
		lp->flags |= AUTODELETE;
		makewinsize(lp, (unsigned char *) bp);
		cmd = agcmd(lay);
		if ((lp->pgrp = execprocess(lp->tty,getwinsize(lp),
							cmd,lay)) == -1) {
			Debug(DEBERROR,"cntrl execprocess fail %s\n",cmd);
			break;
		}
		SET_FD(lp->fd, rd_enabled);
		if (autodelete == 0)
			lp->flags &= ~AUTODELETE;
		break;

	case C_UNBLK:
		if (lay)  {
			if (!pconvs[lay].freepkts) {
				if (lp->fd != -1)
					CLR_FD(lp->fd, rd_enabled);
			} else {
				if (lp->fd != -1)
					SET_FD(lp->fd, rd_enabled);
			}
		}
		if (exiting && (pconvs[0].freepkts >= numpcbufs))
			quit(1);
		break;

	case C_DELETE:
		if (lp->flags&ACTIVE) {
			lp->flags &= ~ACTIVE;
			--active;
		}
		/* watch out for real mouse deletes that collide */
		lp->flags |= DELETING;
		deletehost(lay);
		if ((active == 0) && autodelete)
			startexit();
		break;

	case C_EXIT:
		startexit();
		break;

	case C_BRAINDEATH:
		signalprocess(lay, SIGTERM);
		slavettyset(lp->fd, lp->tty, (struct winsize *)0);
		break;

	case C_RESHAPE:
		makewinsize(lp, (unsigned char *) bp);
#ifndef NO_WINSIZE
		if (lp->fd != -1)
			(void) ioctl(lp->fd, TIOCSWINSZ, getwinsize(lp));
#endif
		break;

	case C_JAGENT:
		(void) recvagent(*bp, bp+1);
		break;

	case C_NOFLOW:
		lp->flags |= NONETFLOW;
		break;

	case C_YESFLOW:
		lp->flags &= ~NONETFLOW;
		break;

	default:
		/*
		 * something's busted
		 */
		Debug(DEBERROR,"invalid control command %d\n",*(bp-1));
		break;
	}
	return(0);
}

allocchanpty(lay)
int lay;
{
	extern int active;
	extern int fdtolayer[];
	int one = 1;
	struct layer *lp = &layer[lay];

	if ((lay < 0) || (lay >= MAXPCHAN)) {
		Debug(DEBERROR,"allocchanpty(%d): bad layer\n", lay);
		return(0);
	}

	if (lp->fd != -1) {
		Debug(DEBERROR,"allocchanpty(%d) already active\n", lay);
		return(0);
	}

	if ((lp->fd = getpty(lay, lp->tty)) == -1) {
		Debug(DEBERROR,"allocchanpty(%d) no more pty\n", lay);
		return(-1);
	}
	if (!(lp->flags & ACTIVE)) {
		active++;
		lp->flags = ACTIVE;   /* intentionally not '|='; clear flags */
	}
	fdtolayer[lp->fd] = lay;
#ifndef NO_TIOCUCNTL
	if (ioctl(lp->fd, JSMPX, &one) == -1) {
		Debug(DEBERROR,"allocchanpty(%d) JSMPX %s failed\n",lay,
							lp->tty);
		return(-1);
	}
#endif
	Debug(DEBINFO,"allocchanpty(%d) %s succeeded\n",lay,lp->tty);

	return(0);
}

sendioctl(lay, jioctl)
int lay;
char jioctl;
{
	char buf[2];

	Debug(DEBINFO,"sendioctl lay:%d jioctl:%o\n",lay, jioctl);

	/*
	 * The eighth bit is stripped off to be compatible with old R&D Unix
	 *   versions of this code which for some reason turned on the
	 *   eighth bit in the JBOOT, JZOMBOOT, and JTERM definitions.
	 */
	buf[0] = jioctl & 0x7f;
	buf[1] = (char) lay;

	if (psend(0, buf, sizeof(buf)) == -1) {
		Debug(DEBERROR,"sendioctl psend failed\n");
		return(-1);
	}
	return(0);
}

static
makewinsize(lp, bp)
register struct layer *lp;
register unsigned char *bp;
{
	lp->cols = *bp++;
	lp->rows = *bp++;
	lp->xpixels = *bp++;
	lp->xpixels |= (*bp++)<<8;
	lp->ypixels = *bp++;
	lp->ypixels |= (*bp++)<<8;
	Debug(DEBINFO,"channel %d size %d x %d bytes, %d x %d bits\n",
		lp - &layer[0], lp->cols, lp->rows, lp->xpixels, lp->ypixels);
}

struct winsize *
getwinsize(lp)
register struct layer *lp;
{
	static struct winsize win;

	win.ws_col = lp->cols;
	win.ws_row = lp->rows;
	win.ws_xpixel = lp->xpixels;
	win.ws_ypixel = lp->ypixels;

	return(&win);
}

startexit()
{
	extern int dmdtype;
	extern int exiting;
	extern int rd_enabled;

	/*
	 * 630's are done when they send a C_EXIT. 5620's will always
	 * follow with a C_UNBLK on channel 0 that must be read up first
	 */
	/*
	if ((dmdtype != DMD_5620) && (dmdtype != DMD_620))
	*/
	if (dmdtype != DMD_5620)
		quit(0);
	ZERO_FD(rd_enabled);
	SET_FD(0, rd_enabled);
	exiting = 1;
	(void) sendioctl(0, JTERM_CHAR);
}
