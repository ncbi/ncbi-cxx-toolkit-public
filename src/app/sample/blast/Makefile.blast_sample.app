#################################
# $Id$
#################################

APP = blast_sample
SRC = blast_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = xblast composition_adjustment xalgodustmask \
		xnetblastcli xnetblast xobjutil xobjsimple \
		scoremat seqdb blastdb tables $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects
### END COPIED SETTINGS
