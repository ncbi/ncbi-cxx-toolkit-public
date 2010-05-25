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

int CReadBlastApp::AnalyzeSeqsViaBioseqs(bool in_pool_prot, bool against_prot)
{
  if(PrintDetails()) NcbiCerr << "AnalyzeSeqsViaBioseqs(in_pool_prot, against_prot) " << "\n";
   if(IsSubmit())
     {
     if (
           m_Submit.GetData().IsEntrys()
            &&
           (*m_Submit.GetData().GetEntrys().begin())->IsSet()
        )
       {
       CBioseq_set::TSeq_set& seqs =  (*m_Submit.SetData().SetEntrys().begin())->SetSet().SetSeq_set();
       AnalyzeSeqsViaBioseqs(seqs, seqs, in_pool_prot, against_prot);
       }
     else
       {
       NcbiCerr << "ERROR: submit file does not have proper seqset"<< NcbiCerr;
       }
     }
   else
     {
     CBioseq_set::TSeq_set& seqs =  m_Entry.SetSet().SetSeq_set();
     AnalyzeSeqsViaBioseqs(seqs, seqs, in_pool_prot, against_prot);
     }

   return -1;

}

int CReadBlastApp::AnalyzeSeqsViaBioseqs(
    CBioseq_set::TSeq_set& in_pool_seqs,
    CBioseq_set::TSeq_set& against_seqs,
    bool in_pool_prot, bool against_prot)
{
  if(PrintDetails()) NcbiCerr << "AnalyzeSeqsViaBioseqs(in_pool_seqs, against_seqs, in_pool_prot, against_prot): "
    << ", in_pool_prot= " << in_pool_prot
    << ", against_prot= " << against_prot << "\n";

    NON_CONST_ITERATE( CBioseq_set::TSeq_set, left, in_pool_seqs)
      {
      if((*left)->IsSet())
        {
        CBioseq_set::TSeq_set& seqs_down = (*left)->SetSet().SetSeq_set();
        AnalyzeSeqsViaBioseqs(seqs_down, against_seqs, in_pool_prot, against_prot);
        }
      else
        {
        string name = GetStringDescr((*left)->GetSeq());
        if(PrintDetails()) NcbiCerr << "AnalyzeSeqsViaBioseqs(in_pool_seqs, against_seqs, in_pool_prot, against_prot): "
           << "left seq entry "
           << name << "\n";
        if(   ( in_pool_prot &&  is_prot_entry((*left)->GetSeq())) ||
              (!in_pool_prot && !is_prot_entry((*left)->GetSeq()))
            )
          {
          AnalyzeSeqsViaBioseqs1((*left)->SetSeq());
          AnalyzeSeqsViaBioseqs((*left)->SetSeq(), against_seqs, against_prot);
          }
        }
      }

    return -1;

}
int CReadBlastApp::AnalyzeSeqsViaBioseqs(CBioseq& left,
    CBioseq_set::TSeq_set& against_seqs, bool against_prot)
{
  if(PrintDetails()) NcbiCerr << "AnalyzeSeqsViaBioseqs(left, against_seqs, against_prot): "
    << GetStringDescr(left)
    << ", against_prot= " << against_prot << "\n";
  NON_CONST_ITERATE( CBioseq_set::TSeq_set, right, against_seqs)
      {
      if((*right)->IsSet())
        {
        CBioseq_set::TSeq_set& seqs_down = (*right)->SetSet().SetSeq_set();
        AnalyzeSeqsViaBioseqs(left, seqs_down, against_prot);
        }
      else
        {
        string name = GetStringDescr((*right)->GetSeq());
/*
        if(PrintDetails()) NcbiCerr << "AnalyzeSeqsViaBioseqs(left, against_seqs, against_prot): "
           << "right seq entry "
           << name << "\n";
*/
        if(   ( against_prot &&  is_prot_entry((*right)->GetSeq())) ||
              (!against_prot && !is_prot_entry((*right)->GetSeq()))
            )
        AnalyzeSeqsViaBioseqs(left, (*right)->SetSeq() );
        }
      }
  return -1;
}
int CReadBlastApp::AnalyzeSeqsViaBioseqs(CBioseq& left, CBioseq& right)
{
/*
  if(PrintDetails()) NcbiCerr << "AnalyzeSeqsViaBioseqs(left, right): "
    << GetStringDescr(left) << ", " << GetStringDescr(right) << NcbiEndl;
*/
  if(is_prot_entry(left) && !is_prot_entry(right))
    {
//    if(PrintDetails()) NcbiCerr << "AnalyzeSeqsViaBioseqs(left, right): going for overlaps\n";
    overlaps_prot_na(left, right.GetAnnot());
    }

  return -1;
}

int CReadBlastApp::AnalyzeSeqsViaBioseqs1(CBioseq& left)
{

  return -1;
}


