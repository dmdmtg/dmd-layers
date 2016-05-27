/*
 *	Enhancements by David Dykstra at AT&T.
 *	This source code may be freely distributed.
 */

/*
 * This module supports two distinct protocols. The "network" protocol
 * is intended for use over end-to-end error correcting networks such
 * as TCP/IP. Network xt protocol relies on the underlying network to
 * provide error detection and correction. The "regular" xt protocol is
 * the original xt protocol which was designed for use over RS232.
 * Regular xt protocol provides its own error detection and correction.
 *
 * Layers asks the windowing terminal which protocol to use by sending an
 * A_XTPROTO agent command. Layers then makes the final decision about
 * which protocol to use based upon the windowing terminal response and
 * it's command line options. The layers command informs the terminal of
 * it's decision through a JXPROTO message.
 *
 * Regular xt packet format is as follows:
 *
 *	Byte 1: 1                 : 1
 *		control flag      : 1
 *		channel number    : 3
 *		sequence number   : 3
 *
 *	Byte 2: size              : 8
 *
 *	Size bytes of data
 *
 *	Two CRC bytes
 *
 * The first bit of the first byte is always 1. The next bit is a
 * control flag with identifies ACK, NAK, UNBLK control packets. The
 * next 3 bits are the xt channel number, and the last 3 bits are
 * sequence number.
 *
 * The second byte is packet data size. In the original xt protocol,
 * maximum packet data size was always 32. With this newer protocol,
 * the maximum packet data size is still 32 from the terminal to the
 * host, but it can be up to 252 from the host to the terminal. Larger
 * packets will only be used if the layers command, through the A_XTPROTO
 * agent command, determines that the terminal being talked to has the new
 * larger packet firmware which can handle larger packets.
 *
 * Network xt packet format is as follows:
 *
 *	Byte 1: 0                 : 1
 *		1                 : 1
 *		reset flow control: 1
 *		flow control flag : 1
 *		ACK flag          : 1
 *		channel number    : 3
 *
 *	Byte 2: size high bits    : 8
 *
 *	Byte 3: size low bits     : 8
 *
 *	Size bytes of data
 *
 * In the first byte, the first bit is always zero. This differentiates
 * a network xt packet from a regular xt packet because this bit is
 * always 1 in regular xt.
 *
 * The second bit of the first byte is always 1. This is used as a
 * sanity check, and will also allow the possibility of other new
 * packet types in the future. The next bit is unused.
 *
 * The next bit is the reset flow control flag. It is used to inform
 * the terminal to reset it's flow control count. This bit will be
 * set whenever a packet is sent when chanp->bytesent == 0. In
 * particular, it is sent in the first packet after a channel is
 * opened to initialize the terminals concept of bytes sent.
 * Another special case is that it is possible for a channel to
 * be closed and then reopened.  It also happens whenever all data
 * sent has already been acked; then it has no effect because the
 * count in the terminal was already zero.
 *
 * The next bit is the flow control flag. This bit identifies a flow
 * control packet which expects an eventual ACK response from the
 * receiving side.
 *
 * The next bit is the ACK flag. The ACK flag specifies that the packet
 * is a network xt high/low water flow control ACK packet. An ACK
 * packet contains no data, so the two size bytes are used to specify
 * the number of characters being ACK'ed rather than the number of data
 * bytes.
 *
 * The last three bits of the first byte are channel number. The usage
 * of channel number in network xt is identical to the equivalent field
 * in regular xt.
 *
 * Size is 16 bits in network xt. Following the 2 size bytes are the number
 * of bytes of data specified by the size field. Network xt has no CRC bytes.
 */

#include	"common.h"
#include	"pconfig.h"
#include	"proto.h"
#include	"packets.h"
#include	"pstats.h"


extern int	numpcbufs;
extern int	crc();
static void	Reply(), Control();
static int	Retry();

struct Pktstate precvpkt;
static Pkt_p	Pkp;
static char *	pbufp;
static char *	Sbufp;
static short	dcount;
static short	Scount;

#define	Pkt	precvpkt.pkt
#define	State	precvpkt.state
#define	Timo	precvpkt.timo
#define Size	precvpkt.size
#define	Header	Pkt.header
#define	Channel	Header.channel
#define	Ptyp	Header.ptyp
#define	Cntl	Header.cntl
#define	Seq	Header.seq
#define	Dsize	Header.dsize
#define	Data	Pkp->data
#define	Nextseq	pconvs[Channel].rseq
#define ENC_ENABLE	2

extern int rd_enabled;
extern int networkxt;

void
precv(bufp, count)
	unsigned char *bufp;
	int	count;
{
	register unsigned char *cp1, *cp2;
	register int i;
	int		haveheader = 0;
	static   int ct = 6;
	static   char hibits;
	extern   int Loadtype;
	
	DebugRaw(0, bufp, count);

	if (Loadtype == HEX_LOAD) {
		/* this is hexdecode */
		for (i = 0, cp1 = cp2 = bufp; i < count; i++, cp2++) {
			if (*cp2 < 0x40 && ((*cp2 & 0xe0) != 0x20))
				continue;
			ct += 2;
			if (((*cp2 & 0xe0) == 0x20) || (ct == 8)) {
				ct = 0;
				hibits = *cp2;
			}
			else
				*cp1++ = (*cp2 & 0x3f) | ((hibits << ct) & 0xc0);
		}
		count = cp1 - bufp;
	}

	while ( count )
	{
		switch ( State )
		{
		 /* In the middle of processing a network xt packet */
		 case PR_NETSIZE1:
		 case PR_NETSIZE2:
		 case PR_NETDATA:
		 case PR_NETERROR:
			netprecv(&bufp, &count);
			continue;

		 case PR_NULL:
			Pkp = (Pkt_p)bufp;
			haveheader++;
			((Pbyte *)&Header)[0] = *bufp++;
			count--;
			if ( Ptyp != 1 )
			{
				if (networkxt) {
					State = PR_NETNULL;
					bufp--;
					count++;
					netprecv(&bufp, &count);
					continue;
				}

				PSTATS(PS_BADHDR);
				plogpkt((char *) Pkp, 1, PLOGIN,
						pstats[PS_BADHDR].descp);
				break;
			}
			Timo = Prtimeout;
			Size = 1;
			if ( !Ptflag )
			{
				Ptflag++;
				(void)alarm(Pscanrate);
			}
			State = PR_SIZE;
			continue;

		 case PR_SIZE:
			((Pbyte *)&Header)[1] = *bufp++;
			count--;
			Size++;
			Scount = Dsize;
			if ( Scount > MAXPKTDSIZE )
			{
				PSTATS(PS_BADSIZE);
				plogpkt((haveheader?Pkp:&Pkt), 2,
					PLOGIN, pstats[PS_BADSIZE].descp);
				break;
			}
			dcount = Scount + EDSIZE;
			if ( dcount <= count && haveheader )
			{
				/* Don't move data */
				Sbufp = (char *) bufp;
				bufp += dcount;
				count -= dcount;
				Size += dcount;
				goto check;
			}
			Pkp = &Pkt;
			Sbufp = (char *)Data;
			pbufp = Sbufp;
			State = PR_DATA;
			continue;

		 case PR_DATA:
			*pbufp++ = *bufp++;
			count--;
			Size++;
			if ( --dcount > 0 )
				continue;

check:
			Timo = 0;  /* complete packet received */

			/** Now at CRC **/


			if ( crc((uchar *)Pkp, (int)(Scount+sizeof(Ph_t)), 0) )
			{
				PSTATS(PS_BADCRC);
				plogpkt((char *) Pkp, 
					(int)(Scount+sizeof(Ph_t)+EDSIZE),
					    PLOGIN, pstats[PS_BADCRC].descp);
			}
			else
			{
				plogpkt((char *) Pkp, 
					(int)(Scount+sizeof(Ph_t)+EDSIZE),
							PLOGIN, 0);
				if ( Cntl )
					Control();
				else
				{
					Pcdata = (uchar)0;
					if ( Seq == Nextseq )
					{
						if ( ( Scount != 0 ) && (*Prfuncp)(Channel, Sbufp, Scount) )
						{
							PSTATS(PS_BUSY);
							/* Better to let this timeout,
							** as a following sequence will
							** generate a second retransmission
							Reply(NAK);
							*/
						}
						else
						{
							Nextseq = (Nextseq+1) & (SEQMOD-1);	/* NB rseq is a byte, not a bit field, for efficiency */
							PSTATS(PS_RPKTS);
#							ifdef	PSTATISTICS
							pstats[PS_RBYTES].count += Scount;
#							endif
							pconvs[Channel].cdata[Seq] = Pcdata;
							Reply(ACK);
						}
					}
					else
					if ( Retry() )
					{
						PSTATS(PS_RDUP);
						Reply(ACK);
						pdumphist(pstats[PS_RDUP].descp);
					}
					else
					{
						PSTATS(PS_OUTSEQ);
						Reply(NAK);
						pdumphist(pstats[PS_OUTSEQ].descp);
					}
				}
			}
		}

		Timo = 0;
		Size = 0;
		State = PR_NULL;
	}
}


/*
 * Handle network xt receive data
 */

/*
 * 1028 was the maximum allowed receive buffer size on streams XT,
 *   I think because 1024, the data portion, was the size of streams buffers.
 * I don't it's an absolute limit but leave it there for compatibility
 *   with streams XT.
 */
char netinbuf[NETHDRSIZE+1+1024];
int netindex;

