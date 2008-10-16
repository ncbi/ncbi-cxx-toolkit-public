# $Id$

APP = test_ncbi_http_get
SRC = test_ncbi_http_get
LIB = connect connssl $(NCBIATOMIC_LIB)

LIBS = $(GNUTLS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_http_get 'http://www.ncbi.nlm.nih.gov/entrez/viewer.cgi?view=0&maxplex=1&save=idf&val=4959943' /CHECK_NAME=test_ncbi_http_get
