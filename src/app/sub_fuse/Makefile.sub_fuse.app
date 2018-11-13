# Build application "sub_fuse"
#################################

APP = sub_fuse
SRC = sfuse subs_collector

LIB = submit xcleanup xobjedit taxon3 xconnect valid xobjutil xregexp $(PCRE_LIB) $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS = dobronad
