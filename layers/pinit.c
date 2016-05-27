/*
 *	Enhancements by David Dykstra at AT&T.
 *	This source code may be freely distributed.
 */

#include	"common.h"
#include 	<sys/types.h>
#include	<sgtty.h>
#include	<signal.h>
#include	<stdio.h>
#include	"pconfig.h"
#include	"proto.h"
#include	"packets.h"
#include	"sys/jioctl.h"

Pch_p		pconvsend;		/* Pointer to end of pconvs */
Pbyte		pseqtable[2*SEQMOD];	/* Table of sequence numbers */

/*
 * Picking the correct timeout value is very tricky because there are a lot
 *  of factors involved.  In addition to the baud rate, packet size and
 *  and system/network load are factors.  Fortunately, if a value that is
 *  only slightly too small is chosen, packets will be occassionally
 *  retransmitted but it won't cause any problems.  A too-large value
 *  will only be a problem on noisy lines because it will take longer to
 *  recover.
 * I have chosen a heuristic based only on baud rate.  I presume that
 *  larger packet sizes will only be used at the higher baud rates where
 *  system/network load delays are more significant, but I allow enough
 *  time even for the maximum 252 byte packet at each baud rate.
 *  In addition, if the user doesn't explicitly tell me with '-b baud'
 *  I assume a maximum baud rate of 4800; that is because some networks
 *  don't properly propagate a dialup baud rate.
 *
 * - Dave Dykstra, 4/27/93
 */

static struct {
	short	speed;
	short	xtimeout;	/* time to get ack back */
	short	rtimeout;	/* time to receive one full packet */
} speeds[] = {
	EXTA,	3,	2,
	B9600,	4,	2,
	B4800,	6,	2,
	B2400,	9,	2,
	B1200,	13,	3,
	B300,	18,	8,
	0,	6,	2,	/* default */
};

struct Pchannel	pconvs[MAXPCHAN];

extern int Loadtype;
extern int numpcbufsarg;
extern int numpcbufs;
extern int maxpktarg;
extern int maxpkt;
extern int highwaterarg;
extern int highwater;
extern int networkxt;
extern int regularxtarg;

extern int buf_write();
extern int control();

struct Pconfig	pconfig	= {
	 buf_write, control, (void(*)())control
};

#define	max(a, b)	(((a) > (b))? (a): (b))

extern char fakespeed;

protocolinit(fd)
{
	extern int enc_write();
	int n;
	
	Pxfdesc = fd;
	maxpkt = MAXPKTDSIZE;
	numpcbufs = NPCBUFS;
	if (Loadtype == HEX_LOAD)
		pconfig.xfuncp = enc_write;
	if (pinit(MAXPCHAN) == -1)
		return (-1);
	
	if (fakespeed == 0) {
		fakespeed = cntrlspeed(0);
		if (fakespeed > B4800)
			fakespeed = B4800;
	}

	for (n = 0; speeds[n].speed; n++) {
		if (speeds[n].speed <= fakespeed)
			break;
	}

	Pxtimeout = speeds[n].xtimeout;
	Prtimeout = speeds[n].rtimeout;
	Pscanrate = 1;

	if (agentcom(0, A_JTIMO, Prtimeout, Pxtimeout, 0, 0) == -1)
		return(-1);
	
	if ((maxpktarg == MAXPKTDSIZE) &&
			(regularxtarg || (Loadtype == HEX_LOAD))) {
		/*
		 * Don't need to inquire the terminal about using network XT
		 *  or increased buffer sizes.  This case is included mostly
		 *  for XT terminal-side implementations that don't respond
		 *  at all to the A_XTPROTO request (all are supposed to
		 *  respond either negatively or affirmatively).
		 */
		xtprotoresp(1, 0);
		return(0);
	}
	/*
	 * Send an agent command to the terminal to see if it handles
	 *   increased buffer sizes or network XT.
	 * The first parameter after A_XTPROTO is the maximum buffer
	 *   size we want to be able to send to the terminal, or 1 if we want
	 *   to ask if they support network XT.
	 * The response will come back into xtprotoresp().
	 */
	return(agentcom(0, A_XTPROTO, 1, 0, 0, 0));
}

