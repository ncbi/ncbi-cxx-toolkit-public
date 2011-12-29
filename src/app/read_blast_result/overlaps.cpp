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

// all things overlaps
// that i've done

int CReadBlastApp::find_overlap(TSimpleSeqs::iterator& seq, const TSimpleSeqs::iterator& ext_rna,
   TSimpleSeqs& seqs, TSimpleSeqs& best_seq)
{
   int nseq=0;
   int from = ext_rna->exons[0].from;
   int to   = ext_rna->exons[ext_rna->exons.size()-1].to;
   string ext_rna_range = printed_range(ext_rna);
   for(;seq!=seqs.end(); seq++, nseq++)
     {
     int from2 = seq->exons[0].from;
     int to2   = seq->exons[seq->exons.size()-1].to;
     bool over_origin = seq->exons.size()>1 && to2-from2 > m_length/2;
     string ext_rna_range2 = printed_range(seq);
     if(PrintDetails()) NcbiCerr << "find_overlap" 
          << "[" << ext_rna_range << "]" 
          << "[" << ext_rna_range2 << "]" << ", trying..." << NcbiEndl;

     if(to2>=from || over_origin)
       {
       if(!over_origin) { if(PrintDetails()) NcbiCerr << "to2>=from" << NcbiEndl; }
       else             { if(PrintDetails()) NcbiCerr << "over_origin" << NcbiEndl; }
       if(from2<=to || over_origin)
         {
         if(!over_origin) { if(PrintDetails()) NcbiCerr << "from2<=to" << NcbiEndl;}
         else             { if(PrintDetails()) NcbiCerr << "over_origin 2" << NcbiEndl;}
         TSimpleSeqs::iterator seq2 = seq;
         for(;seq2!=seqs.end(); seq2++)
           {
           int from2 = seq2->exons[0].from;
           // int to2   = seq2->exons[seq->exons.size()-1].to;
           string ext_rna_range2 = printed_range(seq2);
           if(PrintDetails()) NcbiCerr << "\tfind_overlap" 
              << "[" << ext_rna_range << "]" 
              << "[" << ext_rna_range2 << "]" << ", trying 2..." << NcbiEndl;
           if(from2>to) break;// last seq
           int this_overlap;
           overlaps(ext_rna, seq2, this_overlap);
           if(PrintDetails()) NcbiCerr << "\tfind_overlap" 
              << "[" << ext_rna_range << "]" 
              << "[" << ext_rna_range2 << "]" << ", overlap = " << this_overlap << NcbiEndl;
           if(this_overlap>0) 
             {
//           best_seq.push_back(*seq);  // this is serious.
             best_seq.push_back(*seq2); 
             }
           }
         }
       break;
       }
     }
   return nseq;
}

int CReadBlastApp::find_overlap(TSimpleSeqs::iterator& seq, const TSimpleSeqs::iterator& ext_rna,
   TSimpleSeqs& seqs, int& overlap)
{
   int nseq=0;
   int from = ext_rna->exons[0].from;
   int to   = ext_rna->exons[ext_rna->exons.size()-1].to;
   strstream ext_rna_range_stream; ext_rna_range_stream << from << "..." << to << '\0';
   string ext_rna_range = ext_rna_range_stream.str();
   TSimpleSeqs::iterator& best_seq = seq;
   for(;seq!=seqs.end(); seq++, nseq++)
     {
     int from2 = seq->exons[0].from;
     int to2   = seq->exons[seq->exons.size()-1].to;
     strstream ext_rna_range_stream2; ext_rna_range_stream2 << from2 << "..." << to2 << '\0';
     string ext_rna_range2 = ext_rna_range_stream2.str();
     if(PrintDetails()) NcbiCerr << "find_overlap" 
          << "[" << ext_rna_range << "]" 
          << "[" << ext_rna_range2 << "]" << ", trying..." << NcbiEndl;

     if(to2>=from)
       {
       if(PrintDetails()) NcbiCerr << "to2>=from" << NcbiEndl;
       if(from2<=to)
         {
         if(PrintDetails()) NcbiCerr << "from2<=to" << NcbiEndl;
         TSimpleSeqs::iterator seq2 = seq;
         for(;seq2!=seqs.end(); seq2++)
           {
           int from2 = seq2->exons[0].from;
           int to2   = seq2->exons[seq->exons.size()-1].to;
           strstream ext_rna_range_stream2; ext_rna_range_stream2 << from2 << "..." << to2 << '\0';
           string ext_rna_range2 = ext_rna_range_stream2.str();
           if(PrintDetails()) NcbiCerr << "\tfind_overlap" 
              << "[" << ext_rna_range << "]" 
              << "[" << ext_rna_range2 << "]" << ", trying 2..." << NcbiEndl;
           if(from2>to) break;// last seq
           int this_overlap;
           overlaps(ext_rna, seq2, this_overlap);
           if(PrintDetails()) NcbiCerr << "\tfind_overlap" 
              << "[" << ext_rna_range << "]" 
              << "[" << ext_rna_range2 << "]" << ", overlap = " << this_overlap << NcbiEndl;
           if(this_overlap>overlap) 
             {
             overlap=this_overlap;
             best_seq = seq; // this one
             }
           }
         }
       break;
       }
     }
   return nseq;
}

