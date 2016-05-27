/*
 *	Enhancements by David Dykstra at AT&T.
 *	This source code may be freely distributed.
 */

/*
**	Definition of a structure to hold status information
**	for a conversation with a channel.
*/

#define	NPCBUFS		2
#define MAXNPCBUFS	4

struct Pktstate
{
	struct Packet	pkt;			/* The packet */
	unsigned char	state;			/* Protocol state */
	short		timo;			/* Timeout count */
	unsigned short	size;			/* Packet size */
};

typedef struct Pktstate	*Pks_p;

struct Pchannel
{
	struct Pktstate	pkts[MAXNPCBUFS];		/* The packets */
	Pks_p		nextpkt;		/* Next packet to be acknowledged */
	unsigned char	cdata[SEQMOD];		/* Remember control data */
	unsigned char	rseq;			/* Next receive sequence number */
	unsigned char	xseq;			/* Next transmit sequence number */
	char		freepkts;		/* Number of free buffers */
	char		user;			/* Spare byte for users */
};

typedef struct Pchannel	*	Pch_p;

/**	Transmit packet states	**/

enum {	px_null, px_wait, px_ok	};

#define	PX_NULL		(int)px_null		/* Empty packet */
#define	PX_WAIT		(int)px_wait		/* Packet awaiting acknowledgement */
#define	PX_OK		(int)px_ok		/* Packet has been acknowledged */

/**	Receive packet states	**/

enum {	pr_null, pr_size, pr_data,
	pr_netnull, pr_netsize1, pr_netsize2, pr_netdata, pr_neterror };

#define	PR_NULL		(int)pr_null	 /* New packet expected */
#define	PR_SIZE		(int)pr_size	 /* Size byte next */
#define	PR_DATA		(int)pr_data	 /* Receiving data */
#define	PR_NETNULL	(int)pr_netnull	 /* New network xt packet expected */
#define	PR_NETSIZE1	(int)pr_netsize1 /* Expecting high byte of data count */
#define	PR_NETSIZE2	(int)pr_netsize2 /* Expecting low byte of data count */
#define	PR_NETDATA	(int)pr_netdata	 /* Expecting network xt data */
#define	PR_NETERROR	(int)pr_neterror /* Network xt error state */


/*
**	Interface routines
*/

extern int		psend();		/* Send data to channel */
extern void		precv();		/* Receive data/control from channel */
extern void		ptimeout();		/* Catch alarm timeouts */
extern int		pinit();		/* Initialise conversations */

/*
**	Pointers to externally declared data and functions
*/

extern struct Pchannel	pconvs[];		/* Array of conversations */
extern Pch_p		pconvsend;		/* Pointer to end of pconvs */

/*
**	Receiver data
*/

extern struct Pktstate	precvpkt;		/* Current receive packet */
extern Pbyte		pseqtable[];		/* Table of ordered sequence numbers */

/*
 *	Debugging
 *
 * Always leave PSTATISTICS and PTRACE on.
 */

#ifndef	PSTATISTICS
#define	PSTATISTICS	1
#endif
#ifndef PTRACE
#define PTRACE 1
#endif

#ifdef	PTRACE
#define	PKTHIST		40
#define	PLOGIN		0
#define	PLOGOUT		1
#else
#define	plogpkt(A,B,C,D)
#endif	PTRACE
