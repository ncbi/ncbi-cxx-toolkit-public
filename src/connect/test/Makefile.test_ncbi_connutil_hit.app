# $Id$

APP = test_ncbi_connutil_hit
OBJ = test_ncbi_connutil_hit
LIB = connect

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_ncbi_connutil_hit neptune 80 /Service/bounce.cgi "" Makefile
