##################################

ifeq (0,1)
	APP = makeogrdb.py
endif

WATCHERS = madden jianye
CHECK_COPY = makeogrdb.py
CHECK_CMD = perl -c makeogrdb.py
