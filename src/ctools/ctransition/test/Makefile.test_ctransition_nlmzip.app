# $Id$

APP = test_ctransition_nlmzip
SRC = test_ctransition_nlmzip
LIB = ctransition_nlmzip ctransition xcompress $(CMPRS_LIB) xutil xncbi
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(CMPRS_INCLUDE)

CHECK_CMD = 

WATCHERS = ivanov
