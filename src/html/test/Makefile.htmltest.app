#################################
# $Id$
#################################

# Test for the "XHTML" library
#################################

APP = htmltest
OBJ = htmltest
LIB = xhtml xncbi

LIBS = $(ORIG_LIBS) -L$(NCBI)/ver0.0/ncbi/lib -lfcgi`sh -c 'CC -V 2>&1'|cut -f4 -d' '` -lnsl -lsocket

