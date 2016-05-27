
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
#include <sys/resource.h>
#include <signal.h>
#include <errno.h>
#include "sys/jioctl.h"
#ifdef USE_POLL
#include <stropts.h>
#include <poll.h>
#endif
#include "proto.h"
#include "pconfig.h"
#include "packets.h"
#ifdef NO_TIOCUCNTL
static char rbuf[NETHDRSIZE+MAXNETHIWAT];
#else
static char rbuf[NETHDRSIZE+MAXNETHIWAT+1];
#endif
/* reserve space at the beginning of the buffer for network xt headers */
#define wbuf (&rbuf[NETHDRSIZE])

#ifdef USE_TERMIOS_H
#include <sys/termios.h>
#endif
#ifdef USE_TERMIO_H
#include <sys/termio.h>
#endif
#include <stdio.h>

extern int networkxt;
extern int numpcbufs;

extern int layerdied;
extern int rd_enabled;

extern struct layer layer[];
extern int fdtolayer[];

#ifdef USE_POLL
int
settopoll(set, pollfds)
int set;
struct pollfd *pollfds;
{
	int npollfds, fd, bits;

	npollfds = 0;
	for (fd = 0, bits = 1; fd<MAXFD; fd++, bits <<= 1) {
		if ((bits & set) == 0)
			continue;
		pollfds[npollfds].fd = fd;
		pollfds[npollfds].events = POLLIN;
		npollfds++;
	}
	return(npollfds);
}

int
polltoset(npollfds, pollfds)
int npollfds;
struct pollfd *pollfds;
{
	int set, i;

	ZERO_FD(set);
	for (i = 0; i < npollfds; i++) {
		Debug(DEBDETAIL,"revents for polled fd %d: 0x%x\n",
			pollfds[i].fd, pollfds[i].revents);
		if (pollfds[i].revents & POLLIN) {
			SET_FD(pollfds[i].fd, set);
		}
	}
	return(set);
}
#endif

