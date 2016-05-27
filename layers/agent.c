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
#ifdef USE_DIRENT
#include <dirent.h>
#else
#include <sys/dir.h>
#endif
#ifndef USE_NAMED_PIPE
#include <sys/socket.h>
#include <sys/un.h>
#else
#include <sys/param.h>
#include <sys/stat.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include "sys/jioctl.h"

extern char* sys_errlist[];

static struct aglist *aghead = AGNIL;
static struct aglist *agtail = AGNIL;
static short xmax;
static short ymax;
static short xmin;
static short ymin;
static short border;
extern int agent_pend_set;
extern int agentsock;
extern char asockdir[];
extern char asockname[];

extern struct layer layer[];
extern char *strncpy();
extern char *strcpy();
extern char *malloc();
extern char *getenv();
extern struct aglist *makeaglist();

static char agentname[MAXPATHLEN];
#ifdef USE_PTY_FOR_AGENT
static char agentptyname[sizeof(layer[0].tty)];
static int agentptyfd;
#endif

static struct aglist *last_new_ap = NULL;
static int last_cur_chan;

/*
 * set up all the structures, sockets etc require for the agent system
 */

initagent(tty)
char *tty;
{
	extern char dmdpath[];
	extern char dmdsyspath[];
#ifndef USE_NAMED_PIPE
	struct sockaddr_un insock;
#endif
	if (strncmp(tty, "/dev/", strlen("/dev/")) == 0)
		tty += strlen("/dev/");
	(void)strcpy(agentname, tty);
	for(tty = agentname; *tty != '\0'; tty++)
		if (*tty == '/')
			*tty = '_';

	(void)sprintf(asockdir, "%s/xt", dmdpath);
	(void)sprintf(asockname, "%s/%s", asockdir, agentname);
	(void)unlink(asockname);
#ifdef USE_NAMED_PIPE

#ifdef USE_PTY_FOR_AGENT
	if ((agentsock = allocpty(-1, agentptyname)) < 0) {
		fprintf(stderr,
		    "layers: failure allocating pty for agent commands\r\n");
		return(-1);
	}
	/* open up the slave side to hold it open; this is the only open */
	/*   for read on the slave side but there are multiple writers */
	agentptyfd = open(agentptyname, O_RDONLY);
	Debug(DEBDETAIL, "Opened slave side %s; fd %d\n", agentptyname, agentptyfd);
	pushmodules(agentptyfd);
	setraw(agentptyfd);

#define MKNAMEDPIPE(NAME) symlink(agentptyname, NAME)
#define MKTYPE "symlink"

#else

#define MKNAMEDPIPE(NAME) mknod(NAME, S_IFIFO | 0600, 0)
#define MKTYPE "mknod"

#endif

	if (MKNAMEDPIPE(asockname) != 0) {
		(void)sprintf(asockdir, "%s/xt", dmdsyspath);
		(void)sprintf(asockname, "%s/%s", asockdir, agentname);
		(void)unlink(asockname);
		if (MKNAMEDPIPE(asockname) != 0) {
			fprintf (stderr,
				"layers: %s %s/xt/%s and %s and failed.\r\n",
					 MKTYPE, dmdpath, agentname, asockname);
			fprintf(stderr, "  Try changing $DMD or $DMDSYS.\r\n");
			fprintf(stderr, "  %s %s.\r\n",
				"Also note that $DMD/xt or $DMDSYS/xt",
				"must be world writable");
			return(-1);
		}
	}
#ifndef USE_PTY_FOR_AGENT
	if ((agentsock = open (asockname, O_RDWR, 0)) < 0) {
		fprintf (stderr, "layers: open (%s) failed: %s\r\n",
			 asockname, sys_errlist[errno]);
		perror("");
		return(-1);
	}
#endif
#else
	if ((agentsock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "layers: cannot open hostagent socket\r\n");
		return(-1);
	}

	insock.sun_family = AF_UNIX;
	(void)strcpy(insock.sun_path, asockname);
	if (bind(agentsock, &insock, strlen(asockname) + 2) < 0) {
		(void)sprintf(asockdir, "%s/xt", dmdsyspath);
		(void)sprintf(asockname, "%s/%s", asockdir, agentname);
		(void)unlink(asockname);
		(void)strcpy(insock.sun_path, asockname);
		if (bind(agentsock, &insock, strlen(asockname) + 2) < 0) {
			(void)close(agentsock);
			fprintf(stderr,
				"layers: bind failed on %s/xt/%s and %s.\r\n",
					dmdpath, agentname, asockname);
			fprintf(stderr, "  Try changing $DMD or $DMDSYS.\r\n");
			fprintf(stderr, "  %s %s.\r\n",
				"Also note that $DMD/xt or $DMDSYS/xt",
				"must be world writable");
			return(-1);
		}
	}
#endif /* USE_NAMED_PIPE */
#ifdef USE_SETFD
	if (fcntl(agentsock, F_SETFD, (char*)1) < 0)
#else
	if (ioctl(agentsock, FIOCLEX, (char *)0) == -1)
#endif
	{
		fprintf(stderr, "layers: close on exec failed on socket\r\n");
		return(-1);
	}
#if (!defined(USE_CHMOD0SOCKET) || defined(USE_NAMED_PIPE))
	if (chmod(asockname, 0600) < 0) {
#ifdef USE_PTY_FOR_AGENT
		fprintf(stderr,
			"Layers: warning: chmod of %s failed\r\n", asockname);
		fprintf(stderr,
			"   You probably don't own the pseudo-tty\r\n");
		fprintf(stderr,
			"   Anyone will be able to send agent calls\r\n");
#else
		fprintf(stderr, "layers: chmod failed on %s\r\n", asockname);
		(void) close(agentsock);
		(void) unlink(asockname);
		return(-1);
#endif
	}
#endif
	return(0);
}

