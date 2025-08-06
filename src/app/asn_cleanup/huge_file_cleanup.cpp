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
* Author:  Justin Foley
* File Description:
*
*/
#include <ncbi_pch.hpp>

#include <objtools/edit/huge_file_process.hpp>
#include <objtools/cleanup/cleanup_change.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objtools/cleanup/fix_feature_id.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objtools/readers/objhook_lambdas.hpp>
#include <serial/objistr.hpp>
#include <serial/streamiter.hpp>

//#include "newcleanupp.hpp"
#include <objtools/cleanup/influenza_set.hpp>
#include "huge_file_cleanup.hpp"

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
USING_SCOPE(edit);


CCleanupHugeAsnReader::CCleanupHugeAsnReader(TOptions options)
: m_CleanupOptions(options) {}


const CCleanupChangeCore& CCleanupHugeAsnReader::GetChanges() const
{
    return m_Changes;
}


void CCleanupHugeAsnReader::FlattenGenbankSet()
{
    auto& biosets = m_bioseq_set_list;
    if (biosets.size()>1) {
        auto firstTrueBioset = next(biosets.begin());
        if (firstTrueBioset->m_class == CBioseq_set::eClass_not_set) {
            if (x_LooksLikeNucProtSet()) {
                firstTrueBioset->m_class = CBioseq_set::eClass_nuc_prot;
            }
            else {
                firstTrueBioset->m_class = CBioseq_set::eClass_genbank;
            }
            m_Changes.SetChanged(CCleanupChange::eChangeBioseqSetClass);
        }
    }
    TParent::FlattenGenbankSet();
    x_CleanupTopLevelDescriptors();

    if (m_CleanupOptions & eEnableSmallGenomeSets) {
        x_CreateSmallGenomeSets();
        x_PruneAndReorderTopIds();
    }
}

void CCleanupHugeAsnReader::x_PruneAndReorderTopIds()
{ // Needed when wrapping influenza sequences in small-genome sets
    map<string, CConstRef<CSeq_id>> smallGenomeLabelToId;

    auto it = m_top_ids.begin();
    while (it != m_top_ids.end()) {
        if (auto mit = x_GetFluLabel(*it);
            mit != m_IdToFluLabel.end()) {
            if (smallGenomeLabelToId.find(mit->second)
                == smallGenomeLabelToId.end()) {
                smallGenomeLabelToId.emplace(mit->second, *it);
            }
            it = m_top_ids.erase(it);
            continue;
        }
        ++it;
    }

    for (auto entry : smallGenomeLabelToId) {
        m_top_ids.push_back(entry.second);
    }
}


bool CCleanupHugeAsnReader::x_LooksLikeNucProtSet() const
{
    int numNucs{0};
    int numProts{0};
    for (const auto& bioseq : GetBioseqs()) {
        if (CSeq_inst::IsNa(bioseq.m_mol)) {
            ++numNucs;
            if (numNucs>1) {
                return false;
            }
        }
        else if (CSeq_inst::IsAa(bioseq.m_mol)) {
            ++numProts;
        }
    }
    if (numProts==0) {
        return false;
    }

    _ASSERT(m_bioseq_set_list.size()>1);
    // check for invalid subsets
    auto it = next(m_bioseq_set_list.begin(),2);
    while (it != m_bioseq_set_list.end()) {
        if (it->m_class != CBioseq_set::eClass_segset &&
            it->m_class != CBioseq_set::eClass_parts) {
            return false;
        }
        ++it;
    }
    return true;
}


bool CCleanupHugeAsnReader::x_IsExtendedCleanup() const
{
    return (m_CleanupOptions & eExtendedCleanup);
}


bool CCleanupHugeAsnReader::x_NeedsNcbiUserObject() const
{
    return x_IsExtendedCleanup() && (!(m_CleanupOptions & eNoNcbiUserObjects));
}



