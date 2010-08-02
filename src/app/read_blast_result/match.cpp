/*
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



bool CReadBlastApp::match_na
  (
  const CSeq_feat& f1,
  const string& type1
  )
{
  string diag_name = diagName("tRNA", type1);
  if(PrintDetails()) NcbiCerr << "match_na[f1," << type1 << ",] starts: " << diag_name << NcbiEndl;
  bool absent = true;
  int gleft=0, gright=0;
  int goverlap=0;
  int gabs_left=0;
  bool match_ext=false;
  int input_left, input_right;
  {
  TSeqPos from, to;
  ENa_strand strand;
  getFromTo( f1.GetLocation(), from, to, strand);
  input_left = from;
  input_right = to;
  }
  strstream input_range_stream;
  input_range_stream << (input_left + 1) << "..." << (input_right + 1) << '\0';
  string input_range = input_range_stream.str();
  NON_CONST_ITERATE(TExtRNAtable, ext_rna, m_extRNAtable)
     {
     string type2 = ext_rna->type3;
     if(PrintDetails()) NcbiCerr << "match_na[f1,"
        << type1 << ","
        << input_range
        << "] with "
        << type2 << NcbiEndl;
     if(type1 != type2) continue;
     absent = true;
     int left, right;
     bool strand_match;
     int abs_left;
     int overlap =match_na(f1, *ext_rna, left, right, strand_match, abs_left);
// 1.
     if(overlap==0) continue;
     absent = false;
// 2.
     if(!strand_match) continue;
// 3.
     if(!match_ext  || abs(left)+abs(right)<abs(gleft)+abs(gright) )
       {
       match_ext = true;
       gleft = left;
       gright = right;
       goverlap = overlap;
       gabs_left = abs_left;
       ext_rna->present=true;
       }
     } // NON_CONST_ITERATE(TExtRNAtable, ext_rna, m_extRNAtable)
  if(absent)
    {
    strstream buffer;
    buffer << "no external tRNA for this aminoacid: " <<type1 << "[" << input_range << "]" << NcbiEndl;
    buffer << "start bp: " << gabs_left << NcbiEndl;
    buffer << '\0';
    if(PrintDetails()) NcbiCerr << "match_na[f1,type1]: " << buffer.str() << NcbiEndl;
    strstream misc_feat;
    misc_feat << buffer.str() << '\0';
    problemStr problem = {eTRNAMissing, buffer.str(), misc_feat.str(), "", "", -1, -1, eNa_strand_unknown };
    m_diag[diag_name].problems.push_back(problem);
    return absent;
    }
  if(!match_ext && !absent)
    {
    strstream buffer;
    buffer << "tRNA does not match strand this aminoacid: " <<type1 << "[" << input_range << "]" << NcbiEndl;
    buffer << "start bp: " << gabs_left << NcbiEndl;
    buffer << '\0';
    if(PrintDetails()) NcbiCerr << "match_na[f1,type1]: " << buffer.str() << NcbiEndl;
    strstream misc_feat;
    misc_feat << buffer.str() << '\0';
    problemStr problem = {eTRNABadStrand, buffer.str(), misc_feat.str(), "", "", -1, -1, eNa_strand_unknown };
    m_diag[diag_name].problems.push_back(problem);
    return absent;
    }
  if(gright || gleft)
    {
    if(!goverlap)
      {
      strstream buffer;
      buffer << "closest tRNA for (" <<type1 << "[" << input_range << "]" << ") does not even overlap" << NcbiEndl;
      buffer << "start bp: " << gabs_left << NcbiEndl;
      buffer << '\0';
      strstream misc_feat;
      misc_feat << buffer.str() << '\0';
      if(PrintDetails()) NcbiCerr << "match_na[f1,type1]: " << buffer.str() << NcbiEndl;
      problemStr problem = {eTRNAComMismatch, buffer.str(), misc_feat.str(), "", "", -1, -1, eNa_strand_unknown };
      m_diag[diag_name].problems.push_back(problem);
      }
    else
      {
      strstream buffer;
      buffer << "closest tRNA for this aminoacid: " <<type1 << "[" << input_range << "]" << " have mismatched ends:" << NcbiEndl;
      buffer << "start bp: " << gabs_left << NcbiEndl;
      buffer << "left: " << gleft << ", right: " << gright << " bp shifted relative to the calculated ends" << NcbiEndl;
      buffer << "overlap: " << goverlap << NcbiEndl;
      strstream misc_feat;
      misc_feat << buffer.str() << '\0';
      if(PrintDetails()) NcbiCerr << "match_na[f1,type1]: " << buffer.str() << NcbiEndl;
      problemStr problem = {eTRNAMismatch, buffer.str(), misc_feat.str(), "", "", -1, -1, eNa_strand_unknown };
      m_diag[diag_name].problems.push_back(problem);
      }
    if(PrintDetails()) NcbiCerr << "match_na[f1," << type1 << "[" << input_range << "]" << "] ends with mismatch" << NcbiEndl;
    return absent;
    }

  if(PrintDetails()) NcbiCerr << "match_na[f1," << type1 << "[" << input_range << "]" << "] ends" << NcbiEndl;
  return absent;
}
int CReadBlastApp::match_na
  (
  const CSeq_feat& f1,
  const TExtRNA& ext_rna,
  int& left,
  int& right,
  bool& strand_match,
  int& abs_left
  )
{
   int result = 0;
   if(PrintDetails()) NcbiCerr << "match_na[f1,ext_rna,...] starts" << NcbiEndl;

   ENa_strand input_strand;
   TSeqPos from, to;
   getFromTo( f1.GetLocation(), from, to, input_strand);

   ENa_strand calc_strand  = ext_rna.strand;

   int input_left, input_right, calc_left, calc_right;
   input_left = from;
   input_right = to;
   abs_left = input_left;

   calc_left = ext_rna.from;
   calc_right= ext_rna.to;

// output
   left = input_left - calc_left;
   right = input_right - input_right;

   int max_left = max(input_left, calc_left);
   int min_right = min(input_right, calc_right);
// output
   result = min_right>max_left ? min_right-max_left : 0;
   overlaps(f1.GetLocation(), calc_left, calc_right, result);
   if(PrintDetails()) NcbiCerr << "match_na[f1,ext_rna,...] result: "
        << input_left << ","
        << input_right << ","
        << calc_left << ","
        << calc_right << ","
        << left << ","
        << right << ","
        << result << ","
        << NcbiEndl;

// output
   if(result>0)
     {
     strand_match = input_strand == calc_strand;
     if(PrintDetails()) NcbiCerr << "match_na[f1,ext_rna,...] strands: "
      << int(input_strand) << ","
      << int(calc_strand) << ","
      << strand_match << ","
      << NcbiEndl;
     }
   else
     {
     strand_match = true;
     }

   if(!strand_match)
     {
     if(PrintDetails()) NcbiCerr << "match_na[f1,ext_rna,...] no strand match" << NcbiEndl;
     }

   if(PrintDetails()) NcbiCerr << "match_na[f1,ext_rna,...] ends" << NcbiEndl;
   return result;
}

