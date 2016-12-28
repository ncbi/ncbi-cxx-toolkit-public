#! /bin/sh
cut -f 3 top900_orgn_nucl.txt > taxids.txt
cat taxids.txt |
epost -db taxonomy |
efetch -format xml |
xtract -format |
sed \
    -e 's/<Division>Bacteria<\/Division>/<Division>BCT<\/Division>/g' \
    -e 's/<Division>Environmental samples<\/Division>/<Division>ENV<\/Division>/g' \
    -e 's/<Division>Invertebrates<\/Division>/<Division>INV<\/Division>/g' \
    -e 's/<Division>Mammals<\/Division>/<Division>MAM<\/Division>/g' \
    -e 's/<Division>Phages<\/Division>/<Division>PHG<\/Division>/g' \
    -e 's/<Division>Plants and Fungi<\/Division>/<Division>PLN<\/Division>/g' \
    -e 's/<Division>Primates<\/Division>/<Division>PRI<\/Division>/g' \
    -e 's/<Division>Rodents<\/Division>/<Division>ROD<\/Division>/g' \
    -e 's/<Division>Synthetic and Chimeric<\/Division>/<Division>SYN<\/Division>/g' \
    -e 's/<Division>Unassigned<\/Division>/<Division>UNA<\/Division>/g' \
    -e 's/<Division>Vertebrates<\/Division>/<Division>VRT<\/Division>/g' \
    -e 's/<Division>Viruses<\/Division>/<Division>VRL<\/Division>/g' \
> taxa.xml
cat taxa.xml |
xtract -pattern Taxon -COMM "(-)" -COMM GenbankCommonName -PGCODE "(-)" \
  -block Property -if PropName -equals pgcode -PGCODE PropValueInt \
  -block Taxon -element ScientificName "&COMM" GCId MGCId "&PGCODE" TaxId Division Lineage > tax_table.txt
cat tax_table.txt | sort -f > sorted_taxlist.txt
