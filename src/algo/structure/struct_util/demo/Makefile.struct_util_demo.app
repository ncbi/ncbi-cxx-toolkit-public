# $Id$
# Author:  Paul Thiessen

# Build application "struct_util_demo"
#################################

REQUIRES = objects ctools C-Toolkit

APP = struct_util_demo

SRC = struct_util_demo

LIB = xstruct_util \
      xstruct_dp \
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
      xctools \
      xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)

NCBI_C_LIBS = -lncbimmdb -lncbiid1 -lnetcli -lncbitool -lncbiobj -lncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

