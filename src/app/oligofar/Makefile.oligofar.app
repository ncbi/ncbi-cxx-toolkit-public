REQUIRES = objects bdb -Cygwin

ASN_DEP = 

APP = oligofar

SRC = \
        main \
        capp \
        chit \
        getopt \
        csnpdb \
        cbatch \
        cfilter \
        cguidefile \
        cquery \
        cqueryhash \
        cscoring \
        cscoretbl \
        cseqscanner \
        calignerbase \
        coligofarapp \
        coligofarapp-aligners \
        coligofarapp-tests \
        coutputformatter \
        cseqcoding \
        cpermutator4b \
        cprogressindicator


LIB = bdb $(BLAST_LIBS) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS) $(BERKELEYDB_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS) $(BERKELEYDB_CXXFLAGS)

LDFLAGS = $(LOCAL_LDFLAGS) $(FAST_LDFLAGS)



