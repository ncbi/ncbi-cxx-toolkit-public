APP = hgvsreader

SRC = hgvsreader

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = hgvsread varrep seq \
	  $(OBJREAD_LIBS) xregexp $(PCRE_LIB) xobjutil \
	  entrez2cli entrez2 $(OBJMGR_LIBS)


LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES =


WATCHERS = foleyjp
