# Microsoft Developer Studio Project File - Name="wxvc_dll" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=wxvc_dll - Win32 ReleaseDLL
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "wxvc_dll.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "wxvc_dll.mak" CFG="wxvc_dll - Win32 ReleaseDLL"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "wxvc_dll - Win32 ReleaseDLL" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "wxvc_dll - Win32 DebugDLL" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "wxvc_dll - Win32 ReleaseDLL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\..\ReleaseDLL"
# PROP BASE Intermediate_Dir "ReleaseDLL"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\..\ReleaseDLL"
# PROP Intermediate_Dir "ReleaseDLL"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I ".\..\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /D "WXMAKINGDLL=1" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comctl32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib rpcrt4.lib winmm.lib opengl32.lib glu32.lib /nologo /incremental:no /subsystem:windows /dll /machine:I386 /out:".\..\ReleaseDLL\wx.dll" /pdbtype:sept

!ELSEIF  "$(CFG)" == "wxvc_dll - Win32 DebugDLL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\..\DebugDLL"
# PROP BASE Intermediate_Dir "DebugDLL"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\..\DebugDLL"
# PROP Intermediate_Dir "DebugDLL"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /ZI /Z7 /Od /I ".\..\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "DEBUG=1" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /D "__WXDEBUG__=1" /D "WXMAKINGDLL=1" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comctl32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib rpcrt4.lib winmm.lib opengl32.lib glu32.lib /nologo /incremental:no /subsystem:windows /dll /debug /machine:I386 /out:".\..\DebugDLL\wx.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "wxvc_dll - Win32 ReleaseDLL"
# Name "wxvc_dll - Win32 DebugDLL"

# Begin Group "Common Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\src\common\appcmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\choiccmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\clipcmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\cmndata.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\config.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\ctrlcmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\ctrlsub.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\datetime.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\datstrm.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\db.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\dbtable.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\dcbase.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\dlgcmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\dobjcmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\docmdi.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\docview.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\dosyacc.c
# ADD CPP /D "USE_DEFINE" /D "YY_USE_PROTOS" /D "IDE_INVOKED"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\common\dynarray.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\dynlib.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\encconv.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\event.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\extended.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\common\ffile.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\file.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\fileconf.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\filefn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\filesys.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\fontcmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\fontmap.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\framecmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\fs_inet.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\fs_mem.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\fs_zip.cpp
# ADD CPP /I ".\..\src\zlib"
# End Source File
# Begin Source File

SOURCE=.\..\src\common\ftp.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\gdicmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\gifdecod.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\hash.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\helpbase.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\http.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\imagall.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\imagbmp.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\image.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\imaggif.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\imagjpeg.cpp
# ADD CPP /I ".\..\src\jpeg"
# End Source File
# Begin Source File

SOURCE=.\..\src\common\imagpcx.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\imagpng.cpp
# ADD CPP /I ".\..\src\png" /I ".\..\src\zlib"
# End Source File
# Begin Source File

SOURCE=.\..\src\common\imagpnm.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\imagtiff.cpp
# ADD CPP /I ".\..\src\tiff" /I ".\..\src\zlib"
# End Source File
# Begin Source File

SOURCE=.\..\src\common\intl.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\ipcbase.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\layout.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\lboxcmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\list.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\log.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\longlong.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\matrix.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\memory.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\menucmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\mimecmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\module.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\mstream.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\object.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\objstrm.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\odbc.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\paper.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\prntbase.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\process.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\protocol.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\resourc2.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\resource.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\sckaddr.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\sckfile.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\sckipc.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\sckstrm.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\serbase.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\sizer.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\socket.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\strconv.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\stream.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\string.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\tbarbase.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\textcmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\textfile.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\timercmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\tokenzr.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\txtstrm.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\unzip.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\common\url.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\utilscmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\valgen.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\validate.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\valtext.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\variant.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\wfstream.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\wincmn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\wxchar.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\wxexpr.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\common\zipstrm.cpp
# ADD CPP /I ".\..\src\zlib"
# End Source File
# Begin Source File

