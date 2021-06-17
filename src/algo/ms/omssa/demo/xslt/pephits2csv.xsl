<?xml version="1.0"?>
<xsl:stylesheet version = "1.0"
xmlns:xsl = "http://www.w3.org/1999/XSL/Transform"
xmlns:ncbi="http://www.ncbi.nlm.nih.gov">

<!--
This script reads omssacl XML and creates a CSV file of peptide hit data.
The resulting CSV is suitable for reading with Excel or OpenOffice Calc.

See readme_xsl.txt for more detail.

This assumes that the XML resulted from an omssacl run using the 
xml-encapsulated dta format. Typically, this input file is created by
dta_merge_OMSSA.pl. Using the xml-encapsulated dta format is important
because it adds the MSHitSet_ids_E element to the output which is
and unambiguous data identifier. 

Usage:

xsltproc pephits2csv.xsl omssacl_output.xml > pephits.csv

where omssacl_output.xml is your omssacl output file 
and pephits.csv is the resulting CSV file.

I seem to recollect that this script can take a couple of minutes to run.

Created by Tom Laudeman, 2005. 
-->

<xsl:output method="text"/>
<xsl:strip-space elements="*"/>

<xsl:template match="/">
<xsl:text>MSHitSet_number,MSHits_evalue,MSHits_pepstring,MSHits_mass,MSPepHit_gi,MSPepHit_accession,MSPepHit_defline
</xsl:text>
<xsl:apply-templates />
</xsl:template>

<xsl:template match="ncbi:MSPepHit"><xsl:value-of select="ancestor::*/ncbi:MSHitSet_number"/>,<xsl:value-of select="ancestor::*/ncbi:MSHits_evalue"/>,<xsl:value-of select="ancestor::*/ncbi:MSHits_pepstring"/>,<xsl:value-of select="ancestor::*/ncbi:MSHits_mass div 1000"/>,<xsl:value-of select="ncbi:MSPepHit_gi"/>,<xsl:value-of 
select="ncbi:MSPepHit_accession"/>,"<xsl:value-of
select="ncbi:MSPepHit_defline"/>"
</xsl:template>

<!-- 
This is a list of text elements above MSPepHit in the tree.
Don't list anything except elements with text. If you list any 
elements that are parent nodes of MSPepHit, you won't get any output.
-->
<xsl:template match="ncbi:MSHitSet_number"/>
<xsl:template match="ncbi:MSHits_mzhits"/>
<xsl:template match="ncbi:MSHits_evalue"/>
<xsl:template match="ncbi:MSHits_pvalue"/>
<xsl:template match="ncbi:MSHits_charge"/>
<xsl:template match="ncbi:MSHits_pepstring"/>
<xsl:template match="ncbi:MSResponse_dbversion"/>
<xsl:template match="ncbi:MSHits_mass"/>

</xsl:stylesheet>

