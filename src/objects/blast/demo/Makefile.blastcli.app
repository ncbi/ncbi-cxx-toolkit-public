APP = blastcli
SRC = blastcli

LIB = xobjmgr dbapi_driver xnetblastcli xnetblast scoremat xnetblast id1 \
      seqset $(SEQ_LIBS) pub medline biblio general xser xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
