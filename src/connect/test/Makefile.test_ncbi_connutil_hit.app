# $Id$

APP = test_ncbi_connutil_hit
SRC = test_ncbi_connutil_hit
LIB = connect

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_connutil_hit yar 80 /Service/bounce.cgi "" test_ncbi_connutil_hit.dat
CHECK_COPY = test_ncbi_connutil_hit.dat
