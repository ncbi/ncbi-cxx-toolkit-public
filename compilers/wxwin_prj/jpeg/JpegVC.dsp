# Microsoft Developer Studio Project File - Name="jpeg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=jpeg - Win32 ReleaseMT
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "jpeg.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "jpeg.mak" CFG="jpeg - Win32 ReleaseMT"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "jpeg - Win32 ReleaseMT" (based on "Win32 (x86) Static Library")
!MESSAGE "jpeg - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "jpeg - Win32 DebugMT" (based on "Win32 (x86) Static Library")
!MESSAGE "jpeg - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "jpeg - Win32 ReleaseMT"

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
# ADD CPP /nologo /MT /W3 /GX /O2 /I ".." /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /YX /FD /c
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\ReleaseMT\jpeg.lib"

!ELSEIF  "$(CFG)" == "jpeg - Win32 Release"

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
# ADD CPP /nologo /ML /W3 /GX /O2 /I ".." /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /YX /FD /c
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\Release\jpeg.lib"

!ELSEIF  "$(CFG)" == "jpeg - Win32 DebugMT"

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
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /ZI /Z7 /Od /I ".." /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "DEBUG=1" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /D "__WXDEBUG__=1" /YX /FD /GZ /c
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\DebugMT\jpeg.lib"

!ELSEIF  "$(CFG)" == "jpeg - Win32 Debug"

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
# ADD CPP /nologo /MLd /W3 /Gm /GX /Zi /ZI /Z7 /Od /I ".." /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "DEBUG=1" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /D "__WXDEBUG__=1" /YX /FD /GZ /c
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\Debug\jpeg.lib"

!ENDIF 

# Begin Target

# Name "jpeg - Win32 ReleaseMT"
# Name "jpeg - Win32 Release"
# Name "jpeg - Win32 DebugMT"
# Name "jpeg - Win32 Debug"

# Begin Group "JPEG Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcapimin.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcapistd.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jccoefct.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jccolor.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcdctmgr.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jchuff.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcinit.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcmainct.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcmarker.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcmaster.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcomapi.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcparam.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcphuff.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcprepct.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jcsample.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jctrans.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdapimin.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdapistd.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdatadst.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdatasrc.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdcoefct.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdcolor.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jddctmgr.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdhuff.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdinput.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdmainct.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdmarker.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdmaster.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdmerge.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdphuff.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdpostct.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdsample.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdtrans.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jerror.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jfdctflt.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jfdctfst.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jfdctint.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jidctflt.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jidctfst.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jidctint.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jidctred.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jmemmgr.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jmemnobs.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jquant1.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jquant2.c

# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jutils.c

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\..\..\src\jpeg\jchuff.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jconfig.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdct.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jdhuff.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jerror.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jinclude.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jmemsys.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jmorecfg.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jpegint.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jpeglib.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\jpeg\jversion.h
# End Source File
# End Group
# End Target
# End Project
