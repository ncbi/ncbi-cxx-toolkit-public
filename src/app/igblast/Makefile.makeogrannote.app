##################################

ifeq (0,1)
	APP = makeogrannote.py
endif

WATCHERS = madden jianye
CHECK_COPY = makeogrannote.py
CHECK_CMD = perl -c makeogrannote.py
