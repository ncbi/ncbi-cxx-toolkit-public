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

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
USING_SCOPE(edit);


CCleanupHugeAsnReader::CCleanupHugeAsnReader() {}

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

END_SCOPE(objects)
END_NCBI_SCOPE
