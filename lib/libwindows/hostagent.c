/*
 * Combined pseudo-tty and XT-driver libwindows.
 * Combined by David Dykstra at AT&T, 1993.
 * This source code may be freely distributed.
 */

/*
 *	Portions Copyright (c) 1987 Keith Muller
 *	All Rights Reserved
 *
 *	The author make no claims as to the fitness or correctness of
 *	this software for any use whatsoever, and it is provided ``as is''
 *	without express or implied warranty. Any use of this software is
 *	at the user's own risk.
 */

static char *version="@(#) XT+PTY hostagent.c May 16, 1994";

#include <sys/types.h>
#include "hostagent.h"

#include "sys/jioctl.h"

#include <sys/param.h>
#ifndef USE_NAMED_PIPE
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#else
#include <sys/stat.h>
#include <time.h>
#endif
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#ifndef NO_TERMIO
#ifndef NO_TERMIOS
#include <termios.h>
#else
#include <termio.h>
#endif
#endif
#include <fcntl.h>
#ifdef USE_PTEM
#include <sys/stream.h>
#include <sys/ptem.h>
#endif

#ifdef TRY_STREAMS
#include <sys/stropts.h>
#include <sys/fon.h>
#else
/* it's likely to appear in some later version of this OS, at least try it */
#define TRY_STREAMS
#ifndef I_PUSH
#define I_PUSH		(('S'<<8)|02)
#endif
#ifndef I_LOOK
#define I_LOOK		(('S'<<8)|04)
#endif
#ifndef I_FIND
#define I_FIND		(('S'<<8)|013)
#endif

#ifndef FMNAMESZ
/* actually should be 8, but allow room for expansion */
#define FMNAMESZ	256
#endif
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

/*
 * Define my own _IOR and _IOWR because some systems require single quotes
 *  around the letter argument and some require no single quotes.
 */
#ifndef _IOCPARM_MASK
#ifdef IOCPARM_MASK
#define _IOCPARM_MASK IOCPARM_MASK
#else
#define _IOCPARM_MASK 0xff		/* parameters must be < 256 bytes */
#endif
#endif
#ifndef _IOC_VOID
#ifdef IOC_VOID
#define _IOC_VOID IOC_VOID
#else
#define	_IOC_VOID	0x20000000	/* no parameters */
#endif
#endif
#ifndef _IOC_OUT
#ifdef IOC_OUT
#define _IOC_OUT IOC_OUT
#else
#define	_IOC_OUT	0x40000000	/* copy out parameters */
#endif
#endif
#ifndef _IOC_IN
#ifdef IOC_IN
#define _IOC_IN IOC_IN
#else
#define	_IOC_IN		0x80000000	/* copy in parameters */
#endif
#endif
#ifndef _IOC_INOUT
#ifdef IOC_INOUT
#define _IOC_INOUT IOC_INOUT
#else
#define	_IOC_INOUT	(_IOC_IN|_IOC_OUT)
#endif
#endif
#define	_MYIO(x,y) (_IOC_VOID|(x<<8)|y)
#define	_MYIOR(x,y,t) (_IOC_OUT|((((int)sizeof(t))&_IOCPARM_MASK)<<16)|(x<<8)|y)
#define	_MYIOWN(x,y,t) (_IOC_IN|((((int)(t))&_IOCPARM_MASK)<<16)|(x<<8)|y)
#define	_MYIOWR(x,y,t) (_IOC_INOUT|((((int)sizeof(t))&_IOCPARM_MASK)<<16)|(x<<8)|y)

/***********************************************************************
 * Definitions for both XT-driver and pseudo-tty layers
 */
#define TTYNAMELEN 60

#ifdef NO_BCOPY
#define bcopy(B1, B2, LENGTH) memcpy(B2, B1, LENGTH)
#endif
#ifdef NO_INDEX
#define index strchr
#define rindex strrchr
#endif
extern char* sys_errlist[];
extern char *strcpy();
extern char *strncpy();
extern char *strchr();
extern char *getenv();
extern char *index();

#define UNKNOWNLAYERS 0
#define BADLAYERS 1
#define XTDRIVERLAYERS 2
#define PTYLAYERS 3
static int _layerstype = UNKNOWNLAYERS;

static int mychannum = -2;
static int myttyfd = -1;
static char channame[TTYNAMELEN];

static struct answer ans;


static void agenterr();


/***********************************************************************
 * XT-driver layers support
 */

static char myttyname[TTYNAMELEN];
static char cntlttyname[TTYNAMELEN];
static int cntlfd = -1;
static int streamsbased = 0;

union		bbuf	{
		struct	agentrect	ar;
		char	buf[sizeof(struct agentrect)+1];
};
union		bbuf ret;

/*
 * Definitions from XT-driver jioctl.h on different machine types
 */

/* system V style */
#define	XTJTYPE		('j'<<8)
#define	XTJBOOT1	(XTJTYPE|1)  /* start a download in a window */
#define	XTJTERM1	(XTJTYPE|2)  /* return to default terminal emulator */
#define	XTJMPX1		(XTJTYPE|3)  /* currently running layers? */
#define	XTJWINSIZE1	(XTJTYPE|5)  /* inquire window size */
#define	XTJZOMBOOT1	(XTJTYPE|7)  /* JBOOT but wait for debugger to run */
#define XTJAGENT1	(XTJTYPE|9)  /* control for both directions */
#define XTJTRUN1	(XTJTYPE|10) /* send runlayer command to layers cmd */

/* bsd style */
#define XTJBOOT2	_MYIO('j', 1)
#define XTJTERM2	_MYIO('j', 2)
#define XTJMPX2		_MYIO('j', 3)
#define XTJWINSIZE2	_MYIOR('j', 5, struct xtjwinsize)
#define XTJZOMBOOT2	_MYIO('j', 7)
#define XTJAGENT2	_MYIOWR('j', 9, struct xtbagent)
#define XTJTRUN2	_MYIOWN('j', 10, 255)


