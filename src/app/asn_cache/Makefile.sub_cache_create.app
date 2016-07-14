# $Id$

APP = sub_cache_create
SRC = sub_cache_create

LIB  = $(DATA_LOADERS_UTIL_LIB) $(SRAREAD_LIBS) $(OBJMGR_LIBS)
LIBS = $(DATA_LOADERS_UTIL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

CHECK_CMD = python sub_cache_create_unit_test.py --missing-gis remote.gis --lds remote.lds2 --reference-index reference.idx --extract test.gis --scratch-directory tmp
CHECK_COPY = asn_cache_subprocess.py sub_cache_create_unit_test.py

WATCHERS = mozese2
