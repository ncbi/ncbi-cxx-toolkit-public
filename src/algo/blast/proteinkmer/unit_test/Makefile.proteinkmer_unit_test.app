# $Id$

###  BASIC PROJECT SETTINGS
APP = proteinkmer_unit_test
SRC = proteinkmer_unit_test
# OBJ =

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) $(BLAST_THIRD_PARTY_INCLUDE)

LIB_ = test_boost proteinkmer $(BLAST_DB_DATA_LOADER_LIBS) \
       $(BLAST_LIBS) xobjsimple $(OBJMGR_LIBS)

LIB = $(LIB_:%=%$(STATIC))

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Comment out if you do not want it to run automatically as part of
# "make check".
CHECK_CMD =
# If your test application uses config file, then uncomment this line -- and,
# remember to rename 'foo.ini' to '<your_app_name>.ini'.
#CHECK_COPY = foo.ini
CHECK_COPY = data

###  EXAMPLES OF OTHER SETTINGS THAT MIGHT BE OF INTEREST
# PRE_LIBS = $(NCBI_C_LIBPATH) .....
# CFLAGS   = $(FAST_CFLAGS)
# CXXFLAGS = $(FAST_CXXFLAGS)
# LDFLAGS  = $(FAST_LDFLAGS)
