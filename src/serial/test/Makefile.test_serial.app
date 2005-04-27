#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "test_serial"
#################################

APP = test_serial
SRC = serialobject serialobject_Base test_serial cppwebenv twebenv

DATATOOL_SRC = we_cpp

LIB = we_cpp xcser xser xutil xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(ORIG_LIBS)

CHECK_CMD  = test_serial.sh
CHECK_COPY = test_serial.sh
