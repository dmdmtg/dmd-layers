### Standard Defines ###

DMD=/usr/local/dmdlayers
DMDSYS=$(DMD)
DEFDMD=$(DMD)
DEFDMDSYS=/usr
DMDBIN=$(DMDSYS)/bin
DMDLIB=$(DMDSYS)/lib
DMDSRC=$(DMDSYS)/src
DMDINC=../include
MODE775=755
MODE664=644

### Specific Defines ###
HEADERS=	common.h packets.h pconfig.h proto.h pstats.h \
		$(DMDINC)/lyrscfg.h $(DMDINC)/hostagent.h \
		$(DMDINC)/agent.h $(DMDINC)/sys/jioctl.h
SRCS=		boot.c control.c debug.c externs.c main.c multiplex.c \
		process.c utmp_tty.c agent.c pcheck.c pinit.c \
		precv.c psend.c ptimeout.c termio.c trace.c
OBJ=		boot.o control.o debug.o externs.o main.o multiplex.o \
		process.o utmp_tty.o agent.o pcheck.o pinit.o \
		precv.o psend.o ptimeout.o termio.o trace.o

EXTRACFLAGS=-O
CFLAGS=-I$(DMDINC) -DDEFDMD=\"$(DEFDMD)\" -DDEFDMDSYS=\"$(DEFDMDSYS)\" \
		$(EXTRACFLAGS)

### Standard Targets ###

all:		layers

build:		all

.c.o:
		$(CC) $(CFLAGS) -c $<
.SUFFIXES: .i
.c.i:
		$(CC) $(CFLAGS) -E $< >$*.i

layers:		$(OBJ)
		$(CC) -o layers $(OBJ) $(CFLAGS)

$(OBJ):		$(HEADERS)

install:	layers
		-mkdir $(DMDSYS)/xt $(DMDBIN)
		-chmod 777 $(DMDSYS)/xt
		-touch $(DMDSYS)/xt/.tarholder
		rm -f $(DMDBIN)/layers
		cp layers $(DMDBIN)
		strip $(DMDBIN)/layers
		chmod $(MODE775) $(DMDBIN)/layers

strip:
		-rm -f $(DMDBIN)/layers
		-rm -rf $(DMDSYS)/xt
clean:
		rm -f a.out core gmon.out *.o layers layers.debug

clobber:	clean

lint:
		lint -abch $(CFLAGS) $(SRCS)
