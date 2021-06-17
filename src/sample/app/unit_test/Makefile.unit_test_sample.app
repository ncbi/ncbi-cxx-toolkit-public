# $Id$

APP = unit_test_sample
SRC = unit_test_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = test_boost xncbi

REQUIRES = Boost.Test.Included

# Comment out if you do not want it to run automatically as part of
# "make check".
CHECK_CMD =
# If your test application uses config file, then uncomment this line -- and,
# remember to rename 'unit_test_sample.ini' to '<your_app_name>.ini'.
#CHECK_COPY = unit_test_sample.ini
### END COPIED SETTINGS

WATCHERS = vakatov
