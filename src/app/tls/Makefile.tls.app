#################################
# $Id$
# Author:  Colleen Bollin
#################################

# Build application "tls"
#################################

APP = tls
SRC = tls

LIB = $(OBJEDIT_LIBS) $(XFORMAT_LIBS) xalnmgr xobjutil \
       xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(VDB_LIBS) \
       $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects $(VDB_REQ)

WATCHERS = bollin
