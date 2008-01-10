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

void CReadBlastApp::reportProblems(const bool report_and_forget, diagMap& diag, ostream& out,
   const CBioseq::TAnnot& annots, const EProblem type)
{
   ITERATE(CBioseq::TAnnot, gen_feature, annots)
     {
     if ( !(*gen_feature)->GetData().IsFtable() ) continue;
     reportProblems(report_and_forget, diag, out, (*gen_feature)->GetData().GetFtable(), type);
     }
}

void CReadBlastApp::reportProblems(const bool report_and_forget, diagMap& diag, ostream& out,
   const CSeq_annot::C_Data::TFtable& feats, const EProblem type)
{
  ITERATE(CSeq_annot::C_Data::TFtable, f, feats)
    {
    if( !(*f)->GetData().IsGene() ) continue;
    string qname; (*f)->GetData().GetGene().GetLabel(&qname);
    if( diag.find(qname) == diag.end() ) continue;
    reportProblems(qname, diag, out, type);
    if(report_and_forget) erase_problems(qname, diag, type);
    }
}
void CReadBlastApp::reportProblems(const string& qname, diagMap& diag, ostream& out, const EProblem type)
{
      if(!hasProblems(qname, diag, type)) return;
      if(PrintDetails()) NcbiCerr << "problem type: " << type << NcbiEndl;
      reportProblemSequenceName(qname, out);

// reporting
      IncreaseVerbosity();
      NON_CONST_ITERATE(list<problemStr>, problem, diag[qname].problems)
        {
          if(PrintDetails()) NcbiCerr << "current problem: " << problem->type << NcbiEndl;
          if( !(problem->type & type) ) continue;
          if(PrintDetails()) NcbiCerr << "it is that problem"  << NcbiEndl;
          if(!problem->message.size()) continue;
          if(PrintDetails()) NcbiCerr << "has nonzero message"  << NcbiEndl;
          reportProblemType(problem->type, out);
          reportProblemMessage(problem->message, out);
          if(PrintDetails()) NcbiCerr << "current problem: done: " << problem->type << NcbiEndl;
        }
      DecreaseVerbosity();
}
void CReadBlastApp::erase_problems(const string& qname, diagMap& diag, const EProblem type)
{
      IncreaseVerbosity();
      for(list<problemStr>::iterator problem = diag[qname].problems.begin(); problem!=diag[qname].problems.end();)
        {
        if(PrintDetails()) NcbiCerr << "erasing? current problem: " << problem->type << NcbiEndl;
        if( !(problem->type & type) )
             {
             problem++;
             continue;
             }
        if(PrintDetails()) NcbiCerr << "it is that problem for erazing"  << NcbiEndl;
        diag[qname].problems.erase(problem++);
        if(PrintDetails()) NcbiCerr << "erased"  << NcbiEndl;
        if(PrintDetails()) NcbiCerr << "next current problem: " << problem->type << NcbiEndl;
        }
      DecreaseVerbosity();
}
void CReadBlastApp::reportProblems(const bool report_and_forget, diagMap& diag, ostream& out, const EProblem type)
{
   IncreaseVerbosity();
   map<string,bool> done;
   for (CTypeConstIterator<CBioseq> seq = ConstBegin();  seq;  ++seq)
      {
      if ( !is_prot_entry(*seq) )
        {
        reportProblems(report_and_forget, diag, out, seq->GetAnnot(), type);
        }
      string qname1 = GetStringDescr (*seq);
      string qname2 = CSeq_id::GetStringDescr (*seq, CSeq_id::eFormat_FastA);
      if(PrintDetails()) NcbiCerr << "reportProblems: start: " 
          << qname1 << " or "
          << qname2 
          << NcbiEndl;
      string qnames[2];
      qnames[0]=qname1; qnames[1]=qname2;
      for(int i=0; i<2; i++)
        {
        string& qname = qnames[i];
        if( diag.find(qname) != diag.end() )
          {
          reportProblems(qname, diag, out, type);
          if(report_and_forget) erase_problems(qname, diag, type);
          done[qname]=true;
          if(PrintDetails()) NcbiCerr << "reportProblems: full end: " << qname << NcbiEndl;
          }
        }
      }

   ITERATE(diagMap, problem, diag)
     {
     string qname = problem->first;
     if(done.find(qname)!=done.end()) continue;
     reportProblems(qname, diag, out, type);
     if(report_and_forget) erase_problems(qname, diag, type);
     if(PrintDetails()) NcbiCerr << "reportProblems: alternative problems: " << qname << NcbiEndl;
     }
   DecreaseVerbosity();
}
// hasProblemType(seq, diag[qname].problems), problem->type

