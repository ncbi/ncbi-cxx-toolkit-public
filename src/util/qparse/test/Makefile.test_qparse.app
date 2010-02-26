# $Id$

APP = test_qparse
SRC = test_qparse

CPPFLAGS = $(ORIG_CPPFLAGS) 

LIB  = xncbi xqueryparse
LIBS = $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = kuznets

