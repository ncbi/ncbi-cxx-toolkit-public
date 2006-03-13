# Author:  Lewis Y. Geer

# Build application "omssamerge"
#################################

# pcre moved into platform specific make 
# LIBS = $(PCRE_LIBS) $(ORIG_LIBS)

CPPFLAGS = -I$(top_srcdir)/src/algo/ms/omssa $(ORIG_CPPFLAGS)

APP = omssamerge

SRC = omssamerge

LIB = xomssa$(STATIC) omssa$(STATIC) seqset$(STATIC) seq$(STATIC) seqcode$(STATIC) \
      sequtil$(STATIC) pub$(STATIC) medline$(STATIC) biblio$(STATIC) general$(STATIC) \
      xser$(STATIC) xconnect$(STATIC) xutil$(STATIC) xncbi$(STATIC)

