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
#ifndef USE_SYSV_WAIT
#include <sys/wait.h>
#endif
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#ifdef USE_PTMX
#include <sys/stropts.h>
#endif
#ifdef USE_TERMIOS_H
#include <sys/termios.h>
#endif
#ifdef USE_TERMIO_H
#include <sys/termio.h>
#endif
#if (defined(FAST) && !defined(USE_NICE))
#include <sys/resource.h>
#endif

extern struct layer layer[];
extern int deadlayer[];
extern char shell[];
static char *shellargv[] = {
	shell,
	(char *)0,		/* -c option */
	(char *)0,		/* optional command arg */
	(char *)0		/* terminating null arg */
};

execprocess(tty, wp, cmd, lay)
register char *tty;
struct winsize *wp;
register char *cmd;
int lay;
{
	register int pid;
	int ttyfd;
	char buf[80];
#	ifdef FAST
	extern int oldpriority;
#	endif

	if ((pid = fork()) == 0) {
#		ifdef FAST
#ifdef USE_NICE
		(void) nice( oldpriority );
#else		
		(void) setpriority(PRIO_PROCESS, getpid(), oldpriority);
#endif
#		endif
#		if defined(O_NOCTTY) && defined(TIOCSCTTY)
		(void)setsid();
#		else
#		if defined (NO_TIOCNOTTY)
		(void) setpgrp (getpid(), 0);
#		else
		if ((ttyfd = open("/dev/tty", O_RDWR)) < 0)
			exit(1);
		(void) ioctl(ttyfd, TIOCNOTTY, (char *)0);
		(void) close(ttyfd);
#		endif
#		endif
		(void) close(0);
		(void) close(1);
		(void) close(2);


		if ((ttyfd = open(tty, O_RDWR)) != 0)
			exit(2);
		pushmodules(ttyfd);

#		if defined(O_NOCTTY) && defined(TIOCSCTTY)
		(void)ioctl(ttyfd, TIOCSCTTY, 1);
#		endif
		(void) dup(ttyfd);
		(void) dup(ttyfd);
		slavettyset(ttyfd, tty, wp);
#ifndef USE_SYSV_SIGNALS
		(void) signal(SIGTSTP, SIG_IGN);
		(void) signal(SIGPIPE, SIG_DFL);
#else
#ifdef SIGTSTP
		(void) sigset(SIGTSTP, SIG_IGN);
#endif
		(void) sigset(SIGPIPE, SIG_DFL);
#endif
		sprintf(buf, "%s=%d", CHANENV, lay);
		(void) putenv(buf);
		if ((cmd == (char *)0) || (*cmd == '\0')) {
			shellargv[1] = (char *)0;
			shellargv[2] = (char *)0;
			execvp(shell, shellargv);
		} else {
			shellargv[1] = "-c";
			shellargv[2] = cmd;
			execvp(shell, shellargv);
		}
		exit(3);
	}
	mysleep(1);	/* allow time for new process to open up pty */
	return(pid);
}

void
catchsig(signo)
int signo;
{
	register int pid;
	register int i;
#ifdef USE_SYSV_WAIT
	int status;
#else
	union wait status;
#endif
	extern int layerdied;

	Debug(DEBINFO, "Caught signal %d\n", signo);

	switch (signo) {
	case SIGHUP:
		quit(0);
		break;
	case SIGTERM:
		quit(1);
		break;
	case SIGCHLD:
#ifdef USE_SYSV_WAIT
		if((pid = wait(&status))>0)
#else
		while((pid=wait3(&status,WUNTRACED|WNOHANG,(struct rusage *)0))>0)
#endif
							{
#if !defined(USE_SYSV_SIGNALS) && !defined(USE_SYSV_WAIT)
			if (WIFSTOPPED(status)) {
				Debug(DEBINFO, "stopped pid%d\n",pid);
				(void)kill(pid, SIGCONT);
			} else
#endif
			{
				Debug(DEBINFO,"died %d ret 0x%x core %d\n", pid,
#ifdef USE_SYSV_WAIT
					status,(status & 0x40) != 0);
#else
					status.w_retcode,status.w_coredump);
#endif
				for (i = 1; i < MAXPCHAN; i++) {
					if (layer[i].pgrp != pid) 
						continue;
					layerdied++;
					deadlayer[i] = 1;
					Debug(DEBINFO,"dead lay %d\n",i);
					break;
				}
			}
		}
		break;
	default:
		break;
	}