multiplex()
{
	register int bits, cc, fd, lay, nfd;
	register struct layer *lp;
	int rd_set;
#ifdef USE_POLL
	struct pollfd pollfds[MAXFD];
	int npollfds;
#else
	struct timeval st, pollst;
#endif
	extern int agentsock;
	extern int exiting;
	extern int dmdtype;
	extern int errno;
	extern char *sys_errlist[];
	extern int active;
	extern int deadlayer[];
	extern int agentblock;
	extern int agent_pend_set;
	extern void flush_write();

	ZERO_FD(rd_enabled);
	ZERO_FD(rd_set);
	SET_FD(0, rd_enabled);
	SET_FD(agentsock, rd_enabled);
#ifndef USE_POLL
	st.tv_sec = SELSEC;
	st.tv_usec = SELUSEC;
	pollst.tv_sec = 0;
	pollst.tv_usec = 0;
#endif

	while (1) {
		if (agentblock > 0)
			startagent();
		if (!ISZERO_FD(agent_pend_set))
			checkpendingagent();

#ifdef USE_POLL
		npollfds = settopoll(rd_enabled, pollfds);
		Debug(DEBDETAIL,"multiplex calling timeout=0 poll, rd_set 0x%x\n",
								rd_enabled);
		if ((nfd=poll(pollfds, npollfds, 0)) == 0) {
			flush_write();
			Debug(DEBDETAIL,"no input yet, calling poll with wait\n");
			nfd=poll(pollfds, npollfds,
					(SELSEC * 1000) + (SELUSEC / 1000));
		}
		rd_set = polltoset(npollfds, pollfds);
#else
		rd_set = rd_enabled;
		Debug(DEBDETAIL,"multiplex calling poll select, rd_set 0x%x\n",
					rd_set);
		if ((nfd=select(MAXFD,&rd_set,(int *)0,(int *)0,&pollst)) == 0){
			flush_write();
			Debug(DEBDETAIL,"nothing in poll, calling select\n");
			rd_set = rd_enabled;
			nfd=select(MAXFD,&rd_set,(int *)0,(int *)0,&st);
		}
#endif
		if (nfd <= 0) {
			int sve = errno;
			Debug(DEBDETAIL,"multiplex select <= 0: %d %s\n", nfd, 
					(nfd < 0) ? sys_errlist[sve] : "");
			errno = sve;
			if (nfd == 0) {
				Debug(DEBDETAIL,"multiplex select 0, exiting: %d, Ptflag: %d\n", exiting, Ptflag);
				if (exiting)
					return;
				if (Ptflag)
					ptimeout(SIGALRM);
				errno = 0;
#ifndef USE_POLL
				st.tv_sec = SELSEC;
				st.tv_usec = SELUSEC;
#endif
				continue;
			}
			if (errno == EINTR) {
				errno = 0;
				continue;
			}
			Debug(DEBDETAIL,"multiplex exiting, errno = %d\n", errno);
			return;
		}
		Debug(DEBDETAIL,"multiplex select %d fd's, set 0x%x\n",nfd, rd_set);
		for (fd = 0, bits = 1; (nfd>0)&&(fd<MAXFD); fd++, bits <<= 1) {
			if ((bits & rd_set) == 0)
				continue;
			--nfd;
			Debug(DEBDETAIL,"multiplex selected fd %d\n",fd);
			if (fd == 0) {
				while ((cc = read(fd, rbuf, sizeof(rbuf))) <= 0){
					if ((cc == -1) && (errno == EINTR)) {
						errno = 0;
						continue;
					}
					quit(1);
				}
				precv(rbuf, cc);
				continue;
			}
			if (fd == agentsock) {
				recvhostagent(fd);
				continue;
			}
			if ((lay = fdtolayer[fd]) == -1)
				continue;
			lp = &layer[lay];
#ifdef NO_TIOCUCNTL
			while ((cc = read(fd, wbuf, maxoutsize(lay))) <= 0)
#else
			while ((cc = read(fd, wbuf, maxoutsize(lay)+1)) <= 0)
#endif
				{
				if ((cc == -1) && (errno == EINTR)) {
					errno = 0;
					continue;
				}
				Debug(DEBDETAIL,
					"multiplex eof, cc %d, errno %d\n",
							cc, errno);
				if (lp->flags&AUTODELETE) {
					CLR_FD(fd, rd_enabled);
					if (deleteterm(lay, 1) == -1)
						deletehost(lay);
				} else
					deletehost(lay);
				goto bottom;
			}
#ifndef NO_WINSIZE
			if (lp->flags & SETWINSIZE) {
				extern struct winsize *getwinsize();
				lp->flags &= ~SETWINSIZE;
				(void) ioctl(fd, TIOCSWINSZ, getwinsize(lp));
			}
#endif
#ifdef NO_TIOCUCNTL
			if (networkxt)
				(void) netpsend(lay, wbuf, cc);
			else
				(void) psend(lay, wbuf, cc);
#else
			if (wbuf[0] != 0) {
				if (pconvs[lay].freepkts < numpcbufs) {
					Debug(DEBERROR,
					    "ioctl too soon, freepkts = %d\n",
						pconvs[lay].freepkts);
				}
				(void) sendioctl(lay, wbuf[0]);
			}
			else {
				--cc;
				if (networkxt)
					(void) netpsend(lay, &wbuf[1], cc);
				else
					(void) psend(lay, &wbuf[1], cc);
			}
#endif

			if (maxoutsize(lay) == 0) {
				if (lp->fd != -1) {
					/* disable this channel */
					CLR_FD(lp->fd, rd_enabled);
				}
			}
		bottom:
			continue;
		}

		if (layerdied > 0)
			deadkid(rd_set);
	}
}

int
maxoutsize(lay)
{
    extern int maxpkt;
    extern int highwater;

    if (networkxt) {
	    struct layer *chanp = &layer[lay];
	    if (chanp->flags & NONETFLOW)
		    return(MAXNETHIWAT);

	    return(highwater - chanp->bytesent);
    }
    else {
	    if ((pconvs[lay].freepkts == 0) && (lay != 0))
		    return(0);

	    return(maxpkt);
    }
}

int
isunacked(lay)
int lay;
{
	if (networkxt) {
		/*
		 * Don't always get acks on network xt, even when network
		 *   flow control is enabled.
		 */
		return(0);
	}
	else
		return(pconvs[lay].freepkts < numpcbufs);
}

