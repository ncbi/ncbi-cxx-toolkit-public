APP = gumbelparams_unit_test
SRC = general_score_matrix_unit_test gumbel_params_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
LDFLAGS  = $(FAST_LDFLAGS)

LIB = test_boost xncbi gumbelparams tables xutil

REQUIRES = Boost.Test.Included

CHECK_CMD = gumbelparams_unit_test
CHECK_COPY = data

WATCHERS = blastsoft