void CCleanupHugeAsnReader::x_CleanupTopLevelDescriptors()
{

    m_TopLevelBiosources.clear();
    m_pTopLevelMolInfo.Reset();

    if (x_NeedsNcbiUserObject()) { // RW-2363
        if (! m_top_entry ||
            ! m_top_entry->IsSetDescr() ||
            ! m_top_entry->GetDescr().IsSet()) {
            // If necessary, instantiate m_top_entry
            // Add NcbiCleanup user object
            if (! m_top_entry) {
                m_top_entry                      = Ref(new CSeq_entry());
                m_top_entry->SetSet().SetClass() = CBioseq_set::eClass_genbank;
            }
            CCleanup::AddNcbiCleanupObject(1, m_top_entry->SetSet().SetDescr());
            m_Changes.SetChanged(CCleanupChange::eAddNcbiCleanupObject);
            return;
        }
    } else if (! m_top_entry || ! m_top_entry->IsSetDescr()) {
        return;
    } else if (! m_top_entry->GetDescr().IsSet()) {
        m_top_entry->SetSet().ResetDescr();
        return;
    }

    CCleanup cleanup;
    m_Changes += *cleanup.BasicCleanup(m_top_entry->SetDescr());

    if (m_CleanupOptions & eRemoveNcbiUserObjects) {
        CCleanup::RemoveNcbiCleanupObject(m_top_entry->SetDescr());
        if (! m_top_entry->GetDescr().IsSet() ||
            m_top_entry->GetDescr().Get().empty()) {
            m_top_entry->SetSet().ResetDescr();
        }
    }

    if (!x_IsExtendedCleanup()) {
        return;
    }

    auto& descriptors = m_top_entry->SetDescr().Set();
    auto it = descriptors.begin();
    while (it != descriptors.end()) {
        if (it->Empty()) {
            it = descriptors.erase(it);
        }
        else if ((*it)->IsSource()) {
            m_TopLevelBiosources.push_back(*it);
            it = descriptors.erase(it);
            m_Changes.SetChanged(CCleanupChange::eAddDescriptor);
            m_Changes.SetChanged(CCleanupChange::eRemoveDescriptor);
        }
        else if ((*it)->IsMolinfo()) {
            if (!m_pTopLevelMolInfo) {
                m_pTopLevelMolInfo.Reset(&(**it));
            }
            it = descriptors.erase(it);
            m_Changes.SetChanged(CCleanupChange::eRemoveDescriptor);
        } else {
            ++it;
        }
    }

    if (!(m_CleanupOptions & eNoNcbiUserObjects)) {
        CCleanup::AddNcbiCleanupObject(1, m_top_entry->SetDescr());
        m_Changes.SetChanged(CCleanupChange::eAddNcbiCleanupObject);
    }

    if (descriptors.empty()) {
        m_top_entry->SetSet().ResetDescr();
    }
    else if (CCleanup::NormalizeDescriptorOrder(m_top_entry->SetDescr())) {
        m_Changes.SetChanged(CCleanupChange::eMoveDescriptor);
    }
}


void CCleanupHugeAsnReader::x_AddTopLevelDescriptors(CSeq_entry& entry) const
{
    if ((!x_IsExtendedCleanup()) ||
        (m_TopLevelBiosources.empty() && m_pTopLevelMolInfo.Empty())) {
        return;
    }

    bool addMolInfo = false;
    if (m_pTopLevelMolInfo &&
        entry.IsSetDescr() &&
        entry.GetDescr().IsSet()) {
        const auto& descriptors = entry.GetDescr().Get();
        auto it = find_if(descriptors.begin(), descriptors.end(),
                [](const CRef<CSeqdesc>& pDesc) {
                    return (pDesc && pDesc->IsMolinfo());
                });
        if (it == descriptors.end()) {
            addMolInfo = true;
        }
    }

    for (auto pSource : m_TopLevelBiosources) {
        entry.SetDescr().Set().push_back(pSource);
    }

    if (addMolInfo) {
        entry.SetDescr().Set().push_back(m_pTopLevelMolInfo);
        m_Changes.SetChanged(CCleanupChange::eAddDescriptor);
    }
}


using TFeatIdMap = map<CCleanupHugeAsnReader::TFeatId,CCleanupHugeAsnReader::TFeatId>;


