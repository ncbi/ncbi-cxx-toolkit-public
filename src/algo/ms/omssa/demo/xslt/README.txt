Last updated 2005-Apr-04.

Overview
--------

The XSLT scripts in this directory convert OMSSA XML output into various text
formats suitable for use in programs like excel.  These scripts are run
using a program like xsltproc, which is available via download
at http://xmlsoft.org/XSLT/ for many operating systems (xsltproc is 
contained in the download for libxslt).  For example, to create
a file readable by excel, you would run at the command line:
xsltproc peptides2csv.xsl my_omssa_output.xml
 
Comments about each XSL script are in the individual files. 


Authorship
----------

These scripts were written by Tom Laudeman twl8n@virginia.edu with
help from Lewis Geer at NCBI, and Paul Lieberman.


Notes common to all the scripts
-------------------------------

The ncbi: namespace is required on all element names, and the
xmlns:ncbi="http://www.ncbi.nlm.nih.gov attribute is required in the
xml element in the XSL scripts.

Based on some examples, I chose to use the name() function for some of
my column headers. Literal text works just as well. If the
element names change in the Omssa XML, all these scripts will break,
so I don't see using name() as a generalization feature, but it seemed
clever at the time.


Scripts require "xml-encapsulated dta format"
---------------------------------------------

For the sake of simplicity, I've made all the scripts use the
MSHitSet_ids_E element. This is only present in the output if the
input was XML dta format. This is created by dta_merge_OMSSA.pl. The
format is simple, and if you only have one dta file, you could wrap it
manually in your favorite text editor.

<dta id="1" name="your_id_or_original_dta_file_name" batchFile="./name_of_this_file">
...
</dta>

Simply wrap each dta file as a dta element, and put all the elements
into a single file. 

Attribute "id" is the index number of the dta file. Counting starts
with one (not with zero). Be sure that the id numbers are sequential
and unique within the file.

Attribute "name" is the original dta file name. This should be a
unique identifier, and will be carried through to the results.

Attribute "batchFile" is the name of this file. 

Here is an example (insert your ms/ms spectra data in place of the elipsis ...):

<dta id="1" name="./AB022105_10ug_liver_trypsin_sham2.10001.10001.1.dta" batchFile="./dtaBatch1.txt">
...
</dta>

Use the -fx switch with omssacl. For example, this is how I ran
dtaBatch1.txt against the nr database, and put the output in
dtaBatch1.xml. My databases are in a subdirectory "databases".

./omssacl -d databases/nr -fx dtaBatch1.txt -ox dtaBatch1.xml



About msmzhits2csv.xsl
-----------------------

There are notes in the script, but I felt that a little more
information might be useful to some people.

In relational (SQL) database terms, MSHitsSet has a one to many
relation ship to Peptide hits and MSMZ hits. CSV files are
de-normalized resulting in some consequences. For both peptide hits
and MSMZ hits I've repeated the MS Hit Set number on every line. This
seems to be standard practice for first normal form data. Certainly,
other data formats are possible, and some might be easier for people
to read. One option that comes to mind is an HTML table
representation.

The pephits2csv.xsl script has the same "translation to first normal
form" feature.


xsltproc on Apple OSX
---------------------

While we are largely a Fedora Linux group, we have a substantial
Xserve cluster. We are planning to run omssacl on the Xserve's. Our
sysadmin (Dawn Adelsberger) installed xsltproc via fink. 

One of the dependencies below required gcc version 3.1 (the default is
3.3). Dawn reset the gcc version with gcc_select.

to set to verson 3.1 - gcc_select 3
to set it back to gcc 3.3 gcc_select 3.3

These are the other packages that were installed along with
libxslt. Unfortunately, Dawn is not sure which one required gcc
3.1. Fink should offer to install all the dependencies.

-rw-r--r--   1 root  admin     15752  8 Mar 11:08 isoENT-tar.gz
-rw-r--r--   1 root  admin     20510  8 Mar 11:08 ISOEnts.zip
-rw-r--r--   1 root  admin   1288057  8 Mar 11:08 OpenSP-1.5.tar.gz
-rw-r--r--   1 root  admin    894834  8 Mar 11:08 openjade-1.3.2.tar.gz
-rw-r--r--   1 root  admin   1590578  8 Mar 11:08 libxslt-1.0.32.tar.bz2
-rw-r--r--   1 root  admin   2159329  8 Mar 11:08 gmp-4.1.2.tar.gz
-rw-r--r--   1 root  admin    118824  8 Mar 11:08 gtk-doc-0.9.tar.gz
-rw-r--r--   1 root  admin    134080  8 Mar 11:08 gdbm-1.8.0.tar.gz
-rw-r--r--   1 root  admin     96107  8 Mar 11:08 docbook-xml-4.4.zip
-rw-r--r--   1 root  admin     83865  8 Mar 11:08 docbook-xml-4.3.zip
-rw-r--r--   1 root  admin     78428  8 Mar 11:08 docbook-xml-4.2.zip
-rw-r--r--   1 root  admin     75683  8 Mar 11:08 docbkx412.zip
-rw-r--r--   1 root  admin     75499  8 Mar 11:08 docbkx411.zip
-rw-r--r--   1 root  admin     74627  8 Mar 11:08 docbkx41.zip
-rw-r--r--   1 root  admin     72923  8 Mar 11:08 docbkx40.zip
-rw-r--r--   1 root  admin     55742  8 Mar 11:08 docbk40.zip
-rw-r--r--   1 root  admin     56654  8 Mar 11:08 docbk41.zip
-rw-r--r--   1 root  admin     55952  8 Mar 11:08 docbk31.zip
-rw-r--r--   1 root  admin     43441  8 Mar 11:08 docbk30.zip
-rw-r--r--   1 root  admin     55653  8 Mar 11:08 docbk241.zip
-rw-r--r--   1 root  admin     53462  8 Mar 11:08 docbk23.zip
-rw-r--r--   1 root  admin     37510  8 Mar 11:08 docbk24.zip
-rw-r--r--   1 root  admin     38450  8 Mar 11:08 docbk221.zip
-rw-r--r--   1 root  admin    113063  8 Mar 11:08 docbk21.zip
-rw-r--r--   1 root  admin     23858  8 Mar 11:08 docbk121.zip
-rw-r--r--   1 root  admin     53637  8 Mar 11:07 docbk12.zip
-rw-r--r--   1 root  admin      5071  8 Mar 11:07 docbk10.zip
-rw-r--r--   1 root  admin      6856  8 Mar 11:07 docbk11.zip
-rw-r--r--   1 root  admin    237519  8 Mar 11:07 docbook-dsssl-doc-1.78.tar.gz
-rw-r--r--   1 root  admin    388310  8 Mar 11:07 docbook-dsssl-1.78.tar.gz


