(*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Vlad Lebedev
 *
 * File Description:
 * Main Application Script
 *
 *)


(* ==== Globals ==== *)
global AllLibraries
global AllConsoleTools
global AllApplications
global ToolkitSource
global ProjBuilderLib

global TheNCBIPath, TheFLTKPath, TheBDBPath, TheSQLPath, ThePCREPath, TheOUTPath
global libTypeDLL, cpuOptimization, zeroLink, fixContinue


(* ==== 3rd party libarary properties ==== *)
global libScriptTmpDir
global libScriptPID
property libScriptWasRun : false
property libScriptRunning : false


(* ==== Properties ==== *)
property allPaths : {"pathNCBI", "pathFLTK", "pathBDB", "pathSQL", "pathPCRE", "pathOUT"}
property libDataSource : null
property toolDataSource : null
property appDataSource : null
property curDataSource : null

(* Loading of library scripts *)
on launched theObject
	my loadScripts()
	tell ToolkitSource to Initialize()
	
	-- Load User Defaults	
	repeat with p in allPaths
		try
			set tmp to contents of default entry (p as string) of user defaults
			set contents of text field (p as string) of tab view item "tab1" of tab view "theTab" of window "Main" to tmp
		on error
			set homePath to the POSIX path of (path to home folder) as string
			
			if (p as string) is equal to "pathNCBI" then set contents of text field (p as string) of tab view item "tab1" of tab view "theTab" of window "Main" to homePath & "c++"
			if (p as string) is equal to "pathOUT" then set contents of text field (p as string) of tab view item "tab1" of tab view "theTab" of window "Main" to homePath & "out"
		end try
		
	end repeat
	
	load nib "InstallLibsPanel"
	show window "Main"
end launched


on pathToScripts()
	set appPath to (path to me from user domain) as text
	return (appPath & "Contents:Resources:Scripts:") as text
end pathToScripts

on loadScript(scriptName)
	return load script file (my pathToScripts() & scriptName & ".scpt")
end loadScript

on loadScripts()
	set tootkitLib to my loadScript("Libraries")
	set projLib to my loadScript("ProjBuilder")
	
	set ToolkitSource to ToolkitSource of tootkitLib
	set ProjBuilderLib to ProjBuilder of projLib
end loadScripts

(* Additional Application Setup *)
(* Quit Application after main window is closed *)
on should quit after last window closed theObject
	return true
end should quit after last window closed



