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
*   major top-level routines, including main
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include "read_blast_result.hpp"

// static members definition
// algorithm control
double CReadBlastApp::m_small_tails_threshold;
int    CReadBlastApp::m_n_best_hit;
double CReadBlastApp::m_eThreshold; 
double CReadBlastApp::m_entireThreshold; 
double CReadBlastApp::m_partThreshold; 
int    CReadBlastApp::m_rna_overlapThreshold;
int    CReadBlastApp::m_cds_overlapThreshold;
double CReadBlastApp::m_trnascan_scoreThreshold; 
// verbosity
int    CReadBlastApp::m_verbosity_threshold;
int    CReadBlastApp::m_current_verbosity;
stack < int > CReadBlastApp::m_saved_verbosity;

ESerialDataFormat s_GetFormat(const string& name)
{
    if (name == "asn") {
        return eSerial_AsnText;
    } else if (name == "asnb") {
        return eSerial_AsnBinary;
    } else if (name == "xml") {
        return eSerial_Xml;
    } else {
        // Should be caught by argument processing, but as an illustration...
        THROW1_TRACE(runtime_error, "Bad serial format name " + name);
    }
}


vector<long> CReadBlastApp::getGIs(CBioseq::TAnnot::const_iterator& annot)
{
  vector<long> result;
  result.clear();
  ITERATE(CSeq_align::TScore, score, (*(*annot)->GetData().GetAlign().begin())->GetScore())
    {
    string name = (*score)->GetId().GetStr();
    if(name!="use_this_gi") continue;
    result.push_back((*score)->GetValue().GetInt());
    }

  return result;
}

bool CReadBlastApp::giMatch(const vector<long>& left, const vector<long>& right)
{

  bool result=false;
  if(PrintDetails()) NcbiCerr << "giMatch starts" << NcbiEndl;
  IncreaseVerbosity();
  ITERATE(vector<long>, gi1, left)
    {
    IncreaseVerbosity();
    ITERATE(vector<long>, gi2, right)
      {
      if(PrintDetails()) NcbiCerr << "try: gi1, gi2: " << *gi1 << " " << *gi2 << NcbiEndl;
      if (*gi1==*gi2) 
        {
        if(PrintDetails()) NcbiCerr << "giMatch!!! : gi1, gi2: " << *gi1 << " " << *gi2 << NcbiEndl;
        return true;
        }
      }
    DecreaseVerbosity();
    }
  DecreaseVerbosity();
  if(PrintDetails()) NcbiCerr << "giMatch oops" << NcbiEndl;

  return result;
}

void CReadBlastApp::dumpAlignment( const string& alignment, const string& file)
{
  ofstream out(file.c_str());
  out<< alignment;
}

void CReadBlastApp::dump_fasta_for_pretty_blast(diagMap& diag)
{
  // open out2 here
   string fn = GetArgs()["in"].AsString();
   string::size_type ipos=fn.rfind(".");
   if(ipos!=string::npos) fn.erase(ipos);
   ipos=fn.rfind("/");
   fn.erase(0,ipos+1);
   string fncdd = fn + "_cdd_html.fsa";
   fn+= "_html.fsa";

   CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
   CScope scope(*objmgr);

   auto_ptr<CNcbiOfstream> out2  (new CNcbiOfstream (fn.c_str()));
   CFastaOstream fasta_out(*out2);

   auto_ptr<CNcbiOfstream> out2_cdd ( new CNcbiOfstream (fncdd.c_str()));
   CFastaOstream fasta_out_cdd(*out2_cdd);

   for (CTypeConstIterator<CBioseq> seq = ConstBegin();  seq;  ++seq)
      {
      if( hasProblems(*seq, diag, eFrameShift )||
          hasProblems(*seq, diag, ePartial)
        ) 
        {
        CBioseq_Handle handle = scope.AddBioseq(*seq);
        if( hasProblems(*seq, diag, eFrameShift )) fasta_out.Write(handle);
        if( hasProblems(*seq, diag, ePartial)) fasta_out_cdd.Write(handle);
        }
      }
  
}

