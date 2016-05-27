/*
 * Move a utmp entry from one tty to another.
 * Separated out from 'layers' to minimize the amount of code that needs
 *  to be setuid-root.
 *
 * This source code may be freely distributed.
 *
 * - Dave Dykstra, 5/3/93
 */

#include "lyrscfg.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef USE_UTENT
#ifndef USE_UTXENT
#ifndef USE_BSDUTMP
#define USE_BSDUTMP
#endif
#endif
#endif

#ifdef USE_BSDUTMP
#include <utmp.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#else
#ifdef USE_UTXENT
#include <utmpx.h>
#include <sys/time.h>
#else
#include <utmp.h>
#include <time.h>
#endif
#endif

usage()
{
	fprintf(stderr,"usage: movelogin [-h hostname] fromtty totty\n");
	exit(1);
}

void
checkaccess(tty, mode, message)
char *tty;
int mode;
char * message;
{
	char ttybuf[80];
	strcpy(ttybuf, "/dev/");
	strcat(ttybuf, tty);
	if (access(ttybuf, mode) == -1) {
		fprintf(stderr, "movelogin: %s %s\n", message, ttybuf);
		exit(1);
	}
}

char *moveutmp();

main(argc, argv)
char **argv;
{
	extern char *optarg;
	extern int optind;
	int retval;
	char *hostname = NULL;
	char *rethost;
	char *fromtty, *totty;

	while ((retval = getopt(argc, argv, "h:")) != EOF) {
		switch (retval)
		{
		case 'h':
			hostname = optarg;
			break;
		default:
			usage();
		}
	}
	if (optind != argc - 2)
		usage();

	fromtty = argv[optind];
	totty = argv[optind+1];
	if (strncmp(fromtty, "/dev/", strlen("/dev/")) == 0)
		fromtty += strlen("/dev/");
	if (strncmp(totty, "/dev/", strlen("/dev/")) == 0)
		totty += strlen("/dev/");

	/*
	 * I would have liked to check for read and write access to the
	 *   ttys to make sure that someone is not trying to change a
	 *   utmp entry they're not supposed to, but if somone "su"s to
	 *   another user id they might not necessarily have read and
	 *   write access to the tty they're running on.  What the heck,
	 *   the worse someone could do is to make the utmp file entries
	 *   show people logged on to the wrong /dev/ device which would
	 *   only be a very minor breach of security.
	 */
	checkaccess(fromtty, 0, "cannot access");
	checkaccess(totty, 0, "cannot access");

	rethost = moveutmp(fromtty, totty, hostname);
	if (rethost == NULL)
		exit(1);
	if (hostname != NULL)
		printf("-h %s\n", rethost);
	return(0);
}

#ifdef USE_BSDUTMP
char *getent();

