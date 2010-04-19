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
void CReadBlastApp::getFromTo(const CSeq_loc& loc, TSeqPos& from, TSeqPos& to, ENa_strand& strand)
{
  if(loc.IsInt())
    {
    getFromTo(loc.GetInt(), from, to, strand);
    }
  else if (loc.IsMix())
    {
    getFromTo(loc.GetMix(), from, to, strand);
    }
  else if (loc.IsPacked_int())
    {
    getFromTo(loc.GetPacked_int(), from, to, strand);
    }
  else if (loc.IsNull())
    {
    NcbiCerr << "INFO: getFromTo got NULL interval" << NcbiEndl;
    from = 0; to = 0;
    }
  else
    {
    NcbiCerr << "ERROR: getFromTo got unusual interval" << NcbiEndl;
    from = 0; to = 0;
    }
}


void CReadBlastApp::getFromTo(const CSeq_loc_mix& mix, TSeqPos& from, TSeqPos& to, ENa_strand& strand)
{
  from = 1<<30;
  to   = 0;
  ITERATE(CSeq_loc_mix::Tdata, loc, mix.Get())
    {
    TSeqPos from2, to2;
    getFromTo(**loc, from2, to2, strand);
    if(from==0 && to==0) continue;
    if(from2<from ) from = from2;
    if(to2>to) to = to2;
    }
}

void CReadBlastApp::getFromTo(const CPacked_seqint& mix, TSeqPos& from, TSeqPos& to, ENa_strand& strand)
{
  from = 1<<30;
  to   = 0;
  ITERATE(CPacked_seqint::Tdata, loc, mix.Get())
    {
    TSeqPos from2, to2;
    getFromTo(**loc, from2, to2, strand);
    if(from==0 && to==0) continue;
    if(from2<from ) from = from2;
    if(to2>to) to = to2;
    }
}

void CReadBlastApp::getFromTo(const CSeq_interval& inter, TSeqPos& from, TSeqPos& to, ENa_strand& strand)
{
  from = inter.GetFrom();
  to   = inter.GetTo();
  strand=eNa_strand_unknown;
  if(inter.CanGetStrand())
    {
    strand = inter.GetStrand();
    }
}

bool CReadBlastApp::hasGenomicLocation(const CBioseq& seq)
{
   if(PrintDetails()) NcbiCerr << "hasGenomicLocation starts" << NcbiEndl;
   ITERATE(CBioseq::TAnnot, annot, seq.GetAnnot())
     {
     if( !(*annot)->GetData().IsFtable()  ) continue;
     ITERATE(CSeq_annot::C_Data::TFtable, gen_feature, (*annot)->GetData().GetFtable()  )
       {
       if( !(*gen_feature)->CanGetComment()) continue;
       if( (*gen_feature)->GetComment().find("Genomic Location") == string::npos ) continue;
       return true;
       }
     }
   if(seq.IsAa())
     NcbiCerr << "hasGenomicLocation: ERROR : sequence "  
                               << GetStringDescr (seq)
                               << " does not have genomic location "
                               << NcbiEndl;
   return false;
}
const CSeq_loc&  CReadBlastApp::getGenomicLocation(const CBioseq& seq)
{
   if(PrintDetails()) NcbiCerr << "getGenomicLocation starts" << NcbiEndl;
   ITERATE(CBioseq::TAnnot, annot, seq.GetAnnot())
     {
     if( !(*annot)->GetData().IsFtable()  ) continue;
     ITERATE(CSeq_annot::C_Data::TFtable, gen_feature, (*annot)->GetData().GetFtable()  )
       {
       if( !(*gen_feature)->CanGetComment()) continue;
       if( (*gen_feature)->GetComment().find("Genomic Location") == string::npos ) continue;
       return (*gen_feature)->GetLocation();
       }
     }
   NcbiCerr << "getGenomicLocation: FATAL: sequence "
                               // << CSeq_id::GetStringDescr (seq, CSeq_id::eFormat_FastA)
                               << GetStringDescr (seq)
                               << " does not have genomic location"
                               << NcbiEndl;
   throw;
}

