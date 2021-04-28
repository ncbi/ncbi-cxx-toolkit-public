#################################
# $Id$
#################################

REQUIRES = dbapi FreeTDS Boost.Test.Included

APP = id64_unit_test
SRC = id64_unit_test
LIB = test_boost xobjsimple xobjutil ncbi_xdbapi_ftds $(OBJMGR_LIBS) $(FTDS_LIB)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = all_readers.sh id64_unit_test.ini

CHECK_CMD = all_readers.sh id64_unit_test /CHECK_NAME=id64_unit_test
CHECK_TIMEOUT = 800

WATCHERS = vasilche
