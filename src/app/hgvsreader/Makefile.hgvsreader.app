APP = hgvsreader

SRC = hgvsreader

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = hgvsread varrep \
          $(OBJREAD_LIBS) xobjutil xregexp $(PCRE_LIB) \
	  entrez2cli entrez2 $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES =


WATCHERS = foleyjp
