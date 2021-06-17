<?xml version="1.0"?>
<xsl:stylesheet version = "1.0"
xmlns:xsl = "http://www.w3.org/1999/XSL/Transform"
xmlns:ncbi="http://www.ncbi.nlm.nih.gov">

<!--
This script reads omssacl XML and creates a CSV file of MSMZ data.
The resulting CSV is suitable for reading with Excel or OpenOffice Calc.
The file contains all the MSMZ data for each MSHitSet. 

See readme_xsl.txt for more detail.

This assumes that the XML resulted from an omssacl run using the 
xml-encapsulated dta format. Typically, this input file is created by
dta_merge_OMSSA.pl. Using the xml-encapsulated dta format is important
because it adds the MSHitSet_ids_E element to the output which is
and unambiguous data identifier. 

This script takes approximately 2 minutes to run on a 2.4GHz
with a 16Mb XML input file, and an output file of approx. 2Mb

Usage:

xsltproc msmzhits2csv.xsl omssacl_output.xml > msmzhits.csv

where omssacl_output.xml is your omssacl output file 
and msmzhits.csv is the resulting CSV file.

For the sake of legibility, all literal text is in xsl:text elements. 
This allows the xsl:value-of elements to be on separate lines, instead of 
everything concatenated together into a single very long line.

Created by Tom Laudeman, 2005. 
-->

<xsl:output method="text"/>

  <!-- The column header line -->
  <xsl:template match="/">
     <xsl:value-of select="name(//ncbi:MSHitSet_number)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSHitSet_ids/ncbi:MSHitSet_ids_E)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSHits_evalue)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSHits_pepstring)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSHits_mass)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSMZHit_ion/ncbi:MSIonType)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSMZHit_ion/ncbi:MSIonType/@value)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSMZHit_charge)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSMZHit_number)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSMZHit_mz)"/>
	<xsl:text>
</xsl:text>

    <!-- The actual data. -->
    <xsl:for-each select="//ncbi:MSMZHit"><xsl:value-of select="ancestor::*/ncbi:MSHitSet_number"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHitSet_ids/ncbi:MSHitSet_ids_E"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_evalue"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_pepstring"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_mass div 1000"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ncbi:MSMZHit_ion/ncbi:MSIonType"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ncbi:MSMZHit_ion/ncbi:MSIonType/@value"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ncbi:MSMZHit_charge"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ncbi:MSMZHit_number"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ncbi:MSMZHit_mz div 1000"/>
	<xsl:text>
</xsl:text>

    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>

