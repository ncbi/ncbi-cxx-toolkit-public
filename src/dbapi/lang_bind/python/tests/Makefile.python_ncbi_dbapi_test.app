# $Id$

APP = python_ncbi_dbapi_test
SRC = python_ncbi_dbapi_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(PYTHON_INCLUDE)

LIB  = xutil xncbi
LIBS = $(PYTHON_LIBS) $(ORIG_LIBS)
