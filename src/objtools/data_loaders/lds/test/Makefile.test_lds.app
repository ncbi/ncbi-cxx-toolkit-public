REQUIRES = Boost.Test.Included bdb

APP = test_lds
SRC = test_lds

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = ncbi_xloader_lds lds xobjread xobjutil bdb test_boost $(SOBJMGR_LIBS)
LIBS = $(BERKELEYDB_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = test.data
CHECK_CMD  = test_lds

WATCHERS = vasilche