#ifndef USE_SYSV_SIGNALS
	/* re-set signal handler in case signals are system V compatible */
	/* Not needed if we're actually using USE_SYSV_SIGNALS because then */
	/*   we use sigset which always resets the signal handler */
	(void) signal(signo, catchsig); 
#endif
}

signalprocess(lay, sig)
int lay;
int sig;
{
	register struct layer *lp = &layer[lay];
	int ttypg;

	Debug(DEBINFO, "signalproc lay:%d, sig:%d\n", lay, sig);
	if (lp->pgrp > 0)
#ifdef NO_KILLPG
		(void) kill(-(lp->pgrp), sig);
#else
		(void) killpg(lp->pgrp, sig);
#endif
	if (lp->fd <= 0)
		return;

	/* NOTE: I'm not sure the rest of this function ever actually does
	 *   a kill/killpg because it's trying to get the process group of
	 *   the master side of a pseudo-tty, not the slave -- DWD
	 */
#ifdef TIOCGPGRP
	if (ioctl(lp->fd, TIOCGPGRP, &ttypg) == 0 &&
	    ttypg > 0 && ttypg != lp->pgrp)
#else
	if ((ttypg = tcgetpgrp(lp->fd)) > 0 && ttypg != lp->pgrp)
#endif
#ifdef NO_KILLPG
		(void) kill(-ttypg, sig);
#else
		(void) killpg(ttypg, sig);
#endif
}

deletehost(lay)
int lay;
{
	int fd, i;
	register struct layer *lp = &layer[lay];
	extern int rd_enabled;
	extern int fdtolayer[];

	Debug(DEBINFO, "deletehost layer: %d\n", lay);
	if (lp->fd != -1) {
		fdtolayer[lp->fd] = -1;
		CLR_FD(lp->fd, rd_enabled);
		if (lp->pgrp > 0) {
			/*
			 * NOTE: SIGCHLD should not be held if lp->pgrp > 0
			 *  because then deadlayer[lay] will not get turned on.
			 */
			signalprocess(lay , SIGHUP);
			for (i = 0; i < 5; i++) {
				/*
				 * Allow time for layer to die before freeing
				 *  the pty because on some systems freeing the
				 *  pty causes another SIGHUP which can abort an
				 *  editor such as vi in the process of saving
				 *  changed files.
				 * Any signal will prematurely end the sleep
				 *  including SIGCHLD so it probably will not
				 *  take the full second; if the layer is not
				 *  dead yet after an interrupt retry up to 5
				 *  times (that is, not forever in case 
				 *  something goes wrong).
				 * One second may not be enough on a heavily
				 *  loaded system but I don't want to wait
				 *  any longer because it may be a stuck
				 *  process and if we are deleting several
				 *  windows (such as at exit-time) it could
				 *  add up to some significant wait time.
				 */
				if (deadlayer[lay])
					break;
				Debug(DEBINFO,
				    "layer %d not dead yet, waiting 1 second\n",
					lay);
				if ((mysleep(1) != -1) || (errno != EINTR))
					/* sleep completed */
					break;
				Debug(DEBINFO, "1 second sleep interrupted\n");
			}
			lp->pgrp = -1;
		}
		fd = lp->fd;
		lp->fd = -1;
		freepty(lay, fd, lp->tty);
	} else {
		signalprocess(lay , SIGHUP);
		lp->pgrp = -1;
	}
	lp->bytesent = 0;
}

pushmodules(fd)
int fd;
{
#ifdef USE_PTMX
	(void) ioctl(fd, I_PUSH, "ptem");
	(void) ioctl(fd, I_PUSH, "ldterm");
	(void) ioctl(fd, I_PUSH, "smterm");
	(void) ioctl(fd, I_PUSH, "ttcompat");
#endif

	return(1);
}
