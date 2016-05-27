/*
 * This file added by Dave Dykstra on 7-29-93.
 * This source code may be freely distributed.
 */

#include	"common.h"
#include	<sys/types.h>
#include	<time.h>
#include	<stdio.h>
#include	"proto.h"
#include	"packets.h"
#define	DECLARE				/* Define statistics array here */
#include	"pstats.h"

extern int DebugLevel;
extern FILE *DebugFile;

extern int ptracing;
extern int numpcbufs;
extern int maxpkt;
extern int highwater;
extern int networkxt;

#ifdef	PTRACE

/* 18 is the space used for columns between "DIR" and "DATA" */
#define MINPRINTMSG 18
/*
 * 44 is the space for DATA column up to column 79. Subtract 12
 *   for " [NNNN more]" and 3 in case the last one is hex.
 */
#define MAXPRINTDATA (44-12-3)
/* 3 bytes are used for network XT header */
#define MAXXTHEADER 3
/* maximum log is maximum print data plus the xt header */
#define MAXLOGDATA (MAXPRINTDATA+MAXXTHEADER)

struct logstruct {
	char ident;				/* PLOGIN or PLOGOUT */
	unsigned short size;			/* Packet size */
	char *badmsg;				/* message for bad packet */
	time_t stamp;				/* timestamp */
	char data[MAXLOGDATA];			/* trace data */
} log[PKTHIST];

static struct logstruct *inlog	= log;
static struct logstruct *outlog	= log;
static short logflag;

void
plogpkt(data, size, ident, badmsg)
	char *data;
	int size;
	char ident;
	char *badmsg;
{
	int n;
	time_t stamp;

	if (!ptracing)
		return;

	bcopy((char *) data, inlog->data, sizeof(inlog->data));
	inlog->ident = ident;
	inlog->size = size;
	inlog->badmsg = badmsg;
	inlog->stamp = stamp = time((time_t *) 0);
	if ( ++inlog >= &log[PKTHIST] )
		inlog = log;
	if ( logflag )
	{
		outlog = inlog;
		logflag = 0;
	}
	if ( inlog == outlog )
		logflag++;

	if (((DebugLevel & DEBPTRACE) != 0) ||
			((badmsg != 0) && ((DebugLevel & DEBPERROR) != 0))) {
		fptracepkt(DebugFile, data, size, ident, badmsg, stamp);
		fflush(DebugFile);
	}
}
#endif	PTRACE

tracebyte(fp, ch)
	FILE *	fp;
	unsigned char	ch;
{
	if ((ch >= ' ') && (ch <= '~')) {
		fprintf(fp, "%c", ch);
		return(1);
	}
	else {
		fprintf(fp, "<%02x>", ch);
		return(4);
	}
}

tracebad(fp, badmsg, data, size)
	FILE *	fp;
	char *	badmsg;
	unsigned char *	data;
	int	size;
{
	int cols = MAXPRINTDATA;

	fprintf(fp, "%*s ", -MINPRINTMSG, badmsg);
	if (strlen(badmsg) > MINPRINTMSG)
		cols -= (strlen(badmsg) - MINPRINTMSG);

	while((cols > 0) && (size-- > 0))
		cols -= tracebyte(fp, *data++);
	
	return(size);
}

tracelabels(fp)
	FILE *	fp;
{
	if (networkxt)
		fprintf(fp, "\n  TIME      DIR FLAGS CHAN  SIZE   DATA\n");
	else
		fprintf(fp, "\n  TIME      DIR C/D CHAN SEQ SIZE  DATA\n");
}

tracedatapart(fp, xmit, chan, data, size)
	FILE *	fp;
	int	xmit;
	unsigned char *	data;
	int	chan;
	int	size;
{
	int cols = MAXPRINTDATA;
	extern char *dmd_ctrllist[];
	extern int dmd_maxctrl;
	extern char *dmd_jioctllist[];
	extern int dmd_maxjioctl;

	if (xmit && (chan == 0) && ((int) (*data) <= dmd_maxjioctl)) {
		/* data to control channel starts with jioctl */
		fprintf(fp, "%s ", &dmd_jioctllist[*data][0]);
		cols -= strlen(&dmd_jioctllist[*data][0]) + 1;
		data++;
		size--;
	}
	if (!xmit && (size > 0) && ((int) (*data) >= 1) &&
			((int) (*data) <= dmd_maxctrl)) {
		/* data received always begins with a control byte */
		fprintf(fp, "%s ", &dmd_ctrllist[*data][2]); /* skip C_ */
		cols -= strlen(&dmd_ctrllist[*data][2]) + 1;
		data++;
		size--;
	}

	while((cols > 0) && (size-- > 0))
		cols -= tracebyte(fp, *data++);

	return(size);
}

