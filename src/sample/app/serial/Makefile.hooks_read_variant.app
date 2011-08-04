# $Id$

REQUIRES = serial

APP = hooks_read_variant
SRC = hooks_read_variant

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = seq seqcode pub medline biblio general xser sequtil xutil xncbi
LIBS = $(ORIG_LIBS)
# LIB = seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi
## If you need the C toolkit...
# LIBS     = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)
# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
### END COPIED SETTINGS

WATCHERS = ucko
