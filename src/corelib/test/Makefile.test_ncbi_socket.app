# $Id$

APP = test_ncbi_socket
OBJ = test_ncbi_socket
LIB = xncbi

CPPFLAGS = -I$(srcdir)/.. $(ORIG_CPPFLAGS)
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
