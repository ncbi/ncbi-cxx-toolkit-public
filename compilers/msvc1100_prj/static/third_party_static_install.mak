# $Id$
#################################################################


INSTALL          = .\bin
INSTALL_BINPATH  = $(INSTALL)\$(INTDIR)
THIRDPARTY_MAKEFILES_DIR =  .


META_MAKE = $(THIRDPARTY_MAKEFILES_DIR)\..\third_party_install.meta.mk
!IF EXIST($(META_MAKE))
!INCLUDE $(META_MAKE)
!ELSE
!ERROR  $(META_MAKE)  not found
!ENDIF

THIRD_PARTY_LIBS = \
	install_berkeleydb \
	install_gnutls     \
	install_glew       \
	install_lzo        \
	install_mssql      \
	install_mysql      \
	install_openssl    \
	install_sqlite     \
	install_sqlite3    \
	install_sybase     \
	install_wxwidgets  \
	install_wxwindows  \
	install_xalan      \
	install_xerces     \
	install_libxml     \
	install_libxslt

CLEAN_THIRD_PARTY_LIBS = \
	clean_berkeleydb \
	clean_gnutls     \
	clean_glew       \
	clean_lzo        \
	clean_mssql      \
	clean_mysql      \
	clean_openssl    \
	clean_sqlite     \
	clean_sqlite3    \
	clean_sybase     \
	clean_wxwidgets  \
	clean_wxwindows  \
	clean_xalan      \
	clean_xerces     \
	clean_libxml     \
	clean_libxslt

all : dirs $(THIRD_PARTY_LIBS)

clean : $(CLEAN_THIRD_PARTY_LIBS)

rebuild : clean all

dirs :
    @if not exist $(INSTALL_BINPATH) (echo Creating directory $(INSTALL_BINPATH)... & mkdir $(INSTALL_BINPATH))
