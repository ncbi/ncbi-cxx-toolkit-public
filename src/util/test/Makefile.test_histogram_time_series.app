# $Id$
# Author:  Sergey Satskiy (satskyse@ncbi.nlm.nih.gov)

# Build CHistogramTimeSeries test application "test_histogram_time_series"
#################################

APP = test_histogram_time_series
SRC = test_histogram_time_series
LIB = xutil test_boost xncbi
REQUIRES = Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

CHECK_CMD =

WATCHERS = satskyse
