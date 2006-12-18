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
 *          Base class for representing the guide alignment between two CDs.
 *          Default implementation assumes both are from a single CDFamily
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuGuideAlignment.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

map<CDFamily*, MultipleAlignment*> CGuideAlignment::m_alignmentCache;

void CGuideAlignment::Initialize()
{
    m_isOK = false;
    m_errors.clear();

    Cleanup();
}

void CGuideAlignment::Cleanup()
{
    m_chain1.clear();
    m_chain2.clear();
    m_families.clear();  //  class does not own CDFamily objects
    if (m_blockModels.size() > 0) {
        for (unsigned int i = 0; i < m_blockModels.size(); ++i) {
            delete m_blockModels[i];
        }
        m_blockModels.clear();
    }

    m_guideBlockModelPair.reset();
}

void CGuideAlignment::ClearAlignmentCache()
{
    map<CDFamily*, MultipleAlignment*>::iterator cacheIt = m_alignmentCache.begin(), cacheEnd = m_alignmentCache.end();
    for (; cacheIt != cacheEnd; ++cacheIt) {
        delete cacheIt->second;
    }
    m_alignmentCache.clear();
}

string CGuideAlignment::GetErrors() const
{
    static const string slashN("\n");

    string allErrors;
    for (unsigned int i = 0; i < m_errors.size(); ++i) {
        allErrors += m_errors[i] + slashN;
    }
    return allErrors;
}

bool CGuideAlignment::Make(const CCdCore* cd1, const CCdCore* cd2, CDFamily* family)
{
    bool result = (family && cd1 && cd2);

    //  Remove any previous block model pair, so that in case of errors IsOK returns false.
    Reset();

    if (!result) {
        if (!family) {
            m_errors.push_back("CGuideAlignment error:  Null family - can't make guide alignment.\n");
        }
        if (!cd1) {
            m_errors.push_back("CGuideAlignment error:  First CD is null - can't make guide alignment.\n");
        }
        if (!cd2) {
            m_errors.push_back("CGuideAlignment error:  Second CD is null - can't make guide alignment.\n");
        }
        return false;
    }

    MultipleAlignment maAll;
    CDFamilyIterator fit = family->convergeTo(const_cast<CCdCore*>(cd1), const_cast<CCdCore*>(cd2));
    if (fit == family->end()) {
        m_errors.push_back("CGuideAlignment error:  Common ancestor not found for CDs.\n");
        result = false;
    }

    if (result) {
        maAll.setAlignment(*family, fit);
        result = Make(cd1, cd2, &maAll);
        if (result) {
            MakeChains(cd1, cd2, fit->cd, family);
        }
    }
    return result;
}

bool CGuideAlignment::MakeGuideToRoot(const CCdCore* cd, CDFamily* family, bool cache)
{
    bool result = false;
    MultipleAlignment* ma;
    map<CDFamily*, MultipleAlignment*>::iterator cacheIt;
    CCdCore* root = (family) ? family->getRootCD() : NULL;
    if (!root) {
        Reset();
        m_errors.push_back("CGuideAlignment error:  Could not get root CD for family.\n");
    } else if (!cache) {
        result = Make(root, cd, family);
    } else {
        //  Use cached alignment for family if available.  Otherwise, create and cache it.
        cacheIt = m_alignmentCache.find(family);
        if ( cacheIt != m_alignmentCache.end()) {
            ma = cacheIt->second;
        } else {
            ma = new MultipleAlignment();
            if (ma) {
                ma->setAlignment(*family, family->begin());  //  first iterator is to the root
                m_alignmentCache[family] = ma;
            }
        }

        Reset();
        result = Make(root, cd, ma);
        if (result) {
            MakeChains(root, cd, root, family);
        }

    }
    return result;
}


bool CGuideAlignment::Make(const CCdCore* cd1, const CCdCore* cd2, MultipleAlignment* ma)
{
    int master1, master2;

    m_isOK = (cd1 && cd2 && ma);
    if (m_isOK) {
        master1 = ma->GetRowSourceTable().convertFromCDRow(const_cast<CCdCore*>(cd1), 0);
        master2 = ma->GetRowSourceTable().convertFromCDRow(const_cast<CCdCore*>(cd2), 0);

        if (master1 >= 0 && master2 >= 0) {
            m_guideBlockModelPair.getMaster() = ma->getBlockModel(master1);
            m_guideBlockModelPair.getSlave()  = ma->getBlockModel(master2);
            m_isOK = m_guideBlockModelPair.isValid();
        } else {
            string err;
            if (master1 < 0) {
                err = "CGuideAlignment error:  the master of CD1 (" + cd1->GetAccession() + ") is either not present in its parents or is misaligned with respect to its parents\n";
            }
            if (master2 < 0) {
                err += "CGuideAlignment error:  the master of CD2 (" + cd2->GetAccession() + ") is either not present in its parents or is misaligned with respect to its parents\n";
            }
            m_errors.push_back(err);
            m_isOK = false;
        }
    }

    return m_isOK;
}


