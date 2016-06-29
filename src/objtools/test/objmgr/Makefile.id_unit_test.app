#################################
# $Id$
#################################

REQUIRES = dbapi FreeTDS Boost.Test.Included

APP = id_unit_test
SRC = id_unit_test
LIB = test_boost xobjutil ncbi_xdbapi_ftds $(OBJMGR_LIBS) $(FTDS_LIB)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = all_readers.sh

CHECK_CMD = all_readers.sh id_unit_test
CHECK_TIMEOUT = 400

WATCHERS = vasilche elisovdn
