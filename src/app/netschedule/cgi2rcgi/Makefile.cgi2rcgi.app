# $Id$

# Build test CGI application "cgi_tunnel2grid"
#################################

APP = cgi2rcgi
SRC = cgi2rcgi

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
## Use these two lines for normal CGI.
LIB = xgridcgi xconnserv xcgi xhtml xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS
