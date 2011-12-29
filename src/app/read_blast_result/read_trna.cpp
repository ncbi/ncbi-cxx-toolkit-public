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
* Author: Azat Badretdin
*
* File Description:
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include "read_blast_result.hpp"

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
    char line[0x1000];
    is.getline(line, 0xFFF); 
    if(PrintDetails()) NcbiCerr << "ReadTRNA(" << file << "): line: " << line << NcbiEndl;
    if(!is.good()) break;
    if(line[0] == '#') continue;
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
       << int(ext_rna.strand) << ","
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
    char line[0x1000];
    is.getline(line, 0xFFF); 
    if(PrintDetails()) NcbiCerr << "ReadTRNA2(" << file << "): line: " << line << NcbiEndl;
    if(!is.good()) break;
// gnl|uianeuro|1003       1       19274   19345   Glu     TTC     0       0       0.00
    char *token = strtok(line, " \t");
    int icol=1;
    int from=0, to=0;
    int from_i=0, to_i=0;
    string type3 = "";
    double score = 0.0;
    // int key=0;
    string codon="";
    string genome_name="";
    while(token != 0)
      {
      if(PrintDetails()) NcbiCerr << "ReadTRNA2(" << file << "): token[" << icol << "]: " << token << NcbiEndl;
      switch(icol)
        {
        case 1: genome_name = token; break;
        // case 2: key = atoi(token); break;
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