netprecv(bufpp, countp)
unsigned char **bufpp;
int *countp;
{
	unsigned char *bufp = *bufpp;
	int count = *countp;

	int n;

	static char netack = 0;
	static char inchan;
	static int insize, incount;

	extern struct layer layer[];

	while (count > 0)
	{
		switch (State)
		{
		case PR_NETNULL: 
			/* New packets are expected in this state */

			/* Check that the top bits are "01". */
			if ((*bufp & 0xc0) != 0x40) {
				/* This can happen if someone turns their
				 *   terminal off in layers or if firmware
				 *   crashes.  After this, the only symptom
				 *   will be the xt session hanging.
				 */
				State = PR_NETERROR;
				Debug(DEBERROR,
					"Bad network XT header byte %x\n",
						*bufp);
				break;
			}

			/* Check if this is a flow control ACK packet */
			if(*bufp & 0x8)
				netack = 1;

			inchan = *bufp & 0x7;
			State = PR_NETSIZE1;
			netinbuf[0] = *bufp;
			netindex = 1;
			bufp++;
			count--;
			break;

		case PR_NETSIZE1: 
			/* Get the first size byte */
			netinbuf[netindex++] = *bufp;
			insize = (*bufp++) << 8;
			count--;
			State = PR_NETSIZE2;
			break;

		case PR_NETSIZE2:
			/* Get the second size byte */
			netinbuf[netindex++] = *bufp;
			insize |= *bufp++;
			count--;

			if (netack) {
				/* Process flow control ACK packet. Size is
				 * number of bytes which are being acknowledged,
				 * and there is no data part.
				 */
				struct layer *chanp = &layer[inchan];

				netack = 0;

				/* If insize is greater than bytesent,
				 * assume that bytesent has been reset
				 * and this is a needless ACK sent
				 * by the terminal because it has not
				 * received a packet with the reset flow
				 * control flag set yet (see "reset flow
				 * control packet" in the big comment at
				 * the beginning of this file). Just set
				 * bytesent to 0. The "reset flow control
				 * bit" will sync things back up
				 * eventually. The only bad effect of this
				 * is that the host could end up sending
				 * some extra characters to the terminal
				 * before things sync up, but the problem
				 * is obscure and this effect is not
				 * critical
				 */
				if (insize > chanp->bytesent) {
					chanp->bytesent = 0;
				}
				else {  /* Normal case: decrement bytesent */
					chanp->bytesent -= insize;
				}

				Debug(DEBDETAIL,
				    "NETACK: bytesent chan %d -= %d to %d\n",
					inchan, insize, chanp->bytesent);

				/* enable the channel */
				SET_FD(chanp->fd, rd_enabled);

				plogpkt(netinbuf, NETHDRSIZE, PLOGIN, 0);

				PSTATS(PS_RACK);

				State = PR_NULL;
				goto out;
			}

			incount = insize;

			/*
			 * insize includes the command byte but not the
			 *   header byte or size bytes.
			 */

			if (insize > (sizeof(netinbuf)-NETHDRSIZE)) {
				State = PR_NETERROR;
				Debug(DEBERROR, "Input packet too large: %d\n",
								insize);
				break;
			}

			State = PR_NETDATA;
			if (insize > 0)
				break;
			/* else fall through, no data expected */

		case PR_NETDATA:
			/* Data receiving mode */
			n = count > incount ? incount : count;
			bcopy(bufp, &netinbuf[netindex], n);
			bufp += n;
			count -= n;
			incount -= n;
			netindex += n;

			if (incount > 0)
				/* haven't received it all yet */
				break;

			plogpkt(netinbuf, NETHDRSIZE+insize, PLOGIN, 0);
			PSTATS(PS_RPKTS);

			if (insize > 0) {
				(void) (*Prfuncp)(inchan, &netinbuf[NETHDRSIZE],
							insize);
#				ifdef	PSTATISTICS
				pstats[PS_RBYTES].count += insize;
#				endif
			}

			State = PR_NULL;
			goto out;

		case PR_NETERROR:
			/* If network xt gets bad input, there is a problem.
			 * Possibly a firmware bug. Possibly someone went
			 * through a network to a host, cu'ed to another
			 * machine over RS232, and executed layers on that
			 * remote machine. Either way, it is an error and
			 * I don't think that code to attempt to re-sync
			 * is justified. So, instead I only attempt to
			 * detect errors, and then eat any additional
			 * input.
			 */
			Debug(DEBERROR, "NETERROR eating %d bytes\n", count);
			plogpkt(bufp, count, PLOGIN, "neterror");
			count = 0;
			break;
		default:
			Debug(DEBERROR, "illegal netprecv state %d!\n", State);
			count = 0;
		}
	}

out:
	*countp = count;
	*bufpp = bufp;
}

/*
**	Deal with control packet
*/

static void
Control()
{
	register Pch_p	pcp = &pconvs[Channel];
	register Pks_p	psp = pcp->nextpkt;
	register Pbyte *lastseqp = &pseqtable[Seq+SEQMOD];
	register Pbyte *seqp = lastseqp - (numpcbufs-1);
	register int	hit = 0;

	if ( Scount == 0 )
		goto ack;

	switch ( Data[0] )
	{
	 case ACK:
		/** This and all lesser sequenced packets ok **/
ack:
		do
		{
			if ( *seqp == psp->pkt.header.seq )
			{
				if ( psp->state != PX_WAIT )
				{
#					if PSTATISTICS
					if ( psp->state != PX_OK )
					{
						PSTATS(PS_BADXST);
						pdumphist(pstats[PS_BADXST].descp);
					}
#					endif
				}
				else
				{
					psp->state = PX_OK;
					psp->timo = 0;
					hit++;
				}
				if ( ++psp >= &pcp->pkts[numpcbufs] )
					psp = pcp->pkts;
			}
		} while
			( ++seqp <= lastseqp );
		if ( hit )
		{
			pcp->nextpkt = psp;
			pcp->freepkts += hit;
#			ifdef	PSTATISTICS
			PSTATS(PS_RACK);
			pstats[PS_XPKTS].count += hit;
			if ( hit > 1 ) {
				PSTATS(PS_LOSTACK);
				pdumphist(pstats[PS_LOSTACK].descp);
			}
#			endif
			break;
		}
#		if PSTATISTICS
		PSTATS(PS_BADACK);
		pdumphist(pstats[PS_BADACK].descp);
#		endif
		return;

	 case NAK:
		/** Retransmit this and all lesser sequenced packets **/

		do
		{
			if ( *seqp == psp->pkt.header.seq )
			{
				if ( psp->state != PX_WAIT )
				{
#					if PSTATISTICS
					if ( psp->state != PX_OK )
					{
						PSTATS(PS_BADXST);
						pdumphist(pstats[PS_BADXST].descp);
					}
#					endif
				}
				else
				{
					psp->timo = Pxtimeout;
					(void)(*Pxfuncp)(Pxfdesc, (char *)&psp->pkt, psp->size);
					PSTATS(PS_NAKPKT);
					hit++;
					plogpkt((char *)&psp->pkt, psp->size,
							PLOGOUT, 0);
				}
				if ( ++psp >= &pcp->pkts[numpcbufs] )
					psp = pcp->pkts;
			}
		} while
			( ++seqp <= lastseqp );
		if ( !hit )
		{
			PSTATS(PS_BADNAK);
			pdumphist(pstats[PS_BADNAK].descp);
			return;
		}
		PSTATS(PS_RNAK);
		pdumphist(pstats[PS_RNAK].descp);
		break;

	 case PCDATA:
		break;

	 default:
		PSTATS(PS_BADCNTL);
		pdumphist(pstats[PS_BADCNTL].descp);
		return;
	}

	if ( --Scount > 0 )
	{
		(*Prcfuncp)(Channel, Sbufp+1, Scount);
#		ifdef	PSTATISTICS
		pstats[PS_RCBYTES].count += Scount;
#		endif
	}
}



/*
**	Reply to good packet
*/

static void
Reply(ctl)
	Pbyte		ctl;
{
	register int	count;

#ifdef PSTATISTICS
	switch(ctl)
	{
	case ACK:
		PSTATS(PS_XACK);
		break;
	case NAK:
		PSTATS(PS_XNAK);
		break;
	}
#endif
	Cntl = 1;
	if ( Pcdata )
	{
		PSTATS(PS_XCBYTES);
		count = 2;
		Pkt.data[1] = Pcdata;
		Pkt.data[0] = ctl;
	}
	else
	if ( ctl != ACK )
	{
		count = 1;
		Pkt.data[0] = ctl;
	}
	else 
		count = 0;
	Dsize = count;
	count += sizeof(Ph_t);

	(void)crc((Pbyte *)&Pkt, count, 1);
	count += EDSIZE;

	(void)(*Pxfuncp)(Pxfdesc, (char *)&Pkt, count);
	plogpkt((char *) &Pkt, count, PLOGOUT, 0);
}



/*
**	Non trivial sequence number validation:
**	 is this a valid retransmission?
*/

static int
Retry()
{
	register Pbyte *lastseqp = &pseqtable[Nextseq+SEQMOD-1];
	register Pbyte *seqp = lastseqp - (numpcbufs-1);

	do
		if ( *seqp == Seq )
		{
			Pcdata = pconvs[Channel].cdata[Seq];
			return 1;
		}
	while
		( ++seqp <= lastseqp );

	return 0;
}

