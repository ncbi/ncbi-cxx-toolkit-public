# $Id$

APP = test_ncbi_null
SRC = test_ncbi_null
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_null 'http://www.ncbi.nlm.nih.gov/Service/dispd.cgi?service=bounce'

WATCHERS = lavr
