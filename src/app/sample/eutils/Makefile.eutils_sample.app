#################################
# $Id$
#################################

APP = eutils_sample
SRC = eutils_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = $(EUTILS_LIBS) seqset $(SEQ_LIBS) pub medline biblio general \
      xser xcgi xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects
### END COPIED SETTINGS
