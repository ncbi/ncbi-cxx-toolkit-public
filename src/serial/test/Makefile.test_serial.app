#################################
# $Id$
# Author:  Eugen Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = serialtest
OBJ = serialobject testserial webenv
LIB = xser xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(NCBI_C_INCLUDE) \
	-I$(top_srcdir)/src/internal/webenv/asn

LIBS = -L$(NCBI_C_LIB) -lncbi $(ORIG_LIBS)
