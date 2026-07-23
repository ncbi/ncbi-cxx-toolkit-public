# $Id$

APP = blast_usage_report_unit_test
SRC = blast_usage_report_unit_test 

CPPFLAGS = -DNCBI_MODULE=BLAST $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
LIB = blast_unit_test_util test_boost \
    $(BLAST_LIBS) xobjsimple $(OBJMGR_LIBS:ncbi_x%=ncbi_x%$(DLL))
LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) \
       $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

CHECK_CMD = blast_usage_report_unit_test

WATCHERS = fongah2
