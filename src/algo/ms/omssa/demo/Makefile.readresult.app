# $Id$
# Author:  Lewis Y. Geer

# Build application "omssacl"
#################################

WATCHERS = lewisg gorelenk

APP = readresult

SRC = readresult

LIB = omssa seqset seq seqcode sequtil pub medline biblio general xser xregexp $(PCRE_LIB) xutil xncbi
LIBS = $(PCRE_LIBS) $(ORIG_LIBS)
