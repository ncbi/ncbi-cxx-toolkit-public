/** ===========================================================================
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
int CReadBlastApp::CopyInfoFromGenesToProteins(void)
{
       

    // map<string, tranStr> tranStrMap;
    // map<string, CRef<CSeq_loc> > tranStrMap;
    TranStrMap3 tranStrMap;
    if(PrintDetails()) NcbiCerr << "CopyInfoFromGenesToProteins: start" << NcbiEndl;
    
    IncreaseVerbosity();
    
    for (CTypeIterator<CBioseq> seq = Begin();  seq;  ++seq)
      {
// check if na 
      if(seq->GetInst().GetMol()!=CSeq_inst::eMol_dna) continue;
      if(PrintDetails()) NcbiCerr << "CopyInfoFromGenesToProteins: found DNA" << NcbiEndl;
      if(!seq->IsSetAnnot()) continue;
      if(PrintDetails()) NcbiCerr << "CopyInfoFromGenesToProteins: with " << seq->GetAnnot().size() << " Annotations" << NcbiEndl;
      NON_CONST_ITERATE(CBioseq::TAnnot, annot, seq->SetAnnot())
        {
        processAnnot(annot, tranStrMap);
        } // iterate over sequence annotations
      } // end iteration over all genomic sequences
    DecreaseVerbosity();

    IncreaseVerbosity();

    for (CTypeIterator<CBioseq_set> seq = Begin();  seq;  ++seq)
      {
      if(PrintDetails()) NcbiCerr << "CopyInfoFromGenesToProteins: seqset" << NcbiEndl;
      if(!seq->IsSetAnnot()) continue;
      if(PrintDetails()) NcbiCerr << "CopyInfoFromGenesToProteins: with " 
          << seq->GetAnnot().size() << " Annotations" << NcbiEndl;
      NON_CONST_ITERATE(CBioseq::TAnnot, annot, seq->SetAnnot())
        {
        processAnnot(annot, tranStrMap);
        } // iterate over sequence annotations
      } // end iteration over all genomic sequences
    DecreaseVerbosity();



    if(PrintDetails()) NcbiCerr << "CopyInfoFromGenesToProteins: collected genomic features to the scratch map" << NcbiEndl;
    if(PrintDetails()) NcbiCerr << "CopyInfoFromGenesToProteins: now going over the protein sequences" << NcbiEndl;
    IncreaseVerbosity();

// go over protein sequences and add info from gene locations and strands
    for (CTypeIterator<CBioseq> seq = Begin();  seq;  ++seq)
      {
// check if prot
      if(seq->GetInst().GetMol()!=CSeq_inst::eMol_aa) continue;
      if(PrintDetails()) NcbiCerr << "CopyInfoFromGenesToProteins: found protein sequence" << NcbiEndl;
// assume we need only one id
      string prot_id; /////////////////
      bool find_tags=false;
      ITERATE (CBioseq::TId, iter, seq->GetId())
        {
        CSeq_id::EAccessionInfo type = (*iter)->IdentifyAccession();
        prot_id = (*iter)->AsFastaString();
        if(tranStrMap.find(prot_id) != tranStrMap.end() &&
              tranStrMap[prot_id].find(type) != tranStrMap[prot_id].end())
           {
           find_tags=true;
              try { addLocation(prot_id, *seq, tranStrMap[prot_id][type].seqloc ,
                                tranStrMap[prot_id][type].locus_tag); }
              catch(...)  {find_tags=false; }
           if(find_tags) break;
           }
        }
      if(!find_tags)
            {
            prot_id =GetStringDescr (*seq);
            NcbiCerr << "FATAL: sequence "
                     << prot_id
                     << " no location found"
                     << NcbiEndl;
            throw;
            }
      }
    DecreaseVerbosity();
    if(PrintDetails()) NcbiCerr << "CopyInfoFromGenesToProteins: end" << NcbiEndl;
    CollectSimpleSeqs(m_simple_seqs);
/*
// quick hash access    
    TSimpleSeqHashMap simple_seqs_hash_map;
    ITERATE(TSimpleSeqs, seq, m_simple_seqs)
      {
      int key = seq->key;
      simple_seqs_hash_map[key].push_back(*seq);
      }
    addLocusTags(simple_seqs_hash_map);
*/

    return -1;
}
/*
void CReadBlastApp::addLocusTags(TSimpleSeqHashMap& simple_seqs_hash_map)
{
   
}
*/

