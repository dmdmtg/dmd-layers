/* 	agent - send agent calls to a layers terminal. 	 	*/
/*	Written by David W. Dykstra at AT&T.			*/
/*	This source code may be freely distributed.		*/

char *version="@(#) agent May 16, 1994";

doexit(e)
int e;
{
    closeagent();
    exit(e);
}

#include <stdio.h>
#include <string.h>
#include <hostagent.h>

/* the six shorts are: command, channel, x1, y1, x2, y2 */
unsigned char sendbuf[sizeof(short)*6];
unsigned char recvbuf[sizeof(sendbuf)+1];
unsigned int sendbufidx = 0;
int verbose = 0;

#define ARG(N) ((sendbuf[(N)*2]<<8)+sendbuf[(N)*2+1])

#define HI(c) ((c) >> 8)
#define LO(c) ((c) & 0xff)

usage()
{
fprintf(stderr,
"Usage: agent [-clav] agentcall|\"!com\"|\"&com\" [chan|- [up to 4 args]]\n");
fprintf(stderr,
" agentcall: number, CURRENT, DELETE, TOP, BOTTOM, MOVE, RESHAPE, NEW,\n");
fprintf(stderr,
"                        NEWLAYER, ROMVERSION, or EXIT\n");
fprintf(stderr,
" Additional agentcalls for 730windows: CREATESCREEN, DESTROYSCREEN, NEXTSCREEN,\n");
fprintf(stderr,
"    PREVSCREEN, NEWHOST, EXITHOST, TILELAYER, UNTILELAYER,\n");
fprintf(stderr,
"    HOSTDIALER, DIALRESET, or DIALCONCAT\n");
fprintf(stderr,
" -c : display channel number\n");
fprintf(stderr,
" -l : determine if on logged-in layer and return true or false\n");
fprintf(stderr,
" -a : display return result in ascii (default for ROMVERSION)\n");
fprintf(stderr,
" -v : verbose mode (for debugging)\n");
fprintf(stderr,
" An agentcall of \"!com\" will run shell command in a window; if window\n");
fprintf(stderr,
"    coordinates are given, a new window will be created.\n");
fprintf(stderr,
" An agentcall of \"&com\" is like \"!com\" except agent doesn't wait for \n");
fprintf(stderr,
"    the command to complete.\n");
fprintf(stderr,
" Missing channel on BOTTOM, TOP, DELETE, or CURRENT defaults to current channel.\n");
fprintf(stderr,
" A channel number of \"-\" also defaults to the current channel.\n");
fprintf(stderr,
" Channel number required for MOVE, RESHAPE, NEW, NEWLAYER, and \"!com\";\n");
fprintf(stderr,
"    MOVE takes X and Y coordinate of upper left of window; RESHAPE, NEW,\n");
fprintf(stderr,
"    NEWLAYER, and \"!com\" take coordinates of upper left and lower right.\n");
fprintf(stderr,
" NEW, NEWLAYER, and \"!com\" with coordinates ignores channel number and\n");
fprintf(stderr,
"    allocates a free channel.\n");
fprintf(stderr,
" Only first character of agentcall required for commands that are one \"word\";\n");
fprintf(stderr,
"   first character of both \"words\" required for two-word commands.\n");
fprintf(stderr,
" Agentcalls can be in upper or lower case.\n");
fprintf(stderr, "%s\n", version);
doexit(1);
}

void
agenterrf(msg)
char *msg;
{
	fprintf(stderr, "agent: %s\n", msg);
}

