#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build DATATOOL application
#################################

APP = datatool
SRC = datatool \
	type namespace statictype enumtype reftype unitype blocktype choicetype \
	typestr ptrstr stdstr classstr enumstr stlstr choicestr choiceptrstr \
	value mcontainer module moduleset generate filecode code \
	fileutil alexer aparser parser lexer exceptions comments srcutil \
	dtdlexer dtdparser
LIB = xser xutil xncbi

CHECK_CMD = datatool.sh
CHECK_CMD = datatool.sh /net/sampson/a/coremake/test_data/objects
CHECK_REQUIRES = unix
