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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader for feature tables
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/general/Object_id.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/object_manager.hpp>

#include <objtools/readers/readfeat.hpp>
#include <algo/sequence/orf.hpp>
#include <algo/align/prosplign/prosplign.hpp>

#include <objects/seqset/seqset_macros.hpp>
#include <objects/seq/seq_macros.hpp>

#include <algo/align/splign/compart_matching.hpp>
#include <algo/align/util/compartment_finder.hpp>
#include <objtools/readers/fasta.hpp>

#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Genetic_code.hpp>

#include <objmgr/seq_annot_ci.hpp>

#include <objmgr/annot_selector.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objmgr/annot_ci.hpp>
#include <objtools/edit/gaps_edit.hpp>

#include "feature_table_reader.hpp"

#include "table2asn_context.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace
{

    static string kAssemblyGap_feature = "assembly_gap";
    static string kGapType_qual = "gap_type";
    static string kLinkageEvidence_qual = "linkage_evidence";


    void MoveSomeDescr(CSeq_entry& dest, CBioseq& src)
    {
        CSeq_descr::Tdata::iterator it = src.SetDescr().Set().begin();

        while(it != src.SetDescr().Set().end())
        {
            switch ((**it).Which())
            {
            case CSeqdesc::e_Pub:
            case CSeqdesc::e_Source:
            case CSeqdesc::e_Create_date:
            case CSeqdesc::e_Update_date:
                {
                    dest.SetDescr().Set().push_back(*it);
                    src.SetDescr().Set().erase(it++);
                }
                break;
            default:
                it++;
            }
        }
    }

    bool GetOrgName(string& name, const CSeq_entry& entry)
    {
        if (entry.IsSet() && entry.GetSet().IsSetDescr())
        {
            ITERATE(CSeq_descr_Base::Tdata, it, entry.GetSet().GetDescr().Get())
            {
                if ((**it).IsSource())
                {
                    const CBioSource& source = (**it).GetSource();
                    if (source.IsSetTaxname())
                    {
                        name = source.GetTaxname();
                        return true;
                    }
                    if (source.IsSetOrgname())
                    {
                        if (source.GetOrgname().GetFlatName(name))
                            return true;
                    }
                    if (source.IsSetOrg() && source.GetOrg().IsSetOrgname())
                    {
                        if (source.GetOrg().GetOrgname().GetFlatName(name))
                            return true;
                    }
                }
                if ((**it).IsOrg())
                {
                    if ((**it).GetOrg().IsSetOrgname())
                    {
                        if ((**it).GetOrg().GetOrgname().GetFlatName(name))
                            return true;
                    }
                }
            }
        }
        else
        if (entry.IsSeq())
        {
        }
        return false;
    }

    struct SCSeqidCompare
    {
      inline
      bool operator()(const CSeq_id* left, const CSeq_id* right) const
      { 
         return *left < *right;
      };
    };

    const char mapids[] = {
        CSeqFeatData::e_Cdregion,
        CSeqFeatData::e_Rna,
        CSeqFeatData::e_Gene,
        CSeqFeatData::e_Org,
        CSeqFeatData::e_Prot,
        CSeqFeatData::e_Pub,              ///< publication applies to this seq
        CSeqFeatData::e_Seq,              ///< to annotate origin from another seq
        CSeqFeatData::e_Imp,
        CSeqFeatData::e_Region,           ///< named region (globin locus)
        CSeqFeatData::e_Comment,          ///< just a comment
        CSeqFeatData::e_Bond,
        CSeqFeatData::e_Site,
        CSeqFeatData::e_Rsite,            ///< restriction site  (for maps really)
        CSeqFeatData::e_User,             ///< user defined structure
        CSeqFeatData::e_Txinit,           ///< transcription initiation
        CSeqFeatData::e_Num,              ///< a numbering system
        CSeqFeatData::e_Psec_str,
        CSeqFeatData::e_Non_std_residue,  ///< non-standard residue here in seq
        CSeqFeatData::e_Het,              ///< cofactor, prosthetic grp, etc, bound to seq
        CSeqFeatData::e_Biosrc,
        CSeqFeatData::e_Clone,
        CSeqFeatData::e_Variation,
        CSeqFeatData::e_not_set      ///< No variant selected
    };

    struct SSeqAnnotCompare
    {
        static inline
        int mapwhich(CSeqFeatData::E_Choice c)
        {
            const char* m = mapids;
            if (c == CSeqFeatData::e_Gene)
                c = CSeqFeatData::e_Rna;

            return strchr(m, c)-m;
        }

        inline
        bool operator()(const CSeq_feat* left, const CSeq_feat* right) const
        {
            if (left->IsSetData() != right->IsSetData())
               return left < right;
            return mapwhich(left->GetData().Which()) < mapwhich(right->GetData().Which());
        }
    };

    void PostProcessFeatureTable(CSeq_entry& entry, CSeq_annot::TData::TFtable& ftable, int& id)
    {
        ftable.sort(SSeqAnnotCompare());
        for (CSeq_annot::C_Data::TFtable::iterator it = ftable.begin(); it != ftable.end(); )
        {
            if ((**it).IsSetData() && (**it).GetData().IsPub())
            {
                CRef<CSeqdesc> pub_desc(new CSeqdesc);
                pub_desc->SetPub((**it).SetData().SetPub());
                entry.SetDescr().Set().push_back(pub_desc);
                ftable.erase(it++);
                continue; // avoid iterator increment
            }

            if (!(**it).IsSetId())
               (**it).SetId().SetLocal().SetId(++id);
            it++;
        }
    }

    bool GetProteinName(string& protein_name, const CSeq_feat& feature)
    {
        if (feature.IsSetData())
        {
            if (feature.GetData().IsProt() &&
                feature.GetData().GetProt().IsSetName())
            {
                feature.GetData().GetProt().GetLabel(&protein_name);
                return true;
            }
        }

        if (feature.IsSetXref())
        {
            ITERATE(CSeq_feat_Base::TXref, xref_it, feature.GetXref())
            {
                if ((**xref_it).IsSetData())
                {
                    if ((**xref_it).GetData().IsProt() &&
                        (**xref_it).GetData().GetProt().IsSetName())
                    {
                        protein_name = (**xref_it).GetData().GetProt().GetName().front();
                        return true;
                    }
                }
            }
        }
        return false;
    }

    CRef<CSeq_id> GetNewProteinId(CSeq_entry_Handle seh, const string& id_base)
    {
        int offset = 1;
        string id_label = id_base + "_" + NStr::NumericToString(offset);
        CRef<CSeq_id> id(new CSeq_id());
        id->SetLocal().SetStr(id_label);
        CBioseq_Handle b_found = seh.GetBioseqHandle(*id);
        while (b_found) {
            offset++;
            id_label = id_base + "_" + NStr::NumericToString(offset);
            id->SetLocal().SetStr(id_label);
            b_found = seh.GetBioseqHandle(*id);
        }
        return id;
    }

    CRef<CSeq_id> GetNewProteinId(CSeq_entry_Handle seh, CBioseq_Handle bsh)
    {
        string id_base;
        CSeq_id_Handle hid;

        ITERATE(CBioseq_Handle::TId, it, bsh.GetId()) {
            if (!hid || !it->IsBetter(hid)) {
                hid = *it;
            }
        }

        return GetNewProteinId(seh, id_base);
    }

    string NewProteinName(const CSeq_feat& feature, CTempString locustag)
    {
        string protein_name;
        GetProteinName(protein_name, feature);

        if (protein_name.empty())
        {
            // find locus
            protein_name = locustag;
        }

        if (protein_name.empty())
            protein_name = "hypothetical protein";

        return protein_name;
    }

    CConstRef<CSeq_entry> LocateProtein(CConstRef<CSeq_entry> proteins, const CSeq_feat& feature)
    {
        if (proteins.NotEmpty())
        {
            const CSeq_id* id = feature.GetProduct().GetId();
            ITERATE(CSeq_entry::TSet::TSeq_set, it, proteins->GetSet().GetSeq_set())
            {
                ITERATE(CBioseq::TId, seq_it, (**it).GetSeq().GetId())
                {
                    if ((**seq_it).Compare(*id) == CSeq_id::e_YES)
                    {
                        return *it;
                    }
                }
            }
        }
        return CConstRef<CSeq_entry>();
    }


    CRef<CSeq_annot> FindORF(const CBioseq& bioseq)
    {
        if (bioseq.IsNa())
        {
            COrf::TLocVec orfs;
            CSeqVector  seq_vec(bioseq);
            COrf::FindOrfs(seq_vec, orfs);
            if (orfs.size()>0)
            {
                CRef<CSeq_id> seqid(new CSeq_id);
                seqid->Assign(*bioseq.GetId().begin()->GetPointerOrNull());
                COrf::TLocVec best;
                best.push_back(orfs.front());
                ITERATE(COrf::TLocVec, it, orfs)
                {
                    if ((**it).GetTotalRange().GetLength() >
                        best.front()->GetTotalRange().GetLength() )
                        best.front() = *it;
                }

                CRef<CSeq_annot> annot = COrf::MakeCDSAnnot(best, 1, seqid);
                return annot;
            }
        }
        return CRef<CSeq_annot>();
    }

    bool BioseqHasId(const CBioseq& seq, const CSeq_id* id)
    {
        if (id && seq.IsSetId())
        {
            ITERATE(CBioseq::TId, it, seq.GetId())
            {
                if (id->Compare(**it) == CSeq_id::e_YES)
                {
                    return true;
                }
            }
        }
        return false;
    }

    void MergeSeqIds(CBioseq& bioseq, CBioseq::TId& seq_ids)
    {
        ITERATE(CBioseq::TId, it, seq_ids)
        {
            if (!BioseqHasId(bioseq, *it))
            {
                bioseq.SetId().push_back(*it);
            }
        }
    }


    CSeq_feat& CreateOrSetFTable(CBioseq& bioseq)
    {
        CSeq_annot::C_Data::TFtable* ftable = 0;                    
        if (bioseq.IsSetAnnot())
        {
            NON_CONST_ITERATE(CBioseq::TAnnot, it, bioseq.SetAnnot())
            {
                if ((**it).IsFtable())
                {
                    ftable = &(**it).SetData().SetFtable();                    
                    break;
                }
            }
        }
        if (ftable == 0)
        {
            CRef<CSeq_annot> annot(new CSeq_annot);
            ftable = &annot->SetData().SetFtable();
            bioseq.SetAnnot().push_back(annot);
        }

        if (ftable->empty())
        {
            CRef<CSeq_feat> feat;
            feat.Reset(new CSeq_feat);
            ftable->push_back(feat);
            return *feat;
        }
        else
        {
            return *ftable->front();
        }
    }

}