int CReadBlastApp::overlaps(const TSimpleSeqs::iterator& seq1, const TSimpleSeqs::iterator& seq2, int& overlap)
{
  overlap = 0;
  for(TSimplePairs::const_iterator e1=seq1->exons.begin(); e1!=seq1->exons.end(); e1++)
    {
    for(TSimplePairs::const_iterator e2=seq2->exons.begin(); e2!=seq2->exons.end(); e2++)
      {
      int o = min(e2->to, e1->to)-max(e1->from, e2->from)+1;
      if(o>0) overlap+=o;
      }
    }
  return overlap;
}

bool CReadBlastApp::overlaps_prot_na
  (
  CBioseq& seq,
  const CBioseq::TAnnot& annots
  )
{ 
  bool result = false;
  if(PrintDetails()) NcbiCerr << "overlaps_prot_na[annots] starts" << NcbiEndl;
  IncreaseVerbosity();
  ITERATE(CBioseq::TAnnot, gen_feature, annots)
    {
    if ( !(*gen_feature)->GetData().IsFtable() ) continue;
    bool lres;

    lres = overlaps_prot_na(seq, (*gen_feature)->GetData().GetFtable());
    result = lres || result;
    }
  DecreaseVerbosity();
  if(PrintDetails()) NcbiCerr << "overlaps_prot_na[annots] ends" << NcbiEndl;
  return result;
}
bool CReadBlastApp::overlaps_na
  (
  const CBioseq::TAnnot& annots
  )
{
   bool result = false;
   if(PrintDetails()) NcbiCerr << "overlaps_na[annots] starts" << NcbiEndl;
   IncreaseVerbosity();
   ITERATE(CBioseq::TAnnot, gen_feature, annots)
     {
     if ( !(*gen_feature)->GetData().IsFtable() ) continue;
     bool lres;

     lres = overlaps_na((*gen_feature)->GetData().GetFtable());
     result = lres || result;

     }
   DecreaseVerbosity();
   if(PrintDetails()) NcbiCerr << "overlaps_na[annots] ends" << NcbiEndl;
   return result;
}

