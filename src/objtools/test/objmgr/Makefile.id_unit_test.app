#################################
# $Id$
#################################

REQUIRES = dbapi FreeTDS Boost.Test.Included

APP = id_unit_test
SRC = id_unit_test
LIB = test_boost xobjsimple xobjutil ncbi_xdbapi_ftds $(OBJMGR_LIBS) $(FTDS_LIB)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = all_readers.sh id_unit_test.ini

CHECK_CMD = all_readers.sh id_unit_test /CHECK_NAME=id_unit_test
CHECK_CMD = id_unit_test -psg /CHECK_NAME=id_unit_test_psg
CHECK_CMD = id_unit_test -id2-service ID2_SNP2_DEV /CHECK_NAME=id_unit_test_osg_psg
CHECK_TIMEOUT = 800

WATCHERS = vasilche
