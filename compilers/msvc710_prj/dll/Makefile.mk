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
	$(TEST_NOT_STAMP)\fltk$(STAMP_SUFFIX).installed       echo Copying FLTK DLLs...
	$(TEST_IF__STAMP)\fltk$(STAMP_SUFFIX).installed       echo FLTK DLLs are already installed
	$(TEST_NOT_STAMP)\fltk$(STAMP_SUFFIX).installed       copy /Y $(FLTK_SRC)\*.dll $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\fltk$(STAMP_SUFFIX).installed       if exist "$(FLTK_SRC)\*.pdb" copy /Y $(FLTK_SRC)\*.pdb $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\fltk$(STAMP_SUFFIX).installed       echo "" > $(THIRDPARTY_CFG_PATH)\fltk$(STAMP_SUFFIX).installed

BERKELEYDB_SRC      = $(BERKELEYDB_BINPATH)\$(INTDIR)
install_berkeleydb:
	$(TEST_NOT_STAMP)\berkeleydb$(STAMP_SUFFIX).installed echo Copying BerkeleyDB DLLs...
	$(TEST_IF__STAMP)\berkeleydb$(STAMP_SUFFIX).installed echo BerkeleyDB DLLs are already installed
	$(TEST_NOT_STAMP)\berkeleydb$(STAMP_SUFFIX).installed copy /Y $(BERKELEYDB_SRC)\*.dll $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\berkeleydb$(STAMP_SUFFIX).installed if exist "$(BERKELEYDB_SRC)\*.pdb" copy /Y $(BERKELEYDB_SRC)\*.pdb $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\berkeleydb$(STAMP_SUFFIX).installed echo "" > $(THIRDPARTY_CFG_PATH)\berkeleydb$(STAMP_SUFFIX).installed

SQLITE_SRC          = $(SQLITE_BINPATH)\$(INTDIR)
install_sqlite:
	$(TEST_NOT_STAMP)\sqlite$(STAMP_SUFFIX).installed     echo Copying SQLITE DLLs...
	$(TEST_IF__STAMP)\sqlite$(STAMP_SUFFIX).installed     echo SQLITE DLLs are already installed
	$(TEST_NOT_STAMP)\sqlite$(STAMP_SUFFIX).installed     copy /Y $(SQLITE_SRC)\*.dll $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\sqlite$(STAMP_SUFFIX).installed     if exist "$(SQLITE_SRC)\*.pdb" copy /Y $(SQLITE_SRC)\*.pdb $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\sqlite$(STAMP_SUFFIX).installed     echo "" > $(THIRDPARTY_CFG_PATH)\sqlite$(STAMP_SUFFIX).installed

#
# All other third-parties
#

WXWINDOWS_SRC       = $(WXWINDOWS_BINPATH)\$(INTDIR)
install_wxwindows:
	$(TEST_NOT_STAMP)\wxwindows$(STAMP_SUFFIX).installed  echo Copying wxWindows DLLs...
	$(TEST_IF__STAMP)\wxwindows$(STAMP_SUFFIX).installed  echo wxWindows DLLs are already installed
	$(TEST_NOT_STAMP)\wxwindows$(STAMP_SUFFIX).installed  copy /Y $(WXWINDOWS_SRC)\*.dll $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\wxwindows$(STAMP_SUFFIX).installed  if exist "$(WXWINDOWS_SRC)\*.pdb" copy /Y $(WXWINDOWS_SRC)\*.pdb $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\wxwindows$(STAMP_SUFFIX).installed  echo "" > $(THIRDPARTY_CFG_PATH)\wxwindows$(STAMP_SUFFIX).installed

SYBASE_SRC       = $(SYBASE_BINPATH)\$(INTDIR)
install_sybase:
	$(TEST_NOT_STAMP)\sybase$(STAMP_SUFFIX).installed     echo Copying Sybase DLLs...
	$(TEST_IF__STAMP)\sybase$(STAMP_SUFFIX).installed     echo Sybase DLLs are already installed
	$(TEST_NOT_STAMP)\sybase$(STAMP_SUFFIX).installed     copy /Y $(SYBASE_SRC)\*.dll $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\sybase$(STAMP_SUFFIX).installed     if exist "$(SYBASE_SRC)\*.pdb" copy /Y $(SYBASE_SRC)\*.pdb $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\sybase$(STAMP_SUFFIX).installed     echo "" > $(THIRDPARTY_CFG_PATH)\sybase$(STAMP_SUFFIX).installed


MYSQL_SRC        = $(MYSQL_BINPATH)\$(INTDIR)
install_mysql:
	$(TEST_NOT_STAMP)\mysql$(STAMP_SUFFIX).installed      echo Copying MySQL DLLs...
	$(TEST_IF__STAMP)\mysql$(STAMP_SUFFIX).installed      echo MySQL DLLs are already installed
	$(TEST_NOT_STAMP)\mysql$(STAMP_SUFFIX).installed      copy /Y $(MYSQL_SRC)\*.dll $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\mysql$(STAMP_SUFFIX).installed      if exist "$(MYSQL_SRC)\*.pdb" copy /Y $(MYSQL_SRC)\*.pdb $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\mysql$(STAMP_SUFFIX).installed      echo "" > $(THIRDPARTY_CFG_PATH)\mysql$(STAMP_SUFFIX).installed

MSSQL_SRC        = $(MSSQL_BINPATH)\$(INTDIR)
install_mssql:
	$(TEST_NOT_STAMP)\mssql$(STAMP_SUFFIX).installed      echo Copying MSSQL DLLs...
	$(TEST_IF__STAMP)\mssql$(STAMP_SUFFIX).installed      echo MSSQL DLLs are already installed
	$(TEST_NOT_STAMP)\mssql$(STAMP_SUFFIX).installed      copy /Y $(MSSQL_SRC)\*.dll $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\mssql$(STAMP_SUFFIX).installed      if exist "$(MSSQL_SRC)\*.pdb" copy /Y $(MSSQL_SRC)\*.pdb $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\mssql$(STAMP_SUFFIX).installed      echo "" > $(THIRDPARTY_CFG_PATH)\mssql$(STAMP_SUFFIX).installed

#
# MSVC7.10 run-time DLLs'
#
MSVCRT_SRC = \\snowman\win-coremake\Lib\ThirdParty\msvc\msvc71\7.1\bin
install_msvc:
	$(TEST_NOT_STAMP)\msvc$(STAMP_SUFFIX).installed       echo Copying MSVC DLLs...
	$(TEST_IF__STAMP)\msvc$(STAMP_SUFFIX).installed       echo MSVC DLLs are already installed
	$(TEST_NOT_STAMP)\msvc$(STAMP_SUFFIX).installed       copy /Y $(MSVCRT_SRC)\*.dll $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\msvc$(STAMP_SUFFIX).installed       if exist "$(MSVCRT_SRC)\*.pdb" copy /Y $(MSVCRT_SRC)\*.pdb $(INSTALL_BINPATH) > NUL
	$(TEST_NOT_STAMP)\msvc$(STAMP_SUFFIX).installed       echo "" > $(THIRDPARTY_CFG_PATH)\msvc$(STAMP_SUFFIX).installed