bool CReadBlastApp::overlaps_prot_na
  (
  CBioseq& seq,
  const CSeq_annot::C_Data::TFtable& feats
  )
{
   if(PrintDetails()) NcbiCerr << "overlaps_prot_na[seq,feats] starts" << NcbiEndl;
   IncreaseVerbosity();
   bool result = false;
   LocMap loc_map;
   GetLocMap(loc_map, feats); // gene features for each location
   EMyFeatureType seq_type = get_my_seq_type(seq);
   string n2 = GetStringDescr (seq);
   string name2 = GetProtName(seq);
/*
   bool hasLoc = hasGenomicInterval(seq);
   const CSeq_interval& seq_interval = getGenomicInterval(seq);
*/
   // bool hasLoc = hasGenomicLocation(seq);
   const CSeq_loc& seq_interval = getGenomicLocation(seq);
   TSeqPos from2, to2;
   ENa_strand strand2;
   getFromTo(seq_interval, from2, to2, strand2);
   
   ITERATE(CSeq_annot::C_Data::TFtable, f1, feats)
     {
     int overlap;
     if( !(*f1)->GetData().IsRna() ) continue;
//     bool lres=overlaps_prot_na(n2, seq_interval, **f1, overlap);
     bool lres=overlaps(seq_interval, (*f1)->GetLocation(), overlap);
     // CSeqFeatData::E_Choice esite = (*f1)->GetData().Which();
     if(lres && overlap >= m_rna_overlapThreshold)
       {
       string n1  =  GetLocusTag(**f1, loc_map);
       string trna_type = get_trna_string(**f1);
       string name1;
       if(trna_type.size()>0) name1 = trna_type;
       else name1 = GetRNAname(**f1);
       EMyFeatureType rna_feat_type = get_my_feat_type(**f1, loc_map);
// check overlaps with protein
       if(PrintDetails()) NcbiCerr << "overlaps_prot_na[seq,feats]: FOUND OVERLAP" << NcbiEndl;
       TSeqPos from1, to1;
       ENa_strand strand1;
       getFromTo((*f1)->GetLocation(), from1, to1, strand1);
       int min1, min2, max1, max2;
       min1 = min(from1, to1);
       min2 = min(from2, to2);
       max1 = max(from1, to1);
       max2 = max(from2, to2);
       // int mint; int maxt;
       // mint = min(min1, min2);
       // maxt = max(max1, max2);

       distanceReportStr *report = new distanceReportStr;
       int left_frame  = (from1-1)%3+1;
       int right_frame  = (from2-1)%3+1;

       report->left_strand  = strand1;
       report->right_strand = strand2;
       report->q_loc_left_from  = from1;
       report->q_loc_right_from = from2;
       report->q_loc_left_to    = to1;
       report->q_loc_right_to   = to2;
       report->q_id_left    = n1;
       report->q_id_right   = n2;
       report->q_name_left  = name1;
       report->q_name_right = name2;
       report->space          = overlap; // not used
       report->left_frame     = left_frame;
       report->right_frame    = right_frame;
       report->loc1 = &((*f1)->GetLocation());
       report->loc2 = &seq_interval;

       char bufferchar[20480];  memset(bufferchar, 0, 20480);
       strstream buffer(bufferchar, 20480, IOS_BASE::out);
       printOverlapReport(report, buffer);

       strstream buff_misc_feat_rna;
       buff_misc_feat_rna
            << "potential RNA location (" 
            << name1 << ") that overlaps protein (" << get_title(seq) << ")" << '\0';

       strstream buff_misc_feat_protein;
       buff_misc_feat_protein
            << "potential protein location ("
            << get_title(seq) << ") that overlaps RNA (" << name1 << ")" << '\0';


       strstream misc_feat_rna;
       misc_feat_rna << buff_misc_feat_rna.str() << '\0';
       strstream misc_feat_protein;
       misc_feat_protein << buff_misc_feat_protein.str() << '\0';

       if(PrintDetails()) NcbiCerr << "overlaps_prot_na[seq,feats]: created RNA buffer: "     << buff_misc_feat_rna.str()     << "\n";
       if(PrintDetails()) NcbiCerr << "overlaps_prot_na[seq,feats]: created protein buffer: " << buff_misc_feat_protein.str() << "\n";
       problemStr problem = {eRnaOverlap,  buffer.str(), "", "", "", -1, -1, eNa_strand_unknown };
       m_diag[n1].problems.push_back(problem);
       bool removeit=false;
       string removen = "";
//       problemStr problemCOH = {eRemoveOverlap, "", misc_feat.str(), "", "", mint, maxt, eNa_strand_unknown };
       if      (rna_feat_type == eMyFeatureType_pseudo_tRNA && seq_type != eMyFeatureType_hypo_CDS)
         {
         NcbiCerr << "overlaps_prot_na[seq,feats]: WARNING: RNA location "
           << n1 << " marked for deletion (pseudo)" << "\n";
         removen = n1;
         removeit=true;
         }
       else if (rna_feat_type == eMyFeatureType_atypical_tRNA && seq_type != eMyFeatureType_hypo_CDS)
         {
         NcbiCerr << "overlaps_prot_na[seq,feats]: WARNING: RNA location "
               << n1 << " marked for deletion (atypical)" << "\n";
         removen = n1;
         removeit=true;
         }
       else if 
         ( 
           (
             (rna_feat_type == eMyFeatureType_normal_tRNA || 
             rna_feat_type == eMyFeatureType_atypical_tRNA 
             )
             && seq_type == eMyFeatureType_hypo_CDS
           ) ||
           rna_feat_type == eMyFeatureType_rRNA
         )
         {
         NcbiCerr << "overlaps_prot_na[seq,feats]: WARNING: CDS and gene "
               << n2 << " marked for deletion (hypothetical)" << "\n";
         removen = n2;
         removeit=true;
         }
       else
         {
         if(PrintDetails()) NcbiCerr << "overlaps_prot_na[seq,feats]: no deletion\n";
         removeit=false;
         }
       if(removeit)
         {
         if(removen == n1) // RNA location is deleted
           {
           problemStr problemCOH = {eRemoveOverlap, "", misc_feat_rna.str(), "", "", min1, max1, strand1};
           m_diag[removen].problems.push_back(problemCOH);
           }
         else // protein location is deleted
           {
           problemStr problemCOH = {eRemoveOverlap, "", misc_feat_protein.str(), "", "", min2, max2, strand2};
           m_diag[removen].problems.push_back(problemCOH);
           }
         if(PrintDetails())  NcbiCerr << "overlaps_prot_na[seq,feats]: sequence " 
                  << "[" << removen << "]" 
                  << " is marked for removal"
                  << NcbiEndl;
         try
           {
           CBioseq_set::TSeq_set* seqs = get_parent_seqset(seq);
           if(seqs!=NULL) append_misc_feature(*seqs, removen, eRemoveOverlap);
           }
         catch(...)
           {
           NcbiCerr << "overlaps_prot_na[seq,feats]: WARNING: get_parent_seqset threw when trying to append misc_feature for " << removen << NcbiEndl;
           }
         }
       }
     result = lres || result;
     } //  if(lres && overlap >= m_rna_overlapThreshold)

   DecreaseVerbosity();
   if(PrintDetails()) NcbiCerr << "overlaps_prot_na[seq,feats] ends" << NcbiEndl;
   return result;
}

