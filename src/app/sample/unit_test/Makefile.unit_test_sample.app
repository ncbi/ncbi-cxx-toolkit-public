# $Id$
APP = unit_test_sample
SRC = unit_test_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = $(SEQ_LIBS) pub medline biblio general xser xutil xncbi
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(ORIG_LIBS)

# Uncomment to run automatically as part of "make check"
# CHECK_CMD =
### END COPIED SETTINGS
