#################################
# $Id$
# Author:  Eugen Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = serialtest
OBJ = serialobject testserial webenv rtti
LIB = xser xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(NCBI_C_INCLUDE) \
	-I$(top_srcdir)/src/internal/webenv/asn