initagentenv()
{
	static char envbuf[MAXPATHLEN+10];
	static char envbuf2[MAXPATHLEN+10];
	/*
	 * The putenv here is used so that child processes know what
	 * socket to write to for agent requests. See libwindows.
	 * The libwindows inlayers() function also keys off this variable
	 * to determine that it is running under pseudo-tty layers.
	 */
	sprintf(envbuf, "%s=%s", AGENV, agentname);
	if (putenv(envbuf) < 0) {
		fprintf(stderr, "layers: could not putenv agent name\r\n");
		(void) close(agentsock);
		(void) unlink(asockname);
		return(-1);
	}
	/*
	 * Also tell them exactly what directory the socket is in because
	 *  libwindows programs might be compiled on a machine which have
	 *  different default $DMD and $DMDSYS.  Also prevents problems
	 *  if the user changes $DMD or $DMDSYS after getting into layers.
	 */
	sprintf(envbuf2, "%s=%s", AGDIRENV, asockdir);
	(void) putenv(envbuf2);
	return(0);
}

/*
 * Process an agent request from a process which just made a libwindows
 *   library call.
 * We immediately send back an A_RECV answer for every request made
 *   and then later send back the real response (usually after getting
 *   the response from the terminal.  The libwindows library sends back
 *   an A_DONE response to our later response.
 *
 *  libwindows	
 *    program		     layers		    terminal	 STATE
 *	| ===== A_request ====>	| 			|	AG_PEND
 *	| <==== A_RECV    =====	|			|
 *	  Wait until no pending data from pseudo-tty.		AG_NEW
 *	  Wait for other agent calls sent to terminal.		AG_SENT
 *	|			| ===== A_request ====>	|
 *	|			| <==== A_request =====	|	AG_COMP
 *	| <==== A_DONE	  =====	|			|
 *	| ===== A_DONE	  ====>	|			|
 */

/* receive an agent packet from a libwindows process */
recvhostagent(fd)
int fd;
{
	register struct aglist *ap;
	struct request req;
	int fromlen = 0;
	int recvlen;

#ifndef USE_NAMED_PIPE
	if ((recvlen = recvfrom(fd, &req, sizeof(req), 0,
			(struct sockaddr *) 0, &fromlen)) < 0) {
		Debug(DEBERROR, "recvhostagent: recvfrom(%d) failed: %s\n", fd,
							sys_errlist[errno]);
		startagent();
		return;
	}
#else
	if ((recvlen = read (fd, &req, sizeof(req))) < 0) {
		Debug(DEBERROR, "recvhostagent: read(%d) failed: %s\n", fd,
							sys_errlist[errno]);
		startagent();
		return;
	}
#endif /* USE_NAMED_PIPE */

	if (recvlen < sizeof(req)) {
		/* must be an older style request with smaller structure */
		bzero(((char *)&req) + recvlen, sizeof(req) - recvlen);
	}

	Debug(DEBINFO,"rcvhagnt ch:%d cmd:%d time: %x pid: %d chan:%s\n",
		req.ar.chan, req.ar.command, req.time, req.pid, req.chanstr);

	for (ap = aghead; ap != AGNIL; ap = ap->next) {
		if (ap->pid != req.pid)
			continue;
		switch(req.ar.command) {
			case A_RECV:	/* don't really expect A_RECV */
			case A_DONE:
				/*
				 * ack complete, just remove
				 */
				delagent(ap);
				break;
			default:
				/* send back previous response ??? */
				sendhostagent(ap);
				break;
		}
		startagent();
		return;
	}
	switch(req.ar.command) {
		case A_RECV:
		case A_DONE:
		    /* received a response to non-pending request ??? */
		    startagent();
		    return;
	}

	/*
	 * a new agent request
	 */
	if ((ap = (struct aglist *)malloc(sizeof(struct aglist))) == AGNIL)
		return;
	bcopy(&req, ap, sizeof(struct request));
	ap->result = 0;
	ap->state = AG_NEW;
	ap->flags = 0;
	if ((ap->buf = malloc(ANLEN*sizeof(char))) == (char *) 0) {
		/* out of memory */
		ap->result = 1;
		ap->state = AG_COMP;
	}
	else
		bzero(ap->buf, ANLEN*sizeof(char));
	ap->next = AGNIL;
#ifdef USE_NAMED_PIPE
	ap->xtfd = -1;
#endif

	if (req.chanstr[0] == '\0') {
		/* old style request, request channel not known */
		ap->reqchan = -1;
	}
	else if (req.chanstr[0] != '/') {
		/* explicit channel number */
		ap->reqchan = atoi(req.chanstr);
	}
	else {
		int i;
		struct layer *lp;
		for(i = 0; i < MAXPCHAN; i++) {
			lp = &layer[i];
			if ((lp->fd != -1) && (lp->tty != 0) &&
					    matchtty(lp,req.chanstr))
				break;
		}
		if (i == MAXPCHAN) {
			ap->result = 1;
			ap->state = AG_COMP;
			ap->reqchan = -1;
		}
		else
			ap->reqchan = i;
	}

	Debug(DEBDETAIL, "rcvhagent: %s is channel %d\n", req.chanstr,
								ap->reqchan);

	if ((ap->reqchan != -1) && (ap->state == AG_NEW)
			    && ((fd = layer[ap->reqchan].fd) != -1)) {
		int req_set;
		if (ISSET_FD(fd, agent_pend_set))
			/* another request is already pending */
			ap->state = AG_PEND;
		else {
			ZERO_FD(req_set);
			SET_FD(fd, req_set);

			if (!ISZERO_FD(isdatapending(req_set))){
				/*
				 * delay processing the request until data from
				 *  pseudo-tty is finished
				 */
				SET_FD(fd, agent_pend_set);
				ap->state = AG_PEND;
				Debug(DEBDETAIL, "data pending on fd %d\n", fd);
			}
		}
	}
	addagent(ap);
	sendhostagent(ap);
	startagent();
}