char *
moveutmp(fromtty, totty, hostname)
char *fromtty;
char *totty;
char *hostname;
{
	long fromslot, toslot;
	struct utmp utmp;
	static struct utmp nullutmp;	/* statics are initialized to zero */
	static char oldhost[sizeof(utmp.ut_host)+1];
	int utmpfd;

	if ((fromslot = slotno(fromtty)) == -1L)
		return(NULL);

	if ((toslot = slotno(totty)) == -1L)
		return(NULL);

	if ((utmpfd = open("/etc/utmp", O_RDWR)) < 0) {
		fprintf(stderr, "movelogin: Cannot open /etc/utmp for writing");
		if (geteuid() != 0)
		    fprintf(stderr, "; not installed setuid-root");
		fprintf(stderr, "\n");
		return(NULL);
	}

	/*
	 * Check 'to' slot to make sure not clobbering an entry
	 */
	if (lseek(utmpfd, toslot, 0) == -1L) {
		fprintf(stderr,
			"movelogin: Cannot lseek /etc/utmp to position %d\n",
				toslot);
		goto errreturn;
	}
	if (read(utmpfd, &utmp, sizeof(utmp)) != sizeof(utmp)) {
		/* slot probably hasn't been created yet */
		utmp.ut_name[0] = '\0';
	}

	if (utmp.ut_name[0] != '\0') {
		/*
		 * The entry is not empty.  Give a second chance if both ttys
		 *  are readable and writable by user in case the previous
		 *  owner bombed without cleaning up.  There is no ut_pid
		 *  slot in this style of utmp file so we can't verify for
		 *  sure that the previous owner process is still running.
		 * Need to check both ttys because otherwise a hacker could
		 *  use this program to wipe out his own entry by moving
		 *  someone else's to his slot and back to where it belongs.
		 *  (A moot point on systems such as SunOS 4.1 that have
		 *  universal access to the utmp file.)
		 * Note that if the totty utmp slot is not empty AND the user
		 *  had su'ed to another user id, they're out of luck.
		 */
		checkaccess(totty, 06,
			"utmp slot not empty and no read and write access to");
		checkaccess(fromtty, 06,
			"totty slot not empty no read and write access to");
	}

	/*
	 * Read 'from' slot
	 */
	if (lseek(utmpfd, fromslot, 0) == -1L) {
		fprintf(stderr,
			"movelogin: Cannot lseek /etc/utmp to position %d\n",
				fromslot);
		goto errreturn;
	}
	if (read(utmpfd, &utmp, sizeof(utmp)) != sizeof(utmp)) {
		fprintf(stderr, "movelogin: Error reading /etc/utmp\n");
		goto errreturn;
	}
	if (utmp.ut_name[0] == '\0') {
		fprintf(stderr, "movelogin: fromtty slot empty\n");
		goto errreturn;
	}

	/*
	 * Null out 'from' slot
	 */
	if (lseek(utmpfd, fromslot, 0) == -1L) {
		fprintf(stderr,
			"movelogin: Cannot lseek /etc/utmp to position %d\n",
				fromslot);
		goto errreturn;
	}
	if (write(utmpfd, &nullutmp, sizeof(nullutmp)) != sizeof(nullutmp)) {
		fprintf(stderr, "movelogin: Error writing /etc/utmp\n");
		goto errreturn;
	}

	/*
	 * Fill in 'to' slot
	 */
	if (lseek(utmpfd, toslot, 0) == -1L) {
		fprintf(stderr,
			"movelogin: Cannot lseek /etc/utmp to position %d\n",
				toslot);
		goto errreturn;
	}

	if (hostname != NULL) {
		sprintf(oldhost, "%.*s", sizeof(oldhost)-1, utmp.ut_host);
		strncpy(utmp.ut_host, hostname, sizeof(utmp.ut_host));
	}
	else
		oldhost[0] = '\0';

	strncpy(utmp.ut_line, totty, sizeof(utmp.ut_line));

	utmp.ut_time = (long) time((time_t *) 0);

	if (write(utmpfd, &utmp, sizeof(utmp)) != sizeof(utmp)) {
		fprintf(stderr, "movelogin: Error writing /etc/utmp\n");
		goto errreturn;
	}

	close(utmpfd);

	return(oldhost);

errreturn:
	close(utmpfd);
	return(NULL);
}

/*
 * Return the offset of the slot in the utmp file corresponding to the
 * argument line.  Comes from the line number in the /etc/ttys file.
 */
static FILE *entfd;

static long
slotno(tty)
char *tty;
{
	char *ty;
	int n;

	if ((entfd = fopen("/etc/ttys", "r")) == NULL) {
		fprintf(stderr, "movelogin: cannot open /etc/ttys\n");
		return(-1L);
	}

	for (n = 1; (ty = getent()) != NULL; n++) {
		if (strcmp(ty, tty) == 0) {
			break;
		}
	}

	fclose(entfd);

	if (ty == NULL) {
		fprintf(stderr, "movelogin: %s not in /etc/ttys\n", tty);
		return(-1L);
	}
	else
		return((long)n * sizeof(struct utmp));
}

char *
getent()
{
	char *p;
	int c;
	char *q;
	static char line[256];

	if (entfd == NULL)
		return ((char *)0);
	do {
		if ((p = fgets(line, sizeof(line), entfd)) == NULL)
			return((char *) 0);
		while ((((c = *p) == '\t') || (c == ' ') || (c == '\n')) &&
			(c != '\0'))
			p++;
	} while ((c == '\0') || (c == '#'));
	for (q = p; *q != '\0'; q++) {
		if ((*q == '\t') || (*q == ' ') || (*q == '\n')) {
			*q = '\0';
			break;
		}
	}
	if ((*p >= '0') && (*p <= '9')) {
		if ((p+2) >= q)
			return((char *)0);
		else
			return(p+2);
	}
	return(p);
}

