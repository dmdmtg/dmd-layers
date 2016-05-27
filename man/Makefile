DMD=/usr/local/dmdlayers
DMDSYS=$(DMD)
MODE664=644
MODE775=755
DMDMAN=	$(DMDSYS)/man
DMDCATMAN=$(DMDSYS)/catman
SRCS=	layers.1 ismpx.1 jterm.1 jwin.1 relogin.1 xtagent.1 xts.1 xtt.1 \
		pfkey.1 plabel.1 penter.1 pmenu.1 libwindows.3 xtproto.5

all: 

install: all
	-mkdir $(DMDMAN) $(DMDMAN)/man1 $(DMDMAN)/man3 $(DMDMAN)/man5
	-chmod $(MODE775) $(DMDMAN) $(DMDMAN)/man1 $(DMDMAN)/man3 $(DMDMAN)/man5
	for s in $(SRCS); do \
	    suff=`echo $$s|sed 's/.*\.//'`; \
	    cp $$s $(DMDMAN)/man$$suff; \
	    chmod $(MODE664) $(DMDMAN)/man$$suff/$$s; \
	done

catman: all
	-mkdir $(DMDCATMAN) $(DMDCATMAN)/DMD \
		$(DMDCATMAN)/DMD/g1 $(DMDCATMAN)/DMD/g3 \
		$(DMDCATMAN)/DMD/g5
	-chmod $(MODE775) $(DMDCATMAN) $(DMDCATMAN)/DMD \
		$(DMDCATMAN)/DMD/g1 $(DMDCATMAN)/DMD/g3 \
		$(DMDCATMAN)/DMD/g5
	for s in $(SRCS); do \
	    suff=`echo $$s|sed 's/.*\.//'`; \
	    base=`echo $$s|sed "s/\.$$suff\$$//"`; \
	    if [ ! -z "`find $(DMDCATMAN)/DMD/g$$suff/$$base.z -newer $$s -print 2>/dev/null`" ]; then continue; fi; \
	    rm -f $(DMDCATMAN)/DMD/g$$suff/$$base*; \
	    nroff -man $$s > $(DMDCATMAN)/DMD/g$$suff/$$base; \
	    pack -f $(DMDCATMAN)/DMD/g$$suff/$$base; \
	    chmod $(MODE664) $(DMDCATMAN)/DMD/g$$suff/$$base*; \
	done

clean:
