# $Id$

# Build sample application "http_session_sample"
#################################

APP = http_session_sample
SRC = http_session_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = xconnect xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

## If you need the C toolkit...
# LIBS     = $(NCBI_C_LIBPATH) -lncbi $(NETWORK_LIBS) $(ORIG_LIBS)
# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
### END COPIED SETTINGS

CHECK_CMD  =
CHECK_COPY =

WATCHERS = grichenk
