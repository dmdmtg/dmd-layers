### Standard Defines ###

###############################################################################
# Default DMD definitions
###############################################################################
#
# 'DMDSYS' is the location of layers, and 'DMD' is the location of the software
#   development package.  The corresponding 'DEF' variables are the default
#   values for those variables if they are not set at run-time.  The 'DMDSYS'
#   variable here in the makefile is for the installation location of the
#   make run which does not have to be the same as the default installation
#   location.   The 'DMD' make variable here is not really used anymore by the
#   layers makefiles except it specifies the default values of $(DMDSYS) for
#   compatibility with the way these makefiles used to be set up.
#
DMD=/usr/local/dmdlayers
DMDSYS=$(DMD)
DEFDMD=$(DMDSYS)
DEFDMDSYS=$(DEFDMD)
#
###############################################################################
# EXTRACFLAGS are extra flags to pass to every $(CC) command
###############################################################################
#
EXTRACFLAGS=-O
# Use the following to link with static libraries
#EXTRACFLAGS=-O -Bstatic
# Use the following for Solaris 2.0 or later:
#EXTRACFLAGS=-O -DUSE_SYSVR4LIKE -lsocket -lnsl
# Use the following for IRIX 5.3 or later:
#EXTRACFLAGS=-O -DUSE_SYSVR4LIKE -cckr
# Use this for Mips Risc/OS:
#EXTRACFLAGS=-O -I/usr/include -I/usr/include/bsd -lc -lbsd
# Use this for Linux:
#EXTRACFLAGS=-O -DNO_SYSVR4LIKE -I/usr/include -I/usr/include/bsd -lbsd
# Use this for HP-UX:
#EXTRACFLAGS=-O -lV3
# Use this for UnixWare 2.01 or later:
#EXTRACFLAGS=-O -lgen -Xt
#
###############################################################################
# The following defines should NOT be changed
###############################################################################
#
DMDBIN=$(DMDSYS)/bin
DMDLIB=$(DMDSYS)/lib
DMDXT=$(DMDSYS)/xt
DMDMAN=$(DMDSYS)/man
MODE775=755
MODE664=644

### Specific Defines ###
MAKE_ARGS=	$(MFLAGS) DMDSYS="$(DMDSYS)" DEFDMDSYS="$(DEFDMDSYS)" \
		DEFDMD="$(DEFDMD)" DMDBIN="$(DMDBIN)" DMDLIB="$(DMDLIB)" \
		DMDMAN="$(DMDMAN)" MODE775="$(MODE775)" MODE664="$(MODE664)" \
		EXTRACFLAGS="$(EXTRACFLAGS)"

PRODUCTS=	include lib misc layers man

### Standard Targets ###

default:	build

all:		$(PRODUCTS)

build:
		@set -x; p=`pwd`; for d in $(PRODUCTS); do \
		    cd $$p/$$d; make $(MAKE_ARGS); \
		done

install:	mkdirs
		@set -x; p=`pwd`; for d in $(PRODUCTS); do \
		    cd $$p/$$d; make $(MAKE_ARGS) install; \
		done

mkdirs:
		-mkdir $(DMDSYS)
		-chmod $(MODE775) $(DMDSYS)
		-mkdir $(DMDBIN)
		-chmod $(MODE775) $(DMDBIN)
		-mkdir $(DMDMAN)
		-chmod $(MODE775) $(DMDMAN)
		-mkdir $(DMDLIB)
		-chmod $(MODE775) $(DMDLIB)
		-mkdir $(DMDXT)
		-chmod 0777 $(DMDXT)
		-touch $(DMDXT)/.tarholder

$(PRODUCTS):	FRC
		cd $@; make $(MAKE_ARGS)

clobber:	clean
clean:
		@set -x; p=`pwd`; for d in $(PRODUCTS); do \
		    cd $$p/$$d; make $(MAKE_ARGS) clean; \
		done

### Additional Dependencies ###
FRC:
