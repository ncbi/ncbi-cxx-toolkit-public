# Author:  Lewis Y. Geer

# Build application "omssamerge"
#################################


CXXFLAGS = $(FAST_CXXFLAGS) $(CMPRS_INCLUDE) $(STATIC_CXXFLAGS)

LDFLAGS  = $(FAST_LDFLAGS) $(STATIC_LDFLAGS) $(RUNPATH_ORIGIN)

CPPFLAGS = -I$(top_srcdir)/src/algo/ms/omssa $(ORIG_CPPFLAGS)

APP = omssamerge

SRC = omssamerge

LIB = xomssa pepXML omssa blast composition_adjustment tables seqdb blastdb \
      xregexp $(PCRE_LIB) xconnect $(COMPRESS_LIBS) $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)
