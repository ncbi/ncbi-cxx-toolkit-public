# $Id$
# Author:  Lewis Y. Geer

# Build application "omssacl"
#################################

APP = readresult

SRC = readresult

LIB = omssa seqset seq seqcode pub medline biblio general xser xregexp $(PCRE_LIB) xutil xncbi
LIBS = $(PCRE_LIBS) $(ORIG_LIBS)
