#################################
# $Id$
# Author:  Eugen Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build datatool application
#################################

APP = datatool
OBJ = type statictype enumtype reftype unitype blocktype \
	value module moduleset generate typestr filecode code \
	main alexer aparser parser lexer
LIB = xser xncbi

CPPFLAGS = -I. $(ORIG_CPPFLAGS)
LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(ORIG_LIBS)
