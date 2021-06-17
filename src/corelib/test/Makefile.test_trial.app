#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

APP = test_trial
SRC = test_trial
LIB = xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = vasilche
