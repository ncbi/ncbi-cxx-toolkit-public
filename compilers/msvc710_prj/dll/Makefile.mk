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


FLTK_SRC = $(FLTK_BINPATH)\$(INTDIR)

install_fltk : $(THIRDPARTY_CFG_PATH)\fltk$(STAMP_SUFFIX).installed

$(THIRDPARTY_CFG_PATH)\fltk$(STAMP_SUFFIX).installed : $(FLTK_SRC)/fltkdll.dll
    @echo $(FLTK_SRC)/fltkdll.dll
	@echo Copying FLTK DLLs...
	@if exist "$(FLTK_SRC)\fltkdll.dll" (copy /Y "$(FLTK_SRC)\fltkdll.dll" "$(INSTALL_BINPATH)" > NUL) else (echo WARNING: fltkdll.dll not found.)
	@if exist "$(FLTK_SRC)\fltkdll.pdb" (copy /Y "$(FLTK_SRC)\fltkdll.pdb" "$(INSTALL_BINPATH)" > NUL)
	@if exist "$(FLTK_SRC)\fltkdll.dll" (echo "" > "$(THIRDPARTY_CFG_PATH)\fltk$(STAMP_SUFFIX).installed")

$(FLTK_SRC)/fltkdll.dll :

BERKELEYDB_SRC = $(BERKELEYDB_BINPATH)\$(INTDIR)
install_berkeleydb:
	$(TEST_NOT_STAMP)\berkeleydb$(STAMP_SUFFIX).installed echo Copying BerkeleyDB DLLs...
	$(TEST_IF__STAMP)\berkeleydb$(STAMP_SUFFIX).installed echo BerkeleyDB DLLs are already installed
	$(TEST_NOT_STAMP)\berkeleydb$(STAMP_SUFFIX).installed if exist "$(BERKELEYDB_SRC)\*.dll" (copy /Y "$(BERKELEYDB_SRC)\*.dll" "$(INSTALL_BINPATH)" > NUL) else (echo WARNING: BerkeleyDB DLLs not found.)
	$(TEST_NOT_STAMP)\berkeleydb$(STAMP_SUFFIX).installed if exist "$(BERKELEYDB_SRC)\*.pdb" (copy /Y "$(BERKELEYDB_SRC)\*.pdb" "$(INSTALL_BINPATH)" > NUL)
	$(TEST_NOT_STAMP)\berkeleydb$(STAMP_SUFFIX).installed if exist "$(BERKELEYDB_SRC)\*.dll" (echo "" > "$(THIRDPARTY_CFG_PATH)\berkeleydb$(STAMP_SUFFIX).installed")

SQLITE_SRC = $(SQLITE_BINPATH)\$(INTDIR)
install_sqlite:
	$(TEST_NOT_STAMP)\sqlite$(STAMP_SUFFIX).installed     echo Copying SQLite DLLs...
	$(TEST_IF__STAMP)\sqlite$(STAMP_SUFFIX).installed     echo SQLite DLLs are already installed
	$(TEST_NOT_STAMP)\sqlite$(STAMP_SUFFIX).installed     if exist "$(SQLITE_SRC)\*.dll" (copy /Y "$(SQLITE_SRC)\*.dll" "$(INSTALL_BINPATH)" > NUL) else (echo WARNING: SQLite DLLs not found.)
	$(TEST_NOT_STAMP)\sqlite$(STAMP_SUFFIX).installed     if exist "$(SQLITE_SRC)\*.pdb" (copy /Y "$(SQLITE_SRC)\*.pdb" "$(INSTALL_BINPATH)" > NUL)
	$(TEST_NOT_STAMP)\sqlite$(STAMP_SUFFIX).installed     if exist "$(SQLITE_SRC)\*.dll" (echo "" > "$(THIRDPARTY_CFG_PATH)\sqlite$(STAMP_SUFFIX).installed")

