# $Id$
# Author:  Paul Thiessen

# Build application "asniotest"
#################################

APP = asniotest
SRC = asniotest

LIB = \
    ncbimime \
    cdd \
    scoremat \
	cn3d \
	mmdb \
	seqset $(SEQ_LIBS) \
	pub \
	medline \
	biblio \
	general \
	xser \
	xutil \
	xncbi

CHECK_CMD =
# Must keep on a single line!
CHECK_COPY = 1doi.bin ADF.bin badAtom.txt badMSP.txt goodAtom.txt goodColor.txt goodGID.txt goodMSP.txt goodRC.txt pdbSeqId.txt
