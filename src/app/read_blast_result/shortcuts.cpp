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
#include <objects/seqloc/Seq_point.hpp>

void CReadBlastApp::GetGenomeLen()
{
    for (CTypeIterator<CBioseq> seq = Begin();  seq;  ++seq)
      {
// check if na
      if(seq->GetInst().GetMol()!=CSeq_inst::eMol_dna) continue;
      if(PrintDetails()) NcbiCerr << "GetGenomeLen: found DNA" << NcbiEndl;
      m_length = seq->GetInst().GetLength(); // let the toolkit take care of exception
      } // end iteration over all genomic sequences
}

void CReadBlastApp::CheckUniqLocusTag()
{
  typedef map<string, int> THaveIt;
  THaveIt locuses; 
  for(CTypeIterator< CSeq_feat > f = Begin(); f; ++f)
    {
    // const CSeq_loc&  loc = f->GetLocation();
    if(f->GetData().IsGene())
      {
      if (f->GetData().GetGene().CanGetLocus_tag())
        {
        string locus_tag =  f->GetData().GetGene().GetLocus_tag();
        locuses[locus_tag]++;
        }
      }
    }

  bool bad=false;
  ITERATE(THaveIt, locus, locuses)
    {
    if(locus->second<2) continue;
    bad=true;
    NcbiCerr << "ERROR: CReadBlastApp::CheckUniqLocusTag : more than one gene with the same locus_tag: "
             << locus->first 
             << " = "
             << locus->second
             << NcbiEndl;
    }
  if(bad)
    {
    NcbiCerr << "FATAL: sequences or genes with the same locus_tag are not allowed" << NcbiEndl;
    throw;
    }
}

string CReadBlastApp::GetProtName(const CBioseq& seq)
{
  ITERATE(CBioseq::TAnnot, annot, seq.GetAnnot())
    {
    if( ! (*annot)->GetData().IsFtable() ) continue;
    ITERATE(CSeq_annot::TData::TFtable, feat, (*annot)->GetData().GetFtable())
      {
      if ( ! (*feat)->GetData().IsProt() ) continue;
      if ( ! (*feat)->GetData().GetProt().CanGetName() ) continue;
      return *( (*feat)->GetData().GetProt().GetName().begin() );
      }
    }
  return "default name";
}
int CReadBlastApp::getQueryLen(const CBioseq& seq)
{
  return seq.GetInst().GetLength();
}

string GetRRNAtype(const CRNA_ref& rna)
{
   string type =  "unknown rRNA";
   if(!rna.CanGetExt()) return type;
   if(!rna.GetExt().IsName()) return type;
   string name = rna.GetExt().GetName();
   if(name.find("5S") != string::npos) return "5S";
   if(name.find("16S") != string::npos) return "16S";
   if(name.find("23S") != string::npos) return "23S";
   if(name.find("SSU") != string::npos) return "16S";
   if(name.find("LSU") != string::npos) return "23S";
   return type;
}

string Get3type(const CRNA_ref& rna)
{
  string type;

  if(!rna.CanGetExt()) throw;
  if(!rna.GetExt().IsTRNA()) throw;
  if(!rna.GetExt().GetTRNA().CanGetAa()) throw;
  CTrna_ext::C_Aa::E_Choice choice = rna.GetExt().GetTRNA().GetAa().Which();

  char let1;
  switch(choice)
    {
    case CTrna_ext::C_Aa::e_Ncbieaa:
      let1 = rna.GetExt().GetTRNA().GetAa().GetNcbieaa();
      type = let1_2_let3(let1);
      break;
    case CTrna_ext::C_Aa::e_Iupacaa:
      let1 = rna.GetExt().GetTRNA().GetAa().GetIupacaa();
      type = let1_2_let3(let1);
      break;
    case CTrna_ext::C_Aa::e_Ncbi8aa:
      NcbiCerr << "Get3type(FATAL)::type e_Ncbi8aa(" << rna.GetExt().GetTRNA().GetAa().GetNcbi8aa() << ") is not handled now" << NcbiEndl;
      throw;
      break;
    case CTrna_ext::C_Aa::e_Ncbistdaa:
      NcbiCerr << "Get3type(FATAL)::type e_Ncbistdaa (" << rna.GetExt().GetTRNA().GetAa().GetNcbistdaa() << ") is not handled now" << NcbiEndl;
      throw;
      break;
    default:
      NcbiCerr << "Get3type: INTERNAL FATAL:: you should never be here" << NcbiEndl;
      break;
    }
  return type;
}

string get_title(const CBioseq& seq)
{
  string descr="";
  if(seq.CanGetDescr())
    {
    ITERATE(CSeq_descr::Tdata, desc, seq.GetDescr().Get())
      {
      if( (*desc)->IsTitle() )
        {
        descr=(*desc)->GetTitle(); break;
        }
      }
    }

 return descr;
}

