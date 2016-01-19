#################################
# $Id$
# Author:  Colleen Bollin
#################################

# Build application "tls"
#################################

APP = tls
SRC = tls

LIB  = $(OBJEDIT_LIBS) $(XFORMAT_LIBS) xobjutil \
       xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(LIBXSLT_LIBS) $(PCRE_LIBS)  \
       $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

POST_LINK = 

REQUIRES = objects 


WATCHERS = bollin
