# $Id$

APP = mysql_lang
SRC = mysql_lang

LIB  = dbapi_driver_mysql dbapi_driver xncbi

MYSQL_INCLUDE = -I/usr/local/mysql/include/mysql
MYSQL_LIBS = -L/usr/local/mysql/lib/mysql -lmysqlclient

LIBS = $(MYSQL_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(MYSQL_INCLUDE) $(ORIG_CPPFLAGS)

