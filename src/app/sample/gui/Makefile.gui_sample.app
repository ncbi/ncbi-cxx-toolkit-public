# $Id$

APP = gui_sample
SRC = gui_sample
REQUIRES = FLTK

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
# LIB      = xncbi
# LIB      = xser xhtml xcgi xconnect xutil xncbi
LIBS       = $(FLTK_LIBS) $(ORIG_LIBS)
CPPFLAGS   = $(ORIG_CPPFLAGS) $(FLTK_INCLUDE)
### END COPIED SETTINGS

