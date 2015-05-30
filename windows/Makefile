#
# rcsid $Header: /usr/cvsroot/magic-7.5/windows/Makefile,v 1.2 2008/01/11 01:02:42 tim Exp $
#

MODULE    = windows
MAGICDIR  = ..
SRCS      = windClient.c windCmdAM.c windCmdNR.c windCmdSZ.c \
            windSend.c windDebug.c windDisp.c windMain.c windMove.c \
            windSearch.c windTrans.c windView.c 

GLYPHS    = windows7.glyphs windows11.glyphs windows14.glyphs windows22.glyphs 
FONTS	  = vfont.B.12 vfont.I.12 vfont.R.8

include ${MAGICDIR}/defs.mak
include ${MAGICDIR}/rules.mak

install-tcl:
	@echo --- installing glyphs to $(DESTDIR)${SYSDIR}
	for i in ${GLYPHS} ${FONTS}; do \
		(cd $(DESTDIR)${SYSDIR} && ${RM} $$i); \
		${CP} $$i $(DESTDIR)${SYSDIR}; done

install:
	@echo --- installing glyphs to $(DESTDIR)${SYSDIR}
	for i in ${GLYPHS} ${FONTS}; do \
		(cd $(DESTDIR)${SYSDIR} && ${RM} $$i); \
		${CP} $$i $(DESTDIR)${SYSDIR}; done
