#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "formatguess"
#################################

APP = formatguess
SRC = formatguess
LIB = xobjread creaders seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xncbi

REQUIRES = objects -Cygwin


WATCHERS = ludwigf
