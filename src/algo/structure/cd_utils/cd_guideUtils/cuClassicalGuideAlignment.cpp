/* $Id$
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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *      Concrete subclasses for representing classical guide alignments.
 *      Simplest subclass assumes that masters can be used for two given CDs.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
//#include <algo/structure/cd_utils/cuUtils.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuClassicalGuideAlignment.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

map<const CDFamily*, MultipleAlignment*> CValidatedHierarchyGuideAlignment::m_alignmentCache;


// ========================================
//      CValidatedHierarchyGuideAlignment
// ========================================

CGuideAlignment_Base* CValidatedHierarchyGuideAlignment::Copy() const {
    CValidatedHierarchyGuideAlignment* guideCopy = new CValidatedHierarchyGuideAlignment(m_family1);
    CopyBase(guideCopy);
    return guideCopy;
}


void CValidatedHierarchyGuideAlignment::ClearAlignmentCache()
{
    map<const CDFamily*, MultipleAlignment*>::iterator cacheIt = m_alignmentCache.begin(), cacheEnd = m_alignmentCache.end();
    for (; cacheIt != cacheEnd; ++cacheIt) {
        delete cacheIt->second;
    }
    m_alignmentCache.clear();
}

bool CValidatedHierarchyGuideAlignment::Make(const CCdCore* cd1, const CCdCore* cd2)
{
    bool result = (m_family1 && cd1 && cd2);

    //  Remove any previous block model pair, so that in case of errors IsOK returns false.
    Reset();

    if (!result) {
        if (!m_family1) {
            m_errors.push_back("CValidatedHierarchyGuideAlignment error:  Null family - can't make guide alignment.\n");
        }
        if (!cd1) {
            m_errors.push_back("CValidatedHierarchyGuideAlignment error:  First CD is null - can't make guide alignment.\n");
        }
        if (!cd2) {
            m_errors.push_back("CValidatedHierarchyGuideAlignment error:  Second CD is null - can't make guide alignment.\n");
        }
        return false;
    }

    CDFamilyIterator famEnd = m_family1->end();
    if (m_family1->findCD(const_cast<CCdCore*>(cd1)) == famEnd) {
        m_errors.push_back("CValidatedHierarchyGuideAlignment error:  First CD is not in the family - not making guide alignment.\n");
        return false;
    } else if (m_family1->findCD(const_cast<CCdCore*>(cd2)) == famEnd) {
        m_errors.push_back("CValidatedHierarchyGuideAlignment error:  Second CD is not in the family - not making guide alignment.\n");
        return false;
    }


    MultipleAlignment maAll;
    CDFamilyIterator fit = m_family1->convergeTo(const_cast<CCdCore*>(cd1), const_cast<CCdCore*>(cd2));
    if (fit == famEnd) {
        m_errors.push_back("CValidatedHierarchyGuideAlignment error:  Common ancestor not found for CDs.\n");
        result = false;
    }

    if (result) {
        maAll.setAlignment(*m_family1, fit);
        result = MakeFromMultipleAlignment(cd1, cd2, &maAll);
        if (result) {
            MakeChains(cd1, cd2, fit->cd);
        }
    }
    return result;
}

bool CValidatedHierarchyGuideAlignment::MakeGuideToRoot(const CCdCore* cd, bool cache)
{
    bool result = false;
    MultipleAlignment* ma;
    map<const CDFamily*, MultipleAlignment*>::iterator cacheIt;
    CCdCore* root = (m_family1) ? m_family1->getRootCD() : NULL;
    if (!root) {
        Reset();
        m_errors.push_back("CValidatedHierarchyGuideAlignment error:  Could not get root CD for family.\n");
    } else if (!cache) {
        result = Make(root, cd);
    } else {
        //  Use cached alignment for family if available.  Otherwise, create and cache it.
        cacheIt = m_alignmentCache.find(m_family1);
        if ( cacheIt != m_alignmentCache.end()) {
            ma = cacheIt->second;
        } else {
            ma = new MultipleAlignment();
            if (ma) {
                CDFamilyIterator rootIt = m_family1->begin();
//                LOG_POST("about to call ma->setAlignment\n");
                ma->setAlignment(*m_family1, rootIt);  //  first iterator is to the root
//                LOG_POST("returned from ma->setAlignment\n");
                m_alignmentCache[m_family1] = ma;
            }
        }

        Reset();
//        LOG_POST("about to call Make(root, cd, ma)\n");
        result = MakeFromMultipleAlignment(root, cd, ma);
        if (result) {
            MakeChains(root, cd, root);
        }

    }
    return result;
}


bool CValidatedHierarchyGuideAlignment::MakeFromMultipleAlignment(const CCdCore* cd1, const CCdCore* cd2, MultipleAlignment* ma)
{
    int master1, master2;
    string master1Str, master2Str;

    m_isOK = (cd1 && cd2 && ma);
    if (m_isOK) {
        master1 = ma->GetRowSourceTable().convertFromCDRow(const_cast<CCdCore*>(cd1), 0);
        master2 = ma->GetRowSourceTable().convertFromCDRow(const_cast<CCdCore*>(cd2), 0);
        cd1->Get_GI_or_PDB_String_FromAlignment(0, master1Str, false, 0);
        cd2->Get_GI_or_PDB_String_FromAlignment(0, master2Str, false, 0);

        _TRACE("cd1 " << cd1->GetAccession() << " master " << master1 << "; " << master1Str);
        _TRACE("cd2 " << cd2->GetAccession() << " master " << master2 << "; " << master2Str);

        if (master1 >= 0 && master2 >= 0) {
            m_guideBlockModelPair.getMaster() = ma->getBlockModel(master1);
            m_guideBlockModelPair.getSlave()  = ma->getBlockModel(master2);
            m_isOK = m_guideBlockModelPair.isValid();
        } else {
            string err;
            if (master1 < 0) {
                err = "CValidatedHierarchyGuideAlignment error:  the master of CD1 (" + cd1->GetAccession() + ") is either not present in its parents or is misaligned with respect to its parents\n";
            }
            if (master2 < 0) {
                err += "CValidatedHierarchyGuideAlignment error:  the master of CD2 (" + cd2->GetAccession() + ") is either not present in its parents or is misaligned with respect to its parents\n";
            }
            m_errors.push_back(err);
            m_isOK = false;
        }
    }

    return m_isOK;
}


bool CValidatedHierarchyGuideAlignment::Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2)
{
    bool result = (guideInput1.fam == guideInput2.fam && guideInput1.fam == m_family1);
    if (result) {
        result = Make(guideInput1.cd, guideInput2.cd);
    } else {
        Reset();
        m_errors.push_back("CValidatedHierarchyGuideAlignment error:  different families passed to Make.\n");
    }
    return result;
}

void CValidatedHierarchyGuideAlignment::MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd)
{
    _ASSERT(guideInput1.fam == guideInput2.fam);
    MakeChains(guideInput1.cd, guideInput2.cd, commonCd);
}

void CValidatedHierarchyGuideAlignment::MakeChains(const CCdCore* cd1, const CCdCore* cd2, const CCdCore* commonCd)
{
    vector<CCdCore*> path1, path2;
    int pathLen1 = m_family1->getPathToRoot(const_cast<CCdCore*>(cd1), path1);
    int pathLen2 = m_family1->getPathToRoot(const_cast<CCdCore*>(cd2), path2);

    for (int i = 0; i < pathLen1; ++i) {
        m_chain1.push_back(SGuideAlignmentLink(path1[i]));
        if (path1[i] == commonCd) {
            break;
        }
    }

    for (int i = 0; i < pathLen2; ++i) {
        m_chain2.push_back(SGuideAlignmentLink(path2[i]));
        if (path2[i] == commonCd) {
            break;
        }
    }

}


END_SCOPE(cd_utils)
END_NCBI_SCOPE
