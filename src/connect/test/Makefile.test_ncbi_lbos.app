# $Id$

###  BASIC PROJECT SETTINGS
APP = test_ncbi_lbos
SRC = test_ncbi_lbos
# OBJ =

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xconnect xncbi $(NCBIATOMIC_LIB) test_boost

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

#LINK = purify $(ORIG_LINK)

REQUIRES = Boost.Test.Included

# Comment out if you do not want it to run automatically as part of
# "make check".
CHECK_CMD =
CHECK_COPY = test_ncbi_lbos.ini

WATCHERS = elisovdn
# If your test application uses config file, then uncomment this line -- and,
# remember to rename 'ncbi_lbos_unittests.ini' to '<your_app_name>.ini'.
#CHECK_COPY = ncbi_lbos_unittests.ini

###  EXAMPLES OF OTHER SETTINGS THAT MIGHT BE OF INTEREST
# PRE_LIBS = $(NCBI_C_LIBPATH) .....
# CFLAGS   = $(FAST_CFLAGS)
# CXXFLAGS = $(FAST_CXXFLAGS)
# LDFLAGS  = $(FAST_LDFLAGS)
