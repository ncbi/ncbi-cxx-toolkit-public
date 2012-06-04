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

bool CReadBlastApp::fit_blast
  (     
  const CBioseq& left,
  const CBioseq& right,
  string& common_subject
  )     
{
  bool result=false;
  // check if prot
  if(PrintDetails()) NcbiCerr << "fit_blast, seq level " << NcbiEndl;
//  string qname = CSeq_id::GetStringDescr (left, CSeq_id::eFormat_FastA);
//  string qrname = CSeq_id::GetStringDescr (right, CSeq_id::eFormat_FastA);
  string qname = GetStringDescr (left);
  string qrname = GetStringDescr (right);
  // if(PrintDetails()) NcbiCerr << "left  " <<  CSeq_id::GetStringDescr (left, CSeq_id::eFormat_FastA) << NcbiEndl;
  // if(PrintDetails()) NcbiCerr << "right " << CSeq_id::GetStringDescr (right, CSeq_id::eFormat_FastA) << NcbiEndl;
  if(PrintDetails()) NcbiCerr << "left  " <<  GetStringDescr (left) << NcbiEndl;
  if(PrintDetails()) NcbiCerr << "right " << GetStringDescr (right) << NcbiEndl;
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
  TSeqPos from1, to1, from2, to2;

  ENa_strand  left_strand;
  ENa_strand right_strand;
  getFromTo(left_genomic_int, from1, to1, left_strand);
  getFromTo(right_genomic_int, from2, to2, right_strand);

  int left_frame  = (from1-1)%3+1; // might be not making sense of location is composite
  int right_frame = (from2-1)%3+1;
  if(left_strand  != eNa_strand_plus )  left_frame=-left_frame;
  if(right_strand != eNa_strand_plus ) right_frame=-right_frame;
  if(left_strand != right_strand)
    {
    if(PrintDetails())
       {
       NcbiCerr << "Strands mismatch for " << NcbiEndl;
       NcbiCerr << "left  " <<  GetStringDescr (left) << NcbiEndl;
       NcbiCerr << "right " << GetStringDescr (right) << NcbiEndl;
       }
    return result;
    }
  if(PrintDetails()) NcbiCerr << "Matching strands" << NcbiEndl;
  bool reverse = left_strand == eNa_strand_minus;
  int space = ((int)from2-(int)to1)/3+1; // +1 for stop codon, /3 to convert n to aa
  if(PrintDetails()) NcbiCerr << "space: " << space << NcbiEndl;
  int left_qLen  = getQueryLen(left);
  int right_qLen = getQueryLen(right);
  int num=0, numFit=0, numGI=0;
  int numLeft=0, numRight=0;
  vector<perfectHitStr> left_perfect;
  vector<perfectHitStr> right_perfect;
  collectPerfectHits(left_perfect, left);
  collectPerfectHits(right_perfect, right);

  if(PrintDetails()) NcbiCerr << "perfect hits are : " << left_perfect.size() << " " << right_perfect.size() << NcbiEndl;
  int n_frameshift_pairs=0;
  list<problemStr> problemsl;
  list<problemStr> problemsr;
  IncreaseVerbosity();
  common_subject="";
  string any_common_subject="";
  ITERATE(CBioseq::TAnnot, left_annot, left.GetAnnot())
    {
    if(PrintDetails()) NcbiCerr << "Next left annot: numLeft: " << numLeft << NcbiEndl;
    if(!(*left_annot)->GetData().IsAlign()) continue;
    numLeft++;
    if(PrintDetails()) NcbiCerr << "Left annot is alignment: numLeft: " << numLeft << NcbiEndl;
    vector<long> left_gis = getGIs(left_annot);
    if(PrintDetails()) NcbiCerr << "Left annot is alignment: left_gis: " << left_gis.size()  << NcbiEndl;

    IncreaseVerbosity();
    ITERATE(CBioseq::TAnnot, right_annot, right.GetAnnot())
      {
      if(PrintDetails()) NcbiCerr << "Next right annot: numRight: " << numRight << NcbiEndl;
      if(!(*right_annot)->GetData().IsAlign()) continue;
      numRight++;
      if(PrintDetails()) NcbiCerr << "Right annot is alignment: numRight: " << numRight << NcbiEndl;
      vector<long> right_gis = getGIs(right_annot);
      if(PrintDetails()) NcbiCerr << "Right annot is alignment: right_gis: " << right_gis.size()  << NcbiEndl;
      PushVerbosity();
      if(!giMatch(left_gis, right_gis))
         {
         PopVerbosity();
         continue;
         }
      PopVerbosity();
      if(PrintDetails()) NcbiCerr << "!!!!!!!!! GI Match !!!!!!!!!!" << NcbiEndl;
      numGI++;
      distanceReportStr *report = new distanceReportStr;
      report->left_strand  = left_strand;
      report->right_strand = right_strand;
      report->s_id = left_gis[0];
      report->q_loc_left_from  = reverse ? from2: from1;
      report->q_loc_right_from = reverse ? from1 : from2;
      report->q_loc_left_to    = reverse ? to2 : to1;
      report->q_loc_right_to   = reverse ? to1 : to2;
      int frame_from=report->q_loc_left_from;
      int frame_to  =frame_from;
      frame_from = min(frame_from, report->q_loc_left_from);
      frame_to   = max(frame_to  , report->q_loc_left_from);
      frame_from = min(frame_from, report->q_loc_right_from);
      frame_to   = max(frame_to  , report->q_loc_right_from);
      frame_from = min(frame_from, report->q_loc_left_to  );
      frame_to   = max(frame_to  , report->q_loc_left_to  );
      frame_from = min(frame_from, report->q_loc_right_to );
      frame_to   = max(frame_to  , report->q_loc_right_to );
      report->left_frame       = left_frame;
      report->right_frame      = right_frame;
      if( (!reverse && fit_blast(left, right, left_annot, right_annot, left_qLen, right_qLen, space, report)) ||
          ( reverse && fit_blast(right, left, right_annot, left_annot, right_qLen, left_qLen, space, report))
        )
        {
        numFit++;
        string this_subject = getAnnotName(left_annot);
        if(numFit==1) any_common_subject = this_subject;
        if(common_subject.size()==0 && this_subject.find("hypothetical")==string::npos)
          {
// best subject
          common_subject = this_subject;
          if(PrintDetails()) NcbiCerr << "zero common_subject changed to non-hypothetical this_subject " << this_subject
                                      << NcbiEndl;
          }
        if(PrintDetails()) NcbiCerr << "!!!! Blast bounds match !!!!! numFit: " << numFit << NcbiEndl;

        char bufferchar[20480];  memset(bufferchar, 0, 20480);
        strstream buffer(bufferchar, 20480, IOS_BASE::out);
        printReport(report, buffer);
        strstream misc_feat;
        if(common_subject.size())
          {
          misc_feat << "potential frameshift: common BLAST hit: "
                    << common_subject << '\0';
          }
        else
          {
          misc_feat << '\0';
          }
        {
        if(PrintDetails()) NcbiCerr << "added a problem: " << misc_feat.str() << NcbiEndl;
        if(PrintDetails()) NcbiCerr << "added a problem(left): " << qname << NcbiEndl;
        if(PrintDetails()) NcbiCerr << "added a problem(right): " << qrname << NcbiEndl;
        problemStr problem = { eFrameShift, buffer.str(),  misc_feat.str(),
          qname, qrname,
          frame_from,
          frame_to ,
          report->left_strand};
        problemsl.push_back(problem);
        }
        {
        problemStr problem = { eFrameShift, "", "", "", "", -1, -1, eNa_strand_unknown };
        problemsr.push_back(problem);
        }
        n_frameshift_pairs++;
        }
      else
        delete report;
      }
    DecreaseVerbosity();
    }
  DecreaseVerbosity();
  num=numLeft*numRight;
  if(common_subject.size()==0) common_subject = any_common_subject;


  if(numFit) // this block determines whether it is frameshift or not
    {
// one of the queries is hypothetical - frameshift!
    if(qname.find("hypothetical") != string::npos || qrname.find("hypothetical") != string::npos) 
      {
      if(PrintDetails()) NcbiCerr << "Frameshift or not? (" << qname << ", " << qrname << "): "
                                  << "one of those is hypo: FRAMESHIFT" << NcbiEndl;
      result = true;
      }
    else
      {
// not enough frameshift evidence - not frameshift!
      if(numFit<2) 
        {
        if(PrintDetails()) NcbiCerr << "Frameshift or not? (" << qname << ", " << qrname << "): "
                                  << "numFit < 2: NOT A FRAMESHIFT" << NcbiEndl;
        result = false;
        }
      else
        {
// there are truely exhonerating hits - not frameshift!
        if(left_perfect.size() || right_perfect.size()) 
          {
          if(PrintDetails()) NcbiCerr << "Frameshift or not? (" << qname << ", " << qrname << "): "
                                  << "there are exhonerating hits: NOT A FRAMESHIFT" << NcbiEndl;
          result = false;
          }
// the rest are frameshifts
        else 
          {
          if(PrintDetails()) NcbiCerr << "Frameshift or not? (" << qname << ", " << qrname << "): "
                                  << "there are no exhonerating hits: FRAMESHIFT" << NcbiEndl;
          result = true;
          }
        }
      }
    }
  else 
    {
    if(PrintDetails()) NcbiCerr << "Frameshift or not? (" << qname << ", " << qrname << "): "
                                  << "numFit = 0: NOT A FRAMESHIFT" << NcbiEndl;
    result = false;
    }
  if(PrintDetails()) NcbiCerr << "after, left  " <<  GetStringDescr (left)  << NcbiEndl;
  if(PrintDetails()) NcbiCerr << "after, right " <<  GetStringDescr (right) << NcbiEndl;
  if(PrintDetails()) NcbiCerr << "perfect hits are : " << left_perfect.size() << " " << right_perfect.size() << NcbiEndl;
  if(PrintDetails()) NcbiCerr << "final numGI, numFit : " << numGI << " " << numFit << NcbiEndl;
  if( !result && numFit  )
// print exhonerating hits to stderr if detailed printing
     {
     char bufferchar[2048];  memset(bufferchar, 0, 2048);
     strstream buffer(bufferchar, 2048, IOS_BASE::out);
     buffer << "Left sequence has " << left_perfect.size() << " perfect hits." << NcbiEndl;
     ITERATE(vector<perfectHitStr>, hit, left_perfect)
       {
       printPerfectHit(*hit, buffer);
       }
     buffer << "Right sequence has " << right_perfect.size() << " perfect hits." << NcbiEndl;
     ITERATE(vector<perfectHitStr>, hit, right_perfect)
       {
       printPerfectHit(*hit, buffer);
       }
     buffer << "In total, " << numFit << " pairs of hits (out of " << num << ") match for these two sequences." << NcbiEndl;
     if(PrintDetails()) NcbiCerr << buffer.str();
     if(PrintDetails()) NcbiCerr << "that is, left sequence (" << qname << ")." << NcbiEndl;
     }
  if(result)
    {
    addProblems(m_diag[qname].problems, problemsl);
    addProblems(m_diag[qrname].problems, problemsr);
    }

  return result;
}