/* send an agent packet to a libwindows process */
sendhostagent(ap)
register struct aglist *ap;
{
	struct answer ans;
#ifdef USE_NAMED_PIPE
	char path[MAXPATHLEN];
#else
	int len;
	struct sockaddr_un outsock;
#endif
	extern int errno;

	printagent("sndhstagnt", ap);

	if (ap->pid == 0) {
		/* a local (non-libwindows) request */
		if (ap->ar.command == A_XTPROTO) 
			xtprotoresp(ap->result, (unsigned char) ap->buf[1]);
		if (ap == last_new_ap)
			last_cur_chan = ap->ar.chan;
		delagent(ap);
		startagent();
		return;
	}
	ans.result = ap->result;
	ans.chan = ap->ar.chan;
	ans.time = ap->time;
	if (ap->buf != (char *)0)
		bcopy(ap->buf, ans.answerbuf, sizeof(ans.answerbuf));
#ifdef USE_NAMED_PIPE
	sprintf (path, "%s.%u", asockname, ap->pid);
#else
	outsock.sun_family = AF_UNIX;
	(void)sprintf(outsock.sun_path, "%s.%u", asockname, ap->pid);
	len = strlen(outsock.sun_path) + 1 + sizeof(outsock.sun_family);
#ifdef USE_CHMOD0SOCKET
	chmod(outsock.sun_path, 0);
#endif
#endif
	switch (ap->state) {
		case AG_NEW:
		case AG_SENT:
		case AG_PEND:
			ans.command = A_RECV;
			break;
		case AG_COMP:
			ans.command = A_DONE;
			break;
		default:
			return;
	}
#ifdef USE_NAMED_PIPE
	if (ap->xtfd == -1) {
		if ((ap->xtfd = open (path, O_WRONLY, 0)) < 0) {
			Debug(DEBERROR, "sendhostagent: open (%s) failed: %s\n",
				 path, sys_errlist[errno]);
		}
	}
	if (ap->xtfd >= 0) {
		errno = 0;
		if (check_write (ap->xtfd, &ans, sizeof(ans)) == sizeof(ans))
			return;
		Debug(DEBERROR, "sendhostagent: can't write to %s: %s\n",
			 path, sys_errlist[errno]);
	}
#else
	if (sendto(agentsock, &ans, sizeof(ans), 0, &outsock, len) >= 0)
		return;
	if ((errno != ENOTSOCK) && (errno != ECONNREFUSED) && (errno != ENOENT)
	    && (errno != EPROTOTYPE))
		return;
#endif /* USE_NAMED_PIPE */
	/* there was an error sending */
	if (ap->state == AG_SENT) {
		ap->state = AG_DEAD;
		return;
	}
	delagent(ap);
}

addagent(ap)
register struct aglist *ap;
{
	if (ap == AGNIL)
		return;

	printagent("addagent", ap);

	if (aghead == AGNIL) {
		aghead = ap;
		agtail = ap;
	} else {
		agtail->next = ap;
		agtail = ap;
	}
}

delagent(targ)
register struct aglist *targ;
{
	register struct aglist *prev;
	register struct aglist *ap;

	if (targ == AGNIL)
		return;

	printagent("delagent", targ);

	for (ap=aghead,prev=AGNIL;(ap!=AGNIL)&&(ap!=targ);prev=ap,ap=ap->next);
	if (ap == AGNIL) 
		goto remove;
	if (targ == aghead)
		aghead = targ->next;
	else
		prev->next = ap->next;
	if (targ == agtail) {
		if (prev != AGNIL)
			agtail = prev;
		else
			agtail = aghead;
	}
    remove:
	if (targ->buf != (char *)0)
		(void) free(targ->buf);
#ifdef USE_NAMED_PIPE
	if (targ->xtfd != -1)
		(void) close(targ->xtfd);
#endif
	(void) free(targ);
}

