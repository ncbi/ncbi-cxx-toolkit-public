# $Id$
# Author:  Lewis Y. Geer

# Build application "omssacl"
#################################

CXXFLAGS = $(FAST_CXXFLAGS)

LDFLAGS  = $(FAST_LDFLAGS)

APP = omssacl

SRC = omssacl

LIB = xomssa omssa blast composition_adjustment tables seqdb blastdb \
      seqset $(SEQ_LIBS) pub medline biblio general xser xregexp \
      $(PCRE_LIB) xconnect xutil xncbi

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

