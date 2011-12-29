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

int CReadBlastApp::ReadRRNA2(const string& file)
{
  if(PrintDetails()) NcbiCerr << "ReadRRNA2(" << file << "): start" << NcbiEndl;
  int n=0;
  ifstream is(file.c_str());
  if(!is.good()) 
    {
    NcbiCerr << "CReadBlastApp::ReadRRNA2(" << file << "): ERROR: cannot open " << NcbiEndl;
    }
  map <string, TSimpleSeq> tmp_rrna; // with hash
  while(is.good())
    {
    char line[0x1000];
    is.getline(line, 0xFFF); 
    if(PrintDetails()) NcbiCerr << "ReadRRNA2(" << file << "): line: " << line << NcbiEndl;
    if(!is.good()) break;
// <TAB>223771  225324  7EB2BCB7        +       16S     ideal
    char *token = strtok(line, " \t");
    int icol=1;
    int from=0, to=0;
    string type3 = "";
    string codon="";
    string genome_name="";
    map <string, int> last_for_type;
    string hash;
    string strand1;
    string method;
    // int length=0;
    // double probability=0.0;
    while(token != 0)
      {
      if(PrintDetails()) NcbiCerr << "ReadRRNA2(" << file << "): token[" << icol << "]: " << token << NcbiEndl;
      switch(icol)
        {
        case 1: from = atoi(token); break;
        case 2: to   = atoi(token); break;
        case 3: hash = token; break;
        case 4: strand1 = token; break; 
        case 5: type3 = token; break;
        case 6: method = token; break;
        // case 7: length = atoi(token); break;
        // case 8: probability = atof(token); break;
        default: break;
        }
      token = strtok(0, " \t");
      icol++;
      }
    // if(method != "ideal" && type3 == "5S") continue;
    bool new_rrna =  tmp_rrna.find(hash) == tmp_rrna.end();
    ENa_strand strand = strand1 == "-" ? eNa_strand_minus : eNa_strand_plus;
// last for type
    // int ilast_for_type = 1;
    // if(last_for_type.find(type3) != last_for_type.end()) ilast_for_type = last_for_type[type3];
    last_for_type[type3]++;
//
    strstream descstr;
    descstr << type3 << " ribosomal RNA predicted by NCBI method with " 
       << (method=="ideal" ? "high":"low" )
       << " score";
/*
    if(probability > 0)
      descstr << " with probability " << probability;
*/
    descstr << '\0';
    TSimpleSeq new_ext_rna;
    TSimpleSeq& ext_rna = new_rrna ? new_ext_rna : tmp_rrna[hash];
// lot of tRNAs seems to be having left position just one bp off, assuming that it is a difference in naming
    TSimplePair pair;
    pair.from = from;
    pair.to   = to  ;
    pair.strand   = strand;
    ext_rna.exons.push_back(pair);
    ext_rna.type = type3;
    ext_rna.key = ext_rna.exons[0].from;
    ext_rna.name = hash;
    ext_rna.description = descstr.str();
    if(new_rrna) tmp_rrna[hash] = new_ext_rna;
    }
  for(map<string,TSimpleSeq>::const_iterator seq = tmp_rrna.begin(); seq!=tmp_rrna.end(); seq++)
    {
    m_extRNAtable2.push_back(seq->second);
    string ext_rna_range = printed_range(seq->second);
    if(PrintDetails()) NcbiCerr << "ReadRRNA2(" << file << "): adding "
       << ext_rna_range << NcbiEndl;
    n++;
    }

  if(PrintDetails()) NcbiCerr << "ReadRRNA2(" << file << "): end" << NcbiEndl;
  return n;
}


