#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "sra_test"
#################################

APP = bam2graph
SRC = bam2graph

LIB =   bamread xobjreadex submit xobjutil $(OBJMGR_LIBS) $(BAM_LIBS)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

WATCHERS = vasilche ucko
