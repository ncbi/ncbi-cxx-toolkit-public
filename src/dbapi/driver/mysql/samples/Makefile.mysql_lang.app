# $Id$

APP = mysql_lang
SRC = mysql_lang

LIB  = ncbi_xdbapi_mysql dbapi_driver $(XCONNEXT) xconnect xncbi

LIBS = $(MYSQL_LIBS) $(Z_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(MYSQL_INCLUDE) $(Z_INCLUDE) $(ORIG_CPPFLAGS)

# CHECK_REQUIRES = in-house-resources
# CHECK_CMD = mysql_lang -S mysql-dev.ncbi.nlm.nih.gov -U cppcore -P chan8me -D cppcore

WATCHERS = ucko