CFeatureTableReader::CFeatureTableReader(IMessageListener* logger): m_logger(logger), m_local_id_counter(0)
{
}

CRef<CSeq_entry> CFeatureTableReader::TranslateProtein(CScope& scope, CSeq_entry_Handle top_entry_h, const CSeq_feat& feature, CTempString locustag)
{
    CConstRef<CSeq_entry> replacement = LocateProtein(m_replacement_protein, feature);
    CRef<CBioseq> protein;
    if (replacement.Empty())
    {
        protein = CSeqTranslator::TranslateToProtein(feature, scope);
        if (protein.Empty())
            return CRef<CSeq_entry>();
    }
    else
    {
        protein.Reset(new CBioseq());
        protein->Assign(replacement->GetSeq());
    }

    CRef<CSeq_entry> pentry(new CSeq_entry);
    pentry->SetSeq(*protein);

    CAutoAddDesc molinfo_desc(pentry->SetSeq().SetDescr(), CSeqdesc::e_Molinfo);
    molinfo_desc.Set().SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    molinfo_desc.Set().SetMolinfo().SetTech(CMolInfo::eTech_concept_trans);

    string org_name;
    GetOrgName(org_name, *top_entry_h.GetCompleteObject());

    string protein_name = NewProteinName(feature, locustag);
    string title = protein_name;
    if (org_name.length() > 0)
    {
        title += " [";
        title += org_name;
        title += "]";
    }

    CAutoAddDesc title_desc(pentry->SetSeq().SetDescr(), CSeqdesc::e_Title);
    title_desc.Set().SetTitle(title);

    string base_name;
    const string& protein_ids = feature.GetNamedQual("protein_id");

    if (protein_ids.empty())
    {
        if (feature.IsSetProduct())
        {
#if 0
            const CSeq_id* id = feature.GetProduct().GetId();
            if (id)
            {
                id->GetLabel(&base_name, CSeq_id::eContent);
            }
            CRef<CSeq_id> newid = GetNewProteinId(top_entry_h, base_name);
            protein->SetId().clear();
            protein->SetId().push_back(newid);
#else
            const CSeq_id* id = feature.GetProduct().GetId();
            if (!BioseqHasId(*protein, id)) // to avoid dupolicate ID creation
            {
                //id->GetLabel(&base_name, CSeq_id::eContent);
                CRef<CSeq_id> newid(new CSeq_id);
                newid->Assign(*id);
                protein->SetId().push_back(newid);
            }
#endif
        }
    }
    else
    {
        CBioseq::TId new_ids;
        CSeq_id::ParseIDs(new_ids, protein_ids, CSeq_id::fParse_ValidLocal | CSeq_id::fParse_PartialOK);
        MergeSeqIds(*protein, new_ids);
    }

    if (protein->GetId().empty())
    {
        CRef<CSeq_id> newid = GetNewProteinId(top_entry_h, base_name);
        protein->SetId().push_back(newid);
    }

    CSeq_feat* feat = &CreateOrSetFTable(*protein);

    CProt_ref::TName& prot_ref = feat->SetData().SetProt().SetName();
    if (prot_ref.empty())
        prot_ref.push_back(protein_name);

    //feat->SetLocation().Assign(feature.GetLocation());
    feat->SetId().SetLocal().SetId(++m_local_id_counter);
    feat->SetLocation().SetInt().SetFrom(0);       
    feat->SetLocation().SetInt().SetTo(protein->GetInst().GetLength()-1);
    //feat->SetLocation().SetInt().SetId().SetLocal().SetStr("debug");
    feat->SetLocation().SetInt().SetId().Assign(*protein->GetId().front());

    CBioseq_Handle bioseq_handle = scope.AddBioseq(*protein);

    return pentry;
}

