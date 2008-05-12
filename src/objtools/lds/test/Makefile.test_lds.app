REQUIRES = Boost.Test objects bdb

APP = test_lds
SRC = test_lds

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = ncbi_xloader_lds lds xobjread bdb xobjutil $(SOBJMGR_LIBS)
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(BERKELEYDB_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = test.data
CHECK_CMD  = test_lds