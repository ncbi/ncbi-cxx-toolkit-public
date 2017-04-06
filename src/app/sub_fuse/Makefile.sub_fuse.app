# Build application "sub_fuse"
#################################

APP = sub_fuse
SRC = sfuse subs_collector

LIB = submit $(SOBJMGR_LIBS)

LIBS = $(ORIG_LIBS)

WATCHERS = dobronad