void CReadBlastApp::printOverlapReport
  (
  distanceReportStr *report,
  ostream& out
  )
{
  out << "q1 | lcl | " << report->q_id_left  << " | "   << report->q_name_left  << NcbiEndl;
  out << "q2 | lcl | " << report->q_id_right << " | "   << report->q_name_right << NcbiEndl;
  out      << report->q_id_left  
           << (report->left_strand  == eNa_strand_plus ? "(+)" : "(-)" )     
           << "\t"  
           << report->q_id_right 
           << (report->right_strand == eNa_strand_plus ? "(+)" : "(-)" )     
           << NcbiEndl;
  out
           << "[" <<  report->q_loc_left_from  << "..." << report->q_loc_left_to  << "] " << report->left_frame << "\t\t"
           << "[" <<  report->q_loc_right_from << "..." << report->q_loc_right_to << "] " << report->right_frame
           << NcbiEndl;
  out << NcbiEndl;

  out << "Overlap: " << report->space << " aa" << NcbiEndl;
}

void CReadBlastApp::printPerfectHit
  (
  const perfectHitStr& hit,
  ostream& out
  )
{

  out << "Perfect hit: " << hit.s_name << NcbiEndl;
  
}

void CReadBlastApp::printGeneralInfo(ostream& out)
{
  out << "General info extracted from the header of the submission:" << NcbiEndl;
  out << NcbiEndl; 
  out << "reldate: ";
  if ( IsSubmit() && m_Submit.GetSub().CanGetReldate() )
    {
    string date;
    m_Submit.GetSub().GetReldate().GetDate(&date);
    out << date.c_str();
    }
  else
    out << "unspecified";
  out << NcbiEndl; 


// simple texts 
  out << "tool: ";
  if ( IsSubmit() && m_Submit.GetSub().CanGetTool() )
    out << m_Submit.GetSub().GetTool();
  else
    out << "unspecified";
  out << NcbiEndl; 

  out << "user_tag: ";
  if ( IsSubmit() && m_Submit.GetSub().CanGetUser_tag() )
    out << m_Submit.GetSub().GetUser_tag();
  else
    out << "unspecified";
  out << NcbiEndl; 

  out << "comment: ";
  if ( IsSubmit() && m_Submit.GetSub().CanGetComment() )
    out << m_Submit.GetSub().GetComment();
  else
    out << "unspecified";
  out << NcbiEndl; 


  out << NcbiEndl; 
}

void CReadBlastApp::printReport
  (
  distanceReportStr *report,
  ostream& out
  )
{
  out << "q1 | lcl | " << report->q_id_left  << " | "   << report->q_name_left  << NcbiEndl;
  out << "q2 | lcl | " << report->q_id_right << " | "   << report->q_name_right << NcbiEndl;
  out << "s | "   << report->s_name       << NcbiEndl;
  out      << report->q_id_left
           << (report->left_strand  == eNa_strand_plus ? "(+)" : "(-)" )
           << "\t"
           << report->q_id_right
           << (report->right_strand == eNa_strand_plus ? "(+)" : "(-)" )
           << NcbiEndl;
  out 
           << "[" <<  report->q_loc_left_from  << "..." << report->q_loc_left_to  << "]\t\t"  
           << "[" <<  report->q_loc_right_from << "..." << report->q_loc_right_to << "]"  
           << NcbiEndl;
  out      << NcbiEndl;
  out 
           << "q\t" << report->q_left_left  << "\t" << report->q_left_middle  << "\t" << report->q_left_right
           << "\t"  << report->space
           << "\t"  << report->q_right_left << "\t" << report->q_right_middle << "\t" << report->q_right_right
           << NcbiEndl;
  out 
           << "s\t" << report->s_left_left  << "\t" << report->s_left_middle  << "\t" << "..."
           << "\t"  << "..."
           << "\t"  << "..."                << "\t" << report->s_left_right   << "\t" << "..."
           << NcbiEndl;
  out
           << "s\t" << "..."                << "\t" << report->s_right_left   << "\t" << "..."                  
           << "\t"  << "..."                     
           << "\t"  << "..."                << "\t" << report->s_right_middle << "\t" << report->s_right_right
           << NcbiEndl;
  out << "diff_left, diff_right: " << report->diff_left << ", " << report->diff_right << NcbiEndl;
  out << "diff_edge_left, diff_edge_right: " << report->diff_edge_left << ", " << report->diff_edge_right << NcbiEndl;
  if(report->diff_edge_left > 0)
    {
    out << "Potential deletetion of a nucleotide sequence equivalent to " << report->diff_edge_left << " occurred." << NcbiEndl;
    }
  else if (report->diff_edge_left < 0)
    {
    if ( -report->diff_edge_left < report->space )
      out << "Potential insertion of a nucleotide sequence equivalent to " << -report->diff_edge_left << " occurred." << NcbiEndl;
    else
      out << "Subjects are shifted apart by " << -report->diff_edge_left << NcbiEndl;
    }
  else
    {
    out << "Potential sequencing error or replacement mutation without insertion or deletion of a nucleotide sequence occurred." << NcbiEndl;
    }

  
}

