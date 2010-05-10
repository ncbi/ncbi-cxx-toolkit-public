#
# $Id$
#
#
#############################################################################
###  EDIT SETTINGS FOR THE DEFAULT (APPLICATION) TARGET HERE              ### 

REQUIRES = LIBXML LIBXSLT

APP = mzXML2hdf5
SRC = mzXML2hdf5 MzXmlReader
# OBJ =

# PRE_LIBS = $(NCBI_C_LIBPATH) .....

#LINK_WRAPPER = $(top_srcdir)/scripts/common/impl/favor-static

LIB = mshdf5 mzXML xmlwrapp general xregexp $(PCRE_LIB) xconnect xser xutil xncbi xcompress
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(LIBXML_LIBS) $(HDF5_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(LIBXML_INCLUDE) $(HDF5_INCLUDE)

#                                                                         ###
#############################################################################

WATCHERS = slottad