bool CReadBlastApp::overlaps_na
  (
  const CSeq_annot::C_Data::TFtable& feats
  )
{
   if(PrintDetails()) NcbiCerr << "overlaps_na[feats] starts" << NcbiEndl;
   IncreaseVerbosity();
   bool result = false;
   LocMap loc_map;
   GetLocMap(loc_map, feats);
   ITERATE(CSeq_annot::C_Data::TFtable, f1, feats)
     {
     if(PrintDetails()) NcbiCerr << "overlaps_na[feats] cycling through rna_feats" << NcbiEndl;
// check overlaps with externally defined tRNAs
     if ( !(*f1)->GetData().IsRna() ) continue;
     CRNA_ref::EType rna_type = (*f1)->GetData().GetRna().GetType();
     if(rna_type != CRNA_ref::eType_tRNA || rna_type != CRNA_ref::eType_rRNA ) continue;
     string type1;
     if ( rna_type == CRNA_ref::eType_tRNA )
       {
       if ( !(*f1)->GetData().GetRna().CanGetExt() ) continue;
       try { type1 = Get3type((*f1)->GetData().GetRna());}
       catch  (...)
         {
         NcbiCerr << "overlaps_na[feats]: FATAL: cannot get aminoacid type for one trna feats" << NcbiEndl;
         throw;
         }
       }
     else
       {
       type1 = GetRRNAtype((*f1)->GetData().GetRna());
       }
     if(type1.size()==0) continue;
     match_na(**f1, type1);
     }
   result = true;
   DecreaseVerbosity();
   if(PrintDetails()) NcbiCerr << "overlaps_na[feats] ends" << NcbiEndl;
   return result;
}

bool CReadBlastApp::overlaps_prot_na
  (
  const string& n1,
  const CSeq_interval& i1,
  const CSeq_feat& f2,
  int& overlap
  )
{
  bool result = false;
  if(PrintDetails()) NcbiCerr << "overlaps_prot_na[n1,i1,f2] starts" << NcbiEndl;
  overlap=0;
  string n2="not gene";
  if(f2.GetData().IsGene()) f2.GetData().GetGene().GetLabel(&n2);
  if(PrintDetails()) NcbiCerr << "overlaps_prot_na[n1,i1,f2]: input: "
                               << n1
                               << ","
                               << n2
                               << NcbiEndl;
  result = overlaps(i1, f2.GetLocation(), overlap);

  if(PrintDetails()) NcbiCerr << "overlaps_prot_na[n1,i1,f2] ends" << NcbiEndl;
  return result;

}


bool CReadBlastApp::overlaps_na
  (
  const CSeq_feat& f1,
  const CSeq_feat& f2,
  int& overlap
  )
{
   bool result = false;
   if(PrintDetails()) NcbiCerr << "overlaps_na[f1,f2] starts" << NcbiEndl;
   overlap=0;

   string n1; f1.GetData().GetGene().GetLabel(&n1);
   string n2; f2.GetData().GetGene().GetLabel(&n2);
   if(PrintDetails()) NcbiCerr << "overlaps_na[f1,f2]: input: "
                               << n1
                               << ","
                               << n2
                               << NcbiEndl;
   if(n1==n2) return result;

   result = overlaps(f1.GetLocation(), f2.GetLocation(), overlap);

   if(PrintDetails()) NcbiCerr << "overlaps_na[f1,f2] ends" << NcbiEndl;
   return result;

}
template <typename t1, typename t2> bool
CReadBlastApp::overlaps
  (
/*
  const CSeq_loc& l1,
  const CSeq_loc& l2,
*/
  const t1& l1,
  const t2& l2,
  int& overlap
  )
{
  bool result = false;
  overlap=0;
  for (CTypeConstIterator<CSeq_interval> i1 = ncbi::ConstBegin(l1); i1; ++i1)
    {
    TSeqPos from1, to1, from2, to2;
    ENa_strand strand1, strand2;
    int min1, min2, max1, max2;
    getFromTo( *i1, from1, to1, strand1);
    min1 = min(from1, to1);
    max1 = max(from1, to1);
    for (CTypeConstIterator<CSeq_interval> i2 = ncbi::ConstBegin(l2); i2; ++i2)
      {
      getFromTo( *i2, from2, to2, strand2);
      min2 = min(from2, to2);
      max2 = max(from2, to2);
      int overlap_start, overlap_end;
      overlap_end = min(max1, max2);
      overlap_start = max(min1, min2);

      bool result2 = overlap_end >= overlap_start;
      if(!result2) continue;
      overlap+=overlap_end - overlap_start + 1;
      result=true;
      }
    }
  return result;
}

bool CReadBlastApp::overlaps
  (
  const CSeq_loc& l1,
  int from2, int to2,
  int& overlap
  )
{
  bool result = false;
  overlap=0;
  TSeqPos from1, to1;
  int min1, min2, max1, max2;
  min2 = min(from2, to2);
  max2 = max(from2, to2);
  for (CTypeConstIterator<CSeq_interval> i1 = ::ConstBegin(l1); i1; ++i1)
    {
    ENa_strand strand1;
    getFromTo(*i1, from1, to1, strand1);
    min1 = min(from1, to1);
    max1 = max(from1, to1);
    int overlap_start, overlap_end;
    overlap_end = min(max1, max2);
    overlap_start = max(min1, min2);

    bool result2 = overlap_end >= overlap_start;
    if(result2) result=true; // found one segment that overlaps with the second input segment
    overlap+=overlap_end - overlap_start + 1;
    }
  return result;
}

bool CReadBlastApp::complete_overlap // l1 is covered by l2
  (
  const CSeq_loc& l1,
  const CSeq_loc& l2
  )
{
  bool result = false;
  for (CTypeConstIterator<CSeq_interval> i1 = ::ConstBegin(l1); i1; ++i1)
    {
    TSeqPos from1, to1, from2, to2;
    ENa_strand strand1, strand2;
    int min1, min2, max1, max2;
    getFromTo( *i1, from1, to1, strand1);
    min1 = min(from1, to1);
    max1 = max(from1, to1);
    result = false;  // assume this piece
    for (CTypeConstIterator<CSeq_interval> i2 = ::ConstBegin(l2); i2; ++i2)
      {
      getFromTo( *i2, from2, to2, strand2);
// note that this does not take into account weird cases when two pieces of l2 are stuck together without any gap, covering i1 in combination
      min2 = min(from2, to2);
      max2 = max(from2, to2);
      if(min2<=min1 && max2>=max1) // found completely covering piece
        {
        if(PrintDetails()) NcbiCerr << "complete_overlap: "
            << from1 << " ... " << to1 << " "
            << from2 << " ... " << to2 << " "
            << NcbiEndl;
        result=true;
        break;
        }
      }
    if(!result) return result;
    }
  return result;
}