void CReadBlastApp::GetRNAfeats
  (
  const LocMap& loc_map,
  CSeq_annot::C_Data::TFtable& rna_feats,
  const CSeq_annot::C_Data::TFtable& feats
  )
{
  if(PrintDetails()) NcbiCerr << "GetRNAfeats() starts"  << NcbiEndl;
  IncreaseVerbosity();
  ITERATE(CSeq_annot::C_Data::TFtable, f, feats) 
    {
    if(PrintDetails()) NcbiCerr << "GetRNAfeats(): feature start "  << NcbiEndl;
    if( !(*f)->GetData().IsRna() ) continue;
    if(PrintDetails()) NcbiCerr << "GetRNAfeats(): feature is RNA"  << NcbiEndl;
    string loc_string = GetLocationString( (*f)->GetLocation() );
    if(PrintDetails()) NcbiCerr << "GetRNAfeats(): ["
                                << loc_string  
                                << "] feature location"  << NcbiEndl;
    LocMap::const_iterator aname = loc_map.find(loc_string);
    if(aname == loc_map.end())
      {
      // error
      NcbiCerr << "CReadBlastApp::GetRNAfeats(): ERROR: cannot find gene for location " 
               << loc_string
               << NcbiEndl;
      continue;
      }
    rna_feats.push_back(aname->second);
    if(PrintDetails()) NcbiCerr << "GetRNAfeats(): ["
                                << aname->first
                                << "] feature end"  << NcbiEndl;
    }
  DecreaseVerbosity();
  if(PrintDetails()) NcbiCerr << "GetRNAfeats(): rna_feats size: "  
                              << rna_feats.size()
                              << NcbiEndl;
  if(PrintDetails()) NcbiCerr << "GetRNAfeats() ends"  << NcbiEndl;

  return;
}


string GetLocusTag(const CSeq_feat& f, const LocMap& loc_map)
{
  string loc_string = GetLocationString(f.GetLocation());
  LocMap::const_iterator aname = loc_map.find(loc_string);
  if(aname == loc_map.end())
    {
    return loc_string;
    }
  else
    {
    string qname;
    aname->second->GetData().GetGene().GetLabel(&qname); 
    return qname;
    }
}

CBioseq_set::TSeq_set* get_parent_seqset(const CBioseq& seq)
{
  string name = GetStringDescr(seq);
  CSeq_entry* parent = seq.GetParentEntry();
  if(parent==NULL) 
    {
    NcbiCerr << "get_parent_seqset: WARNING: " << name << ": no parent\n";
    return NULL;
    }
  if(parent->IsSeq())
    {
    while(parent!=NULL && !parent->IsSet())
       parent = parent->GetParentEntry();
    if(parent==NULL)
      {
      NcbiCerr << "get_parent_seqset: WARNING: " << name << ": no set ancestor\n";
      return NULL;
      }
    }
  else if(!parent->IsSet()) return NULL;

  if(!parent->GetSet().CanGetSeq_set())
    {
    NcbiCerr << "get_parent_seqset: WARNING: " << name << ": (grand)parent set does not have Seq_set\n";
    return NULL;
    }
  return &(parent->SetSet().SetSeq_set()); 
}


string let1_2_let3(char let1)
{
  switch(let1)
    {
    case 'A': return "Ala"; break;
    case 'Q': return "Gln"; break;
    case 'W': return "Trp"; break;
    case 'E': return "Glu"; break;
    case 'R': return "Arg"; break;
    case 'T': return "Thr"; break;
    case 'Y': return "Tyr"; break;
    case 'U': return "Sec"; break;
    case 'I': return "Ile"; break;
    case 'J': return "Xeu"; break; // Ile or Leu, there is no conventional three-letter code
    case 'P': return "Pro"; break;
    case 'S': return "Ser"; break;
    case 'D': return "Asp"; break;
    case 'F': return "Phe"; break;
    case 'G': return "Gly"; break;
    case 'H': return "His"; break;
    case 'K': return "Lys"; break;
    case 'L': return "Leu"; break;
    case 'Z': return "Glx"; break;
    case 'X': return "Ukn"; break;
    case 'C': return "Cys"; break;
    case 'V': return "Val"; break;
    case 'B': return "Asx"; break;
    case 'N': return "Asn"; break;
    case 'M': return "Met"; break;
    default: break;
    }
  NcbiCerr << "let1_2_let3: ERROR: char " << let1 << "(" << (int)let1 << ") is not handled" << NcbiEndl;

  return "";
}

int CReadBlastApp::collectPerfectHits(vector<perfectHitStr>& perfect, const CBioseq& seq)
{
  ITERATE(CBioseq::TAnnot, annot, seq.GetAnnot())
    {
    if(!(*annot)->GetData().IsAlign()) continue;
    check_alignment(annot, seq, perfect);
    }
  return perfect.size();
}

void CReadBlastApp::check_alignment
  (
  CBioseq::TAnnot::const_iterator& annot,
  const CBioseq& seq, 
  vector<perfectHitStr>& results
  )
{
  int sLen  = getLenScore(annot);
  int qLen  = getQueryLen(seq);
  string q_name  = GetProtName(seq);

  string s_name = getAnnotName(annot);

  int qFrom, qTo, sFrom, sTo;
  getBounds(annot,  &qFrom,  &qTo,  &sFrom,  &sTo);

  bool result=false;

  int qtails = qLen - qTo + qFrom - 1;
  int stails = sLen - sTo + sFrom - 1;
  double thr = m_small_tails_threshold; // default should be .1
  result = 
     (double)qtails/qLen < thr &&
     (double)stails/sLen < thr &&
     s_name.find("hypothetical") == string::npos;
// hypothetical exhonerating hits do not count

  if(result)
    {
    results.resize(results.size()+1);
    int ihit = results.size()-1;
    results[ihit].s_name = s_name;
    }
}

bool CReadBlastApp::is_prot_entry(const CBioseq& seq)
{
    bool result;
    if(PrintDetails()) NcbiCerr << "is_prot_entry?"  << NcbiEndl;
    result=seq.GetInst().IsAa();
    if(PrintDetails()) NcbiCerr << result  << NcbiEndl;
    return result;
}


int CReadBlastApp::SortSeqs(void)
{
   if(PrintDetails()) NcbiCerr << "SortSeqs: start " << NcbiEndl;
   PushVerbosity();
   if(IsSubmit())
     {
     if(PrintDetails()) NcbiCerr << "SortSeqs: IsSubmit" << NcbiEndl;
     if (
           m_Submit.GetData().IsEntrys()
            &&
           (*m_Submit.GetData().GetEntrys().begin())->IsSet()
        )
       {
       SortSeqs((*m_Submit.SetData().SetEntrys().begin())->SetSet().SetSeq_set());
       }
     else
       {
       NcbiCerr << "ERROR: submit file does not have proper seqset"<< NcbiCerr;
       }
     }
   else
     {
     if(PrintDetails()) NcbiCerr << "SortSeqs: IsEntry" << NcbiEndl;
       SortSeqs(m_Entry.SetSet().SetSeq_set());
     }

   PopVerbosity();
   if(PrintDetails()) NcbiCerr << "SortSeqs: end" << NcbiEndl;
   return -1;
}



int CReadBlastApp::SetParents(CSeq_entry* parent, CBioseq_set::TSeq_set& where)
{
/*
   NON_CONST_ITERATE( CBioseq_set::TSeq_set, left, where)
     {
     if((*left)->IsSet())
        {
        CBioseq_set::TSeq_set& seqs_down = (*left)->SetSet().SetSeq_set();
        SetParents(parent, seqs_down);
        } 
     else
       {
       (*left)->SetParentEntry(parent);
       }
     }
*/
    return 0;
}

// TSimpleSeq

bool CReadBlastApp::less_simple_seq(const TSimpleSeq& first,
                                    const TSimpleSeq& second)
{
   return first.key < second.key;
} // less_seq