bool CReadBlastApp::hasProblems(const CBioseq& seq, diagMap& diag, const EProblem type)
{
   string qname = GetStringDescr (seq);
   string qname2 = CSeq_id::GetStringDescr (seq, CSeq_id::eFormat_FastA);
   return hasProblems(qname, diag, type) || hasProblems(qname2, diag, type);
}

bool CReadBlastApp::hasProblems(const string& qname, diagMap& diag, const EProblem type)
{
  if(PrintDetails()) NcbiCerr << "hasProblems: start: " << qname << NcbiEndl;
  if( type != eAllProblems && diag.find(qname) == diag.end() ) return false;
  if ( type == eAllProblems && diag[qname].problems.size()>0) return true;

  IncreaseVerbosity();
  ITERATE(list<problemStr>, problem, diag[qname].problems)
    {
    if(PrintDetails()) NcbiCerr << "hasProblems: checking problem type: " << "\t"
                               << problem->type << "\t"
                               << qname << "\t"
                               << NcbiEndl;
    if (problem->type & type)
      {
      if(PrintDetails()) NcbiCerr << "hasProblems: end: does have problem: " << qname << NcbiEndl;
      return true;
      }
    }
  DecreaseVerbosity();
  if(PrintDetails()) NcbiCerr << "hasProblems: end: no problem: " << qname << NcbiEndl;
  return false;
}

void CReadBlastApp::reportProblemMessage(const string& message, ostream& out)
{
   out << message.c_str() << NcbiEndl;
}
void CReadBlastApp::reportProblemType(const EProblem type, ostream& out)
{
   out
      << "---"
      << " ";
   if(type == eOverlap)
      {
      out << "Potential overlap found";
      }
   else if(type == eRnaOverlap)
      {
      out << "Potential RNA overlap found";
      }
   else if(type == eCompleteOverlap)
      {
      out << "Complete overlap found";
      }
   else if(type == eFrameShift)
      {
      out << "Potential frame shift evidence found";
      }
   else if(type == eMayBeNotFrameShift)
      {
      out << "Evidence absolving from the frame shift accusation found";
      }
   else if(type == ePartial)
      {
      out << "Potential partial protein annotation found";
      }
   else if(type == eTRNAMissing)
      {
      out << "tRNA is missing in the list of independently annotated tRNAs";
      }
   else if(type == eTRNAAbsent)
      {
      out << "RNA is missing in the list of annotated RNAs in the input";
      }
   else if(type == eTRNABadStrand)
      {
      out << "RNA is present at the wrong strand";
      }
   else if(type == eTRNAComMismatch)
      {
      out << "tRNA is a complete mismatch";
      }
   else if(type == eTRNAMismatch)
      {
      out << "tRNA has mismatched ends";
      }
   else
      {
      NcbiCerr << "FATAL: internal error: unknown problem type" << type << NcbiEndl;
      throw;
      }
   out
      << " "
      << "---"
      << NcbiEndl;
}

void CReadBlastApp::reportProblemSequenceName(const string& name, ostream& out)
{
   out
      << "====="
      << " ";
   out << name.c_str() ;
   out
      << " "
      << "====="
      << NcbiEndl;
}
int CReadBlastApp::RemoveProblems(void)
{

   if(PrintDetails()) NcbiCerr << "RemoveProblems: start " << NcbiEndl;
   map<string,bool> problem_names;
   PushVerbosity();
   CollectFrameshiftedSeqs(problem_names);
   if(IsSubmit())
     { 
     if (
           m_Submit.GetData().IsEntrys()
            &&
           (*m_Submit.GetData().GetEntrys().begin())->IsSet()
        ) 
       {
// annotations for a sequence set, separate from the sequence
       if ( (*m_Submit.SetData().SetEntrys().begin())->SetSet().CanGetAnnot() )
         {
         RemoveProblems( (*m_Submit.SetData().SetEntrys().begin())->SetSet().SetAnnot(),
           problem_names); 
         }
       RemoveProblems((*m_Submit.SetData().SetEntrys().begin())->SetSet().SetSeq_set(), problem_names);
       }
     else
       {
       NcbiCerr << "ERROR: submit file does not have proper seqset"<< NcbiCerr;
       }
     }
   else
     {
       if(m_Entry.SetSet().CanGetAnnot())
         RemoveProblems(m_Entry.SetSet().SetAnnot(), problem_names);
       RemoveProblems(m_Entry.SetSet().SetSeq_set(), problem_names);
     }
   PopVerbosity();
   if(PrintDetails()) NcbiCerr << "RemoveProblems: end" << NcbiEndl;
   return -1;
}

