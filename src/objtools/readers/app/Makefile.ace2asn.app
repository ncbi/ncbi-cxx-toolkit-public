#################################
# $Id$
#################################

REQUIRES = objects

APP = ace2asn
SRC = ace2asn

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = xobjread pub medline biblio seqset $(SEQ_LIBS) general xser xutil xncbi
### END COPIED SETTINGS
