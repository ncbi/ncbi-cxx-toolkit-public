#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build DATATOOL application
#################################

APP = datatool
OBJ = datatool \
	type namespace statictype enumtype reftype unitype blocktype choicetype \
	typestr ptrstr stdstr classstr enumstr stlstr choicestr choiceptrstr \
	value mcontainer module moduleset generate filecode code \
	fileutil alexer aparser parser lexer exceptions comments srcutil
LIB = xser xutil xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(ORIG_LIBS)

CHECK_CMD = datatool.sh
