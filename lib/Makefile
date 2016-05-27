### Standard Defines ###

DMD=/usr/local/dmdlayers
DMDSYS=$(DMD)
DEFDMD=$(DMD)
DEFDMDSYS=/usr
DMDBIN=$(DMDSYS)/bin
DMDLIB=$(DMDSYS)/lib
DMDINC=../../include
MODE775=755
MODE664=644
EXTRACFLAGS=-O
CFLAGS=$(EXTRACFLAGS)

### Specific Defines ###
MAKE_ARGS=	$(MFLAGS) DMDSYS="$(DMDSYS)" DEFDMDSYS="$(DEFDMDSYS)" \
		DEFDMD="$(DEFDMD)" DMDBIN="$(DMDBIN)" DMDLIB="$(DMDLIB)" \
		DMDINC="$(DMDINC)" MODE775="$(MODE775)" MODE664="$(MODE664)" \
		CFLAGS="$(CFLAGS)" EXTRACFLAGS="$(EXTRACFLAGS)"

PRODUCTS= libwindows

### Standard Targets ###

all:		$(PRODUCTS)

lint:

install:	all
		-mkdir $(DMDLIB)
		-chmod $(MODE775) $(DMDLIB)
		cd libwindows; make $(MAKE_ARGS) install

build:		install

libwindows:	FRC
		cd libwindows; make $(MAKE_ARGS)

clean:
		cd libwindows; make $(MAKE_ARGS) clean

### Additional Dependencies ###
FRC:
