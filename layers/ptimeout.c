/*
 *	Enhancements by David Dykstra at AT&T.
 *	This source code may be freely distributed.
 */

/*
**	Process timeouts for packets
*/

#include	"common.h"
#include	"pconfig.h"
#include	"proto.h"
#include	"packets.h"
#include	"pstats.h"
#include	<signal.h>
#ifdef USE_FILIO
#include	<sys/filio.h>
#else
#ifdef USE_TERMIO_H
#include	<sys/termio.h>
#else
#include	<sgtty.h>
#endif
#endif

extern int numpcbufs;
extern int DebugTimeout;

void
ptimeout(sig)
int sig;
{
	register Pch_p	pcp;
	register Pks_p	psp;
	register int	i;
	register int	retrys;

	DebugTimeout = 1;
	Debug(DEBDETAIL, "ptimeout begins\n");

	Ptflag = 0;

	if ( precvpkt.timo > 0 ) {
		int nc = 0;
		Debug(DEBDETAIL, "ptimeout: precvpkt.timo > 0\n");
		(void) ioctl(0, FIONREAD, &nc);
		if (nc) {
			Debug(DEBDETAIL,
				"delaying recv timeout because data pending\n");
			precvpkt.timo += Pscanrate;
		}
	}
	if (precvpkt.timo > 0 && ++Ptflag && (precvpkt.timo -= (short) Pscanrate) <= 0)
	{
		precvpkt.state = PR_NULL;
		PSTATS(PS_RECVTIMO);
		plogpkt((char *)&precvpkt.pkt, precvpkt.size, PLOGIN,
				pstats[PS_RECVTIMO].descp);
	}

	for (pcp=pconvs,retrys=0;pcp<pconvsend && retrys < MAXTIMORETRYS;pcp++)
		for ( psp = pcp->nextpkt, i = numpcbufs ; i-- ; )
		{
			if (psp->timo > 0)
				Debug(DEBDETAIL,
					"ptimeout: psp->timo > 0 chan %d\n",
						pcp - pconvs);
			if (psp->timo>0&& ++Ptflag&&(psp->timo -= (short) Pscanrate)<=0)
			{
				psp->timo = Pxtimeout;
				(*Pxfuncp)(Pxfdesc,(char *)&psp->pkt,psp->size);
				PSTATS(PS_TIMOPKT);
				plogpkt((char *)&psp->pkt, psp->size, PLOGOUT,
						pstats[PS_TIMOPKT].descp);
				if ( ++retrys >= MAXTIMORETRYS )
					break;
			}
			if ( ++psp >= &pcp->pkts[numpcbufs] )
				psp = pcp->pkts;
		}

#ifndef USE_SYSV_SIGNALS
	/* re-set signal handler in case signals are system V compatible */
	/* Not needed if we're actually using USE_SYSV_SIGNALS because then */
	/*   we use sigset which always resets the signal handler */
	(void) signal(sig, ptimeout); 
#endif

	if ( Ptflag )
		(void)alarm(Pscanrate);

	Debug(DEBDETAIL,"ptimeout ends\n");
	DebugTimeout = 0;
}
