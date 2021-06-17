#/bin/sh
#Note: Before running this script, you must have saved worksheets 1 and 2 from
#the ICLAC Excel file in tab-delimited format as cell_line1.txt and cell_line2.txt

./adjust_cell_line.pl cell_line1.txt > cell_line.txt
./adjust_cell_line.pl cell_line2.txt >> cell_line.txt
/am/ncbiapdata/scripts/misc/txt2inc.sh cell_line.txt

