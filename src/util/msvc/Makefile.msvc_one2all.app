#################################
# $Id$

APP = one2all
SRC = one2all
LIB = xregexp $(PCRE_LIB) xutil xncbi
LIBS = $(PCRE_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(PCRE_INCLUDE)

#CHECK_CMD =