SQLITE3_SRC = $(SQLITE3_BINPATH)\$(INTDIR)
install_sqlite3:
	$(TEST_NOT_STAMP)\sqlite3$(STAMP_SUFFIX).installed     echo Copying SQLite3 DLLs...
	$(TEST_IF__STAMP)\sqlite3$(STAMP_SUFFIX).installed     echo SQLite3 DLLs are already installed
	$(TEST_NOT_STAMP)\sqlite3$(STAMP_SUFFIX).installed     if exist "$(SQLITE3_SRC)\*.dll" (copy /Y "$(SQLITE3_SRC)\*.dll" "$(INSTALL_BINPATH)" > NUL) else (echo WARNING: SQLite3 DLLs not found.)
	$(TEST_NOT_STAMP)\sqlite3$(STAMP_SUFFIX).installed     if exist "$(SQLITE3_SRC)\*.pdb" (copy /Y "$(SQLITE3_SRC)\*.pdb" "$(INSTALL_BINPATH)" > NUL)
	$(TEST_NOT_STAMP)\sqlite3$(STAMP_SUFFIX).installed     if exist "$(SQLITE3_SRC)\*.dll" (echo "" > "$(THIRDPARTY_CFG_PATH)\sqlite3$(STAMP_SUFFIX).installed")

#
# All other third-parties
#

WXWINDOWS_SRC = $(WXWINDOWS_BINPATH)\$(INTDIR)
install_wxwindows:
	$(TEST_NOT_STAMP)\wxwindows$(STAMP_SUFFIX).installed  echo Copying wxWindows DLLs...
	$(TEST_IF__STAMP)\wxwindows$(STAMP_SUFFIX).installed  echo wxWindows DLLs are already installed
	$(TEST_NOT_STAMP)\wxwindows$(STAMP_SUFFIX).installed  if exist "$(WXWINDOWS_SRC)\*.dll" (copy /Y "$(WXWINDOWS_SRC)\*.dll" "$(INSTALL_BINPATH)" > NUL) else (echo WARNING: wxWindows DLLs not found.)
	$(TEST_NOT_STAMP)\wxwindows$(STAMP_SUFFIX).installed  if exist "$(WXWINDOWS_SRC)\*.pdb" (copy /Y "$(WXWINDOWS_SRC)\*.pdb" "$(INSTALL_BINPATH)" > NUL)
	$(TEST_NOT_STAMP)\wxwindows$(STAMP_SUFFIX).installed  if exist "$(WXWINDOWS_SRC)\*.dll" (echo "" > "$(THIRDPARTY_CFG_PATH)\wxwindows$(STAMP_SUFFIX).installed")

SYBASE_SRC = $(SYBASE_BINPATH)\$(INTDIR)
install_sybase:
	$(TEST_NOT_STAMP)\sybase$(STAMP_SUFFIX).installed     echo Copying Sybase DLLs...
	$(TEST_IF__STAMP)\sybase$(STAMP_SUFFIX).installed     echo Sybase DLLs are already installed
	$(TEST_NOT_STAMP)\sybase$(STAMP_SUFFIX).installed     if exist "$(SYBASE_SRC)\*.dll" (copy /Y "$(SYBASE_SRC)\*.dll" "$(INSTALL_BINPATH)" > NUL) else (echo WARNING: Sybase DLLs not found.)
	$(TEST_NOT_STAMP)\sybase$(STAMP_SUFFIX).installed     if exist "$(SYBASE_SRC)\*.pdb" (copy /Y "$(SYBASE_SRC)\*.pdb" "$(INSTALL_BINPATH)" > NUL)
	$(TEST_NOT_STAMP)\sybase$(STAMP_SUFFIX).installed     if exist "$(SYBASE_SRC)\*.dll" (echo "" > "$(THIRDPARTY_CFG_PATH)\sybase$(STAMP_SUFFIX).installed")


MYSQL_SRC = $(MYSQL_BINPATH)\$(INTDIR)
install_mysql:
	$(TEST_NOT_STAMP)\mysql$(STAMP_SUFFIX).installed      echo Copying MySQL DLLs...
	$(TEST_IF__STAMP)\mysql$(STAMP_SUFFIX).installed      echo MySQL DLLs are already installed
	$(TEST_NOT_STAMP)\mysql$(STAMP_SUFFIX).installed      if exist "$(MYSQL_SRC)\*.dll" (copy /Y "$(MYSQL_SRC)\*.dll" "$(INSTALL_BINPATH)" > NUL) else (echo WARNING: MySQL DLLs not found.)
	$(TEST_NOT_STAMP)\mysql$(STAMP_SUFFIX).installed      if exist "$(MYSQL_SRC)\*.pdb" (copy /Y "$(MYSQL_SRC)\*.pdb" "$(INSTALL_BINPATH)" > NUL)
	$(TEST_NOT_STAMP)\mysql$(STAMP_SUFFIX).installed      if exist "$(MYSQL_SRC)\*.dll" (echo "" > "$(THIRDPARTY_CFG_PATH)\mysql$(STAMP_SUFFIX).installed")