traceregular(fp, xmit, data, size)
	FILE *	fp;
	int	xmit;
	unsigned char *	data;
	int	size;
{
	int isctrl;
	int chan;
	int seq;

	size -= 2; /* skip CRC */

	isctrl = (*data & 0100);
	chan = (*data & 070) >> 3;
	seq = *data & 07;
	fprintf(fp, " %c   %d    %d ", (isctrl ? 'C' : 'D'), chan, seq);
	data++;
	fprintf(fp, " %3d   ", *data++);
	size -= 2;
	if (isctrl) {
		if (size == 0) {
			/* ACK with no data */
			fprintf(fp, "ACK");
			return(0);
		}
		switch(*data)
		{
		case ACK:
			fprintf(fp, "ACK ");
			break;
		case NAK:
			fprintf(fp, "NAK ");
			break;
		default:
			fprintf(fp, "<%02x>", *data);
			break;
		}
		data++;
		size--;
	}
	return(tracedatapart(fp, xmit, chan, data, size));
}

tracenet(fp, xmit, data, size)
	FILE *	fp;
	int	xmit;
	unsigned char *	data;
	int	size;
{
	int isack;
	int chan;

	isack = *data & 0x08;
	chan = *data & 0x07;
	fprintf(fp, " %c%c    %d   ", ((*data & 0x20) ? 'R' : ' '), 
			((*data & 0x10) ? 'F' : ' '), chan);
	data++;
	fprintf(fp, " %4d   ", ((*data) << 8) + (*(data + 1)));
	data += 2;
	size -= 3;
	if (isack) {
		fprintf(fp, "FLOW CONTROL ACK");
		return(0);
	}
	return(tracedatapart(fp, xmit, chan, data, size));
}

fptracepkt(fp, data, size, ident, badmsg, stamp)
	FILE *	fp;
	char *	data;
	int	size;
	char	ident;
	char *	badmsg;
	time_t	stamp;
{
#ifdef	PTRACE
	int more;
	struct tm *loctime;

	loctime = localtime(&stamp);
	fprintf(fp, "%02d:%02d:%02d    %s ", loctime->tm_hour, loctime->tm_min,
		loctime->tm_sec, (ident == PLOGOUT) ? "Xmt" : "Rcv");

	if ((badmsg != NULL) && (*badmsg != '\0'))
		more = tracebad(fp, badmsg, data, size);
	else if ((*data & 0x80) == 0x80)
		more = traceregular(fp, (ident == PLOGOUT), data, size);
	else if ((*data & 0xc0) == 0x40)
		more = tracenet(fp, (ident == PLOGOUT), data, size);
	else
		more = tracebad(fp, "unrecognized protocol", data, size);

	if (more > 0)
		fprintf(fp, " [%d more]", more);
	fprintf(fp, "\n");
#endif	PTRACE
}

fpdumphist(fp)
	FILE *	fp;
{
#ifdef	PTRACE
	if (!ptracing) {
		ptracing = 1;
		fprintf(fp, "\nPacket tracing enabled\n");
		return;
	}
	tracelabels(fp);
	while ( outlog != inlog || logflag )
	{
		logflag = 0;
		fptracepkt(fp, outlog->data, outlog->size, outlog->ident,
					outlog->badmsg, outlog->stamp);
		if ( ++outlog >= &log[PKTHIST] )
			outlog = log;
	}
	fflush(fp);
#endif	PTRACE
}



void
pdumphist(s)
	char *	s;
{
	if ((DebugLevel & DEBPERROR) != 0) {
		fpdumphist(DebugFile);
		Debug(DEBPERROR, "Trace history dumped because of %s\n", s);
	}
}

void
fpsummary(fp)
FILE *fp;
{
#ifdef PSTATISTICS
	register int i;
	extern int Loadtype;

	if (fp == NULL)
		return;
	fprintf(fp, "\nPacket protocol statistics:\n");
	if (Loadtype == HEX_LOAD)
		fprintf(fp, " (hex encoding enabled)\n");
	if (networkxt) {
		fprintf(fp, " (network xt protocol)\n");
		fprintf(fp, " (transmit high water %d bytes)\n", highwater);
	}
	else
		fprintf(fp, " (%d byte max transmit packets)\n", maxpkt);
	if (numpcbufs > NPCBUFS)
		fprintf(fp, " (transmit %d packets before blocking)\n",
						numpcbufs);
	for (i = 0; i < PS_NSTATS ; i++) {
		if (pstats[i].count <= 0)
			continue;
		fprintf(fp, "%6ld %s\n",pstats[i].count ,pstats[i].descp);
	}
#endif	PSTATISTICS
	fflush(fp);
}
