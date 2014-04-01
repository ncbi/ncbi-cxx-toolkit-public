/*
 * $Id$
 *
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *            National Center for Biotechnology Information (NCBI)
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government do not place any restriction on its use or reproduction.
 *  We would, however, appreciate having the NCBI and the author cited in
 *  any work or product based on this material
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 * ===========================================================================
 *
 * Author: Jie Chen
 *
 * File Description:
 *   Checking data objects 
 *
 */

#include <ncbi_pch.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include <objtools/discrepancy_report/hchecking_class.hpp>
#include <objtools/discrepancy_report/hDiscRep_config.hpp>
#include <objtools/discrepancy_report/hDiscRep_tests.hpp>
#include <objtools/discrepancy_report/hUtilib.hpp>

BEGIN_NCBI_SCOPE

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);

static string strtmp;
static CDiscRepInfo  thisInfo;
static CDiscTestInfo thisTest;
static CTestGrp      thisGrp;

void CCheckingClass :: CheckSeqInstMol (CSeq_inst& seq_inst,
                                                        CBioseq& bioseq)
{

} // CheckSeqInstMol

CCheckingClass :: CCheckingClass()
{
   m_num_entry = 0;
/*
   // subtypes cover type: RBS, exon and intron for IMP;
   sel_seqfeat.IncludeFeatType(CSeqFeatData::e_Gene)
              .IncludeFeatType(CSeqFeatData::e_Cdregion)
              .IncludeFeatType(CSeqFeatData::e_Rna)
              .IncludeFeatSubtype(CSeqFeatData::eSubtype_RBS)
              .IncludeFeatSubtype(CSeqFeatData::eSubtype_exon)
              .IncludeFeatSubtype(CSeqFeatData::eSubtype_intron)
              .IncludeFeatType(CSeqFeatData::e_Prot);
*/

   sel_seqfeat_4_seq_entry.IncludeFeatType(CSeqFeatData::e_Pub)
                          .IncludeFeatType(CSeqFeatData::e_Biosrc);

   sel_seqdesc.reserve(6);
   sel_seqdesc.push_back(CSeqdesc::e_Pub);
   sel_seqdesc.push_back(CSeqdesc::e_Comment);
   sel_seqdesc.push_back(CSeqdesc::e_Source);
   sel_seqdesc.push_back(CSeqdesc::e_Title);
   sel_seqdesc.push_back(CSeqdesc::e_User);
   sel_seqdesc.push_back(CSeqdesc::e_Org);

   sel_seqdesc_4_bioseq.reserve(5);
   sel_seqdesc_4_bioseq.push_back(CSeqdesc::e_Source);
   sel_seqdesc_4_bioseq.push_back(CSeqdesc::e_Molinfo);
   sel_seqdesc_4_bioseq.push_back(CSeqdesc::e_Title);
   sel_seqdesc_4_bioseq.push_back(CSeqdesc::e_User);
   sel_seqdesc_4_bioseq.push_back(CSeqdesc::e_Genbank);

   m_vec_sf.push_back(&CTestAndRepData :: all_feat);
   m_vec_sf.push_back(&CTestAndRepData :: bioseq_biosrc_feat);
   m_vec_sf.push_back(&CTestAndRepData :: biosrc_feat);
   m_vec_sf.push_back(&CTestAndRepData :: biosrc_orgmod_feat);
   m_vec_sf.push_back(&CTestAndRepData :: biosrc_subsrc_feat);
   m_vec_sf.push_back(&CTestAndRepData :: cd_feat);
   m_vec_sf.push_back(&CTestAndRepData :: D_loop_feat);
   m_vec_sf.push_back(&CTestAndRepData :: exon_feat);
   m_vec_sf.push_back(&CTestAndRepData :: gap_feat);
   m_vec_sf.push_back(&CTestAndRepData :: gene_feat);
   m_vec_sf.push_back(&CTestAndRepData :: intron_feat);
   m_vec_sf.push_back(&CTestAndRepData :: miscfeat_feat);
   m_vec_sf.push_back(&CTestAndRepData :: mix_feat);
   m_vec_sf.push_back(&CTestAndRepData :: mrna_feat);
   m_vec_sf.push_back(&CTestAndRepData :: non_prot_feat);
   m_vec_sf.push_back(&CTestAndRepData :: org_orgmod_feat);
   m_vec_sf.push_back(&CTestAndRepData :: otherRna_feat);
   m_vec_sf.push_back(&CTestAndRepData :: promoter_feat);
   m_vec_sf.push_back(&CTestAndRepData :: prot_feat);
   m_vec_sf.push_back(&CTestAndRepData :: pub_feat);
   m_vec_sf.push_back(&CTestAndRepData :: rbs_feat);
   m_vec_sf.push_back(&CTestAndRepData :: rna_feat);
   m_vec_sf.push_back(&CTestAndRepData :: rna_not_mrna_feat);
   m_vec_sf.push_back(&CTestAndRepData :: rrna_feat);
   m_vec_sf.push_back(&CTestAndRepData :: trna_feat);
   m_vec_sf.push_back(&CTestAndRepData :: utr3_feat);
   m_vec_sf.push_back(&CTestAndRepData :: utr5_feat);

   m_vec_desc.push_back(&CTestAndRepData :: bioseq_biosrc_seqdesc);
   m_vec_desc.push_back(&CTestAndRepData :: bioseq_genbank);
   m_vec_desc.push_back(&CTestAndRepData :: bioseq_title);
   m_vec_desc.push_back(&CTestAndRepData :: bioseq_user);
   m_vec_desc.push_back(&CTestAndRepData :: biosrc_orgmod_seqdesc);
   m_vec_desc.push_back(&CTestAndRepData :: biosrc_seqdesc);
   m_vec_desc.push_back(&CTestAndRepData :: biosrc_subsrc_seqdesc);
   m_vec_desc.push_back(&CTestAndRepData :: comm_seqdesc);
   m_vec_desc.push_back(&CTestAndRepData :: molinfo_seqdesc);
   m_vec_desc.push_back(&CTestAndRepData :: org_orgmod_seqdesc);
   m_vec_desc.push_back(&CTestAndRepData :: pub_seqdesc);
   m_vec_desc.push_back(&CTestAndRepData :: title_seqdesc);
   m_vec_desc.push_back(&CTestAndRepData :: user_seqdesc);

   m_vec_entry.push_back(&CTestAndRepData :: biosrc_orgmod_seqdesc_seqentry);
   m_vec_entry.push_back(&CTestAndRepData :: biosrc_seqdesc_seqentry);
   m_vec_entry.push_back(&CTestAndRepData :: biosrc_subsrc_seqdesc_seqentry);
   m_vec_entry.push_back(&CTestAndRepData :: comm_seqdesc_seqentry);
   m_vec_entry.push_back(&CTestAndRepData :: molinfo_seqdesc_seqentry);
   m_vec_entry.push_back(&CTestAndRepData :: org_orgmod_seqdesc_seqentry);
   m_vec_entry.push_back(&CTestAndRepData :: pub_seqdesc_seqentry);
   m_vec_entry.push_back(&CTestAndRepData :: title_seqdesc_seqentry);
   m_vec_entry.push_back(&CTestAndRepData :: user_seqdesc_seqentry);
};