bool CReadBlastApp::overlaps
// genomic interval is already stored from the NA_annotations
  (
  const CBioseq& left,
  const CBioseq& right
  )
{
  bool result=false;
  // check if prot
  // string qname = CSeq_id::GetStringDescr (left, CSeq_id::eFormat_FastA);
  string qname = GetStringDescr (left);
  string qrname = GetStringDescr (right);
  if(PrintDetails()) NcbiCerr << "overlaps, seq level " << NcbiEndl;
  if(PrintDetails()) NcbiCerr << "left  " <<  GetStringDescr (left) << NcbiEndl;
  if(PrintDetails()) NcbiCerr << "right " <<  GetStringDescr (right) << NcbiEndl;
// assuming that protein sequences come in one piece of seq-set
  if(left.GetInst().GetMol()!=CSeq_inst::eMol_aa) return result;
  if(PrintDetails()) NcbiCerr << "left is aa" << NcbiEndl;
  if(right.GetInst().GetMol()!=CSeq_inst::eMol_aa) return result;
  if(PrintDetails()) NcbiCerr << "right is aa" << NcbiEndl;
/*
  if(!hasGenomicInterval(left)) return result;
  if(!hasGenomicInterval(right)) return result;
  const CSeq_interval& left_genomic_int = getGenomicInterval(left);
  const CSeq_interval& right_genomic_int = getGenomicInterval(right);
*/
  if(!hasGenomicLocation(left)) return result;
  if(!hasGenomicLocation(right)) return result;
  const CSeq_loc& left_genomic_int = getGenomicLocation(left);
  const CSeq_loc& right_genomic_int = getGenomicLocation(right);

  if(PrintDetails()) NcbiCerr << "Got intervals" << NcbiEndl;
  TSeqPos from1, to1, from2, to2;
  ENa_strand  left_strand;
  ENa_strand right_strand;
  getFromTo(left_genomic_int, from1, to1, left_strand);
  getFromTo(right_genomic_int, from2, to2, right_strand);

  if(PrintDetails()) NcbiCerr << "Got strands" << NcbiEndl;
  int left_frame=-0xFF, right_frame=-0xFF;
  if(left_genomic_int.IsInt())
    {
    left_frame  = (from1-1)%3+1;
    }
  if(right_genomic_int.IsInt())
    {
    right_frame  = (from2-1)%3+1;
    }

  if(left_strand  != eNa_strand_plus && left_genomic_int.IsInt())  left_frame=-left_frame;
  if(right_strand != eNa_strand_plus && right_genomic_int.IsInt()) right_frame=-right_frame;
/*
  Tue 10/7/2008 9:25 AM, Bill Klimke + his consultation w/ Leigh Riley
  opposite strands overlaps should be treated exactly the same way
*/
  // if(left_strand != right_strand) return result;
  if(PrintDetails()) NcbiCerr << "Matching strands" << NcbiEndl;
  int space =
                 (min((int)to1, (int)to2)-
                  max((int)from2, (int)from1)
                  +1
                 )/3;

  bool complete_overlaps = false;
  int scratch_overlap;
  result = overlaps(left_genomic_int, right_genomic_int, scratch_overlap);
  bool  left_covered_by_right=false;
  bool  right_covered_by_left=false;
  if(result) complete_overlaps = (left_covered_by_right=complete_overlap(left_genomic_int, right_genomic_int))
                       || (right_covered_by_left=complete_overlap(right_genomic_int, left_genomic_int));
  if(PrintDetails()) NcbiCerr << "space = " << space
     << ", complete_overlap = " << complete_overlaps
     << ", result = " << result
     << NcbiEndl;
  if(result && scratch_overlap >= m_cds_overlapThreshold)  // overlap
    {
    distanceReportStr *report = new distanceReportStr;
    report->left_strand  = left_strand;
    report->right_strand = right_strand;
    report->q_loc_left_from  = from1;
    report->q_loc_right_from = from2;
    report->q_loc_left_to    = to1;
    report->q_loc_right_to   = to2;
    report->q_id_left    = GetStringDescr (left);
    report->q_id_right   = GetStringDescr (right);
    report->q_name_left  = GetProtName(left);
    report->q_name_right = GetProtName(right);
    report->space          = space;
    report->left_frame     = left_frame;
    report->right_frame    = right_frame;

    char bufferchar[20480];  memset(bufferchar, 0, 20480);
    strstream buffer(bufferchar, 20480, IOS_BASE::out);
    printOverlapReport(report, buffer);
/*
    strstream misc_feat;
    misc_feat << "potential protein locations )" << GetProtName(left)
            << ") and " << printed_range(right)
            << " overlap by " << scratch_overlap
            << "bp"
            << NcbiEndl << '\0';
*/
    strstream misc_feat_left;
    misc_feat_left 
       << "potential protein location (" << GetProtName(left) 
       << ") that overlaps protein (" << GetProtName(right) << ")" << NcbiEndl << '\0';

    strstream misc_feat_right;
    misc_feat_right
       << "potential protein location (" << GetProtName(right) 
       << ") that overlaps protein (" << GetProtName(left) << ")" << NcbiEndl << '\0';

    // problemStr problemCO = {eCompleteOverlap, buffer.str(), misc_feat.str(), "", "", -1, -1, eNa_strand_unknown };
    // problemStr problemO = {eOverlap, buffer.str(), misc_feat.str(), "", "", -1, -1, eNa_strand_unknown };
    // problemStr problem;
    // problemStr problemCOH = {eRemoveOverlap   , "", misc_feat.str(), "", "", -1, -1, eNa_strand_unknown };


    if(complete_overlaps)
      {
      // problem  = problemCO;
      if(report->q_name_left.find("hypothetical")!=string::npos && left_covered_by_right && !right_covered_by_left)
        {
        NcbiCerr << "CReadBlastApp::overlaps: WARNING: sequence of a hypothetical protein "
                 << "[" << qname << "]"
                 << " is marked for removal because of a complete overlap"
                 << NcbiEndl;
        problemStr problemCOH = {eRemoveOverlap   , "", misc_feat_left.str(), "", "", from1, to1, left_strand};
        problemStr problemCO = {eCompleteOverlap, buffer.str(), misc_feat_left.str(), "", "", -1, -1, eNa_strand_unknown };
        m_diag[qname].problems.push_back(problemCOH);
        m_diag[qname].problems.push_back(problemCO);
        try
          {
          CBioseq_set::TSeq_set* seqs = get_parent_seqset(left);
          if(seqs!=NULL) append_misc_feature(*seqs, qname, eRemoveOverlap);
          }
        catch(...)
          {
          NcbiCerr << "overlaps_prot_na[seq,feats]: WARNING: get_parent_seqset threw when trying to append misc_feature for " 
                   << qname << NcbiEndl;
          }
        }
      if(report->q_name_right.find("hypothetical")!=string::npos && right_covered_by_left)
        {
        NcbiCerr << "CReadBlastApp::overlaps: WARNING: sequence of a hypothetical protein "
                 << "[" << qrname << "]"
                 << " is marked for removal because of a complete overlap"
                 << NcbiEndl;
        problemStr problemCOH = {eRemoveOverlap   , "", misc_feat_right.str(), "", "", from2, to2, right_strand};
        problemStr problemCO = {eCompleteOverlap, buffer.str(), misc_feat_right.str(), "", "", -1, -1, eNa_strand_unknown };
        m_diag[qrname].problems.push_back(problemCOH);
        m_diag[qrname].problems.push_back(problemCO);
        try
          {
          CBioseq_set::TSeq_set* seqs = get_parent_seqset(right);
          if(seqs!=NULL) append_misc_feature(*seqs, qrname, eRemoveOverlap);
          }
        catch(...)
          {
          NcbiCerr << "overlaps_prot_na[seq,feats]: WARNING: get_parent_seqset threw when trying to append misc_feature for " 
                   << qrname << NcbiEndl;
          }
        }
      {
      problemStr problemO_l = {eCompleteOverlap, buffer.str(), misc_feat_left.str(), "", "", -1, -1, eNa_strand_unknown };
      problemStr problemO_r = {eCompleteOverlap, "", misc_feat_right.str(), "", "", -1, -1, eNa_strand_unknown };
      m_diag[qname].problems.push_back(problemO_l);
      m_diag[qrname].problems.push_back(problemO_r);
      }
      }
    else                 
      {
      problemStr problemO_l = {eOverlap, buffer.str(), misc_feat_left.str(), "", "", -1, -1, eNa_strand_unknown };
      problemStr problemO_r = {eOverlap, "", misc_feat_right.str(), "", "", -1, -1, eNa_strand_unknown };
      m_diag[qname].problems.push_back(problemO_l);
      m_diag[qrname].problems.push_back(problemO_r);
      }
    delete report;
    }
  return result;
}

