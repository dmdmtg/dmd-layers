/*
 * Enhancements by David Dykstra at AT&T.
 * This source code may be freely distributed.
 */

#define	A_NEWLAYER	1	/* make a new layer */
#define	A_CURRENT	2	/* make layer process current */
#define	A_DELETE	3	/* delete a layer */
#define	A_TOP		4	/* bring a layer to top */
#define	A_BOTTOM	5	/* put a layer on bottom */
#define	A_MOVE		6	/* move a layer */
#define	A_RESHAPE	7	/* reshape a layer */
#define	A_NEW		8	/* make a new layer and send C_NEW to layers */
#define	A_EXIT		9	/* exit layers program */

/* Leave some room for future mouse operations to be implemented 	*/

#define	A_ROMVERSION	20	/* tell us your rom version, e.g. 8;8;6 */
#define A_STACKSIZE	21	/* set stack size on a 5620 */


typedef struct agentPoint {
	short	x;
	short	y;
} agentPoint;

typedef struct agentRectangle {
	agentPoint origin;
	agentPoint corner;
} agentRectangle;

struct agentrect{
	short	command;	/* A_NEWLAYER, A_CURRENT, A_DELETE, etc. */
	short	chan;		/* channel */
	agentRectangle r;	/* rectangle description */
};
