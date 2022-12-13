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
#include <objtools/cleanup/huge_file_cleanup.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objmgr/seq_entry_handle.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
USING_SCOPE(edit);


CCleanupHugeAsnReader::CCleanupHugeAsnReader(bool doExtendedCleanup) 
: m_ExtendedCleanup(doExtendedCleanup) {}


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
        }
    }
    TParent::FlattenGenbankSet();
    x_CleanupTopLevelDescriptors();
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

void CCleanupHugeAsnReader::x_CleanupTopLevelDescriptors() 
{

    m_TopLevelBiosources.clear();
    m_pTopLevelMolInfo.Reset();


    if (!m_top_entry || 
        !m_top_entry->IsSetDescr() ||
        !m_top_entry->GetDescr().IsSet()) {
        return;
    }

    CCleanup cleanup;
    cleanup.BasicCleanup(m_top_entry->SetDescr());
    
    if (!m_ExtendedCleanup) {
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
        }
        else if ((*it)->IsMolinfo()) {
            if (!m_pTopLevelMolInfo) {
                m_pTopLevelMolInfo.Reset(&(**it));
            }
            it = descriptors.erase(it);
        } else {
            ++it;
        }
    }

    if (descriptors.empty()) {
        m_top_entry->SetSet().ResetDescr();
    }
}


void CCleanupHugeAsnReader::AddTopLevelDescriptors(CSeq_entry_Handle seh) 
{
    if (!m_ExtendedCleanup ||
        (m_TopLevelBiosources.empty() && m_pTopLevelMolInfo.Empty())) {
        return;
    }

    bool addMolInfo = false;
    if (m_pTopLevelMolInfo &&
        seh.IsSetDescr() &&
        seh.GetDescr().IsSet()) {
        const auto& descriptors = seh.GetDescr().Get();
        auto it = find_if(descriptors.begin(), descriptors.end(),
                [](const CRef<CSeqdesc>& pDesc) {
                    return (pDesc && pDesc->IsMolinfo());
                });
        if (it == descriptors.end()) {
            addMolInfo = true;
        }
    }

    auto editHandle = seh.GetEditHandle();
    for (auto pSource : m_TopLevelBiosources) {
        editHandle.AddSeqdesc(*pSource);
    }

    if (addMolInfo) {
        editHandle.AddSeqdesc(*m_pTopLevelMolInfo);
    }
}

// CHugeFileCleanup::CHugeFileCleanup() {}


END_SCOPE(objects)
END_NCBI_SCOPE
