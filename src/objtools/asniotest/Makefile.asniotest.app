# $Id$
# Author:  Paul Thiessen

# Build application "asniotest"
#################################

APP = asniotest
SRC = asniotest

LIB = \
    ncbimime \
    cdd \
    coremat \
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
CHECK_COPY = ADF.bin pdbSeqId.txt