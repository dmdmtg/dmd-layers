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
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#ifdef USE_CLONE_PTY
#include <sys/sysmacros.h>
#endif
#ifdef USE_PTMX
#include <stropts.h>
#include <signal.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#ifndef NO_TERMIO
#ifndef NO_TERMIOS
#include <termios.h>
#else
#include <termio.h>
#endif
#else
#ifdef USE_TERMIOS_H
#include <sys/termios.h>
#endif
#ifdef USE_TERMIO_H
#include <sys/termio.h>
#endif
#endif

#ifndef LIBSYS
#define LIBSYS		"lib/layersys"
#endif

static struct stat statb;
#ifndef NO_TERMIO
#ifndef NO_TERMIOS
struct termios saveterm;
#else
struct termio saveterm;
#endif
#else
static struct sgttyb sgttyb;
static struct tchars tchars;
static struct ltchars ltchars;
static int ldisc, lmode;
#endif
#ifdef USE_ALT_PTYALLOC
static char ptyc[] = "pqrstuvwxyzabcefghijklmno";
#else
static char ptyc[] = "pqrstuvwxyzPQRST";
#endif
static char ptyn[] = "0123456789abcdef";
#ifdef USE_MAXI_PTYALLOC
#define NPTY	0xfff
#else
#define NPTYC	(sizeof(ptyc)-1)
#define NPTYN	(sizeof(ptyn)-1)
#define NPTY	(NPTYC*NPTYN)
#endif

extern int errno;
extern int booted;
extern char dmdpath[];
extern char dmdsyspath[];
extern struct layer layer[];
extern char *strcat();
extern char *strncpy();
extern char *strcpy();
extern long lseek();

static char * layer0ttyname;
static char layer0hostname[MAXPATHLEN];
static int loggedinlay;
static char loggedintty[sizeof(layer[0].tty)];
static int loggedinfd = -1;

savemode(fd)
int fd;
{
	(void) fstat(fd, &statb);
#ifndef NO_TERMIO
#ifndef NO_TERMIOS
	(void) tcgetattr(fd, &saveterm);
#else
	(void) ioctl(fd, TCGETA, &saveterm);
#endif
#else
	(void) ioctl(fd, TIOCGETP, &sgttyb);
	(void) ioctl(fd, TIOCGETC, &tchars);
	(void) ioctl(fd, TIOCGLTC, &ltchars);
	(void) ioctl(fd, TIOCLGET, &lmode);
	(void) ioctl(fd, TIOCGETD, &ldisc);
#endif
}

setraw(fd)
int fd;
{
#ifndef NO_TERMIO
	termio_setcntrl(fd);
#else
	struct sgttyb ttyb;

	ttyb = sgttyb;
	ttyb.sg_flags = RAW | ANYP;
	(void) ioctl(fd, TIOCSETN, &ttyb);
#endif
}

setcntrl(fd, ttyname)
int fd;
char *ttyname;
{
	setraw(fd);

#ifdef TIOCEXCL
	(void) ioctl(fd, TIOCEXCL, (char *)0);
#endif
	if ((statb.st_mode&S_IFMT) == S_IFCHR)
		(void) chmod(ttyname, statb.st_mode&06600);
}

resetcntrl(fd, ttyname)
int fd;
char *ttyname;
{
	mysleep(1);
#ifdef TIOCFLUSH
	(void) ioctl(fd, TIOCFLUSH, FREAD);
#endif
#ifdef TCFLSH
	(void) ioctl(fd, TCFLSH, 0);
#endif
#ifndef NO_TERMIO
	termio_resetcntrl(fd);
#else
	(void) ioctl(fd, TIOCSETP, &sgttyb);
#endif
#ifdef TIOCNXCL
	(void) ioctl(fd, TIOCNXCL, (char *)0);
#endif
	if ((statb.st_mode&S_IFMT) == S_IFCHR)
		(void) chmod(ttyname, statb.st_mode&06777);
}

/*ARGSUSED*/
cntrlspeed(fd)
int fd;
{
#ifndef NO_TERMIO
	return saveterm.c_cflag & CBAUD;
#else
	return (sgttyb.sg_ospeed);
#endif
}

