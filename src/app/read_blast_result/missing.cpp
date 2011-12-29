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
     to = ext_rna->exons[ext_rna->exons.size()-1].to;
     ENa_strand strand = ext_rna->exons[0].strand;
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
       if(PrintDetails()) NcbiCerr << "strand = " << int(strand) << NcbiEndl;
       if(PrintDetails()) NcbiCerr << "seq2->exons[0].strand = " << int(seq2->exons[0].strand) << NcbiEndl;
       absent =  absent && (!overlap || ext_rna->type != seq2->type); // Absent
       bool bad_strand =  (overlap>0 && ext_rna->type == seq2->type &&  strand != seq2->exons[0].strand); // BadStrand
       if(!bad_strand) continue;
       string diag_name2 = seq2->name;
       int from2, to2;
       from2 = seq2->exons[0].from;  
       to2 = seq2->exons[seq2->exons.size()-1].to;
       bool undef_strand = seq2->exons[0].strand == eNa_strand_unknown;
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
       string seq_range = printed_range(seq);
       EProblem trnaStrandProblem = undef_strand ? eTRNAUndefStrand : eTRNABadStrand;
       misc_feat << "RNA does not match strand for feature located at " << seq_range << NcbiEndl;
       misc_feat << '\0';
// this goes to the misc_feat, has to be original location, and name, corrected strand
       problemStr problem = {trnaStrandProblem, bufferstr, misc_feat.str(), "", "", from2, to2, strand};
       m_diag[diag_name2].problems.push_back(problem);
       if(PrintDetails()) NcbiCerr << "simple_overlaps: adding problem:" << "\t"
               << diag_name << "\t"
               << "eTRNABadStrand" << "\t"
               << bufferstr << "\t"
               << NcbiEndl; 
// this goes to the log, has to be new
       problemStr problem2 = {trnaStrandProblem, bufferstr, "", "", "", from, to, strand};
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
       misc_feat << "no RNA in the input this type: " <<type2 << "[" << ext_rna_range << "]" << NcbiEndl;
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
     int key = seq->key;
     int from2 = seq->exons[0].from;
     int to2   = seq->exons[seq->exons.size()-1].to;
     if(to2-from2> 50000) 
       {
       NcbiCerr << "get_neighboring_sequences: WARNING: span of annotation "
                << seq->locus_tag << "" 
                << "[" << seq->name<< "]," 
                << "[" << seq->description<< "]" 
                << " is > 50000, probably a break in a circular molecule cutting across the annotation. This annotation will be ignored." << NcbiEndl;
       continue;
       }
     if(PrintDetails())
       {
       NcbiCerr << "get_neighboring_sequences: seq = " << printed_range(seq) << NcbiEndl; 
       NcbiCerr << "get_neighboring_sequences: first_in_range_set = " << first_in_range_set << NcbiEndl; 
       }
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
  // int proximity = 0;
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
 