/*
 * Definitions from pseudo-tty XT on different machine types
 */

#ifdef NO_TIOCUCNTL
/* system V style */

	/* Well, there are no ioctl commands since the SV pseudo-tty
	** implementation (ptlayers) uses named pipes instead of ioctl calls.
	**
	** Use inlayers() in place of JMPX, Jwinsize() in place of 
	** JWINSIZE, Jboot() in plase of JBOOT, Jzomboot() in place of
	** JZOMBOOT, and Jterm() in place of JTERM.
	*/

#else
/* bsd style */

#ifndef UIOCCMD
#define UIOCCMD(n)	_IO(u, n)
#endif

#define	XTJBOOT3	UIOCCMD(1)
#define	XTJTERM3	UIOCCMD(2)
#define	XTJMPX3		UIOCCMD(0)
#define	XTJTIMO3	UIOCCMD(4)	/* Timeouts in seconds */
#define	XTJTIMOM3	UIOCCMD(6)	/* Timeouts in millisecs */
#define	XTJZOMBOOT3	UIOCCMD(7)
#define	XTJAGENT3	UIOCCMD(9)	/* control for both directions */
#define	XTJSMPX3	TIOCUCNTL
#define	XTJWINSIZE3	TIOCGWINSZ

#endif






struct xtbagent {        /* this is supposed to be 12 bytes long */
	long size;      /* size of src string going in and dest string out */
	char * src;	/* address of the source byte string */
	char * dest;	/* address of the destination byte string */
};

struct xtjwinsize
{
	unsigned char	bytesx, bytesy;	/* Window size in characters */
	short	bitsx, bitsy;	/* Window size in bits */
};


/* 
 * End of jioctl.h definitions
 */

static int
xtopenagent()
{
	if (cntlttyname[0] == '\0') {
		agenterr(
		    "openagent: unable to determine control channel from %s",
				myttyname);
		return(-1);
	}
	cntlfd = open(cntlttyname, O_RDONLY);
	return(cntlfd);
}

static int
xtcloseagent()
{
	return(close(cntlfd));
}

/* This function will open the tty file for the indicated channel in the read
 * write mode.
 * RETURNS : the file descriptor for the opened channel.
 *
 * Note that the open is attempted several times on error for the following
 * reason. When a window is deleted, xt sends an M_HANGUP upstream. After the
 * stream head gets this M_HANGUP, it will reject all open attempts until
 * the last process which has that channel open closes the channel.
 * A couple of programs have been encountered which use libwindows to
 * Delete() one window and then very quickly do an openchan() either directly
 * or through Runlayers(). If the openchan is attempted before the
 * process running in the previous window dies, the openchan() will fail.
 *
 * No good solution for this problem is known, and that is why the open
 * is attempted several times. This should fix the problem unless the
 * process traps hangups and is very slow about exiting. Better to
 * fix it most of the time than none of the time.
 */
static int
xtopenchanname(name)
char *name;
{
	int i;
	int retval;

	for(i = 0 ; i < 5 ; ++i) {
		if( (retval = open (name, O_RDWR)) != -1 )
			break;
		sleep(2);
	}

	if (retval == -1)
		return(-1);

#ifdef TRY_STREAMS
	if (streamsbased) {
		if (ioctl(retval, I_FIND, "nldterm") == 0)
			(void) ioctl(retval, I_PUSH, "nldterm");
		else if (ioctl(retval, I_FIND, "ldterm") == 0)
			(void) ioctl(retval, I_PUSH, "ldterm");
	}
#endif

	/*
	 * bug in NXT driver or layers sometimes ignores first write
	 *   or two if previous process died rather than getting
	 *   killed by window deletion.
	 * bug in XT driver on amdahl 2.1.6a prevents either closing of
	 *   the channel or TCSETAW until something's been written.
	 */
	write(retval, " ", 1);
	write(retval, "\r", 1);

	return ( retval );
}

static char *
xtfindchanname(chan)
int chan;
{
	if (cntlttyname[0] == '\0')
		return(NULL);
	strcpy(channame, cntlttyname);
	channame[strlen(channame) - 1] = chan + '0';

	return(channame);
}

static int
xtioctlagent(fd, command, x1, y1, x2, y2, chan)
int	fd;
int	command;
int	x1, y1, x2, y2;
short	chan;
{
	union	bbuf	arbuf;
	static struct 		xtbagent cmd;

	arbuf.ar.command = command;
	arbuf.ar.chan = chan;
	arbuf.ar.r.origin.x = x1;
	arbuf.ar.r.origin.y = y1;
	arbuf.ar.r.corner.x = x2;
	arbuf.ar.r.corner.y = y2;

#ifdef USE_LITTLEENDIAN
	swab(arbuf.buf, arbuf.buf, sizeof(struct agentrect));
#endif	

	cmd.size = sizeof(struct agentrect);
	cmd.src = arbuf.buf;
	cmd.dest = ret.buf;
	if( ioctl(fd, XTJAGENT1, &cmd) != sizeof(struct agentrect) ) {
		if( ioctl(fd, XTJAGENT2, &cmd) != sizeof(struct agentrect) )
			return(-1);
	}

	/* copy un-swabbed data to answerbuf */
	bcopy(((char *) &ret.ar.chan) + sizeof(ret.ar.chan),
						ans.answerbuf, ANLEN);

#ifdef USE_LITTLEENDIAN
	swab(ret.buf, ret.buf, sizeof(struct agentrect));
#endif
	
	ans.result = ret.ar.command;
	ans.chan = ret.ar.chan;

	if(ret.ar.command == (-1))
		return(-1);
	return (0);
}


/***********************************************************************
 * Pseudo-tty layers support
 */

#define MAXPOLLS 10
#define WAITTIME 120
#define NOBUFWAIT 15
#define NOBUFRETRY 10

static char *agenv;
static char *chanenv = 0;
static int agntsock = -1;
static char xtsockpath[MAXPATHLEN];
static char agntsockpath[MAXPATHLEN];
static struct request req;

