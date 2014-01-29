# $Id$


APP = hgvs2variation
SRC = hgvs2variation

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) -Wno-unused-local-typedefs

LIB_ = hgvs $(OBJREAD_LIBS) \
       seq entrez2cli entrez2 xregexp $(PCRE_LIB) xobjutil $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
