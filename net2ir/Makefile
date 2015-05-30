#
# rcsid = "$Header: /usr/cvsroot/magic-7.5/net2ir/Makefile,v 1.1.1.1 2006/04/10 22:03:14 tim Exp $"
#

MODULE   = net2ir
MAGICDIR = ..
SRCS     = net2ir.c

EXTRA_LIBS  = ${MAGICDIR}/utils/libutils.a

include ${MAGICDIR}/defs.mak

tcl-main:
	echo "Nothing to do here"

main: net2ir

install: $(DESTDIR)${BINDIR}/${MODULE}${EXEEXT}

install-tcl:
	echo "Nothing to do here"

include ${MAGICDIR}/rules.mak
