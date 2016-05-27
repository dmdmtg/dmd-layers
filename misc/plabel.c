/*
 * plabel: utility for writing a string into the label area starting at the
 * left, right or center (-l,-c,-r)
 */
#include <stdio.h>
#include "windowproc.h"
#define USEAGE	"Usage: %s [-f][-l|-c|-r string]\n"

main(argc, argv)
	int argc;
	char **argv;
{
	register int retval;
	register int location;
	extern char *optarg;
	extern int optind;
	
	retval = getopt(argc, argv, "l:c:r:f");
	switch(retval) {
	case 'l':
		location = L_LABEL;
		break;
	case 'c':
		location = C_LABEL;
		break;
	case 'r':
		location = R_LABEL;
		break;
	case 'f':
		location = L_LABEL;
		optarg = "";
		break;
	case '?':
	default:
		fprintf(stderr, USEAGE, argv[0]);
		exit(1);
	}
	printf(PRGLABEL, strlen(optarg), location, optarg);
	exit(0);
}
