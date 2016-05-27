/*
 * An open pty master device should be on the fd passed in arg 1,
 * and the name of the slave pty device in arg2.
 *
 * We can't accept only a name for the slave side for security reasons;
 * need to make sure the name of the slave side matches the pty master fd.
 *
 * The name of the slave side device could be inferred from the master fd
 * but it could take a long time (if /dev/[pt]ty?? scheme is in use) because
 * it involves use of ttyname(), so it saves a lot of time to require the
 * name of the slave side as a parameter.
 *
 * This source code may be freely distributed.
 *
 * - Dave Dykstra, 4/29/93
 */

#include "lyrscfg.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <string.h>
#include <sys/sysmacros.h>

#ifdef USE_CLONE_PTY

#define AMASTER	"/dev/ptc"
#define ASLAVE	"/dev/ttyq0"
#define AMASTER0 "/dev/ptmc0"
#define AMASTER1 "/dev/ptmc1"
#define AMASTER2 "/dev/ptmc2"
#define AMASTER3 "/dev/ptmc3"
#define ASLAVE0	"/dev/ttyq0"
#define ASLAVE1	"/dev/ttyq256"
#define ASLAVE2	"/dev/ttyq512"
#define ASLAVE3	"/dev/ttyq768"

#else

#ifdef USE_MAXI_PTYALLOC
#define AMASTER	"/dev/ptyp000"
#define ASLAVE	"/dev/ttyp000"
#else
#define AMASTER	"/dev/ptyp0"
#define ASLAVE	"/dev/ttyp0"
#endif

#endif

usage()
{
    fprintf(stderr, "Usage: grantpt [-r] fd ttyname\n");
    exit(1);
}

int
main(argc, argv)
char **argv;
{
    int fd;
    struct stat mstb, stb, sstb;
    char *tty;
    int release;
    int retval;
    extern int optind;
#ifdef USE_PTMX
    char *tty2, *ptsname();
#else
    char *slave;
#endif

    while ((retval = getopt(argc, argv, "r")) != EOF) {
	switch (retval)
	{
	case 'r':
	    release = 1;
	    break;
	default:
	    usage();
	}
    }
    if (optind != argc - 2)
	    usage();

    fd = atoi(argv[optind]);
    tty = argv[optind+1];

#ifdef USE_PTMX
    if ((tty2 = ptsname(fd)) == NULL) {
	fprintf(stderr, "grantpt: Fd %d is not a pts master\n", fd);
	exit(1);
    }
    if (strcmp(tty, tty2) != 0) {
	fprintf(stderr, "grantpt: %s does not match %s\n", tty, tty2);
	exit(1);
    }
#else

    if (fstat(fd, &mstb) < 0) {
	fprintf(stderr, "grantpt: Cannot fstat fd %d\n", fd);
	exit(1);
    }

#ifdef USE_CLONE_PTY
    /*
	For CLONE_PTY, we go through each of the 5 possible masters,
	trying to find the correct one.  The first four are for an
	extension to the standard Mips clone pty implementation (EPIX?)
	where the MAJOR number of the AMASTER? devices indicate the
	major number of the master half of pseudo-ttys, and the last
	one is for the base Mips implementation where the MINOR number
	of AMASTER indicates the major number of the master half.
    */
    if ((stat(AMASTER0, &stb) == 0) &&
		(major(mstb.st_rdev) == major(stb.st_rdev)))
	slave = ASLAVE0;
    else if ((stat(AMASTER1, &stb) == 0) &&
		(major(mstb.st_rdev) == major(stb.st_rdev)))
	slave = ASLAVE1;
    else if ((stat(AMASTER2, &stb) == 0) &&
		(major(mstb.st_rdev) == major(stb.st_rdev)))
	slave = ASLAVE2;
    else if ((stat(AMASTER3, &stb) == 0) &&
		(major(mstb.st_rdev) == major(stb.st_rdev)))
	slave = ASLAVE3;
    else if ((stat(AMASTER, &stb) == 0) &&
		(major(mstb.st_rdev) == minor(stb.st_rdev)))
	slave = ASLAVE;
    else {
	fprintf(stderr, "grantpt: Fd %d is not a pty master\n", fd);
	exit(1);
    }
#else 
    slave = ASLAVE;

    if (stat(AMASTER, &stb) < 0) {
	fprintf(stderr, "grantpt: Cannot stat %s\n", AMASTER);
	exit(1);
    }
    if (major(mstb.st_rdev) != major(stb.st_rdev)) {
	fprintf(stderr, "grantpt: Fd %d is not a pty master\n", fd);
	exit(1);
    }
#endif

    if (stat(tty, &sstb) < 0) {
	fprintf(stderr, "grantpt: Cannot stat %s\n",tty);
	exit(1);
    }
    if (stat(slave, &stb) < 0) {
	fprintf(stderr, "grantpt: Cannot stat %s\n", slave);
	exit(1);
    }
    if (major(sstb.st_rdev) != major(stb.st_rdev)) {
	fprintf(stderr, "grantpt: %s is not a pty slave\n", tty);
	exit(1);
    }
    if (minor(sstb.st_rdev) != minor(mstb.st_rdev)) {
	fprintf(stderr,
		"grantpt: Minor %d of slave %s doesn't match master minor %d\n",
			minor(sstb.st_rdev), tty, minor(mstb.st_rdev));
	exit(1);
    }
#endif

    if (geteuid() != 0) {
	fprintf(stderr, "grantpt: not installed setuid-root\n");
	exit(1);
    }
    if (release && (chmod(tty, 0666) < 0)) {
	fprintf(stderr, "grantpt: Cannot chmod %s\n", tty);
	exit(1);
    }
    if (chown(tty, release ? 0 : getuid(), release ? 0 : getgid()) < 0) {
	fprintf(stderr, "grantpt: Cannot chown %s\n", tty);
	exit(1);
    }
    if (!release && (chmod(tty, 0620) < 0)) {
	fprintf(stderr, "grantpt: Cannot chmod %s\n", tty);
	exit(1);
    }
    return(0);
}
