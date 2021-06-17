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
    if(PrintDetails())
      {
      NcbiCerr << "DEBUG: CollectSimpleSeqs(): added loc to CDS: " 
               << "(" << seq.name  << ")"
               << "(" << printed_range(loc) << ")"
               << "(" << seq.key << ":" << printed_range(seq) << ")"
               <<  NcbiEndl;
      }
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
      if(PrintDetails())
      {
      NcbiCerr << "DEBUG: CollectSimpleSeqs(): added loc to gene: " 
               << "(" << name << ")"
               << "(" << printed_range(loc) << ")"
               << "(" << gene.key << ":" << printed_range(gene) << ")"
               <<  NcbiEndl;
      }
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
    string gene_range = printed_range(gene);
    int seq_from=0, seq_to=0;
    int gene_from = gene->exons[0].from;
    int gene_to   = gene->exons[0].to;
    for(;seq!=seqs.end(); seq++)
       {
       string seq_range = printed_range(seq);
       seq_from = seq->exons[0].from;
       seq_to   = seq->exons[0].to;
       if(PrintDetails()) 
         {
         NcbiCerr << "DEBUG: CollectSimpleSeqs(): sliding seq " << seq_range << "(key: " << seq->key << ") to reach gene " << gene_range << "(key: " << gene->key << "), locus=" << gene->locus_tag << NcbiEndl;
         }
       if(gene->key<=seq->key) break; 
       }
    if(seq==seqs.end()) break; 

      seq_from = seq->exons[0].from;
      seq_to   = seq->exons[0].to;
      string seq_range = printed_range(seq);
      if(PrintDetails()) 
        {
        NcbiCerr << "DEBUG: CollectSimpleSeqs(): sliding seq " << seq_range << "(key: " << seq->key << ") reached gene " << gene_range << "(key: " << gene->key << "), locus=" << gene->locus_tag << NcbiEndl;
        }
      seq_to  = seq->exons[seq->exons.size()-1].to;
      if(seq->exons[0].strand != eNa_strand_plus) // JIRA-PR-147
        {
        seq_to   = seq->exons[0].to;
        seq_from= seq->exons[seq->exons.size()-1].from;
        }
      gene_to = gene->exons[gene->exons.size()-1].to;
      if(seq_to==gene_to && seq_from==gene_from)  // match
        { 
        seq->locus_tag = gene->locus_tag; 
        gene=genes.erase(gene++); 
        }
      else gene++;
    
    }