void CFeatureTableReader::MergeCDSFeatures(CSeq_entry& entry)
{
    if (entry.IsSeq() && !entry.GetSeq().IsSetInst() )
        return;

    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        if (CheckIfNeedConversion(entry))
        {
            ConvertSeqIntoSeqSet(entry, true);
            ParseCdregions(entry);
        }
        break;
    case CSeq_entry::e_Set:
        if (entry.GetSet().IsSetClass())
        {
            switch (entry.GetSet().GetClass())
            {
                case CBioseq_set::eClass_nuc_prot:
                case CBioseq_set::eClass_gen_prod_set:
                    //ParseCdregions(entry);
                    return;
                case CBioseq_set::eClass_genbank:
                    break;
                default:
                    return;
            }
        }
        NON_CONST_ITERATE(CBioseq_set_Base::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            MergeCDSFeatures(**it);
        }
        break;
    default:
        break;
    }
}

CConstRef<CSeq_id> GetSeqIdWithoutVersion(const CSeq_id& id)
{
   const CTextseq_id* text_id = id.GetTextseq_Id();

   if (text_id && text_id->IsSetVersion())
   {
       CRef<CSeq_id> new_id(new CSeq_id);
       new_id->Assign(id);
       CTextseq_id* text_id = (CTextseq_id*) new_id->GetTextseq_Id();
       text_id->ResetVersion();
       return CConstRef<CSeq_id>(new_id);
   }

   return CConstRef<CSeq_id>(0);
}

void CFeatureTableReader::ReadFeatureTable(CSeq_entry& entry, ILineReader& line_reader)
{
    //CFeature_table_reader::ReadSequinFeatureTables(line_reader, entry, CFeature_table_reader::fCreateGenesFromCDSs, m_logger);

    // let's use map to speedup matching on very large files, see SQD-1847
    map<CConstRef<CSeq_id>, CRef<CBioseq>, SCSeqidCompare> seq_map;

    for (CTypeIterator<CBioseq> seqit(entry);  seqit;  ++seqit) {
        ITERATE (CBioseq::TId, seq_id_it, seqit->GetId()) 
        {
            CConstRef<CSeq_id> seq_id = *seq_id_it;
            seq_map[seq_id].Reset(&*seqit);

            CConstRef<CSeq_id> alt_id = GetSeqIdWithoutVersion(*seq_id);
            if (alt_id)
            {
                CRef<CBioseq>& alt_seq = seq_map[alt_id];
                if (alt_seq.Empty())
                  alt_seq.Reset(&*seqit);
            }

            //cout << "Found id: " << seq_id->AsFastaString() << endl;
        }
    }

    while ( !line_reader.AtEOF() ) {
        CRef<CSeq_annot> annot = CFeature_table_reader::ReadSequinFeatureTable(
            line_reader, CFeature_table_reader::fCreateGenesFromCDSs, m_logger, 0/*filter*/);

        if (annot.Empty() || !annot->IsSetData() || !annot->GetData().IsFtable() ||
            annot->GetData().GetFtable().empty()) {
            continue;
        }

        // otherwise, take the first feature, which should be representative
        const CSeq_feat& feat    = *annot->GetData().GetFtable().front();
        CConstRef<CSeq_id> feat_id(feat.GetLocation().GetId());

        _ASSERT(feat_id); // we expect a uniform sequence ID
        CRef<CBioseq> seq = seq_map[feat_id];
        if (seq.Empty())
        {
            CConstRef<CSeq_id> alt_id = GetSeqIdWithoutVersion(*feat_id);
            if (alt_id)
            {
                seq = seq_map[alt_id];
            }
        }

        if (seq.NotEmpty()) { // found a match
            seq->SetAnnot().push_back(annot);
        } else { // just package on the set
            //cout << "Not found id: " << feat_id->AsFastaString() << endl;
            /*
            ERR_POST_X(6, Warning
                       << "ReadSequinFeatureTables: unable to find match for "
                       << feat_id->AsFastaString());
            entry.SetSet().SetAnnot().push_back(annot);
            */
        }
    }

}