startagent()
{
	register struct aglist *ap;
	register struct layer *lp;
	char ctlbuf[9];
	char path[MAXPATHLEN];
	FILE *fp;
	extern int rd_enabled;
	extern int agentblock;
	extern int dmdtype;
	extern int dmdvers;
	extern int networkxt;

	if (agentblock)
		Debug(DEBDETAIL, "startagent after blocked\n");

	agentblock = 0;
	if (aghead == AGNIL)
		return;
	/*
	 * Check for unacked packets on channel 0 because most agent calls
	 *  send on channel 0.  This check fixes the problem of attempting
	 *  to send too many packets on channel 0 if multiple dmdld's are
	 *  starting at the same time with A_JBOOT.  This is also used at
	 *  initialization time to delay startup file agent requests while
	 *  still initializating the protocol.
	 * All that really needs to be done is to make sure that there's at
	 *  least one free packet, but if any agent calls ever want to
	 *  send two control channel messages in a row this will allow it
	 *  (because NPCBUFS is 2).
	 */
	if (isunacked(0)) {
		Debug(DEBINFO,
			"unacked packets on channel 0, blocking agent\n");
		agentblock = 1;
		return;
	}
	/* check for dead requesting processes */
	for (ap = aghead; ap != AGNIL; ap = ap->next){
		if ((ap->pid == 0) || (ap->state == AG_DEAD))
			continue;
		if (kill(ap->pid, 0) >= 0)
			/* process is still alive */
			continue;
		if (ap->state == AG_SENT) {
			ap->state = AG_DEAD;
			continue;
		}
		delagent(ap);
	}

	for (ap = aghead; ap != AGNIL; ap = ap->next) {
		switch(ap->state) {
		case AG_NEW:
			switch(ap->ar.command) {
			case A_JZOMBOOT:
			case A_JBOOT:
			case A_JTERM:
				if (ap->reqchan == -1)
					ap->result = 1;
				else switch(ap->ar.command)
				{
				case A_JZOMBOOT:
					(void) sendioctl(ap->reqchan,
								JZOMBOOT_CHAR);
					break;
				case A_JBOOT:
					(void) sendioctl(ap->reqchan,
								JBOOT_CHAR);
					break;
				case A_JTERM:
					(void) sendioctl(ap->reqchan,
								JTERM_CHAR);
					break;
				}
				break;
			case A_JXTPROTO:
				ctlbuf[0] = JXTPROTO_CHAR;
				ctlbuf[1] = ap->ar.r.origin.x;
				if (networkxt) {
					/* need to transmit this with reg xt */
					networkxt = 0;
					psend(0, ctlbuf, 2);
					networkxt = 1;
				}
				else
					psend(0, ctlbuf, 2);
				break;
			case A_JTIMO:
				ctlbuf[0] = JTIMO_CHAR;
				ctlbuf[1] = ap->ar.r.origin.x;
				ctlbuf[2] = ap->ar.r.origin.y;
				psend(0, ctlbuf, 3);
				break;
			case A_JTIMOM:
				ctlbuf[0] = JTIMOM_CHAR;
				ctlbuf[1] = (ap->ar.r.origin.x) >> 8;
				ctlbuf[2] = ap->ar.r.origin.x;
				ctlbuf[3] = (ap->ar.r.origin.y) >> 8;
				ctlbuf[4] = ap->ar.r.origin.y;
				psend(0, ctlbuf, 5);
				break;
			case A_RELOGIN:
				if ((ap->ar.chan < 1) || !moveutmp(ap->ar.chan))
					ap->result = 1;
				break;
			case A_XTSTATS:
			case A_XTTRACE:
				sprintf(path, "/tmp/xt%c.%d",
				    ap->ar.command == A_XTSTATS ? 's' : 't',
						ap->pid);
				if ((fp = fopen(path, "w")) == NULL)
					ap->result = 1;
				else {
					if (ap->ar.command == A_XTSTATS)
						fpsummary(fp);
					else
						fpdumphist(fp);
					fclose(fp);
				}
				break;
			case A_ROMVERSION:
				/* don't send the command to the terminal */
				/* we already know the answer */
				(void)sprintf(ap->buf, "8;%.1d;%.1d",
						dmdtype, dmdvers);
				break;
			case A_CHAN:
				if (allocchanpty(ap->ar.chan) == -1) {
					ap->result = 1;
				}
				else {
					lp = &layer[ap->ar.chan];
					strncpy(ap->buf,
					    &lp->tty[sizeof("/dev/")-1], ANLEN);
				}
				lp->pgrp = ap->pid;
				SET_FD(lp->fd, rd_enabled);
				/* 
				 * Have to wait until other side of pseudo-tty
				 *   is opened before setting window size.
				 */
				lp->flags |= SETWINSIZE;
				break;
			case A_CHANGEPROC:
				/* change owner process for the channel */
				if ((ap->ar.chan < 1) ||
						(ap->ar.chan >= MAXPCHAN)) {
					ap->result = 1;
				}
				else {
					layer[ap->ar.chan].pgrp =
						ap->ar.r.origin.x +
						    (ap->ar.r.origin.y << 16);
				}
				break;
			case A_CURRENT:
				if ((ap->pid == 0) && (ap->ar.chan == 0)) {
					if (last_new_ap == NULL) {
						Debug(DEBINFO, "%s %s %s\n",
						    "skipping making last",
						    "window current; another",
						    "window already current");
						break;
					}
					ap->ar.chan = last_cur_chan;
					Debug(DEBINFO,
					    "making last window (%d) current\n",
							last_cur_chan);
				}
				/*
				 * Set last_new_ap to NULL here so if 
				 *  another A_CURRENT comes in before the
				 *  the one for the last window in the rcfile,
				 *  it will be skipped.
				 */
				last_new_ap = NULL;
				/* fall through */
			default:
				if (sendagent(ap) == 0) {
					ap->state = AG_SENT;
					return;
				}
				/*
				 * the agent command was not sent, queue up and
				 * try again on next pass through multiplex
				 */
				agentblock = 1;
				Debug(DEBERROR, "sendagent failed\n");
				return;
			}
			/* one of the special agent calls, reply immediately */
			ap->state = AG_COMP;
			sendhostagent(ap);
			return;
		case AG_SENT:
		case AG_DEAD:
			return;
		}
	}
}

