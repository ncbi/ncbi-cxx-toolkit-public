# $Id$

REQUIRES = objects

APP = objects_sample
SRC = objects_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi
# The serialization code needs the C Toolkit corelib, but only if it's present.
LIBS     = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS
