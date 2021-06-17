REQUIRES = objects bdb -Cygwin

ASN_DEP = 

APP = bmfilter

SRC = bmfilter-main \
		util \
        getopt \
        cseqcoding \
        creadtagger \
        csamrecords \
		cshortreader \
		cbitmaskaccess \
        cprogressindicator


LIB = bdb $(BLAST_LIBS) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS) $(BERKELEYDB_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS) $(BERKELEYDB_CXXFLAGS)
ifdef NOASSERT 
CXXFLAGS += -DNOASSERT
endif

ORIG_CPPFLAGS = -I$(includedir) -I$(includedir)/internal $(CONF_CPPFLAGS)

LDFLAGS = $(LOCAL_LDFLAGS) $(FAST_LDFLAGS)


#CHECK_CMD = ./test-oligofar.sh
#CHECK_FILES = NM_012345.fa NM_012345.reads NM_012345.pairs NM_012345.reads.out NM_012345.pairs.out

WATCHERS = rotmistr
