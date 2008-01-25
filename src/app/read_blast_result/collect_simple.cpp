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
*   Collects sequence information in NCBI Toolkit format and stores it in a 
*   simple sequences format in memory
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include "read_blast_result.hpp"

static int gene_feat_fit(TSimpleSeqs::iterator& seq, int from, int to);

int CReadBlastApp::CollectSimpleSeqs(TSimpleSeqs& seqs)
{
// collect stuff from proteins
  for(CTypeIterator< CSeq_entry > s = Begin(); s; ++s)
    {
    if(s->IsSet()) continue;
    if(!is_prot_entry(s->GetSeq())) continue;
    TSimpleSeq seq;
    seq.description = GetProtName(s->GetSeq());
    seq.name = GetStringDescr (s->GetSeq());
    seq.type = "CDS";
    seq.seq = CRef<CBioseq>(&(s->SetSeq()));
    const CSeq_loc&  loc = getGenomicLocation(s->GetSeq());
    addLoctoSimpleSeq(seq, loc);
    seqs.push_back(seq);
    }
// collect features from RNAs and genes
  string name;
  TSimpleSeqs genes;
  for(CTypeIterator< CSeq_feat > f = Begin(); f; ++f)
    {
    const CSeq_loc&  loc = f->GetLocation();
    if(f->GetData().IsGene())
      {
      name = "Bad or no locus tag";
      if (f->GetData().GetGene().CanGetLocus_tag())
        name =  f->GetData().GetGene().GetLocus_tag();
// I am assuming that each RNA feature is preceded by a gene
      TSimpleSeq gene; 
      gene.type = "gene";
      gene.locus_tag = name;
      addLoctoSimpleSeq(gene, loc);
      genes.push_back(gene);
      continue;
      }
    else if(!f->GetData().IsRna()) continue;
    CRNA_ref::EType rna_type = f->GetData().GetRna().GetType();
    string description="Bad or no descriptioin";
    if ( rna_type == CRNA_ref::eType_tRNA )
      {
      if ( f->GetData().GetRna().CanGetExt() )
        {
        string type1;
        try { type1 = Get3type(f->GetData().GetRna());}
        catch  (...)
          {
          NcbiCerr << "simple_overlaps: FATAL: cannot get aminoacid type for one trna feats" << NcbiEndl;
          throw;
          }
        description = "tRNA:" + type1;
        }
      } // if tRNA
    else
      {
      if(f->GetData().GetRna().CanGetExt() &&
         f->GetData().GetRna().GetExt().IsName())
         description = f->GetData().GetRna().GetExt().GetName();
      }
    TSimpleSeq seq;
    if      ( rna_type == CRNA_ref::eType_tRNA ) { seq.type = "tRNA"; }
    else if ( rna_type == CRNA_ref::eType_rRNA ) { seq.type = GetRRNAtype(f->GetData().GetRna());}
    else if ( rna_type == CRNA_ref::eType_premsg ) { seq.type = "premsg"; }
    else if ( rna_type == CRNA_ref::eType_mRNA ) { seq.type = "mRNA"; }
    else if ( rna_type == CRNA_ref::eType_snRNA ) { seq.type = "snRNA"; }
    else if ( rna_type == CRNA_ref::eType_scRNA ) { seq.type = "scRNA"; }
    else if ( rna_type == CRNA_ref::eType_snoRNA ) { seq.type = "snoRNA"; }
    else if ( rna_type == CRNA_ref::eType_other ) { seq.type = "other RNA"; }
    else { seq.type = "unknown RNA"; }
    seq.name = name;
    seq.description = description;
    addLoctoSimpleSeq(seq, loc);
    seqs.push_back(seq);
    } // features

// need to tidy up  before doing what is next
  seqs.sort(less_simple_seq);
  genes.sort(less_simple_seq);

// now go over all gene features and match them to seqs features;
// first of all do all exact locations
  TSimpleSeqs::iterator seq = seqs.begin();
  for(TSimpleSeqs::iterator gene = genes.begin(); gene!=genes.end(); )
    {
    int seq_from=0;
    int gene_from = gene->exons[0].from;
    for(;seq!=seqs.end(); seq++)
       {
       seq_from = seq->exons[0].from;
       if(gene_from<=seq_from) break;
       }
    if(seq==seqs.end()) break;
    int seq_to  = seq->exons[seq->exons.size()-1].to;
    int gene_to = gene->exons[gene->exons.size()-1].to;
    if(seq_to==gene_to && seq_from==gene_from) 
      { 
      seq->locus_tag = gene->locus_tag; 
      genes.erase(gene++); 
      }
    else gene++;
    }

// now try to assign non-exact gene-CDS matches
  seq=seqs.begin();
  for(TSimpleSeqs::iterator gene = genes.begin(); gene!=genes.end(); )
    {
    int gene_from = gene->exons[0].from;
// find first sequence that could match a gene
    TSimpleSeqs::iterator seq_start=seq;
    for(;seq_start!=seqs.end(); seq_start++)
       {
       if(seq->locus_tag != "") continue; // this is done
       if(gene_from<=seq_start->exons[0].from) break;
       }
    if(seq_start==seqs.end()) break; // done with seqs
// now check if other ends fit
    int seq_to = seq_start->exons[seq_start->exons.size()-1].to;
    int gene_to = gene->exons[gene->exons.size()-1].to;
    if (seq_to > gene_to) 
      {
// sequences jumped over this gene, this gene does not fit any sequence, will be flagged later
      gene++;
      continue;
      }
// end find first sequence that could match a gene
// find first sequence that does not match a gene
    TSimpleSeqs::iterator seq_end = seq_start;
    int nmatches=0;
    for(;seq_end!=seqs.end() &&
         gene_to >= seq_end->exons[seq_end->exons.size()-1].to;
        seq_end++)
       {
       if(seq->type == "CDS" && seq->locus_tag == "" ) nmatches++;
       }
    if(seq_end!=seqs.end() ) seq_end++;
// end find first sequence that does not match a gene
    if(nmatches>1)
      {
      string range = printed_range(gene);
      NcbiCerr << "CReadBlastApp::CollectSimpleSeqs: WARNING: gene matches several (" << nmatches << ") CDS features: "
         << "locus = " << gene->locus_tag << ", "
         << "[" << range << "]" << NcbiEndl;
      }

// look at all found fits
    bool gene_used=false;
// find best fit and assign locus tag only for that feature
    TSimpleSeqs::iterator best_seq=seqs.end();
    int best_gene_feat_fit = 0x0FFFFFFF; // intentionally less than the const in gene_feat_fit function
    for(seq=seq_start; seq!=seq_end; seq++)
      {
      if(seq->locus_tag != "") continue; // this is done already
      if(seq->type != "CDS" )
        {
        string range = printed_range(seq);
        NcbiCerr << "CReadBlastApp::CollectSimpleSeqs: ERROR: non-CDS sequence does not have a gene with exactly the same boundaries: "
         << "type = " << seq->type << ", "
         << "name = " << seq->name << ", "
         << "[" << range << "]" << NcbiEndl;
        }
      else 
        {
        int fit=gene_feat_fit(seq, gene_from, gene_to);
        if(fit <= best_gene_feat_fit )
          {
          best_seq=seq; best_gene_feat_fit = fit; 
          }
        }
      } // for(seq=seq_start; seq!=seq_end; seq++)
// found suitable seqs
   if(best_seq!=seqs.end())
      {
      best_seq->locus_tag = gene->locus_tag;
      gene_used = true;
      }
// go to next gene
    if(gene_used) genes.erase(gene++);
    else gene++;
    }

// swipe over seqs flag those that do not have locus tag
  NON_CONST_ITERATE(TSimpleSeqs,seq, seqs)
    {
    if(seq->locus_tag != "") 
      {
      if(seq->type == "CDS")
         {
         for(CTypeIterator<CSeq_feat> feat=::Begin(*(seq->seq)); feat; ++feat)
           {
           if(feat->CanGetComment() && feat->GetComment().find("Genomic Location: ") != string::npos)
             {
             string comment = "Genomic Location: " + seq->locus_tag;
             feat->SetComment(comment);
             }
           }
         }
      continue;
      }
    string range = printed_range(seq);
    NcbiCerr << "CReadBlastApp::CollectSimpleSeqs: ERROR: feature does not have a matching gene: "
      << "type = " << seq->type << ", "
      << "name = " << seq->name << ", "
      << "[" << range << "]" << NcbiEndl;
    }
// swipe over genes and flag those that are not used 
  NON_CONST_ITERATE(TSimpleSeqs,gene, genes)
    {
    string range = printed_range(gene);
    NcbiCerr << "CReadBlastApp::CollectSimpleSeqs: WARNING: gene does not match any feature: "
         << "locus = " << gene->locus_tag << ", "
         << "[" << range << "]" << NcbiEndl;
    }
   
// simple seqs collected

  return 1;

}

int gene_feat_fit(TSimpleSeqs::iterator& seq, int from, int to)
{
  int r=0xFFFFFFFF;
  int from2 = seq->exons[0].from;
  int to2   = seq->exons[seq->exons.size()-1].to;
  if(from2<from) return r; // no fit at all
  if(to2>to    ) return r; // no fit at all

  return from2-from+to-to2;  
}

void CReadBlastApp::addLoctoSimpleSeq(TSimpleSeq& seq, const CSeq_loc&  loc)
{
    seq.exons.clear();
    seq.key=0xFFFFFFFF;
    for(CTypeConstIterator<CSeq_interval> inter = ::Begin(loc); inter; ++inter)
       {
       TSeqPos  from, to;
       ENa_strand strand;
       getFromTo(*inter, from, to, strand);
       TSimplePair exon; exon.from=from; exon.to=to; exon.strand=strand;
       if(seq.key>(int)from) seq.key = (int)from;
       seq.exons.push_back(exon);
       }
}