static void s_UpdateFeatureId(CFeat_id& featId, const TFeatIdMap& idMap)
{
    if (!featId.IsLocal() || !featId.GetLocal().IsId()) {
        return;
    }
    const auto id = featId.GetLocal().GetId();
    if (auto it=idMap.find(id); it != idMap.end()) {
        featId.SetLocal().SetId() = it->second;
    }
}


static void s_UpdateFeatureIds(CSeq_feat& feat, const TFeatIdMap& idMap)
{
    if (feat.IsSetId()) {
        s_UpdateFeatureId(feat.SetId(), idMap);
    }

    if (feat.IsSetIds()) {
        for (auto pFeatId : feat.SetIds()) {
            if (pFeatId) {
                s_UpdateFeatureId(*pFeatId, idMap);
            }
        }
    }

    if (feat.IsSetXref()) {
        for (auto pXref : feat.SetXref()) {
            if (pXref && pXref->IsSetId()) {
                s_UpdateFeatureId(pXref->SetId(), idMap);
            }
        }
    }
}


static void s_UpdateFeatureIds(CSeq_annot& annot, const TFeatIdMap& idMap)
{
    if (!annot.IsFtable()) {
        return;
    }

    for (auto pSeqFeat : annot.SetData().SetFtable()) {
        if (pSeqFeat) {
            s_UpdateFeatureIds(*pSeqFeat, idMap);
        }
    }
}


static void s_UpdateFeatureIds(CBioseq& bioseq, const TFeatIdMap& idMap) {
    if (!bioseq.IsSetAnnot()) {
        return;
    }

    for (auto pAnnot : bioseq.SetAnnot()) {
        if (pAnnot) {
            s_UpdateFeatureIds(*pAnnot, idMap);
        }
    }
}


static void s_UpdateFeatureIds(CBioseq_set& bioseqSet, const TFeatIdMap& idMap)
{
    if (bioseqSet.IsSetAnnot()) {
        for (auto pAnnot : bioseqSet.SetAnnot()) {
            if (pAnnot) {
                s_UpdateFeatureIds(*pAnnot, idMap);
            }
        }
    }

    if (bioseqSet.IsSetSeq_set()) {
        for (auto pSubEntry : bioseqSet.SetSeq_set()) {
            if (pSubEntry) {
                if (pSubEntry->IsSeq()) {
                    s_UpdateFeatureIds(pSubEntry->SetSeq(), idMap);
                }
                else {
                    s_UpdateFeatureIds(pSubEntry->SetSet(), idMap);
                }
            }
        }
    }
}



static void s_UpdateFeatureIds(CSeq_entry& entry, const TFeatIdMap& idMap)
{
    if (entry.IsSeq()) {
        s_UpdateFeatureIds(entry.SetSeq(), idMap);
    }
    else {
        s_UpdateFeatureIds(entry.SetSet(), idMap);
    }
}


CRef<CSeq_entry> CCleanupHugeAsnReader::LoadSeqEntry(
        const TBioseqSetInfo& info,
        eAddTopEntry add_top_entry) const
{
    if (m_CleanupOptions & eEnableSmallGenomeSets) {
        auto it = m_SetPosToFluLabel.find(info.m_pos);
        if (it != m_SetPosToFluLabel.end()) {
            auto pSmallGenomeEntry = Ref(new CSeq_entry());
            pSmallGenomeEntry->SetSet().SetClass() = CBioseq_set::eClass_small_genome_set;
            for (const auto& setInfo : m_FluLabelToSetInfo.at(it->second)) {
                auto pSubEntry = TParent::LoadSeqEntry(setInfo, eAddTopEntry::no);

                if (x_IsExtendedCleanup()) {
                    if (auto posIt = m_FeatIdInfo.PosToIdMap.find(setInfo.m_pos);
                        posIt != m_FeatIdInfo.PosToIdMap.end()) {
                        s_UpdateFeatureIds(*pSubEntry, posIt->second);
                    }
                }
                pSmallGenomeEntry->SetSet().SetSeq_set().push_back(pSubEntry);
            }
            if (add_top_entry == eAddTopEntry::yes) {
                x_AddTopLevelDescriptors(*pSmallGenomeEntry);
            }
            return pSmallGenomeEntry;
        }
    }

    auto pEntry = TParent::LoadSeqEntry(info, eAddTopEntry::no);
    if (add_top_entry == eAddTopEntry::yes) {
        x_AddTopLevelDescriptors(*pEntry);
    }

    if (x_IsExtendedCleanup()) {
        if (auto posIt = m_FeatIdInfo.PosToIdMap.find(info.m_pos);
            posIt != m_FeatIdInfo.PosToIdMap.end()) {
            s_UpdateFeatureIds(*pEntry, posIt->second);
        }
    }


    return pEntry;
}


