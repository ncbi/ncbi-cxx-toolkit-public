# $Id$


APP = test_eutils_client
SRC = test_eutils_client

LIB  = eutils_client xmlwrapp xcgi $(CONNEXT) xconnect xutil xncbi

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(NETWORK_LIBS) \
	   $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = LIBXML LIBXSLT

CHECK_CMD = test_eutils_client -db pubmed -count dog  /CHECK_NAME=EUtilsCli_Dog

WATCHERS = perkovan smithrg grichenk
