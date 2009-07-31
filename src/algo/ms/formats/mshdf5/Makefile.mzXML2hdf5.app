#
# $Id$
#
#
#############################################################################
###  EDIT SETTINGS FOR THE DEFAULT (APPLICATION) TARGET HERE              ### 
APP = mzXML2hdf5
SRC = mzXML2hdf5 BinCompressedInputStream CompressedInputSource InFile MzXmlReader
# OBJ =

# PRE_LIBS = $(NCBI_C_LIBPATH) .....

#LINK_WRAPPER = $(top_srcdir)/scripts/common/impl/favor-static

LIB = mshdf5 mzXML general xregexp $(PCRE_LIB) xconnect xser xutil xncbi xcompress
#LIB = mshdf5 mzXML xmlwrapp general xregexp $(PCRE_LIB) xconnect xser xutil xncbi xcompress
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(HDF5_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(XERCES_INCLUDE) $(HDF5_INCLUDE)
#CPPFLAGS = $(ORIG_CPPFLAGS) $(LIBXML_INCLUDE) $(HDF5_INCLUDE)

# CFLAGS   = $(ORIG_CFLAGS)
# CXXFLAGS = $(ORIG_CXXFLAGS)
LDFLAGS  =  $(ORIG_LDFLAGS) $(XERCES_LIBS) $(HDF5_LIBS)
#LDFLAGS  =  $(ORIG_LDFLAGS) $(LIBXML_LIBS) $(HDF5_LIBS)

#                                                                         ###
#############################################################################