#else
/* USE_UTENT or USE_UTXENT */
#ifdef USE_UTXENT
#define GETUTLINE getutxline
#define PUTUTLINE pututxline
#define SETUTENT setutxent
#define ENDUTENT endutxent
#else
#define GETUTLINE getutline
#ifdef USE_ALT_PUTUTLINE
#define PUTUTLINE _pututline
#else
#define PUTUTLINE pututline
#endif
#define SETUTENT setutent
#define ENDUTENT endutent
#endif

char *
moveutmp(fromtty, totty, hostname)
char *fromtty;
char *totty;
char *hostname;
{
#ifdef USE_UTXENT
	struct utmpx utmp, *up;
	static char oldhost[sizeof(utmp.ut_host)+1];
#else
	struct utmp utmp, *up;
	static char oldhost[1];
#endif

	/*
	 * Check to see if totty already in utmp
	 */
	strncpy(utmp.ut_line, totty, sizeof(utmp.ut_line));

	up = GETUTLINE(&utmp);

	if (up != NULL) {
		/*
		 * Check to see if the owner of the slot is still active.
		 */
		if (kill(up->ut_pid, 0) != -1) {
			fprintf(stderr, "movelogin: %s %s %d %s\n",
				totty, "already in utmp and process",
						up->ut_pid, "still active");
			goto errreturn;
		}

		/*
		 * Already exists but override it.  Clear out old slot.
		 */
		up->ut_type = DEAD_PROCESS;
#ifndef NO_UT_EXIT
		up->ut_exit.e_termination = 0;
		up->ut_exit.e_exit = 0;
#endif
#ifdef USE_UTXENT
		gettimeofday(&up->ut_tv, NULL);
#else
		up->ut_time = time((time_t *) 0);
#endif
#ifdef USE_VOID_PUTUTLINE
		PUTUTLINE(up);
#else
		up = PUTUTLINE(up);

		if (up == NULL) {
			fprintf(stderr, "movelogin: Error writing utmp file");
			if (geteuid() != 0)
			    fprintf(stderr, "; not installed setuid-root");
			fprintf(stderr, "\n");
			goto errreturn;
		}
#endif

	}

	SETUTENT();

	/*
	 * Find utmp entry
	 */
	strncpy(utmp.ut_line, fromtty, sizeof(utmp.ut_line));

	up = GETUTLINE(&utmp);

	if (up == NULL) {
		fprintf(stderr, "movelogin: Error finding %s in utmp file\n",
				fromtty);
		goto errreturn;
	}

	/*
	 * Check to see that the owner of the slot is still active.
	 */
	if (kill(up->ut_pid, 0) == -1) {
		fprintf(stderr, "movelogin: %s process %d not active\n",
					fromtty, up->ut_pid);
		goto errreturn;
	}

	/*
	 * Write out new entry
	 */
	strncpy(up->ut_line, totty, sizeof(up->ut_line));

	oldhost[0] = '\0';
#ifdef USE_UTXENT
	if (hostname != NULL) {
		sprintf(oldhost, "%.*s", sizeof(oldhost)-1, up->ut_host);
		strncpy(up->ut_host, hostname, sizeof(up->ut_host));
	}
	gettimeofday(&up->ut_tv, NULL);
#else
	up->ut_time = time((time_t *) 0);
#endif

#ifdef USE_VOID_PUTUTLINE
	PUTUTLINE(up);
#else
	up = PUTUTLINE(up);

	if (up == NULL) {
		fprintf(stderr, "movelogin: Error writing utmp file");
		if (geteuid() != 0)
		    fprintf(stderr, "; not installed setuid-root");
		fprintf(stderr, "\n");
		goto errreturn;
	}
#endif

	ENDUTENT();
	return(oldhost);

errreturn:
	ENDUTENT();
	return(NULL);
}

#endif
