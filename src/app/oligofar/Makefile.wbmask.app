REQUIRES = objects bdb -Cygwin

ASN_DEP = 

APP = oligofar.wbmask

SRC = \
    subjectwmask-main \
    cbitmaskbuilder \
	util \
    getopt \
    cseqcoding \
    cprogressindicator

LIB = bdb $(BLAST_LIBS) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS) $(BERKELEYDB_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS) $(BERKELEYDB_CXXFLAGS)
ifdef NOASSERT 
CXXFLAGS += -DNOASSERT
endif

ORIG_CPPFLAGS = -I$(includedir) -I$(includedir)/internal $(CONF_CPPFLAGS)

LDFLAGS = $(LOCAL_LDFLAGS) $(FAST_LDFLAGS)



WATCHERS = rotmistr
