--  $Id$
-- * ===========================================================================
-- *
-- *                            PUBLIC DOMAIN NOTICE
-- *               National Center for Biotechnology Information
-- *
-- *  This software/database is a "United States Government Work" under the
-- *  terms of the United States Copyright Act.  It was written as part of
-- *  the author's official duties as a United States Government employee and
-- *  thus cannot be copyrighted.  This software/database is freely available
-- *  to the public for use. The National Library of Medicine and the U.S.
-- *  Government have not placed any restriction on its use or reproduction.
-- *
-- *  Although all reasonable efforts have been taken to ensure the accuracy
-- *  and reliability of the software and data, the NLM and the U.S.
-- *  Government do not and cannot warrant the performance or results that
-- *  may be obtained by using this software or data. The NLM and the U.S.
-- *  Government disclaim all warranties, express or implied, including
-- *  warranties of performance, merchantability or fitness for any particular
-- *  purpose.
-- *
-- *  Please cite the author in any work or product based on this material.
-- *
-- * ===========================================================================
-- *
-- * Authors:  Yury Merezhuk 
--
--  This is AppleScript file to uninstall BLAST+ package.
-- 
property svn_date : "$Revision"

on run
	set blast_folder to POSIX file "/usr/local/ncbi/blast" -- DEFAULT. Not used.
	set blast_path to POSIX file "/etc/paths.d/ncbi_blast" -- DEFAULT
	
	if not DasExist(blast_path) then
		return
	end if
	-- Discover actual BLAST+ binaries location by reading PATH settings
	set blast_bin_path to (read file blast_path)
	set blast_user_folder to POSIX file findAndReplaceInText(blast_bin_path, "/bin" & linefeed, "")
	-- log "DEBUG: READ: blast_bin_path: " & blast_bin_path
	
	-- Ask user to actually uninstall BLAST+ binaries?
	set theAlertText to "Attention"
	set theAlertMessage to "Proceed to uninstall BLAST+ binaries " & linefeed & "from " & POSIX file blast_user_folder & " ?" & linefeed & linefeed & linefeed & linefeed & linefeed & svn_date
	set user_choice to button returned of (display alert theAlertText message theAlertMessage as critical buttons {"Don't Continue", "Continue"} default button "Continue" cancel button "Don't Continue")
	if user_choice is "Don't Continue" then return
	
	-- log "DO UNINSTALL."
	-- log "blast_folder (default):     " & blast_folder
	-- log "blast_user_folder (from path): " & blast_user_folder
	
	
	
	
	-- Check if anything to delete...
	if DasExist(blast_user_folder) then
		-- Actual deletion.	
		try
			-- Actual delete operation
			tell application "Finder"
				delete (files of folder blast_user_folder) & (folders of folder blast_user_folder) & (delete blast_user_folder) & (delete blast_path)
				-- log blast_user_folder		
			end tell
		on error e number n
			-- display dialog "Uninstallation error code: " & n & " : " & e
		end try
		display dialog "BLAST+ binaries uninstalled and moved to Trash"
	end if
	
	
	
end run

-- Utility function check file/folder existance via resolving alias  
-- https://developer.apple.com/library/archive/documentation/AppleScript/Conceptual/AppleScriptLangGuide/conceptual/ASLR_fundamentals.html#//apple_ref/doc/uid/TP40000983-CH218-SW28
on DasExist(checked_file_folder)
	try
		alias checked_file_folder
		return true
	on error
		return false
	end try
end DasExist

-- Utility function: string  find/replace 
-- https://developer.apple.com/library/archive/documentation/LanguagesUtilities/Conceptual/MacAutomationScriptingGuide/ManipulateText.html
--
on findAndReplaceInText(theText, theSearchString, theReplacementString)
	set AppleScript's text item delimiters to theSearchString
	set theTextItems to every text item of theText
	set AppleScript's text item delimiters to theReplacementString
	set theText to theTextItems as string
	set AppleScript's text item delimiters to ""
	return theText
end findAndReplaceInText
