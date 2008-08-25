# $Id$

REQUIRES = objects

APP = autodef_demo
SRC = autodef_demo

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi xobjutil xobjedit $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

## If you need the C toolkit...
# LIBS     = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)
# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
### END COPIED SETTINGS
