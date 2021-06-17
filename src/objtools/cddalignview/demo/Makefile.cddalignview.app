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
	
 WATCHERS = thiessen