static string s_GetInfluenzaLabel(const CSeq_descr& descr)
{
    if (descr.IsSet()) {
        for (const auto& pDesc : descr.Get()) {
            if (pDesc->IsSource()) {
                const auto& source = pDesc->GetSource();
                if (source.IsSetOrg()) {
                    auto key = CInfluenzaSet::GetKey(source.GetOrg());
                    if (!NStr::IsBlank(key)) {
                        return key;
                    }
                }
            }
        }
    }
    return "";
}


template<typename TMap>
static void s_RemoveEntriesWithVal(const string& val, TMap& mapToVal)
{
    if (mapToVal.empty()) {
        return;
    }

    auto it = mapToVal.begin();
    while (it != mapToVal.end()) {
        if (it->second == val) {
            it = mapToVal.erase(it);
        } else {
            ++it;
        }
    }
}


static bool s_CheckForSegments(CConstRef<CSeq_descr> seqDescrs,
        CConstRef<CSeq_descr> setDescrs,
        const string& fluLabel,
        set<size_t>& segments)
{
    if (NStr::IsBlank(fluLabel)) {
        return false;
    }
    auto fluType = CInfluenzaSet::GetInfluenzaType(fluLabel);
    if (fluType == CInfluenzaSet::eNotInfluenza) {
        return false;
    }

    auto numRequired = CInfluenzaSet::GetNumRequired(fluType);

    if (seqDescrs && seqDescrs->IsSet()) {
        for (auto pDesc : seqDescrs->Get()) {
            if (pDesc->IsSource()) {
                return g_FindSegs(pDesc->GetSource(), numRequired, segments);
            }
        }
    }

    if (setDescrs && setDescrs->IsSet()) {
        for (auto pDesc : setDescrs->Get()) {
            if (pDesc->IsSource()) {
                return g_FindSegs(pDesc->GetSource(), numRequired, segments);
            }
        }
    }
    return false;
}


static bool s_IdInSet(const CConstRef<CSeq_id>& pId,
        const set<CConstRef<CSeq_id>, CHugeAsnReader::CRefLess>& idSet)
{
    auto it = idSet.lower_bound(pId);
    if (it != idSet.end()) {
        if ((*it)->CompareOrdered(*pId) == 0 ||
            (*it)->Compare(*pId) == CSeq_id::E_SIC::e_YES) {
            return true;
        }
    }
    return false;
}


void CCleanupHugeAsnReader::x_CreateSmallGenomeSets()
{
    map<string, set<size_t>> fluLabelToSegs;
    CConstRef<CSeq_descr> pNpSetDescr;
    for (const auto& bioseqInfo : GetBioseqs()) {
        if (!CSeq_inst::IsNa(bioseqInfo.m_mol)) {
            continue;
        }
        auto parent = bioseqInfo.m_parent_set;
        if (parent->m_class == CBioseq_set::eClass_nuc_prot) {
            pNpSetDescr = parent->m_descr;
            parent = parent->m_parent_set;
        }
        if (!IsHugeSet(parent->m_class)) {
            continue;
        }
        string fluLabel;
        if (bioseqInfo.m_descr) {
            fluLabel = s_GetInfluenzaLabel(*(bioseqInfo.m_descr));
            if (NStr::IsBlank(fluLabel) && pNpSetDescr) {
                fluLabel = s_GetInfluenzaLabel(*pNpSetDescr);
            }
        }
        if (!NStr::IsBlank(fluLabel)) {
            bool makeSmallGenomeSet =
                s_CheckForSegments(bioseqInfo.m_descr, pNpSetDescr, fluLabel, fluLabelToSegs[fluLabel]);

            if (makeSmallGenomeSet) {
                const auto& setInfo = *FindTopObject(bioseqInfo.m_ids.front());
                m_FluLabelToSetInfo[fluLabel].push_back(setInfo);
                m_SetPosToFluLabel[setInfo.m_pos] = fluLabel;
                for (auto pId : bioseqInfo.m_ids) {
                    m_IdToFluLabel[pId] = fluLabel;
                }
            }
        }
    }

    // Prune if there are missing segments
    for (const auto& entry : fluLabelToSegs) {
        const auto& fluLabel = entry.first;
        const auto& segsFound = entry.second;
        x_PruneIfSegsMissing(fluLabel, segsFound);
    };
    x_PruneIfFeatsIncomplete();
}



