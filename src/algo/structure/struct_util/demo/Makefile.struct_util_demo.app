# $Id$
# Author:  Paul Thiessen

# Build application "struct_util_demo"
#################################

REQUIRES = objects

APP = struct_util_demo

SRC = struct_util_demo

LIB = xstruct_util \
      xstruct_dp \
      xblast seqdb blastdb xnetblastcli xnetblast tables xobjsimple xobjutil \
      ncbimime \
      cdd \
      scoremat \
      cn3d \
      mmdb \
      $(OBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
