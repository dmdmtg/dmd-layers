#
# Doing a make install builds $(DMDSYS)/include
#
#
DMD=/usr/local/dmdlayers
DMDSYS=$(DMD)
MODE775=755
MODE664=644
STD=	agent.h hostagent.h lyrscfg.h
SYS=	jioctl.h
all:

lint:

build:	install

strip:	FRC
	-rm -rf $(DMDSYS)/include

install:
	-mkdir $(DMDSYS)/include
	-chmod $(MODE775) $(DMDSYS)/include
	-mkdir $(DMDSYS)/include/sys
	-chmod $(MODE775) $(DMDSYS)/include/sys
	-for i in ${STD}; do \
		cmp -s $$i $(DMDSYS)/include/$$i || \
		    cp $$i $(DMDSYS)/include/$$i; \
		chmod $(MODE664) $(DMDSYS)/include/$$i; \
	done
	-(cd sys; for i in ${SYS}; do \
		cmp -s $$i $(DMDSYS)/include/sys/$$i || \
		    cp $$i $(DMDSYS)/include/sys/$$i; \
		chmod $(MODE664) $(DMDSYS)/include/sys/$$i; \
	done)
clean:
FRC:
