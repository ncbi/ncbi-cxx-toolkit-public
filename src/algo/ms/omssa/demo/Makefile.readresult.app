# $Id$
# Author:  Lewis Y. Geer

# Build application "omssacl"
#################################

APP = readresult

SRC = readresult

LIB = omssa xser xregexp $(PCRE_LIB) xutil xncbi
LIBS = $(PCRE_LIBS) $(ORIG_LIBS)
