# $Id$

#
# Rules for installation of third-party libraries.
# Uses definition provided by project tree builder configure.
#

#
# Include macrodefinitions provided by project tree builder
#
!include $(THIRDPARTY_MAKEFILES_DIR)\$(INTDIR)\Makefile.third_party.mk


#
# Common macros for third-party DLLs' installation
#
THIRDPARTY_CFG_PATH = $(THIRDPARTY_MAKEFILES_DIR)\$(INTDIR)
TEST_NOT_STAMP      = @if not exist $(THIRDPARTY_CFG_PATH)
TEST_IF__STAMP      = @if exist     $(THIRDPARTY_CFG_PATH)


#
# GBENCH third-party libraries
#


FLTK_SRC            = $(FLTK_BINPATH)\$(INTDIR)
!IF !EXIST($(FLTK_SRC))
FLTK_SRC            = $(FLTK_BINPATH)\$(ALTDIR)
!ENDIF
install_fltk:
	$(TEST_NOT_STAMP)\fltk.installed       echo Copying FLTK DLLs...
	$(TEST_IF__STAMP)\fltk.installed       echo FLTK DLLs are already installed
	$(TEST_NOT_STAMP)\fltk.installed       if exist "$(FLTK_SRC)\*.dll" copy /Y $(FLTK_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\fltk.installed       if exist "$(FLTK_SRC)\*.pdb" copy /Y $(FLTK_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\fltk.installed       echo "" > $(THIRDPARTY_CFG_PATH)\fltk.installed

BERKELEYDB_SRC      = $(BERKELEYDB_BINPATH)\$(INTDIR)
!IF !EXIST($(BERKELEYDB_SRC))
BERKELEYDB_SRC      = $(BERKELEYDB_BINPATH)\$(ALTDIR)
!ENDIF
install_berkeleydb:
	$(TEST_NOT_STAMP)\berkeleydb.installed echo Copying BerkeleyDB DLLs...
	$(TEST_IF__STAMP)\berkeleydb.installed echo BerkeleyDB DLLs are already installed
	$(TEST_NOT_STAMP)\berkeleydb.installed if exist "$(BERKELEYDB_SRC)\*.dll" copy /Y $(BERKELEYDB_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\berkeleydb.installed if exist "$(BERKELEYDB_SRC)\*.pdb" copy /Y $(BERKELEYDB_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\berkeleydb.installed echo "" > $(THIRDPARTY_CFG_PATH)\berkeleydb.installed

SQLITE_SRC          = $(SQLITE_BINPATH)\$(INTDIR)
!IF !EXIST($(SQLITE_SRC))
SQLITE_SRC          = $(SQLITE_BINPATH)\$(ALTDIR)
!ENDIF
install_sqlite:
	$(TEST_NOT_STAMP)\sqlite.installed     echo Copying SQLITE DLLs...
	$(TEST_IF__STAMP)\sqlite.installed     echo SQLITE DLLs are already installed
	$(TEST_NOT_STAMP)\sqlite.installed     if exist "$(SQLITE_SRC)\*.dll" copy /Y $(SQLITE_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\sqlite.installed     if exist "$(SQLITE_SRC)\*.pdb" copy /Y $(SQLITE_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\sqlite.installed     echo "" > $(THIRDPARTY_CFG_PATH)\sqlite.installed

SQLITE3_SRC         = $(SQLITE3_BINPATH)\$(INTDIR)
!IF !EXIST($(SQLITE3_SRC))
SQLITE3_SRC         = $(SQLITE3_BINPATH)\$(ALTDIR)
!ENDIF
install_sqlite3:
	$(TEST_NOT_STAMP)\sqlite3.installed    echo Copying SQLite3 DLLs...
	$(TEST_IF__STAMP)\sqlite3.installed    echo SQLite3 DLLs are already installed
	$(TEST_NOT_STAMP)\sqlite3.installed    if exist "$(SQLITE3_SRC)\*.dll" copy /Y $(SQLITE3_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\sqlite3.installed    if exist "$(SQLITE3_SRC)\*.pdb" copy /Y $(SQLITE3_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\sqlite3.installed    echo "" > $(THIRDPARTY_CFG_PATH)\sqlite3.installed

#
# All other third-parties
#

WXWINDOWS_SRC       = $(WXWINDOWS_BINPATH)\$(INTDIR)
!IF !EXIST($(WXWINDOWS_SRC))
WXWINDOWS_SRC       = $(WXWINDOWS_BINPATH)\$(ALTDIR)
!ENDIF
install_wxwindows:
	$(TEST_NOT_STAMP)\wxwindows.installed  echo Copying wxWindows DLLs...
	$(TEST_IF__STAMP)\wxwindows.installed  echo wxWindows DLLs are already installed
	$(TEST_NOT_STAMP)\wxwindows.installed  if exist "$(WXWINDOWS_SRC)\*.dll" copy /Y $(WXWINDOWS_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\wxwindows.installed  if exist "$(WXWINDOWS_SRC)\*.pdb" copy /Y $(WXWINDOWS_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\wxwindows.installed  echo "" > $(THIRDPARTY_CFG_PATH)\wxwindows.installed

SYBASE_SRC       = $(SYBASE_BINPATH)\$(INTDIR)
!IF !EXIST($(SYBASE_SRC))
SYBASE_SRC       = $(SYBASE_BINPATH)\$(ALTDIR)
!ENDIF
install_sybase:
	$(TEST_NOT_STAMP)\sybase.installed     echo Copying Sybase DLLs...
	$(TEST_IF__STAMP)\sybase.installed     echo Sybase DLLs are already installed
	$(TEST_NOT_STAMP)\sybase.installed     if exist "$(SYBASE_SRC)\*.dll" copy /Y $(SYBASE_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\sybase.installed     if exist "$(SYBASE_SRC)\*.pdb" copy /Y $(SYBASE_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\sybase.installed     echo "" > $(THIRDPARTY_CFG_PATH)\sybase.installed


MYSQL_SRC        = $(MYSQL_BINPATH)\$(INTDIR)
!IF !EXIST($(MYSQL_SRC))
MYSQL_SRC        = $(MYSQL_BINPATH)\$(ALTDIR)
!ENDIF
install_mysql:
	$(TEST_NOT_STAMP)\mysql.installed      echo Copying MySQL DLLs...
	$(TEST_IF__STAMP)\mysql.installed      echo MySQL DLLs are already installed
	$(TEST_NOT_STAMP)\mysql.installed      if exist "$(MYSQL_SRC)\*.dll" copy /Y $(MYSQL_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\mysql.installed      if exist "$(MYSQL_SRC)\*.pdb" copy /Y $(MYSQL_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\mysql.installed      echo "" > $(THIRDPARTY_CFG_PATH)\mysql.installed

MSSQL_SRC        = $(MSSQL_BINPATH)\$(INTDIR)
!IF !EXIST($(MSSQL_SRC))
MSSQL_SRC        = $(MSSQL_BINPATH)\$(ALTDIR)
!ENDIF
install_mssql:
	$(TEST_NOT_STAMP)\mssql.installed      echo Copying MSSQL DLLs...
	$(TEST_IF__STAMP)\mssql.installed      echo MSSQL DLLs are already installed
	$(TEST_NOT_STAMP)\mssql.installed      if exist "$(MSSQL_SRC)\*.dll" copy /Y $(MSSQL_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\mssql.installed      if exist "$(MSSQL_SRC)\*.pdb" copy /Y $(MSSQL_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\mssql.installed      echo "" > $(THIRDPARTY_CFG_PATH)\mssql.installed

OPENSSL_SRC      = $(OPENSSL_BINPATH)\$(INTDIR)
!IF !EXIST($(OPENSSL_SRC))
OPENSSL_SRC      = $(OPENSSL_BINPATH)\$(ALTDIR)
!ENDIF
install_openssl:
	$(TEST_NOT_STAMP)\openssl.installed    echo Copying OpenSSL DLLs...
	$(TEST_IF__STAMP)\openssl.installed    echo OpenSSL DLLs are already installed
	$(TEST_NOT_STAMP)\openssl.installed    if exist "$(OPENSSL_SRC)\*.dll" copy /Y $(OPENSSL_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\openssl.installed    echo "" > $(THIRDPARTY_CFG_PATH)\openssl.installed

LZO_SRC          = $(LZO_BINPATH)\$(INTDIR)
!IF !EXIST($(LZO_SRC))
LZO_SRC          = $(LZO_BINPATH)\$(ALTDIR)
!ENDIF
install_lzo:
	$(TEST_NOT_STAMP)\lzo.installed        echo Copying LZO DLLs...
	$(TEST_IF__STAMP)\lzo.installed        echo LZO DLLs are already installed
	$(TEST_NOT_STAMP)\lzo.installed        if exist "$(LZO_SRC)\*.dll" copy /Y $(LZO_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\lzo.installed        echo "" > $(THIRDPARTY_CFG_PATH)\lzo.installed


#
# MSVC7.10 run-time DLLs'
#
MSVCRT_SRC = \\snowman\win-coremake\Lib\ThirdParty\msvc\msvc71\7.1\bin
install_msvc:
	$(TEST_NOT_STAMP)\msvc.installed       echo Copying MSVC DLLs...
	$(TEST_IF__STAMP)\msvc.installed       echo MSVC DLLs are already installed
	$(TEST_NOT_STAMP)\msvc.installed       if exist "$(MSVCRT_SRC)\*.dll" copy /Y $(MSVCRT_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\msvc.installed       if exist "$(MSVCRT_SRC)\*.pdb" copy /Y $(MSVCRT_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\msvc.installed       echo "" > $(THIRDPARTY_CFG_PATH)\msvc.installed