void CCheckingClass :: x_Clean()
{
   NON_CONST_ITERATE (vector <vector <const CSeq_feat*> * >, it, m_vec_sf) {
       if ( !(*it)->empty() ) {
          (*it)->clear();
       }
   };

   NON_CONST_ITERATE (vector < vector <const CSeqdesc*> * >, it, m_vec_desc) {
       if ( !(*it)->empty() ) {
           (*it)->clear();
       }
   };

   NON_CONST_ITERATE (vector < vector <const CSeq_entry*> * >, it, m_vec_entry){
       if ( !(*it)->empty() ) {
          (*it)->clear();
       }
   };
};

void CCheckingClass :: CollectSeqdescFromSeqEntry(const CSeq_entry_Handle& seq_entry_h)
{
   const CSeq_entry* entry_pt= seq_entry_h.GetCompleteObject().GetPointer();
   for (CSeqdesc_CI seqdesc_it(seq_entry_h, sel_seqdesc, 1); seqdesc_it; ++seqdesc_it) {
       if (seqdesc_it->IsOrg()) {
           if (seqdesc_it->GetOrg().IsSetOrgMod()) {
               CTestAndRepData::org_orgmod_seqdesc.push_back( &(*seqdesc_it) );
               CTestAndRepData::org_orgmod_seqdesc_seqentry.push_back(entry_pt);
           }
       } 
       else if (seqdesc_it->IsMolinfo()) {
           CTestAndRepData :: molinfo_seqdesc.push_back( &(*seqdesc_it) );
           CTestAndRepData :: molinfo_seqdesc_seqentry.push_back(entry_pt);
       }
       else if ( seqdesc_it->IsPub() ) {
           CTestAndRepData :: pub_seqdesc.push_back( &(*seqdesc_it) );
           CTestAndRepData :: pub_seqdesc_seqentry.push_back(entry_pt);
       }
       else if ( seqdesc_it->IsComment() ) {
           CTestAndRepData :: comm_seqdesc.push_back( &(*seqdesc_it) );
           CTestAndRepData :: comm_seqdesc_seqentry.push_back(entry_pt);
       }
       else if ( seqdesc_it->IsSource() ) {
           CTestAndRepData :: biosrc_seqdesc.push_back( &(*seqdesc_it) );
           CTestAndRepData :: biosrc_seqdesc_seqentry.push_back(entry_pt);
           if (seqdesc_it->GetSource().IsSetOrgMod() ) {
               CTestAndRepData::biosrc_orgmod_seqdesc.push_back(&(*seqdesc_it));
               CTestAndRepData::biosrc_orgmod_seqdesc_seqentry.push_back(entry_pt);
           }
           if (seqdesc_it->GetSource().CanGetSubtype()) {
               CTestAndRepData :: biosrc_subsrc_seqdesc.push_back( &(*seqdesc_it) );
               CTestAndRepData :: biosrc_subsrc_seqdesc_seqentry.push_back(entry_pt);
           }
       } 
       else if ( seqdesc_it->IsTitle()) {
           CTestAndRepData :: title_seqdesc.push_back( &(*seqdesc_it) );
           CTestAndRepData :: title_seqdesc_seqentry.push_back(entry_pt);
       }
       else if ( seqdesc_it->IsUser()) {
           CTestAndRepData :: user_seqdesc.push_back( &(*seqdesc_it) );
           CTestAndRepData :: user_seqdesc_seqentry.push_back(entry_pt);
       }
   }
   if (seq_entry_h.IsSet()) {
       const CBioseq_set& bioseq_set = *(seq_entry_h.GetSet().GetCompleteObject());
       ITERATE (list <CRef <CSeq_entry> >, it, bioseq_set.GetSeq_set()) {
          CSeq_entry_Handle 
              subseq_hl = thisInfo.scope->GetSeq_entryHandle( **it );
          CollectSeqdescFromSeqEntry(subseq_hl);
       }
   } 
};

