#################################
# $Id$
# Author:  Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#################################

# Build test application "cgidemo"
#################################

APP = cgidemo
OBJ = cgidemo
LIB = xcgi xncbi

LIBS = $(FASTCGI_LIBS) $(ORIG_LIBS)
