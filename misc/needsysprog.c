/*
 * Exit with an exit code of 0 if setuid program is needed.
 * This is a C program instead of a shell so it can easily include lyrscfg.h.
 *
 * This source code may be freely distributed.
 */
#include "lyrscfg.h"
#include <stdio.h>

main(argc, argv)
char **argv;
{
    if (argc != 2) {
	fprintf(stderr, "Usage: needsysprog progname\n");
	exit(1);
    }
#ifdef USE_PTMX
    if (strcmp(argv[1], "grantpt") == 0)
	exit(1);
#endif
    return(0);
}