string CReadBlastApp::getLocusTag(const CBioseq& seq)
{
   string tag = "";
   string mark = "Genomic Location: ";
   ITERATE(CBioseq::TAnnot, annot, seq.GetAnnot())
     {
     if( !(*annot)->GetData().IsFtable()  ) continue;
     ITERATE(CSeq_annot::C_Data::TFtable, gen_feature, (*annot)->GetData().GetFtable()  )
       {
       if( !(*gen_feature)->CanGetComment()) continue;
       if( (*gen_feature)->GetComment().find(mark) == string::npos ) continue;
       tag = (*gen_feature)->GetComment();
       tag.erase(0, mark.length());
       return tag;
       }
     }
   return tag;
}

bool CReadBlastApp::hasGenomicInterval(const CBioseq& seq)
{
   if(PrintDetails()) NcbiCerr << "hasGenomicInterval starts" << NcbiEndl;
   ITERATE(CBioseq::TAnnot, annot, seq.GetAnnot())
     {
     if( !(*annot)->GetData().IsFtable()  ) continue;
     ITERATE(CSeq_annot::C_Data::TFtable, gen_feature, (*annot)->GetData().GetFtable()  )
       {
       if(! (*gen_feature)->GetLocation().GetInt().CanGetStrand() ) continue;
       if(! (*gen_feature)->GetLocation().GetInt().GetId().IsLocal() ) continue;
       return true;
       }
     }
   NcbiCerr << "hasGenomicInterval: ERROR: sequence "
                               // << CSeq_id::GetStringDescr (seq, CSeq_id::eFormat_FastA)
                               << GetStringDescr (seq)
                               << " does not have intervals with strands"
                               << NcbiEndl;
   return false;
}

const CSeq_interval& CReadBlastApp::getGenomicInterval(const CBioseq& seq)
{
   if(PrintDetails()) NcbiCerr << "getGenomicInterval starts" << NcbiEndl;
   ITERATE(CBioseq::TAnnot, annot, seq.GetAnnot())
     {
     if( !(*annot)->GetData().IsFtable()  ) continue;
     ITERATE(CSeq_annot::C_Data::TFtable, gen_feature, (*annot)->GetData().GetFtable()  )
       {
       if(! (*gen_feature)->GetLocation().GetInt().CanGetStrand() ) continue;
       if(! (*gen_feature)->GetLocation().GetInt().GetId().IsLocal() ) continue;
       return (*gen_feature)->GetLocation().GetInt();
       }
     }
   NcbiCerr << "getGenomicInterval: FATAL: sequence "
                               // << CSeq_id::GetStringDescr (seq, CSeq_id::eFormat_FastA)
                               << GetStringDescr (seq)
                               << " does not have intervals with strands"
                               << NcbiEndl;
   throw;
}

void CReadBlastApp::GetLocMap
  ( 
  LocMap& loc_map,  // location -> gene_feature for that location
  const CSeq_annot::C_Data::TFtable& feats
  )
{
  if(PrintDetails()) NcbiCerr << "GetLocMap starts" << NcbiEndl;
  loc_map.empty();
  IncreaseVerbosity();
  ITERATE(CSeq_annot::C_Data::TFtable, f, feats)
    {
    if( !(*f)->GetData().IsGene() ) continue;
// each location gets a gene name
    string n; (*f)->GetData().GetGene().GetLabel(&n);
    string loc_string = GetLocationString(**f);
    loc_map[loc_string]=*f;
    // if(PrintDetails()) NcbiCerr << "GetLocMap: stored location (" << loc_string << ")" << NcbiEndl; // this is too much
    }
  DecreaseVerbosity();
  if(PrintDetails()) NcbiCerr << "GetLocMap: locMap size: "
                              << loc_map.size()
                              << NcbiEndl;
  if(PrintDetails()) NcbiCerr << "GetLocMap ends" << NcbiEndl;
  return;
}

string GetLocationString(const CSeq_feat& f)
{
  return GetLocationString(f.GetLocation());
}
template <typename interval_type> string GetLocationString ( const interval_type& loc)
{
  TSeqPos from, to;
  ENa_strand strand;
  CReadBlastApp::getFromTo( loc, from, to, strand);
  strstream label;
  string strand_sign = strand == eNa_strand_plus ? "+" : "-";
  label << from << "-" << to << ":" << strand_sign << '\0';

  return label.str();


}

template string GetLocationString<CSeq_loc>(const CSeq_loc& loc);
