/*
 *	Enhancements by David Dykstra at AT&T.
 *	This source code may be freely distributed.
 */

/* #ident	"@(#)misc:ismpx.c	2.4" */

#include <stdio.h>

main(argc, argv)
	int   argc;
	char *argv[];
{
	if (argc != 1)
		if (!(argc == 2 && (strcmp("-s", argv[1]) == 0 ||
		                    strcmp("-",  argv[1]) == 0 ))) {
			fprintf(stderr, "usage: ismpx [-s]\n");
			exit(1);
		}
	if (!inlayers()) {
		if(argc==1)
			printf("no\n");
		exit(1);
	}
	if (argc == 1)
		printf("yes\n");
	exit(0);
}
