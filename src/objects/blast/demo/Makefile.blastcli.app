APP = blastcli
SRC = blastcli

LIB = xobjmgr dbapi_driver blast scoremat blast id1 seqset $(SEQ_LIBS) \
      pub medline biblio general xser xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
