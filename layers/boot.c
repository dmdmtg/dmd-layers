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
#include <sys/stat.h>
#include <sys/file.h>
#ifdef USE_UNISTD
#include <unistd.h>
#endif
#ifdef USE_POLL
#include <stropts.h>
#include <poll.h>
#else
#include <sys/time.h>
#endif
#ifndef USE_SYSV_WAIT
#include <sys/wait.h>
#endif
#include <sys/param.h>
#include <errno.h>
#include <stdio.h>

#include "sys/jioctl.h"

#ifdef USE_TERMIOS_H
#include <sys/termios.h>
#endif
#ifdef USE_TERMIO_H
#include <sys/termio.h>
#endif

extern int Loadtype;

printidstring(p)
unsigned char *p;
{
    while (*p) {
	if ((*p >= ' ') && (*p <= '~'))
	    fprintf(stderr,"%c",*p);
	else
	    fprintf(stderr,"\\%03o",*p);
	p++;
    }
}

striphigh(p,n)
unsigned char *p;
int n;
{
    int i;
    for (i = 0; i < n; i++) {
	*p &= 0x7f;
	p++;
    }
}

checkterm(fd)
{
	register int cc, n;
#ifndef USE_POLL
	struct timeval t;
#endif
	int rd_set;
	char answer[BUFSIZ];
	int encflag;
	extern int dmdtype;
	extern int dmdvers;
	extern char *getenv();

#ifdef TIOCFLUSH
	(void) ioctl(fd, TIOCFLUSH, FREAD);
#endif
#ifdef TCFLSH
	(void) ioctl(fd, TCFLSH, 0);
#endif
	if (check_write(fd, SENDTERMID, strlen(SENDTERMID)) <= 0) {
		fprintf(stderr,"layers: checkterm(): write failed (errno %d)\r\n",errno);
		return(-1);
	}
#ifndef USE_POLL
	ZERO_FD(rd_set);
	t.tv_sec = 7;
	t.tv_usec = 0;
#endif
	for (cc = 0; (cc == 0) || (answer[cc-1] != 'c'); cc += n) {

#ifdef USE_POLL
		struct pollfd pollfd;
		pollfd.fd = fd;
		pollfd.events = POLLIN;
		if ((n = poll(&pollfd, 1, 7000)) <= 0)
#else
		SET_FD(fd, rd_set);
		if ((n = select(MAXFD, &rd_set, (int *)0, (int *)0, &t)) <= 0)
#endif
		{
			if (n == 0) {
				fprintf(stderr,"layers: timed out waiting for end of terminal id string\r\n",errno);
				if (cc > 0) {
					answer[cc] = '\0';
					fprintf(stderr," only received: ");
					printidstring(answer);
					fprintf(stderr,"\r\n");
				}
			}
			else
				fprintf(stderr,"layers: checkterm(): select failed (errno %d)\r\n",errno);
			return(-1);
		}
		if ((n = read(fd, &answer[cc], BUFSIZ-cc)) <= 0) {
			fprintf(stderr,"layers: checkterm(): read failed (errno %d)\r\n",errno);
			return(-1);
		}
		DebugRaw(0, &answer[cc], n);
		striphigh(&answer[cc], n);
	}
	answer[cc] = '\0';
	/*
	 * allow for missing left bracket as is the case on 730 wproc with 8
	 *   bit terminal controls enabled.
	 */
	cc = 0;
	if (answer[cc] == '\033') {
	    if (answer[++cc] == '[')
	        cc++;
	}
	if (sscanf(&answer[cc], "?8;%d;%dc", &dmdtype, &dmdvers) != 2) {
		fprintf(stderr, "Layers: Not a DMD terminal, id string ");
		printidstring(answer);
		fprintf(stderr,"\r\n");
		return(-1);
	}
	if (dmdtype == DMD_5620) {
		if (dmdvers <= 2) {
			fprintf(stderr, "Layers: Obsolete 5620 firmware version\r\n");
			return(-1);
		}
		if (dmdvers < 5)
			return(0);
	}
	if (check_write(fd, SENDENC, strlen(SENDENC)) <= 0) {
		fprintf(stderr,"layers: checkterm(): write failed (errno %d)\r\n",errno);
		return(-1);
	}
	for (cc = 0; (cc == 0) || (answer[cc-1] != 'F'); cc += n) {
#ifdef USE_POLL
		struct pollfd pollfd;
		pollfd.fd = fd;
		pollfd.events = POLLIN;
		if ((n = poll(&pollfd, 1, 7000)) <= 0)
#else
		SET_FD(fd, rd_set);
		if ((n = select(MAXFD, &rd_set, (int *)0, (int *)0, &t)) <= 0)
#endif
		{
			if (n == 0) {
				fprintf(stderr,"layers: timed out waiting for terminal encoding string\r\n",errno);
				if (cc > 0) {
					answer[cc] = '\0';
					fprintf(stderr," only received: ");
					printidstring(answer);
					fprintf(stderr,"\r\n");
				}
			}
			else
				fprintf(stderr,"layers: checkterm(): select failed (errno %d)\r\n",errno);
			return(-1);
		}
		if ((n = read(fd, &answer[cc], BUFSIZ-cc)) <= 0) {
			fprintf(stderr,"layers: checkterm(): read failed (errno %d)\r\n",errno);
			return(-1);
		}
		DebugRaw(0, &answer[cc], n);
		striphigh(&answer[cc], n);
	}
	answer[cc] = '\0';
	cc = 0;
	if (answer[cc] == '\033') {
	    if (answer[++cc] == '[')
	        cc++;
	}
	if (sscanf(&answer[cc], "%dF", &encflag) != 1) {
		fprintf(stderr, "Layers: Invalid encoding response ");
		printidstring(answer);
		fprintf(stderr, "\r\n");
		return(-1);
	}
	if (Loadtype == UNKNOWN_LOAD) {
		if (encflag)
			Loadtype = HEX_LOAD;
		else
			Loadtype = BINARY_LOAD;
	}
	return(0);
}

boot(fd)
{
	register int pid, r;
	char lsysbuf[16];
	char loader[MAXPATHLEN];
	char patch[MAXPATHLEN];
#ifdef USE_SYSV_WAIT
	int status;
#else
	union wait status;
#endif
	struct stat statbuf;
	char *arg0;
	extern char dmdpath[];
	extern int dmdtype;
	extern int dmdvers;
	extern char lsys[];

	if (checkterm(fd))
		return(-1);

	statbuf.st_size = 0;
	patch[0] = '\0';
	if (dmdtype == DMD_5620) {
		moveutmp(2);	/* 5620 never uses channel 1 */
		if (lsys[0] == '\0') {
			sprintf(lsysbuf, "lsys.8;%d;%d", dmdtype, dmdvers);
			findsysfile(lsys, lsysbuf, 04,
					dmdvers <= 4 ? "error:" : NULL);
			if (lsys[0] != '\0')
				(void) stat(lsys, &statbuf);
		}
		else if (stat(lsys, &statbuf) == -1) {
			fprintf(stderr, "Layers: Cannot stat layersys %s\r\n",
						lsys);
			return(-1);
		}
		if (dmdvers <= 4) {
			if (statbuf.st_size == 0) {
				fprintf(stderr, 
					"Layers: 5620 version 1 firmware requires non-zero download\r\n");
				return(-1);
			}
			findsysfile(patch, "set_enc.j", 04,
			      "5620 version 1 firmware requires set_enc.j\r\n");
			if (patch[0] == '\0')
				return(-1);
		}
	}
	else if (lsys[0] != '\0') {
		if (stat(lsys, &statbuf) == -1) {
			fprintf(stderr, "Layers: Cannot stat layersys %s\r\n",
						lsys);
			return(-1);
		}
		if (statbuf.st_size != 0) {
			fprintf(stderr,
			   "Layers: layersys download only works on 5620s\r\n");
			return(-1);
		}
	}

	if (statbuf.st_size == 0) {
		/*
		 * Tell terminal to go into multiplex mode
		 */
		if (Loadtype == HEX_LOAD)
			printf("\033[2;2v");
		else
			printf("\033[2;0v");
		fflush(stdout);
		return(0);
	}

	(void)sprintf(loader, "%s/bin/32ld", dmdpath);
	arg0 = "32ld";
	if (access(loader, X_OK) == -1) {
		fprintf(stderr, "Layers: cannot find loader %s\r\n", loader);
		return(-1);
	}

	if (patch[0] != '\0') {
		printf("\n\n\tPlease stand by.\r\n");
		printf("\n\tDouble download for layers coming up.\r\n");
		printf("\tThere is a delay between the two.\r\n");
		fflush(stdout);
		mysleep(5); /* time for the message to be read */
#ifdef NO_VFORK
		if ((pid = fork()) == 0)
#else
		if ((pid = vfork()) == 0)
#endif
		{
			execl(loader, arg0, patch, (char *)0);
			fprintf(stderr,"Layers: cannot exec %s %s\r\n",loader,patch);
			_exit(127);
		}
		while ((r = wait(&status)) != pid && r != -1)
			;
		if ((r == -1) ||
#ifdef USE_SYSV_WAIT
				(status != 0)
#else
				(status.w_status != 0)
#endif
							) {
			fprintf(stderr, "Layers: cannot download patch %s\r\n",patch);
			return(-1);
		}
		mysleep(2);
		printf("\n\tSecond download coming up.\r\n");
		fflush(stdout);
		mysleep(3);
	}
#ifdef NO_VFORK
	if ((pid = fork()) == 0)
#else
	if ((pid = vfork()) == 0)
#endif
	{
		if (Loadtype == HEX_LOAD)
			execl(loader, arg0, "-l", "-p", lsys, (char *)0);
		else
			execl(loader, arg0, "-l", lsys, (char *)0);
		fprintf(stderr, "Layers: cannot exec %s %s\r\n",loader, lsys);
		_exit(127);
	}
	while ((r = wait(&status)) != pid && r != -1)
		;
	if ((r == -1) ||
#ifdef USE_SYSV_WAIT
			(status != 0)
#else
			(status.w_status != 0)
#endif
							) {
		fprintf(stderr, "Layers: cannot boot %s\r\n", lsys);
		return(-1);
	}
	return(0);
}
