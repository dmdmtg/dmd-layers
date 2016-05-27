/*
 *	Copyright (c) 1987 Keith Muller
 *	All Rights Reserved
 *
 *	The author make no claims as to the fitness or correctness of
 *	this software for any use whatsoever, and it is provided ``as is''
 *	without express or implied warranty. Any use of this software is
 *	at the user's own risk.
 *
 *	Enhancements by David Dykstra at AT&T.
 *	This source code may be freely distributed.
 */
#ifndef A_NEWLAYER
#include "hostagent.h"
#endif
#ifndef NOFILE
#include <sys/param.h>
#endif

/* Debug levels */
#define DEBERROR	0x01		/* non-protocol error */
#define DEBINFO		0x02		/* low-volume info messages */
#define DEBPERROR	0x04		/* protocol error */
#define DEBPTRACE	0x08		/* protocol trace */
#define DEBRTRACE	0x10		/* raw bytes to/from terminal trace */
#define DEBDETAIL	0x20		/* high-volume info messages */

#if NOFILE < 35
#define MAXFD		NOFILE-1
#else
#define MAXFD		32
#endif
/*
 * bsd compatibility defines
 */
#ifdef NO_BCOPY
#define bcopy(B1, B2, LENGTH) memcpy(B2, B1, LENGTH)
#define bzero(B, LENGTH) memset(B, 0, LENGTH)
#endif
#ifdef NO_INDEX
#define index strchr
#define rindex strrchr
#endif
/*
 * terminal sizes, pixel models
 */
#define BORD_630	8
#define YMAX_630	1018
#define XMAX_630	1018
#define YMIN_630	6
#define XMIN_630	6
#define BORD_620	10
#define YMAX_620	427
#define XMAX_620	654
#define YMIN_620	2
#define XMIN_620	2
#define BORD_5620	4
#define YMAX_5620	1016
#define XMAX_5620	792
#define YMIN_5620	8
#define XMIN_5620	8
/*
 * terminal sizes, character models
 */
#define VXMAX_620	81
#define VYMAX_620	25
#define VXMIN_620	0
#define VYMIN_620	0
#define WXMAX_620	123
#define WYMAX_620	24
#define WXMIN_620	3
#define WYMIN_620	2
#define VXMAX_615	133
#define VYMAX_615	25
#define VXMIN_615	0
#define VYMIN_615	0
#define WXMAX_615	133
#define WYMAX_615	25
#define WXMIN_615	80
#define WYMIN_615	24
/*
 * extract constants for character terminals
 */
#define GETX(x)		((x)/100)
#define GETY(x)		((x)%100)
/*
 * type fields in encoding string
 */
#define DMD_630		8
#define DMD_5620	7
#define DMD_615		4
#define DMD_620		0
/*
 * download  modes
 */
#define UNKNOWN_LOAD	-1
#define BINARY_LOAD	0
#define HEX_LOAD	1
/*
 * layers state flags
 */
#define AUTODELETE	0x01
#define DELETING	0x02
#define ACTIVE		0x04
#define SETWINSIZE	0x08
#define NONETFLOW	0x10
/*
 * agent command flags
 */
#define IN_PROG		0x01
#define COMMAND		0x02
/*
 * misc
 */
#define SELSEC		60L
#define SELUSEC		0L
#define SENDTERMID	"\033[c"
#define TERMIDSIZE	(sizeof("\033[?8;8;nc")-1)
#define SENDENC		"\033[F"
#define ENCDSIZE	(sizeof("\033[nF")-1)
#define PTFILE		"pltrace"
#define	LAYERSRC	".layersrc"
#define DSHELL		"/bin/sh"
#define SET_FD(fd, mask)	((mask) |= (1 << (fd)))
#define CLR_FD(fd, mask)	((mask) &= ~(1 << (fd)))
#define ZERO_FD(mask)		((mask) = 0)
#define ISSET_FD(fd, mask)	((mask) & (1 << (fd)))
#define ISZERO_FD(mask)		((mask) == 0)

/*
 * jioctl characters to be sent to DMD
 * these are what the dmd wants to see. defined like this so the host ioctl's
 * can be made whatever they need to be if required, but these are the values
 * sent to the DMD.
 */
#define JBOOT_CHAR	1
#define JTERM_CHAR	2
#define JMPX_CHAR	3
#define JTIMO_CHAR	4
#define JWINSIZE_CHAR	5
#define JTIMOM_CHAR	6
#define JZOMBOOT_CHAR	7
#define JAGENT_CHAR	9
#define JTRUN_CHAR	10
#define JXTPROTO_CHAR	11

/*
 * startup file keywords
 */
#define KEY_AUTOD	"autodelete"
#define KEY_ON		"on"
#define KEY_OFF		"off"
#define KEY_AUTOC	"autocurrent"
#define KEY_NEW		"new"
#define KEY_INTERACT	"interactive"
#define KEY_COMMAND	"command"
#define KEY_DELETE	"delete"
#define KEY_TOP		"top"
#define KEY_CURRENT	"current"
#define KEY_BOTTOM	"bottom"
#define KEY_RESHAPE	"reshape"
#define KEY_MOVE	"move"

/*
 * aglist request state
 */
#define AG_NEW		0
#define AG_SENT		1
#define AG_COMP		2
#define AG_DEAD		3
#define AG_PEND		4

/*
 * structure of an agent list on FIFO list
 */
struct aglist {
	struct agentrect ar;			/* agentrect cmd */
	unsigned long	time;			/* timestamp for dups */
	unsigned int	pid;			/* pid of requestor for ack */
	int	result;				/* result if AG_COMP */
	char	*buf;				/* cmd str for WINCMD */
	struct aglist *next;			/* ptr to next in agent chain */
	short 	state;				/* state of this agent req */
	short	flags;				/* Misc flags */
#ifdef USE_NAMED_PIPE
	int	xtfd;				/* fd for reply channel */
#endif
	int reqchan;				/* requesting channel */
};
#define AGNIL		(struct aglist *)0

/*
 * structure that describes each channel to the DMD
 */
struct layer {
	int	fd;				/* fd assoc with this channel */
	int	pgrp;				/* pgrp of this channel */
	short	flags;				/* misc flags */
	int	cols;				/* number of columns */
	int	rows;				/* number of rows */
	int 	xpixels;			/* number of x pixels */
	int	ypixels;			/* number of y pixels */
	char	tty[sizeof("/dev/")+ANLEN];	/* the pseudo tty if one */
	int	bytesent;			/* flow control counter for */
						/*  network XT protocol */
};
