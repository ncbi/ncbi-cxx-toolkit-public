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
 *          General representation of a child domain and the links
 *          to its parent domains.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuChildDomain.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

//////////////////////////////////////////////////
//
//  CLinkToParent
//
//////////////////////////////////////////////////


bool CLinkToParent::AddMask(const BlockModel& bm) 
{
    if (!m_guideAlignment) return false;

    const BlockModel& guideMaster = m_guideAlignment->GetGuide().getMaster();
    bool result = guideMaster.overlap(bm);
    if (result) {
        m_maskedRegions.push_back(bm);
    }
    return result;
}

bool CLinkToParent::AddMask(unsigned int from, unsigned int to)
{
    bool result = false;
    if (!m_guideAlignment || to < from) return result;

    BlockModel bm;
    Block block(from, to - from + 1);
    CRef<CSeq_id> guideSeqId = m_guideAlignment->GetGuide().getMaster().getSeqId();
    if (guideSeqId.NotEmpty()) {
        bm.setSeqId(guideSeqId);
        bm.addBlock(block);
        result = AddMask(bm);
    } 
    return result;
}

//  Assumes the SeqIds in all input block models are the same so that the block 
//  positions refer to the same coordinate system.
//  keepOriginalBlockBounds:  if true, break blocks in the union at all breaks in the input.
//  if false, blocks separated in the input block models may be merged.
BlockModel BlockModelUnion(const vector<BlockModel>& blockModels, bool keepOriginalBlockBounds)
{
    //  The combined block model is the union of all input block models.

    BlockModel unionBM;
    unsigned int nBlocks, nBlockModels = blockModels.size();
    if (nBlockModels == 0) return unionBM;

    //  If there's only one block model, we can simply return it.
    if (nBlockModels == 1) {
        return blockModels[0];
    }

    int pos;
    unsigned int i, j;
    set<int> unionPositions, originalBounds;
    set<int>::iterator posIt, posEnd, boundsEnd;

    vector<Block>& combinedBlocks = unionBM.getBlocks();
    unionBM.setSeqId(blockModels[0].getSeqId());

    for (i = 0; i < nBlockModels; ++i) {
        const vector<Block>& blocks = blockModels[i].getBlocks();
        nBlocks = blocks.size();
        for (j = 0; j < nBlocks; ++j) {
            for (pos = blocks[j].getStart(); pos <= blocks[j].getEnd(); ++pos) {
                unionPositions.insert(pos);
            }
            if (keepOriginalBlockBounds) {
                originalBounds.insert(blocks[j].getEnd());
            }
        }
    }

    bool inBlock = false;
    posIt = unionPositions.begin();
    posEnd = unionPositions.end();
    boundsEnd = originalBounds.end();
    int blockId = 0;
    int start = *posIt;

    //  Build the union block model.
    //  In this loop, 'pos' is the position from the previous iteration.
    for (; posIt != posEnd; ++posIt) {

        //  start a block
        if (!inBlock) {
            start = *posIt;
            inBlock = true;
        } else {
            //  If skipped a position, or at a kept block bound, start a new block.
            if (pos != *posIt - 1 || (keepOriginalBlockBounds && originalBounds.find(pos) != boundsEnd)) {
                inBlock = false;
                string s = "adding block:  [" + NStr::IntToString(start) + "  " + NStr::IntToString(pos) + "]";
                if (pos == *posIt - 1) s.append("  due to end of an input block");
                combinedBlocks.push_back(Block(start, pos - start + 1, blockId));
                start = *posIt;
                ++blockId;
            }
        }
        pos = *posIt;
    }

    if (inBlock) {
        string s = "adding last block:  [" + NStr::IntToString(start) + "  " + NStr::IntToString(pos) + "]";
        combinedBlocks.push_back(Block(start, pos - start + 1, blockId));
    }
    return unionBM;
}

