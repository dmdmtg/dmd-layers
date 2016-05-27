/*
 * penter: utility for setting the "enter" key defination in the window it is
 * invoked in. PENTERMAX chars max. can be bound to the "enter" key. -r sets the
 * enter key to a \n.
 * usage: penter <string>
 */
#include <stdio.h>
#include "windowproc.h"
#define USEAGE	"Usage: %s <string>\n"
main(argc, argv)
	int argc;
	char **argv;
{
	int len;
	register int retval;
	int errflag = 0;
	extern char *optarg;
	extern int optind;
	
	while ((retval = getopt(argc, argv, "r")) != EOF) {
		switch(retval) {
		case 'r':
			printf(PRGENTER, 1, "\n");
			return(0);
		case '?':
		default:
			fprintf(stderr, USEAGE, argv[0]);
			exit(1);
		}
	}
	if ((len = strlen(argv[1])) > PENTERMAX) {
		fprintf(stderr,"%s: %d characters maximium\n",PENTERMAX,argv[0]);
		exit(2);
	}
	printf(PRGENTER, len, argv[1]);
	exit(0);
}
