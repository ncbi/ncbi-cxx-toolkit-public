# $Id$

APP = unit_test_alt_sample
SRC = unit_test_alt_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xncbi test_boost
LIBS = $(ORIG_LIBS)
PRE_LIBS = $(BOOST_LIBS)

REQUIRES = Boost.Test

# Uncomment to run automatically as part of "make check"
# CHECK_CMD =
### END COPIED SETTINGS