CGuideAlignment_Base* CLinkToParent::GetMaskedGuide() const
{
    if (!m_guideAlignment) return NULL;

    CGuideAlignment_Base* maskedGuide = m_guideAlignment->Copy();

    if (maskedGuide && m_maskedRegions.size() > 0) {
        BlockModel mask = BlockModelUnion(m_maskedRegions, false);
        maskedGuide->Mask(mask);
        if (maskedGuide->GetGuide().getMaster().getTotalBlockLength() <= 0) {
            delete maskedGuide;
            maskedGuide = NULL;
        }
    }

    return maskedGuide;
}

//////////////////////////////////////////////////
//
//  CChildDomain
//
//////////////////////////////////////////////////

string CChildDomain::GetDomainParentAccession(const CDomain_parent& domainParent)
{
    string acc = "";
    const CCdd_id& ancestorId = domainParent.GetParentid();
    if (ancestorId.IsGid())
        acc = ancestorId.GetGid().GetAccession();
    return acc;
}

void CChildDomain::Cleanup() {
    PurgeAddedLinks();
    m_originalAncestors.clear();
}

void CChildDomain::Initialize(CCdCore* cd) {
    m_childCD = cd;
    if (cd && cd->IsSetAncestors()) {
        m_originalAncestors = cd->GetAncestors();
    }
}

bool CChildDomain::AddLinkToOriginalAncestor(CLinkToParent* link)
{
    bool result = false;
    if (!link || !m_childCD || !link->GetFullGuide() || (link->GetFullGuide()->GetMasterCD() != m_childCD)) return result;

    string ancestorAcc = link->GetFullGuide()->GetSlaveCDAcc();
    CCdd::TAncestors::iterator ancIt = m_originalAncestors.begin(), ancEnd = m_originalAncestors.end();
    for (; ancIt != ancEnd; ++ancIt) {
        if ((*ancIt).NotEmpty() && (GetDomainParentAccession(**ancIt) == ancestorAcc)) {
            m_preExistingParents.insert(TPEPVT(ancestorAcc, link));
            result = true;
        }
    }
    return result;
}

bool CChildDomain::AddParentLink(CLinkToParent* link)
{
    bool result = false;
    //  Sanity check that link is to this CD.
    if (link && link->GetFullGuide() && (link->GetFullGuide()->GetMasterCD() == m_childCD)) {
        if (CreateDomainParent(link)) {
            m_parentLinks.push_back(link);
            result = true;
        }
    } 
    return result;
}

bool CChildDomain::CreateDomainParent(CLinkToParent* link)
{
    bool result = true;
    unsigned int nParentsNow = m_childCD->SetAncestors().size();
    CRef< CDomain_parent > newParent(new CDomain_parent);

    newParent->SetParent_type(link->GetType());

    //  By convention the guide master is the child; guide slave is the parent.
    const CCdCore* parent = link->GetFullGuide()->GetSlaveCD();
    if (parent && parent->GetId().Get().size() > 0) {
        CRef< CCdd_id > cddId = parent->GetId().Get().front();
        newParent->SetParentid(*cddId);
    } else {
        result = false;
    }

    CGuideAlignment_Base* maskedGuide = link->GetMaskedGuide();
    if (maskedGuide) {
        CRef<CSeq_align> parentAlign = maskedGuide->GetGuideAsSeqAlign();  
        if (parentAlign.NotEmpty()) {
            newParent->SetSeqannot().SetData().SetAlign().push_back(parentAlign);
        } else {
            result = maskedGuide->IsOK();  //  a degenerate guide may have no seqAlign
        }
        delete maskedGuide;
    } else {
        result = false;
    }

    if (result) {
        m_addedAncestors.push_back(newParent);
    } else {
        newParent.Reset();
    }
    return result;
}

CCdCore* CChildDomain::UpdateParentage(bool keepExistingParents)
{
    if (!m_childCD) return NULL;
    unsigned int nParentsNow = m_childCD->SetAncestors().size();

    if (keepExistingParents) {
        RollbackAncestors(false);
    } else {
        ResetAncestors();
    }

    UpdateAncestorListInChild();
    return m_childCD;
}