slavettyset(fd, ttyname, wp)
int fd;
char *ttyname;
struct winsize *wp;
{
	if (fd == -1)
		return;
#ifndef NO_TERMIO
#ifndef NO_TERMIOS
	(void) tcsetattr(fd, TCSADRAIN, &saveterm);
#else
	(void) ioctl(fd, TCSETAW, &saveterm);
#endif
#else
	(void) ioctl(fd, TIOCSETD, &ldisc);
	(void) ioctl(fd, TIOCLSET, &lmode);
	(void) ioctl(fd, TIOCSLTC, &ltchars);
	(void) ioctl(fd, TIOCSETC, &tchars);
	(void) ioctl(fd, TIOCSETN, &sgttyb);
#endif
#ifndef NO_WINSIZE
	if (wp != (struct winsize *)0)
		(void) ioctl(fd, TIOCSWINSZ, wp);
#endif
	if ((statb.st_mode&S_IFMT) == S_IFCHR)
		(void) chmod(ttyname, statb.st_mode&06777);
#ifdef TIOCFLUSH
	(void) ioctl(fd, TIOCFLUSH, 0);
#endif
#ifdef TCFLSH
	(void) ioctl(fd, TCFLSH, 0);
#endif
}

findsysfile(buf, file, mode, msg)
char *buf;
char *file;
int mode;
char *msg;
{
	char syspath[MAXPATHLEN];

	sprintf(buf, "%s/%s/%s", dmdpath, LIBSYS, file);
	if (access(buf, mode) != -1)
		return;
	sprintf(buf, "%s/%s/%s", dmdsyspath, LIBSYS, file);
	if (access(buf, mode) != -1)
		return;
	*buf = '\0';

	Debug(DEBINFO,"findsysfile: cannot find sys file %s\n", file);

	if (msg != NULL) {
		fprintf(stderr, "Layers: %s cannot access %s/%s/%s\r\n",
				msg, dmdpath, LIBSYS, file);
		fprintf(stderr, "  or %s/%s/%s\r\n", dmdsyspath, LIBSYS, file);
		fprintf(stderr, "  Perhaps $DMD or $DMDSYS is incorrect.\r\n");
	}
}

char *
callsysprog(argv)
char **argv;
{
	static char answer[BUFSIZ];
	int anslen;
	int n;
	int pipefds[2];
	int rdfd;
	int wrfd;
	int pid;

	Debug(DEBINFO, "callsysprog");
	for (n = 0; argv[n] != 0; n++)
		Debug(DEBINFO, " %s", argv[n]);
	Debug(DEBINFO, "\n");
	
	if (pipe(pipefds) < 0) {
		Debug(DEBERROR, "callsysprog: pipe() call failed\n");
		return(NULL);
	}

	rdfd = pipefds[0];
	wrfd = pipefds[1];

#ifdef NO_VFORK
	if ((pid = fork()) == 0)
#else
	if ((pid = vfork()) == 0)
#endif
	{
		char *msg;
		/* this is the child */
		/* open wrfd for stdin, stdout, and stderr */
		if (wrfd != 0)
			close(0);
		if (wrfd != 1)
			close(1);
		if (wrfd != 2)
			close(2);
		dup(wrfd);
		dup(wrfd);
		dup(wrfd);
		execv(argv[0], argv);

		/* don't use printf() or exit() here because vfork shares
		 * data with parent process
		 */
		msg = "Error execing ";
		write(2, msg, strlen(msg));
		write(2, argv[0], strlen(argv[0]));
		write(2, "\n", 1);
		_exit(errno);
	}

	if (pid == -1) {
		Debug(DEBERROR, "callsysprog: cannot fork\n");
		return(NULL);
	}

	/*
	 * This is the parent.
	 * Don't need to wait for the child.  A signal handler for SIGCLD
	 *   is already in place and will ignore the death of this child.
	 * Just read from it's pipe.
	 */

	close(wrfd);

	anslen = 0;
	while (1) {
		n = read(rdfd, &answer[anslen], BUFSIZ - anslen - 1);
		if (n == -1) {
			if (errno == EINTR)
				continue;
			Debug(DEBERROR, "error %d reading pipe\n", errno);
			break;
		}
		if (n == 0)
			break;
		anslen += n;
	}

	close(rdfd);

	if ((anslen > 0) && (answer[anslen-1] == '\n'))
		--anslen;
	answer[anslen] = '\0';

	Debug(DEBINFO, "answer from %s: \n%s\n", argv[0], answer);

	return(answer);
}

