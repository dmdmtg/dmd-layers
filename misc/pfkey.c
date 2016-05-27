/*
 * pfkey: utility for setting the strings bound to function keys on the 630MTG.
 * 80 chars max. can be bound to each function keys.
 * usage: pfkey -[1|2|3|4|5|6|7|8<str>]
 */
#include <stdio.h>
#include "windowproc.h"
#define USEAGE	"Usage: %s -[1|2|3|4|5|6|7|8<string>]\n"
main(argc, argv)
	int argc;
	char **argv;
{
	register int retval;
	int len;
	int funkey;
	int errflag = 0;
	extern char *optarg;
	extern int optind;
	
	while ((retval = getopt(argc, argv, "1:2:3:4:5:6:7:8:")) != EOF) {
		switch(retval) {
		case '1':
			funkey = 1;
			break;
		case '2':
			funkey = 2;
			break;
		case '3':
			funkey = 3;
			break;
		case '4':
			funkey = 4;
			break;
		case '5':
			funkey = 5;
			break;
		case '6':
			funkey = 6;
			break;
		case '7':
			funkey = 7;
			break;
		case '8':
			funkey = 8;
			break;
		case '?':
		default:
			fprintf(stderr, USEAGE, argv[0]);
			exit(1);
		}
		if ((len = strlen(optarg)) > PFKEYMAX) {
			fprintf(stderr, "%s: %d characters maximium\n",PFKEYMAX,
				optarg);
			exit(2);
		}
		printf(PRGPFKEY, funkey, len, optarg);
	}
	exit(0);
}