void CCleanupHugeAsnReader::x_PruneIfSegsMissing(const string& fluLabel, const set<size_t>& segsFound)
{
    if (auto it = m_FluLabelToSetInfo.find(fluLabel); it != m_FluLabelToSetInfo.end()) {
        auto fluType = CInfluenzaSet::GetInfluenzaType(fluLabel);
        auto numRequired = CInfluenzaSet::GetNumRequired(fluType);
        if (segsFound.size() != numRequired) {
            m_FluLabelToSetInfo.erase(it);
            s_RemoveEntriesWithVal(fluLabel, m_SetPosToFluLabel);
            s_RemoveEntriesWithVal(fluLabel, m_IdToFluLabel);
        }
    }
}


void CCleanupHugeAsnReader::x_PruneIfFeatsIncomplete()
{
    // Prune if any of the sequences has incomplete cdregion or gene feats
    auto it = m_IdToFluLabel.begin();
    set<string> fluLabelsToRemove;
    while (it != m_IdToFluLabel.end()) {
        if (s_IdInSet(it->first, m_HasIncompleteFeats)) {
            auto fluLabel = it->second;
            fluLabelsToRemove.insert(fluLabel);
            s_RemoveEntriesWithVal(fluLabel, m_SetPosToFluLabel);
            if (auto fluLabelIt = m_FluLabelToSetInfo.find(fluLabel); fluLabelIt != m_FluLabelToSetInfo.end()) {
                m_FluLabelToSetInfo.erase(fluLabelIt);
            }
            it = m_IdToFluLabel.erase(it);
        }
        else {
            ++it;
        }
    }

    for (const auto& fluLabel : fluLabelsToRemove) {
        s_RemoveEntriesWithVal(fluLabel, m_IdToFluLabel);
    }
}



CCleanupHugeAsnReader::TIdToFluLabel::iterator
CCleanupHugeAsnReader::x_GetFluLabel(const CConstRef<CSeq_id>& pId)
{
    auto it = m_IdToFluLabel.lower_bound(pId);
    if (it != m_IdToFluLabel.end()) {
        if (it->first->CompareOrdered(*pId) == 0 ||
            it->first->Compare(*pId) == CSeq_id::E_SIC::e_YES) {
            return it;
        }
    }
    return m_IdToFluLabel.end();
}


using TFeatIdSet = set<CCleanupHugeAsnReader::TFeatId>;


static void s_FindNextOffset(const TFeatIdSet &existing_ids,
        const TFeatIdSet &new_existing_ids,
        const TFeatIdSet &current_ids,
        CCleanupHugeAsnReader::TFeatId &offset)
{
    do
    {
        ++offset;
    } while(existing_ids.find(offset) != existing_ids.end() ||
            new_existing_ids.find(offset) != new_existing_ids.end() ||
            current_ids.find(offset) != current_ids.end());
}