rcfile()
{
	register int n;
	register struct aglist *ap;
	register char *p;
	char *sp;
	int lay;
	short com;
	int x1, y1, x2, y2;
	FILE *fp;
	char command[BUFSIZ];
	char inbuf[BUFSIZ];
	int len,interact;
	short vxmax, vymax, vxmin, vymin, wxmax, wymax, wxmin, wymin;
	extern int autodelete;
	extern int autocurrent;
	extern char ffile[];
	extern int dmdtype;

	switch(dmdtype) {
	case DMD_630:
		border = BORD_630;
		xmax = XMAX_630;
		ymax = YMAX_630;
		xmin = XMIN_630;
		ymin = YMIN_630;
		break;
	case DMD_615:
		xmax = VXMAX_615;
		ymax = VYMAX_615;
		xmin = VXMIN_615;
		ymin = VYMIN_615;
		vxmax = VXMAX_615;
		vymax = VYMAX_615;
		vxmin = VXMIN_615;
		vymin = VYMIN_615;
		wxmax = WXMAX_615;
		wymax = WYMAX_615;
		wxmin = WXMIN_615;
		wymin = WYMIN_615;
		break;
	case DMD_620:
		border = BORD_620;
		xmax = XMAX_620;
		ymax = YMAX_620;
		xmin = XMIN_620;
		ymin = YMIN_620;
		vxmax = VXMAX_620;
		vymax = VYMAX_620;
		vxmin = VXMIN_620;
		vymin = VYMIN_620;
		wxmax = WXMAX_620;
		wymax = WYMAX_620;
		wxmin = WXMIN_620;
		wymin = WYMIN_620;
		break;
	case DMD_5620:
	default:
		border = BORD_5620;
		xmax = XMAX_5620;
		ymax = YMAX_5620;
		xmin = XMIN_5620;
		ymin = YMIN_5620;
		break;
	}
	if ((ffile[0] == '\0') && ((p = getenv("HOME")) != (char *)0) &&
	    (*p != '\0'))
		(void)sprintf(ffile, "%s/%s", p, LAYERSRC);
	if ((fp = fopen(ffile, "r")) == NULL)
		return;
	while (fgets(inbuf, sizeof(inbuf), fp) != NULL) {
		if (inbuf[0] == '#')
			continue;
		for (sp = inbuf; (*sp != '\0') && (isspace(*sp)); sp++);
		if (isdigit(*sp)) {
			lay = 0;
			com = A_NEW;
			n = sscanf(sp," %d %d %d %d %[^\n]", &x1, &y1, &x2,
				   &y2, command);
			switch (n) {
			case 4:
			case 5:
				interact = 1;
				break;
			default:
				continue;
			}
		} else {
			for (p = sp; (*p != '\0') && (!isspace(*p)); p++);
			if (*p == '\0')
				continue;
			*p = '\0';
			for (p = p+1; (*p != '\0') && (isspace(*p)); p++);
			if (strcmp(sp, KEY_AUTOD) == 0) {
				if (strncmp(p, KEY_ON, 2) == 0)
					autodelete = 1;
				else if (strncmp(p, KEY_OFF, 3) == 0)
					autodelete = 0;
				continue;
			}
			if (strcmp(sp, KEY_AUTOC) == 0) {
				if (strncmp(p, KEY_ON, 2) == 0)
					autocurrent = 1;
				else if (strncmp(p, KEY_OFF, 3) == 0)
					autocurrent = 0;
				continue;
			}
			if (strcmp(sp, KEY_NEW) == 0) {
				n = sscanf(p,"%d %d %d %d %[^\n]", &x1, &y1,
					   &x2, &y2, command);
				switch (n) {
				case 4:
				case 5:
					interact = 1;
					break;
				default:
					continue;
				}
				lay = 0;
				com = A_NEW;
				goto checkit;
			}
			if (strcmp(sp, KEY_INTERACT) == 0) {
				n = sscanf(p,"%d %d %d %d %[^\n]", &x1, &y1,
					   &x2, &y2, command);
				if (n < 4)
					continue;
				lay = 0;
				com = A_NEW;
				interact = 1;
				goto checkit;
			}
			if (strcmp(sp, KEY_COMMAND) == 0) {
				n = sscanf(p,"%d %d %d %d %[^\n]", &x1, &y1,
					   &x2, &y2, command);
				if (n < 4)
					continue;
				lay = 0;
				com = A_NEW;
				interact = 0;
				goto checkit;
			}
			n = sscanf(p, "%d %d %d %d %d", &lay,&x1,&y1,&x2,&y2);
			Debug(DEBINFO, "rcfile ct:%d lay:%d cmd:%s\n", n, lay,
							inbuf);
			if ((n <= 0) || (lay < 0) || (lay >= MAXPCHAN))
				continue;
			if (strcmp(sp, KEY_DELETE) == 0) {
				(void) deleteterm(lay, 0);
				continue;
			}
			if (strcmp(sp, KEY_TOP) == 0) {
				if (n != 1)
					continue;
				x1 = x2 = y1 = y2 = 0;
				com = A_TOP;
			} else if (strcmp(sp, KEY_CURRENT) == 0) {
				if (n != 1)
					continue;
				x1 = x2 = y1 = y2 = 0;
				com = A_CURRENT;
			} else if (strcmp(sp, KEY_BOTTOM) == 0) {
				if (n != 1)
					continue;
				x1 = x2 = y1 = y2 = 0;
				com = A_BOTTOM;
			} else if (strcmp(sp, KEY_RESHAPE) == 0) {
				if (n != 5)
					continue;
				n = 4; /* hack */
				com = A_RESHAPE;
			} else if (strcmp(sp, KEY_MOVE) == 0) {
				if (n != 3)
					continue;
				com = A_MOVE;
				/* hack */
				x2 = xmax;
				y2 = ymax;
			} else
				continue;
		}
	    checkit:
		/*
		 * watch out for pixel or character sizes. the 620 does both
		 */
		if ((dmdtype == DMD_615) || ((y2 == 0)&&(dmdtype == DMD_620))){
			/*
			 * character size spec
			 */
			if (y2 != 0)
				continue;
			if ((GETX(x1) > wxmax) || (GETY(x1) > wymax) ||
			    (GETX(y1) > vxmax) || (GETY(y1) > vymax) ||
			    (GETX(x2) > wxmax) || (GETY(x2) > wymax))
				continue;
			if ((GETX(x1) < 0) || (GETY(x1) < 0) ||
			    ((GETX(y1) < vxmin) && (GETX(y1) != 0)) ||
			    ((GETY(y1) < vymin) && (GETY(y1) != 0)) ||
			    ((GETX(x2) < wxmin) && (GETX(x2) != 0)) ||
			    ((GETY(x2) < wymin) && (GETY(x2) != 0)))
				continue;
		} else {
			/*
			 * pixel size spec
			 */
			if ((x1 > x2) || (y1 > y2))
				continue;
			if (((x1 == x2)&&(x1 != 0)) || ((y1 == y2)&&(y1 != 0)))
				continue;
			if (x1 < xmin)
				x1 = xmin;
			if (y1 < ymin)
				y1 = ymin;
			if (x2 > xmax)
				x2 = xmax;
			if (y2 > ymax)
				y2 = ymax;
		}
		if ((ap = makeaglist(lay, com)) == AGNIL) {
			(void) fclose(fp);
			return;
		}
		if ((n == 5) && (command[0] != '#')) {
			len = strlen(command) + 1;
			if ((ap->buf = malloc(len)) == (char *)0) {
				(void)free(ap);
				(void) fclose(fp);
				return;
			}
			bcopy(command, ap->buf, len);
		}
		if ((com == A_NEW) && (interact == 0))
			ap->flags |= COMMAND;
		ap->ar.r.origin.x = x1;
		ap->ar.r.origin.y = y1;
		if (com == A_MOVE) {
			ap->ar.r.corner.x = 0;
			ap->ar.r.corner.y = 0;
		} else {
			ap->ar.r.corner.x = x2;
			ap->ar.r.corner.y = y2;
		}
		addagent(ap);

		if (com == A_NEW)
			last_new_ap = ap;
	}
	(void) fclose(fp);
	if (last_new_ap != NULL) {
		/*
		 * Put in a agent command to make the last window current,
		 *  to be compatible with XT-driver based layers.
		 * We don't know what the channel number of the last window
		 *  is yet so for now we'll make the channel number zero, but
		 *  we need to create it now to avoid race conditions with
		 *  other "current" commands.
		 * Care has to be taken to make sure that a user-specified
		 *  "current" command, either through the rcfile or through
		 *  libwindows, doesn't get overridden (see code elsewhere
		 *  in this file dealing with "last_new_ap").
		 */
		if ((ap = makeaglist(0, A_CURRENT)) != NULL) {
			ap->ar.r.origin.x = 0;
			ap->ar.r.origin.y = 0;
			ap->ar.r.corner.x = 0;
			ap->ar.r.corner.y = 0;
			addagent(ap);
		}
	}
}

