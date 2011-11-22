APP = phytree_calc_unit_test
SRC = phytree_calc_unit_test

LIB = xalgoalignnw xalgophytree fastme xalnmgr biotree \
      test_boost tables xobjread xobjutil $(OBJMGR_LIBS)

LIBS = $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) -D__C99FEATURES__

CHECK_CMD = phytree_calc_unit_test
CHECK_COPY = data

WATCHERS = blastsoft
