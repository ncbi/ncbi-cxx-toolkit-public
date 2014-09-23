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

#include "feature_table_reader.hpp"

#include "table2asn_context.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace
{

    void MoveSomeDescr(CSeq_entry& dest, CBioseq& src)
    {
        CSeq_descr::Tdata::iterator it = src.SetDescr().Set().begin();

        while(it != src.SetDescr().Set().end())
        {
            switch ((**it).Which())
            {
            case CSeqdesc_Base::e_Pub:
            case CSeqdesc_Base::e_Source:
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
                CRef<CSeqdesc> seqdesc(new CSeqdesc);
                seqdesc->SetPub((**it).SetData().SetPub());
                entry.SetDescr().Set().push_back(seqdesc);
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
        if (feature.IsSetXref())
        {
            ITERATE(CSeq_feat_Base::TXref, xref_it, feature.GetXref())
            {
                if ((**xref_it).IsSetData())
                {
                    if ((**xref_it).GetData().IsProt())
                    {
                        protein_name = (**xref_it).GetData().GetProt().GetName().front();
                        return true;
                    }
                }
            }
        }
        return false;
    }

    CRef<objects::CSeq_id>
        GetNewProteinId(objects::CSeq_entry_Handle seh, const string& id_base)
    {
#if 0
        string id_base;
        objects::CSeq_id_Handle hid;

        ITERATE(objects::CBioseq_Handle::TId, it, bsh.GetId()) {
            if (!hid || !it->IsBetter(hid)) {
                hid = *it;
            }
        }

        hid.GetSeqId()->GetLabel(&id_base, objects::CSeq_id::eContent);
#endif

        int offset = 1;
        string id_label = id_base + "_" + NStr::NumericToString(offset);
        CRef<objects::CSeq_id> id(new objects::CSeq_id());
        id->SetLocal().SetStr(id_label);
        objects::CBioseq_Handle b_found = seh.GetBioseqHandle(*id);
        while (b_found) {
            offset++;
            id_label = id_base + "_" + NStr::NumericToString(offset);
            id->SetLocal().SetStr(id_label);
            b_found = seh.GetBioseqHandle(*id);
        }
        return id;
    }

    CTempString GetQual(const CSeq_feat& feature, const string& qual)
    {
        ITERATE(CSeq_feat::TQual, it, feature.GetQual())
        {
            if (0 == NStr::Compare((**it).GetQual(), qual))
            {
                return (**it).GetVal();
            }
        }
        return CTempString();
    }

    CRef<CSeq_entry> LocateProtein(CConstRef<CSeq_entry> proteins, const CSeq_feat& feature)
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
                        CRef<CSeq_entry> result(new CSeq_entry);
                        result->Assign(**it);
                        return result;
                    }
                }
            }
        }
        return CRef<CSeq_entry>();
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
}

CFeatureTableReader::CFeatureTableReader(objects::IMessageListener* logger): m_logger(logger), m_local_id_counter(0)
{
}


CRef<CSeq_entry> CFeatureTableReader::TranslateProtein(CScope& scope, CSeq_entry_Handle top_entry_h, const CSeq_feat& feature, CTempString locustag)
{
    CRef<CSeq_entry> pentry = LocateProtein(m_replacement_protein, feature);
    if (pentry.NotEmpty())
        return pentry;

    CRef<CBioseq> protein = CSeqTranslator::TranslateToProtein(feature, scope);

    if (protein.Empty())
        return CRef<CSeq_entry>();

    pentry.Reset(new CSeq_entry);
    pentry->SetSeq(*protein);

    string org_name;

    GetOrgName(org_name, *top_entry_h.GetCompleteObject());

    CRef<CSeqdesc> molinfo_desc(new CSeqdesc);
    molinfo_desc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    molinfo_desc->SetMolinfo().SetTech(CMolInfo::eTech_concept_trans);
    pentry->SetSeq().SetDescr().Set().push_back(molinfo_desc);

    string protein_name;
    GetProteinName(protein_name, feature);

    if (protein_name.empty())
        protein_name = locustag;

    if (protein_name.empty())
        protein_name = "hypothetical protein";

    string title = protein_name;
    if (org_name.length() > 0)
        title += " [" + org_name + "]";

    CRef<CSeqdesc> title_desc(new CSeqdesc);
    title_desc->SetTitle(title);
    pentry->SetSeq().SetDescr().Set().push_back(title_desc);

    string base_name;
    CTempString protein_ids = GetQual(feature, "protein_id");

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
            if (id)
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
        CSeq_id::ParseIDs(protein->SetId(), protein_ids,
                CSeq_id::fParse_ValidLocal
            | CSeq_id::fParse_PartialOK);
    }

    if (protein->GetId().empty())
    {
        CRef<CSeq_id> newid = GetNewProteinId(top_entry_h, base_name);
        protein->SetId().push_back(newid);
    }

