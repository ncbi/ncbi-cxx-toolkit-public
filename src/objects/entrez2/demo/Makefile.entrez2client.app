# ===================================
# $Id$
#
# Meta-makefile for entrez2 command-line test app
# Author: Mike DiCuccio
# ===================================

APP = entrez2client
SRC = entrez2client
LIB = entrez2cli entrez2 xconnect xser xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects


WATCHERS = dicuccio
