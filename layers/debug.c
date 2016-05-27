/*
 * This file added by Dave Dykstra on 7-29-93.
 * This source code may be freely distributed.
 */
#include "common.h"
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <varargs.h>

extern int DebugLevel;
extern int DebugTimeout;
extern FILE *DebugFile;

void
Debug(level, format, va_alist)
int level;
char *format;
va_dcl
{
	va_list args;
	time_t seconds;
	struct tm *loctime;
	static int dotimestamp = 1;

	if ((DebugLevel & level) == 0)
		return;

	if (DebugTimeout) {
		/* timeout debug messages can interrupt other debug messages */
		if (!dotimestamp) {
			fprintf(DebugFile, "\n");
			dotimestamp = 1;
		}
		DebugTimeout = 0;
	}

	if (dotimestamp) {
		seconds = time((time_t *) 0);
		loctime = localtime(&seconds);
		fprintf(DebugFile, "%02d:%02d:%02d %2d ", loctime->tm_hour,
			loctime->tm_min, loctime->tm_sec, level);
	}

	va_start(args);
	(void) vfprintf(DebugFile, format, args);
	va_end(args);

	if ((*format == '\0') || (format[strlen(format)-1] == '\n')) {
		fflush(DebugFile);
		dotimestamp = 1;
	}
	else
		dotimestamp = 0;
}

void
DebugRaw(xmit, buf, nbytes)
int xmit;
unsigned char *buf;
int nbytes;
{
	int i;

	if ((DebugLevel & DEBRTRACE) == 0)
		return;

	if (xmit)
		Debug(DEBRTRACE, "sending %d bytes to terminal:", nbytes);
	else
		Debug(DEBRTRACE, "received %d bytes from terminal:", nbytes);

	i = 8;
	while(nbytes > 0) {
		for (; (nbytes > 0) && (i < 15); i++, nbytes--)
			fprintf(DebugFile, "<%02x>", *buf++);
		fprintf(DebugFile, "\n");
		i = 0;
	}

	Debug(DEBRTRACE, "", 0);  /* reset dotimestamp */
}

DebugInit(debugfname)
char *debugfname;
{
	time_t now;
	char *ascnow;
	extern char *asctime();

	if (DebugLevel > 0) {
		if ((DebugFile = fopen(debugfname, "w")) == NULL) {
			fprintf(stderr, "Layers: cannot open debug file %s\n",
					debugfname);
			exit(1);
		}
		fprintf(stderr, "** Debug messages going to %s **\n",
						debugfname);
		now = time((time_t *) 0);
		ascnow = asctime(localtime(&now));
		if (ascnow[strlen(ascnow)-1] == '\n')
			ascnow[strlen(ascnow)-1] = '\0';
		Debug(DebugLevel, "** Trace started at %s **\n", ascnow);
		Debug(DebugLevel, "** using Debug level %d **\n", DebugLevel);
	}
}