void CReadBlastApp::addLocation(string& prot_id, CBioseq& seq, CRef<CSeq_loc>& loc, const string& locus_tag)
{
// check if annotation exists
   if(!seq.IsSetAnnot()) return;
   if(PrintDetails()) NcbiCerr << "addLocation: with annotation " << prot_id << " to seq " << GetStringDescr (seq) << NcbiEndl;
// assuming we need only one annotation
   if(!(*seq.GetAnnot().begin())->GetData().IsFtable())
// if it is not a feature table
     {
     // oh-uh.
     if(PrintDetails()) NcbiCerr << "addLocation: without table,oops" << NcbiEndl;
     return;
     }
   if(PrintDetails()) NcbiCerr << "addLocation: with table" << NcbiEndl;
   CSeq_annot::C_Data::TFtable& table = (*seq.SetAnnot().begin())->SetData().SetFtable();
// assuming we have only one feature
// add feature containing transfer info
   CRef< CSeq_feat > feat (new CSeq_feat);
   table.push_back(feat);
   feat->SetData(); // let us hope it will work just like that
   feat->SetData().SetProt();
   feat->SetLocation(*loc);
   string comment = "Genomic Location: ";
   feat->SetComment(comment);

   if(PrintDetails()) NcbiCerr << "addLocation: added genomic annotation to the end of protein feature table" << NcbiEndl;
   return;

}

void CReadBlastApp::processFeature
  (
  CSeq_annot::C_Data::TFtable::iterator& feat,
  TranStrMap3& tranStrMap
  )
{
  bool cdregion;
  cdregion = (*feat)->GetData().IsCdregion();
  if(cdregion)
    {
    if(!(*feat)->IsSetProduct()) return;
    if(!(*feat)->GetProduct().IsWhole()) return;
    CSeq_loc& loc = (*feat)->SetLocation();
    const CSeq_id& seqid = (*feat)->GetProduct().GetWhole();
    CSeq_id::EAccessionInfo type = seqid.IdentifyAccession();
    string key = seqid.AsFastaString();
    tranStrMap[key][type].seqloc=&loc;
    TSeqPos from, to2;
    ENa_strand strand2;
    getFromTo(loc, from, to2, strand2);
    tranStrMap[key][type].sort_key=from;
    }
}

template <typename t>
void CReadBlastApp::processAnnot(CBioseq::TAnnot::iterator& annot, t& tranStrMap)
{
        if(!(*annot)->GetData().IsFtable())
// if it is not a feature table
          {
          if(PrintDetails()) NcbiCerr << "processAnnot: annotation has no features" << NcbiEndl;
          return  ;
          }
        if(PrintDetails()) NcbiCerr << "processAnnot: with features" << NcbiEndl;
        CSeq_annot::C_Data::TFtable& table = (*annot)->SetData().SetFtable();
        IncreaseVerbosity();
        if(PrintDetails()) NcbiCerr << "processAnnot: start cycle of features" << NcbiEndl;
        NON_CONST_ITERATE(CSeq_annot::C_Data::TFtable, feat, table)
          {
          processFeature(feat, tranStrMap);
          } // end iteration over all features
        DecreaseVerbosity();
}

/*
 * $Log: copy_loc.cpp,v $
 * Revision 1.2  2007/11/08 15:49:04  badrazat
 * 1. added locus tags
 * 2. fixed bugs in detecting RNA problems (simple seq related)
 *
 * Revision 1.1  2007/10/04 15:46:18  badrazat
 * another split from _lib.cpp
 *
 *
*/


