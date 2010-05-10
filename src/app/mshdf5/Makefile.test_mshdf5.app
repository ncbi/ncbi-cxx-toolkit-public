#
# $Id$
#
#
#############################################################################
###  EDIT SETTINGS FOR THE DEFAULT (APPLICATION) TARGET HERE              ### 
APP = test_mshdf5
SRC = test_mshdf5
# OBJ =

# PRE_LIBS = $(NCBI_C_LIBPATH) .....

#LINK_WRAPPER = $(top_srcdir)/scripts/common/impl/favor-static

LIB = mshdf5 mzXML general xregexp $(PCRE_LIB) xconnect xser xutil xncbi xcompress
## If you need the C toolkit...
# LIBS     = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(HDF5_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(HDF5_INCLUDE)

# CFLAGS   = $(ORIG_CFLAGS)
# CXXFLAGS = $(ORIG_CXXFLAGS)
LDFLAGS  =  $(ORIG_LDFLAGS) $(HDF5_LIBS) 

#                                                                         ###
#############################################################################

WATCHERS = slottad
