# $Id$

APP = test_bam_loader
SRC = test_bam_loader vdb_user_agent

REQUIRES = Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) $(SRA_INCLUDE)

LIB = ncbi_xloader_bam bamread $(BAM_LIBS) xobjreadex $(OBJREAD_LIBS) \
      xobjutil xobjsimple test_boost $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

CHECK_COPY = mapfile
CHECK_CMD = test_bam_loader
CHECK_TIMEOUT  = 500
CHECK_REQUIRES = in-house-resources -Solaris

WATCHERS = vasilche ucko
