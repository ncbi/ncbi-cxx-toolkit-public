#################################
# $Id:
# Author:  Liangshou Wu
#################################


###  BASIC PROJECT SETTINGS
APP = test_uudutil
SRC = test_uudutil
# OBJ =

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = uudutil gbproj submit seqset $(SEQ_LIBS) pub medline biblio general \
      xser xconnserv xutil xconnect $(COMPRESS_LIBS) test_boost xncbi

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Uncomment if you do not want it to run automatically as part of
# "make check".
CHECK_CMD = test_uudutil
CHECK_COPY = test_uudutil.ini test_align_annot.asn test_gbproject.gbp

###  EXAMPLES OF OTHER SETTINGS THAT MIGHT BE OF INTEREST
# PRE_LIBS = $(NCBI_C_LIBPATH) .....
# CFLAGS   = $(FAST_CFLAGS)
# CXXFLAGS = $(FAST_CXXFLAGS)
# LDFLAGS  = $(FAST_LDFLAGS)
