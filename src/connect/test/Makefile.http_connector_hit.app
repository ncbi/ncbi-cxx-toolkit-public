# $Id$

APP = http_connector_hit
SRC = http_connector_hit
LIB = connect

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = http_connector_hit graceland.ncbi.nlm.nih.gov 6224 /ieb/ToolBox/NETWORK/con_url.cgi 'arg1+arg2+arg3'
