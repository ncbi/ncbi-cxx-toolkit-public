# $Id$
# Author:  Lewis Y. Geer

# Build application "omssacl"
#################################

LIBS =  $(ORIG_LIBS) $(NCBI_C_LIBPATH) -lncbitool -lncbiobj -lncbi

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(srcdir) $(NCBI_C_INCLUDE)

APP = omssacl

SRC = omssacl

LIB = xomssa omssa xser xutil xncbi xregexp regexp


