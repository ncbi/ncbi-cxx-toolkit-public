<?xml version="1.0"?>
<xsl:stylesheet version = "1.0" xmlns:xsl = 
"http://www.w3.org/1999/XSL/Transform" xmlns:ncbi="http://www.ncbi.nlm.nih.gov">

<!-- example xslt transform to change OMSSA xml output into csv -->
<!-- original from Tom Laudeman, UVa -->

<xsl:output method="text"/>
<xsl:template match="/">
<xsl:for-each select="//ncbi:MSPepHit">

<xsl:value-of select="ncbi:MSPepHit_gi"/>,<xsl:value-of 
select="ncbi:MSPepHit_accession"/>,<xsl:value-of select="ncbi:MSPepHit_defline"/>
<xsl:text>
</xsl:text>

</xsl:for-each>
</xsl:template>
</xsl:stylesheet>
