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
* Authors:  Justin Foley
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/logging/message.hpp>
#include <objtools/logging/listener.hpp>
#include <objtools/readers/mod_reader.hpp>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <objtools/readers/mod_reader.hpp>
#include <objtools/readers/mod_error.hpp>
#include "feature_mod_apply.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CFeatModApply::CFeatModApply(CBioseq& bioseq,
        FReportError fReportError,
        TSkippedMods& skipped_mods) :
    m_Bioseq(bioseq),
    m_fReportError(fReportError),
    m_SkippedMods(skipped_mods) {}


CFeatModApply::~CFeatModApply() {}


bool CFeatModApply::Apply(const TModEntry& mod_entry)
{
    // A nucleotide sequence cannot have protein feature annotations
    static unordered_set<string> protein_quals = {"protein-desc", "protein", "ec-number", "activity"};
    if (m_Bioseq.IsNa() &&
        protein_quals.find(x_GetModName(mod_entry)) != protein_quals.end()) {


        if (m_fReportError) {
            for (const auto& mod_data : mod_entry.second) {

                string msg = "Cannot apply protein modifier to nucleotide sequence. The following modifier will be ignored: " + mod_data.GetName();

                m_fReportError(mod_data, msg, eDiag_Warning, eModSubcode_ProteinModOnNucseq);
                for (const auto& mod_data : mod_entry.second) {
                    m_SkippedMods.push_back(mod_data);
                }
                return true;
            }
        }

        set<string> qual_names;
        for (const auto& mod_data : mod_entry.second) {
            qual_names.insert(mod_data.GetName());
        }
        string name_string;
        bool first_name = true;
        for (const auto& name : qual_names) {
            if (first_name) {
                first_name = false;
            }
            else {
                name_string += ", ";
            }
            name_string  += name;
        }
        string msg = "Cannot apply protein modifier to nucleotide sequence. The following modifiers will be ignored: "
                   + name_string + ".";

        NCBI_THROW(CModReaderException, eInvalidModifier, msg);
    }

    if (x_TryProtRefMod(mod_entry)) {
        return true;
    }

    return false;
}


bool CFeatModApply::x_TryProtRefMod(const TModEntry& mod_entry)
{
    const auto& mod_name = x_GetModName(mod_entry);

    if (mod_name == "protein-desc") {
        const auto& value = x_GetModValue(mod_entry);
        x_SetProtein().SetData().SetProt().SetDesc(value);
        return true;
    }

    if (mod_name == "protein") {
        CProt_ref::TName names;
        for (const auto& mod : mod_entry.second) {
            names.push_back(mod.GetValue());
        }
        x_SetProtein().SetData().SetProt().SetName() = move(names);
        return true;
    }


    if (mod_name == "ec-number") {
        CProt_ref::TEc ec_numbers;
        for (const auto& mod : mod_entry.second) {
            ec_numbers.push_back(mod.GetValue());
        }
        x_SetProtein().SetData().SetProt().SetEc() = move(ec_numbers);
        return true;
    }


    if (mod_name == "activity") {
        CProt_ref::TActivity activity;
        for (const auto& mod : mod_entry.second) {
            activity.push_back(mod.GetValue());
        }
        x_SetProtein().SetData().SetProt().SetActivity() = move(activity);
        return true;
    }
    return false;
}


CSeq_feat& CFeatModApply::x_SetProtein(void)
{

    if (!m_pProtein) {
        m_pProtein = x_FindSeqfeat([](const CSeq_feat& seq_feat)
                { return (seq_feat.IsSetData() &&
                          seq_feat.GetData().IsProt()); });

        if (!m_pProtein) {
            auto pFeatLoc = x_GetWholeSeqLoc();
            m_pProtein =
                x_CreateSeqfeat([]() {
                    auto pData = Ref(new CSeqFeatData());
                    pData->SetProt();
                    return pData;
                    },
                    *pFeatLoc);
        }
    }

    return *m_pProtein;
}


const string& CFeatModApply::x_GetModName(const TModEntry& mod_entry)
{
    return CModHandler::GetCanonicalName(mod_entry);
}


const string& CFeatModApply::x_GetModValue(const TModEntry& mod_entry)
{
    return CModHandler::AssertReturnSingleValue(mod_entry);
}


CRef<CSeq_loc> CFeatModApply::x_GetWholeSeqLoc(void)
{
    auto pSeqLoc = Ref(new CSeq_loc());
    auto pSeqId = FindBestChoice(m_Bioseq.GetId(), CSeq_id::BestRank);
    if (pSeqId) {
        pSeqLoc->SetWhole(*pSeqId);
    }
    return pSeqLoc;
}


CRef<CSeq_feat> CFeatModApply::x_FindSeqfeat(FVerifyFeat fVerifyFeature)
{
    if (m_Bioseq.IsSetAnnot()) {
        for (auto& pAnnot : m_Bioseq.SetAnnot()) {
            if (pAnnot && pAnnot->IsFtable()) {
                for (auto pSeqfeat : pAnnot->SetData().SetFtable()) {
                    if (pSeqfeat &&
                        fVerifyFeature(*pSeqfeat)) {
                        return pSeqfeat;
                    }
                }
            }
        }
    }
    return CRef<CSeq_feat>();
}


CRef<CSeq_feat> CFeatModApply::x_CreateSeqfeat(
        FCreateFeatData fCreateFeatData,
        const CSeq_loc& feat_loc)
{
    auto pSeqfeat = Ref(new CSeq_feat());
    pSeqfeat->SetData(*fCreateFeatData());
    pSeqfeat->SetLocation(const_cast<CSeq_loc&>(feat_loc));

    auto pAnnot = Ref(new CSeq_annot());
    pAnnot->SetData().SetFtable().push_back(pSeqfeat);
    m_Bioseq.SetAnnot().push_back(pAnnot);

    return pSeqfeat;
}

END_SCOPE(objects)
END_NCBI_SCOPE
