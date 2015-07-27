#
# Makefile:  Makefile.unit_test_biosample_util.app
#
#

###  BASIC PROJECT SETTINGS
APP = unit_test_biosample_util
SRC = unit_test_biosample_util
# OBJ =

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xunittestutil xbiosample_util xvalidate xcleanup valerr \
      $(XFORMAT_LIBS) xalnmgr xobjutil tables \
      macro xregexp xmlwrapp xser $(PCRE_LIB) $(OBJREAD_LIBS) $(OBJMGR_LIBS) \
      test_boost xncbi \
      $(OBJEDIT_LIBS)

LIBS = $(PCRE_LIBS) \
       $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS) $(LIBXML_LIBS) $(LIBXSLT_LIBS)

REQUIRES = Boost.Test.Included

# Comment out if you do not want it to run automatically as part of
# "make check".
CHECK_CMD =
# If your test application uses config file, then uncomment this line -- and,
# remember to rename 'unit_test_discrepancy_report.ini' to '<your_app_name>.ini'.
#CHECK_COPY = unit_test_discrepancy_report.ini

###  EXAMPLES OF OTHER SETTINGS THAT MIGHT BE OF INTEREST
# PRE_LIBS = $(NCBI_C_LIBPATH) .....
# CFLAGS   = $(FAST_CFLAGS)
# CXXFLAGS = $(FAST_CXXFLAGS)
# LDFLAGS  = $(FAST_LDFLAGS)

WATCHERS = bollin