main(argc, argv)
int argc;
char **argv;
{
    int i,n;
    char *p;
    int required;
    int supplyargs = 0;
    int chan0;
    int channum;
    int dispchan = 0;
    int dispascii = 0;
    char *command = 0;
    char agcall;
    int layerstype;
    int skipagent = 0;
    int getchanflag = 0;
    extern char * getagentanswer();

    setagenterrf(agenterrf);

    ++argv;
    if (--argc == 0)
	usage();

    while ((*argv)[0] == '-')
    {
	char *p;
	for (p = &((*argv)[1]); *p; p++) {
	    switch (*p)
	    {
	    case 'c':
		dispchan = 1;
		break;
	    case 'l':
		if (!inlayers())  {
		    printf("no, not in layers\n");
		    exit(1);
		}
		if (onloginlayer()) {
		    printf("yes, on logged-in layer\n");
		    exit(0);
		}
		printf("no, not on logged-in layer\n");
		exit(1);
	    case 'a':
		dispascii = 1;
		break;
	    case 'v':
		verbose = 1;
		break;
	    default:
		usage();
	    }
	}
	++argv;
	if (--argc == 0) {
	    if (!dispchan)
		usage();
	    else
		break;
	}
    }

    layerstype = inlayers();
    if (!layerstype) {
	fprintf(stderr, "agent: not in layers\n");
	exit(3);
    }

    if (verbose) {
	switch(layerstype)
	{
	case 1:
	    fprintf(stderr, "layers is character XT-driver based\n");
	    break;
	case 2:
	    fprintf(stderr, "layers is streams XT-driver based\n");
	    break;
	default:
	    fprintf(stderr, "layers is pseudo-tty based\n");
	    break;
	}
    }

    if ((argc == 0) && dispchan) {
	channum = getchan();
	printf("%d\n", channum);
	if (channum == -1) {
	    fprintf(stderr, "agent: getchan() failed\n");
	    if (layerstype > 2)
		fprintf(stderr, "You probably need to upgrade 'layers'.\n");
	    exit(1);
	}
	exit(0);
    }

    chan0 = openagent();
    if (chan0 == -1) {
	fprintf(stderr, "agent: openagent failed.\n");
	doexit(3);
    }

    switch(agcall = (*argv)[0])
    {
    case 'B':
    case 'b':
	n = A_BOTTOM;
	required = 1;
	supplyargs = 1;
	break;
    case 'C':
    case 'c':
	if ((strchr(*argv, 's') != 0) || (strchr(*argv, 'S') != 0)) {
	    n = A_SCRCREATE;
	    required = 0;
	}
	else {
	    n = A_CURRENT;
	    required = 1;
	    supplyargs = 1;
	}
	break;
    case 'D':
    case 'd':
	if ((strchr(*argv, 'c') != 0) || (strchr(*argv, 'C') != 0)) {
	    n = A_DIALCONCAT;
	    required = 1;
	}
	else if ((strchr(*argv, 's') != 0) || (strchr(*argv, 'S') != 0)) {
	    n = A_SCRDESTROY;
	    required = 0;
	}
	else if ((strchr(*argv, 'r') != 0) || (strchr(*argv, 'R') != 0)) {
	    n = A_DIALRESET;
	    required = 1;
	}
	else {
	    n = A_DELETE;
	    required = 1;
	    supplyargs = 1;
	}
	break;
    case 'E':
    case 'e':
	if ((strchr(*argv, 'h') != 0) || (strchr(*argv, 'H') != 0)) {
	    n = A_HOSTEXIT;
	    required = 1;
	}
	else {
	    n = A_EXIT;
	    required = 0;
	}
	break;
    case 'H':
    case 'h':
	if ((strchr(*argv, 'd') != 0) || (strchr(*argv, 'D') != 0)) {
	    n = A_SETDIAL;
	    required = 2;
	}
	else 
	    usage();
	break;
    case 'M':
    case 'm':
	n = A_MOVE;
	required = 3;
	break;
    case 'N':
    case 'n':
	if ((strchr(*argv, 'h') != 0) || (strchr(*argv, 'H') != 0)) {
	    n = A_HOSTNEW;
	    required = 2;
	}
	else if ((strchr(*argv, 'l') != 0) || (strchr(*argv, 'L') != 0)) {
	    n = A_NEWLAYER;
	    required = 5;
	}
	else if ((strchr(*argv, 's') != 0) || (strchr(*argv, 'S') != 0)) {
	    n = A_SCRNEXT;
	    required = 0;
	}
	else {
	    n = A_NEW;
	    required = 5;
	}
	break;
    case 'P':
    case 'p':
	if ((strchr(*argv, 's') != 0) || (strchr(*argv, 'S') != 0)) {
	    n = A_SCRPREV;
	    required = 0;
	}
	else
	    usage();
	break;
    case 'R':
    case 'r':
	if ((strchr(*argv, 'l') != 0) || (strchr(*argv, 'L') != 0)) {
	    /* undocumented, for use by relogin shell script */
	    n = A_RELOGIN;
	    required = 1;
	    supplyargs = 1;
	}
	else if ((strchr(*argv, 'v') != 0) || (strchr(*argv, 'V') != 0)) {
	    n = A_ROMVERSION;
	    required = 0;
	    dispascii++;
	}
	else {
	    n = A_RESHAPE;
	    required = 5;
	}
	break;
    case 'T':
    case 't':
	if ((strchr(*argv, 'l') != 0) || (strchr(*argv, 'L') != 0)) {
	    n = A_TILE;
	    required = 1;
	    supplyargs = 1;
	}
	else {
	    n = A_TOP;
	    required = 1;
	    supplyargs = 1;
	}
	break;
    case 'U':
    case 'u':
	if ((strchr(*argv, 'l') != 0) || (strchr(*argv, 'L') != 0)) {
	    n = A_UNTILE;
	    required = 1;
	    supplyargs = 1;
	}
	else
	    usage();
	break;
    case 'X':
    case 'x':
	/* undocumented, for use by xts and xtt shell scripts */
	if ((strchr(*argv, 's') != 0) || (strchr(*argv, 'S') != 0))
	    n = A_XTSTATS;
	else if ((strchr(*argv, 't') != 0) || (strchr(*argv, 'T') != 0))
	    n = A_XTTRACE;
	else
	    usage();
	required = 0;
	break;
    case '!':
    case '&':
	if (argc < 3) {
	    /* skip creating a new window */
	    skipagent = 1;
	    required = 1;
	}
	else
	    required = 5;
	n = A_NEWLAYER;
	command = &((*argv)[1]);
	break;
    default:
	if (sscanf(*argv, "%d", &n) != 1)
	    usage();
	required = 1;
	supplyargs = 1;
	break;
    }

    sendbuf[sendbufidx++] = HI(n);
    sendbuf[sendbufidx++] = LO(n);

    if (--argc == 0) {
	if ((required > 0) && supplyargs)
	    getchanflag = 1;
    }
    else {
	++argv;
	if (((*argv)[0] == '-') && ((*argv)[1] == '\0')) {
	    getchanflag = 1;
	    --argc;
	    ++argv;
	}
    }

    if (getchanflag) {
	if ((channum = getchan()) == -1)
	{
	    /* getchan() support added to pseudo-tty layers mid-to-late 1991 */
	    fprintf(stderr, "agent: %s%s\n",
	    	"getchan() failed; either provide channel number or",
			" upgrade 'layers'");
	    doexit(1);
	}
	sendbuf[sendbufidx++] = HI(channum);
	sendbuf[sendbufidx++] = LO(channum);
	required--;
    }

    while(argc--)
    {
	if (sscanf(*argv, "%d", &n) != 1)
	{
	    fprintf(stderr,"agent: %s is not a number\n",*argv);
	    usage();
	}
	if (!getchanflag) {
	    getchanflag = 1;
	    channum = n;
	}
	sendbuf[sendbufidx++] = HI(n);
	sendbuf[sendbufidx++] = LO(n);
	required--;
	++argv;
    }

    if (required > 0) 
    {
	if (supplyargs) {
	    while(required--) {
		sendbuf[sendbufidx++] = 0;
		sendbuf[sendbufidx++] = 0;
	    }
	}
	else {
	    fprintf(stderr,"agent: not enough args\n");
	    usage();
	}
    }
    if (skipagent)
	goto afteragent;

    if (verbose)
    {
	fprintf(stderr,
		"send command: %d, chan: %d, x1: %d, y1: %d, x2: %d, y2: %d\n",
			ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5));
	fprintf(stderr,"send buffer:\n");
	for (i = 0; i < sizeof(sendbuf); i++)
	{
	    fprintf(stderr,"  sendbuf[%d] = %d 0x%x\n",i,sendbuf[i],sendbuf[i]);
	}
    }

    n = ioctlagent(chan0,ARG(0),ARG(2),ARG(3),ARG(4),ARG(5),ARG(1));
    if (n != 0)
	n = -1;
    recvbuf[0] = HI(n);
    recvbuf[1] = LO(n);
    channum = getagentanschan();
    recvbuf[2] = HI(channum);
    recvbuf[3] = LO(channum);
    p = getagentanswer();
    for (i = 4; i < sizeof(recvbuf)-1; i++)
	recvbuf[i] = p[i-4];
    recvbuf[sizeof(recvbuf)-1] = '\0';

    if (verbose)
    {
	fprintf(stderr, "receive result: %d, chan: %d\n", 
					getagentansresult(), channum);
	fprintf(stderr,"receive buffer:\n");
	for (i = 0; i < sizeof(recvbuf)-1; i++)
	    fprintf(stderr,"  recvbuf[%d] = %d 0x%x\n",i,recvbuf[i],recvbuf[i]);
    }
    if (n == -1) {
	fprintf(stderr,"agent: call failed\n");
	if (dispchan)
	    printf("-1\n");
	doexit(2);
    }

afteragent:
    if (dispchan)
	printf("%d\n", channum);
    if (dispascii)
	printf("%s\n", &recvbuf[4]);
    if (command != 0) {
	if (agcall == '&') {
	    n = Runlayer(channum, command);
	    if (n > 0) {
		if (verbose)
		    fprintf(stderr, "child process id: %d\n", n);
		if (layerstype == 1)
		    fprintf(stderr, "warning: %s\n",
			"child process will not die when window is deleted!");
		doexit(0);
	    }
	    doexit(255);
	}
	else {
	    n = Runwaitlayer(channum, command);
	    if (n == -1)
		doexit(255);
	    doexit(n);
	}
    }
    doexit(0);
}