void CCheckingClass :: CheckSeqEntry(CRef <CSeq_entry> seq_entry)
{
  // ini.
  thisTest.is_BIOSRC_run = false;
  thisTest.is_Biosrc_Orgmod_run = false;
  thisTest.is_Comment_run = false;
  thisTest.is_Defl_run = false;
  thisTest.is_DESC_user_run = false;
  thisTest.is_IncnstUser_run = false;
  thisTest.is_Pub_run = false;
  thisTest.is_Quals_run = false;
  thisTest.is_Subsrc_run = false;
  thisTest.is_TaxCflts_run = false;

  GoTests(thisGrp.tests_on_SubmitBlk, *seq_entry);

  if (!thisGrp.tests_on_SeqEntry_feat_desc.empty()) {
      CSeq_entry_Handle 
          seq_entry_h = thisInfo.scope->GetSeq_entryHandle(*seq_entry);

      for (CFeat_CI feat_it(seq_entry_h, sel_seqfeat_4_seq_entry); feat_it; ++feat_it) {
          const CSeq_feat& seq_feat = feat_it->GetOriginalFeature();
          const CSeqFeatData& seq_feat_dt = seq_feat.GetData();
          switch (seq_feat_dt.Which()) {
            case CSeqFeatData::e_Org: {
                if (seq_feat_dt.GetOrg().IsSetOrgMod()) 
                       CTestAndRepData::org_orgmod_feat.push_back(&seq_feat); 
                break;
            }
            case CSeqFeatData::e_Pub: {
                CTestAndRepData::pub_feat.push_back(&seq_feat); 
                break;
            }
            case CSeqFeatData::e_Biosrc: {
                CTestAndRepData::biosrc_feat.push_back(&seq_feat);
                if ( seq_feat_dt.GetBiosrc().IsSetOrgMod() ) 
                          CTestAndRepData::biosrc_orgmod_feat.push_back(&seq_feat);
                if (seq_feat_dt.GetBiosrc().CanGetSubtype())
                           CTestAndRepData::biosrc_subsrc_feat.push_back(&seq_feat);
                break;
            }
            default: break;
          }
      }

      CollectSeqdescFromSeqEntry(seq_entry_h);

      GoTests(thisGrp.tests_on_SeqEntry_feat_desc, *seq_entry);
  }

  GoTests(thisGrp.tests_on_SeqEntry, *seq_entry);

  // clean
  x_Clean();
}

