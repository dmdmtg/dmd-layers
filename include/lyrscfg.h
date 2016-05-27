/*
 * This file contains the defines for configuration options for dmdlayers.
 * Convention: None of these defines are needed to be on for SunOS 4.1.
 *   Defines that depend on operating system feature availability begin
 *   with USE_ or NO_.
 *
 * This file should be editted to insure that the defines needed for this
 *   machine are always enabled.
 *
 * Written by David Dykstra at AT&T.
 * This source code may be freely distributed.
 */

/*
 * NOTE: just because a machine type is mentioned in here, that does not
 *  necessarily mean that layers has been proven to work on that platform.
 *  Some are here to enable just libwindows to compile on them.
 */

#ifndef PSEUDOTTYLAYERS
#define PSEUDOTTYLAYERS

#if 0
#define FAST -1			/* priority decrease (increase if negative) */
				/*    for layers process		    */
				/* only takes effect if layers is setuid-root */
#endif


#if (vax || i386)
#define USE_LITTLEENDIAN	/* host is little endian byte order */
#endif

/*
 * NOTE: it's important to have the following define to be the
 *  same on all operating systems variants on the same processor.
 *  This define is used in libwindows so it needs to be the
 *  same to maintain binary compatibility between support programs
 *  (such as dmdld and proof) on different processors.
 * Therefore this define especially should use processor types
 *  in the "#if".
 */

#if (i386 || mips || hppa || u3b2 || u3b || uts || pyr || vax || alliant)

#define USE_NAMED_PIPE		/* use named pipes instead of sockets */

#endif

/****** End of binary compatibility options ******/

/****** Begin selection of major option groups ******/

#if (defined(USE_BSDLIKE) && !(defined(NO_SYSVLIKE)))
#define NO_SYSVLIKE
#endif

#ifndef NO_SYSVLIKE

#if (i386 || mips || hppa || u3b2 || u3b || uts || pyr || vax || alliant)
#ifndef USE_SYSVLIKE
#define USE_SYSVLIKE
#endif
#endif

#if (i386)
#if (!defined(USE_SYSVR4LIKE) && !defined(NO_SYSVR4LIKE))
#define USE_SYSVR4LIKE
#endif
#endif

#if (vax || uts || u3b || alliant)
#if (!defined(USE_SYSVR2LIKE) && !defined(NO_SYSVR2LIKE))
#define USE_SYSVR2LIKE
#endif
#endif

#if ((defined(USE_SYSVR4LIKE) || defined(USE_SYSVR2LIKE)) \
					&& !defined(USE_SYSVLIKE))
#define USE_SYSVLIKE
#endif

#endif


/****** End selection of major option groups ******/

/****** Begin definition of major option groups ******/

#ifdef USE_SYSVLIKE
#define NO_TIOCUCNTL		/* don't support old style pseudo-tty 	    */
				/*   TIOCUCNTL mechanism for JMPX, JBOOT,   */
				/*   JZOMBOOT, and JTERM ioctls on pty	    */
#define NO_KILLPG		/* no killpg() system call available, use    */
				/*   kill() with negative process id instead */
#define USE_NICE		/* use nice() instead of setpriority() */
#define NO_TIOCNOTTY		/* use setpgrp() instead of TIOCNOTTY ioctl */
#define USE_SETFD		/* use F_SETFD fcntl instead of FIOCLEX ioctl */
#define USE_TERMIOS_H		/* include sys/termios.h */
#define NO_INDEX		/* use strchr/strrchr instead index/rindex */
#define NO_BCOPY		/* use memcpy/memset instead of bcopy/bzero */
#define USE_SYSV_WAIT		/* use system V wait() */
#define USE_SYSV_SIGNALS	/* no sigblock()/sigsetmask(), etc, use */
				/*   sighold()/sigrelse() instead       */

#define NO_TTYNAMECOMPAT	/* ttyname() is not compatible between OS  */
				/*   versions; use popen("tty") instead.   */
				/*   Only impacts libwindows and only when */
				/*   using XT-driver layers.		   */

#ifdef USE_SYSVR4LIKE
#define USE_PTMX		/* use /dev/ptmx for pseudo-tty as in SVR4 */
#define USE_DIRENT		/* use dirent instead of direct */
#define USE_FILIO		/* include sys/filio.h */
#define USE_TTYDEV		/* include sys/ttydev.h */
#define USE_UNISTD		/* include unistd.h */
#define USE_UTXENT		/* use getutxent() functions for utmp access */
#else
/* System V non-SVR4 */
#define USE_UTENT		/* use getutent() functions for utmp access */
#define NO_TERMIOS		/* use termio instead of termios */
#endif

#endif

#ifdef USE_SYSVR2LIKE
#define USE_INT_SIG		/* signal handlers have integer returns */
				/*   rather than void returns           */
#endif

#ifdef USE_BSDLIKE
#define NO_TERMIO		/* use sgtty stuff instead of termio */
#endif

/****** End definition of major option groups ******/

/****** Begin specific machine type options ******/

#if (mips)
/* any mips (especially RISC/os and IRIX) */
#define NO_VFORK		/* no vfork(), use fork() */
#endif

#if (mips && !defined(USE_SYSVR4LIKE))
/* Mips RISC/os (or EPIX) */
#undef USE_TERMIOS_H
#define USE_TERMIOS_H		/* include sys/termios.h */
#define USE_CLONE_PTY		/* use clone pty device as in Mips RISC/os */
#define USE_WAIT2		/* use wait2() instead of wait3() */
#define USE_UNISTD		/* include unistd.h */
#define USE_DIRENT		/* use dirent instead of direct */
#endif

#if (sun && defined(USE_SYSVR4LIKE))
/* Solaris */
#define USE_IGNORESIGCHLDGRANTPT /* ignore SIGCHLD during grantpt() */
#define USE_CHMOD0SOCKET	/* work around bug that causes socket ops */
				/*  to fail if socket is not mode 0 */
#endif

#ifdef hppa
#undef USE_SYSV_SIGNALS
#define USE_ALT_PUTUTLINE
#define USE_ALT_PTYALLOC
#endif

#if (u3b2 || i386)
#define USE_PTEM		/* include sys/ptem.h to get struct winsize */
#endif

#if (alliant)
#define USE_PRIVATEWINSIZE	/* define private copy of struct winsize */
#endif

#if (linux)
/* assumes NO_SYSVR4LIKE is defined by the Makefile */
#define USE_DIRENT
#define USE_UNISTD
#define NO_UT_EXIT
#define USE_VOID_PUTUTLINE
#undef USE_SYSV_WAIT
#undef USE_SYSV_SIGNALS
#undef USE_PTEM
#undef NO_TERMIOS
#ifndef CEOF
#define CEOF 04			/* workaround a linux header file problem */
#endif
#endif

#ifdef uts
/* amdahl's uts, at least version 2.1.2 */
#define USE_POLL		/* use poll(2) instead of select */
#define USE_PTY_FOR_AGENT	/* use a pseudo-tty instead of named pipe */
#define USE_DIRENT
#define USE_FILIO
#define USE_MAXI_PTYALLOC
#define NO_VFORK
#endif

/****** End specific machine type options ******/

/*
 * LEAVE THE REST ALONE
 */

/* Tell support programs that they should be using hostagent calls */

#ifndef USE_HOSTAGENT	/* jioctl.h may define USE_HOSTAGENT already */
#define USE_HOSTAGENT 1
#endif

#endif