#if 1
    CRef<CSeq_annot> annot(new CSeq_annot);
    CRef<CSeq_feat> feat(new CSeq_feat);
    feat->SetData().SetProt().SetName().push_back(protein_name);
    //feat->SetLocation().Assign(feature.GetLocation());
    feat->SetId().SetLocal().SetId(++m_local_id_counter);
    feat->SetLocation().SetInt().SetFrom(0);       
    feat->SetLocation().SetInt().SetTo(protein->GetInst().GetLength()-1);
    //feat->SetLocation().SetInt().SetId().SetLocal().SetStr("debug");
    feat->SetLocation().SetInt().SetId().Assign(*protein->GetId().front());
    annot->SetData().SetFtable().push_back(feat);
    pentry->SetSeq().SetAnnot().push_back(annot);
#endif

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
        NON_CONST_ITERATE(CBioseq_set_Base::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            MergeCDSFeatures(**it);
        }
        break;
    default:
        break;
    }
}

void CFeatureTableReader::ReadFeatureTable(CSeq_entry& entry, ILineReader& line_reader)
{
    //CFeature_table_reader::ReadSequinFeatureTables(line_reader, entry, CFeature_table_reader::fCreateGenesFromCDSs, m_logger);

    // let's use map to speedup matching on very large files, see SQD-1847
    map<const CSeq_id*, CRef<CBioseq>, SCSeqidCompare> seq_map;

    for (CTypeIterator<CBioseq> seqit(entry);  seqit;  ++seqit) {
        ITERATE (CBioseq::TId, seq_id, seqit->GetId()) {
            seq_map[seq_id->GetPointer()].Reset(&*seqit);
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
        const CSeq_id*   feat_id = feat.GetLocation().GetId();
        CBioseq*         seq     = NULL;
        _ASSERT(feat_id); // we expect a uniform sequence ID
        seq = seq_map[feat_id].GetPointer();
        if (seq) { // found a match
            if (false)
            {
                CNcbiOfstream debug_annot("annot.sqn");
                debug_annot << MSerial_AsnText
                            << MSerial_VerifyNo
                            << *annot;
            }
            seq->SetAnnot().push_back(annot);
        } else { // just package on the set
            /*
            ERR_POST_X(6, Warning
                       << "ReadSequinFeatureTables: unable to find match for "
                       << feat_id->AsFastaString());
            entry.SetSet().SetAnnot().push_back(annot);
            */
        }
    }

}

void CFeatureTableReader::FindOpenReadingFrame(objects::CSeq_entry& entry) const
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

CRef<objects::CSeq_entry> CFeatureTableReader::ReadReplacementProtein(ILineReader& line_reader)
{
    int flags = 0;
    flags |= CFastaReader::fAddMods
          |  CFastaReader::fNoUserObjs
          |  CFastaReader::fBadModThrow
          |  CFastaReader::fAssumeProt;

    auto_ptr<CFastaReader> pReader(new CFastaReader(0, flags));

    CRef<CSerialObject> pep = pReader->ReadObject(line_reader, m_logger);
    CRef<objects::CSeq_entry> result;

    if (pep.NotEmpty())
    {
        if (pep->GetThisTypeInfo()->IsType(CSeq_entry::GetTypeInfo()))
        {
            result = (CSeq_entry*)(pep.GetPointerOrNull());
        }
    }

    return result;
}

void CFeatureTableReader::ParseCdregions(CSeq_entry& entry)
{
    if (!entry.IsSet() ||
        entry.GetSet().GetClass() != CBioseq_set::eClass_nuc_prot)
        return;

    CRef<CObjectManager> mgr(CObjectManager::GetInstance());

    CScope scope(*mgr);
    CSeq_entry_Handle entry_h = scope.AddTopLevelSeqEntry(entry);

    // Create empty annotation holding cdregion features
    CRef<CSeq_annot> set_annot(new CSeq_annot);
    set_annot->SetData().SetFtable();
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

            // sort and number ids
            PostProcessFeatureTable(entry, seq_annot->SetData().SetFtable(), m_local_id_counter);

            CTempString locustag;
            for (CSeq_annot::TData::TFtable::iterator feat_it = seq_annot->SetData().SetFtable().begin();
                seq_annot->SetData().SetFtable().end() != feat_it;)
            {
                CRef<CSeq_feat> feature = (*feat_it);
                if (!feature->IsSetData())
                {
                    ++feat_it; 
                    continue;
                }

                CSeqFeatData& data = feature->SetData();
                if (data.IsGene())
                {
                    if (data.GetGene().IsSetLocus_tag())
                        locustag = data.GetGene().GetLocus_tag();
                }
                else
                if (data.IsCdregion())
                {
                    data.SetCdregion().SetCode().SetId(11); //???
                    data.SetCdregion().SetFrame(CCdregion::eFrame_one); //???
                    CRef<CSeq_entry> protein = TranslateProtein(scope, entry_h, *feature, locustag);
                    locustag.clear();
                    if (protein.NotEmpty())
                    {
                        //data.SetCdregion().re
                        entry.SetSet().SetSeq_set().push_back(protein);
                        // move the cdregion into protein and step iterator to next
                        set_annot->SetData().SetFtable().push_back(feature);
                        seq_annot->SetData().SetFtable().erase(feat_it++);
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
}

CRef<objects::CSeq_entry> CFeatureTableReader::ReadProtein(ILineReader& line_reader)
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
    CSeq_entry_Handle h_entry = scope.AddTopLevelSeqEntry(entry);

    //CCompartmentFinder c_finder(;

    CBioseq_CI bio_it(h_entry, CSeq_inst::eMol_na);
    //CSeq_entry_CI seq_it(h_entry, CSeq_entry_CI::fRecursive, CSeq_entry::e_Seq);
    //for (; seq_it; ++seq_it)
    for (; bio_it; ++bio_it)
    {
        const CBioseq_Handle& bs = *bio_it;
        CRef<CSeq_loc> nucloc( bs.GetRangeSeq_loc (0, bs.GetInst_Length()));

        ITERATE(CSeq_entry::TSet::TSeq_set, prot_it, possible_proteins.GetSet().GetSeq_set())
        {
            CBioseq_Handle protein_h = scope.AddBioseq((**prot_it).GetSeq());
            try
            {
                CProSplignOutputOptions opts;

                CProSplign aligner;
                CRef<CSeq_align> alignment = aligner.FindAlignment(scope, *protein_h.GetSeqId(), *nucloc, opts);

#if 1
                cout << MSerial_AsnText << *alignment << endl;
#else
                string name;
                genomic->GetId()->GetLabel(&name);
                NStr::ReplaceInPlace(name, "|", "_");

                CNcbiOfstream ostr((name + ".align").c_str());
                ostr << MSerial_AsnText
                    << MSerial_VerifyNo;
                ostr << *align;
#endif
            }
            catch(CException& )
            {
            }
            scope.RemoveBioseq(protein_h);
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
            bioseq.IsSetInst())
        {
            bioseq.SetInst().SetMol(CSeq_inst::eMol_dna);
        }
        entry.SetSet().SetClass(nuc_prod_set?CBioseq_set::eClass_nuc_prot:CBioseq_set::eClass_gen_prod_set);
        entry.Parentize();
    }
}




END_NCBI_SCOPE

