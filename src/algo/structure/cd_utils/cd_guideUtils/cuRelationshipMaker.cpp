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
 *          Singleton class that makes parent/child relationships between CDs.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuRelationshipMaker.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuGuideAlignmentFactory.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)


//////////////////////////////////////////////////
//
//  CRelationshipMaker
//
//////////////////////////////////////////////////

const int CRelationshipMaker::m_defaultOverlapPercentage = 70;
CRelationshipMaker* CRelationshipMaker::m_instance = NULL;
int CRelationshipMaker::m_overlapPercentage = CRelationshipMaker::m_defaultOverlapPercentage;
bool CRelationshipMaker::m_forceSharedSequenceInterval = false;
bool CRelationshipMaker::m_keepExistingParents = true;
bool CRelationshipMaker::m_autoUpdateChild = true;

//  Modify the block model to remove any parts outside of the interval [min, max].
//  [eventually refactor this method to the BlockModel class]
/*
void ClipToRange(BlockModel& bm, unsigned int min, unsigned max)
{
    unsigned int nBlocks = bm.getBlocks().size();
    if (nBlocks == 0) return;

    vector<Block> maskBlocks;
    int firstAligned = bm.getBlocks().front().getStart();
    int lastAligned = bm.getBlocks().back().getEnd();

    //  Add a masking block for all residues < min.
    if (firstAligned < min) {
        maskBlocks.push_back(Block(firstAligned, min - firstAligned));
    }
    //  Add a masking block for all residues > max.
    if (lastAligned > max) {
        maskBlocks.push_back(Block(max + 1, lastAligned - max));
    }

    //  for the moment, this is from cuGuideAlignment
    CGuideAlignment_Base::Mask(bm, maskBlocks);
}
*/

CRelationshipMaker* CRelationshipMaker::Get()
{
    if (m_instance == NULL) {
        m_instance = new CRelationshipMaker();
    }
    return m_instance;
}

void CRelationshipMaker::Reset()
{
    delete m_childDomain;
    m_childDomain = NULL;
    m_parentFamily = NULL;
    m_childFamily = NULL;

    m_rowsInCopy.clear();

    m_lastErrorMsg.erase();
}

bool CRelationshipMaker::SetOverlapPercentage(int overlapPercentage)
{
    if (overlapPercentage < 0 || overlapPercentage > 100) return false;
    m_overlapPercentage = overlapPercentage;
    return true;
}

void CRelationshipMaker::InstallChild(CCdCore* child)
{
    ncbi::cd_utils::CDFamilyIterator famIt, parentIt;
    string existingParentAcc;
    CCdCore* existingParent;
    CGuideAlignmentFactory gaFactory(m_overlapPercentage, true);  //  for now, always allow a consensus as master
    CGuideAlignment_Base* guide = NULL;
    bool addedLink;

    if (m_childDomain) {
        delete m_childDomain;
        m_childDomain = NULL;
    }

    m_childDomain = new CChildDomain(child);

    //  When 'child' has a pre-existing parent w/o a guide, use m_childFamily or m_parentFamily to build one.
    if (m_childDomain->GetNumOriginalAncestors() > 0) {
        
        const CCdd::TAncestors& origAnc = m_childDomain->GetOriginalAncestors();
        CCdd::TAncestors::const_iterator ancIt = origAnc.begin(), ancEnd = origAnc.end();
        for (; ancIt != ancEnd; ++ancIt) {

            //  Don't override any guides originally present.
            if ((*ancIt)->IsSetSeqannot()) continue;

            existingParent = NULL;
            existingParentAcc = CChildDomain::GetDomainParentAccession(**ancIt);

            //  Is this parent in m_childFamily???
            if (m_childFamily) {
                parentIt = m_childFamily->findCDByAccession(existingParentAcc);
                if (parentIt != m_childFamily->end()) {
                    existingParent = parentIt->cd;
                }
            }

            //  If didn't find it in m_childFamily, is this parent in m_parentFamily???
            if (!existingParent && m_parentFamily) {
                parentIt = m_parentFamily->findCDByAccession(existingParentAcc);
                if (parentIt != m_parentFamily->end()) {
                    existingParent = parentIt->cd;
                }
            }

            //  If have a pointer to the existing parent, try and make a guide to it...
            if (existingParent) {
                if (m_forceSharedSequenceInterval) {
                    guide = gaFactory.MakeGuide(existingParent, child);
                } else {
                    guide = gaFactory.MakeGuide(existingParent, m_parentFamily, child, m_childFamily);
                }
            }

            addedLink = false;
            if (existingParent && guide  && guide->IsOK()) {
                //  If the guide has the child as slave, reverse the guide; the m_childDomain object expects the *child* as master.
                if (guide->GetMasterCD() != child && guide->GetSlaveCD() == child) {
                    guide->Reverse();
                }
                CLinkToParent* link = new CLinkToParent(guide, (CDomain_parent::EParent_type) (*ancIt)->GetParent_type());
                addedLink = m_childDomain->AddLinkToOriginalAncestor(link);
            } 
            
            if (!addedLink) {
                m_lastErrorMsg = ResultCodeToString(RELMAKER_NO_GUIDE_TO_EXISTING_PARENT);
                if (guide) {
                    m_lastErrorMsg += "\n" + guide->GetErrors();
                }
                delete guide;
                guide = NULL;
            }
        }

    }
}