void CChildDomain::UpdateAncestorListInChild()
{
    CCdd::TAncestors& ancList = m_childCD->SetAncestors();
    CCdd::TAncestors::iterator ancIt, ancEnd;
    string ancestorAcc;
    CLinkToParent* link;
    TPEPIt pepIt, pepEnd = m_preExistingParents.end();
    unsigned int minAdded;
    unsigned int nAdded = m_addedAncestors.size();
    unsigned int nExisting = ancList.size();

    if (nAdded == 0) return;

    //  Assume that the pre-existing ancestors were self-consistent and only require
    //  adjustment if extra links were added.
    if (nExisting == 0) {
        minAdded = 1;
    } else {
        minAdded = 0;

        //  Fix up guides for pre-existing ancestors...
        //  ... although we can only add a guide if pointer to pre-existing parent has been added.
        //  Only classical ancestors should need a guide alignment.
        ancEnd = ancList.end();
        for (ancIt = ancList.begin(); ancIt != ancEnd; ++ancIt) {
            CRef< CDomain_parent >& parent = *ancIt;

            //  Don't touch a domain parent that has an existing guide.
            if (parent->IsSetSeqannot()) continue;

            ancestorAcc = GetDomainParentAccession(*parent);
            if (parent->GetParent_type() == CDomain_parent::eParent_type_classical) {
                //  make classical parents fusion parents for now...
                parent->SetParent_type(CDomain_parent::eParent_type_fusion);

                //  add a guide aligment, if a link was provided...
                //  ...use the first such link found.
                pepIt = m_preExistingParents.find(ancestorAcc);
                if (pepIt != pepEnd) {
                    link = pepIt->second;
                    const CGuideAlignment_Base* guide = (link->HasMask()) ? link->GetMaskedGuide() : link->GetFullGuide();
                    if (guide) {
                        CRef<CSeq_align> parentAlign = guide->GetGuideAsSeqAlign();  
                        if (parentAlign.NotEmpty()) {
                            parent->ResetSeqannot();
                            parent->SetSeqannot().SetData().SetAlign().push_back(parentAlign);
                        }

                        //  Only have ownership if guide was masked.
                        if (link->HasMask()) delete guide;
                    }
                }
            }
        }
    }

    //  Only need to do anything if more than 'minAdded' ancestors were added.
    if (nAdded > minAdded) {
        ancEnd = m_addedAncestors.end();
        for (ancIt = m_addedAncestors.begin(); ancIt != ancEnd; ++ancIt) {
            //  make all of them fusion parents for now...
            CRef< CDomain_parent >& parent = *ancIt;
            if (parent->GetParent_type() != CDomain_parent::eParent_type_fusion) {
                parent->SetParent_type(CDomain_parent::eParent_type_fusion);
            }
        }
    }
    ancList.insert(ancList.end(), m_addedAncestors.begin(), m_addedAncestors.end());
}

void CChildDomain::PurgeAddedLinks()
{
    TPEPIt pepIt = m_preExistingParents.begin(), pepEnd = m_preExistingParents.end();
    for (; pepIt != pepEnd; ++pepIt) {
        delete pepIt->second;
    }
    m_preExistingParents.clear();

    for (unsigned int i = 0; i < m_parentLinks.size(); ++i) {
        delete m_parentLinks[i];
    }
    m_parentLinks.clear();
    m_addedAncestors.clear();
}

void CChildDomain::ResetAncestors()
{
    if (m_childCD) m_childCD->ResetAncestors();
}

//  Restores the original state of the ancestors to the CD.
void CChildDomain::RollbackAncestors(bool purgeAddedLinks)
{
    ResetAncestors();
    if (m_childCD && m_originalAncestors.size() > 0) {
        m_childCD->SetAncestors() = m_originalAncestors;
    }
    if (purgeAddedLinks) PurgeAddedLinks();
}

bool CChildDomain::IsParentageValid(bool doUpdateFirst) const
{
    bool result = false;
    return result;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

