### Standard Defines ###

DMD=/usr/local/dmdlayers
DMDSYS=$(DMD)
DEFDMD=$(DMD)
DMDBIN=$(DMDSYS)/bin
DMDLIB=$(DMDSYS)/lib
DMDINC=../../include
MODE775=755
MODE664=644
EXTRACFLAGS=-O
CFLAGS=$(EXTRACFLAGS)

### Specific Defines ###
PRODUCTS= libwindows.a

### Standard Targets ###

all:		$(PRODUCTS)

install:	all
		-mkdir $(DMDLIB)
		-chmod $(MODE775) $(DMDLIB)
		cp libwindows.a $(DMDLIB)
		chmod $(MODE664) $(DMDLIB)/libwindows.a
		(ranlib $(DMDLIB)/libwindows.a) 2>/dev/null || true

build:		install

libwindows.a:	hostagent.c $(DMDINC)/hostagent.h $(DMDINC)/agent.h \
				$(DMDINC)/lyrscfg.h $(DMDINC)/sys/jioctl.h
		$(CC) $(CFLAGS) -DDEFDMD=\"$(DEFDMD)\" -I$(DMDINC) -c hostagent.c
		ar crvu libwindows.a hostagent.o
		(ranlib libwindows.a) 2>/dev/null || true

clean:
		rm -f libwindows.a hostagent.o

lint:
		lint -abchx -DDMD=\"$(DMD)\" -I$(DMDINC) hostagent.c

### Additional Dependencies ###
FRC:
