<?xml version="1.0"?>
<xsl:stylesheet version = "1.0"
xmlns:xsl = "http://www.w3.org/1999/XSL/Transform"
xmlns:ncbi="http://www.ncbi.nlm.nih.gov">

<!--
This script reads omssacl XML and creates a tab separated text file suitable as 
input for Bill Pearson's post-MS protein identification analysis scripts.
These scripts may not be widely available, so this file format is probably not 
something that everyone will need. 

See readme_xsl.txt for more detail.

This assumes that the XML resulted from an omssacl run using the 
xml-encapsulated dta format. Typically, this input file is created by
dta_merge_OMSSA.pl. Using the xml-encapsulated dta format is important
because it adds the MSHitSet_ids_E element to the output which is
and unambiguous data identifier. 

Usage:

xsltproc pepcsv4fasts.xsl omssacl_output.xml > experiment_name.txt

where omssacl_output.xml is your omssacl output file 
and experiment_name.txt is the resulting file. 

For the sake of source code legibility, all literal text in
this XSL script is in xsl:text elements. 
This allows the xsl:value-of elements to be on separate lines, instead of 
everything concatenated together into a single very long line.

Created by Tom Laudeman, 2005. 
-->


<xsl:output method="text"/>
<xsl:strip-space elements="*"/>

  <xsl:template match="/">
<xsl:text>Peptide sequence	MSHits_pvalue	ID	MSModHit_site	MSModHit_modtype	MSModHit_value	accession number	Description
</xsl:text>

    <xsl:for-each select="//ncbi:MSHits_pepstring">
	<xsl:value-of select="ancestor::*/ncbi:MSHits_pepstring"/>
	<xsl:text>	</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_pvalue"/>
	<xsl:text>	</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHitSet_ids/ncbi:MSHitSet_ids_E"/>
	<xsl:text>	</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_mods/ncbi:MSModHit/ncbi:MSModHit_site"/>
	<xsl:text>	</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_mods/ncbi:MSModHit/ncbi:MSModHit_modtype/ncbi:MSMod/@value"/>
	<xsl:text>	</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_mods/ncbi:MSModHit/ncbi:MSModHit_modtype/ncbi:MSMod"/>
	<xsl:text>	gi|</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_pephits/ncbi:MSPepHit/ncbi:MSPepHit_gi"/>
	<xsl:text>|ncbi|</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_pephits/ncbi:MSPepHit/ncbi:MSPepHit_accession"/>
	<xsl:text>|	</xsl:text>
	<xsl:value-of select="ancestor::*/ncbi:MSHits_pephits/ncbi:MSPepHit/ncbi:MSPepHit_defline"/>
	<xsl:text>
</xsl:text>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>