SOURCE=.\..\src\common\zstream.cpp
# End Source File
# End Group
# Begin Group "Generic Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\src\generic\busyinfo.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\calctrl.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\choicdgg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\dragimgg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\grid.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\gridsel.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\helpext.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\helphtml.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\helpwxht.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\laywin.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\logg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\numdlgg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\panelg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\plot.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\progdlgg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\prop.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\propform.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\proplist.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\sashwin.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\scrolwin.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\splitter.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\statusbr.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\tabg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\tbarsmpl.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\textdlgg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\tipdlg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\treelay.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\generic\wizard.cpp
# End Source File
# End Group
# Begin Group "wxHTML Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\src\html\helpctrl.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\helpdata.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\helpfrm.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\htmlcell.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\htmlfilt.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\htmlpars.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\htmltag.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\htmlwin.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\htmprint.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\m_dflist.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\m_fonts.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\m_hline.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\m_image.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\m_layout.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\m_links.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\m_list.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\m_meta.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\m_pre.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\m_tables.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\html\winpars.cpp
# End Source File
# End Group
# Begin Group "MSW Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\src\msw\accel.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\app.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\bitmap.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\bmpbuttn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\brush.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\button.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\caret.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\checkbox.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\checklst.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\choice.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\clipbrd.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\colordlg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\colour.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\combobox.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\control.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\curico.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\cursor.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\data.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dc.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dcclient.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dcmemory.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dcprint.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dcscreen.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dde.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dialog.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dialup.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dib.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dibutils.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dir.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dirdlg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dragimag.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\dummy.cpp
# ADD CPP /Yc"wx/wxprec.h"
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\enhmeta.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\filedlg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\font.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\fontdlg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\fontenum.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\fontutil.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\frame.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\gauge95.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\gaugemsw.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\gdiimage.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\gdiobj.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\glcanvas.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\gsocket.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\gsockmsw.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\helpchm.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\helpwin.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\icon.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\imaglist.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\iniconf.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\joystick.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\listbox.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\listctrl.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\main.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\mdi.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\menu.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\menuitem.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\metafile.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\mimetype.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\minifram.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\msgdlg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\nativdlg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\notebook.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\ownerdrw.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\palette.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\pen.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\penwin.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\printdlg.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\printwin.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\radiobox.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\radiobut.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\regconf.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\region.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\registry.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\scrolbar.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\settings.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\slider95.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\slidrmsw.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\spinbutt.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\spinctrl.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\statbmp.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\statbox.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\statbr95.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\statline.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\stattext.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\tabctrl.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\taskbar.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\tbar95.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\textctrl.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\thread.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\timer.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\tooltip.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\treectrl.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\utils.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\utilsexc.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\wave.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\window.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\xpmhand.cpp
# ADD CPP /I ".\..\src\xpm"
# End Source File
# End Group
# Begin Group "OLE Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\src\msw\ole\automtn.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\ole\dataobj.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\ole\dropsrc.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\ole\droptgt.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\ole\oleutils.cpp
# End Source File
# Begin Source File

SOURCE=.\..\src\msw\ole\uuid.cpp
# End Source File
# End Group
# Begin Group "PNG Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\src\png\png.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngerror.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngget.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngmem.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngpread.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngread.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngrio.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngrtran.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngrutil.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngset.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngtest.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngtrans.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngwio.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngwrite.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngwtran.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\png\pngwutil.c
# ADD CPP /I ".\..\src\zlib"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# End Group
# Begin Group "XPM Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\src\xpm\attrib.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\crbuffri.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\crdatfri.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\create.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\crifrbuf.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\crifrdat.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\dataxpm.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\hashtab.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\imagexpm.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\info.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\misc.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\parse.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\rdftodat.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\rdftoi.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\rgb.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\scan.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\simx.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\wrffrdat.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\xpm\wrffri.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# End Group
# Begin Group "Zlib Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\src\zlib\adler32.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\compress.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\crc32.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\deflate.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\gzio.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\infblock.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\infcodes.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\inffast.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\inflate.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\inftrees.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\infutil.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\trees.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\uncompr.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\zlib\zutil.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# End Group
# Begin Group "Setup"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\include\wx\msw\setup.h
# End Source File
# End Group
# Begin Group "JPEG Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\src\jpeg\jcapimin.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jcapistd.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jccoefct.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jccolor.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jcdctmgr.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jchuff.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jcinit.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jcmainct.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jcmarker.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jcmaster.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jcomapi.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jcparam.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jcphuff.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jcprepct.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jcsample.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jctrans.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdapimin.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdapistd.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdatadst.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdatasrc.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdcoefct.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdcolor.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jddctmgr.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdhuff.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdinput.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdmainct.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdmarker.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdmaster.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdmerge.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdphuff.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdpostct.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdsample.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jdtrans.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jerror.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jfdctflt.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jfdctfst.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jfdctint.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jidctflt.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jidctfst.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jidctint.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jidctred.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jmemmgr.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jmemnobs.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jquant1.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jquant2.c

# End Source File
# Begin Source File

SOURCE=.\..\src\jpeg\jutils.c

# End Source File
# End Group
# Begin Group "TIFF files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\..\src\tiff\tif_aux.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_close.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_codec.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_compress.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_dir.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_dirinfo.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_dirread.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_dirwrite.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_dumpmode.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_error.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_fax3.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_fax3sm.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_flush.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_getimage.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_jpeg.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_luv.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_lzw.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_next.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_open.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_packbits.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_pixarlog.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_predict.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_print.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_read.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_strip.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_swab.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_thunder.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_tile.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_version.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_warning.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_win32.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_write.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\..\src\tiff\tif_zip.c
# ADD CPP /I ".\..\src\tiff"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# End Group
# End Target
# End Project
