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
	install_fltk       \
	install_berkeleydb \
	install_openssl     \
	install_sqlite     \
	install_sqlite3    \
	install_wxwindows  \
	install_wxwidgets  \
	install_sybase     \
	install_mysql      \
	install_mssql      \
	install_openssl    \
	install_lzo

CLEAN_THIRD_PARTY_LIBS = \
	clean_fltk       \
	clean_berkeleydb \
	clean_sqlite     \
	clean_sqlite3    \
	clean_wxwindows  \
	clean_wxwidgets  \
	clean_sybase     \
	clean_mysql      \
	clean_mssql      \
	clean_openssl    \
	clean_lzo

all : dirs $(THIRD_PARTY_LIBS)

clean : $(CLEAN_THIRD_PARTY_LIBS)

rebuild : clean all

dirs :
    @if not exist $(INSTALL_BINPATH) (echo Creating directory $(INSTALL_BINPATH)... & mkdir $(INSTALL_BINPATH))