MSSQL_SRC = $(MSSQL_BINPATH)\$(INTDIR)
install_mssql:
	$(TEST_NOT_STAMP)\mssql$(STAMP_SUFFIX).installed      echo Copying MSSQL DLLs...
	$(TEST_IF__STAMP)\mssql$(STAMP_SUFFIX).installed      echo MSSQL DLLs are already installed
	$(TEST_NOT_STAMP)\mssql$(STAMP_SUFFIX).installed      if exist "$(MSSQL_SRC)\*.dll" (copy /Y "$(MSSQL_SRC)\*.dll" "$(INSTALL_BINPATH)" > NUL) else (echo WARNING: MSSQL DLLs not found.)
	$(TEST_NOT_STAMP)\mssql$(STAMP_SUFFIX).installed      if exist "$(MSSQL_SRC)\*.pdb" (copy /Y "$(MSSQL_SRC)\*.pdb" "$(INSTALL_BINPATH)" > NUL)
	$(TEST_NOT_STAMP)\mssql$(STAMP_SUFFIX).installed      if exist "$(MSSQL_SRC)\*.dll" (echo "" > "$(THIRDPARTY_CFG_PATH)\mssql$(STAMP_SUFFIX).installed")

OPENSSL_SRC = $(OPENSSL_BINPATH)\$(INTDIR)
install_openssl:
	$(TEST_NOT_STAMP)\openssl$(STAMP_SUFFIX).installed    echo Copying OpenSSL DLLs...
	$(TEST_IF__STAMP)\openssl$(STAMP_SUFFIX).installed    echo OpenSSL DLLs are already installed
	$(TEST_NOT_STAMP)\openssl$(STAMP_SUFFIX).installed    if exist "$(OPENSSL_SRC)\*.dll" (copy /Y "$(OPENSSL_SRC)\*.dll" "$(INSTALL_BINPATH)" > NUL) else (echo WARNING: OpenSSL DLLs not found.)
	$(TEST_NOT_STAMP)\openssl$(STAMP_SUFFIX).installed    if exist "$(OPENSSL_SRC)\*.dll" (echo "" > "$(THIRDPARTY_CFG_PATH)\openssl$(STAMP_SUFFIX).installed")

LZO_SRC = $(LZO_BINPATH)\$(INTDIR)
install_lzo:
	$(TEST_NOT_STAMP)\lzo$(STAMP_SUFFIX).installed        echo Copying LZO DLLs...
	$(TEST_IF__STAMP)\lzo$(STAMP_SUFFIX).installed        echo LZO DLLs are already installed
	$(TEST_NOT_STAMP)\lzo$(STAMP_SUFFIX).installed        if exist "$(LZO_SRC)\*.dll" (copy /Y "$(LZO_SRC)\*.dll" "$(INSTALL_BINPATH)" > NUL) else (echo WARNING: LZO DLLs not found.)
	$(TEST_NOT_STAMP)\lzo$(STAMP_SUFFIX).installed        if exist "$(LZO_SRC)\*.dll" (echo "" > "$(THIRDPARTY_CFG_PATH)\lzo$(STAMP_SUFFIX).installed")

#
# MSVC7.10 run-time DLLs'
#
MSVCRT_SRC = \\snowman\win-coremake\Lib\ThirdParty\msvc\msvc71\7.1\bin
install_msvc:
	$(TEST_NOT_STAMP)\msvc$(STAMP_SUFFIX).installed       echo Copying MSVC DLLs...
	$(TEST_IF__STAMP)\msvc$(STAMP_SUFFIX).installed       echo MSVC DLLs are already installed
	$(TEST_NOT_STAMP)\msvc$(STAMP_SUFFIX).installed       if exist "$(MSVCRT_SRC)\*.dll" (copy /Y "$(MSVCRT_SRC)\*.dll" "$(INSTALL_BINPATH)" > NUL) else (echo WARNING: MSVC DLLs not found.)
	$(TEST_NOT_STAMP)\msvc$(STAMP_SUFFIX).installed       if exist "$(MSVCRT_SRC)\*.pdb" (copy /Y "$(MSVCRT_SRC)\*.pdb" "$(INSTALL_BINPATH)" > NUL)
	$(TEST_NOT_STAMP)\msvc$(STAMP_SUFFIX).installed       if exist "$(MSVCRT_SRC)\*.dll" (echo "" > "$(THIRDPARTY_CFG_PATH)\msvc$(STAMP_SUFFIX).installed")
