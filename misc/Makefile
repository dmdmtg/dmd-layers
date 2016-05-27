DMD=/usr/local/dmdlayers
DMDSYS=$(DMD)
DMDBIN=$(DMDSYS)/bin
DMDLSYS=$(DMDSYS)/lib/layersys
DMDMISC=$(DMDSYS)/misc
DMDINC=../include
LIBWINDOWS=../lib/libwindows/libwindows.a
EXTRACFLAGS=-O
CFLAGS=$(EXTRACFLAGS)
MODE775=755
MODE664=644
SHPROGS=xts xtt relogin
CPROGS=ismpx jterm jwin xtagent plabel penter pfkey pmenu
BINPROGS=$(CPROGS) $(SHPROGS)
SYSPROGS=grantpt movelogin

all:	$(CPROGS) $(SYSPROGS)

lint:

plabel:	plabel.c windowproc.h
	$(CC) -o plabel plabel.c $(CFLAGS)

penter:	penter.c windowproc.h
	$(CC) -o penter penter.c $(CFLAGS)

pfkey:	pfkey.c windowproc.h
	$(CC) -o pfkey pfkey.c $(CFLAGS)

pmenu:	pmenu.c windowproc.h
pmenu:	$(DMDINC)/lyrscfg.h
	$(CC) -I$(DMDINC) -o pmenu pmenu.c $(CFLAGS)

ismpx:	ismpx.c
ismpx:	$(DMDINC)/lyrscfg.h
ismpx:	$(DMDINC)/sys/jioctl.h
ismpx:	$(LIBWINDOWS)
	$(CC) -I$(DMDINC) -o ismpx ismpx.c $(LIBWINDOWS) $(CFLAGS)

jterm:	jterm.c
jterm:	$(DMDINC)/lyrscfg.h
jterm:	$(DMDINC)/sys/jioctl.h
jterm:	$(LIBWINDOWS)
	$(CC) -I$(DMDINC) -o jterm jterm.c $(LIBWINDOWS) $(CFLAGS)

jwin:	jwin.c
jwin:	$(DMDINC)/lyrscfg.h
jwin:	$(DMDINC)/sys/jioctl.h
jwin:	$(LIBWINDOWS)
	$(CC) -I$(DMDINC) -o jwin jwin.c $(LIBWINDOWS) $(CFLAGS)

xtagent: xtagent.c
xtagent: $(DMDINC)/lyrscfg.h
xtagent: $(DMDINC)/hostagent.h
xtagent: $(DMDINC)/agent.h
xtagent: $(LIBWINDOWS)
	$(CC) -I$(DMDINC) -o xtagent xtagent.c $(LIBWINDOWS) $(CFLAGS)

grantpt: grantpt.c
grantpt: $(DMDINC)/lyrscfg.h
	$(CC) -I$(DMDINC) -o grantpt grantpt.c $(CFLAGS)

movelogin: movelogin.c
movelogin: $(DMDINC)/lyrscfg.h
	$(CC) -I$(DMDINC) -o movelogin movelogin.c $(CFLAGS)

install:	all
	-mkdir $(DMDMISC) $(DMDLSYS)
	chmod $(MODE775) $(DMDMISC) $(DMDLSYS)
	for p in $(CPROGS); do \
	    cp $$p $(DMDBIN); \
	    chmod $(MODE775) $(DMDBIN)/$$p; \
	    strip $(DMDBIN)/$$p; \
	done
	for p in $(SHPROGS); do \
	    cp $$p $(DMDBIN); \
	    chmod $(MODE775) $(DMDBIN)/$$p; \
	done
	$(CC) -I$(DMDINC) $(CFLAGS) needsysprog.c -o needsysprog
	set -x; for p in $(SYSPROGS); do \
	    if ./needsysprog $$p && \
		  [ -z "`find $(DMDLSYS)/$$p -newer $$p -print 2>/dev/null || true`" ];\
		then \
		rm -f $(DMDLSYS)/$$p; \
		cp $$p $(DMDLSYS); \
		chmod 4$(MODE775) $(DMDLSYS)/$$p; \
		if [ "`find $(DMDLSYS)/$$p -user root -print`" = "" ]; then \
		    chmod $(MODE775) $(DMDLSYS)/$$p; \
		    set +x; \
		    echo "Warning: $(DMDLSYS)/$$p not installed setuid-root."; \
		    if [ "$$p" = "movelogin" ]; then \
			echo "  This may not be a problem -- see README."; \
		    fi; \
		fi; \
	    fi; \
	done
	rm -f needsysprog
	rm -f $(DMDMISC)/README $(DMDMISC)/HISTORY
	cp ../README ../HISTORY $(DMDMISC)
	chmod $(MODE664) $(DMDMISC)/README $(DMDMISC)/HISTORY
	rm -f $(DMDMISC)/MTG.termcap $(DMDMISC)/MTG.terminfo
	cp MTG.termcap MTG.terminfo $(DMDMISC)
	chmod $(MODE664) $(DMDMISC)/MTG.termcap $(DMDMISC)/MTG.terminfo
	@echo "Add MTG.termcap to /etc/termcap and/or run 'tic MTG.terminfo'" 
	@echo " if you need them on your system and want to support 630/730s."

clean: clobber

clobber:	 FRC
	rm -f $(CPROGS) $(SYSPROGS) *.o

FRC:
