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

// internal functions

static int get_max_distance(const int range_scale);

bool CReadBlastApp::CheckMissingRibosomalRNA
  (
  const CBioseq::TAnnot& annots
  )
{
   bool result = false;
   if(PrintDetails()) NcbiCerr << "CheckMissingRibosomalRNA[annots] starts" << NcbiEndl;
   IncreaseVerbosity();
   ITERATE(CBioseq::TAnnot, gen_feature, annots)
     {
     if ( !(*gen_feature)->GetData().IsFtable() ) continue;
     bool lres = CheckMissingRibosomalRNA((*gen_feature)->GetData().GetFtable());
     result = lres || result;
     }
   DecreaseVerbosity();
   if(PrintDetails()) NcbiCerr << "CheckMissingRibosomalRNA[annots] ends" << NcbiEndl;
   return result;
}

bool CReadBlastApp::CheckMissingRibosomalRNA
  (
  const CSeq_annot::C_Data::TFtable& feats
  )
{
// all rRNAs must be in one table!

   if(PrintDetails()) NcbiCerr << "CheckMissingRibosomalRNA[feats] starts" << NcbiEndl;
   IncreaseVerbosity();
   bool result = false;
   bool has5S=false;
   bool has16S=false;
   bool has23S=false;
   ITERATE(CSeq_annot::C_Data::TFtable, f1, feats)
     {
     if( !(*f1)->GetData().IsRna() ) continue;
     CRNA_ref::EType rna_type = (*f1)->GetData().GetRna().GetType(); 
     if( rna_type == CRNA_ref::eType_rRNA )
       {
       if( !(*f1)->GetData().GetRna().CanGetExt() ) 
         {
         NcbiCerr << "CReadBlastApp::CheckMissingRibosomalRNA[feats]: FATAL: no ext feature in rRNA" << NcbiEndl;
         throw;
         }
       string type = GetRRNAtype((*f1)->GetData().GetRna());
       if(type == "5S") {has5S = true; }
       if(type == "16S") {has16S = true; }
       if(type == "23S") {has23S = true; }
       }
     }
   if(!has5S)
     NcbiCerr << "CReadBlastApp::CheckMissingRibosomalRNA[feats]: ERROR: 5S ribosomal RNA is missing" << NcbiEndl;
   if(!has16S)
     NcbiCerr << "CReadBlastApp::CheckMissingRibosomalRNA[feats]: ERROR: 16S ribosomal RNA is missing" << NcbiEndl;
   if(!has23S)
     NcbiCerr << "CReadBlastApp::CheckMissingRibosomalRNA[feats]: ERROR: 23S ribosomal RNA is missing" << NcbiEndl;
   DecreaseVerbosity();
   if(PrintDetails()) NcbiCerr << "CheckMissingRibosomalRNA[feats] ends" << NcbiEndl;
   return result;
}

void CReadBlastApp::ugly_simple_overlaps_call(int& n_user_neighbors, int& n_ext_neighbors,
  TSimpleSeqs::iterator& ext_rna, 
  TSimpleSeqs::iterator& first_user_in_range, TSimpleSeqs::iterator& first_user_non_in_range, 
  TSimpleSeqs& seqs, int max_distance, 
  TSimpleSeqs::iterator& first_ext_in_range, TSimpleSeqs::iterator& first_ext_non_in_range, 
  string& bufferstr)
{
         if(PrintDetails())
           {
           if(first_user_in_range==seqs.end())
              {
              NcbiCerr << "ugly_simple_overlaps_call: first_user_in_range is already at the end" << NcbiEndl;
              }
           else
              {
              NcbiCerr << "ugly_simple_overlaps_call: first_user_in_range = " << printed_range(first_user_in_range) << NcbiEndl;
              }
           }

         n_user_neighbors = get_neighboring_sequences(ext_rna, first_user_in_range, first_user_non_in_range, 
                                                       seqs, max_distance);
         n_ext_neighbors = get_neighboring_sequences(ext_rna, first_ext_in_range, first_ext_non_in_range, 
                                                       m_extRNAtable2, max_distance);
         if(PrintDetails())
           {
           if(first_user_in_range==seqs.end())
              {
              NcbiCerr << "ugly_simple_overlaps_call: after call: first_user_in_range is already at the end" << NcbiEndl;
              }
           else
              {
              NcbiCerr << "ugly_simple_overlaps_call: after call: first_user_in_range = " << printed_range(first_user_in_range) << NcbiEndl;
              }
           }

         strstream buffer;
         addSimpleTab(buffer, "CENTER_REFERENCE", ext_rna, max_distance);
         for(TSimpleSeqs::iterator entry = first_ext_in_range; entry!= first_ext_non_in_range; entry++)
           {
           if(entry==ext_rna) continue; // addSimpleTab(buffer, "CENTER_REFERENCE", entry);
           else addSimpleTab(buffer, "REFERENCE", entry, max_distance);
           }
         for(TSimpleSeqs::iterator entry = first_user_in_range; entry!=first_user_non_in_range; entry++)
           {
           addSimpleTab(buffer, "VICINITY", entry, max_distance);
           }
         buffer << '\0';
         bufferstr=buffer.str();
}

int CReadBlastApp::simple_overlaps()
{
  int nabsent=0;
  int saved_m_verbosity_threshold = m_verbosity_threshold;
  // m_verbosity_threshold = 300;
  if(PrintDetails()) NcbiCerr << "simple_overlaps starts: " << NcbiEndl;
  TSimpleSeqs& seqs=m_simple_seqs;  // now calculated in CopyGenestoforgotthename 

  TSimpleSeqs::iterator first_user_in_range = seqs.begin();
  TSimpleSeqs::iterator first_user_non_in_range = seqs.begin();
  TSimpleSeqs::iterator first_ext_in_range = m_extRNAtable2.begin();
  TSimpleSeqs::iterator first_ext_non_in_range = m_extRNAtable2.begin();
  TSimpleSeqs::iterator seq = seqs.begin();
  NON_CONST_ITERATE(TSimpleSeqs, ext_rna, m_extRNAtable2)
     {
     int from, to;
     from = ext_rna->exons[0].from;  
     ENa_strand strand = ext_rna->exons[0].strand;
     to = ext_rna->exons[ext_rna->exons.size()-1].to;
     int range_scale = to - from;
     int max_distance = get_max_distance(range_scale);
     string type2 = ext_rna->name;
     string ext_rna_range = printed_range(ext_rna);
     if(PrintDetails()) NcbiCerr << "simple_overlaps[" << type2 << "[" << ext_rna_range << "]" << "]" << NcbiEndl;
// find BEST overlap, not good enough here
     TSimpleSeqs best_seq;
     find_overlap(seq, ext_rna, seqs, best_seq); // this will slide seq along seqs
     bool absent = true;
     string diag_name = ext_rna->name;
// for buffer
     int n_user_neighbors=0; int n_ext_neighbors = 0; string bufferstr="";
     NON_CONST_ITERATE(TSimpleSeqs, seq2, best_seq)
       {
       int overlap=0;
       overlaps(ext_rna, seq2, overlap);
       strstream seq2_range_stream; 
       string seq2_range = printed_range(seq2);
       if(PrintDetails()) NcbiCerr << "simple_overlaps"
                                   << "[" << type2 
                                      << "[" << ext_rna_range << "]" 
                                      << "[" << seq2_range << "]" 
                                   << "]" 
                                   << ". "
                                   << "Overlap = " << overlap
                                   << NcbiEndl;
       if(PrintDetails()) NcbiCerr << "ext_rna->type = " << ext_rna->type << NcbiEndl;
       if(PrintDetails()) NcbiCerr << "seq2->type = " << seq2->type  << NcbiEndl;
       if(PrintDetails()) NcbiCerr << "strand = " << strand << NcbiEndl;
       if(PrintDetails()) NcbiCerr << "seq2->exons[0].strand = " << seq2->exons[0].strand << NcbiEndl;
       absent =  absent && !overlap; // Absent
       bool bad_strand =  (overlap>0 && ext_rna->type == seq2->type &&  strand != seq2->exons[0].strand); // BadStrand
       if(!bad_strand) continue;
       if(!bufferstr.size())
         {
         if(PrintDetails())
           {
           if(first_user_in_range==seqs.end())
              {
              NcbiCerr << "simple_overlaps: first_user_in_range is already at the end" << NcbiEndl;
              }
           else
              {
              NcbiCerr << "simple_overlaps: first_user_in_range = " << printed_range(first_user_in_range) << NcbiEndl;
              }
           }
         ugly_simple_overlaps_call(n_user_neighbors, n_ext_neighbors,
            ext_rna, first_user_in_range, first_user_non_in_range, seqs, max_distance,
            first_ext_in_range, first_ext_non_in_range, bufferstr);
         }
       strstream misc_feat;
       misc_feat << "RNA does not match strand for " << seq->locus_tag << NcbiEndl;
       misc_feat << '\0';
       problemStr problem = {eTRNABadStrand, "", misc_feat.str(), "", "", from, to, strand};
       m_diag[diag_name].problems.push_back(problem);
       if(PrintDetails()) NcbiCerr << "simple_overlaps: adding problem:" << "\t"
               << diag_name << "\t"
               << "eTRNABadStrand" << "\t"
               << bufferstr << "\t"
               << NcbiEndl; 
       problemStr problem2 = {eTRNABadStrand , bufferstr, "", "", "", -1, -1, strand};
       m_diag[diag_name].problems.push_back(problem2);

       } // best_Seq iteration NON_CONST_ITERATE(TSimpleSeqs, seq2, best_seq)

     if(absent)
       {
       if(!bufferstr.size())
         {
         ugly_simple_overlaps_call(n_user_neighbors, n_ext_neighbors,
             ext_rna, first_user_in_range, first_user_non_in_range, seqs, max_distance,
             first_ext_in_range, first_ext_non_in_range, bufferstr);
         }
       strstream misc_feat;
       misc_feat << "NO RNA in the input this type: " <<type2 << "[" << ext_rna_range << "]" << NcbiEndl;
       misc_feat << '\0';
       problemStr problem = {eTRNAAbsent , "", misc_feat.str(), "", "", from, to, strand};
       m_diag[diag_name].problems.push_back(problem);
       if(PrintDetails()) NcbiCerr << "simple_overlaps: adding problem:" << "\t"
               << diag_name << "\t"
               << "eTRNAAbsent" << "\t"
               << bufferstr << "\t"
               << NcbiEndl; 
       problemStr problem2 = {eTRNAAbsent , bufferstr, "", "", "", -1, -1, strand};
       m_diag[diag_name].problems.push_back(problem2);
       nabsent++;
       }
// if no neighbors were in sight we need to settle for the current seq
     if(first_user_in_range==seqs.end()) first_user_in_range=seq;

     } // NON_CONST_ITERATE(TSimpleSeqs, ext_rna, m_extRNAtable2)
  if(PrintDetails()) NcbiCerr << "simple_overlaps ends: "  << NcbiEndl;
  m_verbosity_threshold = saved_m_verbosity_threshold;

  return nabsent;
}


