/*
 *	Enhancements by David Dykstra at AT&T.
 *	This source code may be freely distributed.
 */

/*
**	Send data to channel
**
**	Assumes count <= MAXPKTDSIZE
**	      & channel <= NLAYERS
**
**	Returns -1 if output queue full,
**		else value of (*Pxfuncp)().
*/

#include	"common.h"
#include	"pconfig.h"
#include	"proto.h"
#include	"packets.h"
#include	"pstats.h"
#include	"errno.h"

extern int	numpcbufs;
extern int	crc();
extern char	*sys_errlist[];
extern int	errno;
extern int	networkxt;
extern struct layer layer[];

int
psend(channel, bufp, count)
	int		channel;
	char *		bufp;
	int		count;
{
	register int	i;
	register Pkt_p	pkp;
	register Pch_p	pcp;
	register Pks_p	psp;

	if (networkxt) {
		/* netpsend requires a buffer with extra space at the front */
		char netbuf[NETHDRSIZE+MAXOUTDSIZE];
		bcopy(bufp, &netbuf[NETHDRSIZE], count);
		return(netpsend(channel, &netbuf[NETHDRSIZE], count));
	}

	pcp = &pconvs[channel];
	pcp->freepkts = 0;

	Debug(DEBDETAIL, "psend: (%d %x %d)\n", channel, bufp, count);
	for(pkp=0,psp=pcp->nextpkt,i=numpcbufs; i--; ) {
		if ( psp->state != PX_WAIT )
			if ( pkp )
				pcp->freepkts++;
			else {
				pkp = &psp->pkt;
				psp->state = PX_WAIT;
				psp->timo = Pxtimeout;
				if(!Ptflag) {
					Ptflag++;
					(void)alarm(Pscanrate);
				}
			}
		if(++psp >= &pcp->pkts[numpcbufs])
			psp = pcp->pkts;
	}
	if(pkp == 0) {
		Debug(DEBERROR, "psend: packet allocation failure\n");
		return -1;
	}

	pkp->header.ptyp = 1;
	pkp->header.cntl = 0;
	pkp->header.seq = pcp->xseq++;
	pkp->header.channel = channel;

	bcopy(bufp, pkp->data, count);

#	ifdef	PSTATISTICS
	pstats[PS_XBYTES].count += count;
#	endif

	pkp->header.dsize = count;
	count += sizeof(Ph_t);

	(void)crc((Pbyte *)pkp, count, 1);
	count += EDSIZE;
	((Pks_p)pkp)->size = count;

	plogpkt((char *)pkp, count, PLOGOUT, 0);
	return (*Pxfuncp)(Pxfdesc, (char *)pkp, count);
}

/*
 * NOTE: the buffer passed to netpsend must have NETHDRSIZE extra bytes
 *    before bufp!
 */

int
netpsend(channel, bufp, count)
	int		channel;
	char *		bufp;
	int		count;
{
	struct layer *chanp = &layer[channel];

	*--bufp = (char) count;
	*--bufp = (char) (count >> 8);

	*--bufp = 0x40 | (channel & 0x7);
	if (!(chanp->flags & NONETFLOW)) {
		if (chanp->bytesent == 0)
			*bufp |= 0x20;
		*bufp |= 0x10;
		chanp->bytesent += count;
	}

	plogpkt(bufp, count + NETHDRSIZE, PLOGOUT, 0);
#	ifdef	PSTATISTICS
	PSTATS(PS_XPKTS);
	pstats[PS_XBYTES].count += count;
#	endif
	return (*Pxfuncp)(Pxfdesc, bufp, count + NETHDRSIZE);
}

int
check_write(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	int ret;
	int prevbytes = 0;
	int try = 0;

	while (1) {
		Debug(DEBDETAIL, "check_write %d (%d %x %d)\n", ++try, 
							    fd, buf, nbytes);
		ret = write(fd, buf, nbytes);

		if (ret == -1) {
			if (errno != EINTR) {
				int sve = errno;
				Debug(DEBERROR,
					"check_write: error writing: %s\n",
							sys_errlist[errno]);
				errno = sve;
				break;
			}
			Debug(DEBDETAIL,
				"check_write: EINTR while writing, retrying\n");
		}
		else {
			if (fd == 0)
				DebugRaw(1, buf, ret);

			if (ret < nbytes) {
				Debug(DEBINFO,
				    "check_write: wrote only %d bytes\n",ret);
				nbytes -= ret;
				buf += nbytes;
				prevbytes += ret;
			}
			else {
				ret += prevbytes;
				break;
			}
		}
	}
	Debug(DEBDETAIL, "check_write: returning %d\n", ret);
	return(ret);
}

/* allow for fully encoded full XT packet */
#define MAXWRITEPKTSIZE	((((sizeof(Ph_t)+MAXOUTDSIZE+EDSIZE)*4)/3)+2)
#define WRITEBUFSIZE	(MAXWRITEPKTSIZE*10)
static char writefd = -1;
static char writebuf[WRITEBUFSIZE];
static int writenbytes = 0;

void
flush_write()
{
	if (writenbytes > 0) {
		Debug(DEBDETAIL, "flushing %d bytes\n", writenbytes);
		(void) check_write(writefd, writebuf, writenbytes);
		writenbytes = 0;
	}
}

int
buf_write(fd, buf, nbytes)
{
	Debug(DEBDETAIL, "buf_write (%d %x %d)\n", fd, buf, nbytes);
	if (fd != writefd) {
		flush_write();
		writefd = fd;
	}
	if (nbytes > MAXWRITEPKTSIZE) {
		flush_write();
		return(check_write(fd, buf, nbytes));
	}
	if (writenbytes > (WRITEBUFSIZE-MAXWRITEPKTSIZE))
		flush_write();
	bcopy(buf, &writebuf[writenbytes], nbytes);
	writenbytes += nbytes;
	return(nbytes);
}

int
enc_write(d, buf, nbytes)
int d;
unsigned char *buf;
int nbytes;
{
	register int i;
	register unsigned char *fp, *tp;
	unsigned char c1, c2, c3;
	unsigned char encbuf[(((sizeof(Ph_t)+MAXOUTDSIZE+EDSIZE)*4)/3)+2];

	for (fp = buf, tp = encbuf, i = 0; i < nbytes; i += 3) {
		c1 = *fp++;
		if (i+1 < nbytes)
			c2 = *fp++;
		if (i+2 < nbytes)
			c3 = *fp++;
		*tp++ = 0x40 | (((int)(c1&0xc0))>>2) | (((int)(c2&0xc0))>>4) |
					(((int)(c3&0xc0))>>6);
		*tp++ = 0x40 | (c1&0x3f);
		if (i+1 < nbytes)
			*tp++ = 0x40 | (c2&0x3f);
		if (i+2 < nbytes)
			*tp++ = 0x40 | (c3&0x3f);
	}
	encbuf[0] &= 0x3f; 	/* start of packet indicator */
	return(buf_write(d, encbuf, (tp - encbuf)));
}
