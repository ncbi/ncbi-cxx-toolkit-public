#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "test_serial"
#################################

APP = test_serial
SRC = serialobject serialobject_Base test_serial test_cserial test_common cppwebenv twebenv

LIB = test_boost we_cpp xcser xser xutil xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(ORIG_LIBS)

CHECK_CMD  =
CHECK_COPY = webenv.ent webenv.bin ctest_serial.asn cpptest_serial.asn ctest_serial.asb cpptest_serial.asb

WATCHERS = gouriano
