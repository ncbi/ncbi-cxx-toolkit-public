#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = serialtest
OBJ = serialobject serialobject_Base testserial cppwebenv twebenv \
      we_cpp__ we_cpp___
LIB = xser xutil xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE) -I$(srcdir) -I$(srcdir)/webenv

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(ORIG_LIBS)
