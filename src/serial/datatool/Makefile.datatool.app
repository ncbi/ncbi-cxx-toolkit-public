#################################
# $Id$
# Author:  Eugen Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build datatool application
#################################

APP = datatool
OBJ = moduleset alexer aparser parser lexer type value module main generate
LIB = xser xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) -I.

LIBS = -L$(NCBI_C_LIB) -lncbi $(ORIG_LIBS)
