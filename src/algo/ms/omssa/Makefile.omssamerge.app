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
      xregexp $(PCRE_LIB) xconnect $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
