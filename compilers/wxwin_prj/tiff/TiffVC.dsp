# Microsoft Developer Studio Project File - Name="tiff" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=tiff - Win32 ReleaseMT
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "tiff.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "tiff.mak" CFG="tiff - Win32 ReleaseMT"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "tiff - Win32 ReleaseMT" (based on "Win32 (x86) Static Library")
!MESSAGE "tiff - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "tiff - Win32 DebugMT" (based on "Win32 (x86) Static Library")
!MESSAGE "tiff - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "tiff - Win32 ReleaseMT"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseMT"
# PROP BASE Intermediate_Dir "ReleaseMT"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseMT"
# PROP Intermediate_Dir "ReleaseMT"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /YX /FD /c
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\ReleaseMT\tiff.lib"

!ELSEIF  "$(CFG)" == "tiff - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /YX /FD /c
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\Release\tiff.lib"

!ELSEIF  "$(CFG)" == "tiff - Win32 DebugMT"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DebugMT"
# PROP BASE Intermediate_Dir "DebugMT"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugMT"
# PROP Intermediate_Dir "DebugMT"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /ZI /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "DEBUG=1" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /D "__WXDEBUG__=1" /YX /FD /GZ /c
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\DebugMT\tiff.lib"

!ELSEIF  "$(CFG)" == "tiff - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /GZ /c
# ADD CPP /nologo /MLd /W3 /Gm /GX /Zi /ZI /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "DEBUG=1" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /D "__WXDEBUG__=1" /YX /FD /GZ /c
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\Debug\tiff.lib"

!ENDIF 

# Begin Target

# Name "tiff - Win32 ReleaseMT"
# Name "tiff - Win32 Release"
# Name "tiff - Win32 DebugMT"
# Name "tiff - Win32 Debug"

# Begin Group "tiff Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_aux.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_close.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_codec.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_compress.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_dir.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_dirinfo.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_dirread.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_dirwrite.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_dumpmode.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_error.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_fax3.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_fax3sm.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_flush.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_getimage.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_jpeg.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_luv.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_lzw.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_next.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_open.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_packbits.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_pixarlog.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_predict.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_print.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_read.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_strip.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_swab.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_thunder.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_tile.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_version.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_warning.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_win32.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_write.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_zip.c

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\..\..\src\tiff\port.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\t4.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_dir.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_fax3.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tif_predict.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tiff.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tiffcomp.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tiffconf.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tiffio.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\tiffiop.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\tiff\uvcode.h
# End Source File
# End Group
# End Target
# End Project
