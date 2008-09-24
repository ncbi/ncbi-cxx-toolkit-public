#############################################################################
###  EDIT SETTINGS FOR THE DEFAULT (APPLICATION) TARGET HERE              ###
APP = oligofar.fadice
SRC = \
        capp \
		getopt \
		cfadiceapp \
        fadice-main \
		cseqcoding \
        cprogressindicator
# OBJ =

# PRE_LIBS = $(NCBI_C_LIBPATH) .....

LIB = bdb xobjsimple $(BLAST_LIBS) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS) $(BERKELEYDB_LIBS)

# These settings are necessary for optimized WorkShop builds, due to
# BLAST's own use of them.
CXXFLAGS = $(FAST_CXXFLAGS) $(BERKELEYDB_CXXFLAGS)
ORIG_CPPFLAGS = -I$(includedir) -I$(includedir)/internal $(CONF_CPPFLAGS)

### LOCAL_LDFLAGS automatically added
LDFLAGS = $(LOCAL_LDFLAGS) $(FAST_LDFLAGS)

REQUIRES = objects -Cygwin

# CFLAGS   = $(ORIG_CFLAGS)
# CXXFLAGS = $(ORIG_CXXFLAGS)
# LDFLAGS  = $(ORIG_LDFLAGS)
#                                                                         ###
#############################################################################
