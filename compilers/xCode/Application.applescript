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

global AllLibraries
global AllConsoleTools
global AllApplications
global ToolkitSource
global ProjBuilderLib

global TheNCBIPath, TheFLTKPath, TheBDBPath, TheSQLPath, ThePCREPath, TheOUTPath
global libTypeDLL

property allPaths : {"pathNCBI", "pathFLTK", "pathBDB", "pathSQL", "pathPCRE", "pathOUT"}

(* Loading of library scripts *)
on launched theObject
	my loadScripts()
	
	tell ToolkitSource to Initialize()
	
	-- Load User Defaults	
	repeat with p in allPaths
		try
			set tmp to contents of default entry (p as string) of user defaults
			set contents of text field (p as string) of box "pathBox" of window "Main" to tmp
		on error
			set homePath to ":" & (path to home folder as string)
			
			if (p as string) is equal to "pathNCBI" then set contents of text field (p as string) of box "pathBox" of window "Main" to x_Replace(homePath, ":", "/") & "c++"
			
			if (p as string) is equal to "pathOUT" then set contents of text field (p as string) of box "pathBox" of window "Main" to x_Replace(homePath, ":", "/") & "out"
		end try
		
	end repeat
	
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
	tell theObject
		-- Set the drawer up with some initial values.
		set leading offset of drawer "Drawer" to 20
		set trailing offset of drawer "Drawer" to 20
	end tell
end awake from nib


(* Actual work starts here *)
on clicked theObject
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
			tell application "Safari"
				activate
				--open location "http://www.ncbi.nlm.nih.gov"
				open location "http://www.ncbi.nlm.nih.gov/books/bv.fcgi?call=bv.View..ShowTOC&rid=toolkit.TOC&depth=2"
			end tell
		end if
		
		(* Handle Paths *)
		tell box "pathBox"
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
		set thePath to the contents of text field (p as string) of box "pathBox" of window "Main"
		make new default entry at end of default entries of user defaults with properties {name:p, contents:thePath}
		set contents of default entry (p as string) of user defaults to thePath
	end repeat
end will quit

on mouse entered theObject event theEvent
	(*Add your script here.*)
end mouse entered


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
		set contents of text field textField of box "pathBox" of window "Main" to thePath
	end if
	
end ChooseFolder

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
		set src_files to {}
		repeat with lib in libs of library
			set src_files to src_files & my GetSourceFiles(lib)
			x_AddtoLog("Processing: " & name of lib)
		end repeat
		--if name of library = "ncbi_core" then --then --or name of lib = "xser" or name of lib = "xutil" or name of lib = "access" then
		tell ProjBuilderLib to MakeNewLibraryTarget(library, src_files)
		--end if
	end repeat
	
	repeat with toolBundle in AllConsoleTools
		repeat with tool in toolBundle
			set src_files to my GetSourceFiles(tool)
			x_AddtoLog("Processing: " & name of tool)
			tell ProjBuilderLib to MakeNewToolTarget(tool, src_files)
		end repeat
	end repeat
	
	repeat with appBundle in AllApplications
		repeat with theApp in appBundle
			set src_files to my GetSourceFiles(theApp)
			x_AddtoLog("Processing: " & name of theApp)
			tell ProjBuilderLib to MakeNewAppTarget(theApp, src_files)
		end repeat
	end repeat
	
	x_AddtoLog("Saving project file")
	tell ProjBuilderLib to SaveProjectFile()
	
	x_AddtoLog("Opening generated project: " & TheOUTPath & "/NCBI.xCode")
	do shell script "open " & TheOUTPath & "/NCBI.xCode" -- Open Project
	
	tell application "Xcode"
		set p to the a reference to project "NCBI"
		set intermediates path of p to TheOUTPath
		set product path of p to (TheOUTPath & "/bin")
	end tell
	x_AddtoLog("Done")
	
	if content of button "buildProj" of window "Main" is true then -- build the new project
		x_AddtoLog("Building")
		tell application "Xcode" to build project "NCBI" -- not yet implemented in xCode 1.2
	end if
	
	
end CreateProject


(* Retriece a list of source files based on library info *)
on GetSourceFiles(lib)
	set fullSourcePath to x_Replace(TheNCBIPath, "/", ":") & ":src:"
	set incfileList to {}
	set excfileList to {}
	set src_files to {}
	
	try -- Try to get main path
		set fullSourcePath to fullSourcePath & path of lib & ":"
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
	
	return src_files
end GetSourceFiles



(* Returns a content of a foder, with *.c and *.cpp files, excluding "excfileList"  and full path *)
on x_GetFolderContent(folderName, excfileList)
	set folderName to the characters 2 thru (length of folderName) of folderName as string -- remove the leading ":" from the folder path
	
	set fileList to list folder folderName without invisibles
	set fileList to my ExcludeFiles(fileList, excfileList)
	set fileList to EndsWith(fileList, ".c") & EndsWith(fileList, ".cpp")
	
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
	tell box "pathBox" of window "Main"
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
	if (do shell script "file " & ThePCREPath & "/include/pcre.h") contains "No such file or directory" then
		return "PCRE installation was not found at " & ThePCREPath
	end if
	
	if (do shell script "file " & ThePCREPath & "/include/pcre.h") contains "No such file or directory" then
		return "PCRE installation was not found at " & ThePCREPath
	end if
	
	if (do shell script "file " & ThePCREPath & "/include/tiff.h") contains "No such file or directory" then
		return "Lib TIFF installation was not found at " & ThePCREPath
	end if
	
	if (do shell script "file " & ThePCREPath & "/include/jpeglib.h") contains "No such file or directory" then
		return "Lib JPEG installation was not found at " & ThePCREPath
	end if
	
	if (do shell script "file " & ThePCREPath & "/include/png.h") contains "No such file or directory" then
		return "Lib PNG installation was not found at " & ThePCREPath
	end if
	
	if (do shell script "file " & ThePCREPath & "/include/gif_lib.h") contains "No such file or directory" then
		return "Lib GIF installation was not found at " & ThePCREPath
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
		return "The Output folder was not found at: " & TheOUTPath
	end if
	
	set libTypeDLL to content of button "libType" of window "Main" -- DLL or Static
	
	return "" -- no errors found
end ValidatePaths


(* Display a message box *)
on x_ShowAlert(msg)
	log msg
	display dialog msg buttons {"OK"} default button 1 attached to window "Main" with icon caution
end x_ShowAlert



(*
 * ===========================================================================
 * $Log$
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