# $Id$
# Author:  Paul Thiessen

# Build application "cddalignview"
#################################

APP = cddalignview

SRC = \
	cav_main

LIB = \
	xcddalignview \
	ncbimime \
	cdd \
	cn3d \
	mmdb1 \
	mmdb2 \
	mmdb3 \
	seqset $(SEQ_LIBS) \
	pub \
	medline \
	biblio \
	general \
	xser \
	xutil \
	xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(srcdir)/..
