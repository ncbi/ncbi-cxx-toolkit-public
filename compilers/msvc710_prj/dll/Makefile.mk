# $Id$

#
# Rules for installation of third-party libraries.
# Uses definition provided by project tree builder configure.
#

!include $(THIRDPARTY_MAKEFILES_DIR)\$(INTDIR)\Makefile.third_party.mk


#
# GBENCH third-party libraries
#

install_fltk:
	@echo Coping FLTK DLLs
	copy /Y $(FLTK_BINPATH)\$(INTDIR)\*.dll $(INSTALL_BINPATH)

install_berkeleydb:
	@echo Coping BerkeleyDB DLLs
	copy /Y $(BERKELEYDB_BINPATH)\$(INTDIR)\*.dll $(INSTALL_BINPATH)

install_sqlite:
	@echo Coping SQLITE DLLs
	copy /Y $(SQLITE_BINPATH)\$(INTDIR)\*.dll $(INSTALL_BINPATH)

#
# All other third-parties
#

install_wxwindows:
	@echo Coping wxWindows DLLs
	copy /Y $(WXWINDOWS_BINPATH)\$(INTDIR)\*.dll $(INSTALL_BINPATH)

install_sybase:
	@echo Coping Sybase DLLs
	copy /Y $(SYBASE_BINPATH)\$(INTDIR)\*.dll $(INSTALL_BINPATH)

install_mysql:
	@echo Coping MySQL DLLs
	copy /Y $(MYSQL_BINPATH)\$(INTDIR)\*.dll $(INSTALL_BINPATH)

install_mssql:
	@echo Coping MSSQL DLLs
	copy /Y $(MSSQL_BINPATH)\$(INTDIR)\*.dll $(INSTALL_BINPATH)
