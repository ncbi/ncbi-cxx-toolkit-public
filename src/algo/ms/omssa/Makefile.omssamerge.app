# Author:  Lewis Y. Geer

# Build application "omssamerge"
#################################

# pcre moved into platform specific make 
# LIBS = $(PCRE_LIBS) $(ORIG_LIBS)

CPPFLAGS = -I$(top_srcdir)/src/algo/ms/omssa $(ORIG_CPPFLAGS)

APP = omssamerge

SRC = omssamerge

LIB = xomssa omssa blast composition_adjustment seqdb blastdb tables \
       seqset $(SEQ_LIBS) pub medline biblio general \
       xser xconnect xregexp xutil xncbi

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

