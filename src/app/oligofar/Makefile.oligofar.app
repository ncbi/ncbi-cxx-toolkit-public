REQUIRES = objects bdb -Cygwin

ASN_DEP = 

APP = oligofar

SRC = \
        main \
		util \
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
        cpermutator8b \
        cprogressindicator


LIB = bdb $(BLAST_LIBS) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS) $(BERKELEYDB_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS) $(BERKELEYDB_CXXFLAGS)
ORIG_CPPFLAGS = -I$(includedir) -I$(includedir)/internal $(CONF_CPPFLAGS)

LDFLAGS = $(LOCAL_LDFLAGS) $(FAST_LDFLAGS)


CHECK_CMD = ./test-oligofar.sh
CHECK_FILES = NM_012345.fa NM_012345.reads NM_012345.pairs NM_012345.reads.out NM_012345.pairs.out