void CCleanupHugeAsnReader::x_RecordFeatureId(const CFeat_id& featId)
{
    if (!featId.IsLocal() || !featId.GetLocal().IsId()) {
        return;
    }

    const auto id = featId.GetLocal().GetId();

    if (m_FeatIdInfo.ExistingIds.find(id) != m_FeatIdInfo.ExistingIds.end() ||
            m_FeatIdInfo.NewIds.find(id) != m_FeatIdInfo.NewIds.end()) {
        auto it = m_FeatIdInfo.RemappedIds.find(id);
        if (it != m_FeatIdInfo.RemappedIds.end()) {
            m_FeatIdInfo.IdOffset = it->second;
        }
        else {
            s_FindNextOffset(m_FeatIdInfo.ExistingIds, m_FeatIdInfo.NewExistingIds, m_FeatIdInfo.NewIds, m_FeatIdInfo.IdOffset);
            m_FeatIdInfo.RemappedIds.emplace(id, m_FeatIdInfo.IdOffset);
        }
        m_FeatIdInfo.NewIds.insert(m_FeatIdInfo.IdOffset);
    }
    else {
        m_FeatIdInfo.NewExistingIds.insert(id);
    }
}


void CCleanupHugeAsnReader::x_SetBioseqHooks(CObjectIStream& objStream, TContext& context)
{
    CObjectTypeInfo bioseq_info = CType<CBioseq>();

    SetLocalSkipHook(bioseq_info, objStream,
        [this, &context](CObjectIStream& in, const CObjectTypeInfo& type)
    {
        auto pos = in.GetStreamPos() + m_next_pos;
        context.bioseq_stack.push_back({});

        auto parent = context.bioseq_set_stack.back();
        const bool hasGenbankParent = (parent->m_class == CBioseq_set::eClass_genbank);
        if (hasGenbankParent) {
            m_FeatIdInfo.NewIds.clear();
            m_FeatIdInfo.NewExistingIds.clear();
            m_FeatIdInfo.RemappedIds.clear();
        }

        type.GetTypeInfo()->DefaultSkipData(in);

        auto& bioseqinfo = context.bioseq_stack.back();
        m_bioseq_list.push_back({pos, parent, bioseqinfo.m_length, bioseqinfo.m_descr, bioseqinfo.m_ids, bioseqinfo.m_mol, bioseqinfo.m_repr});
        context.bioseq_stack.pop_back();

        if (x_IsExtendedCleanup() && hasGenbankParent) {
            m_FeatIdInfo.ExistingIds.insert(m_FeatIdInfo.NewExistingIds.begin(), m_FeatIdInfo.NewExistingIds.end());
            m_FeatIdInfo.ExistingIds.insert(m_FeatIdInfo.NewIds.begin(), m_FeatIdInfo.NewIds.end());
            if (!m_FeatIdInfo.RemappedIds.empty()) {
                m_FeatIdInfo.PosToIdMap.emplace(pos, m_FeatIdInfo.RemappedIds);
            }
        }

    });
}