xtprotoresp(result, maxpktresult)
short result;
short maxpktresult;
{
	int i;
	char buf[3];
	int maxpktreq;
	int validxtproto = 0;

	if (result == 0) {
		if ((maxpktresult == 1) || ((maxpktresult >= MAXPKTDSIZE) &&
					(maxpktresult <= MAXOUTDSIZE))) {
			maxpkt = maxpktresult;
			validxtproto = 1;
		}
		else
			Debug(DEBINFO, "Invalid A_XTPROTO from terminal: %d\n",
					maxpktresult);
	}
	else
		Debug(DEBINFO, "A_XTPROTO not supported by terminal\n");

	if (maxpkt == 1) {
		/* Hex encoding implies regular xt */
		if (!regularxtarg && (Loadtype != HEX_LOAD))
			networkxt = 1;
		else
			maxpkt = MAXOUTDSIZE;
	}

	if (networkxt) {
		maxpktreq = 1;
		if (highwaterarg)
			highwater = highwaterarg;
		else
			highwater = DEFNETHIWAT;
	}
	else {
		if (maxpktarg) {
			/*
			 * User specified a packet size.
			 * Limit it to what the terminal supports.
			 */
			if (maxpktarg < maxpkt)
				maxpkt = maxpktarg;
		}
		else {
			/*
			 * User did not specify a packet size.
			 * Adjust maxpkt to baud rate.
			 */
			if (maxpkt > 128)
				maxpkt = 128;
			if ((fakespeed < B9600) && (maxpkt > 64))
				maxpkt = 64;
			if (fakespeed <= B1200)
				maxpkt = 32;
		}

		maxpktreq = maxpkt;

		if (numpcbufsarg && validxtproto) {
			for (i = 0; i < MAXPCHAN; i++)
				pconvs[i].freepkts += numpcbufsarg - numpcbufs;
			numpcbufs = numpcbufsarg;
			maxpktreq = maxpktreq * numpcbufs / NPCBUFS;
			if (maxpktreq > MAXOUTDSIZE) {
				/* will be trouble if don't have modified */
				/*    terminal! (at least 730) */
				maxpktreq = MAXOUTDSIZE;
			}
		}
	}

	if ((maxpktreq == 1) || (maxpktreq > MAXPKTDSIZE)) {
		Debug(DEBINFO, "requesting maxpkt of %d\n", maxpktreq);
		agentcom(0, A_JXTPROTO, maxpktreq, 0, 0, 0);
	}

	(void) rcfile();
}

int
pinit(channels)
	register int	channels;
{
	register int	i;

	if ( channels > MAXPCHAN )
		return -1;

	if ( Pscanrate == 0 || Prtimeout == 0 || Pxtimeout == 0 )
	{
		Pscanrate = PSCANRATE;
		for (i = 0; speeds[i].speed; i++) {
			/* use 1200 baud value for startup */
			if (speeds[i].speed <= B1200)
				break;
		}
		Pxtimeout = speeds[i].xtimeout;
		Prtimeout = speeds[i].rtimeout;
	}

	pconvsend = &pconvs[channels];

	while ( channels-- > 0 )
	{
		register Pch_p	pcp = &pconvs[channels];
	
		pcp->nextpkt = pcp->pkts;
		pcp->freepkts = numpcbufs;
	
		for ( i = 0; i < MAXNPCBUFS ; i++ )
		{
			static Ph_t	Zheader;
	
			pcp->pkts[i].state = PX_NULL;
			pcp->pkts[i].timo = 0;
			pcp->pkts[i].pkt.header = Zheader;
		}
	}

	for ( i = 0 ; i < 2*SEQMOD ; i++ )
		pseqtable[i] = i%SEQMOD;

#ifdef USE_SYSV_SIGNALS
	(void) sigset(SIGALRM, ptimeout);
#else
	(void) signal(SIGALRM, ptimeout);
#endif
	Ptflag = 0;
	return 0;
}