fixptyperms(fd, tty, release)
int fd;
char *tty;
int release;
{
	char path[MAXPATHLEN];

#ifdef USE_PTMX
#ifdef USE_IGNORESIGCHLDGRANTPT
	void (*savesigchld)();
#endif

	if (!release) {
#ifdef USE_IGNORESIGCHLDGRANTPT
		/*
		 * Some versions of grantpt() hold SIGCHLD until after it
		 *   has waited for the child setuid-root process.  Then
		 *   if we don't ignore SIGCHLD signals during grantpt(),
		 *   we get a SIGCHLD and wait forever for the child process
		 *   which has already gone.
		 * I hate to ignore SIGCHLD signals during this because it's
		 *   possible that a process from another window might die
		 *   in the middle and we'd miss it.  It's a race condition.
		 *   The chances are pretty low that that would happen, however,
		 *   and I think it's the least of the evils.  The other
		 *   possibility would be to just forget about grantpt() and
		 *   use our own "grantpt" setuid-root program, but then if
		 *   someone wants to use layers who doesn't have root
		 *   privilege they'd be unable to get ownership of their
		 *   ptys.
		 */
		savesigchld = sigset(SIGCHLD, SIG_DFL);
#endif
		if (grantpt(fd) == -1) {
#ifdef USE_IGNORESIGCHLDGRANTPT
			(void) sigset(SIGCHLD, savesigchld);
#endif
			if (!booted) {
				perror("Layers: warning: grantpt() failed");
				fprintf(stderr, "Won't own pseudo-ttys\n");
			}
		}
#ifdef USE_IGNORESIGCHLDGRANTPT
		(void) sigset(SIGCHLD, savesigchld);
#endif
	}
#else
	char fdbuf[8];
	char *argv[10];
	int argc = 0;
	char *ans;
	static int previousfailure = 0;

	if (previousfailure)
		return;

	findsysfile(path, "grantpt", 01, booted ? NULL : "warning:");
	if (path[0] != '\0') {
		argv[argc++] = path;
		if (release)
			argv[argc++] = "-r";
		sprintf(fdbuf, "%d", fd);
		argv[argc++] = fdbuf;
		argv[argc++] = tty;
		argv[argc++] = 0;
		ans = callsysprog(argv);
		if (!booted) {
			if (ans == NULL) {
				fprintf(stderr,
					"Layers: warning: error executing %s\n",
								path);
				path[0] = '\0';
			}
			else if (*ans != '\0') {
				fprintf(stderr,
					"Layers: warning: error from %s:\n",
								path);
				fprintf(stderr, "   %s\n", ans);
				path[0] = '\0';
			}
		}
	}
	if (path[0] == '\0') {
		previousfailure = 1;
		if (!booted)
			fprintf(stderr, "Won't be able to own pseudo-ttys\n");
	}

#endif
	if (!release)
		chmod(tty, (int)(statb.st_mode&06777));
}

