# $Id$

# Build test CGI application "grid_cgi_sample"
#################################

APP = grid_cgi_sample.cgi
SRC = grid_cgi_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
## Use these two lines for normal CGI.
LIB = xgridcgi xconnserv xcgi xhtml xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
## Use these two lines for FastCGI.  (No other changes needed!)
# LIB = xgridcgi xconnserv xfcgi xhtml xconnect xutil xncbi
# LIBS = $(FASTCGI_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS

CHECK_CMD  =
CHECK_COPY = grid_cgi_sample.html
