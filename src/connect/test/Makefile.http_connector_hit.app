# $Id$

APP = http_connector_hit
OBJ = http_connector_hit
LIB = connect

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = http_connector_hit ray.nlm.nih.gov 6224 /tools/vakatov/con_url.cgi 'arg1+arg2+arg3'
