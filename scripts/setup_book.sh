#!/bin/bash
#
# AUTHOR: Karanjit S. Siyan, NCBI
#
#  Run this script just once to setup the book XML structure
#
#set -x
ERR=0
echo Setting up XML toolkit book structure...
mkdir bookdoc
pushd bookdoc
[ -e dtd ] && echo "dtd directory already exists." &&  ERR=1;
[ -e data ] && echo "data directory already exists." &&  ERR=1;
[ -e www ] && echo "www directory already exists." &&  ERR=1;
[ "$ERR" = "1" ] && echo "Remove or rename existing directories." && popd && exit 1;
# Create book_html for holding transformed XML files
mkdir book_html

# Create dtd directory at top level
cvs co  -d dtd internal/books/prod/dtd
chmod -w dtd/*.dtd

# Create xml directory two levels down
mkdir -p data/toolkit
pushd data/toolkit
cvs co -d xml internal/books/prod/data/toolkit/xml
cvs co -kb -d xerces internal/books/prod/data/toolkit/xerces
popd

# Create xsl and html directories 3 levels down
mkdir -p www/data/toolkit
pushd www/data/toolkit
cvs co -d xsl internal/books/prod/www/data/toolkit/xsl
cvs co -d html internal/books/prod/www/data/toolkit/html
cd ../../
# Create pic directory for icons
cvs co -d pic internal/books/prod/www/pic
popd

cat << END > book.spp
<?xml version="1.0" encoding="UTF-8"?>
<Project ValidFileSet="Yes" ValidFile=".\dtd\ncbi-book.dtd" XSL_XMLUseSet="Yes" XSL_XMLUseFile=".\www\data\toolkit\xsl\ch.xsl" XSL_DestFolderSet="Yes" XSL_DestFolder=".\book_html" XSL_DestFileExtSet="Yes">
	<Folder FolderName="XML Files" ExtStr="xml;cml;math;mtx;rdf;smil;svg;wml">
		<File FilePath=".\data\toolkit\xml\app1.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\booktoolkit.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch-template.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch1.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch10.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch11.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch12.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch13.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch14.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch15.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch16.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch17.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch18.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch19.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch2.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch20.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch21.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch22.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch23.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch3.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch4.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch5.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch6.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch7.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch8.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\ch9.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\fm.xml" HomeFolder="Yes"/>
		<File FilePath=".\data\toolkit\xml\graphics.xml" HomeFolder="Yes"/>
	</Folder>
	<Folder FolderName="XSL Files" ExtStr="xsl;xslt"/>
	<Folder FolderName="HTML Files" ExtStr="html;htm;xhtml;asp"/>
	<Folder FolderName="DTD/Schemas" ExtStr="dtd;dcd;xdr;biz;xsd"/>
	<Folder FolderName="Entities" ExtStr="ent"/>
</Project>
END


echo Read only copy of booktoolkit.dtd in dtd/
echo Chapter XML files and scripts in data/toolkit/xml/
echo 
echo You will do most of your work by editing files in
echo the data/toolkit/xml directory.
popd
