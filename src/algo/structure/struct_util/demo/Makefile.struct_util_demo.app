# $Id$
# Author:  Paul Thiessen

# Build application "struct_util_demo"
#################################

REQUIRES = objects

APP = struct_util_demo

SRC = struct_util_demo

LIB = xstruct_util \
      xstruct_dp \
	xblast blastdb xnetblast tables \
      ncbimime \
      cdd \
      scoremat \
      cn3d \
      mmdb \
      seqset $(SEQ_LIBS) sequtil \
      pub \
      medline \
      biblio \
      general \
      xconnect \
      xser \
      xutil \
      xctools \
      xncbi

CPPFLAGS = $(ORIG_CPPFLAGS)

LIBS = $(ORIG_LIBS) $(NETWORK_LIBS)
