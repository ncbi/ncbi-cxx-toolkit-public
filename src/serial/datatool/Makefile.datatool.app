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
	dtdaux dtdlexer dtdparser rpcgen aliasstr
LIB = xser xutil xncbi

# Build even --without-exe, to avoid version skew.
APP_OR_NULL = app

CHECK_CMD = datatool.sh
CHECK_CMD = datatool.sh /am/ncbiapdata/test_data/objects
CHECK_CMD = datatool_xml.sh
CHECK_REQUIRES = unix
CHECK_TIMEOUT = 600
