# $Id$

APP = test_qparse
SRC = test_qparse

CPPFLAGS = $(ORIG_CPPFLAGS) 

LIB  = xqueryparse xncbi
LIBS = $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = kuznets

