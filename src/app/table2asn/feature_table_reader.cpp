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

#include "feature_table_reader.hpp"

#include <objtools/readers/fasta.hpp>

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
        if (entry.IsSet())
        {
            ITERATE(CSeq_descr_Base::Tdata, it, entry.GetSet().GetDescr().Get())
            {
                if ((**it).IsOrg())
                {
                    if ((**it).GetOrg().IsSetOrgname())
                    {
                        if ((**it).GetOrg().GetOrgname().GetFlatName(name))
                            return true;
                    }
                }
                if ((**it).IsSource())
                {
                    if ((**it).GetSource().IsSetOrgname())
                    {
                        if ((**it).GetSource().GetOrgname().GetFlatName(name))
                            return true;
                    }
                    if ((**it).GetSource().IsSetOrg())
                    {
                        if ((**it).GetSource().GetOrg().GetOrgname().GetFlatName(name))
                            return true;
                    }
                    if ((**it).GetSource().IsSetTaxname())
                    {
                        name = (**it).GetSource().GetTaxname();
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
  

    CRef<CSeq_entry> TranslateProtein(CScope& scope, CSeq_entry_Handle top_entry_h, const CSeq_feat& feature, CTempString locustag, CConstRef<CSeq_entry> proteins)
    {       
        CRef<CSeq_entry> pentry = LocateProtein(proteins, feature);
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
            protein_name = "hypothetical protein";

        if (!locustag.empty())
        {
            protein_name += " ";
            protein_name += locustag;
        }

#if 0
        CRef<CSeq_annot> annot(new CSeq_annot);
        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetData().SetProt().SetName().push_back(protein_name);
        //feat->SetLocation().set
        //Assign(feature.GetLocation());
        annot->SetData().SetFtable().push_back(feat);
        pentry->SetSeq().SetAnnot().push_back(annot);
#endif

        if (org_name.length() > 0)
            protein_name += " [" + org_name + "]";

        CRef<CSeqdesc> title_desc(new CSeqdesc);
        title_desc->SetTitle(protein_name);
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

        CBioseq_Handle bioseq_handle = scope.AddBioseq(*protein);

        return pentry;
    }

    void ConvertSeqIntoSeqSet(CSeq_entry& entry)
    {
        CRef<CSeq_entry> newentry(new CSeq_entry);
        newentry->SetSeq(entry.SetSeq());

        entry.SetSet().SetSeq_set().push_back(newentry);
        MoveSomeDescr(entry, newentry->SetSeq());
        entry.SetSet().SetClass(CBioseq_set::eClass_nuc_prot);

        CRef<CSeqdesc> molinfo_desc;
        NON_CONST_ITERATE(CSeq_descr::Tdata, it, newentry->SetDescr().Set())
        {
            if ((**it).IsMolinfo())
            {
                molinfo_desc = *it;
                break;
            }
        }

        if (molinfo_desc.Empty())
        {
           molinfo_desc.Reset(new CSeqdesc);
           newentry->SetDescr().Set().push_back(molinfo_desc);
        }
        //molinfo_desc->SetMolinfo().SetTech(CMolInfo::eTech_concept_trans);
        if (!molinfo_desc->SetMolinfo().IsSetBiomol())
            molinfo_desc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);

        if (newentry->GetSeq().IsNa() &&
            newentry->GetSeq().IsSetInst())
        {
            newentry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
        }
    }

    bool CheckIfNeedConversion(const CSeq_entry& entry)
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

void CFeatureTableReader::MergeCDSFeatures(CSeq_entry& entry)
{
    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        if (CheckIfNeedConversion(entry))
        {
            ConvertSeqIntoSeqSet(entry);
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
    CFeature_table_reader::ReadSequinFeatureTables(line_reader, entry, CFeature_table_reader::fCreateGenesFromCDSs, m_logger);
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
    //for (CBioseq_set::TSeq_set::iterator seq_it = entry.SetSet().SetSeq_set().begin();
    //    entry.SetSet().SetSeq_set().end() != seq_it;)
    {
        CRef<CSeq_entry> seq = *seq_it;
        if (seq->IsSeq() &&
            seq->GetSeq().IsSetInst() &&
            seq->GetSeq().IsNa() && 
            seq->GetSeq().IsSetAnnot() )
        {
            for (CBioseq::TAnnot::iterator annot_it = seq->SetSeq().SetAnnot().begin(); 
                seq->SetSeq().SetAnnot().end() != annot_it;)
            {
                CRef<CSeq_annot> seq_annot(*annot_it);

                if (seq_annot->IsFtable())
                {
                    CTempString locustag;
                    for (CSeq_annot::TData::TFtable::iterator feat_it = seq_annot->SetData().SetFtable().begin();
                        seq_annot->SetData().SetFtable().end() != feat_it;)
                    {
                        CRef<CSeq_feat> feature = (*feat_it);
                        if (feature->IsSetData())
                        {
                            CSeqFeatData& data = feature->SetData();
                            if (data.IsGene())
                            {
                                if (data.GetGene().IsSetLocus_tag())
                                    locustag = data.GetGene().GetLocus_tag();
                            }
                            else
                            if (data.IsCdregion())
                            {
                                CRef<CSeq_entry> protein = TranslateProtein(scope, entry_h, *feature, locustag, m_replacement_protein);
                                locustag.clear();
                                if (protein.NotEmpty())
                                {
                                    entry.SetSet().SetSeq_set().push_back(protein);
                                    // move the cdregion into protein and step iterator to next
                                    set_annot->SetData().SetFtable().push_back(feature);
                                    seq_annot->SetData().SetFtable().erase(feat_it++);
                                    continue; // avoid iterator increment
                                }
                            }
                            else
                            if (data.IsPub())
                            {
                                CRef<CSeqdesc> seqdesc(new CSeqdesc);
                                seqdesc->SetPub(data.SetPub());
                                entry.SetDescr().Set().push_back(seqdesc);
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
                        
                    }
                    ++annot_it;
                }                 
            if (seq->GetSeq().GetAnnot().empty())
            {
                seq->SetSeq().ResetAnnot();
            }
            }
    }
    entry.Parentize();
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

    CProSplign aligner;  
    ITERATE(CSeq_entry::TSet::TSeq_set, prot_it, possible_proteins.GetSet().GetSeq_set())
    {
        scope.AddBioseq((**prot_it).GetSeq());
        CBioseq_CI bio_it(h_entry, CSeq_inst::eMol_na);
        //CSeq_entry_CI seq_it(h_entry, CSeq_entry_CI::fRecursive, CSeq_entry::e_Seq);
        //for (; seq_it; ++seq_it)
        for (; bio_it; ++bio_it)
        {
            CRef<CSeq_id> protein((**prot_it).GetSeq().GetId().front());
            CRef<CSeq_id> seq_id(new CSeq_id);
            seq_id->Assign(*bio_it->GetBioseqCore()->GetId().front());
                //bio_it->GetId().front().GetSeqId());
                //seq_it->GetSeq_entryCore()->GetSeq().GetId().front());  
            CRef<CSeq_loc> genomic(new CSeq_loc);
            genomic->SetWhole(*seq_id);
            genomic->SetId(*seq_id);
            try
            {
                CProSplignOutputOptions opts;

                CRef<CSeq_align> align = aligner.FindAlignment(scope, *protein, *genomic, opts);

                /*
                string name;
                genomic->GetId()->GetLabel(&name);

                CNcbiOfstream ostr((name + ".align").c_str());
                ostr << MSerial_AsnText 
                    << MSerial_VerifyNo;
                ostr << *align;
                */
            }
            catch(CException& )
            {
            }
        }
    }

    /*
    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        if (entry.GetSeq().IsNa() && entry.GetSeq().IsSetInst())
        {
        }
        break;
    case CSeq_entry::e_Set:
        NON_CONST_ITERATE(CBioseq_set_Base::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            AddProteins(possible_proteins, **it);
        }
        break;
    default:
        break;
    }
*/
}

END_NCBI_SCOPE