int CReadBlastApp::get_neighboring_sequences(
      const TSimpleSeqs::iterator& ext_rna, 
      TSimpleSeqs::iterator& first_user_in_range, TSimpleSeqs::iterator& first_user_non_in_range,
      TSimpleSeqs& seqs, const int max_distance)
// given ext_rna, shifts the window first_user_in_range:first_user_non_in_range in the list of
// previously sorted seqs
{
   int  from = ext_rna->exons[0].from;  
   int  to = ext_rna->exons[ext_rna->exons.size()-1].to;

   bool first_in_range_set = false;
   int n=0;
   if(PrintDetails())
     {
     if(first_user_in_range==seqs.end())
       {
       NcbiCerr << "get_neighboring_sequences: first_user_in_range is already at the end" << NcbiEndl;
       }
     else
       {
       NcbiCerr << "get_neighboring_sequences: first_user_in_range = " 
                 << printed_range(first_user_in_range) << NcbiEndl; 
       } 
     }

   TSimpleSeqs::iterator seq = first_user_in_range;
   for(; seq !=seqs.end(); seq++)
     {
     if(PrintDetails())
       {
       NcbiCerr << "get_neighboring_sequences: seq = " << printed_range(seq) << NcbiEndl; 
       NcbiCerr << "get_neighboring_sequences: first_in_range_set = " << first_in_range_set << NcbiEndl; 
       }
     int key = seq->key;
     int from2 = seq->exons[0].from;
     int to2   = seq->exons[seq->exons.size()-1].to;
     int proximity = sequence_proximity(from, to, from2, to2, key, max_distance);
     if(proximity<0) continue;
     if (proximity==0) // in the range
       {
       if(!first_in_range_set) { first_user_in_range = seq; first_in_range_set=true; }
       n++;
       }
     else // already after the range
       {
       break;
       }
     }
   if(PrintDetails()) NcbiCerr << "get_neighboring_sequences: after cycle first_in_range_set = " 
                               << first_in_range_set << NcbiEndl; 
   first_user_non_in_range = seq; 
   if(!first_in_range_set) {first_user_in_range = first_user_non_in_range = seqs.end();}
   if(first_user_non_in_range==seqs.end())
     {
     n++; n--;
     }
   if(first_user_in_range==seqs.end())
     {
     n++; n--;
     }
   if(PrintDetails()) NcbiCerr << "get_neighboring_sequences: returning:  " << n << NcbiEndl; 
   return n;
}