int
isdatapending(req_set)
int req_set;
{
	extern int errno;
	int unacked_set;
	int lay;
	int nfd;
#ifdef USE_POLL
	struct pollfd pollfds[MAXFD];
	int npollfds;
#else
	struct timeval st;
#endif

	Debug(DEBDETAIL, "isdatapending checking set 0x%x\n", req_set);

	/*
	 * first check if there are outstanding un-acked packets on any
	 *  of the channels.
	 */
	ZERO_FD(unacked_set);
	for(lay = 0; lay < MAXPCHAN; lay++) {
		int fd = layer[lay].fd;
		if (fd == -1)
			continue;
		if (ISSET_FD(fd, req_set) && isunacked(lay))
			SET_FD(fd, unacked_set);
	}

	/* remove the known unacked channels from the request set */
	req_set &= ~unacked_set;
	
	Debug(DEBDETAIL, "unacked set 0x%x, remaining request set 0x%x\n",
					unacked_set, req_set);

	if (ISZERO_FD(req_set))
		return(unacked_set);

	/* Check the remaining fds with a zero-timeout select */

#ifndef USE_POLL
	st.tv_sec = 0;
	st.tv_usec = 0;

	if ((nfd = select(MAXFD, &req_set, (int *)0, (int *)0, &st)) < 0)
#else
	npollfds = settopoll(req_set, pollfds);
	if ((nfd = poll(pollfds, npollfds, 0)) < 0)
#endif
	{
		Debug(DEBERROR,
			"error from isdatapending select, errno = %d\n", errno);
		ZERO_FD(req_set);
	}
	else if (nfd == 0)
		ZERO_FD(req_set);

	Debug(DEBDETAIL, "isdatapending returns set 0x%x\n",
						req_set | unacked_set);

	return(req_set | unacked_set);
}

deadkid(setbits)
int setbits;
{
	register int i;
	register struct layer *lp;
	register int deadcnt = layerdied;
	extern struct layer layer[];
	extern int deadlayer[];
	extern int layerdied;
#ifndef USE_SYSV_SIGNALS
	int oldmask;
	oldmask = sigblock(sigmask(SIGCHLD));
#else
	sighold (SIGCHLD);
#endif
	Debug(DEBINFO, "deadkid(%d), dead count = %d\n", setbits, layerdied);
	for (i = 1; (i < MAXPCHAN) && (deadcnt > 0); i++) {
		if (deadlayer[i] == 0)
			continue;
		lp = &layer[i];
		--deadcnt;
		if ((lp->fd != -1) && (ISSET_FD(lp->fd, setbits)))
			continue;
		--layerdied;
		deadlayer[i] = 0;
		lp->pgrp = -1;
		if (lp->flags&AUTODELETE) {
			CLR_FD(lp->fd, rd_enabled);
			if (deleteterm(i, 1) == -1)
				deletehost(i);
		} else
			deletehost(i);
	}
#ifndef USE_SYSV_SIGNALS
	(void) sigsetmask(oldmask);
#else
	sigrelse (SIGCHLD);
#endif
}

/*
 * Some systems have trouble with calling sleep when alarms are in progress
 *  so use select with a timeout instead and hold alarms during it to keep
 *  it from getting prematurely interrupted.
 */
mysleep(seconds)
int seconds;
{
#ifdef USE_POLL
	struct pollfd pollfds[MAXFD];
	int npollfds;
#else
	struct timeval st;
#endif
	int req_set;
	int ret;
	int saverr;
#ifndef USE_SYSV_SIGNALS
	int oldmask;
	oldmask = sigblock(sigmask(SIGALRM));
#else
	sighold (SIGALRM);
#endif

	ZERO_FD(req_set);

#ifdef USE_POLL
	npollfds = settopoll(req_set, pollfds);
	ret = poll(pollfds, npollfds, seconds * 1000);
#else
	st.tv_sec = seconds;
	st.tv_usec = 0;
    	ret = select(MAXFD, &req_set, (int *)0, (int *)0, &st);
#endif

	saverr = errno;
#ifndef USE_SYSV_SIGNALS
	(void) sigsetmask(oldmask);
#else
	sigrelse (SIGALRM);
#endif
	errno = saverr;
	return(ret);
}
