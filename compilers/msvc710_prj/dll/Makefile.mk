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
install_fltk:
	$(TEST_NOT_STAMP)\fltk.installed       echo Coping FLTK DLLs
	$(TEST_IF__STAMP)\fltk.installed       echo FLTK DLLs are already installed
	$(TEST_NOT_STAMP)\fltk.installed       copy /Y $(FLTK_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\fltk.installed       if exist "$(FLTK_SRC)\*.pdb" copy /Y $(FLTK_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\fltk.installed       echo "" > $(THIRDPARTY_CFG_PATH)\fltk.installed

BERKELEYDB_SRC      = $(BERKELEYDB_BINPATH)\$(INTDIR)
install_berkeleydb:
	$(TEST_NOT_STAMP)\berkeleydb.installed echo Coping BerkeleyDB DLLs
	$(TEST_IF__STAMP)\berkeleydb.installed echo BerkeleyDB DLLs are already installed
	$(TEST_NOT_STAMP)\berkeleydb.installed copy /Y $(BERKELEYDB_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\berkeleydb.installed if exist "$(BERKELEYDB_SRC)\*.pdb" copy /Y $(BERKELEYDB_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\berkeleydb.installed echo "" > $(THIRDPARTY_CFG_PATH)\berkeleydb.installed

SQLITE_SRC          = $(SQLITE_BINPATH)\$(INTDIR)
install_sqlite:
	$(TEST_NOT_STAMP)\sqlite.installed     echo Coping SQLITE DLLs
	$(TEST_IF__STAMP)\sqlite.installed     echo SQLITE DLLs are already installed
	$(TEST_NOT_STAMP)\sqlite.installed     copy /Y $(SQLITE_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\sqlite.installed     if exist "$(SQLITE_SRC)\*.pdb" copy /Y $(SQLITE_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\sqlite.installed     echo "" > $(THIRDPARTY_CFG_PATH)\sqlite.installed

#
# All other third-parties
#

WXWINDOWS_SRC       = $(WXWINDOWS_BINPATH)\$(INTDIR)
install_wxwindows:
	$(TEST_NOT_STAMP)\wxwindows.installed  echo Coping wxWindows DLLs
	$(TEST_IF__STAMP)\wxwindows.installed  echo wxWindows DLLs are already installed
	$(TEST_NOT_STAMP)\wxwindows.installed  copy /Y $(WXWINDOWS_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\wxwindows.installed  if exist "$(WXWINDOWS_SRC)\*.pdb" copy /Y $(WXWINDOWS_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\wxwindows.installed  echo "" > $(THIRDPARTY_CFG_PATH)\wxwindows.installed

SYBASE_SRC       = $(SYBASE_BINPATH)\$(INTDIR)
install_sybase:
	$(TEST_NOT_STAMP)\sybase.installed     echo Coping Sybase DLLs
	$(TEST_IF__STAMP)\sybase.installed     echo Sybase DLLs are already installed
	$(TEST_NOT_STAMP)\sybase.installed     copy /Y $(SYBASE_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\sybase.installed     if exist "$(SYBASE_SRC)\*.pdb" copy /Y $(SYBASE_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\sybase.installed     echo "" > $(THIRDPARTY_CFG_PATH)\sybase.installed


MYSQL_SRC        = $(MYSQL_BINPATH)\$(INTDIR)
install_mysql:
	$(TEST_NOT_STAMP)\mysql.installed      echo Coping MySQL DLLs
	$(TEST_IF__STAMP)\mysql.installed      echo MySQL DLLs are already installed
	$(TEST_NOT_STAMP)\mysql.installed      copy /Y $(MYSQL_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\mysql.installed      if exist "$(MYSQL_SRC)\*.pdb" copy /Y $(MYSQL_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\mysql.installed      echo "" > $(THIRDPARTY_CFG_PATH)\mysql.installed

MSSQL_SRC        = $(MSSQL_BINPATH)\$(INTDIR)
install_mssql:
	$(TEST_NOT_STAMP)\mssql.installed      echo Coping MSSQL DLLs
	$(TEST_IF__STAMP)\mssql.installed      echo MSSQL DLLs are already installed
	$(TEST_NOT_STAMP)\mssql.installed      copy /Y $(MSSQL_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\mssql.installed      if exist "$(MSSQL_SRC)\*.pdb" copy /Y $(MSSQL_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\mssql.installed      echo "" > $(THIRDPARTY_CFG_PATH)\mssql.installed

#
# MSVC7.10 run-time DLLs'
#
MSVCRT_SRC = \\snowman\win-coremake\Lib\ThirdParty\msvc\7.1\bin
install_msvc:
	$(TEST_NOT_STAMP)\msvc.installed       echo Coping MSVC DLLs
	$(TEST_IF__STAMP)\msvc.installed       echo MSVC DLLs are already installed
	$(TEST_NOT_STAMP)\msvc.installed       copy /Y $(MSVCRT_SRC)\*.dll $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\msvc.installed       if exist "$(MSVCRT_SRC)\*.pdb" copy /Y $(MSVCRT_SRC)\*.pdb $(INSTALL_BINPATH)
	$(TEST_NOT_STAMP)\msvc.installed       echo "" > $(THIRDPARTY_CFG_PATH)\msvc.installed
