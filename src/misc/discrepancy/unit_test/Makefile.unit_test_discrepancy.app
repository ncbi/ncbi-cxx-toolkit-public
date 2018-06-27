#
# Makefile:  Makefile.unit_test_discrepancy.app
#
#

###  BASIC PROJECT SETTINGS
APP = unit_test_discrepancy
SRC = unit_test_discrepancy

CPPFLAGS = $(BOOST_INCLUDE)

LIB = xdiscrepancy test_boost

LIBS = $(PCRE_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD = unit_test_discrepancy

WATCHERS = kachalos
