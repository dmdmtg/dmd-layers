/*
 *	Enhancements by David Dykstra at AT&T.
 *	This source code may be freely distributed.
 */

/*
**	Mpx -- Blit packet protocol definition
*/

typedef	unsigned char	Pbyte;			/* The unit of communication */

#ifndef MAXPCHAN
#define	MAXPCHAN	8			/* Maximum channel number */
#endif
#define	CBITS		3			/* Bits for channel number */
#define	MAXPKTDSIZE	(32 * sizeof(Pbyte))	/* Maximum data part size */
#define MAXOUTDSIZE	(252 * sizeof(Pbyte))	/* Maximum data size for
						   outgoing packets. */
#define	EDSIZE		(2 * sizeof(Pbyte))	/* Error detection part size */
#define	SEQMOD		8			/* Sequence number modulus */
#define	SBITS		3			/* Bits for sequence number */

/*
**	Packet header
**	(if only bit fields in C were m/c independent, sigh...)
*/

#ifdef USE_LITTLEENDIAN
struct P_header
{
	Pbyte		seq	:SBITS,		/* Sequence number */
			channel	:CBITS,		/* Channel number */
			cntl	:1,		/* TRUE if control packet */
			ptyp	:1;		/* Always 1 */
	Pbyte		dsize;			/* Size of data part */
};
#else
struct P_header
{
	Pbyte		ptyp	:1,		/* Always 1 */
			cntl	:1,		/* TRUE if control packet */
			channel	:CBITS,		/* Channel number */
			seq	:SBITS;		/* Sequence number */
	Pbyte		dsize;			/* Size of data part */
};
#endif

typedef	struct P_header	Ph_t;

/*
**	Packet definition for maximum sized packet for transmission
*/

struct Packet
{
	Ph_t		header;			/* Header part */
	Pbyte		data[MAXOUTDSIZE];	/* Data part */
	Pbyte		edb[EDSIZE];		/* Error detection part */
};

typedef struct Packet *	Pkt_p;

/*
**	Control codes
*/

#ifdef TERM_68
#define	PPCNTL		0200			/* Packet protocol control code */
#else
#define	PPCNTL		0000			/* Packet protocol control code */
#endif
#define	ACK		(Pbyte)(PPCNTL|006)	/* Last packet with same sequence ok and in sequence */
#define	NAK		(Pbyte)(PPCNTL|025)	/* Last packet with same sequence received out of sequence */
#define	PCDATA		(Pbyte)(PPCNTL|0100)	/* Data only control packet */

/*
 * Network XT defines
 */
#define NETHDRSIZE	3	/* number of bytes in net xt header */
#define MAXNETHIWAT	4096	/* max net xt high water mark */
#define MINNETHIWAT	256	/* min net xt high water.  Has to be at least */
				/*  256 because the 730 terminal doesn't send */
				/*  the ack back until then. */
#define DEFNETHIWAT	384	/* default net xt high water.  384 was chosen */
				/*  for streams-XT driver to reduce the       */
				/*  amount of skid after the user aborts a    */
				/*  command.     */
