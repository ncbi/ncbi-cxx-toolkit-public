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
#include <objects/seqfeat/Gb_qual.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include "hchecking_class.hpp"
#include "hDiscRep_app.hpp"
#include "hDiscRep_config.hpp"
#include "hDiscRep_tests.hpp"
#include "hUtilib.hpp"

using namespace ncbi;
using namespace objects;
using namespace DiscRepNmSpc;
using namespace sequence;

string strtmp;
static CDiscRepInfo  thisInfo;
static CDiscTestInfo thisTest;

// unused
bool CCheckingClass :: CanGetOrgMod(const CBioSource& biosrc)
{
  if (biosrc.GetOrg().CanGetOrgname() && biosrc.GetOrg().GetOrgname().CanGetMod())
       return true;
  else return false;
};


void CCheckingClass :: CollectSeqdescFromSeqEntry(const CSeq_entry_Handle& seq_entry_h)
{
   for (CSeqdesc_CI seqdesc_it(seq_entry_h, sel_seqdesc, 1); seqdesc_it; ++seqdesc_it) {
         const CSeq_entry* entry_pt = seq_entry_h.GetCompleteObject().GetPointer();
         if (seqdesc_it->IsOrg()) {
               if (seqdesc_it->GetOrg().IsSetOrgMod()) {
                  CTestAndRepData :: org_orgmod_seqdesc.push_back( &(*seqdesc_it) );
                  CTestAndRepData :: org_orgmod_seqdesc_seqentry.push_back(entry_pt);
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
               if ( seqdesc_it->GetSource().IsSetOrgMod() ) {
                  CTestAndRepData :: biosrc_orgmod_seqdesc.push_back( &(*seqdesc_it) );
                  CTestAndRepData :: biosrc_orgmod_seqdesc_seqentry.push_back(entry_pt);
               }
               if ( seqdesc_it->GetSource().CanGetSubtype()) {
                  CTestAndRepData :: biosrc_subsrc_seqdesc.push_back( &(*seqdesc_it) );
                  CTestAndRepData :: biosrc_subsrc_seqdesc_seqentry.push_back(entry_pt);
               }
         }
         else if ( seqdesc_it->IsTitle() && seq_entry_h.IsSet()) {  // why IsSet()?
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
        CSeq_entry_Handle subseq_hl = thisInfo.scope->GetSeq_entryHandle( **it );
        CollectSeqdescFromSeqEntry(subseq_hl);
     }
   } 
};
 


void CCheckingClass :: CheckSeqEntry(CRef <CSeq_entry> seq_entry)
{
  // ini.
  thisTest.is_BIOSRC_run = false;
  thisTest.is_BIOSRC1_run = false;
  thisTest.is_Biosrc_Orgmod_run = false;
  thisTest.is_Comment_run = false;
  thisTest.is_Defl_run = false;
  thisTest.is_DESC_user_run = false;
  thisTest.is_Pub_run = false;
  thisTest.is_Quals_run = false;
  thisTest.is_Subsrc_run = false;
  thisTest.is_TaxCflts_run = false;

  GoTests(CRepConfig :: tests_on_SubmitBlk, *seq_entry);

  if (!CRepConfig::tests_on_SeqEntry_feat_desc.empty()) {
     CSeq_entry_Handle seq_entry_h = thisInfo.scope->GetSeq_entryHandle(*seq_entry);

     CTestAndRepData::pub_feat.clear();
     CTestAndRepData::biosrc_feat.clear();
     CTestAndRepData::biosrc_orgmod_feat.clear();
     CTestAndRepData::biosrc_subsrc_feat.clear();
     CTestAndRepData::org_orgmod_feat.clear();

     for (CFeat_CI feat_it(seq_entry_h, sel_seqfeat_4_seq_entry); feat_it; ++feat_it) {
        const CSeq_feat& seq_feat = (feat_it->GetOriginalFeature());
        const CSeqFeatData& seq_feat_dt = seq_feat.GetData();
        switch (seq_feat_dt.Which()) {
           case CSeqFeatData::e_Org:
                  if (seq_feat_dt.GetOrg().IsSetOrgMod()) 
                     CTestAndRepData::org_orgmod_feat.push_back(&seq_feat); 
                  break;
           case CSeqFeatData::e_Pub: 
                  CTestAndRepData::pub_feat.push_back(&seq_feat); break;
           case CSeqFeatData::e_Biosrc:
                  CTestAndRepData::biosrc_feat.push_back(&seq_feat);
                  if ( seq_feat_dt.GetBiosrc().IsSetOrgMod() ) 
                          CTestAndRepData::biosrc_orgmod_feat.push_back(&seq_feat);
                  if (seq_feat_dt.GetBiosrc().CanGetSubtype())
                           CTestAndRepData::biosrc_subsrc_feat.push_back(&seq_feat);
                  break;
           default: break;
        }
     }

     CTestAndRepData :: molinfo_seqdesc.clear();
     CTestAndRepData :: pub_seqdesc.clear();
     CTestAndRepData :: comm_seqdesc.clear();
     CTestAndRepData :: biosrc_seqdesc.clear();
     CTestAndRepData :: biosrc_orgmod_seqdesc.clear();
     CTestAndRepData :: biosrc_subsrc_seqdesc.clear();
     CTestAndRepData :: title_seqdesc.clear();
     CTestAndRepData :: user_seqdesc.clear();
     CTestAndRepData :: org_orgmod_seqdesc.clear();

     CTestAndRepData :: molinfo_seqdesc_seqentry.clear();
     CTestAndRepData :: pub_seqdesc_seqentry.clear();
     CTestAndRepData :: comm_seqdesc_seqentry.clear();
     CTestAndRepData :: biosrc_seqdesc_seqentry.clear();
     CTestAndRepData :: biosrc_orgmod_seqdesc_seqentry.clear();
     CTestAndRepData :: biosrc_subsrc_seqdesc_seqentry.clear();
     CTestAndRepData :: title_seqdesc_seqentry.clear();
     CTestAndRepData :: user_seqdesc_seqentry.clear();
     CTestAndRepData :: org_orgmod_seqdesc_seqentry.clear();

     CollectSeqdescFromSeqEntry(seq_entry_h);

     GoTests(CRepConfig :: tests_on_SeqEntry_feat_desc, *seq_entry);
  }

  GoTests(CRepConfig :: tests_on_SeqEntry, *seq_entry);
}


void CCheckingClass :: CheckGbQual (const vector < CRef< CGb_qual > >& gb_quals)
{
    has_locus_tag = false;
    ITERATE (vector <CRef< CGb_qual > >, it, gb_quals) {
//         if ((gb_quals[i])->CanGetQual() && (gb_quals[i])->GetQual()== "locus_tag") {
         if ( (*it)->CanGetQual() && (*it)->GetQual()== "locus_tag") {
            has_locus_tag = true;
            num_qual_locus_tag ++;
            return;
         }
         else has_locus_tag = false;
    }

} // CheckGbQual



void CCheckingClass :: CheckSeqFeat(CSeq_feat& seq_feat)
{
num_seq_feats++;
  GoTests(CRepConfig::tests_on_SeqFeat, seq_feat);
 
  CBioseq_Handle bioseq_h = GetBioseqFromSeqLoc(seq_feat.GetLocation(),
                                                      *thisInfo.scope);
  CConstRef <CBioseq> bioseq = bioseq_h.GetCompleteBioseq();
  if (bioseq.GetPointer() 
       && !(CTestAndRepData::IsmRNASequenceInGenProdSet(*bioseq)) ) {
     GoTests(CRepConfig::tests_on_GenProdSetFeat, seq_feat);
  }

} // CheckSeqFeat()


bool CCheckingClass :: SortByFrom(const CSeq_feat* seqfeat1, const CSeq_feat* seqfeat2)
{
  return (seqfeat1->GetLocation().GetStart(eExtreme_Positional)
                          < seqfeat2->GetLocation().GetStart(eExtreme_Positional));
};



void CCheckingClass :: CheckBioseqSet ( CBioseq_set& bioseq_set)
{
   thisTest.is_BioSet_run = false;
   GoTests(CRepConfig::tests_on_BioseqSet, bioseq_set); 
};


void CCheckingClass :: CheckBioseq ( CBioseq& bioseq)
{
// ini.
   thisTest.is_Aa_run = false;
   thisTest.is_AllAnnot_run = false;
   thisTest.is_BacPartial_run = false;
   thisTest.is_BASES_N_run = false;
   thisTest.is_CDs_run = false;
   thisTest.is_CdTransl_run = false;
   thisTest.is_GP_Set_run = false;
   thisTest.is_MolInfo_run = false;
   thisTest.is_MRNA_run = false;
   thisTest.is_mRNA_run = false;
   thisTest.is_Prot_run = false;
   thisTest.is_ProtFeat_run = false;
   thisTest.is_TRRna_run = false;
   thisTest.is_RRna_run = false;
   thisTest.is_SusPhrase_run = false;
   thisTest.is_TaxDef_run = false;

   GoTests(CRepConfig::tests_on_Bioseq, bioseq);

   if (!CRepConfig::tests_on_Bioseq_CFeat.empty() 
             || (!CRepConfig::tests_on_Bioseq_CFeat_NotInGenProdSet.empty()
                     && !CTestAndRepData::IsmRNASequenceInGenProdSet(bioseq)) 
             || !CRepConfig::tests_on_Bioseq_CFeat_CSeqdesc.empty() ) {

     CBioseq_Handle bioseq_hl = thisInfo.scope->GetBioseqHandle(bioseq);
     //for (CFeat_CI feat_it(bioseq_hl, sel_seqfeat); feat_it; ++feat_it) 
     CSeqFeatData::ESubtype subtp;
     for (CFeat_CI feat_it(bioseq_hl); feat_it; ++feat_it) {
        const CSeq_feat& seq_feat = (feat_it->GetOriginalFeature());
        const CSeqFeatData& seq_feat_dt = seq_feat.GetData();

        CTestAndRepData :: all_feat.push_back(&seq_feat);

        if (!(seq_feat_dt.IsProt())) CTestAndRepData::non_prot_feat.push_back(&seq_feat);

        switch (seq_feat_dt.Which()) {
          case CSeqFeatData::e_Biosrc:
               CTestAndRepData::bioseq_biosrc_feat.push_back(&seq_feat); break;
          case CSeqFeatData::e_Gene:
                CTestAndRepData::gene_feat.push_back(&seq_feat); break;
          case CSeqFeatData::e_Prot:
                CTestAndRepData::prot_feat.push_back(&seq_feat); break;
          case CSeqFeatData::e_Cdregion:
                CTestAndRepData::cd_feat.push_back(&seq_feat);
                CTestAndRepData::mix_feat.push_back(&seq_feat);
                break;
          case CSeqFeatData::e_Rna:
                CTestAndRepData::rna_feat.push_back(&seq_feat);
                subtp = seq_feat_dt.GetSubtype();
                if (subtp == CSeqFeatData :: eSubtype_mRNA)
                       CTestAndRepData::mrna_feat.push_back(&seq_feat);
                else {
                   CTestAndRepData::rna_not_mrna_feat.push_back(&seq_feat);
                   switch (subtp) {
                     case CSeqFeatData::eSubtype_rRNA:
                          CTestAndRepData::rrna_feat.push_back(&seq_feat); break;
                     case CSeqFeatData::eSubtype_otherRNA:
                          CTestAndRepData::otherRna_feat.push_back(&seq_feat); break;
                     case  CSeqFeatData::eSubtype_tRNA:
                          CTestAndRepData::trna_feat.push_back(&seq_feat); break;
                     default: break;
                   }
                }
                CTestAndRepData::mix_feat.push_back(&seq_feat);
                break;
          case CSeqFeatData::e_Imp:
                subtp = seq_feat_dt.GetSubtype();
                switch (subtp) {
                  case CSeqFeatData::eSubtype_RBS :
                       CTestAndRepData::rbs_feat.push_back(&seq_feat);
                       CTestAndRepData::mix_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData::eSubtype_intron :
                       CTestAndRepData::intron_feat.push_back(&seq_feat);
                       CTestAndRepData::mix_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData::eSubtype_exon:
                       CTestAndRepData::exon_feat.push_back(&seq_feat);
                       CTestAndRepData::mix_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData :: eSubtype_misc_feature:
                       CTestAndRepData :: miscfeat_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData :: eSubtype_3UTR:
                       CTestAndRepData :: utr3_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData :: eSubtype_5UTR:
                       CTestAndRepData :: utr5_feat.push_back(&seq_feat);
                       break;
                  case CSeqFeatData :: eSubtype_promoter:
                       CTestAndRepData :: promoter_feat.push_back(&seq_feat);
                       break;
                  default: break;
              }
          default: break;
        }

/*
        if (seq_feat_dt.IsGene()) CTestAndRepData::gene_feat.push_back(&seq_feat);
        else if (seq_feat_dt.IsProt()) CTestAndRepData::prot_feat.push_back(&seq_feat);
        else  if (seq_feat_dt.IsCdregion()) {
               CTestAndRepData::cd_feat.push_back(&seq_feat);
               CTestAndRepData::mix_feat.push_back(&seq_feat);
        }
        else if (seq_feat_dt.IsRna()) {
               CTestAndRepData::rna_feat.push_back(&seq_feat); 
               CSeqFeatData::ESubtype subtp = seq_feat_dt.GetSubtype();
               if (subtp != CSeqFeatData :: eSubtype_mRNA) {
                 CTestAndRepData::rna_not_mrna_feat.push_back(&seq_feat);
                 if (subtp == CSeqFeatData :: eSubtype_rRNA)
                      CTestAndRepData :: rrna_feat.push_back(&seq_feat);
               }
               CTestAndRepData::mix_feat.push_back(&seq_feat);
        }
        else if (seq_feat_dt.IsImp()) {
              CSeqFeatData::ESubtype subtp = seq_feat_dt.GetSubtype();
              switch (subtp) {
                case CSeqFeatData::eSubtype_D_loop:
                    CTestAndRepData::D_loop_feat.push_back(&seq_feat);
                    break;
                case CSeqFeatData::eSubtype_repeat_region:
                    CTestAndRepData::repeat_region_feat.push_back(&seq_feat);
                    break;
                case CSeqFeatData::eSubtype_RBS :
                    CTestAndRepData::rbs_feat.push_back(&seq_feat);
                    CTestAndRepData::mix_feat.push_back(&seq_feat);
                    break;
                case CSeqFeatData::eSubtype_intron :
                     CTestAndRepData::intron_feat.push_back(&seq_feat);
                     CTestAndRepData::mix_feat.push_back(&seq_feat);
                     break;
                case CSeqFeatData::eSubtype_exon:
                     CTestAndRepData::mix_feat.push_back(&seq_feat);
                     break;
                case CSeqFeatData :: eSubtype_misc_feature:
                     CTestAndRepData :: miscfeat_feat.push_back(&seq_feat);
                     break;
                default: break;
              }
        } 
*/
     }

// CFeat_CI is sorted:
//    sort(CTestAndRepData::gene_feat.begin(), CTestAndRepData::gene_feat.end(), CCheckingClass :: SortByFrom);
     
     if (!CRepConfig::tests_on_Bioseq_CFeat_CSeqdesc.empty()) {
        for (CSeqdesc_CI it(bioseq_hl, sel_seqdesc_4_bioseq); it; ++it) {
          switch (it->Which()) {
            case CSeqdesc::e_Source: 
                   CTestAndRepData::bioseq_biosrc_seqdesc.push_back(&(*it));
                   break;
            case CSeqdesc::e_Molinfo: 
                   CTestAndRepData::bioseq_molinfo.push_back(&(*it));
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

        GoTests(CRepConfig::tests_on_Bioseq_CFeat_CSeqdesc, bioseq);

        CTestAndRepData::bioseq_biosrc_seqdesc.clear();
        CTestAndRepData::bioseq_molinfo.clear();
        CTestAndRepData::bioseq_title.clear();
     }

     if (!CRepConfig::tests_on_Bioseq_CFeat.empty())
              GoTests(CRepConfig::tests_on_Bioseq_CFeat, bioseq);
     if (!CRepConfig::tests_on_Bioseq_CFeat_NotInGenProdSet.empty()
                               && !CTestAndRepData::IsmRNASequenceInGenProdSet(bioseq))
               GoTests(CRepConfig::tests_on_Bioseq_CFeat_NotInGenProdSet, bioseq);

     CTestAndRepData::gene_feat.clear();
     CTestAndRepData::bioseq_biosrc_feat.clear();
     CTestAndRepData::mix_feat.clear();
     CTestAndRepData::cd_feat.clear();
     CTestAndRepData::rna_feat.clear();
     CTestAndRepData::rna_not_mrna_feat.clear();
     CTestAndRepData::prot_feat.clear();
     CTestAndRepData::pub_feat.clear();
     CTestAndRepData::all_feat.clear();
     CTestAndRepData::non_prot_feat.clear();
     CTestAndRepData::rrna_feat.clear();
     CTestAndRepData::miscfeat_feat.clear();
     CTestAndRepData::otherRna_feat.clear();
     CTestAndRepData::utr3_feat.clear();
     CTestAndRepData::utr5_feat.clear();
     CTestAndRepData::promoter_feat.clear();
     CTestAndRepData::mrna_feat.clear();
     CTestAndRepData::trna_feat.clear();
   }

/*
   if (!CTestAndRepData::IsmRNASequenceInGenProdSet(bioseq))
         GoTests(CRepConfig::tests_on_Bioseq_NotInGenProdSet, bioseq);
*/

   unsigned prev_len = CRepConfig:: tests_4_once.size();
   if ( bioseq.IsNa() && !CRepConfig::tests_on_Bioseq_na.empty()) {
        GoTests(CRepConfig::tests_on_Bioseq_na, bioseq);        

        unsigned cur_len = CRepConfig:: tests_4_once.size();
        if (cur_len > prev_len) {
            NON_CONST_ITERATE(vector < CRef < CTestAndRepData > >, it, 
                                                         CRepConfig::tests_on_Bioseq_na) {
                if (CRepConfig:: tests_4_once[cur_len - 1].GetPointer() == *it) {
                    CRepConfig::tests_on_Bioseq_na.erase(it);
                    break;
                }
            }
       }
   }

   if (bioseq.IsAa() && !CRepConfig::tests_on_Bioseq_aa.empty())
        GoTests(CRepConfig::tests_on_Bioseq_aa, bioseq);        
} // CheckBioseq



void CCheckingClass :: CheckSeqInstMol (CSeq_inst& seq_inst, 
                                                        CBioseq& bioseq)
{

} // CheckSeqInstMol



void CCheckingClass :: GoGetRep(vector <CRef <CTestAndRepData> >& test_category)
{
   NON_CONST_ITERATE (vector <CRef <CTestAndRepData> >, it, test_category) {
       CRef < CClickableItem > c_item (new CClickableItem);
       if (thisInfo.test_item_list.find((*it)->GetName()) != thisInfo.test_item_list.end()) {
 //cerr <<"outptu " << (*it)->GetName() << endl;
            c_item->setting_name = (*it)->GetName();
            c_item->item_list = thisInfo.test_item_list[(*it)->GetName()];
            strtmp = (*it)->GetName();
            if ( strtmp != "DISC_SOURCE_QUALS_ASNDISC"
                        && strtmp != "LOCUS_TAGS"
                        && strtmp != "INCONSISTENT_PROTEIN_ID_PREFIX"
               )
                  thisInfo.disc_report_data.push_back(c_item);
            (*it)->GetReport(c_item); 
       }
       else if ( (*it)->GetName() == "DISC_FEATURE_COUNT") (*it)->GetReport(c_item); 
 //cerr << "done\n";
   }
};


void CCheckingClass :: CollectRepData()
{
  GoGetRep(CRepConfig::tests_on_Bioseq);
  GoGetRep(CRepConfig::tests_on_Bioseq_aa);
  GoGetRep(CRepConfig::tests_on_Bioseq_na);
  GoGetRep(CRepConfig::tests_on_Bioseq_CFeat);
  GoGetRep(CRepConfig::tests_on_Bioseq_CFeat_NotInGenProdSet);
  GoGetRep(CRepConfig::tests_on_Bioseq_NotInGenProdSet);
  GoGetRep(CRepConfig::tests_on_GenProdSetFeat);
  GoGetRep(CRepConfig::tests_on_SeqEntry);
  GoGetRep(CRepConfig::tests_on_SeqEntry_feat_desc);
  GoGetRep(CRepConfig::tests_4_once);
  GoGetRep(CRepConfig::tests_on_BioseqSet);
  GoGetRep(CRepConfig::tests_on_Bioseq_CFeat_CSeqdesc);

// clean
  CTestAndRepData :: pub_feat.clear();
  CTestAndRepData :: biosrc_feat.clear();
  CTestAndRepData :: pub_seqdesc.clear();
  CTestAndRepData :: biosrc_seqdesc.clear();
  CTestAndRepData :: title_seqdesc.clear();

/*
  auto_ptr < CTestAndRepData > disc_repdt (CTestAndRepData::factory("none"));
  Str2Strs :: iterator it;
  ITERATE (Str2Strs, it, thisInfo.test_item_list) {
       CRef < CClickableItem > c_item (new CClickableItem);
       c_item->setting_name = it->first;
       c_item->item_list = it->second;

       disc_repdt.reset(CTestAndRepData::factory(it->first));
       if (disc_repdt.get()) 
            disc_repdt->GetReport( c_item, it->first, it->second);
  }
*/
/*
  unsigned i, j;
  for (i=0; i< CRepConfig::tests_on_Bioseq.size(); i++) {
       disc_repdt = auto_ptr <CTestAndRepData> 
                       (CTestAndRepData::factory(CRepConfig::tests_on_Bioseq[i]));
       for (j=0; j<bioseq_ls.size(); j++) 
                               disc_testrep->TestOnObj(*(bioseq_ls[j]));
       disc_testrep->GetReport();
  }
*/

} // CollectRepData()



