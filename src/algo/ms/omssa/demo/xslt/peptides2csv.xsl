<?xml version="1.0"?>
<xsl:stylesheet version = "1.0"
xmlns:xsl = "http://www.w3.org/1999/XSL/Transform"
xmlns:ncbi="http://www.ncbi.nlm.nih.gov">

<!--
This script reads omssacl XML and creates a CSV (comma separated values)
file of peptides. 

This file outputs one record for each MSHit that has a MSHits_pepstring. 
Some (many?) spectra do not result in a peptide, and therefore will not have
a line in the output. In other words, the output of this XSL script is
a list of the peptides that resulted from the Omssa run.

The resulting CSV is suitable for reading with Excel or OpenOffice Calc.

This script takes approximately 15 seconds to run on a 2.4GHz Xeon
with a 16Mb XML input file, and an output file of approx. 145Kb.

See readme_xsl.txt for more detail.

This assumes that the XML resulted from an omssacl run using the 
xml-encapsulated dta format. Typically, this input file is created by
dta_merge_OMSSA.pl. Using the xml-encapsulated dta format is important
because it adds the MSHitSet_ids_E element to the output which is
and unambiguous data identifier. 

Usage:

xsltproc peptides2csv.xsl omssacl_output.xml > peptides.csv

where omssacl_output.xml is your omssacl output file 
and peptides.csv is the resulting CSV file.

For the sake of legibility, all literal text is in xsl:text elements. 
This allows the xsl:value-of elements to be on separate lines, instead of 
everything concatenated together into a single very long line.

Created by Tom Laudeman, 2005. 
-->


<xsl:output method="text"/>
<xsl:strip-space elements="*"/>

  <!-- The column header line -->
  <xsl:template match="/">
     <xsl:value-of select="name(//ncbi:MSHitSet_number)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSHits_pepstring)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSHits_evalue)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSHits_pvalue)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSHits_charge)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSHits_mass)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSHitSet_ids/ncbi:MSHitSet_ids_E)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSPepHit_gi)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSPepHit_accession)"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="name(//ncbi:MSPepHit_defline)"/>
	<xsl:text>
</xsl:text>

   <!-- The actual data. Only selects records that have a MSHits_pepstring (peptide string) -->
   <xsl:for-each select="//ncbi:MSHits_pepstring">
	<xsl:value-of select="ancestor::*/ncbi:MSHitSet_number"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_pepstring"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_evalue"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_pvalue"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_charge"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_mass div 1000"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHitSet_ids/ncbi:MSHitSet_ids_E"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_pephits/ncbi:MSPepHit/ncbi:MSPepHit_gi"/>
	<xsl:text>,</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_pephits/ncbi:MSPepHit/ncbi:MSPepHit_accession"/>
	<xsl:text>,"</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_pephits/ncbi:MSPepHit/ncbi:MSPepHit_defline"/>
        <xsl:text>","</xsl:text>

        <xsl:for-each select="ancestor::*/ncbi:MSHits_mods/ncbi:MSModHit">
          <xsl:value-of select="child::ncbi:MSModHit_modtype/ncbi:MSMod/@value"/>
          <xsl:text>:</xsl:text>
          <xsl:value-of select="child::ncbi:MSModHit_site+1"/>
          <xsl:text>,</xsl:text>
        </xsl:for-each>

        <xsl:text>"
</xsl:text>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>