string CRelationshipMaker::AncestorsToString(const CCdCore* cd)
{
    string s, err;
    if (cd && cd->IsSetAncestors()) {
        const CCdd::TAncestors& ancList = cd->GetAncestors();
        CCdd::TAncestors::const_iterator it = ancList.begin(), itEnd = ancList.end();
        for (; it != itEnd; ++it) {
            CNcbiOstrstream oss;
            if (it->NotEmpty() && WriteASNToStream(oss, **it, false, &err)) {
                s += CNcbiOstrstreamToString(oss);
            }
        }
    }
    return s;
}

string CRelationshipMaker::ResultCodeToString(int code)
{
    static const string sSuccess("CRelationshipMaker:  Success");
    static const string sShared("CRelationshipMaker:  Could not make a guide alignment when common sequence interval is required");
    static const string sNoGuide("CRelationshipMaker:  Could not make a guide alignment");
    static const string sNoIntersect("CRelationshipMaker:  Guide alignment did not intersect block model provided");
    static const string sBadFromTo("CRelationshipMaker:  Problem with from/to positions provided");
    static const string sNoLink("CRelationshipMaker:  Parent link created but unable to add to child");
    static const string sNoGuideForExistingParent("CRelationshipMaker:  Failed to create guide to existing guide-less parent when adding other parents");
    static const string sUnspecified("CRelationshipMaker:  Unspecified error");
    static const string sUnknown("CRelationshipMaker:  Unknown result code ");
    static map<int, string> codeToStringMap;


    if (codeToStringMap.size() == 0) {
        codeToStringMap[RELMAKER_SUCCESS] = sSuccess;
        codeToStringMap[RELMAKER_SHARED_SEQUENCE_GUIDE] = sShared;
        codeToStringMap[RELMAKER_GUIDE_ALIGNMENT] = sNoGuide;
        codeToStringMap[RELMAKER_NO_INTERSECTION_WITH_BLOCK_MODEL] = sNoIntersect;
        codeToStringMap[RELMAKER_BAD_BLOCK_MODEL_RANGE] = sBadFromTo;
        codeToStringMap[RELMAKER_LINK_NOT_ADDED] = sNoLink;
        codeToStringMap[RELMAKER_NO_GUIDE_TO_EXISTING_PARENT] = sNoGuideForExistingParent;
        codeToStringMap[RELMAKER_UNSPECIFIED_ERROR] = sUnspecified;
    }

    map<int, string>::iterator mapIt = codeToStringMap.find(code);
    if (mapIt != codeToStringMap.end()) {
        return mapIt->second;
    } else {
        return sUnknown + NStr::IntToString(code);
    }
}


CDomain_parent::EParent_type CRelationshipMaker::InferRelationshipTypeNeeded(CCdCore* parent, const BlockModel* blocksOnParentMaster)
{
    CDomain_parent::EParent_type parentType = CDomain_parent::eParent_type_other;

    //  A parent can't be its own child.
    if (!m_childDomain || m_childDomain->GetChildCD() == parent) return parentType;

    unsigned int numExistingParents = m_childDomain->GetNumOriginalAncestors() + m_childDomain->GetNumAddedAncestors();
    if (numExistingParents == 0) {
        parentType = CDomain_parent::eParent_type_classical;
    } else if (numExistingParents >= 1) {
        parentType = CDomain_parent::eParent_type_fusion;
//    } else if (false) {
//      parentType = CDomain_parent::eParent_type_deletion;
//    } else if (false) {
//      parentType = CDomain_parent::eParent_type_permutation;
    }
    return parentType;
}

BlockModel* CRelationshipMaker::MakeClippedBlockModelFromMaster(CCdCore* cd, unsigned int from, unsigned int to)
{
    BlockModel* bm = NULL;
    if (cd && from <= to) {
        const CRef< CSeq_align>& masterAlign = cd->GetSeqAlign(0);
        if (masterAlign.NotEmpty()) {
            bm = new BlockModel(masterAlign, false);
            bm->clipToRange(from, to);
        }
    }
    return bm;
}

        //////////////////////////////////////////////////
        //  CreateChild methods
        //////////////////////////////////////////////////