bool CReadBlastApp::fit_blast
  (
  const CBioseq& left,
  const CBioseq& right,
  CBioseq::TAnnot::const_iterator& left_annot,
  CBioseq::TAnnot::const_iterator& right_annot,
  int left_qLen, int right_qLen,
  int space, distanceReportStr* report
  )
{
  bool result=false;
  // report->q_id_left    = CSeq_id::GetStringDescr (left, CSeq_id::eFormat_FastA);
  // report->q_id_right   = CSeq_id::GetStringDescr (right, CSeq_id::eFormat_FastA);
  report->q_id_left    = GetStringDescr (left);
  report->q_id_right   = GetStringDescr (right);
  report->q_name_left  = GetProtName(left);
  report->q_name_right = GetProtName(right);

  int left_sLen  = getLenScore(left_annot);
  int right_sLen = getLenScore(right_annot);

  int left_qFrom, left_qTo, right_qFrom, right_qTo;
  int left_sFrom, left_sTo, right_sFrom, right_sTo;
  getBounds(left_annot,  &left_qFrom,  &left_qTo,  &left_sFrom,  &left_sTo);
  getBounds(right_annot, &right_qFrom, &right_qTo, &right_sFrom, &right_sTo);

  report->q_left_left    = left_qFrom-1;
  report->q_left_middle  = left_qTo-left_qFrom+1;
  report->q_left_right   = left_qLen-left_qTo;

  report->space          = space;
  report->s_name         ="cannot get subject name";
  report->s_name = getAnnotName(left_annot);
  report->alignment_left = getAnnotComment(left_annot);
  report->alignment_right = getAnnotComment(right_annot);

  if(PrintDetails()) NcbiCerr << "fit_blast annot level: " << report->s_name.c_str() << NcbiEndl;

  report->q_right_left   = right_qFrom-1;
  report->q_right_middle = right_qTo-right_qFrom+1;
  report->q_right_right  = right_qLen-right_qTo;

  report->s_left_left    = left_sFrom-1;
  report->s_left_middle  = left_sTo-left_sFrom+1;
  report->s_left_right   = left_sLen-left_sTo;

  report->s_right_left   = right_sFrom-1;
  report->s_right_middle = right_sTo-right_sFrom+1;
  report->s_right_right  = right_sLen-right_sTo;


  int result_left = report->diff_left  =
                 (report->s_left_right - report->s_right_right) -
                 (report->q_left_right + report->space + report->q_right_left)
               ;
  int result_right = report->diff_right =
                 (report->s_right_left - report->s_left_left  ) -
                 (report->q_left_right + report->space + report->q_right_left)
               ;

  report->diff_left  -= report->q_right_middle;
  report->diff_right -= report->q_left_middle;

  report->diff_edge_left  = report->s_right_left -
                           (report->s_left_left  + report->s_left_middle  +
                            report->space + report->q_left_right + report->q_right_left );
  report->diff_edge_right = report->s_left_right -
                           (report->s_right_right+ report->s_right_middle +
                            report->space + report->q_left_right + report->q_right_left );
  if(PrintDetails())
      printReport(report, NcbiCerr);



  // result = diff<150;
// subject's hypothetical or not does not play a role
  result = result_left > 0 && result_right > 0; // && report->s_name.find("hypothetical")==string::npos;
  return result;
}
bool CReadBlastApp::has_blast_hits(const CBioseq& seq)
{
    bool result;
    if(PrintDetails()) NcbiCerr << "has_blast_hits?"  << NcbiEndl;
    result=seq.GetAnnot().size()>1;
    if(PrintDetails()) NcbiCerr << result  << NcbiEndl;
    return result;
}