// AnalyzeSeqs family
int CReadBlastApp::AnalyzeSeqs(void)
{

   if(PrintDetails()) NcbiCerr << "AnalyzeSeqs: start " << NcbiEndl;
   PushVerbosity();
   if(IsSubmit())
     {
     if (
           m_Submit.GetData().IsEntrys()
            &&
           (*m_Submit.GetData().GetEntrys().begin())->IsSet()
        )
       {
       AnalyzeSeqs((*m_Submit.SetData().SetEntrys().begin())->SetSet().SetSeq_set());
       }
     else
       {
       NcbiCerr << "ERROR: submit file does not have proper seqset"<< NcbiCerr;
       }
     }
   else
     {
       AnalyzeSeqs(m_Entry.SetSet().SetSeq_set());
     }

   PopVerbosity();
   if(PrintDetails()) NcbiCerr << "AnalyzeSeqs: end" << NcbiEndl;
   return -1;
}

// w.out CTypeConstIterator
int CReadBlastApp::AnalyzeSeqs(CBioseq_set::TSeq_set& seqs)
{
   CArgs args = GetArgs();
   IncreaseVerbosity();
   string tblFile;
   if (args["outTbl"].HasValue())
      tblFile = args["outTbl"].AsString();
   else
      tblFile = "/dev/null";
   ofstream tblOut(tblFile.c_str(), IOS_BASE::app | IOS_BASE::out );
   NON_CONST_ITERATE( CBioseq_set::TSeq_set, left, seqs)
     {
     if((*left)->IsSet())
       {
       if(PrintDetails())
           NcbiCerr << "AnalyzeSeqs: going down: "
                    << NcbiEndl;
       CBioseq_set::TSeq_set& seqs2 = (*left)->SetSet().SetSeq_set();
       PushVerbosity();
       AnalyzeSeqs(seqs2);
       PopVerbosity();
       continue;
       }

     if(PrintDetails())
          NcbiCerr << "AnalyzeSeqs: left: "
                   // <<  CSeq_id::GetStringDescr ((*left)->GetSeq(), CSeq_id::eFormat_FastA) << NcbiEndl;
                   <<  GetStringDescr ((*left)->GetSeq()) << NcbiEndl;
/////////////////////////////////
// not a protein. Do NA stuff
     if( !is_prot_entry((*left)->GetSeq())  )
       {
// NA, process all RNA and what not annotations here and compare for overlaps
       // CheckMissingRibosomalRNA((*left)->GetSeq().GetAnnot() );

// check overlaps of the sequence with other features
       overlaps_na((*left)->GetSeq().GetAnnot() );
       continue;
       }
///////////////////////////////////
// compare to...
     CBioseq_set::TSeq_set::iterator right = left; 
     bool again=true;
     bool last_right=false;
     while(again) // have overlaps
       {
       again=false;
       ++right;
       if(!skip_toprot(right, seqs)) {last_right=true; break;}
       if(PrintDetails())
          {
          NcbiCerr << "AnalyzeSeqs: right: "
                   <<  GetStringDescr ((*right)->GetSeq()) << NcbiEndl;
          }
// analyze for overlaps with the next one
       PushVerbosity();
// if there are overlaps, keep on working on left, iterating through right
       again=overlaps((*left)->GetSeq(), (*right)->GetSeq() );
       PopVerbosity();
       }
     if (last_right) break;
     if(PrintDetails())
       NcbiCerr << "AnalyzeSeqs: finished lower level seq, overlaps: "
                << NcbiEndl;
     }

   NON_CONST_ITERATE( CBioseq_set::TSeq_set, left, seqs)
     {
     if((*left)->IsSet()) continue;
// does not hit. Skip
     if( !has_blast_hits((*left)->GetSeq()) ) continue;
     if(PrintDetails()) NcbiCerr << "AnalyzeSeqs: left: valid" <<  NcbiEndl;
     CBioseq_set::TSeq_set::iterator right = left;  ++right;
     if(!skip_to_valid_seq_cand(right, seqs)) break;
     if(PrintDetails()) NcbiCerr << "AnalyzeSeqs: right: valid" <<  NcbiEndl;
     string common_subject;
     bool fit_blast_result = fit_blast((*left)->GetSeq(), (*right)->GetSeq(), common_subject);
     bool lhp = hasProblems((*left)->GetSeq(), m_diag, eFrameShift);
     bool rhp = hasProblems((*right)->GetSeq(), m_diag, eFrameShift);
     bool lhoe = hasProblems((*left)->GetSeq(), m_diag, eMayBeNotFrameShift);
     bool rhoe = hasProblems((*right)->GetSeq(), m_diag, eMayBeNotFrameShift);
     if(PrintDetails())
       NcbiCerr << "AnalyzeSeqs: after fit_blast:"
        << fit_blast_result
        << lhp
        << lhoe
        << rhp
        << rhoe
        << NcbiEndl;
//     if(fit_blast_result && (lhp && !lhoe) && (rhp && !rhoe))
     if(fit_blast_result)
        {
// go to the same sequence set, find first NA, add misc_feature
        append_misc_feature(seqs, GetStringDescr((*left)->GetSeq()), eFrameShift);
        }
     if(PrintDetails())
       NcbiCerr << "AnalyzeSeqs: finished lower level seq, frameshifts: "
                << NcbiEndl;
     }
   DecreaseVerbosity();
   return -1;
}