bool CGuideAlignment::Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2)
{
    bool result = (guideInput1.fam == guideInput2.fam);
    if (result) {
        result = Make(guideInput1.cd, guideInput2.cd, guideInput1.fam);
    } else {
        Reset();
        m_errors.push_back("CGuideAlignment error:  different families passed to Make.\n");
    }
    return result;
}

void CGuideAlignment::MakeChains(const CCdCore* cd1, const CCdCore* cd2, const CCdCore* commonCd, CDFamily* family)
{
    vector<CCdCore*> path1, path2;
    int pathLen1 = family->getPathToRoot(const_cast<CCdCore*>(cd1), path1);
    int pathLen2 = family->getPathToRoot(const_cast<CCdCore*>(cd2), path2);

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


const CCdCore* CGuideAlignment::GetMasterCD() const
{
    return (m_chain1.size() > 0) ? m_chain1[0].cd : NULL;
}

string CGuideAlignment::GetMasterCDAcc() const 
{
    const CCdCore* cd = (m_chain1.size() > 0) ? m_chain1[0].cd : NULL;
    return (cd) ? cd->GetAccession() : "";
}

unsigned int CGuideAlignment::GetMasterCDRow() const
{
    return (m_chain1.size() > 0) ? m_chain1[0].row : kMax_UInt;
}

const CCdCore* CGuideAlignment::GetSlaveCD() const
{
    return (m_chain2.size() > 0) ? m_chain2[0].cd : NULL;
}
string CGuideAlignment::GetSlaveCDAcc() const
{
    const CCdCore* cd = (m_chain2.size() > 0) ? m_chain2[0].cd : NULL;
    return (cd) ? cd->GetAccession() : "";
}
unsigned int CGuideAlignment::GetSlaveCDRow() const
{
    return (m_chain2.size() > 0) ? m_chain2[0].row : kMax_UInt;
}

const CCdCore* CGuideAlignment::GetCommonCD() const
{
    unsigned int n = m_chain1.size();
    return (n > 0) ? m_chain1[n-1].cd : NULL;
}
string CGuideAlignment::GetCommonCDAcc() const
{
    unsigned int n = m_chain1.size();
    const CCdCore* cd = (n > 0) ? m_chain1[n-1].cd : NULL;
    return (cd) ? cd->GetAccession() : "";
}
unsigned int CGuideAlignment::GetCommonCDRow() const
{
    unsigned int n = m_chain1.size();
    return (n > 0) ? m_chain1[n-1].row : kMax_UInt;
}

// ========================================
//      CGuideAlignmentFactory
// ========================================

void CGuideAlignmentFactory::MakeAllGuidesToRoot(CDFamily* family, GuideMap& guideMap)
{
    guideMap.clear();
    if (!family) return;

    CGuideAlignment* ga;
    CCdCore* thisCd;
    CCdCore* rootCd = family->getRootCD();
    CDFamilyIterator famIt = family->begin(), famEnd = family->end();

	for (;  famIt != famEnd;  ++famIt) {
        ga = NULL;
        thisCd = famIt->cd;
        if (thisCd != rootCd) {
            ga = new CGuideAlignment();
            if (ga) {
                ga->MakeGuideToRoot(thisCd, family, true);   //  cache the multiple alignment 
            }
        }

        //  It is the caller's responsibility to see if the creation succeeded by calling CGuideAlignment methods.
        guideMap.insert(GuideMapVT(thisCd, ga));
	}

    //  Clean up the static cache as noone else likely needs it any longer.
    CGuideAlignment::ClearAlignmentCache();
}

//  Infers the proper type of guide alignment needed based on the contents of the guide inputs,
//  then uses the virtual Make method to create objects of the associated class.
CGuideAlignment* MakeGuide(const CGuideAlignment::SGuideInput& guideInput1, const CGuideAlignment::SGuideInput& guideInput2)
{
    //  Simple case when need a classical alignment (only type supported right now...)
    CGuideAlignment* ga = new CGuideAlignment();
    if (ga) {
        ga->Make(guideInput1, guideInput2);
    }

    //  It is the caller's responsibility to see if the creation succeeded by calling CGuideAlignment methods.
    return ga;
}



END_SCOPE(cd_utils)
END_NCBI_SCOPE