/* send an agent packet to the terminal */
sendagent(agent)
struct aglist *agent;
{
	char buf[2*sizeof(char) + sizeof(struct agentrect)];
	register char *arpt = &buf[2*sizeof(char)];

	printagent("sendagent", agent);

	buf[0] = C_JAGENT;
	buf[1] = sizeof(struct agentrect);
#	ifdef USE_LITTLEENDIAN 
	swab((char *)agent, (char *)arpt, sizeof(struct agentrect));
#	else
	bcopy((char *)agent, (char *)arpt, sizeof(struct agentrect));
#	endif
	if (psend(0, buf, sizeof(buf)) == -1)
		return(-1);
	return(0);
}

/* receive agent packet from the terminal */
recvagent(recvlen, agntpt)
int recvlen;
char *agntpt;
{
	register struct aglist *ap;
	struct agentrect tmpagent;
	struct agentrect *agpt = &tmpagent;
	extern int active;
	extern int autodelete;
	extern int dmdtype;

#	ifdef USE_LITTLEENDIAN
	swab(agntpt, (char *)agpt, sizeof(struct agentrect));
#	else
	bcopy(agntpt, (char *)agpt, sizeof(struct agentrect));
#	endif

	if (recvlen < sizeof(struct agentrect))
		bzero(((char *)agpt) + recvlen, sizeof(struct agentrect) - recvlen);

	if (recvlen < 2) {
		/*
		 * 5620 responds with one byte (0) to agent calls it doesn't
		 *   understand.
		 */
		agpt->command = -1;
	}

	Debug(DEBINFO,"recvagent: len: %d chan: %d cmd: %d, %d/%d/%d/%d\n",
		recvlen, agpt->chan, agpt->command, agpt->r.origin.x,
		agpt->r.origin.y, agpt->r.corner.x, agpt->r.corner.y);
	/*
	 * assumes that there is only ONE outstanding agent request.
	 */
	for (ap = aghead; ap != AGNIL; ap = ap->next){
		if (ap->state != AG_SENT)
			continue;
		ap->state = AG_COMP;
		ap->result = (agpt->command == (short) -1);
		if (ap->buf != (char *)0) {
			/* return un-swabbed data */
			if (recvlen > (2 * sizeof(short))) {
				bcopy(agntpt + (2 * sizeof(short)), ap->buf,
					recvlen - (2 * sizeof(short)));
			}
		}
		ap->ar.chan = agpt->chan;
		if ((ap->ar.command == A_NEWLAYER) && (ap->result == 0)) {
			active++;
			layer[ap->ar.chan].flags |= ACTIVE;
		}
		/*
		 * Most 620's have bug that returns ok on ALL agent calls, even
		 * those that fail
		 */
		if ((autodelete) && (ap->ar.command == A_DELETE) &&
		    ((ap->result != 0) || (dmdtype == DMD_620)) &&
		    (ap->ar.chan > 0) && (layer[ap->ar.chan].flags & ACTIVE)) {
			if (active > 1)
				deletehost(ap->ar.chan);
			else {
				delagent(ap);
				startexit();
			}
		}
		if (ap->state == AG_DEAD) {
			delagent(ap);
			break;
		}
		sendhostagent(ap);
		break;
	}
	startagent();
	return(0);
}

