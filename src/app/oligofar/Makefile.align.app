REQUIRES = objects bdb -Cygwin

ASN_DEP = 

APP = oligofar.align

SRC = \
        align-main \
		caligntest \
		util \
        capp \
        getopt \
        cscoring \
        cscoretbl \
        calignerbase \
        cseqcoding \
        cprogressindicator


LIB = bdb $(BLAST_LIBS) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS) $(BERKELEYDB_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS) $(BERKELEYDB_CXXFLAGS)
ORIG_CPPFLAGS = -I$(includedir) -I$(includedir)/internal $(CONF_CPPFLAGS)

LDFLAGS = $(LOCAL_LDFLAGS) $(FAST_LDFLAGS)



WATCHERS = rotmistr