EMyFeatureType get_my_seq_type(const CBioseq& seq)
{
  string name = GetStringDescr(seq);
  EMyFeatureType type = eMyFeatureType_unknown;

  string descr="";
  descr=get_title(seq);
  if( descr.find("hypothetical") != string::npos)
    {
    type = eMyFeatureType_hypo_CDS;
    }
  else
    {
    type = eMyFeatureType_normal_CDS;
    }
  if(CReadBlastApp::PrintDetails()) NcbiCerr << "get_my_seq_type: " << name << ", "
                              << "descr " << descr << ", "
                              << "type= " << type
                              << NcbiEndl;
  return type;
}
string get_trna_string(const CSeq_feat& feat)
{
  string trna_string="";
  bool isPseudo=false;
  char aa=0;

  if(!feat.GetData().IsRna()) return trna_string;
  if(!feat.GetData().GetRna().CanGetType()) return trna_string;
  CRNA_ref::EType rna_type = feat.GetData().GetRna().GetType();
  if(rna_type != CRNA_ref::eType_tRNA) return trna_string;
  if(feat.GetData().GetRna().CanGetPseudo())
     {
     isPseudo=feat.GetData().GetRna().GetPseudo();
     }
  if(feat.GetData().GetRna().CanGetExt() &&
     feat.GetData().GetRna().GetExt().IsTRNA() &&
     feat.GetData().GetRna().GetExt().GetTRNA().CanGetAa() &&
     feat.GetData().GetRna().GetExt().GetTRNA().GetAa().IsNcbieaa())
     {
     aa  = feat.GetData().GetRna().GetExt().GetTRNA().GetAa().GetNcbieaa();
     }
  trna_string = isPseudo ? "pseudo-tRNA-":"tRNA-";
  switch(aa)
    {
    case 0: trna_string+="unknown"; break;
    case 'X': trna_string+="other"; break;
    default: trna_string+=aa; break;
    }
  return trna_string;

}

string GetRNAname(const CSeq_feat& feat)
{
  string trna_string="";

  if(!feat.GetData().IsRna()) return trna_string;
  if(!feat.GetData().GetRna().CanGetType()) return trna_string;
  // CRNA_ref::EType rna_type = feat.GetData().GetRna().GetType();
//  if(rna_type != CRNA_ref::eType_rRNA) return trna_string;
  if(feat.GetData().GetRna().CanGetExt() &&
     feat.GetData().GetRna().GetExt().IsName())
     {
     trna_string = feat.GetData().GetRna().GetExt().GetName();
     }
  return trna_string;

}

EMyFeatureType get_my_feat_type(const CSeq_feat& feat, const LocMap& loc_map)
{
  EMyFeatureType feat_type = eMyFeatureType_unknown;
  string name = GetLocusTag(feat, loc_map);

//  need to get product name from sequence. Bummer.

  if(feat.GetData().IsRna())
    {
    if(feat.GetData().GetRna().CanGetType())
      {
      CRNA_ref::EType rna_type = feat.GetData().GetRna().GetType();
      if(CReadBlastApp::PrintDetails()) NcbiCerr << "get_my_feat_type: " << name
                              << ", rna_type= " << rna_type
                              << NcbiEndl;
      if(rna_type == CRNA_ref::eType_tRNA)
        {
        if(feat.GetData().GetRna().CanGetPseudo() && feat.GetData().GetRna().GetPseudo() == true)
          {
          feat_type = eMyFeatureType_pseudo_tRNA;
          }
        else if(feat.GetData().GetRna().CanGetExt() &&
                feat.GetData().GetRna().GetExt().IsTRNA() &&
                feat.GetData().GetRna().GetExt().GetTRNA().CanGetAa() &&
                feat.GetData().GetRna().GetExt().GetTRNA().GetAa().IsNcbieaa())
          {
          int aa = feat.GetData().GetRna().GetExt().GetTRNA().GetAa().GetNcbieaa();
          if( aa=='A' ||
              (aa>='C' && aa<='I') ||
              (aa>='K' && aa<='N') ||
              (aa>='P' && aa<='T') ||
              aa=='V' ||
              aa=='W' ||
              aa=='Y'
            )
            {
            feat_type = eMyFeatureType_normal_tRNA;
            }
          else
            {
            feat_type = eMyFeatureType_atypical_tRNA;
            }
          if(CReadBlastApp::PrintDetails()) NcbiCerr << "get_my_feat_type: " << name
                              << ", aa= " << aa
                              << ", type= " << feat_type
                              << NcbiEndl;
          }
        } // if(rna_type == CRNA_ref::eType_tRNA)
      else if (rna_type == CRNA_ref::eType_rRNA)
        {
        feat_type = eMyFeatureType_rRNA;
        }
      else if (rna_type == CRNA_ref::eType_miscRNA)
        {
        feat_type = eMyFeatureType_miscRNA;
        }
      }
    }
  if(CReadBlastApp::PrintDetails()) NcbiCerr << "get_my_feat_type: " << name
                              << ", type= " << feat_type
                              << NcbiEndl;
  return feat_type;
}
string GetStringDescr(const CBioseq& bioseq)
{

  string result = CSeq_id::GetStringDescr (bioseq, CSeq_id::eFormat_FastA);
  string locus_tag = CReadBlastApp::getLocusTag(bioseq);
// make sure locus_tag does not match result

  if(locus_tag != "" && result.find(locus_tag) == string::npos) result += "|" + locus_tag;
  return result;
}