allocpty(lay, tty)
int lay;
register char *tty;
{
	register int fd, n;
#ifdef USE_PTMX
	char ch;
	int ttyfd;
	extern char *ptsname();
#else
#ifdef USE_CLONE_PTY
	struct stat stb, stm;
#else
	static int last;
#endif
#endif

#ifdef USE_PTMX
	Debug(DEBINFO, "opening ptmx\n");
	fd = open("/dev/ptmx", O_RDWR);
	if (fd < 0)
		return(-1);
	unlockpt(fd);
	strcpy(tty, ptsname(fd));

#else
#ifdef USE_CLONE_PTY
	Debug(DEBINFO, "opening /dev/ptc\n");
	fd = open("/dev/ptc", O_RDWR | O_NDELAY);
	if (fd < 0 || fstat(fd, &stb) < 0) {
		if (fd >= 0)
			close(fd);
		Debug(DEBINFO, "/dev/ptc failed, trying /dev/ptcm\n");
		fd = open("/dev/ptcm", O_RDWR | O_NDELAY);
		if (fd < 0 || fstat(fd, &stb) < 0) {
			if ( fd >= 0 )
			 	close(fd);
			Debug(DEBINFO,"/dev/ptcm failed\n");
			return (-1);
		}
 
		for (n = 0; n < 3; n++) {
			char mdevice[256];

			sprintf(mdevice,"%s%d","/dev/ptmc",n);
			if ((stat(mdevice, &stm) == 0) &&
				  (major(stm.st_rdev) == major(stb.st_rdev))) {
				Debug(DEBINFO,"Match on %s\n",mdevice);
				sprintf(tty,"/dev/ttyq%d",
					n*256 + minor(stb.st_rdev));
				break;
			}
		}
		if (n == 3) {
			Debug(DEBINFO,"No match on any /dev/ptmc? device\n");
			return(-1);
		}
	}
	else
		sprintf(tty,"/dev/ttyq%d", minor(stb.st_rdev));

#else
	for (n = 0; n < NPTY; n++) {
#ifdef USE_MAXI_PTYALLOC
		(void) sprintf(tty, "/dev/ptyp%03x", n);
#else
		(void) sprintf(tty, "/dev/pty%c%c", ptyc[last/NPTYN],
				ptyn[last%NPTYN]);
#endif
		if ((fd = open(tty, O_RDWR)) >= 0) {
			tty[sizeof("/dev/")-1] = 't';
			last++;
			break;
		}
		if (errno == ENOENT) {
			n += NPTY - last;
			last = 0;
			continue;
		}
		if (++last >= NPTY) {
			last = 0;
		}
	}
	if (n == NPTY)
		return(-1);

#endif /* USE_CLONE_PTY */
#endif /* USE_PTMX */

	Debug(DEBINFO, "Allocated pseudo-tty %s, master fd %d\n", tty, fd);

	if (!booted && (strlen(tty) > (strlen("/dev/") + ANLEN))) {
		/*
		 * I don't know what to do here.  If the pseudo-ttys are
		 *  any longer then libwindows' openchan() will fail
		 *  because that's all the room we allow for the response.
		 */
		fprintf(stderr, "Layers: pty name %s too long\n", tty);
		return(-1);
	}

	fixptyperms(fd, tty, 0);

#ifdef USE_SETFD
	if (fcntl(fd, F_SETFD, (char*)1) == -1)
#else
	if (ioctl(fd, FIOCLEX, (char *)0) == -1)
#endif
	{
		Debug(DEBERROR,"allocpty(%d): set close-on-exec of %s failed\n",
						lay, tty);
		return(-1);
	}

	return(fd);
}

getpty(lay, tty)
int lay;
register char *tty;
{
	if (loggedinlay == lay) {
		/* already allocated */
		strcpy(tty, loggedintty);
		return(loggedinfd);
	}
	return(allocpty(lay, tty));

}

freepty(lay, fd, tty)
int lay;
int fd;
char *tty;
{
	/* clear close-on-exec flag before calling grantpt */
#ifdef USE_SETFD
	(void) fcntl(fd, F_SETFD, (char*)0);
#else
	(void) ioctl(fd, FIONCLEX, (char *)0);
#endif

	fixptyperms(fd, tty, 1);

#ifdef USE_PTMX
	/*
	 * Flush the streams read queue first.  If we don't do this,
	 *  a process writing data to the slave side can hang and
	 *  be unkillable on some systems.
	 */
	ioctl(fd, I_FLUSH, FLUSHR);
#endif
	(void) close(fd);

	if (loggedinlay == lay) {
		/* need to have a pty open for this layer */
		loggedinfd = -1;
		layer[lay].fd = -1;
		moveutmp(loggedinlay);
	}
}

/*
 * This function is here because some systems have multiple /dev names for
 *  the same pty, and ttyname() (which libwindows may use) might not necessarily
 *  return the same one that we think we have.  If we haven't had a match
 *  before, compare the major and minor numbers if the name doesn't match.
 * This is MUCH faster than opening the device ourselves and calling ttyname()
 *  on it to determine the name.
 */
matchtty(lp, tty)
struct layer *lp;
char *tty;
{
	struct stat stb1, stb2;

	if (strcmp(lp->tty, tty) == 0)
		return(1);

   	if (stat(lp->tty, &stb1) < 0) {
		Debug(DEBERROR, "matchtty: cannot stat %s\n", lp->tty);
		return(0);
	}
   	if (stat(tty, &stb2) < 0) {
		Debug(DEBERROR, "matchtty: cannot stat %s\n", tty);
		return(0);
	}

	if (stb1.st_rdev == stb2.st_rdev) {
		Debug(DEBINFO, "matchtty: switching %s to %s\n", lp->tty, tty);
		strcpy(lp->tty, tty);
		return(1);
	}

	return(0);
}

