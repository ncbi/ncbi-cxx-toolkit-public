# $Id$

APP = test_ncbi_connutil_misc
SRC = test_ncbi_connutil_misc
LIB = connect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)
#LINK = purify $(C_LINK)

CHECK_CMD =