/*
CCdCore* CRelationshipMaker::CreateChild(CCdCore* parent, unsigned int from, unsigned int to, const string& childAccession, const string& childShortname)
{
    CCdCore* result = NULL;
    BlockModel* bm = MakeClippedBlockModelFromMaster(parent, from, to);
    if (bm) {
        result = CreateChild(parent, *bm, childAccession, childShortname);
        delete bm;
    }
    return result;
}

CCdCore* CRelationshipMaker::CreateChild(vector<CCdCore*>& parents, vector<unsigned int>& from, vector<unsigned int>& to, const string& childAccession, const string& childShortname)
{
    bool invalidData = false;
    unsigned int i, nParents = parents.size();
    vector<BlockModel> blocksOnParents;
    BlockModel* bm;

    if (nParents == 0 || nParents != from.size() || from.size() != to.size()) return NULL;

    for (i = 0; i < nParents && !invalidData; ++i) {
        if (parents[i] == NULL || (from[i] > to[i])) {
            invalidData = true;
        }
    }

    for (i =0; i < nParents && !invalidData; ++i) {

        CCdCore* result = NULL;
        bm = MakeClippedBlockModelFromMaster(parents[i], from[i], to[i]);
        if (bm) {
            blocksOnParents.push_back(*bm);
            delete bm;
        } else {
            invalidData = true;
        }
    }

    if (invalidData) {
        return NULL;
    } else {
        return CreateChild(parents, blocksOnParents, childAccession, childShortname);
    }
}

CCdCore* CRelationshipMaker::CreateChild(vector<CCdCore*>& parents, vector<BlockModel>& blocksOnParents, const string& childAccession, const string& childShortname)
{
    bool created = true;
    unsigned int i, nParents = parents.size();
    if (nParents != blocksOnParents.size() || nParents == 0) return NULL;

    //  Create and install the child object using the first parent.
    CCdCore* child = CreateChild(parents[0], blocksOnParents[0], childAccession, childShortname);
    if (!child) {
        return NULL;
    }

    for (i = 1; i < nParents && created; ++i) {
        created = CreateRelationship(child, parents[i], &(blocksOnParents[i]));
    }

    if (!created) {
        delete child;
        delete m_childDomain;
        m_childDomain = NULL;
        child = NULL;
    }

    return child;
}

CCdCore* CRelationshipMaker::CreateChild(CCdCore* parent, BlockModel& blocksOnParent, const string& childAccession, const string& childShortname)
{
    CCdCore* child = NULL;

    if (child) InstallChild(child);

    return child;
}

CCdCore* CRelationshipMaker::CreateChild(CCdCore* parent, const string& childAccession, const string& childShortname)
{
    CCdCore*  result = NULL;

    if (parent) {
        const CRef< CSeq_align>& masterAlign = parent->GetSeqAlign(0);
        if (masterAlign.NotEmpty()) {
            BlockModel bm(masterAlign, false);
            result = CreateChild(parent, bm, childAccession, childShortname);
        }
    }
    return result;
}
*/
        //////////////////////////////////////////////////
        //  CreateParent methods
        //////////////////////////////////////////////////
/*
//  From the child-to-be CD, make a copy of the CD to serve as a parent aligned
//  only over the specified region of the child's alignment.
//  NULL pointer returned on failure.
CCdCore* CRelationshipMaker::CreateParent(CCdCore* child, unsigned int from, unsigned int to, const string& parentAccession, const string& parentShortname)
{
    CCdCore* result = NULL;
    BlockModel* bm = MakeClippedBlockModelFromMaster(child, from, to);
    if (bm) {
        result = CreateParent(child, *bm, parentAccession, parentShortname);
        delete bm;
    }
    return result;
}

CCdCore* CRelationshipMaker::CreateParent(CCdCore* child, BlockModel& blocksOnParent, const string& parentAccession, const string& parentShortname)
{
    CCdCore* parent = NULL;

    //  If the creation succeeds and child not already installed, install child.
    if (parent && (!m_childDomain || m_childDomain->GetChildCD() != child)) {
        InstallChild(child);
    }

    return parent;
}

CCdCore* CRelationshipMaker::CreateParent(CCdCore* child, const string& parentAccession, const string& parentShortname)
{
    CCdCore*  result = NULL;

    if (child) {
        const CRef< CSeq_align>& masterAlign = child->GetSeqAlign(0);
        if (masterAlign.NotEmpty()) {
            BlockModel bm(masterAlign, false);
            result = CreateParent(child, bm, parentAccession, parentShortname);
        }
    }
    return result;
}
*/

        //////////////////////////////////////////////////
        //  CreateRelationship methods
        //////////////////////////////////////////////////

