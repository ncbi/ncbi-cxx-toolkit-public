# Author:  Lewis Y. Geer

# Build application "omssamerge"
#################################

# pcre moved into platform specific make 
# LIBS = $(PCRE_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS) $(CMPRS_INCLUDE)

LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = -I$(top_srcdir)/src/algo/ms/omssa $(ORIG_CPPFLAGS)

APP = omssamerge

SRC = omssamerge

LIB = xomssa pepXML omssa blast composition_adjustment tables seqdb blastdb \
      xregexp $(PCRE_LIB) xconnect $(COMPRESS_LIBS) $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)
