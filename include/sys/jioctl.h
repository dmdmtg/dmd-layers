/*
 * Unix to Jerq I/O control codes pseudo-tty layers version
 *
 * Enhancements by David Dykstra at AT&T.
 * This source code may be freely distributed.
 */
#include "lyrscfg.h"
#include <sys/ioctl.h>

#ifdef NO_TIOCUCNTL
/*
 * Need to use agent calls instead of these if NO_TIOCUCNTL is turned on.
 * Use inlayers() in place of JMPX, Jwinsize() in place of JWINSIZE, Jboot()
 *   in place of JBOOT, Jzomboot() in place of JZOMBOOT, and Jterm() in
 *   place of JTERM.
 */
#else

#ifndef UIOCCMD
#define UIOCCMD(n)	_IO(u, n)
#endif

#define	JBOOT		UIOCCMD(1)
#define	JTERM		UIOCCMD(2)
#define	JMPX		UIOCCMD(0)
#define	JTIMO		UIOCCMD(4)	/* Timeouts in seconds */
#define	JTIMOM		UIOCCMD(6)	/* Timeouts in millisecs */
#define	JZOMBOOT	UIOCCMD(7)
#define	JAGENT		UIOCCMD(9)	/* control for both directions */
#define	JSMPX		TIOCUCNTL

#define	JWINSIZE	TIOCGWINSZ

#define bitsx		ws_xpixel
#define bitsy		ws_ypixel
#define bytesx		ws_col
#define bytesy		ws_row
#define jwinsize	winsize
#endif

#ifdef NO_WINSIZE
struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};
#endif

/**	Channel 0 control message format **/

struct jerqmesg
{
	char	cmd;		/* A control code above */
	char	chan;		/* Channel it refers to */
};

/*
**	Character-driven state machine information for Jerq to Unix communication.
*/

#define	C_SENDCHAR	1	/* Send character to layer process */
#define	C_NEW		2	/* Create new layer process group */
#define	C_UNBLK		3	/* Unblock layer process */
#define	C_DELETE	4	/* Delete layer process group */
#define	C_EXIT		5	/* Exit */
#define	C_BRAINDEATH	6	/* Send terminate signal to proc. group */
#define	C_SENDNCHARS	7	/* Send several characters to layer proc. */
#define	C_RESHAPE	8	/* Layer has been reshaped */
#define C_JAGENT	9	/* Agent command follows */
#define C_NOFLOW        10      /* Disable network xt flow control */
#define C_YESFLOW       11      /* Enable network xt flow control */

/*
**	Usual format is: [command][data]
*/

/*
 *	This defines things to do with the host agent on the Blit.
 */

struct bagent{
	int size;	/* size of src string going in and dest string out */
	char * src;	/* address of the source byte string */
	char * dest;	/* address of the destination byte string */
};
