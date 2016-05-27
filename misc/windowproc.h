/*
 *	Enhancements by David Dykstra at AT&T.
 *	This source code may be freely distributed.
 */

/*
 * defines for host resident programs to program parts of windowproc
 */

/*
 * program the Enter key
 * args: char count (max 4); the string
 */
#define PRGENTER	"[25;%d|%s"
#define PENTERMAX	40

/*
 * Program the PF key
 * args: key number (1 - 8); length of string (80 max); string
 */
#define PRGPFKEY	"[%d;%dq%s"
#define PFKEYMAX	80

/*
 * Program the Program menu <item><string sent when item selected>
 * args: length of item; length of string; menu depth (0-3); item; string
 */
#define PRGRMENU	"[?%d;%d;%dx%s%s"
#define MTGMENURC	".mtgmenurc"

/*
 * Program the Label Area
 * args: length of text; placment (0-2); text
 */
#define PRGLABEL	"[?%d;%dv%s"
#define L_LABEL		0
#define C_LABEL		1
#define R_LABEL		2
