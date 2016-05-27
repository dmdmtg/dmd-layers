/*
 *	Enhancements by David Dykstra at AT&T.
 *	This source code may be freely distributed.
 */

/* #ident	"@(#)misc:jwin.c	2.4" */

#include <stdio.h>

main(argc,argv)
char *argv[];
{
	int any = 0;
	int bytesx, bytesy, bitsx, bitsy;
	
	if ((argc == 2) && (argv[1][0] == '-') && (argv[1][1] == 'a')) {
		argc--;
		any++;
	}
	    
	if (argc != 1) {
		fprintf(stderr,"usage: jwin [-a]\n");
		exit(1);
	}

	if (!any && !inlayers()) {
		fprintf(stderr,"jwin: not in layers; try jwin -a\n");
		exit(1);
	}
	if (Jwinsize(&bytesx, &bytesy, &bitsx, &bitsy) == -1) {
		fprintf(stderr,"jwin: error getting winsize\n");
		exit(1);
	}
	printf("bytes:\t%d %d\n", bytesx, bytesy);
	if (bitsx != 0 || bitsy != 0)
		printf("bits:\t%d %d\n", bitsx, bitsy);
	exit(0);
}