int CReadBlastApp::RemoveProblems(CBioseq_set::TSeq_set& seqs, const map<string, bool>& problem_seqs)
{
   CArgs args = GetArgs();
   IncreaseVerbosity();
   for(CBioseq_set::TSeq_set::iterator left_end = seqs.end(),
       left=seqs.begin(); left != left_end; )
     {
     if((*left)->IsSet())
       {
       if(PrintDetails())
           NcbiCerr << "RemoveProblems: going down: "
                    << NcbiEndl;
       CBioseq_set::TSeq_set& seqs2 = (*left)->SetSet().SetSeq_set();
       PushVerbosity();
       RemoveProblems(seqs2, problem_seqs);
       PopVerbosity();
       }
     else
/*
this probably does not work. Silently
*/
       {
// remove sequence with all its innotations
       string thisName = GetStringDescr((*left)->GetSeq());
       string::size_type ipos = thisName.rfind('|'); if(ipos!=string::npos) thisName.erase(0, ipos+1);
       ipos = thisName.rfind('_'); if(ipos!=string::npos) ipos= thisName.rfind('_', ipos-1);

       if(problem_seqs.find(thisName) != problem_seqs.end()) // whack the sequence
           {
           seqs.erase(left++);
           }
       else if((*left)->GetSeq().IsNa())
         {
         RemoveProblems((*left)->SetSeq().SetAnnot(), problem_seqs);
         }
       }
     left++;
     }

  return 1;
}

int CReadBlastApp::RemoveProblems(CBioseq::TAnnot& annots, const map<string, bool>& problem_seqs)
{
  NON_CONST_ITERATE(CBioseq::TAnnot, annot, annots)
    {
    if( !(*annot)->GetData().IsFtable()) continue;
    RemoveProblems((*annot)->SetData().SetFtable(), problem_seqs);
    }

  return 1;
}

int CReadBlastApp::RemoveProblems(CSeq_annot::C_Data::TFtable& table, const map<string, bool>& problem_seqs)
{
  LocMap loc_map;
  GetLocMap(loc_map, table);
  for(CSeq_annot::C_Data::TFtable::iterator feat_end = table.end(),
      feat     = table.begin();
      feat != feat_end;)
    {
    bool gene, cdregion;
    gene = (*feat)->GetData().IsGene();
    cdregion = (*feat)->GetData().IsCdregion();
    bool del_feature=false;
/*
    string loc_string = GetLocationString(**feat);
    if(problem_seqs.find(loc_string) != problem_seqs.end()) del_feature=true;
*/
    string loc_string = GetLocusTag(**feat, loc_map); // more general, returns location string
    if(problem_seqs.find(loc_string) != problem_seqs.end()) del_feature=true;

    if ( PrintDetails() )
      {
      NcbiCerr << "RemoveProblems: feature " << loc_string << ": ";
      if(del_feature)
           NcbiCerr << "WILL BE REMOVED";
      else
           NcbiCerr << "stays until further analysis for it";
      NcbiCerr << NcbiEndl;
      }
    if(!del_feature && gene  && (*feat)->GetData().GetGene().CanGetLocus_tag() )
      {
      string locus_tag = (*feat)->GetData().GetGene().GetLocus_tag();
      if(problem_seqs.find(locus_tag) != problem_seqs.end()) del_feature=true;
      if ( PrintDetails() )
        {
        NcbiCerr << "RemoveProblems: gene " << locus_tag << ": ";
        if(del_feature)
           NcbiCerr << "WILL BE REMOVED";
        else
           NcbiCerr << "stays";
        NcbiCerr << NcbiEndl;
        }
      }
    if(!del_feature && cdregion && (*feat)->CanGetProduct() )
      {
      string productName;
      if( (*feat)->CanGetProduct() &&
          (*feat)->GetProduct().IsWhole() &&
          (*feat)->GetProduct().GetWhole().IsGeneral() &&
          (*feat)->GetProduct().GetWhole().GetGeneral().CanGetTag() &&
          (*feat)->GetProduct().GetWhole().GetGeneral().GetTag().IsStr() )
        {
        productName = (*feat)->GetProduct().GetWhole().GetGeneral().GetTag().GetStr();
        }
// strip leading contig ID if any
      string::size_type ipos=productName.rfind('_', productName.size());
      if(ipos != string::npos) ipos=productName.rfind('_', ipos-1);
      if(ipos != string::npos) productName.erase(0, ipos+1);
      // "1103032000567_RAZWK3B_00550" -> "RAZWK3B_00550";
      if(productName.length() && problem_seqs.find(productName) != problem_seqs.end()) del_feature=true;
      if ( PrintDetails() )
        {
        NcbiCerr << "RemoveProblems: cdregion " << productName << ": ";
        if(del_feature)
           NcbiCerr << "WILL BE REMOVED";
        else
           NcbiCerr << "stays";
        NcbiCerr << NcbiEndl;
        }
      }
    if(del_feature) table.erase(feat++);
    else feat++;
    }
  return 1;
}

