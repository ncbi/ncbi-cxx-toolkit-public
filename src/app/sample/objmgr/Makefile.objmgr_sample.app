#################################
# $Id$
#################################

REQUIRES = objects dbapi

APP = objmgr_sample
SRC = objmgr_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = xobjutil $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS

CHECK_CMD = objmgr_sample -gi 178614
CHECK_CMD = objmgr_sample -gi 18565551
CHECK_CMD = objmgr_sample -gi 19550966
CHECK_CMD = objmgr_sample -gi 455025