char *
agcmd(lay)
int lay;
{
	register struct aglist *ap;
	short ax, ay;
	extern int dmdtype;

	if (aghead == AGNIL)
		return ((char *)0);
	for (ap = aghead; ap != AGNIL; ap = ap->next) {
		if (ap->state == AG_SENT)
			break;
	}
	if ((ap == AGNIL) || (ap->pid != 0) || (ap->ar.command != A_NEW) ||
	    (ap->flags&IN_PROG))
		return ((char *)0);
	Debug(DEBINFO, "agcmd c.x: %d c.y: %d o.x: %d o.y: %d\n",
		ap->ar.r.corner.x, ap->ar.r.corner.y,
		ap->ar.r.origin.x, ap->ar.r.origin.y);

	if (((dmdtype == DMD_620) && (ap->ar.r.corner.y == 0)) ||
	    (dmdtype == DMD_615)) {
		/*
		 * sizes are window and viewport dimensions
		 */
		ax = GETX(ap->ar.r.corner.x);
		ay = GETY(ap->ar.r.corner.x);
		if ((ax != layer[lay].cols) ||
		    (ay != layer[lay].rows))
			return ((char *)0);
	} else {
		/*
		 * sizes are in pixels
		 */
		ax = ap->ar.r.corner.x - ap->ar.r.origin.x - border;
		ay = ap->ar.r.corner.y - ap->ar.r.origin.y - border;
		if ((ax != layer[lay].xpixels) ||
		    (ay != layer[lay].ypixels))
			return ((char *)0);
	}
	ap->flags |= IN_PROG;
	if (ap->flags&COMMAND) {
		layer[lay].flags &= ~AUTODELETE;
	}
	return(ap->buf);
}

deleteterm(lay, start)
register int lay;
int start;
{
	register int i;
	register struct aglist *ap1 = AGNIL;
	register struct aglist *ap2 = AGNIL;
	register struct aglist *ap3 = AGNIL;
	extern int dmdtype;
	extern int autocurrent;

	if (layer[lay].flags&DELETING)
		return(0);
	Debug(DEBINFO, "deleteterm layer: %d\n",lay);
	if (autocurrent) {
		for (i = MAXPCHAN-1; i > 0; --i) {
			if ((i != lay) && (layer[i].flags & ACTIVE) &&
			    (layer[i].fd != -1) && !(layer[i].flags & DELETING))
				break;
		}
		if (i == 0) {
			for (i = MAXPCHAN-1; i > 0; --i) {
				if ((i != lay) && (layer[i].flags & ACTIVE) &&
				    !(layer[i].flags & DELETING))
					break;
			}
			if (i == 0)
				goto out;
		}
		if ((ap1 = makeaglist(i, A_TOP)) == AGNIL)
			return(-1);
		if ((ap2 = makeaglist(i, A_CURRENT)) == AGNIL) {
			(void)free(ap1);
			return(-1);
		}
	}
    out:
	if ((ap3 = makeaglist(lay, A_DELETE)) == AGNIL) {
		if (ap1 != AGNIL)
			(void)free(ap1);
		if (ap2 != AGNIL)
			(void)free(ap2);
		return(-1);
	}
	if (ap1 != AGNIL)
		addagent(ap1);
	if (ap2 != AGNIL)
		addagent(ap2);
	addagent(ap3);
	layer[lay].flags |= DELETING;
	if (start)
		startagent();
	return(0);
}
	
/*
 * This function is for making an agent command to send from layers to the
 *  terminal, not agent calls that originate from libwindows programs.
 */
struct aglist *
makeaglist(lay, command)
int lay;
short command;
{
	register struct aglist *ap;

	if ((ap = (struct aglist *)malloc(sizeof(struct aglist))) == AGNIL)
		return(AGNIL);
	bzero(ap, sizeof(struct aglist));
	ap->next = AGNIL;
	ap->state = AG_NEW;
	ap->ar.command = command;
	ap->ar.chan = lay;
#ifdef USE_NAMED_PIPE
	ap->xtfd = -1;
#endif
	return(ap);
}

agentcom(chan, command, x1, y1, x2, y2)
int chan;
short command;
short x1, y1, x2, y2;
{
	struct aglist *ap;

	if ((ap = makeaglist(chan, command)) == AGNIL)
		return(-1);
	if ((ap->buf = malloc(ANLEN*sizeof(char))) == (char *) 0)
		return(-1);
	bzero(ap->buf, ANLEN*sizeof(char));
	ap->ar.r.origin.x = x1;
	ap->ar.r.origin.y = y1;
	ap->ar.r.corner.x = x2;
	ap->ar.r.corner.y = y2;
	addagent(ap);
	startagent();
	return(0);
}

exitagent()
{
#ifdef USE_DIRENT
    	register struct dirent *dp;
#else
	register struct direct *dp;
#endif
	register int n;
	DIR *dirp;
	extern char dmdpath[];

#ifdef USE_PTY_FOR_AGENT
	freepty(-1, agentsock, agentptyname);
	close(agentptyfd);
#else
	close(agentsock);
#endif
	if (asockdir[0] == '\0')
		return;
	if (chdir(asockdir) < 0)
		return;
	if ((dirp = opendir(".")) == NULL)
		return;
	n = strlen(agentname);
	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
#ifdef USE_DIRENT
		if (dp->d_reclen < (unsigned short) n)
#else
		if (dp->d_namlen < n)
#endif
			continue;
		if (strncmp(agentname, dp->d_name, n))
			continue;
		if ( dp->d_name[n] != '\0' && dp->d_name[n] != '.' )
			/* Either not the full name or not followed by dot */
			continue;
		Debug(DEBINFO, "removing %s\n",dp->d_name);
		(void) unlink(dp->d_name);
	}
	closedir(dirp);
	return;
}

/* check for pending agent requests from libwindows users */
checkpendingagent()
{
	int pend_set;
	int i;
	struct aglist *ap;

	pend_set = isdatapending(agent_pend_set);
	if (pend_set == agent_pend_set) {
		/* nothing has changed */
		Debug(DEBDETAIL, "agent pending set still %x\n", pend_set);
		return;
	}

	for(i = 0; i < MAXPCHAN; i++) {
		int fd = layer[i].fd;
		if (fd == -1)
			continue;
		if (ISSET_FD(fd, agent_pend_set) && !ISSET_FD(fd, pend_set)) {
			CLR_FD(fd, agent_pend_set);
			for (ap = aghead; ap != AGNIL; ap = ap->next) {
				if ((ap->reqchan == i) &&
						(ap->state == AG_PEND)) {
					ap->state = AG_NEW;
					Debug(DEBDETAIL,
						"%s channel %d pid %d\n",
					   "clearing pending agent request for",
							i, ap->pid);
				}
			}
		}
	}
	startagent();
}

printagent(msg, ap)
char *msg;
register struct aglist *ap;
{
	char cmdbuf[64];
	extern char *dmd_agentlist[];
	extern char *dmd_agentctrllist[];
	extern int dmd_maxagent;
	extern int DebugLevel;

	if (!(DebugLevel & DEBINFO))
		return;

	if ((ap->ar.command >= A_FIRSTCONTROL) &&
				(ap->ar.command <= A_LASTCONTROL)) {
		strcpy(cmdbuf, dmd_agentctrllist[(ap->ar.command -
							A_FIRSTCONTROL)]);
	}
	else if (ap->ar.command <= dmd_maxagent)
		strcpy(cmdbuf, dmd_agentlist[ap->ar.command]);
	else 
		sprintf(cmdbuf, "%d", ap->ar.command);

	Debug(DEBINFO, "%s ch:%d cmd:%s %d/%d/%d/%d res:%d flg:%d st: %d\n",
		msg, ap->ar.chan, cmdbuf,
		ap->ar.r.origin.x, ap->ar.r.origin.y,
		ap->ar.r.corner.x,ap->ar.r.corner.y,
		ap->result, ap->flags, ap->state);

	if ((ap->buf != (char *)0) && (ap->buf[0] != '\0'))
		Debug(DEBINFO,"\tcommand: %s\n", ap->buf);
}