void CCleanupHugeAsnReader::x_SetBioseqSetHooks(CObjectIStream& objStream, TContext& context)
{
    CObjectTypeInfo bioseq_set_info = CType<CBioseq_set>();

    SetLocalSkipHook(bioseq_set_info, objStream,
            [this, &context](CObjectIStream& in, const CObjectTypeInfo& type)
            {
                auto pos = in.GetStreamPos() + m_next_pos;
                auto parent = context.bioseq_set_stack.back();
                const bool hasGenbankParent = (parent->m_class == CBioseq_set::eClass_genbank);
                if (hasGenbankParent) {
                    m_FeatIdInfo.NewIds.clear();
                    m_FeatIdInfo.NewExistingIds.clear();
                    m_FeatIdInfo.RemappedIds.clear();
                }

                m_bioseq_set_list.push_back({pos, parent});

                auto last = prev(m_bioseq_set_list.end());

                context.bioseq_set_stack.push_back(last);

                CObjectInfo objectInfo(type.GetTypeInfo());
                for (CIStreamClassMemberIterator it(in, type.GetTypeInfo()); it; ++it) {
                    it.ReadClassMember(objectInfo);
                    if ((*it).GetAlias() == "class") {
                        auto memIdx = (*it).GetMemberIndex();
                        CObjectInfo memberInfo = CObjectInfoMI(objectInfo, memIdx).GetMember();
                        last->m_class = *CTypeConverter<CBioseq_set::EClass>::SafeCast(memberInfo.GetObjectPtr());
                    }
                }

                auto* pBioseqSet = CTypeConverter<CBioseq_set>::SafeCast(objectInfo.GetObjectPtr());

                if (pBioseqSet->IsSetLevel()) {
                    last->m_Level = pBioseqSet->GetLevel();
                }

                if (pBioseqSet->IsSetDescr()) {
                    last->m_descr.Reset(&(pBioseqSet->GetDescr()));
                }

                if (IsHugeSet(last->m_class) &&
                    last->HasAnnot()) {
                    m_HasHugeSetAnnot = true;
                }

                context.bioseq_set_stack.pop_back();

                if (x_IsExtendedCleanup() && hasGenbankParent) {
                    m_FeatIdInfo.ExistingIds.insert(m_FeatIdInfo.NewExistingIds.begin(), m_FeatIdInfo.NewExistingIds.end());
                    m_FeatIdInfo.ExistingIds.insert(m_FeatIdInfo.NewIds.begin(), m_FeatIdInfo.NewIds.end());
                    if (!m_FeatIdInfo.RemappedIds.empty()) {
                        m_FeatIdInfo.PosToIdMap.emplace(pos, m_FeatIdInfo.RemappedIds);
                    }
                }
            });
}


void CCleanupHugeAsnReader::x_SetSeqFeatHooks(CObjectIStream& objStream)
{

    SetLocalReadHook(CType<CSeq_feat>(), objStream,
            [this](CObjectIStream& in, const CObjectInfo& object)
            {
                auto* pObject = object.GetObjectPtr();
                object.GetTypeInfo()->DefaultReadData(in, pObject);

                if (!x_IsExtendedCleanup()) {
                    return;
                }

                auto* pSeqFeat = CTypeConverter<CSeq_feat>::SafeCast(pObject);

                if (pSeqFeat->IsSetId()) {
                    x_RecordFeatureId(pSeqFeat->GetId());
                }

                if (pSeqFeat->IsSetIds()) {
                    for (auto pFeatId : pSeqFeat->GetIds()) {
                        if (pFeatId) {
                            x_RecordFeatureId(*pFeatId);
                        }
                    }
                }
            });


    SetLocalSkipHook(CType<CSeq_feat>(), objStream,
            [this](CObjectIStream& in, const CObjectTypeInfo& type)
            {
                auto pSeqFeat = Ref(new CSeq_feat());
                type.GetTypeInfo()->DefaultReadData(in, pSeqFeat);

                if (x_IsExtendedCleanup()) {
                    if (pSeqFeat->IsSetId()) {
                        x_RecordFeatureId(pSeqFeat->GetId());
                    }

                    if (pSeqFeat->IsSetIds()) {
                        for (auto pFeatId : pSeqFeat->GetIds()) {
                            if (pFeatId) {
                                x_RecordFeatureId(*pFeatId);
                            }
                        }
                    }
                }


                if (!(m_CleanupOptions & eEnableSmallGenomeSets)) {
                    return;
                }

                if (pSeqFeat->IsSetData() &&
                    (pSeqFeat->GetData().IsCdregion() ||
                    pSeqFeat->GetData().IsGene())) {
                    if (pSeqFeat->GetLocation().IsPartialStart(eExtreme_Biological) ||
                        pSeqFeat->GetLocation().IsPartialStop(eExtreme_Biological)) {
                        const auto* pSeqId = pSeqFeat->GetLocation().GetId();
                        if (pSeqId) {
                            CConstRef<CSeq_id> pConstId(pSeqId);
                            m_HasIncompleteFeats.insert(pConstId);
                        }
                    }
                }
            });


}




void CCleanupHugeAsnReader::x_SetHooks(CObjectIStream& objStream, TContext& context)
{
    TParent::x_SetHooks(objStream, context);

    x_SetSeqFeatHooks(objStream);
}


END_SCOPE(objects)
END_NCBI_SCOPE
