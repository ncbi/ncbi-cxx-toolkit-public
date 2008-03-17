#
# Makefile:  /Users/slotta/NCBI/omssa2pepXML/Makefile.omssa2pepXML_app

APP = omssa2pepXML
SRC = omssa2pepXML

CXXFLAGS = $(FAST_CXXFLAGS)

LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = -I$(top_srcdir)/src/algo/ms/omssa $(ORIG_CPPFLAGS)

LIB = xomssa omssa pepXML blast composition_adjustment tables seqdb blastdb \
      xregexp $(PCRE_LIB) xconnect $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
