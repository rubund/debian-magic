#
# rcsid "$Header: /usr/cvsroot/magic-7.5/ext2spice/Makefile,v 1.1.1.1 2006/04/10 22:03:14 tim Exp $"
#

MODULE   = ext2spice
MAGICDIR = ..
SRCS     = ext2spice.c

EXTRA_LIBS = ${MAGICDIR}/extflat/libextflat.o ${MAGICDIR}/utils/libutils.a

include ${MAGICDIR}/defs.mak

LIBS += -lm ${LD_EXTRA_LIBS} ${SUB_EXTRA_LIBS}
CLEANS += exttospice${SHDLIB_EXT} spicewrap.o

main: ext2spice

tcl-main: exttospice${SHDLIB_EXT}

spicewrap.o: ext2spice.c
	${CC} ${CFLAGS} ${CPPFLAGS} ${DFLAGS} ext2spice.c -c -o spicewrap.o

exttospice${SHDLIB_EXT}: spicewrap.o ${MAGICDIR}/extflat/libextflat.o
	@echo --- making exttospice Tcl library \(exttospice${SHDLIB_EXT}\)
	${RM} exttospice${SHDLIB_EXT}
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ ${LDDL_FLAGS} spicewrap.o \
		${MAGICDIR}/extflat/libextflat.o ${LD_SHARED} -lc ${LIBS}

install: $(DESTDIR)${BINDIR}/${MODULE}${EXEEXT} $(DESTDIR)${BINDIR}/spice2sim 

install-tcl: $(DESTDIR)${TCLDIR}/exttospice${SHDLIB_EXT} $(DESTDIR)${BINDIR}/spice2sim

$(DESTDIR)${TCLDIR}/exttospice${SHDLIB_EXT}: exttospice${SHDLIB_EXT}
	${RM} $(DESTDIR)${TCLDIR}/exttospice${SHDLIB_EXT}
	${CP} exttospice${SHDLIB_EXT} $(DESTDIR)${TCLDIR}/exttospice${SHDLIB_EXT}

$(DESTDIR)${BINDIR}/spice2sim: spice2sim
	${RM} $(DESTDIR)${BINDIR}/spice2sim
	${CP} spice2sim $(DESTDIR)${BINDIR}/spice2sim

include ${MAGICDIR}/rules.mak
