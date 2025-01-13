#################################
# $Id$
#################################

APP = test_huge_file_process
SRC = test_huge_file_process

LIB = xhugeasn xobjread xlogging xutil test_boost $(SOBJMGR_LIBS) ncbi_xreader ncbi_xloader_genbank
LIBS = $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY = huge_asn_test_files

WATCHERS = gotvyans foleyjp

