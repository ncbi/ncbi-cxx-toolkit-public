#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "multireader"
#################################

APP =  multireader
SRC =  multireader multifile_source multifile_destination
LIB =  $(OBJEDIT_LIBS) $(XFORMAT_LIBS) \
       xalgophytree biotree fastme xalnmgr tables xobjreadex \
       xobjutil xconnect xregexp xcleanup $(PCRE_LIB) $(SOBJMGR_LIBS) 

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects algo -Cygwin


WATCHERS = ludwigf kornbluh gotvyans