bool CReadBlastApp::less_seq(const CRef<CSeq_entry>& first,
                             const CRef<CSeq_entry>& second)
{
  if(PrintDetails()) NcbiCerr << "less_seq start" << NcbiEndl;
  if (first->IsSet() || second->IsSet()) 
     { 
     return false;
     }
  if(PrintDetails()) NcbiCerr << "less_seq both seqs" << NcbiEndl;
  if (!hasGenomicLocation(first->GetSeq())) 
    {
    NcbiCerr << "less_seq first does not have genomic location or is nucleotide seq" << NcbiEndl;
    return true; // to take care of nucleotide sequence
    }
  if (!hasGenomicLocation(second->GetSeq())) 
    {
    NcbiCerr << "less_seq second does not have genomic location or is nucleotide seq" << NcbiEndl;
    return false;
    }
  if(PrintDetails()) NcbiCerr << "less_seq both have genomic locations" << NcbiEndl;
  const CSeq_loc& left_genomic_int = getGenomicLocation(first->GetSeq());
  const CSeq_loc& right_genomic_int = getGenomicLocation(second->GetSeq());
  TSeqPos from1, to1, from2, to2;

  ENa_strand  strand1;
  ENa_strand  strand2;

  getFromTo(left_genomic_int, from1, to1, strand1);
  getFromTo(right_genomic_int, from2, to2, strand2);
  if(PrintDetails()) NcbiCerr << "less_seq comparing" 
    << ": " << GetStringDescr( first->GetSeq()) << " = " << from1 << "-" << to1
    << ", " << GetStringDescr(second->GetSeq()) << " = " << from2 << "-" << to2
    << NcbiEndl;
  return from1 < from2;
} // less_seq

int CReadBlastApp::SortSeqs(CBioseq_set::TSeq_set& seqs)
{
   if(PrintDetails()) NcbiCerr << "SortSeqs(CBioseq_set::TSeq_set& seqs): start"  
        << ": " << seqs.size()
        << NcbiEndl;
  
   NON_CONST_ITERATE(CBioseq_set::TSeq_set, seq, seqs)
     {
     if ((*seq)->IsSet()) SortSeqs((*seq)->SetSet().SetSeq_set());
     } 
 
   seqs.sort(&less_seq); // does not do anything
   int n =1;

  if(PrintDetails()) NcbiCerr << "SortSeqs(CBioseq_set::TSeq_set& seqs): stop"  << NcbiEndl;
  return n;
}

ECoreDataType CReadBlastApp::getCoreDataType(istream& in)
{
  char buffer[1024];
  in.getline(buffer, 1024);
  in.seekg(0);

  string result =  buffer;
  result.erase(result.find_first_of(": "));

  if(result=="Seq-entry") 
    {
    result =  buffer;
    if(result.find("set") != string::npos)
      return eEntry;
    return eUndefined;
    }
  if(result=="Seq-submit") return eSubmit;
  if(NStr::StartsWith(result, ">Feature")) return eTbl;
  return eUndefined;
}

bool CReadBlastApp::IsSubmit()
{
  return m_coreDataType == eSubmit;
}

bool CReadBlastApp::IsEntry()
{
  return m_coreDataType == eEntry;
}

bool CReadBlastApp::IsTbl()
{
  return m_coreDataType == eTbl;
}


CBeginInfo CReadBlastApp::Begin(void)
{
  if(m_coreDataType == eSubmit) return ::Begin(m_Submit);
  if(m_coreDataType == eEntry)  return ::Begin(m_Entry);

  return ::Begin(m_Submit); // some platforms might have warnings without it
}

CConstBeginInfo CReadBlastApp::ConstBegin(void)
{
  if(m_coreDataType == eSubmit) return ::ConstBegin(m_Submit);
  if(m_coreDataType == eEntry)  return ::ConstBegin(m_Entry);

  return ::ConstBegin(m_Submit); // some platforms might have warnings without it
}


char *CReadBlastApp::next_w(char *w)
{
  while(*w && !isspace(*w)) ++w;
        while(isspace(*w)) ++w;
        return(w);

}
char *CReadBlastApp::skip_space(char *w)
{
        while(*w && isspace(*w)) ++w;
        return(w);
  
}
int CReadBlastApp::skip_toprot(CTypeIterator<CBioseq>& seq)
{
    int nums=0;
    for(nums=0 ;seq.IsValid(); ++seq, nums++) 
      {
      const CSeq_inst& inst = seq->GetInst();
      if(!inst.IsAa()) continue;
      break;
      }
    return nums;
}

int CReadBlastApp::skip_toprot(CTypeConstIterator<CBioseq>& seq)
{
    int nums=0;
    for(nums=0 ;seq; ++seq, nums++)
      {
      const CSeq_inst& inst = seq->GetInst();
      if(!inst.IsAa()) continue;
      break;
      }
    return nums;
}