string printed_range(const TSeqPos from2, const TSeqPos to2) // Mother of All Printed_Ranges
{
   strstream rstr;
   rstr << from2 << "..." << to2 << '\0';
   string r  = rstr.str();
   return r;
}

string printed_range(const CSeq_feat& feat)
{
   return printed_range(feat.GetLocation());
}

string printed_range(const CSeq_loc& seq_interval)
{
   TSeqPos from2, to2;
   from2 = seq_interval.GetStart(eExtreme_Positional);
   to2   = seq_interval.GetStop (eExtreme_Positional);
   return printed_range(from2, to2);
}

string printed_ranges(const CSeq_loc& seq_interval)
{
  typedef list<pair<int,int> > Tlist;
  Tlist ints;
  for(CTypeConstIterator<CSeq_interval> inter=::ConstBegin(seq_interval); inter; ++inter)
    {
    pair<int,int> apair(inter->GetFrom(), inter->GetTo());
    ints.push_back(apair); 
    }
  for(CTypeConstIterator<CSeq_point> pnt=::ConstBegin(seq_interval); pnt; ++pnt)
    {
    pair<int,int> apair(pnt->GetPoint(), pnt->GetPoint());
    ints.push_back(apair); 
    }
  ints.sort(CReadBlastApp::less_pair);

  string ranges="";
  NON_CONST_ITERATE(Tlist, interval, ints)
    {
    if(!ranges.empty()) ranges+=",";
    ranges+=printed_range(interval->first, interval->second);
    }

  return ranges;
}

string printed_range(const CBioseq& seq)
{
   return printed_range(CReadBlastApp::getGenomicLocation(seq));
}

string printed_range(const TSimpleSeqs::iterator& ext_rna)
{
  return printed_range(*ext_rna);
/*
   int from = ext_rna->exons[0].from;
   int to   = ext_rna->exons[ext_rna->exons.size()-1].to;
   strstream ext_rna_range_stream; ext_rna_range_stream << from << "..." << to << '\0';
   string ext_rna_range = ext_rna_range_stream.str();
   return ext_rna_range;
*/
}

string printed_range(const TSimplePair& apair)
{
   return printed_range(apair.from, apair.to);

}


string printed_range(const TSimpleSeq& ext_rna)
{
   int from,to;
   if(ext_rna.exons[0].strand == eNa_strand_plus)
     {
     from = ext_rna.exons[0].from;
     to   = ext_rna.exons[ext_rna.exons.size()-1].to;
     }
   else
    {
     to   = ext_rna.exons[0].to;
     from = ext_rna.exons[ext_rna.exons.size()-1].from;
     }

   return printed_range(from, to);
}

string printed_range(const TSimpleSeqs::iterator& ext_rna, const TSimpleSeqs::iterator& end)
{
   if(ext_rna==end) return "beyond end...beyond end";
   return printed_range(ext_rna);
}

string printed_range(const TSimpleSeqs::iterator& ext_rna, TSimpleSeqs& seqs)
{
   return printed_range(ext_rna, seqs.end());
}


int CReadBlastApp::getLenScore( CBioseq::TAnnot::const_iterator& annot)
{
  int len=-1;
  if(!(*annot)->GetData().IsAlign()) return len;

  ITERATE(CSeq_align::TScore, score, (*(*annot)->GetData().GetAlign().begin())->GetScore())
    {
    string name = (*score)->GetId().GetStr();
    if(name!="sbjLen") continue;
    len = (*score)->GetValue().GetInt();
    return len;
    }
  return len;
}
void CReadBlastApp::getBounds
  (
  CBioseq::TAnnot::const_iterator& annot,
  int* qFrom, int* qTo, int* sFrom, int* sTo
  )
{
  int i=0;
  ITERATE(CSeq_align::TBounds, bounds, (*(*annot)->GetData().GetAlign().begin())->GetBounds() )
    {
    if(!i)
      {
      *qFrom = (*bounds)->GetInt().GetFrom();
      *qTo   = (*bounds)->GetInt().GetTo();
      }
    else if (i==1)
      {
      *sFrom = (*bounds)->GetInt().GetFrom();
      *sTo   = (*bounds)->GetInt().GetTo();
      break;
      }
    i++;
    }

}

string CReadBlastApp::getAnnotName(CBioseq::TAnnot::const_iterator& annot)
{
 ITERATE(CAnnot_descr::Tdata, desc, (*annot)->GetDesc().Get())
    {
    if( (*desc)->IsName() )
      {
      return  (*desc)->GetName();
      }
    }
 return "cannot get name";
}

string CReadBlastApp::getAnnotComment(CBioseq::TAnnot::const_iterator& annot)
{
 ITERATE(CAnnot_descr::Tdata, desc, (*annot)->GetDesc().Get())
    {
    if( (*desc)->IsComment() )
      {
      return  (*desc)->GetComment();
      }
    }
 return "cannot get comment";
}

