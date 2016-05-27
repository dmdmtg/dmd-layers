/*
 * hostagent.h
 * include this instead of agent.h in HOST resident programs. Terminal
 * resident programs should still use agent.h
 *
 * This file contains the structures and defines required for the hostagent
 * simulation package used in the pty version of layers.
 *
 * Enhancements by David Dykstra at AT&T.
 * This source code may be freely distributed.
 */

#include "lyrscfg.h"

#ifndef DADDR
/* needed for agent.h where dmd.h is not available...(DADDR is in dmd.h) */
/* some versions of agent.h do not define these structures */
#define DADDR
typedef struct Point {
	short x;
	short y;
} Point;

typedef struct Rectangle {
	Point origin;
	Point corner;
} Rectangle;
#endif

#include "agent.h"

#ifndef A_XTPROTO
#define A_XTPROTO       22      /* tell me what xt protocol type to use */
#endif

/* Agent calls supported by 730windows */

#ifndef A_SCRCREATE
#define A_SCRCREATE		30		/* Create screen. */
#endif
#ifndef A_SCRDESTROY
#define A_SCRDESTROY		31		/* Destroy screen. */
#endif
#ifndef A_SCRNEXT
#define A_SCRNEXT		32		/* Next screen. */
#endif
#ifndef A_SCRPREV
#define A_SCRPREV		33		/* Previous screen. */
#endif
#ifndef A_HOSTNEW
#define A_HOSTNEW		34		/* Open new host connection. */
#endif
#ifndef A_HOSTEXIT
#define A_HOSTEXIT		35		/* Exit host connection. */
#endif
#ifndef A_SETDIAL
#define A_SETDIAL		36		/* Map dialer to host. */
#endif
#ifndef A_DIALRESET
#define A_DIALRESET		37		/* Reset dial string to 0 */
#endif
#ifndef A_DIALCONCAT
#define A_DIALCONCAT		38		/* Append string to dialer. */
#endif
#ifndef A_DIALREAD
#define A_DIALREAD		39		/* Read dialer string. */
#endif
#ifndef A_TILE
#define A_TILE			40		/* Reduce window to small box */
#endif
#ifndef A_UNTILE
#define A_UNTILE		41		/* Restore window */
#endif

/*
 * Request and answer structures for agent calls to pty layers.
 */
#define ANLEN		8
struct request {
	struct agentrect ar;
	unsigned long time;
	unsigned int pid;
	/* this should be $DMDCHAN but can be ttyname() from older libwindows */
	char chanstr[sizeof("/dev/")+ANLEN];
};

struct answer {
	short command;
	short chan;
	int result;
	unsigned long time;
	char answerbuf[ANLEN];
};

/*
 * The following are not valid agent calls. They are added for simulation
 * under pseudo-tty layers. They must NOT be valid agent numbers (as described
 * in agent.h!!!
 * WARNING: older pseudo-tty layers practically ignore any agent calls > A_DONE,
 * so if there's a chance that the agent call will be sent to an older layers
 * choose a lower number.
 */
#define A_FIRSTCONTROL	60

#define A_JZOMBOOT	60		/* start zombie download */
#define A_JBOOT		61		/* start download */
#define A_JTERM		62		/* stop download */
#define A_CHAN		63		/* request for openchan() */
/* 64 formerly A_RW_CHAN for openrwchan().  Can get away with reusing it
 *  because A_CHANGEPROC is used in Runlayer() which will bomb earlier on the
 *  A_CHAN if it's talking to one of the versions of layers that responds to
 *  A_RW_CHAN; A_CHAN was updated to return a pseudo-tty at the same time
 *  that A_RW_CHAN was removed.
 */
#define A_CHANGEPROC	64		/* change owner pid of channel */
#define A_RECV		65		/* ack for agent receipt */
#define A_DONE		66		/* ack for agent completion */
#define A_RELOGIN	67		/* move login entry to this window */
#define A_JXTPROTO	68		/* send JXTPROTO control message */
#define A_JTIMO		69		/* send JTIMO control message */
#define A_JTIMOM	70		/* send JTIMOM control message */
#define A_XTSTATS	71		/* write out XT protocol statistics */
#define A_XTTRACE	72		/* write out XT protocol trace */
/*
 * insert new ones above here, increase A_LASTCONTROL, and add them
 *  to the list in externs.c
*/
#define A_LASTCONTROL	72

/*
 * Environment variable names.
 */
#define AGENV		"DMDAGENT"
#define AGDIRENV	"DMDAGENTDIR"
#define CHANENV		"DMDCHAN"

#ifndef MAXPCHAN
#define MAXPCHAN	8
#endif
