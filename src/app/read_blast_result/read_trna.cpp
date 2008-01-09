/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author of the template:  Aaron Ucko
*
* File Description:
*   Simple program demonstrating the use of serializable objects (in this
*   case, biological sequences).  Does NOT use the object manager.
*
* Modified: Azat Badretdinov
*   reads seq-submit file, blast file and optional tagmap file to produce list of potential candidates
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include "read_blast_result.h"

int CReadBlastApp::ReadTRNA(const string& file)
{
  if(PrintDetails()) NcbiCerr << "ReadTRNA(" << file << "): start" << NcbiEndl;
  int n=0;
  ifstream is(file.c_str());
  if(!is.good()) 
    {
    NcbiCerr << "CReadBlastApp::ReadTRNA(" << file << "): ERROR: cannot open " << NcbiEndl;
    }
  while(is.good())
    {
    char line[0x100];
    is.getline(line, 0xFF); 
    if(PrintDetails()) NcbiCerr << "ReadTRNA(" << file << "): line: " << line << NcbiEndl;
    if(!is.good()) break;
// gnl|uianeuro|1003       1       19274   19345   Glu     TTC     0       0       0.00
    char *token = strtok(line, " \t");
    int icol=1;
    int from=0, to=0;
    string type3 = "";
    double score = 0.0;
    while(token != 0)
      {
      if(PrintDetails()) NcbiCerr << "ReadTRNA(" << file << "): token[" << icol << "]: " << token << NcbiEndl;
      switch(icol)
        {
        case 3: from = atoi(token); break;
        case 4: to   = atoi(token); break;
        case 5: type3= token; if(type3=="SeC") type3="Sec"; break;
        case 9: score = atof(token); break;
        default: break;
        }
      token = strtok(0, " \t");
      icol++;
      }
    if(score<m_trnascan_scoreThreshold) continue;
    ENa_strand strand = eNa_strand_plus;
    bool reverse=to<from;
    if(reverse) {int t=to; to=from;from=t; strand=eNa_strand_minus; }

    TExtRNA ext_rna;
// lot of tRNAs seems to be having left position just one bp off, assuming that it is a difference in naming
    ext_rna.from = from-1;
    ext_rna.to   = to  ;
    ext_rna.strand   = strand;
    ext_rna.type3 = type3;
    ext_rna.present = false;
    if(PrintDetails()) NcbiCerr << "ReadTRNA(" << file << "): structure: " 
       << ext_rna.from << ","
       << ext_rna.to   << ","
       << ext_rna.strand << ","
       << ext_rna.type3 << ","
       << ext_rna.present << ","
       << NcbiEndl;
    m_extRNAtable.push_back(ext_rna);
    n++;
    }

  if(PrintDetails()) NcbiCerr << "ReadTRNA(" << file << "): end" << NcbiEndl;
  return n;
}

int CReadBlastApp::ReadTRNA2(const string& file)
{
  if(PrintDetails()) NcbiCerr << "ReadTRNA2(" << file << "): start" << NcbiEndl;
  int n=0;
  ifstream is(file.c_str());
  if(!is.good()) 
    {
    NcbiCerr << "CReadBlastApp::ReadTRNA2(" << file << "): ERROR: cannot open " << NcbiEndl;
    }
  map <string, int> last_for_type;
  while(is.good())
    {
    char line[0x100];
    is.getline(line, 0xFF); 
    if(PrintDetails()) NcbiCerr << "ReadTRNA2(" << file << "): line: " << line << NcbiEndl;
    if(!is.good()) break;
// gnl|uianeuro|1003       1       19274   19345   Glu     TTC     0       0       0.00
    char *token = strtok(line, " \t");
    int icol=1;
    int from=0, to=0;
    int from_i=0, to_i=0;
    string type3 = "";
    double score = 0.0;
    int key=0;
    string codon="";
    string genome_name="";
    while(token != 0)
      {
      if(PrintDetails()) NcbiCerr << "ReadTRNA2(" << file << "): token[" << icol << "]: " << token << NcbiEndl;
      switch(icol)
        {
        case 1: genome_name = token; break;
        case 2: key = atoi(token); break;
        case 3: from = atoi(token); break;
        case 4: to   = atoi(token); break;
        case 5: type3= token; if(type3=="SeC") type3="Sec"; break;
        case 6: codon = token; 
        case 7: from_i = atoi(token); break;
        case 8: to_i   = atoi(token); break;
        case 9: score = atof(token); break;
        default: break;
        }
      token = strtok(0, " \t");
      icol++;
      }
    if(score<m_trnascan_scoreThreshold) continue;
    ENa_strand strand = eNa_strand_plus;    
    bool reverse=to<from;
    if(reverse) {int t=to; to=from;from=t; strand=eNa_strand_minus; }
    reverse=to_i<from_i;
    if(reverse) {int t=to_i; to_i=from_i;from_i=t; }
    last_for_type[type3]++;
    if(PrintDetails()) NcbiCerr << "ReadTRNA2(" << file << "): type3 = " << type3 << NcbiEndl;
    if(PrintDetails()) NcbiCerr << "ReadTRNA2(" << file << "): last_for_type[type3] = " << last_for_type[type3]  << NcbiEndl;
    int ilast_for_type = last_for_type[type3];
    strstream namestr;
    namestr << genome_name << "|calculated|" << type3 << "_" << ilast_for_type << '\0';
    strstream descstr;
    descstr << "tRNA for " << type3 << ", codon " << codon << " predicted by tRNAscan" << '\0';
    TSimpleSeq ext_rna;
// lot of tRNAs seems to be having left position just one bp off, assuming that it is a difference in naming
    ext_rna.exons.resize(1);
    ext_rna.exons[0].from = from-1;
    ext_rna.exons[0].strand   = strand;
    if(from_i)
      {
      ext_rna.exons.resize(2);
      ext_rna.exons[0].to=from_i-1;
      ext_rna.exons[1].from=to_i+1;
      ext_rna.exons[1].to  =to;
      ext_rna.exons[1].strand = strand; 
      }
    else
      {
      ext_rna.exons[0].to   = to  ;
      }
    ext_rna.type = "tRNA";
    ext_rna.key = from;
    ext_rna.name = namestr.str();
    ext_rna.description = descstr.str();
    m_extRNAtable2.push_back(ext_rna);
    n++;
    }

  if(PrintDetails()) NcbiCerr << "ReadTRNA2(" << file << "): end" << NcbiEndl;
  return n;
}