int CRelationshipMaker::CreateRelationship(CCdCore* child, CCdCore* parent, bool degenerate)
{
    int result = RELMAKER_UNSPECIFIED_ERROR;

    if (child && parent) {
        result = CreateRelationship(child, parent, NULL, degenerate);
    }
    return result;
}

//  From existing parent and child CDs, create a relationship over the indicated
//  range of the parent's alignment.  On success, child's ancestors are updated.
//  'false' returned on failure.
int CRelationshipMaker::CreateRelationship(CCdCore* child, CCdCore* parent, unsigned int fromOnParentMaster, unsigned int toOnParentMaster, bool degenerate)
{
    int result = RELMAKER_BAD_BLOCK_MODEL_RANGE;
    BlockModel* bm = MakeClippedBlockModelFromMaster(parent, fromOnParentMaster, toOnParentMaster);
    if (bm) {
        result = CreateRelationship(child, parent, bm, degenerate);
        delete bm;
    }
    return result;
}

int CRelationshipMaker::CreateRelationship(CCdCore* child, CCdCore* parent, const BlockModel* blocksOnParentMaster, bool degenerate)
{
    int result = RELMAKER_UNSPECIFIED_ERROR;
    CDomain_parent::EParent_type type;

    CGuideAlignmentFactory gaFactory(m_overlapPercentage, true);  //  for now, always allow a consensus as master
    gaFactory.SetMakeDegenerateGuides(degenerate);

    //  Step 1:  Try and make a guide alignment between the CDs, using the family when present
    //  and if we are not forcing the parent/child to share a common sequence.
    //  The guide has the *parent* as the master after this step.
    CGuideAlignment_Base* guide;
    if (m_forceSharedSequenceInterval) {
        guide = gaFactory.MakeGuide(parent, child);
    } else {
        guide = gaFactory.MakeGuide(parent, m_parentFamily, child, m_childFamily);
    }

    if (!guide || !guide->IsOK()) {
        result = (m_forceSharedSequenceInterval) ? RELMAKER_SHARED_SEQUENCE_GUIDE : RELMAKER_GUIDE_ALIGNMENT;
        m_lastErrorMsg = ResultCodeToString(result);
        if (guide) {
            m_lastErrorMsg += "\n" + guide->GetErrors();
        }
        delete guide;
        guide = NULL;
    }

    //  Step 2:  intersect with the specified block model (making sure there's a non-empty intersection)
    if (guide && blocksOnParentMaster) {
        guide->Intersect(*blocksOnParentMaster);
        if (!guide->IsOK()) {
            result = RELMAKER_NO_INTERSECTION_WITH_BLOCK_MODEL;
            m_lastErrorMsg = ResultCodeToString(result) + "\n" + guide->GetErrors();
//            cdLog::ErrorPrintf("Guide does not intersect with specified block model:\n%s\n", guide->GetErrors().c_str());
            delete guide;
            guide = NULL;
        }
    }

    //  Step 3:  create a CChildDomain, package guide in a CLinkToParent and add it to CChildDomain
    //  and update with the new parents.

    if (guide) {
        //  When dealing w/ the same child, keeps the m_childDomain object
        //  which may have previously-added new links.  Otherwise, clean up the
        //  existing m_childDomain and create a new instance from 'child'.
        if (!m_childDomain || m_childDomain->GetChildCD() != child) {
            InstallChild(child);
        }

        type = InferRelationshipTypeNeeded(parent, blocksOnParentMaster);

        //  If the guide has the child as slave, reverse the guide; the m_childDomain object expects the *child* as master.
        if (guide->GetMasterCD() != child && guide->GetSlaveCD() == child) {
            guide->Reverse();
        }

        CLinkToParent* link = new CLinkToParent(guide, type);
        if (m_childDomain->AddParentLink(link)) {
            if (m_autoUpdateChild) {
                m_childDomain->UpdateParentage(m_keepExistingParents);
            }
            result = RELMAKER_SUCCESS;
        } else {
            result = RELMAKER_LINK_NOT_ADDED;
            m_lastErrorMsg = ResultCodeToString(result) + "\n" + guide->GetErrors();
            delete link;
            delete guide;
            guide = NULL;
        }
    }

    if (result == RELMAKER_SUCCESS) {
        m_lastErrorMsg.erase();
    }

//    return (guide && guide->IsOK());
    return result;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

