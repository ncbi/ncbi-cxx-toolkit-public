# $Id$

APP = http_connector_hit
SRC = http_connector_hit
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = http_connector_hit www.ncbi.nlm.nih.gov 80 /Service/bounce.cgi 'arg1+arg2+arg3' /CHECK_NAME=http_connector_hit