/*
* ===========================================================================
*
* $Log: read_trna.cpp,v $
* Revision 1.3  2007/10/03 16:28:52  badrazat
* update
*
* Revision 1.2  2007/09/20 17:15:42  badrazat
* more editing
*
* Revision 1.1  2007/09/20 14:40:44  badrazat
* read routines to their own files
*
* Revision 1.4  2007/07/25 12:40:41  badrazat
* read_trna.cpp: renaming some routines
* read_trna.cpp: attempt at smarting up RemoveInterim: nothing has been done
* read_blast_result.cpp: second dump of -out asn does not happen, first (debug) dump if'd out
*
* Revision 1.3  2007/07/24 16:15:55  badrazat
* added general tag str to the methods of copying location to seqannots
*
* Revision 1.2  2007/07/19 20:59:26  badrazat
* SortSeqs is done for all seqsets
*
* Revision 1.1  2007/06/21 16:21:31  badrazat
* split
*
* Revision 1.20  2007/06/20 18:28:41  badrazat
* regular checkin
*
* Revision 1.19  2007/05/16 18:57:49  badrazat
* fixed feature elimination
*
* Revision 1.18  2007/05/04 19:42:56  badrazat
* *** empty log message ***
*
* Revision 1.17  2006/11/03 15:22:29  badrazat
* flatfiles genomica location starts with 1, not 0 as in case of ASN.1 file
* changed corresponding flatfile locations in the TRNA output
*
* Revision 1.16  2006/11/03 14:47:50  badrazat
* changes
*
* Revision 1.15  2006/11/02 16:44:44  badrazat
* various changes
*
* Revision 1.14  2006/10/25 12:06:42  badrazat
* added a run over annotations for the seqset in case of submit input
*
* Revision 1.13  2006/10/17 18:14:38  badrazat
* modified output for frameshifts according to chart
*
* Revision 1.12  2006/10/17 16:47:02  badrazat
* added modifications to the output ASN file:
* addition of frameshifts
* removal of frameshifted CDSs
*
* removed product names from misc_feature record and
* added common subject info instead
*
* Revision 1.11  2006/10/02 12:50:15  badrazat
* checkin
*
* Revision 1.9  2006/09/08 19:24:23  badrazat
* made a change for Linux
*
* Revision 1.8  2006/09/07 14:21:20  badrazat
* added support of external tRNA annotation input
*
* Revision 1.7  2006/09/01 13:17:23  badrazat
* init
*
* Revision 1.6  2006/08/21 17:32:12  badrazat
* added CheckMissingRibosomalRNA
*
* Revision 1.5  2006/08/11 19:36:09  badrazat
* update
*
* Revision 1.4  2006/05/09 15:08:51  badrazat
* new file cut_blast_output_qnd and some changes to read_trna.cpp
*
* Revision 1.3  2006/03/29 19:44:21  badrazat
* same borders are included now in complete overlap calculations
*
* Revision 1.2  2006/03/29 17:17:32  badrazat
* added id extraction from whole product record in cdregion annotations
*
* Revision 1.1  2006/03/22 13:32:59  badrazat
* init
*
* Revision 1000.1  2004/06/01 18:31:56  gouriano
* PRODUCTION: UPGRADED [GCC34_MSVC7] Dev-tree R1.3
*
* Revision 1.3  2004/05/21 21:41:41  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2003/03/10 18:48:48  kuznets
* iterate->ITERATE
*
* Revision 1.1  2002/04/18 16:05:13  ucko
* Add centralized tree for sample apps.
*
*
* ===========================================================================
*/