/////////////////////////////
// now try to assign non-exact gene-CDS matches
/////////////////////////////
  seq=seqs.begin();
  for(TSimpleSeqs::iterator gene = genes.begin(); gene!=genes.end(); )
    {
    string gene_printed_range = printed_range(gene);
    if(PrintDetails()) NcbiCerr << "non-exact gene-CDS matches: gene: " << gene_printed_range << NcbiEndl;
    int gene_from = gene->exons[0].from;
// find first sequence that could match a gene
    TSimpleSeqs::iterator seq_start=seq;
    for(;seq_start!=seqs.end(); seq_start++)
       {
       if(PrintDetails()) NcbiCerr << "non-exact gene-CDS matches["<<gene_printed_range<<"]: trying seq_start: " 
                                   << printed_range(seq_start) << NcbiEndl;
       if(seq_start->locus_tag != "") 
         {
         if(PrintDetails()) NcbiCerr << "non-exact gene-CDS matches["<<gene_printed_range<<"]: " 
                                     << seq->locus_tag << ", continue..."<<NcbiEndl;
         continue; // this is done
         }

       int seq_from = seq_start->exons[0].strand == eNa_strand_plus ? seq_start->exons[0].from : seq_start->exons[seq_start->exons.size()-1].from;
       if(gene_from<=seq_from) break; // in case there are cross-origin seqs, they will be in the end of seqs list, so they will be tested the last, thus this incorrect sliding should be fine
       }
    if(seq_start==seqs.end()) break; // done with seqs
// now check if other ends fit
    if(PrintDetails()) NcbiCerr << "non-exact gene-CDS matches["<<gene_printed_range<<"]: found seq_start: " << printed_range(seq_start) << NcbiEndl;
    int seq_to =  seq_start->exons[0].strand == eNa_strand_plus 
      ? seq_start->exons[seq_start->exons.size()-1].to
      : seq_start->exons[0].to;
    int gene_to = gene->exons[gene->exons.size()-1].to;
    if ( gene->exons[0].strand != eNa_strand_plus ) gene_to = gene->exons[0].to;
    if (seq_to > gene_to) 
      {
      if(PrintDetails()) NcbiCerr << "non-exact gene-CDS matches["<<gene_printed_range<<"]: sequences jumped over this gene, this gene does not fit any sequence, will be flagged later"  << NcbiEndl;
// sequences jumped over this gene, this gene does not fit any sequence, will be flagged later
      gene++;
      continue;
      }
// end find first sequence that could match a gene
// find first sequence that does not match a gene
    TSimpleSeqs::iterator seq_end = seq_start;
    int nmatches=0;
    for(;seq_end!=seqs.end() &&
         gene_to >= (seq_end->exons[0].strand == eNa_strand_plus 
           ? seq_end->exons[seq_end->exons.size()-1].to
           : seq_end->exons[0].to);
        seq_end++)
       {
       if(PrintDetails()) NcbiCerr << "non-exact gene-CDS matches["<<gene_printed_range<<"]: trying to find: current_seq_end " 
                                   << printed_range(seq_end)  
                                   << ", gene_to = " << gene_to
                                   << ", seq_end.to = " << (seq_end->exons[0].strand == eNa_strand_plus
                                                           ? seq_end->exons[seq_end->exons.size()-1].to
                                                           : seq_end->exons[0].to)
                                   << NcbiEndl;

       if(seq_end->type == "CDS" && seq_end->locus_tag == "" ) nmatches++;
       }
    if(seq_end!=seqs.end() ) seq_end++;
    if(PrintDetails()) 
      {
      if(seq_end!=seqs.end() ) 
        NcbiCerr << "non-exact gene-CDS matches["<<gene_printed_range<<"]: found seq_end: " << printed_range(seq_start) << NcbiEndl;
      else
        NcbiCerr << "non-exact gene-CDS matches["<<gene_printed_range<<"]: found seq_end: end()"  << NcbiEndl;
      }
// end find first sequence that does not match a gene
    if(PrintDetails()) 
       {
       if(seq_end!=seqs.end() )
         NcbiCerr << "non-exact gene-CDS matches(" << nmatches << "): seq_end: " << printed_range(seq_end) << NcbiEndl;
       else
         NcbiCerr << "non-exact gene-CDS matches(" << nmatches << "): seq_end: end()" << NcbiEndl;
       }
    if(nmatches>1)
      {
      string range = printed_range(gene);
      NcbiCerr << "CReadBlastApp::CollectSimpleSeqs: WARNING: gene["<<gene_printed_range<<"] matches several (" << nmatches << ") CDS features: "
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
      if(PrintDetails()) NcbiCerr << "non-exact gene-CDS matches["<<gene_printed_range<<"]: match: " << printed_range(seq)  << NcbiEndl;
      if(seq->locus_tag != "") continue; // this is done already
      if(PrintDetails()) NcbiCerr << "non-exact gene-CDS matches["<<gene_printed_range<<"]: match: " << printed_range(seq)  
         << " does not have a locus tag yet"
         << NcbiEndl;
/*
      if(seq->type != "CDS" )
        {
        string range = printed_range(seq);
        NcbiCerr << "CReadBlastApp::CollectSimpleSeqs: ERROR: non-CDS sequence does not have a gene with exactly the same boundaries: "
         << "type = " << seq->type << ", "
         << "name = " << seq->name << ", "
         << "[" << range << "]" << NcbiEndl;
        }
      else 
*/
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
    if(gene_used) gene=genes.erase(gene);
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
  if(seq->exons[0].strand != eNa_strand_plus)
    {  
    from2 = seq->exons[seq->exons.size()-1].from;
    to2   = seq->exons[0].to;
    }
// feature seq should be within gene
  if(from2<from) return r; // no fit at all
  if(to2>to    ) return r; // no fit at all

  return from2-from+to-to2;  
}

void CReadBlastApp::addLoctoSimpleSeq(TSimpleSeq& seq, const CSeq_loc&  loc)
{
    seq.exons.clear();
    seq.key=kMax_Int  ;
    for(CTypeConstIterator<CSeq_interval> inter = ncbi::ConstBegin(loc);
        inter; ++inter)
       {
       TSeqPos  from, to;
       ENa_strand strand;
       getFromTo(*inter, from, to, strand);
       TSimplePair exon; exon.from=from; exon.to=to; exon.strand=strand;
       exon.fuzzy_from = inter->IsPartialStart(eExtreme_Positional);
       exon.fuzzy_to   = inter->IsPartialStop (eExtreme_Positional);
       if(seq.key>(int)from) 
         {
         seq.key = (int)from;
         }
       if(PrintDetails())
         {
         NcbiCerr << "addLoctoSimpleSeq(): exon ("<< printed_range(exon) << ")" << NcbiEndl;
         }
       seq.exons.push_back(exon);
       }
   TSeqPos  from, to;
   ENa_strand strand;
   getFromTo(loc, from, to, strand);
   if((int)seq.exons.size()>1 && 
      (int)to-(int)from > (int)m_length/2) 
// over the origin annotation
     {
     int i=0;
     for(CTypeConstIterator<CSeq_interval> inter = ncbi::ConstBegin(loc);
        inter; ++inter, ++i)
       {
       TSeqPos  from, to;
       ENa_strand strand;
       getFromTo(*inter, from, to, strand);
       if(i==0) seq.key = (int)from; // initialize
       else
         {
         if((int)from-seq.key > m_length/2) seq.key = (int)from; // large gap, make it from here
         } 
       }

     }
}


