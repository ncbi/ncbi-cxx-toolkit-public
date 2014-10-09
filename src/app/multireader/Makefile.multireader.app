#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "multireader"
#################################

APP =  multireader
SRC =  multireader
LIB =  xalgophytree biotree fastme xalnmgr tables xobjreadex $(OBJREAD_LIBS) \
       xobjutil xconnect xregexp $(PCRE_LIB) $(SOBJMGR_LIBS) \
       $(OBJEDIT_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects algo -Cygwin


WATCHERS = ludwigf kornbluh gotvyans