int CReadBlastApp::sequence_proximity(const int target_from, const int target_to, 
    const int from, const int to, const int key)
{
  int proximity = 0;
  int range_scale = target_to - target_from;
  int max_distance = get_max_distance(range_scale);
  return sequence_proximity(target_from, target_to, from, to, key, max_distance);
}

int get_max_distance(const int range_scale)
{
  int neighbor_factor = 10;
  int min_range = 0; // 3000;
  int max_range = 5000; // 3000;
  int max_distance = range_scale * neighbor_factor;
  if(max_distance < min_range) max_distance = min_range;
  if(max_distance > max_range) max_distance = max_range;
  return max_distance;
}

int CReadBlastApp::sequence_proximity(const int target_from, const int target_to,
    const int from, const int to, const int key, const int max_distance)
{
  if(to < target_from - max_distance ) return -1;
  if(from > target_to + max_distance ) return +1;
  return 0;
}

void CReadBlastApp::addSimpleTab(strstream& buffer, const string tag, const TSimpleSeqs::iterator& ext_rna,
   const int max_distance)
{
   ITERATE(TSimplePairs, e, ext_rna->exons)
     {
     string strandt = e->strand == eNa_strand_minus ? "-" : "+";
     buffer   << tag << "\t"
              << max_distance << "\t"
              << ext_rna->type << "\t"
              << ext_rna->name << "(" << ext_rna->locus_tag << ")" << "\t"
              << ext_rna->exons[0].from << "\t"
              << ext_rna->description << "\t"
              << e->from << "\t"
              << e->to << "\t"
              << strandt; 
     buffer   << NcbiEndl;
     }
}
 

/*
* ===========================================================================
*
* $Log: missing.cpp,v $
* Revision 1.5  2007/11/13 20:21:04  badrazat
* fixing broken individiual alignment links for CDD html output
*
* Revision 1.4  2007/11/08 15:49:04  badrazat
* 1. added locus tags
* 2. fixed bugs in detecting RNA problems (simple seq related)
*
* Revision 1.3  2007/10/03 16:28:52  badrazat
* update
*
* Revision 1.2  2007/09/20 17:15:42  badrazat
* more editing
*
* Revision 1.1  2007/09/20 16:09:16  badrazat
* init
*
* Revision 1.5  2007/09/20 14:40:44  badrazat
* read routines to their own files
*
* Revision 1.4  2007/07/25 12:40:41  badrazat
* missing.cpp: renaming some routines
* missing.cpp: attempt at smarting up RemoveInterim: nothing has been done
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
* new file cut_blast_output_qnd and some changes to missing.cpp
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
