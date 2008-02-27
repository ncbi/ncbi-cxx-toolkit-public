# Author:  Lewis Y. Geer

# Build application "omssamerge"
#################################

# pcre moved into platform specific make 
# LIBS = $(PCRE_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)

LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = -I$(top_srcdir)/src/algo/ms/omssa $(ORIG_CPPFLAGS)

APP = omssamerge

SRC = omssamerge

LIB = xomssa pepXML omssa blast composition_adjustment tables seqdb blastdb \
      seqset $(SEQ_LIBS) pub medline biblio general xser xregexp \
      $(PCRE_LIB) xconnect xutil xncbi

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