void CCheckingClass :: CheckBioseqSet ( CBioseq_set& bioseq_set)
{
   thisTest.is_BioSet_run = false;
   GoTests(thisGrp.tests_on_BioseqSet, bioseq_set); 
};

void CCheckingClass :: CheckBioseq (CBioseq& bioseq)
{
// ini.
   thisTest.is_Aa_run = false;
   thisTest.is_AllAnnot_run = false;
   thisTest.is_Bad_Gene_Nm = false;
   thisTest.is_BacPartial_run = false;
   thisTest.is_Bases_N_run = false;
   thisTest.is_BASES_N_run = false; // no needed?
   thisTest.is_CDs_run = false;
   thisTest.is_CdTransl_run = false;
   thisTest.is_FeatCnt_run = false;
   thisTest.is_Genes_run = false;
   thisTest.is_Genes_oncall_run = false;
   thisTest.is_GP_Set_run = false;
   thisTest.is_LocusTag_run = false;
   thisTest.is_MolInfo_run = false;
   thisTest.is_MRNA_run = false;
   thisTest.is_mRNA_run = false;
   thisTest.is_Prot_run = false;
   thisTest.is_ProtFeat_run = false;
   thisTest.is_TRna_run = false;
   thisTest.is_RRna_run = false;
   thisTest.is_SusPhrase_run = false;
   thisTest.is_SusProd_run = false;
   thisTest.is_TaxDef_run = false;

   GoTests(thisGrp.tests_on_Bioseq, bioseq);

   if (!thisGrp.tests_on_Bioseq_CFeat.empty() 
          || (!thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.empty()
                     && !CTestAndRepData::IsmRNASequenceInGenProdSet(bioseq)) 
          || !thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.empty() ) {

      CBioseq_Handle bioseq_hl = thisInfo.scope->GetBioseqHandle(bioseq);
      CSeqFeatData::ESubtype subtp;
      for (CFeat_CI feat_it(bioseq_hl); feat_it; ++feat_it) {
          const CSeq_feat& seq_feat = (feat_it->GetOriginalFeature());
          const CSeqFeatData& seq_feat_dt = seq_feat.GetData();

          CTestAndRepData :: all_feat.push_back(&seq_feat);

          if (seq_feat_dt.GetSubtype() != CSeqFeatData::eSubtype_prot) {
               CTestAndRepData::non_prot_feat.push_back(&seq_feat);
          }

          switch (seq_feat_dt.Which()) {
            case CSeqFeatData::e_Biosrc: {
                 CTestAndRepData::bioseq_biosrc_feat.push_back(&seq_feat); 
                 break;
            }
            case CSeqFeatData::e_Gene: {
                 CTestAndRepData::gene_feat.push_back(&seq_feat); 
                 break;
            }
            case CSeqFeatData::e_Prot: {
                 CTestAndRepData::prot_feat.push_back(&seq_feat); 
                 break;
            }
            case CSeqFeatData::e_Cdregion: {
                CTestAndRepData::cd_feat.push_back(&seq_feat);
                CTestAndRepData::mix_feat.push_back(&seq_feat);
                break;
            }
            case CSeqFeatData::e_Rna: {
                CTestAndRepData::rna_feat.push_back(&seq_feat);
                subtp = seq_feat_dt.GetSubtype();
                if (subtp == CSeqFeatData :: eSubtype_mRNA) {
                       CTestAndRepData::mrna_feat.push_back(&seq_feat);
                }
                else {
                   CTestAndRepData::rna_not_mrna_feat.push_back(&seq_feat);
                   switch (subtp) {
                     case CSeqFeatData::eSubtype_rRNA:
                          CTestAndRepData::rrna_feat.push_back(&seq_feat); 
                          break;
                     case CSeqFeatData::eSubtype_otherRNA:
                          CTestAndRepData::otherRna_feat.push_back(&seq_feat); 
                          break;
                     case CSeqFeatData::eSubtype_tRNA:
                          CTestAndRepData::trna_feat.push_back(&seq_feat); 
                          break;
                     default: break;
                   }
                }
                CTestAndRepData::mix_feat.push_back(&seq_feat);
                break;
          }
          case CSeqFeatData::e_Imp: {
                subtp = seq_feat_dt.GetSubtype();
                switch (subtp) {
                  case CSeqFeatData::eSubtype_gap:
                       CTestAndRepData::gap_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData::eSubtype_RBS:
                       CTestAndRepData::rbs_feat.push_back(&seq_feat);
                       CTestAndRepData::mix_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData::eSubtype_intron:
                       CTestAndRepData::intron_feat.push_back(&seq_feat);
                       CTestAndRepData::mix_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData::eSubtype_exon:
                       CTestAndRepData::exon_feat.push_back(&seq_feat);
                       CTestAndRepData::mix_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData::eSubtype_misc_feature:
                       CTestAndRepData :: miscfeat_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData::eSubtype_3UTR:
                       CTestAndRepData::utr3_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData::eSubtype_5UTR:
                       CTestAndRepData::utr5_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData::eSubtype_promoter:
                       CTestAndRepData::promoter_feat.push_back(&seq_feat);
                       break;
                  default: break;
              }
          }
          default: break;
        }
     }

     if (!thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.empty()) {
        for (CSeqdesc_CI it(bioseq_hl, sel_seqdesc_4_bioseq); it; ++it) {
          switch (it->Which()) {
            case CSeqdesc::e_Source: 
                   CTestAndRepData::bioseq_biosrc_seqdesc.push_back(&(*it));
                   break;
            case CSeqdesc::e_Title: 
                   CTestAndRepData::bioseq_title.push_back(&(*it));
                   break;
            case CSeqdesc::e_User:
                   CTestAndRepData::bioseq_user.push_back(&(*it));
                   break;
            case CSeqdesc::e_Genbank:
                   CTestAndRepData::bioseq_genbank.push_back(&(*it));
                   break;
            default: break;
          }
        }

        GoTests(thisGrp.tests_on_Bioseq_CFeat_CSeqdesc, bioseq);
     }

     if (!thisGrp.tests_on_Bioseq_CFeat.empty()) {
          GoTests(thisGrp.tests_on_Bioseq_CFeat, bioseq);
     }
     if (!thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.empty()
                        && !CTestAndRepData::IsmRNASequenceInGenProdSet(bioseq)) {
         GoTests(thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet, bioseq);
     }
   }

   if (!CTestAndRepData::IsmRNASequenceInGenProdSet(bioseq)) {
       GoTests(thisGrp.tests_on_Bioseq_NotInGenProdSet, bioseq);
   }

   unsigned prev_len = thisGrp.tests_4_once.size();
   if ( bioseq.IsNa() && !thisGrp.tests_on_Bioseq_na.empty()) {
        GoTests(thisGrp.tests_on_Bioseq_na, bioseq);        

        unsigned cur_len = thisGrp.tests_4_once.size();
        if (cur_len > prev_len) {
            NON_CONST_ITERATE(vector < CRef < CTestAndRepData > >, 
                              it, 
                              thisGrp.tests_on_Bioseq_na) {
                if (thisGrp.tests_4_once[cur_len - 1].GetPointer() == *it) {
                    thisGrp.tests_on_Bioseq_na.erase(it);
                    break;
                }
            }
       }
   }

   if (bioseq.IsAa() && !thisGrp.tests_on_Bioseq_aa.empty()) {
        GoTests(thisGrp.tests_on_Bioseq_aa, bioseq);        
   }

   // clean
   x_Clean();

} // CheckBioseq

END_NCBI_SCOPE