void CFeatureTableReader::FindOpenReadingFrame(CSeq_entry& entry) const
{
    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        {
        CRef<CSeq_annot> annot = FindORF(entry.SetSeq());
        if (annot.NotEmpty())
        {
            entry.SetSeq().SetAnnot().push_back(annot);
        }
        }
        break;
    case CSeq_entry::e_Set:
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            FindOpenReadingFrame(**it);
        }
        break;
    default:
        break;
    }
}

void CFeatureTableReader::ParseCdregions(CSeq_entry& entry)
{
    if (!entry.IsSet() ||
        entry.GetSet().GetClass() != CBioseq_set::eClass_nuc_prot)
        return;

    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    CSeq_entry_Handle entry_h = scope.AddTopLevelSeqEntry(entry);

    // Create empty annotation holding cdregion features
    CRef<CSeq_annot> set_annot(new CSeq_annot);
    CSeq_annot::TData::TFtable& set_ftable = set_annot->SetData().SetFtable();
    entry.SetSet().SetAnnot().push_back(set_annot);

    NON_CONST_ITERATE(CBioseq_set::TSeq_set, seq_it, entry.SetSet().SetSeq_set())
    {
        CRef<CSeq_entry> seq = *seq_it;
        if (!(seq->IsSeq() &&
            seq->GetSeq().IsSetInst() &&
            seq->GetSeq().IsNa() &&
            seq->GetSeq().IsSetAnnot() ))
            continue;

        for (CBioseq::TAnnot::iterator annot_it = seq->SetSeq().SetAnnot().begin();
            seq->SetSeq().SetAnnot().end() != annot_it;)
        {
            CRef<CSeq_annot> seq_annot(*annot_it);

            if (!seq_annot->IsFtable())
            {
                ++annot_it;
                continue;
            }
            CSeq_annot::TData::TFtable& seq_ftable = seq_annot->SetData().SetFtable();

            // sort and number ids
            PostProcessFeatureTable(entry, seq_annot->SetData().SetFtable(), m_local_id_counter);
            if (false)
            {
                CNcbiOfstream debug_annot("annot.sqn");
                debug_annot << MSerial_AsnText
                            << MSerial_VerifyNo
                            << *seq_annot;
            }
            CTempString locustag;
            for (CSeq_annot::TData::TFtable::iterator feat_it = seq_ftable.begin(); seq_ftable.end() != feat_it;)
            {
                CRef<CSeq_feat> feature = (*feat_it);
                if (!feature->IsSetData())
                {
                    ++feat_it; 
                    continue;
                }

                CSeqFeatData& data = feature->SetData();
#if 0
                if (data.IsGene())
                {
                    if (data.GetGene().IsSetLocus_tag())
                        locustag = data.GetGene().GetLocus_tag();
                }
                else
#endif
                if (data.IsCdregion())
                {
                    if (!data.GetCdregion().IsSetCode())
                       data.SetCdregion().SetCode().SetId(11); //???
                    if (!data.GetCdregion().IsSetFrame())
                       data.SetCdregion().SetFrame(CCdregion::eFrame_one); //???
                    CRef<CSeq_entry> protein = TranslateProtein(scope, entry_h, *feature, locustag);
                    locustag.clear();
                    if (protein.NotEmpty())
                    {
                        entry.SetSet().SetSeq_set().push_back(protein);
                        // move the cdregion into protein and step iterator to next
                        set_ftable.push_back(feature);
                        seq_ftable.erase(feat_it++);
                        continue; // avoid iterator increment
                    }
                }
                ++feat_it;
            }
            if (seq_annot->GetData().GetFtable().empty())
            {
                seq->SetSeq().SetAnnot().erase(annot_it++);
                continue;
            }
            ++annot_it;
        }

        if (seq->GetSeq().GetAnnot().empty())
        {
            seq->SetSeq().ResetAnnot();
        }
    }
    if (set_ftable.empty())
    {
        entry.SetSet().ResetAnnot();
    }

    scope.RemoveTopLevelSeqEntry(entry_h);
}

CRef<CSeq_entry> CFeatureTableReader::ReadProtein(ILineReader& line_reader)
{
    int flags = 0;
    flags |= CFastaReader::fAddMods
          |  CFastaReader::fNoUserObjs
          |  CFastaReader::fBadModThrow
          |  CFastaReader::fAssumeProt;

    auto_ptr<CFastaReader> pReader(new CFastaReader(0, flags));

    CRef<CSerialObject> pep = pReader->ReadObject(line_reader, m_logger);
    CRef<CSeq_entry> result;

    if (pep.NotEmpty())
    {
        if (pep->GetThisTypeInfo()->IsType(CSeq_entry::GetTypeInfo()))
        {
            result = (CSeq_entry*)(pep.GetPointerOrNull());
            if (result->IsSetDescr())
            {
                if (result->GetDescr().Get().size() == 0)
                {
                    result->SetDescr(*(CSeq_descr*)0);
                }
            }
            if (result->IsSeq())
            {
                // convert into seqset
                CRef<CSeq_entry> set(new CSeq_entry);
                set->SetSet().SetSeq_set().push_back(result);
                result = set;
            }
        }
    }

    return result;
}

