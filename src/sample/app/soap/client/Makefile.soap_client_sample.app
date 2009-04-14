#################################
# $Id$
#################################

APP = soap_client_sample
SRC = soap_client_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = soap_dataobj xsoap xcgi xconnect xser xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS
#CHECK_CMD = soap_client_sample
