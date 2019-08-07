# $Id$

APP = blasthits_unit_test
SRC = blasthits_unit_test 

CPPFLAGS = -DNCBI_MODULE=BLAST $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) -I$(srcdir)/../../api \
           -I$(srcdir)/../../core 
LIB = blast_unit_test_util test_boost \
    $(BLAST_LIBS) xobjsimple $(OBJMGR_LIBS:ncbi_x%=ncbi_x%$(DLL)) 
LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = MT in-house-resources
CHECK_CMD = blasthits_unit_test
CHECK_COPY = blasthits_unit_test.ini

WATCHERS = boratyng madden camacho fongah2
