#################################
# $Id$
#################################

# Test for the "XHTML" library
#################################

APP = htmltest
OBJ = htmltest
LIB = xhtml xcgi xncbi

LIBS = $(FASTCGI_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
