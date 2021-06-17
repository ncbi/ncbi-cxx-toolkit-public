
This README is intended to document the process for updating the release notes
when we have a new release.




=== Adding a new Release Notes chapter ===

This section tells how to add a new Release Notes chapter.  


==== Step 1: Create a dummy page in GitHub ====

    Go to pages directory in GitHub book and create a blank chapter.
	Copy the content of the latest RN into that chapter. Do not add it into book's structure - it should be a standalone chapter only visible through direct URL. You can name it rn_new for example.
	Now, everyone can edit it via GitHub web interface.


==== Step 2: Saving content of the current RN as HTML ====

	Save current RN as HTML file. Before doing this, add HTML comments to the beginning and to the end of the content in md file, like this:
	<!-- Content start -->
	and 
	<!--- Content end --->

	This will help with the next step.


==== Step 3: Create HTML file for the existing RN to archive it ====

	From the saved in step 2 file, copy all the content between tags <!-- Content start --> and <!-- Content end -->
	Paste this into release_notes.html file between the same tags. Save the file as temp.html

==== Step 4: Archiving "old" RN ====

	In this directory (CPP_DOC/public_releases/), move release_notes.html file into a new file named release_notes_<date>.html.
	Now move temp.html into release_notes.html . 

==== Step 5: Updating RN_index ====

	Now, RN_index.html file should be updated - the previous RN (saved as release_notes_<date>.html during step #4) should be added to the table of content.
	Commit all the changes in CPP_DOC/public_releases/ directory into svn.

==== Step 6: Publish new RN ====

	When everyone is happy with the look and content of the dummy chapter, copy its content chapter into the actual release_notes.md chapter and remove dummy page.

========================================
IMPORTANT: Do not remove dummy chapter and do not override the current RN with new content until all steps in public_release directory are finished. 