(* When the NIB (resourses are loaded *)
on awake from nib theObject
	if name of theObject is "Main" then
		tell theObject
			-- Set the drawer up with some initial values.
			set leading offset of drawer "Drawer" to 20
			set trailing offset of drawer "Drawer" to 20
		end tell
	end if
end awake from nib


(* Actual work starts here *)
on clicked theObject
	if name of theObject is "theLibsTable" then
		set curDataSource to libDataSource
		x_SaveTableData(libDataSource, AllLibraries)
	else if name of theObject is "theToolsTable" then
		set curDataSource to toolDataSource
		x_SaveTableData(toolDataSource, AllConsoleTools)
	else if name of theObject is "theAppsTable" then
		set curDataSource to appDataSource
		x_SaveTableData(appDataSource, AllApplications)
	end if
	
	if name of theObject is "selectAll" then
		x_SelectAll(true)
	else if name of theObject is "deselectAll" then
		x_SelectAll(false)
	end if
	
	if name of theObject is "otherLibs" then
		set homePath to the POSIX path of (path to home folder) as string
		set contents of text field "tmp_dir" of window "install_libs" to homePath & "tmp"
		set contents of text field "ins_dir" of window "install_libs" to homePath & "sw"
		display panel window "install_libs" attached to window "Main"
	end if
	
	if name of theObject is "close" then
		if libScriptWasRun then
			set new_insdir to contents of text field "ins_dir" of window "install_libs"
			set contents of text field "pathFLTK" of tab view item "tab1" of tab view "theTab" of window "Main" to new_insdir
			set contents of text field "pathBDB" of tab view item "tab1" of tab view "theTab" of window "Main" to new_insdir
			set contents of text field "pathSQL" of tab view item "tab1" of tab view "theTab" of window "Main" to new_insdir
			set contents of text field "pathPCRE" of tab view item "tab1" of tab view "theTab" of window "Main" to new_insdir
		end if
		close panel window "install_libs"
	end if
	
	if name of theObject is "do_it" then
		if libScriptRunning then -- cancel
			set enabled of button "do_it" of window "install_libs" to false -- prevent double kill
			do shell script "kill " & libScriptPID
		else
			x_Install3rdPartyLibs() -- launch new script in the background
		end if
	end if
	
	tell window "Main"
		(* Handle Generate button *)
		if theObject is equal to button "generate" then
			my x_AddtoLog("Log started: " & (current date) as text)
			
			set msg to my ValidatePaths() -- Validate paths and set globals
			if msg is "" then
				tell progress indicator "progBar" to start
				try
					tell ProjBuilderLib to Initialize()
					my CreateProject() -- do the job
				on error errMsg
					log errMsg
					display dialog "Generation failed with the following message: " & return & return & errMsg buttons {"OK"} default button 1
				end try
				tell progress indicator "progBar" to stop
			else
				my x_ShowAlert(msg)
			end if
		end if
		
		(* Help button pressed *)
		if theObject is equal to button "helpButton" then
			open location "http://www.ncbi.nlm.nih.gov/books/bv.fcgi?call=bv.View..ShowTOC&rid=toolkit.TOC&depth=2"
		end if
		
		(* Handle Paths *)
		tell tab view item "tab1" of tab view "theTab"
			if theObject is equal to button "ChooseNCBI" then
				my ChooseFolder("Select NCBI C++ Toolkit location", "pathNCBI")
			end if
			
			if theObject is equal to button "ChooseBDB" then
				my ChooseFolder("Select Berkley DB installation", "pathBDB")
			end if
			
			
			if theObject is equal to button "ChooseFLTK" then
				my ChooseFolder("Select FLTK installation", "pathFLTK")
			end if
			
			if theObject is equal to button "ChooseSQL" then
				my ChooseFolder("Select SQLite installation", "pathSQL")
			end if
			
			if theObject is equal to button "ChoosePCRE" then
				my ChooseFolder("Select PCRE and Image libraries (GIF, JPEG, TIFF & PNG) installation", "pathPCRE")
			end if
			
			if theObject is equal to button "ChooseOUT" then
				my ChooseFolder("Select where the project file will be created", "pathOUT")
			end if
			
		end tell
	end tell
end clicked


(* Called right before application will quit *)
on will quit theObject
	-- Save User Defaults	
	repeat with p in allPaths
		set thePath to the contents of text field (p as string) of tab view item "tab1" of tab view "theTab" of window "Main"
		make new default entry at end of default entries of user defaults with properties {name:p, contents:thePath}
		set contents of default entry (p as string) of user defaults to thePath
	end repeat
end will quit



on selected tab view item theObject tab view item tabViewItem
	if name of tabViewItem is "tab2" then
		set libTable to table view "theLibsTable" of scroll view "theLibsTable" of split view "theSplitter" of tab view item "tab2" of theObject
		set toolTable to table view "theToolsTable" of scroll view "theToolsTable" of split view "theSplitter" of tab view item "tab2" of theObject
		set appTable to table view "theAppsTable" of scroll view "theAppsTable" of split view "theSplitter" of tab view item "tab2" of theObject
		
		-- Here we will add the data columns to the data source of the contacts table view
		if libDataSource is null then
			set libDataSource to make new data source at the end of the data sources with properties {name:"libs"}
			make new data column at the end of data columns of libDataSource with properties {name:"use"}
			make new data column at the end of data columns of libDataSource with properties {name:"name"}
			set data source of libTable to libDataSource
			my x_ReloadTable(libDataSource, AllLibraries)
		end if
		
		if toolDataSource is null then
			set toolDataSource to make new data source at the end of the data sources with properties {name:"tools"}
			make new data column at the end of data columns of toolDataSource with properties {name:"use"}
			make new data column at the end of data columns of toolDataSource with properties {name:"name"}
			make new data column at the end of data columns of toolDataSource with properties {name:"path"}
			set data source of toolTable to toolDataSource
			my x_ReloadTable(toolDataSource, AllConsoleTools)
		end if
		
		if appDataSource is null then
			set appDataSource to make new data source at the end of the data sources with properties {name:"apps"}
			make new data column at the end of data columns of appDataSource with properties {name:"use"}
			make new data column at the end of data columns of appDataSource with properties {name:"name"}
			make new data column at the end of data columns of appDataSource with properties {name:"path"}
			set data source of appTable to appDataSource
			my x_ReloadTable(appDataSource, AllApplications)
		end if
	end if
end selected tab view item


on idle theObject
	if libScriptRunning then
		log "Checking status..."
		try
			set msg to do shell script "tail " & libScriptTmpDir & "/log.txt"
			set thetext to "Last few lines of " & libScriptTmpDir & "/log.txt:" & return & msg
			set contents of text view "status" of scroll view "status" of window "install_libs" to thetext
			set stat to do shell script "ps -p " & libScriptPID & " | grep " & libScriptPID
		on error
			tell progress indicator "progress" of window "install_libs" to stop
			set libScriptRunning to false
			set enabled of button "close" of window "install_libs" to true
			set title of button "do_it" of window "install_libs" to "Install third-party libraries"
			set enabled of button "do_it" of window "install_libs" to true
		end try
	end if
	return 3
end idle


(* Launch shell script to install third party libraries *)
on x_Install3rdPartyLibs()
	set libScriptRunning to true
	set libScriptWasRun to true
	tell progress indicator "progress" of window "install_libs" to start
	try
		set libScriptPID to "0"
		set toolkit_dir to contents of text field "pathNCBI" of tab view item "tab1" of tab view "theTab" of window "Main"
		set libScriptTmpDir to contents of text field "tmp_dir" of window "install_libs"
		set ins_dir to contents of text field "ins_dir" of window "install_libs"
		set theScript to toolkit_dir & "/compilers/xCode/thirdpartylibs.sh"
		
		set theScript to theScript & " " & toolkit_dir -- first argument
		set theScript to theScript & " " & libScriptTmpDir -- second argument
		set theScript to theScript & " " & ins_dir -- third argument
		if content of button "download_it" of window "install_libs" then
			set theScript to theScript & " download" -- optional download flag
		end if
		set theScript to theScript & " > " & libScriptTmpDir & "/log.txt 2>&1 & echo $!" -- to log file
		
		set title of button "do_it" of window "install_libs" to "Cancel"
		set enabled of button "close" of window "install_libs" to false
		log theScript
		set libScriptPID to do shell script theScript -- start background process
		log "Launched PID: " & libScriptPID
	end try
end x_Install3rdPartyLibs


(* Select a directory with given title *)
on ChooseFolder(theTitle, textField)
	tell open panel
		set title to theTitle
		set prompt to "Choose"
		set treat packages as directories to true
		set can choose directories to true
		set can choose files to false
		set allows multiple selection to false
	end tell
	
	set theResult to display open panel in directory "~" with file name ""
	if theResult is 1 then
		set pathNames to (path names of open panel as list)
		set thePath to the first item of pathNames
		log thePath
		set contents of text field textField of tab view item "tab1" of tab view "theTab" of window "Main" to thePath
	end if
	
end ChooseFolder


on x_ReloadTable(theDS, thePack)
	set update views of theDS to false
	delete every data row in theDS
	
	repeat with p in thePack
		set theDataRow to make new data row at the end of the data rows of theDS
		set contents of data cell "use" of theDataRow to req of p
		set contents of data cell "name" of theDataRow to name of p
		if theDS is not equal to libDataSource then set contents of data cell "path" of theDataRow to path of p
	end repeat
	
	set update views of theDS to true
end x_ReloadTable


(* Append to log entry *)
on x_AddtoLog(txt)
	tell window "Main"
		set tmp to contents of text view "logView" of scroll view "scrollView" of drawer "Drawer"
		set contents of text view "logView" of scroll view "scrollView" of drawer "Drawer" to txt & return & tmp
	end tell
	log txt
end x_AddtoLog


(* Actual work happends here *)
on CreateProject()
	repeat with library in AllLibraries -- ncbi_core : {name:"ncbi_core", libs:{xncbi, xcompress, tables, sequtil, creaders, xutil, xregexp, xconnect, xser}}
		if req of library is true then -- selected to be build
			set src_files to {}
			repeat with lib in libs of library
				set src_files to src_files & my GetSourceFiles(lib)
				x_AddtoLog("Processing: " & name of lib)
			end repeat
			--if name of library = "ncbi_core" then --then --or name of lib = "xser" or name of lib = "xutil" or name of lib = "access" then
			tell ProjBuilderLib to MakeNewLibraryTarget(library, src_files)
			--end if
		end if -- req is true
	end repeat
	
	--repeat with toolBundle in AllConsoleTools
	repeat with tool in AllConsoleTools --toolBundle
		if req of tool is true then -- selected to be build
			set src_files to my GetSourceFiles(tool)
			x_AddtoLog("Processing: " & name of tool)
			tell ProjBuilderLib to MakeNewToolTarget(tool, src_files)
		end if
	end repeat
	--end repeat
	
	--repeat with appBundle in AllApplications
	repeat with theApp in AllApplications --appBundle
		if req of theApp is true then -- selected to be build
			set src_files to my GetSourceFiles(theApp)
			x_AddtoLog("Processing: " & name of theApp)
			tell ProjBuilderLib to MakeNewAppTarget(theApp, src_files)
		end if
	end repeat
	--end repeat
	
	x_AddtoLog("Saving project file")
	tell ProjBuilderLib to SaveProjectFile()
	
	x_AddtoLog("Opening generated project: " & TheOUTPath & "/NCBI.xCode")
	do shell script "open " & TheOUTPath & "/NCBI.xCode" -- Open Project
	x_AddtoLog("Done")
	
	tell application "Xcode"
		set ver to version
	end tell
	if ver ³ "1.5" then
		if content of button "buildProj" of window "Main" is true then -- build the new project
			x_AddtoLog("Building")
			tell application "Xcode"
				activate
				tell application "System Events" -- it's a little hack, but it's work
					keystroke "b" using command down
				end tell
			end tell
			--tell application "Xcode" to build project "NCBI" -- not yet implemented in xCode 1.2
		end if
	else
		my x_ShowAlert("xCode version 1.5 or greater is required. Please visit  www.apple.com/developer  to download the latest version.")
	end if
	
end CreateProject


(* Retriece a list of source files based on library info *)
on GetSourceFiles(lib)
	set fullSourcePath to TheNCBIPath & "/src/"
	set incfileList to {}
	set excfileList to {}
	set src_files to {}
	
	try -- Try to get main path
		set fullSourcePath to fullSourcePath & x_Replace((path of lib), ":", "/") & "/"
	end try
	
	try -- Try to get the included file list
		set incfileList to inc of lib
		repeat with f in incfileList
			set src_files to src_files & (fullSourcePath & f)
		end repeat
		return src_files -- done
	end try
	
	
	try -- Try to get the excluded file list
		set excfileList to exc of lib
	end try
	
	-- Get everything in this path
	set src_files to x_GetFolderContent(fullSourcePath, excfileList)
	
	if name of lib is "xncbi" then copy TheOUTPath & "/cfg/ncbicfg.c" to the end of src_files
	return src_files
end GetSourceFiles



(* Returns a content of a foder, with *.c *.c.in and *.cpp files, excluding "excfileList"  and full path *)
on x_GetFolderContent(folderName, excfileList)
	set fileList to list folder (folderName as POSIX file) without invisibles
	set fileList to my ExcludeFiles(fileList, excfileList)
	set fileList to EndsWith(fileList, ".c") & EndsWith(fileList, ".cpp") & EndsWith(fileList, ".c.in")
	
	set filesWithPath to {}
	repeat with f in fileList
		copy folderName & f to the end of filesWithPath
	end repeat
	return filesWithPath
end x_GetFolderContent

(* Returns a new list with items "allFiles" excluding "excFiles" *)
on ExcludeFiles(allFiles, excFiles)
	set newList to {}
	repeat with f in allFiles
		if excFiles does not contain f then
			copy f to the end of newList
		end if
	end repeat
	return newList
end ExcludeFiles


(*  Replace all occurances of "old" in the aString with "new" *)
on x_Replace(aString, old, new)
	set OldDelims to AppleScript's text item delimiters
	set AppleScript's text item delimiters to old
	set newText to text items of aString
	set AppleScript's text item delimiters to new
	set finalText to newText as text
	set AppleScript's text item delimiters to OldDelims
	
	return finalText
end x_Replace


(* Return a subset of "aList" with items ending with "suffix" *)
on EndsWith(aList, suffix)
	set newList to {}
	repeat with f in aList
		if (f ends with suffix) and (f does not end with "_.cpp") then
			copy (f as string) to end of newList
		end if
	end repeat
	return newList
end EndsWith


(* Performs a validation of paths and names before generating a project *)
on ValidatePaths()
	tell tab view item "tab1" of tab view "theTab" of window "Main"
		set TheNCBIPath to contents of text field "pathNCBI"
		set TheFLTKPath to contents of text field "pathFLTK"
		set TheBDBPath to contents of text field "pathBDB"
		set TheSQLPath to contents of text field "pathSQL"
		set ThePCREPath to contents of text field "pathPCRE"
		set TheOUTPath to contents of text field "pathOUT"
	end tell
	
	set ncbiPath to x_Replace(TheNCBIPath, "/", ":")
	
	if TheNCBIPath is "" or TheFLTKPath is "" or TheBDBPath is "" or TheSQLPath is "" or ThePCREPath is "" or TheOUTPath is "" then
		return "Path(s) could not be empty"
	end if
	
	-- check paths"
	if (do shell script "file " & ThePCREPath & "/include/tiff.h") contains "No such file or directory" then
		return "Lib TIFF installation was not found at " & ThePCREPath
	end if
	
	if (do shell script "file " & ThePCREPath & "/include/jpeglib.h") contains "No such file or directory" then
		--return "Lib JPEG installation was not found at " & ThePCREPath
	end if
	
	if (do shell script "file " & ThePCREPath & "/include/png.h") contains "No such file or directory" then
		return "Lib PNG installation was not found at " & ThePCREPath
	end if
	
	if (do shell script "file " & ThePCREPath & "/include/gif_lib.h") contains "No such file or directory" then
		--return "Lib GIF installation was not found at " & ThePCREPath
	end if
	
	if (do shell script "file " & TheSQLPath & "/include/sqlite.h") contains "No such file or directory" then
		return "SQLite installation was not found at " & TheSQLPath
	end if
	
	if (do shell script "file " & TheBDBPath & "/include/db.h") contains "No such file or directory" then
		return "Berkeley DB installation was not found at " & TheBDBPath
	end if
	
	if (do shell script "file " & TheFLTKPath & "/include/FL/Fl.H") contains "No such file or directory" then
		return "FLTK installation was not found at " & TheFLTKPath
	end if
	
	if (do shell script "file " & TheNCBIPath & "/include/ncbiconf.h") contains "No such file or directory" then
		return "NCBI C++ Toolkit was not found at " & TheNCBIPath
	end if
	
	if (do shell script "file " & TheOUTPath) does not contain ": directory" then
		do shell script "mkdir " & TheOUTPath
		x_AddtoLog("The Output folder was created at: " & TheOUTPath)
		do shell script "mkdir " & TheOUTPath & "/cfg"
		set lib_dir to TheOUTPath & "/lib"
		set lib_dir to x_Replace(lib_dir, "/", "\\/")
		set ncbicfg to "sed 's/@ncbi_runpath@/" & lib_dir & "/' <" & TheNCBIPath & "/src/corelib/ncbicfg.c.in >" & TheOUTPath & "/cfg/ncbicfg.c"
		
		do shell script ncbicfg
		--return "The Output folder was not found at: " & TheOUTPath
	end if
	
	set libTypeDLL to content of button "libType" of window "Main" -- DLL or Static
	set cpuOptimization to content of button "cpuOpt" of window "Main" -- CPU specific optimization
	set zeroLink to content of button "zeroLink" of window "Main" -- Use Zero Link
	set fixContinue to content of button "fixCont" of window "Main" -- Use Fix & Continue
	
	return "" -- no errors found
end ValidatePaths


(* Display a message box *)
on x_ShowAlert(msg)
	log msg
	display dialog msg buttons {"OK"} default button 1 attached to window "Main" with icon caution
end x_ShowAlert


on x_SelectAll(theBool)
	if curDataSource is not null then
		repeat with d in data rows of curDataSource
			set contents of data cell "use" of d to theBool
		end repeat
		
		x_SaveTableData(libDataSource, AllLibraries)
		x_SaveTableData(toolDataSource, AllConsoleTools)
		x_SaveTableData(appDataSource, AllApplications)
	end if
end x_SelectAll


on x_SaveTableData(theDS, thePack)
	set c to 1
	repeat with p in thePack
		set theDataRow to item c of the data rows of theDS
		set req of p to contents of data cell "use" of theDataRow
		
		set c to c + 1
	end repeat
end x_SaveTableData


(*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2005/03/24 15:40:12  lebedev
 * Support for installing third-party libraries added
 *
 * Revision 1.14  2005/03/21 19:22:51  lebedev
 * Build phase for generating release GBench disk images added
 *
 * Revision 1.13  2005/03/21 12:27:34  lebedev
 * Fix to handle new plugins registration in the Toolkit
 *
 * Revision 1.12  2004/12/21 13:23:22  lebedev
 * Option to automatically build generated project added
 *
 * Revision 1.11  2004/10/26 17:52:57  lebedev
 * Check for external PCRE library removed
 *
 * Revision 1.10  2004/09/29 15:24:52  lebedev
 * Option to select individual package (lib, tool, application) added
 *
 * Revision 1.9  2004/09/24 17:56:52  lebedev
 * Create output directory of it's not exist
 *
 * Revision 1.8  2004/09/23 12:05:33  lebedev
 * Help button added with link to NCBI C++ Toolkit reference page
 *
 * Revision 1.7  2004/08/13 13:41:45  lebedev
 * Display warning if installed xCode version is less than 1.5
 *
 * Revision 1.6  2004/08/13 11:41:43  lebedev
 * Changes to upgrade to xCode 1.5 and support for G5 CPU specific options
 *
 * Revision 1.5  2004/08/06 15:26:59  lebedev
 * Handle ncbicfg.c.in correctly
 *
 * Revision 1.4  2004/07/06 15:30:25  lebedev
 * Use POSIX paths instead of old Mac standard
 *
 * Revision 1.3  2004/06/25 15:15:38  lebedev
 * Some unnessesary debug traces removed
 *
 * Revision 1.2  2004/06/25 15:11:52  lebedev
 * DEPLOYMENT_POSTPROCESSING set to YES in Deployment build. Missing space before -opm datatool flag added
 *
 * Revision 1.1  2004/06/23 17:09:52  lebedev
 * Initial revision
 * ===========================================================================
 *)