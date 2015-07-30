APP = unit_test_eutils
SRC = unit_test_eutils

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = eutils_client \
	  xmlwrapp \
	  xregexp \
	  xutil \
	  test_boost \
	  xncbi

LIBS = $(LIBXSLT_STATIC_LIBS) \
	   $(LIBXML_STATIC_LIBS) \
	   $(DL_LIBS) $(ORIG_LIBS) $(PCRE_LIBS)

REQUIRES = Boost.Test.Included LIBXML LIBXSLT

WATCHERS = kotliaro

