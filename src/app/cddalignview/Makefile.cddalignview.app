# $Id$
# Author:  Paul Thiessen

# Build application "cddalignview"
#################################

APP = cddalignview

SRC = \
	cav_main

LIB = \
	xcddalignview \
	cdd \
	cn3d \
	ncbimime \
	mmdb1 \
	mmdb2 \
	mmdb3 \
	pub \
	seqset $(SEQ_LIBS) \
	medline \
	biblio \
	general \
	xser \
	xncbi \
	xutil

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(srcdir)/..
