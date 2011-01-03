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
      if(PrintDetails()) NcbiCerr << "problem type: " << ProblemType(type) << NcbiEndl;
      reportProblemSequenceName(qname, out);

// reporting
      IncreaseVerbosity();
      NON_CONST_ITERATE(list<problemStr>, problem, diag[qname].problems)
        {
          if(PrintDetails()) NcbiCerr << "current problem: " << ProblemType(problem->type) << NcbiEndl;
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
        problem=diag[qname].problems.erase(problem);
        if(PrintDetails()) NcbiCerr << "erased"  << NcbiEndl;
        if(PrintDetails()) NcbiCerr << "next current problem: " << problem->type << NcbiEndl;
        }
      DecreaseVerbosity();
}
void CReadBlastApp::reportProblems(const bool report_and_forget, diagMap& diag, ostream& out, const EProblem type)
{
   IncreaseVerbosity();
   map<string,bool> done;
   if(PrintDetails()) NcbiCerr << "reportProblems: all problems of type ("  << ProblemType(type) << ")" << NcbiEndl;
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
                               << ProblemType(problem->type) << "\t"
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

string CReadBlastApp::ProblemType(const EProblem type)
{
   strstream strres;
   strres << "unknown_problem_type=" << type << '\0';
   string result=strres.str();
   if(type & eOverlap)
      result = "Potential overlap found";
   else if(type & eRnaOverlap)
      result =  "Potential RNA overlap found";
   else if(type & eCompleteOverlap)
      result =  "Complete overlap found";
   else if(type & eRemoveOverlap)
      result =  "overlap marked for removal";
   else if(type & eRelFrameShift)
      result =  "Something relevant to frame shift found";
   else if(type & eFrameShift)
      result =  "Potential frame shift evidence found";
   else if(type & eMayBeNotFrameShift)
      result =  "Evidence absolving from the frame shift accusation found";
   else if(type & ePartial)
      result =  "Potential partial protein annotation found";
   else if(type & eShortProtein)
      result =  "Short annotation found";
   else if(type & eTRNAMissing)
      result =  "tRNA is missing in the list of independently annotated tRNAs";
   else if(type & eTRNAAbsent)
      result =  "RNA is missing in the list of annotated RNAs in the input";
   else if(type & eTRNABadStrand)
      result =  "RNA is present at the wrong strand";
   else if(type & eTRNAUndefStrand)
      result =  "RNA is present with undefined strand";
   else if(type & eTRNAComMismatch)
      result =  "tRNA is a complete mismatch";
   else if(type & eTRNAMismatch)
      result =  "tRNA has mismatched ends";

   return result;

}

void CReadBlastApp::reportProblemType(const EProblem type, ostream& out)
{
   out
      << "---"
      << " ";
   string stype = ProblemType(type);
   if(!stype.empty())
      {
      out << stype;
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

int CReadBlastApp::FixStrands(void)
{
  TProblem_locs problem_locs;
  if(PrintDetails()) NcbiCerr << "FixStrands: start " << NcbiEndl;

// relevant problems
  CollectRNAFeatures(problem_locs);

// first, count features matching each range
  for(CTypeIterator<CSeq_feat> feat=Begin(); feat; ++feat)
    {
    if (!feat->GetData().IsRna() && !feat->GetData().IsGene()) continue;
    const CSeq_loc&  loc = feat->GetLocation();
    ENa_strand strand;
    TSeqPos from, to;
    getFromTo(loc, from, to, strand); 
    string range = printed_range(from, to);
    if(PrintDetails()) NcbiCerr << "FixStrands: rna or gene [" << range << "]" << NcbiEndl;
    
    if(problem_locs.find(range)==problem_locs.end()) continue; // not relevant
    if(PrintDetails()) NcbiCerr << "FixStrands: range: " << range << NcbiEndl;
    problem_locs[range].count++;
    if(feat->GetData().IsRna()) problem_locs[range].rnacount++;
    if(feat->GetData().IsGene()) problem_locs[range].genecount++;
    }
// now fix
  for(CTypeIterator<CSeq_feat> feat=Begin(); feat; ++feat)
    {
    if (!feat->GetData().IsRna() && !feat->GetData().IsGene()) continue;
    if(PrintDetails()) NcbiCerr << "FixStrands: rna or gene for fixing" << NcbiEndl;
    CSeq_loc&  loc = feat->SetLocation();
    ENa_strand strand;
    TSeqPos from, to;
    getFromTo(loc, from, to, strand);
    string range = printed_range(from, to);
    if(problem_locs.find(range)==problem_locs.end()) continue; // not relevant
    if(PrintDetails()) NcbiCerr << "FixStrands: range for fix: " << range << NcbiEndl;
    if(   problem_locs[range].count!=2 
       || problem_locs[range].rnacount!=1 
       || problem_locs[range].genecount!=1 
      )
      {
      // no touching
      // warning
      NcbiCerr << "CReadBlastApp::FixStrands: "
               << "WARNING: "
               << "location found, but the number of features with that location is confusing, "
               << "no fixing for "
               << "[" << problem_locs[range].name << "]"
               << "(" << range << ")"
               << NcbiEndl;
      continue;
      }
// over all intervals
    int ninter=0; 
    for(CTypeIterator<CSeq_interval> inter = ::Begin(loc);  inter; ++inter, ++ninter)
      {
      inter->SetStrand(problem_locs[range].strand);
      NcbiCerr << "CReadBlastApp::FixStrands: "
               << "[" << problem_locs[range].name << "] "
               << "fixed"
               << NcbiEndl;
      }
    NcbiCerr << "CReadBlastApp::FixStrands: ninters= " << ninter  << NcbiEndl;
    } // for(CTypeIterator<CSeq_feat>
  if(PrintDetails()) NcbiCerr << "FixStrands: end" << NcbiEndl;
  return 1;
}

int CReadBlastApp::RemoveProblems(map<string,string>& problem_names, LocMap& loc_map)
{
   if(PrintDetails()) NcbiCerr << "RemoveProblems: start " << NcbiEndl;

   PushVerbosity();
   if(IsSubmit())
     { 
     if ( m_Submit.GetData().IsEntrys()) 
       {
       for(CSeq_submit::C_Data::TEntrys::iterator entry = m_Submit.SetData().SetEntrys().begin();
           entry != m_Submit.SetData().SetEntrys().end();)
       // NON_CONST_ITERATE(CSeq_submit::C_Data::TEntrys, entry, m_Submit.SetData().SetEntrys())
         {
         int removeme = RemoveProblems(**entry, problem_names, loc_map);
         if(PrintDetails())
            NcbiCerr
                 << "RemoveProblems(void): doing entry: removeme =  "
                 << removeme
                 << NcbiEndl;
         if(removeme) 
           {
           NcbiCerr << "RemoveProblems(): WARNING: "
                    << "CSeq_entry deleted, loss of annotation might occur"
                    << NcbiEndl;
           entry=m_Submit.SetData().SetEntrys().erase(entry);
           }
         else entry++;
         }
       }
     else
       {
       NcbiCerr << "ERROR: submit file does not have proper seqset"<< NcbiEndl;
       }
     }
   else 
     { 
     if(PrintDetails())
         NcbiCerr
                 << "RemoveProblems(void): case is single entry "
                 << NcbiEndl;
     RemoveProblems(m_Entry, problem_names, loc_map);
     }

   PopVerbosity();
   if(PrintDetails()) NcbiCerr << "RemoveProblems: end" << NcbiEndl;
   return 1;
}

int CReadBlastApp::RemoveProblems(CSeq_entry& entry, map<string, string>& problem_seqs, LocMap& loc_map)
{
   int removeme=0;
   if(PrintDetails()) NcbiCerr << "RemoveProblems(CSeq_entry): start" << NcbiEndl;
   if(entry.IsSeq()) 
     {
     removeme=RemoveProblems(entry.SetSeq(), problem_seqs, loc_map);
     if(PrintDetails())
         NcbiCerr
                 << "RemoveProblems(CSeq_entry)(seq case): removeme = "
                 << removeme
                 << NcbiEndl;

     }
   else //several seqs
     {
     CBioseq_set& bioset = entry.SetSet();
     removeme=RemoveProblems(bioset, problem_seqs, loc_map);
     CBioseq_set::TSeq_set& entries =  bioset.SetSeq_set();
     int size=0;
     for (CTypeConstIterator<CBioseq> seq = ::ConstBegin(entry);  seq;  ++seq, size++);
     if(PrintDetails())
         NcbiCerr
                 << "RemoveProblems(CSeq_entry)(set case): removeme = "
                 << removeme
                 << ", entries.size = "
                 << entries.size()
                 << ", total seqs = "
                 << size
                 << NcbiEndl;
     if (size<=1) NormalizeSeqentry(entry);
     } // entry.IsSet()
   if(PrintDetails()) NcbiCerr << "RemoveProblems(CSeq_entry): end" << NcbiEndl;
   return removeme;
}

void CReadBlastApp::NormalizeSeqentry(CSeq_entry& entry)
{
  if(!entry.IsSet()) return;
  CBioseq_set& bioset = entry.SetSet();
  CBioseq_set::TSeq_set& entries =  bioset.SetSeq_set();
  if(entries.size()!=1) return;
// 1. merge descriptions
  CBioseq& seq = (*entries.begin())->SetSeq();
  if(bioset.IsSetDescr())
    {
    CSeq_descr::Tdata& descs = bioset.SetDescr().Set();
    for(CSeq_descr::Tdata::iterator desc = descs.begin(); desc!=descs.end(); )
      {
      seq.SetDescr().Set().push_back(*desc);
      desc=descs.erase(desc);
      }
    } // if(entry.SetSet().IsSetDescr())
// 2.  move CBioseq under the CSeq_entr
  CRef<CBioseq> pseq (&seq);
  entry.SetSeq(*pseq);
  NcbiCerr << "NormalizeSeqentry(CSeq_entry...): "
           << "WARNING: "
           << "converted sequence set to sequence"
           << NcbiEndl;
  return;
}

int CReadBlastApp::RemoveProblems(CBioseq_set& setseq, map<string, string>& problem_seqs, LocMap& loc_map)
{
   bool noseqs=false;
   bool noannot=false;
   int removeme=0;
   if(PrintDetails()) NcbiCerr << "RemoveProblems(CBioseq_set): start" << NcbiEndl;
   if(setseq.IsSetSeq_set())
     {
     int all_entries_removed = RemoveProblems(setseq.SetSeq_set(), problem_seqs, loc_map);
     if(all_entries_removed > 0) {/* mandatory, no deletion */; noseqs=true;}
     }
   if(setseq.IsSetAnnot())
     {
     int all_annot_removed = RemoveProblems(setseq.SetAnnot(), problem_seqs, loc_map);
     if(all_annot_removed > 0) {setseq.ResetAnnot(); noannot=true;}
     }
   if(noseqs ) removeme = 1;
   if(PrintDetails())
         NcbiCerr
                 << "RemoveProblems(CBioseq_set): noseqs = "
                 << noseqs
                 << ", noannot = "
                 << noannot
                 << ", removeme (return) = "
                 << removeme
                 << NcbiEndl;

   return removeme;
}

int CReadBlastApp::RemoveProblems(CBioseq& seq, map<string, string>& problem_seqs, LocMap& loc_map)
{      
   int remove=0;
   if(PrintDetails()) NcbiCerr << "RemoveProblems(CBioseq): start" << NcbiEndl;
   if(!seq.IsAa())  // nucleotide sequnce
     {           
     if(PrintDetails()) NcbiCerr << "RemoveProblems(CBioseq): nuc" << NcbiEndl;
     if(seq.IsSetAnnot())
       {
       int annotations_removed = RemoveProblems(seq.SetAnnot(), problem_seqs, loc_map);
       if(annotations_removed) seq.ResetAnnot();
       }
     }           
   else // aminoacid sequence
// checkif needed to kill it
     {
     string thisName = GetStringDescr(seq);
     string origName = thisName;
     string::size_type ipos = thisName.rfind('|'); if(ipos!=string::npos) thisName.erase(0, ipos+1);
     ipos = thisName.rfind('_'); if(ipos!=string::npos) ipos= thisName.rfind('_', ipos-1);
     if(PrintDetails())
         NcbiCerr
                 << "RemoveProblems(CBioseq): remove? sequence "
                 << "[" << origName << "]"
                 << " looking for "
                 << "[" << thisName << "]"
                 << NcbiEndl;

     if(problem_seqs.find(thisName) != problem_seqs.end()) 
       {
       NcbiCerr
                 << "RemoveProblems(CBioseq): sequence "
                 << "[" << origName << "]"
                 << " is marked for removal, because of a match to " 
                 << "[" << thisName << "]"
                 << NcbiEndl;
       remove=1; // whack the sequence
       }
     }
   if(PrintDetails())
         NcbiCerr
                 << "RemoveProblems(CBioseq): remove =  "
                 << remove 
                 << NcbiEndl;


   return remove;
}          


int CReadBlastApp::RemoveProblems(CBioseq_set::TSeq_set& entries, map<string, string>& problem_seqs, LocMap& loc_map)
{
   IncreaseVerbosity();
   int remove=0;
   for(CBioseq_set::TSeq_set::iterator entries_end = entries.end(), entry=entries.begin(); entry != entries_end; )
     {
     int removeseq=RemoveProblems(**entry, problem_seqs, loc_map);
     if(PrintDetails())
         NcbiCerr
                 << "RemoveProblems(CBioseq_set::TSeq_set): removeseq = "
                 << removeseq
                 << NcbiEndl;

     if(removeseq) entry=entries.erase(entry);
     else entry++;
     } // each seqs
   if(entries.size()==0) remove=1;
   if(PrintDetails())
         NcbiCerr
                 << "RemoveProblems(CBioseq_set::TSeq_set): nentries = "
                 << entries.size()
                 << NcbiEndl;

   DecreaseVerbosity();
   return remove;
}

int CReadBlastApp::RemoveProblems(CBioseq::TAnnot& annots, map<string, string>& problem_seqs, LocMap& loc_map)
{
  int remove=0;
  if(PrintDetails()) NcbiCerr << "RemoveProblems(CBioseq::TAnnot): start" << NcbiEndl;
  for(CBioseq::TAnnot::iterator annot=annots.begin(); annot!=annots.end(); )
    {
    int removeme=0;
    if( (*annot)->GetData().IsFtable()) removeme=RemoveProblems((*annot)->SetData().SetFtable(), problem_seqs, loc_map);
    if(removeme) 
      {
      NcbiCerr << "RemoveProblems(annots, problem_seqs): "
                 << "INFO: "
                 << "annotation has empty feature table and it will be removed"
                 << NcbiEndl;
      annot=annots.erase(annot);
      }
    else annot++;
    }
  if(annots.size()==0) remove=1;
  if(PrintDetails()) NcbiCerr << "RemoveProblems(CBioseq::TAnnot): end" << NcbiEndl;

  return remove;
}

int CReadBlastApp::RemoveProblems(CSeq_annot::C_Data::TFtable& table, map<string, string>& problem_seqs, LocMap& loc_map)
// this one needs cleaning
{
  int removeme=0;
  if(PrintDetails()) NcbiCerr << "RemoveProblems(CSeq_annot::C_Data::TFtable): start" << NcbiEndl;
  GetLocMap(loc_map, table);
  for(CSeq_annot::C_Data::TFtable::iterator feat_end = table.end(), feat = table.begin(); feat != feat_end;)
    {
    if(PrintDetails()) NcbiCerr << "RemoveProblems(CSeq_annot::C_Data::TFtable): feat 000" << NcbiEndl;
    bool gene, cdregion;
    gene = (*feat)->GetData().IsGene();
    cdregion = (*feat)->GetData().IsCdregion();
    bool del_feature=false;
    if(PrintDetails()) NcbiCerr << "RemoveProblems(CSeq_annot::C_Data::TFtable): feat I" << NcbiEndl;
    string real_loc_string = GetLocationString((*feat)->GetLocation());
    if(PrintDetails()) NcbiCerr << "RemoveProblems(CSeq_annot::C_Data::TFtable): feat II" << NcbiEndl;

    string loc_string = GetLocusTag(**feat, loc_map); // more general, returns location string
    if(PrintDetails()) NcbiCerr << "RemoveProblems(CSeq_annot::C_Data::TFtable): feat: (" << real_loc_string  << ")(" << loc_string << ")" << NcbiEndl;
//
// case *: matching locus tag
//
    if(problem_seqs.find(loc_string) != problem_seqs.end()) 
      {
      if((*feat)->GetData().IsImp() &&
         (*feat)->GetData().GetImp().CanGetKey())
         {
         NcbiCerr << "RemoveProblems: INFO: feature " << loc_string << ": imp, key = " << (*feat)->GetData().GetImp().GetKey()  << NcbiEndl;
         }
      if((*feat)->GetData().IsImp() &&
          (*feat)->CanGetComment() )
         {
         NcbiCerr << "RemoveProblems: INFO: feature " << loc_string << ": imp, comment = " << (*feat)->GetComment()  << NcbiEndl;
         }
/*
      if((*feat)->GetData().IsImp() &&
         (*feat)->GetData().GetImp().CanGetKey() &&
         (*feat)->GetData().GetImp().GetKey() == "misc_feature" &&
         (*feat)->CanGetComment() &&
         (*feat)->GetComment().find("potential frameshift") != string::npos
        ) del_feature = false; // this is a new feature, that we are not supposed to delete
*/
      if((*feat)->GetData().IsImp() &&
         (*feat)->GetData().GetImp().CanGetKey() &&
         (*feat)->GetData().GetImp().GetKey() == "misc_feature"
        ) del_feature = false; // this is a new feature, we do not delete them
      else del_feature=true;
      }

    if ( PrintDetails() )
      {
      NcbiCerr << "RemoveProblems: feature " << loc_string << ": ";
      if(del_feature) NcbiCerr << "WILL BE REMOVED";
      else            NcbiCerr << "stays until further analysis for it";
      NcbiCerr << NcbiEndl;
      }
    if(del_feature)
        {
        NcbiCerr << "RemoveProblems: WARNING: feature " 
                 << "{" << (*feat)->GetData().SelectionName((*feat)->GetData().Which()) << "} "
                 << loc_string << ": ";
        NcbiCerr << "will be removed because of a problem: ";
        NcbiCerr << problem_seqs.find(loc_string)->second;
        NcbiCerr << NcbiEndl;
        }
    if(!del_feature && gene  && (*feat)->GetData().GetGene().CanGetLocus_tag() )
//
// case *: gene
//
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
      if(del_feature)
        {
        NcbiCerr << "RemoveProblems: WARNING: gene " << locus_tag << ": ";
        NcbiCerr << "will be removed because of a problem: ";
        NcbiCerr << problem_seqs.find(locus_tag)->second;
        NcbiCerr << NcbiEndl;
        }
      }
    if(!del_feature && cdregion && (*feat)->CanGetProduct() )
      {
//
// case *: cdregion
//
      string productName;
      if( (*feat)->CanGetProduct() &&
          (*feat)->GetProduct().IsWhole() &&
          (*feat)->GetProduct().GetWhole().IsGeneral() &&
          (*feat)->GetProduct().GetWhole().GetGeneral().CanGetTag() &&
          (*feat)->GetProduct().GetWhole().GetGeneral().GetTag().IsStr() )
        {
        productName = (*feat)->GetProduct().GetWhole().GetGeneral().GetTag().GetStr();
        }
      else if ( 
           (*feat)->CanGetProduct() &&
          (*feat)->GetProduct().IsWhole())
        {
        productName = (*feat)->GetProduct().GetWhole().AsFastaString();
        }
// strip leading contig ID if any
      string::size_type ipos=productName.rfind('_', productName.size());
      if(ipos != string::npos) 
        {
        string::size_type ipos2;
        ipos2=productName.rfind('_', ipos-1);
        if(ipos2 != string::npos) productName.erase(0, ipos2+1);
      // "1103032000567_RAZWK3B_00550" -> "RAZWK3B_00550";
        else 
          {
          ipos2=productName.rfind('|', ipos-1); 
          if(ipos2 != string::npos) productName.erase(0, ipos2+1);
          }
// lcl|Xoryp_00025 -> Xoryp_00025
        }
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
    if(del_feature)
      {
      if(problem_seqs.find(real_loc_string) == problem_seqs.end()) 
        {
        problem_seqs[real_loc_string]=problem_seqs[loc_string]; // saving for later
        }
      }
    if(del_feature) feat=table.erase(feat);
    else feat++;
    }
  if(table.size()==0) removeme=1; 
  if(PrintDetails()) NcbiCerr << "RemoveProblems(CSeq_annot::C_Data::TFtable): end" << NcbiEndl;
  return removeme;
}

// RemoveInterim

int CReadBlastApp::RemoveInterim(void)
{

   int nremoved=0;

   if(PrintDetails()) NcbiCerr << "RemoveInterim: start " << NcbiEndl;
   PushVerbosity();
   for(CTypeIterator<CBioseq> seq=Begin(); seq; ++seq)
     {
     if(seq->IsSetAnnot() && seq->IsAa()) nremoved+= RemoveInterim(seq->SetAnnot());
     if(seq->IsSetAnnot() && seq->IsNa()) nremoved+= RemoveInterim2(seq->SetAnnot());
     }

   PopVerbosity();
   if(PrintDetails()) NcbiCerr << "RemoveInterim: end" << NcbiEndl;
   return nremoved;
}
// protein sequence annotations
int CReadBlastApp::RemoveInterim(CBioseq::TAnnot& annots)
{
   int nremoved=0;
   if(PrintDetails()) NcbiCerr << "RemoveInterim(annots): start " << NcbiEndl;

   for(CBioseq::TAnnot::iterator annot=annots.begin(), annot_end = annots.end(); annot != annot_end; )
     {
     bool erased = false;
     if((*annot)->GetData().IsAlign())
       {
       nremoved++; erased = true;
       }
     if ( (*annot)->GetData().IsFtable())
       {
       int dremoved=0;
       CSeq_annot::C_Data::TFtable& table = (*annot)->SetData().SetFtable();
       for(CSeq_annot::C_Data::TFtable::iterator feat=table.begin(), feat_end= table.end(); feat !=  feat_end; )
         {
         string test = "Genomic Location:"; 
         if ((*feat)->IsSetData() && (*feat)->GetData().IsProt() && 
              (*feat)->IsSetComment() && (*feat)->GetComment().substr(0, test.size()) == test  )
           {
           table.erase(feat++); dremoved++; 
           nremoved++;
           }
         else
           {
           feat++;
           }
         }

/* 
  this is really crappy way of doing it!
*/
/*
       while( (*annot)->GetData().GetFtable().size()>1 )
         {
         (*annot)->SetData().SetFtable().pop_back();
         nremoved++;
         dremoved++;
         }
*/
       if(PrintDetails()) NcbiCerr << "RemoveInterim(CBioseq::TAnnot& annots): dremoved = "
        << dremoved
        << ", left=" << (*annot)->GetData().GetFtable().size()
        << NcbiEndl;
       if((*annot)->SetData().SetFtable().size() == 0) 
         {
         nremoved++;
         erased = true;
         }
       }
     if(erased) annot=annots.erase(annot);
     else annot++;
     }

  if(PrintDetails()) NcbiCerr << "RemoveInterim(annots): end" << NcbiEndl;
  return nremoved;
}

// nucleotide sequence annotations
int CReadBlastApp::RemoveInterim2(CBioseq::TAnnot& annots)
{
   int nremoved=0;
   if(PrintDetails()) NcbiCerr << "RemoveInterim2(annots): start " << NcbiEndl;

   NON_CONST_ITERATE(CBioseq::TAnnot, gen_feature, annots)
     {
     if(PrintDetails()) NcbiCerr << "RemoveInterim2(annots): gen_feature start" << NcbiEndl;
     if ( !(*gen_feature)->GetData().IsFtable() ) continue;
     if(PrintDetails()) NcbiCerr << "RemoveInterim2(annots): gen_feature is ftable" << NcbiEndl;
     map<string,bool> feat_defined;
     CSeq_annot::C_Data::TFtable& table = (*gen_feature)->SetData().SetFtable();
     for(CSeq_annot::C_Data::TFtable::iterator feat_end = table.end(), feat = table.begin(); feat != feat_end;)
       {
       if(PrintDetails()) NcbiCerr << "RemoveInterim2(annots): gen_feature feat start" << NcbiEndl;
       const CSeq_feat& featr = **feat;
       strstream buff;
       buff << MSerial_AsnText  << featr;
       buff << '\0';
       if(PrintDetails()) NcbiCerr << "RemoveInterim2(annots): feat ASN:" << NcbiEndl;
       if(PrintDetails()) NcbiCerr << buff.str() << NcbiEndl;
       if(feat_defined.find(buff.str()) != feat_defined.end()) // dupe
         {
         if(PrintDetails()) NcbiCerr << "RemoveInterim2(annots): gen_feature feat dupe: erase" << NcbiEndl;
         feat=table.erase(feat);
         }
       else 
         {
         if(PrintDetails()) NcbiCerr << "RemoveInterim2(annots): gen_feature feat new: add to feat_defined map" << NcbiEndl;
         feat_defined[buff.str()]=true;
         if(PrintDetails()) NcbiCerr << "RemoveInterim2(annots): gen_feature feat new: add to feat_defined map done" << NcbiEndl;
         // feat_defined[featr]=true;
         feat++;
         }
       if(PrintDetails()) NcbiCerr << "RemoveInterim2(annots): gen_feature feat end" << NcbiEndl;
       }
     
     if(PrintDetails()) NcbiCerr << "RemoveInterim2(annots): gen_feature end" << NcbiEndl;
     }

  if(PrintDetails()) NcbiCerr << "RemoveInterim2(annots): end" << NcbiEndl;
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

int CReadBlastApp::CollectRNAFeatures(TProblem_locs& problem_locs)
{
  ITERATE( diagMap, feat, m_diag)
    {
    ITERATE(list<problemStr>, problem, feat->second.problems)
      {
      bool added = false;
      string name = feat->first;
      string::size_type ipos = name.rfind('|'); if(ipos!=string::npos) name.erase(0, ipos+1);
      ipos = name.rfind('_'); if(ipos!=string::npos) ipos= name.rfind('_', ipos-1);
      if(ipos!=string::npos) name.erase(0, ipos+1);
      string range = printed_range(problem->i1, problem->i2);
      if( 
          (problem->type & eTRNABadStrand || problem->type & eTRNAUndefStrand)   // irrelevant problem
         && !problem->misc_feat_message.empty()
       )
        { 
        problem_locs[range].strand = problem->strand;
        problem_locs[range].name = name;
        problem_locs[range].count = 
        problem_locs[range].rnacount = 
        problem_locs[range].genecount =  0;
        added=true; 
        }
      if(PrintDetails()) 
        NcbiCerr << "CReadBlastApp::CollectRNAFeatures: " << feat->first
        << "[" << range << "]: "
        << "(" << name << ")"
        << (added ? "added" : "skipped")  << NcbiEndl;
      }
    }
  return problem_locs.size();

}

int CReadBlastApp::CollectFrameshiftedSeqs(map<string,string>& problem_names)
{
  CArgs args = GetArgs();
  bool keep_frameshifted = args["kfs"].HasValue();
  ITERATE( diagMap, feat, m_diag)
    {
    ITERATE(list<problemStr>, problem, feat->second.problems)
      {
      bool added = false;
      string name = feat->first;
      string::size_type ipos = name.rfind('|'); if(ipos!=string::npos) name.erase(0, ipos+1);
      ipos = name.rfind('_'); if(ipos!=string::npos) ipos= name.rfind('_', ipos-1);
      if(ipos!=string::npos) name.erase(0, ipos+1);
      if( 
          (problem->type == eFrameShift && !keep_frameshifted)
          || 
          problem->type == eRemoveOverlap
          ||
          problem->type & eShortProtein
        ) 
        { problem_names[name]=ProblemType(problem->type); added=true; }
      if(PrintDetails()) 
        NcbiCerr << "CollectFrameshiftedSeqs: " << feat->first
        << ": "
        << "(" << name << ")"
        << (added ? "added" : "skipped")  << NcbiEndl;
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
      typedef map<string, bool> Tmessage_misced;
      typedef map<EProblem, Tmessage_misced> Tproblem_misced;
      Tproblem_misced problem_misced; // list of problems fixed
      ITERATE(list<problemStr>, problem, m_diag[name].problems)
        {
        if ( !(problem->type & problem_type) ) continue;
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
        if(problem_misced.find(problem->type) != problem_misced.end() &&
           problem_misced[problem->type].find(message) != problem_misced[problem->type].end()
          ) continue;
        else problem_misced[problem->type][message] = true;
        SIZE_TYPE pos;
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
        }

      }
    break;
    }

  return;
}




