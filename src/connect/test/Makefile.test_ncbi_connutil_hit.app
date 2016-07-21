# $Id$

APP = test_ncbi_connutil_hit
SRC = test_ncbi_connutil_hit
LIB = connect connssl $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_connutil_hit www.ncbi.nlm.nih.gov 443 /Service/bounce.cgi dummy test_ncbi_connutil_hit.dat /CHECK_NAME=test_ncbi_connutil_hit
CHECK_COPY = test_ncbi_connutil_hit.dat
CHECK_REQUIRES = GNUTLS

WATCHERS = lavr
