/*
 * This file added by Dave Dykstra.
 * This is in a separate file because attempting to include termio.h in
 *   utmp_tty.c caused all sorts of problems on MIPS RISC/os when using
 *   -systype bsd43.  That is no longer used when compiling layers on
 *   RISC/os but it is left here anyway.
 *
 * This source code may be freely distributed.
 */

#include "common.h"
#ifndef NO_TERMIO

#ifndef NO_TERMIOS
#include <termios.h>
#else
#include <termio.h>
#endif

#ifndef NO_TERMIOS
extern struct termios saveterm;
struct termios ttysetup;
#else
extern struct termio saveterm;
struct termio ttysetup;
#endif

termio_setcntrl(fd)
int fd;
{
    ttysetup = saveterm;
    ttysetup.c_iflag = 0;
    ttysetup.c_oflag = 0;
    ttysetup.c_cflag &= ~(CSIZE|PARENB);
    ttysetup.c_cflag |= CS8;
    ttysetup.c_lflag = 0;
#ifdef NO_TERMIOS
    ttysetup.c_line = 0;
#endif
    ttysetup.c_cc[VMIN] = 1;
    ttysetup.c_cc[VTIME] = 0;
#ifndef NO_TERMIOS
    (void) tcsetattr(fd, TCSADRAIN, &ttysetup);
#else
    (void) ioctl(fd, TCSETAW, &ttysetup);
#endif
}

termio_resetcntrl(fd)
int fd;
{
#ifndef NO_TERMIOS
    (void) tcsetattr(fd, TCSADRAIN, &saveterm);
#else
    (void) ioctl(fd, TCSETAW, &saveterm);
#endif
}

#endif