// RemoveInterim

int CReadBlastApp::RemoveInterim(void)
{

   int nremoved=0;

   if(PrintDetails()) NcbiCerr << "RemoveInterim: start " << NcbiEndl;
   PushVerbosity();
   if(IsSubmit())
     {
     if (
           m_Submit.GetData().IsEntrys()
            &&
           (*m_Submit.GetData().GetEntrys().begin())->IsSet()
        )
       {
       RemoveInterim((*m_Submit.SetData().SetEntrys().begin())->SetSet().SetSeq_set());
       }
     else
       {
       NcbiCerr << "ERROR: submit file does not have proper seqset"<< NcbiCerr;
       }
     }
   else
     {
     nremoved = RemoveInterim(m_Entry.SetSet().SetSeq_set());
     if(PrintDetails()) NcbiCerr << "RemoveInterim: nremoved = " << nremoved << NcbiEndl;
     int nremoved2 = RemoveInterim(m_Entry.SetSet().SetSeq_set());
     if(nremoved2)
       {
       if(PrintDetails())
         NcbiCerr << "RemoveInterim: ERROR: second take on set still gives removed"  << NcbiEndl;
       }
     }

   PopVerbosity();
   if(PrintDetails()) NcbiCerr << "RemoveInterim: end" << NcbiEndl;
   return nremoved;
}

int CReadBlastApp::RemoveInterim(CBioseq_set::TSeq_set& seqs)
{
   int nremoved_this=0;
   // CArgs args = GetArgs();
   IncreaseVerbosity();
   NON_CONST_ITERATE(CBioseq_set::TSeq_set, left, seqs)
     {
     if((*left)->IsSet())
       {
       if(PrintDetails())
           NcbiCerr << "RemoveInterim: going down: "
                    << NcbiEndl;

       CBioseq_set::TSeq_set& seqs2 = (*left)->SetSet().SetSeq_set();
       PushVerbosity();
       RemoveInterim(seqs2);
       PopVerbosity();
       }
     else
       {
       if(! (*left)->GetSeq().IsAa()) continue;
       int nremoved = RemoveInterim((*left)->SetSeq().SetAnnot());
       nremoved_this+=nremoved;
       if(PrintDetails()) NcbiCerr << "RemoveInterim: " << GetStringDescr ((*left)->GetSeq()) << ": removed: " << nremoved << NcbiEndl;
       int nremoved2 = RemoveInterim((*left)->SetSeq().SetAnnot());
       if(PrintDetails()) NcbiCerr << "RemoveInterim: " << GetStringDescr ((*left)->GetSeq()) << ": removed2: " << nremoved2 << NcbiEndl;
       if(nremoved2)
         {
         if(PrintDetails()) NcbiCerr << "RemoveInterim: ERROR: second take on annots still has removes" << NcbiEndl;
         }
       }
     }

  return nremoved_this;
}

