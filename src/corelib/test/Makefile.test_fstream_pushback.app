# $Id$

APP = test_fstream_pushback
SRC = test_fstream_pushback
LIB = xpbacktest test_mt xncbi

#LINK = purify $(ORIG_LINK)

CHECK_CMD  = test_fstream_pushback.sh
CHECK_COPY = test_fstream_pushback.sh

WATCHERS = lavr