#ifndef USE_NAMED_PIPE
#ifndef FD_SETSIZE
#define SET_FD(fd, mask)	((mask) |= (1 << (fd)))
static int agntmask;
#else
fd_set agntmask;
#endif
static int descsize;
static struct timeval polltime = {WAITTIME, 0};
static struct sockaddr_un to;
static int tolen;
#else
static int xtsock = -1;		/* descriptor for xtsockpath */
#endif /* USE_NAMED_PIPE */

static int ptyagcleanup();		/* exit agent system */

static int
ptyopenagent()
{
	register char *pt;
	char agdirpath[MAXPATHLEN];
	int selectfd;

	if (agntsock != -1)
		return(agntsock);

#ifndef USE_NAMED_PIPE
	if ((agntsock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
		return(-1);

#ifndef FD_SETSIZE
	if (agntsock > 31)
		return ptyagcleanup();
#endif
#endif /* USE_NAMED_PIPE */
	req.pid = getpid();

	strcpy(req.chanstr, chanenv);

	if (((pt = getenv(AGDIRENV)) != (char *)0) && (*pt != '\0'))
		(void)strncpy(agdirpath, pt, sizeof(agdirpath));
	else if (((pt = getenv("DMD")) != (char *)0) && (*pt != '\0'))
		(void)sprintf(agdirpath, "%s/xt", pt);
	else
		(void)sprintf(agdirpath, "%s/xt", DEFDMD);

	(void)sprintf(agntsockpath, "%s/%s", agdirpath, agenv);
	(void)sprintf(xtsockpath, "%s.%u", agntsockpath, req.pid);
	(void)unlink(xtsockpath);
#ifndef USE_NAMED_PIPE
	to.sun_family = AF_UNIX;
	(void)strcpy(to.sun_path, xtsockpath);
	tolen = strlen(to.sun_path) + 1 + sizeof(to.sun_family);
	if (bind(agntsock, &to, tolen) < 0) {
		agenterr("openagent: bind %s failed: %s", xtsockpath,
			 	sys_errlist[errno]);
		return ptyagcleanup();
	}
#ifndef USE_CHMOD0SOCKET
	(void)chmod(to.sun_path, 0600);
#endif
	(void)strcpy(to.sun_path, agntsockpath);
	tolen = strlen(to.sun_path) + 1 + sizeof(to.sun_family);
	selectfd = agntsock;
#ifdef FD_SETSIZE
	FD_ZERO(&agntmask);
	FD_SET(selectfd, &agntmask);
#else
	SET_FD(selectfd, agntmask);
#endif
	descsize = selectfd + 1;
#else
	if ((agntsock = open (agntsockpath, O_WRONLY, 0)) < 0) {
		agenterr("openagent: open %s failed: %s", agntsockpath,
					sys_errlist[errno]);
		return ptyagcleanup();
	}
	if (mknod (xtsockpath, S_IFIFO | 0700, 0) < 0) {
		agenterr("openagent: mknod %s failed: %s",
			 xtsockpath, sys_errlist[errno]);
		return ptyagcleanup();
	}
	if ((xtsock = open (xtsockpath, O_RDWR, 0)) < 0) {
		agenterr("openagent: open %s failed: %s",
			 xtsockpath, sys_errlist[errno]);
		return ptyagcleanup();
	}
	selectfd = xtsock;
#endif /* USE_NAMED_PIPE */

#ifdef USE_NAMED_PIPE
	(void)fcntl(xtsock, F_SETFD, (char *)1);
#endif
	return(agntsock);
}

static int
ptycloseagent()
{
	if (agntsock == -1)
		return(-1);
	(void) ptyagcleanup();
	return(0);
}

static int
ptyopenchanname(name)
char *name;
{
	int ret;

	ret = open(name, O_RDWR);
	if (ret < 0)
		return(ret);

#ifdef TRY_STREAMS
	if (ioctl(ret, I_FIND, "ldterm") == 0) {
		if (ioctl(ret, I_FIND, "ptem") == 0)
			(void) ioctl(ret, I_PUSH, "ptem");
		(void) ioctl(ret, I_PUSH, "ldterm");
		if (ioctl(ret, I_FIND, "smterm") == 0)
			(void) ioctl(ret, I_PUSH, "smterm");
		if (ioctl(ret, I_FIND, "ttcompat") == 0)
			(void) ioctl(ret, I_PUSH, "ttcompat");
	}
#endif
	return(ret);
}

static char *
ptyfindchanname(chan)
int chan;
{
	int opened = 0;
	if (agntsock == -1) {
		if (openagent() == -1)
			return(NULL);
		else {
			/*
			 * If need to do an openagent for just this call,
			 *  close it again when finished.  It's not obvious
			 *  to the user program that an openagent() is 
			 *  necessary for this call since it doesn't require
			 *  the "cntlfd" parameter like most agent calls,
			 *  so if this is the only agent call a user makes,
			 *  don't need to require them to call closeagent().
			 */
			opened = 1;
		}
	}
	if (ioctlagent(agntsock, A_CHAN, 0, 0, 0, 0, chan) != 0) {
		if (opened)
			(void) closeagent();
		return(NULL);
	}
	if (opened)
		(void) closeagent();

	strcpy(channame,"/dev/");
	strncpy(&channame[sizeof("/dev/")-1], ans.answerbuf, ANLEN);
	channame[sizeof("/dev/")-1+ANLEN] = '\0';

	return(channame);
}

static int
ptyioctlagent(fd, command, x1, y1, x2, y2, chan)
int fd;
int command;
int x1, y1, x2, y2;
short chan;
{
	register int n;
	register int state = 1;
#ifndef USE_NAMED_PIPE
#ifdef FD_SETSIZE
	fd_set rdfd;
#else
	int rdfd;
#endif
	int xmit;
	struct timeval now;		/* time (secs) value */
	struct timezone zone;		/* timezone value (unused) */
#else
	int oldalarmval;
#endif
	int fromlen = 0;
	int pollcount = 0;
	extern int errno;

	if ((fd != agntsock) || (agntsock == -1))
		return(-1);

	req.ar.r.origin.x = x1;
	req.ar.r.origin.y = y1;
	req.ar.r.corner.x = x2;
	req.ar.r.corner.y = y2;
	req.ar.chan = chan;
	req.ar.command = command;
#ifndef USE_NAMED_PIPE
	if (gettimeofday(&now, &zone) < 0)
		return ptyagcleanup();
	req.time = now.tv_sec;
	xmit = 0;
	while (sendto(agntsock, &req, sizeof(req), 0, &to, tolen) < 0) {
		if ((errno == ENOBUFS) && (++xmit < NOBUFRETRY)) {
			sleep(NOBUFWAIT);
			continue;
		}
		return ptyagcleanup();
	}
#else
	req.time = (long) time(0);
	errno = 0;
	if (write (agntsock, &req, sizeof(req)) < sizeof(req)) {
		agenterr("ioctlagent: write to %s failed: %s",
			 agntsockpath, sys_errlist[errno]);
		return ptyagcleanup();
	}
#endif /* USE_NAMED_PIPE */		
	while(state){
#ifndef USE_NAMED_PIPE
		rdfd = agntmask;
		n = select(descsize,&rdfd,(int *)0,(int *)0,&polltime);
#else
		oldalarmval = alarm(WAITTIME);
		n = read(xtsock, &ans, sizeof(ans));
		alarm(oldalarmval);
#endif

		if (((n < 0) && (errno != EINTR)) ||
		     ((n <= 0) && (pollcount > MAXPOLLS))){
			return ptyagcleanup();
		}
		if (n <= 0) {
			errno = 0;
			pollcount++;
			continue;
		}

#ifndef USE_NAMED_PIPE
		n = recvfrom(agntsock, &ans, sizeof(ans), 0, 
				  (struct sockaddr *)0, &fromlen);
		if (n <= 0) {
			errno = 0;
			pollcount++;
			continue;
		}
#endif
		if (req.time != ans.time)
			continue;
		switch(ans.command) {
			case A_RECV:
				continue;
			case A_DONE:
				state = 0;
				continue;
			default:
				break;
		}
#ifndef USE_NAMED_PIPE
		xmit = 0;
		while (sendto(agntsock, &req, sizeof(req), 0, &to, tolen) < 0) {
			if ((errno == ENOBUFS) && (++xmit < NOBUFRETRY)) {
				sleep(NOBUFWAIT);
				continue;
			}
			return ptyagcleanup();
		}
#else
		errno = 0;
		if (write (agntsock, &req, sizeof(req)) < sizeof(req)) {
			agenterr("ioctlagent: write to %s failed: %s",
				 agntsockpath, sys_errlist[errno]);
			return ptyagcleanup();
		}
#endif /* USE_NAMED_PIPE */
	}
	req.ar.command = A_DONE;
#ifndef USE_NAMED_PIPE
	xmit = 0;
	while (sendto(agntsock, &req, sizeof(req), 0, &to, tolen) < 0) {
		if ((errno == ENOBUFS) && (++xmit < NOBUFRETRY)) {
			sleep(NOBUFWAIT);
			continue;
		}
		return ptyagcleanup();
	}
#else
	errno = 0;
	if (write (agntsock, &req, sizeof(req)) < sizeof(req)) {
		agenterr("ioctlagent: write to %s failed: %s",
			 agntsockpath, sys_errlist[errno]);
		return ptyagcleanup();
	}
#endif /* USE_NAMED_PIPE */

	if (ans.result == 0)
		return(0);

	return(-1);
}

/* Prepare to exit agent system.  Always return -1 as a convenience */
static int
ptyagcleanup()
{
	if (agntsock >= 0)
		close(agntsock);
#ifdef USE_NAMED_PIPE
	if (xtsock >= 0)
		close(xtsock);
	xtsock = -1;
#endif
	if (*xtsockpath)
		unlink(xtsockpath);
	agntsock = -1;
	return -1;
}

/***********************************************************************
 * layers-independent routines
 */

#ifndef NO_TTYNAMECOMPAT
extern char* ttyname();
#define getttyname(FD)	ttyname(FD)
#else
/* read tty from a popen("tty") because ttyname() isn't binary compatible */
static char *
getttyname(fd)
int fd;
{
	FILE *fp, *popen();
	static char buf[TTYNAMELEN+10];
	int closefd = 0;

	if (fd == 1) {
		/* popen will close stdout */
		fd = dup(fd);
		closefd = 1;
	}
	sprintf(buf, "tty <&%d", fd);
	if ((fp = popen(buf, "r")) == NULL) {
		if (closefd)
			close(fd);
		return(NULL);
	}
	if (closefd)
		close(fd);

	fgets(buf, sizeof(buf)-1, fp);
	buf[sizeof(buf)-1] = '\0';

	while(getc(fp) != EOF)
		/* read any more to avoid a "Broken Pipe" */
		;
	pclose(fp);

	if (buf[0] == '/') {
		char *p = &buf[strlen(buf)-1];
		if (*p == '\n')
			/* get rid of trailing newline */
			*p = '\0';
		return(buf);
	}

	return(NULL);
}
#endif

static void
determinettyfd()
{
	for (myttyfd = 0; myttyfd <= 2; myttyfd++) {
		if (isatty(myttyfd))
			break;
	}
	if (myttyfd > 2)
		myttyfd = open("/dev/tty", O_RDWR);
}

static int
determinelayerstype()
{
#ifdef TRY_STREAMS
	char lookbuf[FMNAMESZ+1];
#endif
	if (myttyfd == -1)
		determinettyfd();

	if (((agenv = getenv(AGENV)) != (char *)0) && (*agenv != '\0'))
		return (_layerstype = PTYLAYERS);

	if ((myttyfd != -1) && ((ioctl(myttyfd, XTJMPX1, 0) != -1) ||
				(ioctl(myttyfd, XTJMPX2, 0) != -1))) {
#ifdef TRY_STREAMS
		if (ioctl(myttyfd, I_LOOK, lookbuf) != -1)
			streamsbased = 1;
#endif
		return(_layerstype = XTDRIVERLAYERS);
	}

	return(_layerstype = BADLAYERS);
}

#define layerstype()	\
	((_layerstype == UNKNOWNLAYERS) ? determinelayerstype() : _layerstype)

static void
determinechannum()
{
	char *p, *tty;
	extern int atoi();

	if (mychannum > -2)
		return;

	mychannum = -1;
	switch(layerstype())
	{
	case PTYLAYERS:
		if (((chanenv = getenv(CHANENV)) != (char *)0) &&
							(*chanenv != '\0')) {
			mychannum = atoi(chanenv);
		}
		else {
			/* very old pty layers that doesn't have $DMDCHAN */
			chanenv = "";
		}
		break;
	case XTDRIVERLAYERS:
		if (((tty = getttyname(myttyfd)) != NULL) &&
			((p = index(tty, 'x')) != NULL) && (*(p+1) == 't')) {
			strcpy(cntlttyname, tty);
			cntlttyname[strlen(cntlttyname)-1] = '0';
			mychannum = tty[strlen(tty)-1] - '0';
		}
		else {
			/* Can't figure out control channel! This can happen */
			/*   on some systems if we're using /dev/tty.        */
			cntlttyname[0] = '\0';
		}
		if (tty != NULL)
			strcpy(myttyname, tty);
		else
			myttyname[0] = '\0';
		break;
	}
}

int
inlayers()
{
	switch(layerstype())
	{
	case XTDRIVERLAYERS:
		if (streamsbased)
			return(2);
		return(1);
	case PTYLAYERS:
		return(3);
	default:
		return(0);
	}
}

int
onloginlayer()
{
	FILE *fp, *popen();
	static char buf[40];

	if (!inlayers())
		return(0);
	
	if ((layerstype() == PTYLAYERS) && (getenv(AGDIRENV) == 0))
		/* old pty-layers showed all lines logged in */
		return(0);
	
	if (myttyname[0] == '\0') {
		char *tty;
		if ((tty = getttyname(myttyfd)) != NULL) 
		        strcpy(myttyname, tty);
	}
	if (strncmp(myttyname, "/dev/", 5) == 0) {
		/*
		 * Can't use 'who am i' because Solaris responds non-empty
		 *  to that even on non-logged-in lines.
		 */
		sprintf(buf, "who|awk '{if ($2 == \"%s\") print $2}'",
					&myttyname[5]);
	}
	else
		/* couldn't get tty name */
		return(0);

	if ((fp = popen(buf, "r")) == NULL) {
		return(0);
	}

	if (getc(fp) == EOF) {
		/* no output, not on logged-in line */
		pclose(fp);
		return(0);
	}

	while(getc(fp) != EOF)
		/* read any more to avoid a "Broken Pipe" */
		;
	pclose(fp);

	/* there was output from "who" command */

	return(1);
}

int
openagent()
{
	determinechannum();

	switch(layerstype())
	{
	case XTDRIVERLAYERS:
		return(xtopenagent());
	case PTYLAYERS:
		return(ptyopenagent());
	default:
		return(-1);
	}
}

int
closeagent()
{
	if (myttyfd > 2) {
		/* close /dev/tty */
		close(myttyfd);
		myttyfd = -1;
	}
	switch(layerstype())
	{
	case XTDRIVERLAYERS:
		return(xtcloseagent());
	case PTYLAYERS:
		return(ptycloseagent());
	default:
		return(-1);
	}
}

int
getchan()
{
	determinechannum();

	return(mychannum);
}

char *
findchanname(chan)
int chan;
{
	determinechannum();

	switch(layerstype())
	{
	case XTDRIVERLAYERS:
		return(xtfindchanname(chan));
	case PTYLAYERS:
		return(ptyfindchanname(chan));
	default:
		return(NULL);
	}
}

int 
openchanname(name)
char *name;
{
	switch(layerstype())
	{
	case XTDRIVERLAYERS:
		return(xtopenchanname(name));
	case PTYLAYERS:
		return(ptyopenchanname(name));
	default:
		return(-1);
	}
}

int 
openchan(chan)
int chan;
{
	char *name;

	if ((name = findchanname(chan)) == NULL)
		return(-1);
	return(openchanname(name));
}

int
openrwchan(chan)
int chan;
{
	/* For backward compatibility on pty layers */
	return(openchan(chan));
}

int
ioctlagent(fd, command, x1, y1, x2, y2, chan)
int fd;
int command;
int x1, y1, x2, y2;
short chan;
{
	switch(layerstype())
	{
	case XTDRIVERLAYERS:
		return(xtioctlagent(fd, command, x1, y1, x2, y2, chan));
	case PTYLAYERS:
		return(ptyioctlagent(fd, command, x1, y1, x2, y2, chan));
	default:
		return(-1);
	}
}

int
getagentansresult()
{
    	return(ans.result);
}

int
getagentanschan()
{
    	return(ans.chan);
}

char *
getagentanswer()
{
    	return(ans.answerbuf);
}

int
Newlayer(fd, x1, y1, x2, y2)
int fd;
int x1, y1, x2, y2;
{
	if (ioctlagent(fd, A_NEWLAYER, x1, y1, x2, y2, 0))
		return(-1);
	return(ans.chan);
}

int
New(fd, x1, y1, x2, y2)
int fd;
int x1, y1, x2, y2;
{
	if (ioctlagent(fd, A_NEW, x1, y1, x2, y2, 0))
		return(-1);
	return(ans.chan);
}

int
Current(fd, chan)
int fd;
int chan;
{
	return (ioctlagent(fd, A_CURRENT, 0, 0, 0, 0, chan));
}

int
Delete(fd, chan)
int fd;
int chan;
{
	return (ioctlagent(fd, A_DELETE, 0, 0, 0, 0, chan));
}

int
Top(fd, chan)
int fd;
int chan;
{
	return (ioctlagent(fd, A_TOP, 0, 0, 0, 0, chan));
}

int
Bottom(fd, chan)
int fd;
int chan;
{
	return (ioctlagent(fd, A_BOTTOM, 0, 0, 0, 0, chan));
}

int
Move(fd, chan, x1, y1)
int fd;
int chan;
int x1, y1;
{
	return (ioctlagent(fd, A_MOVE, x1, y1, 0, 0, chan));
}

int
Reshape(fd, chan, x1, y1, x2, y2)
int fd;
int chan;
int x1, y1, x2, y2;
{
	return (ioctlagent(fd, A_RESHAPE, x1, y1, x2, y2, chan));
}

int
Romversion(fd, string, len)
int fd;
char string[];
int len;
{
	int res;

	if ((string == (char *)0) || (len <= 0))
		return(-1);
	if (res = ioctlagent(fd, A_ROMVERSION, 0, 0, 0, 0, 0))
		return(-1);
	(void)strncpy(string, ans.answerbuf, len);
	return(res);
}

int
Exit(fd)
int fd;
{
	int res;

	if (res = ioctlagent(fd, A_EXIT, 0, 0, 0, 0, 0))
		return(-1);
	(void) closeagent();
	return(res);
}

int
Jioctlagent(agcall, xtequiv1, xtequiv2, ptyequiv, arg)
int agcall;
int xtequiv1, xtequiv2;
int ptyequiv;
int arg;
{
	int retval = -1;
	int wasopen;

	switch(layerstype())
	{
	case XTDRIVERLAYERS:
		if ((retval = ioctl(myttyfd, xtequiv1, arg)) == -1)
			retval = ioctl(myttyfd, xtequiv2, arg);
		break;
	case PTYLAYERS:
		/* open/close agent if it wasn't opened */
		if (agntsock == -1) {
			wasopen = 0;
			(void) openagent();
		}
		else
			wasopen = 1;

		retval = ioctlagent(agntsock, agcall, arg, arg >> 16, 0, 0, 0);
		if (!wasopen)
			closeagent();

		if ((retval == -1) && (ptyequiv != 0)) {
			/*
			 * Try old equivalent if fail; may be old pty layers.
			 * Even worse, some turn on high bit of command byte,
			 * try that too; even though some accept both it 
			 *  shouldn't hurt to get the command twice.
			 * Even worse, if two of these ioctls come right
			 *  after each other on a sun, the second one is
			 *  likely to override the first in the pty driver.
			 *  Need to put in a sleep to prevent that.
			 */
			retval = ioctl(myttyfd, ptyequiv | 0x80, arg);
			sleep(1);
			retval = ioctl(myttyfd, ptyequiv, arg);
		}
		break;
	}

	return(retval);
}

int
Jboot()
{
#ifdef XTJBOOT3
	return (Jioctlagent(A_JBOOT, XTJBOOT1, XTJBOOT2, XTJBOOT3, 0));
#else
	return (Jioctlagent(A_JBOOT, XTJBOOT1, XTJBOOT2, 0, 0));
#endif
}

int
Jzomboot()
{
#ifdef XTJZOMBOOT3
	return (Jioctlagent(A_JZOMBOOT, XTJZOMBOOT1, XTJZOMBOOT2, XTJZOMBOOT3, 0));
#else
	return (Jioctlagent(A_JZOMBOOT, XTJZOMBOOT1, XTJZOMBOOT2, 0, 0));
#endif
}

int
Jterm()
{
#ifdef XTJTERM3
	return (Jioctlagent(A_JTERM, XTJTERM1, XTJTERM2, XTJTERM3, 0));
#else
	return (Jioctlagent(A_JTERM, XTJTERM1, XTJTERM2, 0, 0));
#endif
}

#ifndef NO_WINSIZE
#if (!defined(TIOCGWINSZ) || defined(USE_PRIVATEWINSIZE))
struct winsize {
    	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};
#endif
#endif

static int tiocwintry[] = {
#ifdef TIOCGWINSZ
	TIOCGWINSZ,
#endif
	(('T' << 8)|104),
	_MYIOR('t', 104, struct winsize),
};

static int xtwintry[] = {
	XTJWINSIZE1,
	XTJWINSIZE2,
};

int
Jwinsize(bytesxp, bytesyp, bitsxp, bitsyp)
int *bytesxp, *bytesyp, *bitsxp, *bitsyp;
{
	int i;
	struct winsize win;
	struct xtjwinsize jwin;

	if (myttyfd == -1)
		determinettyfd();

	for (i = 0; i < sizeof(tiocwintry)/sizeof(tiocwintry[0]); i++) {
		win.ws_row = 0;
		win.ws_col = 0;
		win.ws_xpixel = 0;
		win.ws_ypixel = 0;
		if (ioctl(myttyfd, tiocwintry[i], &win) != -1) {
			if (bytesxp)
				*bytesxp = win.ws_col;
			if (bytesyp)
				*bytesyp = win.ws_row;
			if (bitsxp)
				*bitsxp = win.ws_xpixel;
			if (bitsyp)
				*bitsyp = win.ws_ypixel;
			if ((win.ws_row != 0) && (win.ws_col != 0))
				return(0);
		}
	}

	for (i = 0; i < sizeof(xtwintry)/sizeof(xtwintry[0]); i++) {
		jwin.bytesx = 0;
		jwin.bytesy = 0;
		jwin.bitsx = 0;
		jwin.bitsy = 0;
		if (ioctl(myttyfd, xtwintry[i], &jwin) != -1) {
			if (bytesxp)
				*bytesxp = jwin.bytesx;
			if (bytesyp)
				*bytesyp = jwin.bytesy;
			if (bitsxp)
				*bitsxp = jwin.bitsx;
			if (bitsyp)
				*bitsyp = jwin.bitsy;
			if ((jwin.bytesx != 0) && (jwin.bytesy != 0))
				return(0);
		}
	}
	
	return(-1);
}

/* This command will result in a C_RUN command from the NXT driver.   	*/
/* Layers will then exec a shell on that window to handle the supplied	*/
/* command. Please note that there is no interaction with the terminal	*/
/* for this command.							*/ 

int
Runremotelayer(chan, command)
int	chan;
char	*command;
{

	int fdc;
	char cb[256];
	int rv;

	if ((layerstype() != XTDRIVERLAYERS) || !streamsbased)
		return(-1);

	if ( (fdc = openchan(chan)) == -1 ) 
		return(-1);
	if ( strlen(command) > sizeof(cb)-2 ) 
		return(-1);
	
	cb[0] = (char )chan;
	cb[1] = '\0';
	strcat(cb,command);
	if ((rv = ioctl(fdc,XTJTRUN1,cb)) == -1)
		rv = ioctl(fdc,XTJTRUN2,cb);
	close(fdc);
	return(rv);
}

static void
childexit(msg1, msg2)
char *msg1, *msg2;
{
	/*
	 * It's better not to use stdio in forked child because who knows
	 *   what's in the buffers that hasn't been flushed.
	 */

	char buf[1024];
	int e = errno;

	buf[0] = '\0';

	if (msg1) 
		strcat(buf, msg1);
	if (msg2) {
		strcat(buf, " ");
		strcat(buf, msg2);
	}

	if (e != 0) {
		strcat(buf, ": ");
		strcat(buf, sys_errlist[e]);
	}
	strcat(buf, "\n");

	write(2, buf, strlen(buf));

	sleep(3); /* give time for message to go out over xt channel */

	_exit(3);
}

#ifndef NO_TERMIO
#ifndef NO_TERMIOS
static struct termios saveterm;
#else
static struct termio saveterm;
#endif
#else
static struct sgttyb sgttyb;
static struct tchars tchars;
static struct ltchars ltchars;
static int ldisc, lmode;
#endif

static int
getagentnonraw(fd)
int fd;
{
#ifndef NO_TERMIO
#ifndef NO_TERMIOS
	if (tcgetattr(fd, &saveterm) == -1)
		return(-1);
#else
	if (ioctl(fd, TCGETA, &saveterm) == -1)
		return(-1);
#endif
	/*
	 * Reset some settings which may have been changed by ksh's
	 *   raw mode if we were run in the background with an &.
	 */
	saveterm.c_lflag |= ECHO | ICANON;
	saveterm.c_iflag |= IGNPAR | ICRNL;
	saveterm.c_cc[VEOF] = CEOF;
#else
	if (ioctl(fd, TIOCGETP, &sgttyb) == -1)
		return(-1);
	(void) ioctl(fd, TIOCGETC, &tchars);
	(void) ioctl(fd, TIOCGLTC, &ltchars);
	(void) ioctl(fd, TIOCLGET, &lmode);
	(void) ioctl(fd, TIOCGETD, &ldisc);
#endif
	return(0);
}

int
defagentnonrawf(fd)
int fd;
{
#ifndef NO_TERMIO
#ifndef NO_TERMIOS
	return(tcsetattr(fd, TCSADRAIN, &saveterm));
#else
	return(ioctl(fd, TCSETAW, &saveterm));
#endif
#else
	(void) ioctl(fd, TIOCSETD, &ldisc);
	(void) ioctl(fd, TIOCLSET, &lmode);
	(void) ioctl(fd, TIOCSLTC, &ltchars);
	(void) ioctl(fd, TIOCSETC, &tchars);
	return(ioctl(fd, TIOCSETN, &sgttyb));
#endif
}

int (*anonrawf)() = defagentnonrawf;

void
setagentnonrawf(nonrawf)
int (*nonrawf)();
{
	/*
	 * User programs call this to define their own function
	 *  which returns will set the passed-in file descriptor
	 *  to non-raw mode and which returns -1 for error.
	 *  Useful for programs which reset the terminal to raw
	 *  mode before calling Runlayer().
	 */
	anonrawf = nonrawf;
}

int
Runlayer(channum, command)
int channum;
char *command;
{
	char *shell;
	int i;
	int pid;
	int numcloserrs;
	char chanputenv[sizeof(CHANENV)+sizeof("=NN")];
	char *name;

	if (anonrawf == defagentnonrawf) {
		if (myttyfd == -1)
			determinettyfd();
		
		if (myttyfd == -1) {
			agenterr("Runlayer: cannot find an open tty");
			return(NULL);
		}

		/* find out old tty settings */
		if (getagentnonraw(myttyfd) == -1)
		{
			agenterr("Runlayer: cannot get tty settings from fd %d: %s",
					myttyfd, sys_errlist[errno]);
			return(NULL);
		}
	}

	/*
	 * Need to find the channel name in the parent process on pseudo-tty
	 *  layers to get proper syncronization of the messages back
	 *  from layers.  The actual opening of the device, however,
	 *  must wait for the child process to get "/dev/tty" to work
	 *  properly for the child process.
	 */
	name = findchanname(channum);
	if (name == NULL) {
		agenterr("Runlayer: cannot findchanname(%d)%s", channum,
			layerstype() == PTYLAYERS ? 
				"; you probably need to upgrade layers" : "");
		return(-1);
	}

	if (((shell = getenv("SHELL")) == 0) || (shell[0] == '\0'))
		shell = "/bin/sh";

	if ((pid = fork()) == 0)
	{
		/* this is the child */

		if (signal(SIGINT, SIG_DFL) == SIG_IGN)
			signal(SIGINT, SIG_IGN);
		if (signal(SIGQUIT, SIG_DFL) == SIG_IGN)
			signal(SIGQUIT, SIG_IGN);
		if (signal(SIGHUP, SIG_DFL) == SIG_IGN)
			signal(SIGHUP, SIG_IGN);
		if (signal(SIGTERM, SIG_DFL) == SIG_IGN)
			signal(SIGTERM, SIG_IGN);

		close(0);

#if 0
/* this workaround doesn't work on amdahl 2.1.6a */
/* Instead use the workaround of a writing a character to the channel */
/*   which is implemented in xtopenchanname(). */
		if ((layerstype() == XTDRIVERLAYERS) && !streamsbased) {
			/*
			 * On character XT layers need to open the channel
			 *  twice because at least on amdahls the first time
			 *  it is opened TCSETAW hangs until some data is
			 *  transferred through the channel.  (An alternate
			 *  work around would be to write a character to the
			 *  channel but this workaround has no visible effects).
			 * Needs to be before setpgrp() or /dev/tty will not
			 *  work in the window.
			 */
			if (openchanname(name) != 0)
				childexit("Runlayer: error first opening",name);
			close(0);
		}
#endif

#ifdef TIOCSCTTY
		(void)setsid();		/* set new session id */

		(void)ioctl(0, TIOCSCTTY, 1);  /* set controlling terminal */
#else
		(void)setpgrp(getpid(), 0);	/* set new process group */
#endif

		if (openchanname(name) != 0)
			childexit("Runlayer: error opening", name);

		/* close all other open file descriptor */
		numcloserrs = 0;
		for (i = 1; ; i++) {
			if (close(i) == -1) {
				if (++numcloserrs >= 10)
					break;
			}
			else
				numcloserrs = 0;
		}

		dup(0); /* this opens stdout */
		dup(0); /* this opens stderr */

		if (layerstype() == PTYLAYERS) {
			/*
			 * set DMDCHAN environment variable to the new channel
			 *   or the child will think it belongs to the window
			 *   that the 'agent' command is running in.
			 */
			sprintf(chanputenv,"%s=%d", CHANENV, channum);
			putenv(chanputenv);
		}


		if ((*anonrawf)(0) == -1)
			childexit("Runlayer: cannot set tty settings", 0);

		execl(shell, shell, "-c", command, 0);

		childexit("Runlayer: error execing", shell);
	}

	/* this is the parent */
	if (pid == -1)
	{
		agenterr("Runlayer: cannot fork: %s", sys_errlist[errno]);
		return(-1);
	}

	if (layerstype() == PTYLAYERS) {
		/*
		 * Notify layers of the new owner process for the channel.
		 * This will enable layers to kill that process when the
		 *   window is deleted.  This is not strictly necessary,
		 *   because the pty driver should give the process a SIGHUP
		 *   when the pty shuts down, but this is done for double
		 *   protection.
		 */
		(void) ptyioctlagent(agntsock, A_CHANGEPROC, pid, pid >> 16,
						0, 0, channum);
	}

	return(pid);
}

static int childpid = 0;

#ifdef USE_INT_SIG
int (*savesigint)();
int (*savesigquit)();
int (*savesighup)();
int (*savesigterm)();
#else
void (*savesigint)();
void (*savesigquit)();
void (*savesighup)();
void (*savesigterm)();
#endif

static void
catchsig(sig)
int sig;
{
#ifdef USE_INT_SIG
	int (*savedsig)();
#else
	void (*savedsig)();
#endif
	if (childpid > 0) {
		/*
		 * Kill the child's whole process group
		 * Send hangup even if it's another signal because we
		 *   really want the child to die if we die.
		 */
		kill(-childpid, SIGHUP);
	}
	switch(sig)
	{
	case SIGINT:
		savedsig = savesigint;
		break;
	case SIGQUIT:
		savedsig = savesigquit;
		break;
	case SIGHUP:
		savedsig = savesighup;
		break;
	case SIGTERM:
		savedsig = savesigterm;
		break;
	default:
		savedsig = SIG_DFL;
	}
	if (savedsig == SIG_DFL)
		exit(sig);
	else
		(*savedsig)(sig);
}

static void
restoresigs()
{
	(void) signal(SIGINT, savesigint);
	(void) signal(SIGQUIT, savesigint);
	(void) signal(SIGHUP, savesigint);
	(void) signal(SIGTERM, savesigint);
}

int
Runwaitlayer(channum, command)
int	channum;
char	*command;
{
	int rpid, status;

	if ((savesigint = signal(SIGINT, catchsig)) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
	if ((savesigquit = signal(SIGQUIT, catchsig)) == SIG_IGN)
		signal(SIGQUIT, SIG_IGN);
	if ((savesighup = signal(SIGHUP, catchsig)) == SIG_IGN)
		signal(SIGHUP, SIG_IGN);
	if ((savesigterm = signal(SIGTERM, catchsig)) == SIG_IGN)
		signal(SIGTERM, SIG_IGN);

	childpid = Runlayer(channum, command);

	if (childpid == -1) {
		restoresigs();
		return(-1);
	}

	status = 0;
	while (((rpid = wait(&status)) != childpid) && (rpid != -1))
		;
	
	if (rpid == -1)
	{
		agenterr("Runwaitlayer: waiting for %d failed: %s",
				 childpid, sys_errlist[errno]);
		restoresigs();
		return(-1);
	}

	restoresigs();

	if (status != 0)
	{
		if ((status & 0xff) == 0)
			/* child process exit code */
			return(status >> 8);
		else
			/* child process terminated by a signal */
			return(status & 0xff);
	}

	return(0);
}

static void
defagenterrf(msg)
char *msg;
{
    fprintf(stderr, "%s\n", msg);
}

static void (*aerrf)() = defagenterrf;

void
setagenterrf(errf)
void (*errf)();
{
	aerrf = errf;
}

static void
agenterr(fmt, a1, a2, a3, a4, a5, a6)
char *fmt;
int *a1, *a2, *a3, *a4, *a5, *a6;
{
	char buf[1000];

	sprintf(buf, fmt, a1, a2, a3, a4, a5, a6);

	(*aerrf)(buf);
}