int CReadBlastApp::RemoveInterim(CBioseq::TAnnot& annots)
{
   int nremoved=0;

   for(CBioseq::TAnnot::iterator annot=annots.begin(), annot_end = annots.end(); annot != annot_end; )
     {
     if((*annot)->GetData().IsAlign())
       {
       annots.erase(annot++);
       nremoved++;
       continue;
       }
     if ( (*annot)->GetData().IsFtable())
       {
       int dremoved=0;
       while( (*annot)->GetData().GetFtable().size()>1 )
         {
         (*annot)->SetData().SetFtable().pop_back();
         nremoved++;
         dremoved++;
         }
       if(PrintDetails()) NcbiCerr << "RemoveInterim(CBioseq::TAnnot& annots): dremoved = "
        << dremoved
        << ", left=" << (*annot)->GetData().GetFtable().size()
        << NcbiEndl;
       }
     annot++;
     }

  return nremoved;
}
string diagName(const string& type, const string& value)
{         
   return type + "|" + value;
}               
                
int addProblems(list<problemStr>& dest, const list<problemStr>& src)
{    
   int n=0;
   ITERATE(list<problemStr>, src_p, src)
     {
     dest.push_back(*src_p);
     n++;
     } 
  return n;
}

int CReadBlastApp::CollectFrameshiftedSeqs(map<string,bool>& problem_names)
{
  ITERATE( diagMap, feat, m_diag)
    {
    ITERATE(list<problemStr>, problem, feat->second.problems)
      {
      string name = feat->first;
      string::size_type ipos = name.rfind('|'); if(ipos!=string::npos) name.erase(0, ipos+1);
      ipos = name.rfind('_'); if(ipos!=string::npos) ipos= name.rfind('_', ipos-1);
      if(ipos!=string::npos) name.erase(0, ipos+1);
      if(problem->type == eFrameShift || problem->type == eRemoveOverlap) problem_names[name]=true;
      if(PrintDetails()) NcbiCerr << "CollectFrameshiftedSeqs: " << feat->first
        << ": "
        << "(" << name << ")"
        << (problem->type == eFrameShift ? "added" : "skipped" ) << NcbiEndl;
      }
    }
  return problem_names.size();
}

void CReadBlastApp::append_misc_feature(CBioseq_set::TSeq_set& seqs, const string& name, EProblem problem_type)
{   
  if(m_diag.find(name)==m_diag.end())
    {
    // should not happen
    NcbiCerr << "append_misc_feature: FATAL: do not have problems for " << name << NcbiEndl;
    throw;
    return;
    } 

  NON_CONST_ITERATE(  CBioseq_set::TSeq_set, na, seqs)
    {
    if( is_prot_entry((*na)->GetSeq())  ) continue;
    list<CRef<CSeq_id> >& na_id = (*na)->SetSeq().SetId();
    NON_CONST_ITERATE(CBioseq::TAnnot, gen_feature, (*na)->SetSeq().SetAnnot())
      {
      if ( !(*gen_feature)->GetData().IsFtable() ) continue;
      map<EProblem, bool> problem_misced;
      ITERATE(list<problemStr>, problem, m_diag[name].problems)
        {
        if ( !(problem->type & problem_type) ) continue;
        if(problem_misced.find(problem->type) != problem_misced.end() ) continue;
        int from=-1, to=-1;
//        string lt1, lt2;
        ENa_strand strand;
        string message="";
        from = problem->i1;
        if(from<0) continue;
// add new feature
        to   = problem->i2;
/*
        lt1  = problem->id1;
        lt2  = problem->id2;
*/
        strand = problem->strand;
        message = problem->misc_feat_message;
        if(message.size()==0) continue; // do not print empty misc_feat, they are empty for a reason
        int pos;
        while((pos=message.find_first_of("\n\r"))!=string::npos)
          {
          message[pos]=' ';
          }
        CRef<CSeq_feat> feat(new CSeq_feat());

        feat->SetComment(message);
        feat->SetData().SetImp().SetKey("misc_feature");
        feat->SetLocation().SetInt().SetFrom(from);
        feat->SetLocation().SetInt().SetTo(to);
        feat->SetLocation().SetInt().SetId(**na_id.begin());
        feat->SetLocation().SetInt().SetStrand(strand);
        (*gen_feature)->SetData().SetFtable().push_back(feat);
        problem_misced[problem->type] = true;
        }

      }
    break;
    }

  return;
}




