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
CHECK_COPY = data