void
initutmp(tty)
char *tty;
{
	/*
	 * don't copy this to layer[0].tty because only enough room for 
	 *  pseudo-ttys is reserved and who knows how long this one is.
	 */
	layer0ttyname = tty;

	loggedinlay = 0;
}

int
moveutmp(newlay)
int newlay;
{
	char *argv[10];
	char path[MAXPATHLEN];
	char newhost[MAXPATHLEN];
	char *p;
	int argc = 0;
	char *fromtty, *totty;
	int newfd = -1;
	char newtty[sizeof(layer[0].tty)];
	static int previousfailure = 0;

	if (previousfailure || (newlay < 0) || (newlay >= MAXPCHAN))
		return(0);

	if ((newlay == loggedinlay) && (loggedinfd != -1))
		/* already there */
		return(1);

	fromtty = (loggedinlay == 0) ? layer0ttyname : loggedintty;

	if (newlay == 0) {
		/* shutting down */
		totty = layer0ttyname;
		if (loggedinfd != -1) {
			if (layer[loggedinlay].fd == -1)
				freepty(0, loggedinfd, loggedintty);
			loggedinfd = -1;
		}
	}
	else if (layer[newlay].fd != -1) {
		/* new layer is open and thus already has a pty */
		totty = layer[newlay].tty;
		strcpy(newtty, totty);
		newfd = layer[newlay].fd;
	}
	else if ((loggedinlay != 0) && (loggedinlay != newlay) &&
						(layer[loggedinlay].fd == -1)) {
		/*
		 * Moving from one un-allocated layer to another.
		 * Just re-assign the pty to the new layer.
		 */
		loggedinlay = newlay;
		return(1);
	}
	else {
		/*
		 * The window is not open.  Allocate a pty for the layer.
		 * When the window is created, it will use this pty.
		 */
		if ((newfd = allocpty(newlay, newtty)) == -1) {
			if (!booted)
				fprintf(stderr,
					"Layers: failure allocating pty\n");
			Debug(DEBERROR, "moveutmp: failure allocating pty\n");
			return(0);
		}
		totty = newtty;
		if ((newlay == loggedinlay) &&
					(strcmp(newtty, loggedintty) == 0)) {
			/* re-opened the one just closed */
			loggedinfd = newfd;
			return(1);
		}
	}

	findsysfile(path, "movelogin", 01, booted ? NULL : "warning:");
	if (path[0] != '\0') {
		argv[argc++] = path;
		argv[argc++] = "-h";
		if (newlay != 0) {
			if (strncmp("/dev/", layer0ttyname,
							sizeof("/dev/")-1) == 0)
				strcpy(newhost,
					    &layer0ttyname[sizeof("/dev/")-1]);
			else
				strcpy(newhost, layer0ttyname);
			strcat(newhost, "-layers");
			argv[argc++] = newhost;
		}
		else
			argv[argc++] = layer0hostname;

		argv[argc++] = fromtty;
		argv[argc++] = totty;
		argv[argc++] = 0;

		p = callsysprog(argv);
		if ((p == NULL) || (*p == '\0')) {
			if (!booted)
				fprintf(stderr,
					"Layers: warning: error executing %s\n",
								path);
		}
		else if (strncmp(p, "-h ", 3) != 0) {
			if (!booted) {
				fprintf(stderr,
					"Layers: warning: error from %s:\n",
									path);
				fprintf(stderr, "   %s\n", p);
			}
		}
		else {
			/*
			 * Success moving the entry.
			 * A previous host name from the utmp file was returned
			 *   preceded with "-h ".
			 */
			if (loggedinlay == 0) {
				strncpy(layer0hostname, p+3,
						    sizeof(layer0hostname)-1);
				layer0hostname[sizeof(layer0hostname)-1] = '\0';
			}
			if (newlay != 0) {
				loggedinfd = newfd;
				strcpy(loggedintty, newtty);
			}
			loggedinlay = newlay;
			return(1);
		}
	}
	/*
	 * Failed to move the entry
	 */
	previousfailure = 1;
	if (!booted)
		fprintf(stderr, "Won't be able to update who file\n");
	if ((newlay != 0) && (layer[newlay].fd == -1)) {
		/* free the temporarily allocated pty */
		freepty(newlay, newfd, newtty);
	}
	return(0);
}
