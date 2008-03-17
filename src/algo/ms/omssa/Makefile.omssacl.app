# $Id$
# Author:  Lewis Y. Geer

# Build application "omssacl"
#################################

CXXFLAGS = $(FAST_CXXFLAGS)

LDFLAGS  = $(FAST_LDFLAGS)

APP = omssacl

SRC = omssacl

LIB = xomssa omssa pepXML blast composition_adjustment tables seqdb blastdb \
      xregexp $(PCRE_LIB) xconnect $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

