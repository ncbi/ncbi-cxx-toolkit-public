# $Id$
# Author:  Lewis Y. Geer

# Build application "omssacl"
#################################

NCBI_C_LIBS =  -lncbitool -lncbiobj -lncbi

LIBS =  $(ORIG_LIBS) $(NCBI_C_LIBPATH) $(NCBI_C_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)

APP = omssacl

SRC = omssacl

LIB = xomssa omssa xser xutil xncbi xregexp regexp

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
