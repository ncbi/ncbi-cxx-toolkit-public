#################################
# $Id$
# Author:  Eugen Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = serialtest
OBJ = serialobject serialobject_Base testserial cppwebenv twebenv
LIB = xser xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) \
  $(NCBI_C_INCLUDE) \
  -I$(top_srcdir)/src/serial/test/

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) \
  $(ORIG_LIBS)