void CFeatureTableReader::AddProteins(const CSeq_entry& possible_proteins, CSeq_entry& entry)
{
    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle tse = scope.AddTopLevelSeqEntry(entry);

    CBioseq_CI nuc_it(tse, CSeq_inst::eMol_na);
    {
        CSeq_entry_Handle h_entry = nuc_it->GetParentEntry(); 

        if (possible_proteins.IsSeq())
        {
            AddProteinToSeqEntry(&possible_proteins, h_entry);
        }
        else
        {
            ITERATE(CBioseq_set::TSeq_set, it, possible_proteins.GetSet().GetSeq_set())
            {
                AddProteinToSeqEntry(*it,  h_entry);
            }
        }
    }
}

bool CFeatureTableReader::CheckIfNeedConversion(const CSeq_entry& entry) const
{
    ITERATE(CSeq_entry::TAnnot, annot_it, entry.GetAnnot())
    {
        if ((**annot_it).IsFtable())
        {
            ITERATE(CSeq_annot::C_Data::TFtable, feat_it, (**annot_it).GetData().GetFtable())
            {
                if((**feat_it).CanGetData())
                {
                    switch ((**feat_it).GetData().Which())
                    {
                    case CSeqFeatData::e_Cdregion:
                        return true;
                    default:
                        break;
                    }
                }
            }
        }
    }

    return false;
}

void CFeatureTableReader::ConvertSeqIntoSeqSet(CSeq_entry& entry, bool nuc_prod_set) const
{
    if (entry.IsSeq())
    {
        CRef<CSeq_entry> newentry(new CSeq_entry);
        newentry->SetSeq(entry.SetSeq());
        CBioseq& bioseq = newentry->SetSeq();
        entry.SetSet().SetSeq_set().push_back(newentry);

        MoveSomeDescr(entry, bioseq);

        CAutoAddDesc molinfo_desc(bioseq.SetDescr(), CSeqdesc::e_Molinfo);

        if (!molinfo_desc.Set().SetMolinfo().IsSetBiomol())
            molinfo_desc.Set().SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);
        //molinfo_desc.Set().SetMolinfo().SetTech(CMolInfo::eTech_concept_trans);


        if (bioseq.IsSetInst() &&
            bioseq.IsNa() &&
            bioseq.IsSetInst() &&
            ! bioseq.GetInst().IsSetMol())
        {
            bioseq.SetInst().SetMol(CSeq_inst::eMol_dna);
        }
        entry.SetSet().SetClass(nuc_prod_set?CBioseq_set::eClass_nuc_prot:CBioseq_set::eClass_gen_prod_set);
        entry.Parentize();
    }
}

namespace {

void s_ExtendIntervalToEnd (CSeq_interval& ival, CBioseq_Handle bsh)
{
    if (ival.IsSetStrand() && ival.GetStrand() == eNa_strand_minus) {
        if (ival.GetFrom() > 3) {
            ival.SetFrom(ival.GetFrom() - 3);
        } else {
            ival.SetFrom(0);
        }
    } else {
        size_t len = bsh.GetBioseqLength();
        if (ival.GetTo() < len - 4) {
            ival.SetTo(ival.GetTo() + 3);
        } else {
            ival.SetTo(len - 1);
        }
    }
}

bool SetMolinfoCompleteness (CMolInfo& mi, bool partial5, bool partial3)
{
    bool changed = false;
    CMolInfo::ECompleteness new_val;
    if ( partial5  &&  partial3 ) {
        new_val = CMolInfo::eCompleteness_no_ends;
    } else if ( partial5 ) {
        new_val = CMolInfo::eCompleteness_no_left;
    } else if ( partial3 ) {
        new_val = CMolInfo::eCompleteness_no_right;
    } else {
        new_val = CMolInfo::eCompleteness_complete;
    }
    if (!mi.IsSetCompleteness() || mi.GetCompleteness() != new_val) {
        mi.SetCompleteness(new_val);
        changed = true;
    }
    return changed;
}


void SetMolinfoForProtein (CSeq_descr& protein_descr, bool partial5, bool partial3)
{
    CAutoAddDesc pdesc(protein_descr, CSeqdesc::e_Molinfo);
    pdesc.Set().SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    SetMolinfoCompleteness(pdesc.Set().SetMolinfo(), partial5, partial3);   
}

CRef<CSeq_feat> AddEmptyProteinFeatureToProtein (CRef<CSeq_entry> protein, bool partial5, bool partial3)
{
    CRef<CSeq_annot> ftable(NULL);
    NON_CONST_ITERATE(CSeq_entry::TAnnot, annot_it, protein->SetSeq().SetAnnot()) {
        if ((*annot_it)->IsFtable()) {
            ftable = *annot_it;
            break;
        }
    }
    if (!ftable) {
        ftable = new CSeq_annot();
        protein->SetSeq().SetAnnot().push_back(ftable);
    }
    
    CRef<CSeq_feat> prot_feat(NULL);
    NON_CONST_ITERATE(CSeq_annot::TData::TFtable, feat_it, ftable->SetData().SetFtable()) {
        if ((*feat_it)->IsSetData() && (*feat_it)->GetData().IsProt() && !(*feat_it)->GetData().GetProt().IsSetProcessed()) {
            prot_feat = *feat_it;
            break;
        }
    }
    if (!prot_feat) {
        prot_feat = new CSeq_feat();
        prot_feat->SetData().SetProt();
        ftable->SetData().SetFtable().push_back(prot_feat);
    }
    CRef<CSeq_id> prot_id(new CSeq_id());
    prot_id->Assign(*(protein->GetSeq().GetId().front()));
    prot_feat->SetLocation().SetInt().SetId(*prot_id);
    prot_feat->SetLocation().SetInt().SetFrom(0);
    prot_feat->SetLocation().SetInt().SetTo(protein->GetSeq().GetLength() - 1);
    prot_feat->SetLocation().SetPartialStart(partial5, eExtreme_Biological);
    prot_feat->SetLocation().SetPartialStop(partial3, eExtreme_Biological);
    if (partial5 || partial3) {
        prot_feat->SetPartial(true);
    } else {
        prot_feat->ResetPartial();
    }
    return prot_feat;
}

CRef<CSeq_feat> AddProteinFeatureToProtein (CRef<CSeq_entry> nuc, CConstRef<CSeq_loc> cds_loc, CRef<CSeq_entry> protein, bool partial5, bool partial3)
{
    CSourceModParser smp(CSourceModParser::eHandleBadMod_Ignore);
    // later - fix protein title by removing attributes used?
    CConstRef<CSeqdesc> title_desc = protein->GetSeq().GetClosestDescriptor(CSeqdesc::e_Title);
    if (title_desc) {
        string& title(const_cast<string&>(title_desc->GetTitle()));
        title = smp.ParseTitle(title, CConstRef<CSeq_id>(protein->GetSeq().GetFirstId()) );
        smp.ApplyAllMods(protein->SetSeq());
        smp.ApplyAllMods(nuc->SetSeq(), "", cds_loc);
    }

    return AddEmptyProteinFeatureToProtein(protein, partial5, partial3);
}

void AddSeqEntry(CSeq_entry_Handle m_SEH, CSeq_entry* m_Add)
{
    CSeq_entry_EditHandle eh = m_SEH.GetEditHandle();
    if (!eh.IsSet() && m_Add->IsSeq() && m_Add->GetSeq().IsAa()) {
        CBioseq_set_Handle nuc_parent = eh.GetParentBioseq_set();
        if (nuc_parent && nuc_parent.IsSetClass() && nuc_parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
            eh = nuc_parent.GetParentEntry().GetEditHandle();
        }
    }
    if (!eh.IsSet()) {
        eh.ConvertSeqToSet();
        if (m_Add->IsSeq() && m_Add->GetSeq().IsAa()) {
            // if adding protein sequence and converting to nuc-prot set, 
            // move all descriptors on nucleotide sequence except molinfo and title to set
            eh.SetSet().SetClass(CBioseq_set::eClass_nuc_prot);
            CConstRef<CBioseq_set> set = eh.GetSet().GetCompleteBioseq_set();
            if (set && set->IsSetSeq_set()) {
                CConstRef<CSeq_entry> nuc = set->GetSeq_set().front();
                CSeq_entry_EditHandle neh = m_SEH.GetScope().GetSeq_entryEditHandle(*nuc);
                CBioseq_set::TDescr::Tdata::const_iterator it = nuc->GetDescr().Get().begin();
                while (it != nuc->GetDescr().Get().end()) {
                    if (!(*it)->IsMolinfo() && !(*it)->IsTitle()) {
                        CRef<CSeqdesc> copy(new CSeqdesc());
                        copy->Assign(**it);
                        eh.AddSeqdesc(*copy);
                        neh.RemoveSeqdesc(**it);
                        it = nuc->GetDescr().Get().begin();
                    } else {
                        ++it;
                    }
                }
            }
        }
    }
    

    CSeq_entry_EditHandle added = eh.AttachEntry(*m_Add);
    /*int m_index = */ eh.GetSet().GetSeq_entry_Index(added);
}

void AddFeature(CSeq_entry_Handle m_seh, CSeq_feat* m_Feat)
{
    if (m_Feat->IsSetData() && m_Feat->GetData().IsCdregion() && m_Feat->IsSetProduct()) {
        CBioseq_Handle bsh = m_seh.GetScope().GetBioseqHandle(m_Feat->GetProduct());
        if (bsh) {
            CBioseq_set_Handle nuc_parent = bsh.GetParentBioseq_set();
            if (nuc_parent && nuc_parent.IsSetClass() && nuc_parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
                m_seh = nuc_parent.GetParentEntry();
            }
        }
    }
    CSeq_annot_Handle ftable;

    CSeq_annot_CI annot_ci(m_seh, CSeq_annot_CI::eSearch_entry);
    for (; annot_ci; ++annot_ci) {
        if ((*annot_ci).IsFtable()) {
            ftable = *annot_ci;
            break;
        }
    }

    CSeq_entry_EditHandle eh = m_seh.GetEditHandle();
    CSeq_feat_EditHandle  m_feh;
    CSeq_annot_EditHandle m_FTableCreated;

    if (!ftable) {
        CRef<CSeq_annot> new_annot(new CSeq_annot());
        ftable = m_FTableCreated = eh.AttachAnnot(*new_annot);
    }

    CSeq_annot_EditHandle aeh(ftable);
    m_feh = aeh.AddFeat(*m_Feat);
}


}

bool CFeatureTableReader::AddProteinToSeqEntry(const CSeq_entry* protein, CSeq_entry_Handle seh)
{
    CRef<CSeq_entry> nuc_entry((CSeq_entry*)seh.GetEditHandle().GetCompleteSeq_entry().GetPointerOrNull());

    CProSplign prosplign(CProSplignScoring(), false, true, false, false);

    // bool rval = false;
    CBioseq_Handle bsh_match;
    bool id_match = false;

    // only add protein if we can match it to a nucleotide sequence via the ID,
    // or if there is only one nucleotide sequence

    ITERATE (CBioseq::TId, id_it, protein->GetSeq().GetId()) {
        bsh_match = seh.GetScope().GetBioseqHandle(**id_it);
        if (bsh_match) {
            id_match = true;
            break;
        }
    }
    if (!bsh_match) {
        // if there is only one nucleotide sequence, we will use that one
        int nuc_count = 0;
        for (CBioseq_CI b_iter(seh, CSeq_inst::eMol_na); b_iter ; ++b_iter ) {
            bsh_match = *b_iter;
            nuc_count++;
            if (nuc_count > 1) {
                break;
            }
        }

        if (nuc_count == 0) {
            //wxMessageBox(wxT("You must import nucleotide sequences before importing protein sequences"), wxT("Error"),
            //             wxOK | wxICON_ERROR, NULL);
            return false;
        } else if (nuc_count > 1) {
            //wxMessageBox(wxT("If you have more than one nucleotide sequence, each protein sequence must use the ID of the nucleotide sequence where the coding region is found."), wxT("Error"),
            //             wxOK | wxICON_ERROR, NULL);
            return false;
        }
    }
            
    CRef<CSeq_id> seq_id(new CSeq_id());
    seq_id->Assign(*(bsh_match.GetSeqId()));
    CRef<CSeq_loc> match_loc(new CSeq_loc(*seq_id, 0, bsh_match.GetBioseqLength() - 1));

    CRef<CSeq_entry> protein_entry(new CSeq_entry());
    protein_entry->Assign(*protein);
    if (id_match) {
        CRef<CSeq_id> product_id = GetNewProteinId(seh, bsh_match);
        protein_entry->SetSeq().ResetId();
        protein_entry->SetSeq().SetId().push_back(product_id);
    }

    CSeq_entry_Handle protein_h = seh.GetScope().AddTopLevelSeqEntry(*protein_entry);

    //time_t t1 = time(NULL);
    CRef<CSeq_align> alignment = prosplign.FindAlignment(seh.GetScope(), *protein_entry->GetSeq().GetId().front(), *match_loc,
                                                     CProSplignOutputOptions(CProSplignOutputOptions::ePassThrough));
    //time_t t2 = time(NULL);
    //time_t elapsed = t2 - t1;
    CRef<CSeq_loc> cds_loc(new CSeq_loc());
    bool found_start_codon = false;
    bool found_stop_codon = false;
    if (alignment && alignment->IsSetSegs() && alignment->GetSegs().IsSpliced()) {
        CRef<CSeq_id> seq_id (new CSeq_id());
        seq_id->Assign(*match_loc->GetId());
        ITERATE (CSpliced_seg::TExons, exon_it, alignment->GetSegs().GetSpliced().GetExons()) {
            CRef<CSeq_loc> exon(new CSeq_loc(*seq_id, 
                                                      (*exon_it)->GetGenomic_start(), 
                                                      (*exon_it)->GetGenomic_end()));                
            if ((*exon_it)->IsSetGenomic_strand()) {
                exon->SetStrand((*exon_it)->GetGenomic_strand());
            }
            cds_loc->SetMix().Set().push_back(exon);
        }
        ITERATE (CSpliced_seg::TModifiers, mod_it,
                 alignment->GetSegs().GetSpliced().GetModifiers()) {
            if ((*mod_it)->IsStart_codon_found()) {
                found_start_codon = (*mod_it)->GetStart_codon_found();
            }
            if ((*mod_it)->IsStop_codon_found()) {
                found_stop_codon = (*mod_it)->GetStop_codon_found();
            }
        }
        
    }
    if (!cds_loc->IsMix()) {
        //no exons, no match
        string label = "";        
        protein->GetSeq().GetId().front()->GetLabel(&label, CSeq_id::eContent);
        string error = "Unable to find coding region location for protein sequence " + label + ".";
        m_logger->PutError(*auto_ptr<CLineError>(
            CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
            error)));
        return false;
    } else {
        if (cds_loc->GetMix().Get().size() == 1) {
            CRef<CSeq_loc> exon = cds_loc->SetMix().Set().front();
            cds_loc->Assign(*exon);
        }
    }
    if (!found_start_codon) {
        cds_loc->SetPartialStart(true, eExtreme_Biological);
    }
    if (found_stop_codon) {
        // extend to cover stop codon        
        if (cds_loc->IsMix()) {
            s_ExtendIntervalToEnd(cds_loc->SetMix().Set().back()->SetInt(), bsh_match);
        } else {
            s_ExtendIntervalToEnd(cds_loc->SetInt(), bsh_match);
        }        
    } else {
        cds_loc->SetPartialStop(true, eExtreme_Biological);
    }

    // if we add the protein sequence, we'll do it in the new nuc-prot set
    seh.GetScope().RemoveTopLevelSeqEntry(protein_h);
    bool partial5 = cds_loc->IsPartialStart(eExtreme_Biological);
    bool partial3 = cds_loc->IsPartialStop(eExtreme_Biological);
    SetMolinfoForProtein(protein_entry->SetDescr(), partial5, partial3);
    CRef<CSeq_feat> protein_feat = AddProteinFeatureToProtein(nuc_entry, cds_loc, protein_entry, partial5, partial3);

    AddSeqEntry(bsh_match.GetParentEntry(), protein_entry);
    CRef<CSeq_feat> new_cds(new CSeq_feat());
    new_cds->SetLocation(*cds_loc);
    if (partial5 || partial3) {
        new_cds->SetPartial(true);
    }
    new_cds->SetData().SetCdregion();
    CRef<CSeq_id> product_id(new CSeq_id());
    product_id->Assign(*(protein_entry->GetSeq().GetId().front()));
    new_cds->SetProduct().SetWhole(*product_id);
    AddFeature(seh, new_cds);

    string org_name;
    GetOrgName(org_name, *seh.GetCompleteObject());

    string protein_name = NewProteinName(*protein_feat, "");
    string title = protein_name;
    if (org_name.length() > 0)
    {
        title += " [";
        title += org_name;
        title += "]";
    }

    CAutoAddDesc title_desc(protein_entry->SetDescr(), CSeqdesc::e_Title);
    title_desc.Set().SetTitle() += title;

    return true;
}

void CFeatureTableReader::RemoveEmptyFtable(objects::CBioseq& bioseq)
{
    if (bioseq.IsSetAnnot())
    {
        for (CBioseq::TAnnot::iterator annot_it = bioseq.SetAnnot().begin(); annot_it != bioseq.SetAnnot().end(); ) // no ++
        {
            if ((**annot_it).IsFtable() && (**annot_it).GetData().GetFtable().empty())
            {
                annot_it = bioseq.SetAnnot().erase(annot_it);
            }
            else
                annot_it++;
        }

        if (bioseq.GetAnnot().empty())
        {
            bioseq.ResetAnnot();
        }
    }
}

CRef<CDelta_seq> CFeatureTableReader::MakeGap(objects::CBioseq_Handle bsh, const CSeq_feat& feature_gap)
{
    const string& sGT = feature_gap.GetNamedQual(kGapType_qual);
    const string& sLE = feature_gap.GetNamedQual(kLinkageEvidence_qual);

    TSeqPos gap_start(kInvalidSeqPos);
    TSeqPos gap_length(kInvalidSeqPos);

    CSeq_gap::EType gap_type = CSeq_gap::eType_unknown;
    CLinkage_evidence::EType evidence = (CLinkage_evidence::EType)-1; //CLinkage_evidence::eType_unspecified;

    if (!sGT.empty())
    {
        const CEnumeratedTypeValues::TNameToValue& 
            linkage_evidence_to_value_map = CLinkage_evidence::GetTypeInfo_enum_EType()->NameToValue();

        CEnumeratedTypeValues::TNameToValue::const_iterator it = linkage_evidence_to_value_map.find(CFastaReader::CanonicalizeString(sLE));
        if (it == linkage_evidence_to_value_map.end())
        {
            m_logger->PutError(*auto_ptr<CLineError>(
                CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                string("Unrecognized linkage evidence ") + sLE)));
            return CRef<CDelta_seq>(0);
        }
        else
        {
            evidence = (CLinkage_evidence::EType)it->second;
        }
    }

    if (!sLE.empty())
    {
        const CFastaReader::SGapTypeInfo * gap_type_info = 
            CFastaReader::NameToGapTypeInfo(sGT);

        if (gap_type_info)
        {
            gap_type = gap_type_info->m_eType;
            switch (gap_type_info->m_eType)
            {
            /// only the "unspecified" linkage-evidence is allowed
            case CFastaReader::eLinkEvid_UnspecifiedOnly:
                if (evidence != CLinkage_evidence::eType_unspecified)
                {
                    m_logger->PutError(*auto_ptr<CLineError>(
                        CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                        string("Linkage evidence must not be specified for ") + sGT)));
                    return CRef<CDelta_seq>(0);
                }
                break;
            /// no linkage-evidence is allowed
            case CFastaReader::eLinkEvid_Forbidden:
                if (evidence == CLinkage_evidence::eType_unspecified)
                {
                    m_logger->PutError(*auto_ptr<CLineError>(
                        CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                        string("Linkage evidence must be specified for ") + sGT)));
                    return CRef<CDelta_seq>(0);
                }
                break;
            /// any linkage-evidence is allowed, and at least one is required
            case CFastaReader::eLinkEvid_Required:
                break;
            default:
                break;
            }
        }
        else
        {
            m_logger->PutError(*auto_ptr<CLineError>(
                CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                string("Unrecognized gap type ") + sGT)));
            return CRef<CDelta_seq>(0);
        }
    }

    if (feature_gap.IsSetLocation() && feature_gap.GetLocation().IsInt())
    {
        gap_start = feature_gap.GetLocation().GetInt().GetFrom();
        gap_length = feature_gap.GetLocation().GetInt().GetTo();
        if (feature_gap.GetLocation().GetInt().IsSetStrand() && feature_gap.GetLocation().GetInt().GetStrand() == eNa_strand_minus)
        {
            swap(gap_start, gap_length);
        }
        gap_length -= gap_start;
        gap_length++;
    }


    return CGapsEditor::CreateGap((CBioseq&)*bsh.GetEditHandle().GetCompleteBioseq(), gap_start, gap_length, gap_type, evidence);
}

void CFeatureTableReader::MakeGapsFromFeatures(CSeq_entry_Handle seh)
{
    for (CBioseq_CI bioseq_it(seh); bioseq_it; ++bioseq_it)
    {
        {
            SAnnotSelector annot_sel(CSeqFeatData::e_Imp);           
            for (CFeat_CI feature_it(*bioseq_it, annot_sel); feature_it; ) // no ++
            {
                if (feature_it->IsSetData() && feature_it->GetData().IsImp())
                {
                    const CImp_feat& imp = feature_it->GetData().GetImp();
                    if (imp.IsSetKey() && imp.GetKey() == kAssemblyGap_feature)
                    {
                        // removing feature
                        const CSeq_feat& feature_gap = feature_it->GetOriginalFeature();
                        CSeq_feat_EditHandle to_remove(*feature_it);
                        ++feature_it;
                        try
                        {
                            CRef<CDelta_seq> gap = MakeGap(*bioseq_it, feature_gap);
                            if (gap.Empty())
                            {
                                m_logger->PutError(*auto_ptr<CLineError>(
                                    CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                                    "Failed to convert feature gap into a gap")));
                            }
                            else
                            {
                                to_remove.Remove();
                            }
                        }
                        catch(const CException& ex)
                        {
                            m_logger->PutError(*auto_ptr<CLineError>(
                                CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                                ex.GetMsg())));
                        }
                        continue;
                    }
                }
                ++feature_it;
            };
        }

        CBioseq& bioseq = (CBioseq&)*bioseq_it->GetEditHandle().GetCompleteBioseq();
        RemoveEmptyFtable(bioseq);

    }
}



END_NCBI_SCOPE

