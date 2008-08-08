#
# Makefile:  /Users/slotta/NCBI/omssa2pepXML/Makefile.omssa2pepXML_app

APP = omssa2pepXML
SRC = omssa2pepXML

CXXFLAGS = $(FAST_CXXFLAGS) $(CMPRS_INCLUDE)

LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = -I$(top_srcdir)/src/algo/ms/omssa $(ORIG_CPPFLAGS)

LIB = xomssa omssa pepXML blast composition_adjustment tables seqdb blastdb \
      xregexp $(PCRE_LIB) xconnect $(COMPRESS_LIBS) $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)
