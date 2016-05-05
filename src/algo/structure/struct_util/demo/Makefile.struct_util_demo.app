# $Id$
# Author:  Paul Thiessen

# Build application "struct_util_demo"
#################################

WATCHERS = thiessen 

REQUIRES = objects

APP = struct_util_demo

SRC = struct_util_demo

LIB = xstruct_util \
      xstruct_dp \
      ncbimime \
      cdd \
      $(BLAST_LIBS) \
      cn3d \
      mmdb \
      $(OBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

# These are necessary to avoid build errors in some configurations
# (notably 32-bit SPARC WorkShop Release).
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)
