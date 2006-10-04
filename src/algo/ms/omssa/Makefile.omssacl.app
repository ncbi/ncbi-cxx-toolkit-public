# $Id$
# Author:  Lewis Y. Geer

# Build application "omssacl"
#################################

CXXFLAGS = $(FAST_CXXFLAGS)

LDFLAGS  = $(FAST_LDFLAGS)

APP = omssacl

SRC = omssacl

LIB = xomssa omssa xblast xalgoblastdbindex composition_adjustment tables \
      seqdb xnetblastcli xnetblast scoremat blastdb xregexp \
      xobjsimple xobjutil xobjread creaders $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