bool CReadBlastApp::skip_toprot(CBioseq_set::TSeq_set::const_iterator& seq,
     const CBioseq_set::TSeq_set& seqs)
{

     IncreaseVerbosity();
     while( seq != seqs.end() &&
              (
              (*seq)->IsSet()                    ||        // set of sequences, skip
              !is_prot_entry((*seq)->GetSeq())             // not a protein, skip
              )
           )
        {
        if(PrintDetails()) NcbiCerr << "skip_to_valid_seq_cand: seq++"
                   // <<  CSeq_id::GetStringDescr ((*seq)->GetSeq(), CSeq_id::eFormat_FastA) << NcbiEndl;
                   <<  GetStringDescr ((*seq)->GetSeq()) << NcbiEndl;
        ++seq;
        }
     DecreaseVerbosity();

  return (seq != seqs.end()) ;

}


bool CReadBlastApp::skip_toprot(CBioseq_set::TSeq_set::iterator& seq,
     CBioseq_set::TSeq_set& seqs)
{

     IncreaseVerbosity();
     while( seq != seqs.end() &&
              (
              (*seq)->IsSet()                    ||        // set of sequences, skip
              !is_prot_entry((*seq)->GetSeq())             // not a protein, skip
              )
           )
        {
        if(PrintDetails()) NcbiCerr << "skip_to_valid_seq_cand: seq++"
                   <<  GetStringDescr ((*seq)->GetSeq()) << NcbiEndl;
        ++seq;
        }
     DecreaseVerbosity();

  return (seq != seqs.end()) ;

}


bool CReadBlastApp::skip_to_valid_seq_cand(
     CBioseq_set::TSeq_set::const_iterator& seq, 
     const CBioseq_set::TSeq_set& seqs)
{
     IncreaseVerbosity();
     while( seq != seqs.end() && 
              (
              (*seq)->IsSet()                    ||        // set of sequences, skip
              !is_prot_entry((*seq)->GetSeq())   ||        // not a protein, skip
              !has_blast_hits((*seq)->GetSeq())            // does not have a blast , skip
              )
           ) 
        {
        if(PrintDetails()) NcbiCerr << "skip_to_valid_seq_cand: seq++" 
                   <<  GetStringDescr ((*seq)->GetSeq())  << NcbiEndl;
        ++seq;
        }
     DecreaseVerbosity();

  return (seq != seqs.end()) ;
}

bool CReadBlastApp::skip_to_valid_seq_cand(
     CBioseq_set::TSeq_set::iterator& seq,
     CBioseq_set::TSeq_set& seqs)
{
     IncreaseVerbosity();
     while( seq != seqs.end() &&
              (
              (*seq)->IsSet()                    ||        // set of sequences, skip
              !is_prot_entry((*seq)->GetSeq())   ||        // not a protein, skip
              !has_blast_hits((*seq)->GetSeq())            // does not have a blast , skip
              )
           )
        {
        if(PrintDetails()) NcbiCerr << "skip_to_valid_seq_cand: seq++"
                   <<  GetStringDescr ((*seq)->GetSeq())  << NcbiEndl;
        ++seq;
        }
     DecreaseVerbosity();

  return (seq != seqs.end()) ;
}

// ReadPreviousAcc(args["parentacc"].AsString(), m_previous_genome)
bool CReadBlastApp::ReadPreviousAcc(const string& file, list<long>& input_acc)
{
   const int max_acc_len=0xF;
   ifstream in(file.c_str());
   if(!in.is_open()) return false; // the input parameter is not a file
   list<long> scratch_acc;
   while(in.good())
     { 
     char buffer[max_acc_len+2];
     in.getline(buffer, max_acc_len+1);
/*
     if(buffer[0] < 'A' || buffer[0] > 'Z' || 
        buffer[1] < 'A' || buffer[1] > 'Z' || 
        buffer[2] != '_') return false;
*/
     string test(buffer);
// remove white spaces
     string::size_type ipos=string::npos;
     while((ipos=test.find_first_of("\t\n\r ")) != string::npos) 
        test.erase(ipos,1);
// too long, may be it is a wrong file
     if(test.size()>max_acc_len) return false;
// has non-digits, go away
     if(test.find_first_not_of("0123456789") != string::npos) return false;
// accept
     scratch_acc.push_back(atol(test.c_str()));
     }
   if(!scratch_acc.size()) return false;
   input_acc = scratch_acc;
   return true;
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CReadBlastApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


