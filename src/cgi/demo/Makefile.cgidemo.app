#################################
# $Id$
# Author:  Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#################################

# Build test CoreLib application "cgidemo"
#################################

APP = cgidemo
OBJ = cgidemo
LIB = xncbi

LIBS = $(FASTCGI_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
