$Id$

This README is intended to document the process for updating the release notes
when we have a new release.

Currently, it just contains the steps that were applicable when the release
notes were part of the Toolkit book.  It needs to be updated to reflect the
new process, now that the release notes are on independent web pages.

See CXX-5290.




=== Adding a new Release Notes chapter ===

This section tells how to add a new Release Notes chapter.  For the purpose of illustration, the examples are based on the addition of new release notes for version 12 in May 2013, when the previous release notes were for version 9 in May 2012.


==== Step 1: Change directory to your working copy ====

    cd $CPP_BOOK


==== Step 2: Establish files for the old and new release notes ====

The current release notes will become the old release notes, so their file names will need to be changed to include their version/date info.  However, the new release notes will need the same file names as the current release notes (i.e. without a version or date).  So just copy the current release notes files to new files for the old release notes, and use the current files as placeholders for the new release notes.  For example:

    cp oltpl/release_notes.html.oltpl oltpl/release_notes_v9_2012.html.oltpl
    cp server/release_notes.html server/release_notes_v9_2012.html
    cp sanity/release_notes-gold.txt sanity/release_notes_v9_2012-gold.txt
    cp word/release_notes.doc word/release_notes_v9_2012.doc
    cp word/wordml/release_notes.doc.xml word/wordml/release_notes_v9_2012.doc.xml
    cp word/wordml/oltpl/release_notes.doc.xml.oltpl word/wordml/oltpl/release_notes_v9_2012.doc.xml.oltpl
    cp xml/release_notes.xml xml/release_notes_v9_2012.xml
    cp xml/oltpl/release_notes.xml.oltpl xml/oltpl/release_notes_v9_2012.xml.oltpl

Establishing all the files is necessary because the bkedit script assumes the files it works with are already in SVN.


==== Step 3: Add the new files to SVN ====

<b>N.B.</b> Some SVN properties will need to be changed, so make sure all the properties of the new files are identical to the properties for their corresponding old files.

Specifically:
* All files ending in <tt>.xml</tt> or <tt>.oltpl</tt> will need to have the <tt>svn:keywords</tt> property deleted and have the <tt>ncbi:disabled-keywords</tt> property set to <tt><nowiki>"Id Revision Author Date HeadURL"</nowiki></tt>.
* The <tt>*.html</tt> files will need to have the <tt>svn:keywords</tt> property deleted.
* The <tt>*.doc.xml</tt> and <tt>*.doc.xml.oltpl</tt> files will need to have the <tt>svn:eol-style</tt> property set to <tt>CRLF</tt>.


==== Step 4: Begin the edit-publish process ====

Create a new bkedit 'edit'.  For example, the following command creates an edit with the alias '<tt>rn</tt>':

    bkedit begin -j 4035 -m "Release notes for May 2013." rn toc,part8,release_notes,release_notes_v9_2012


==== Step 5: Update the book structure and edit the source files ====

a. Edit <tt>booktoolkit.xml</tt> to add the new chapter and manually upload it to CMS.  Specifically:

* Add an ENTITY line for the <b>previous</b> release notes immediately after the ENTITY line for "release_notes".  For example:

|| <b>Old</b> || <b>New</b> ||
||<tt>...</tt><br /><tt><!ENTITY release_notes SYSTEM "release_notes.xml"></tt><br />&nbsp;<br /><tt><!ENTITY release_notes_7-05_2011 SYSTEM "release_notes_7-05_2011.xml"></tt><br /><tt>...</tt> ||<tt>...</tt><br /><tt><!ENTITY release_notes SYSTEM "release_notes.xml"></tt><br /><tt><!ENTITY release_notes_v9_2012 SYSTEM "release_notes_v9_2012.xml"></tt><br /><tt><!ENTITY release_notes_7-05_2011 SYSTEM "release_notes_7-05_2011.xml"></tt><br /><tt>...</tt> ||

* Add a comment line for the <b>previous</b> release notes immediately after the comment line for "release_notes".  For example:

|| <b>Old</b> || <b>New</b> ||
||<tt>...</tt><br /><tt><!--NCBI C++ Toolkit Release (May, 2013)-->&release_notes;</tt><br />&nbsp;<br /><tt><!--NCBI C++ Toolkit Release (May, 2011)-->&release_notes_7-05_2011;</tt><br /><tt>...</tt> ||<tt>...</tt><br /><tt><!--NCBI C++ Toolkit Release (May, 2013)-->&release_notes;</tt><br /><tt><!--NCBI C++ Toolkit Release (May, 2012)-->&release_notes_v9_2012;</tt><br /><tt><!--NCBI C++ Toolkit Release (May, 2011)-->&release_notes_7-05_2011;</tt><br /><tt>...</tt> ||

For example: http://mini.ncbi.nlm.nih.gov/1k51a

b. Edit the old release notes XML file (just changing the ID).  For example: http://mini.ncbi.nlm.nih.gov/1k51b

c. Edit the new release notes Word file (with the up-to-date content).


==== Step 6: Perform Word Conversion and generate derived files ====

The new release notes will be edited in Word (because it would be ridiculously hard to edit all the new content in XML), but the old release notes should be edited in XML (because that's the best way, if not the only way, to reliably change all the IDs).  Therefore, Word Conversion will occur in both directions for this edit.  Currently, bkedit doesn't support simultaneous Word Conversion in both directions for different pages within the same edit.  Therefore, you will need to generate derived files for each page separately.  For example:

    bkedit gen -w doc2xml -e rn -p release_notes -R
    bkedit gen -w xml2doc -e rn -p release_notes_v9_2012
    bkedit gen -w xml2doc -e rn -p toc
    bkedit gen -w xml2doc -e rn -p part8


==== Step 7: Review, commit, and update the servers ====

Review, check in, copy to servers, and run sanity checks as usual, for example:

    # review files in working copy; if ok:
    bkedit cp_dev rn
    # review in dev; if ok:
    bkedit up_dev -C -s rn
    bkedit up_test -s rn
    # review in test; if ok:
    bkedit up_prod rn


==== Step 8: End the edit-publish process ====

If everything looks good on [http://mini.ncbi.nlm.nih.gov/1k51d www] then you're done - except for cleaning up and ending the process (which, by default, will re-index):

    bkedit end rn

==== Step 9: Release Notes for FTP site ====

To generate HTML version of the new Release Notes for FTP site, simply run:

    xsltproc -stringparam page-request release_notes xslt/main_RN_html.xsl xml/booktoolkit.xml

The output file will be in FTP_HTML folder. This file should be uploaded to the current release directory at [ftp://ftp.ncbi.nih.gov/toolbox/ncbi_tools++/ARCHIVE/ ftp://ftp.ncbi.nih.gov/toolbox/ncbi_tools++/ARCHIVE/]

A new PDF version of the book should be generated and uploaded to FTP as well (at this time, we ask books group to make the whole book PDF for us).


