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
#include <corelib/ncbidbg.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>
#include <algo/structure/cd_utils/cd_guideUtils/cuGuideAlignment.hpp>
#include <algo/structure/cd_utils/cuBlockIntersector.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

string GuideChainToString(const TGuideChain& chain) {
    string s;
    unsigned int i, len = chain.size();
    if (len > 0) {
        for (i = 0; i < len; ++i) {
            s += "Chain Link " + NStr::UIntToString(i) + ":  " + chain[i].Print(true);
        }
    } else {
        s = "Empty chain";
    }
    return s;
}


// ============================================
//      CGuideAlignment_Base implementation
// ============================================

void CGuideAlignment_Base::Initialize()
{
    Cleanup();
}

void CGuideAlignment_Base::Cleanup()
{
    m_isOK = false;
    m_errors.clear();

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

string CGuideAlignment_Base::GetErrors() const
{
    static const string slashN("\n");

    string allErrors;
    for (unsigned int i = 0; i < m_errors.size(); ++i) {
        allErrors += m_errors[i] + slashN;
    }
    return allErrors;
}


bool CGuideAlignment_Base::Make(const CCdCore* cd1, const CCdCore* cd2)
{
    bool result = (cd1 && cd2);

    //  Remove any previous block model pair, so that in case of errors IsOK returns false.
    Reset();

    if (!result) {
        if (!cd1) {
            m_errors.push_back("CGuideAlignment error:  First CD is null - can't make guide alignment.\n");
        }
        if (!cd2) {
            m_errors.push_back("CGuideAlignment error:  Second CD is null - can't make guide alignment.\n");
        }
        return false;
    }
    
    SGuideInput input1(const_cast<CCdCore*>(cd1), NULL);
    SGuideInput input2(const_cast<CCdCore*>(cd2), NULL);
    return Make(input1, input2);
}

/*
bool CGuideAlignment_Base::Make(const CCdCore* cd1, const CCdCore* cd2, CDFamily* family)
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
    
    SGuideInput input1(const_cast<CCdCore*>(cd1), family);
    SGuideInput input2(const_cast<CCdCore*>(cd2), family);
    return Make(input1, input2);
}

bool CGuideAlignment_Base::Make(const CCdCore* cd1, CDFamily* family1, const CCdCore* cd2, CDFamily* family2)
{
    bool result = (family1 && family2 && cd1 && cd2);

    //  Remove any previous block model pair, so that in case of errors IsOK returns false.
    Reset();

    if (!result) {
        if (!family1) {
            m_errors.push_back("CGuideAlignment error:  Null first family - can't make guide alignment.\n");
        }
        if (!family2) {
            m_errors.push_back("CGuideAlignment error:  Null second family - can't make guide alignment.\n");
        }
        if (!cd1) {
            m_errors.push_back("CGuideAlignment error:  First CD is null - can't make guide alignment.\n");
        }
        if (!cd2) {
            m_errors.push_back("CGuideAlignment error:  Second CD is null - can't make guide alignment.\n");
        }
        return false;
    }
    
    SGuideInput input1(const_cast<CCdCore*>(cd1), family1);
    SGuideInput input2(const_cast<CCdCore*>(cd2), family2);
    result = Make(input1, input2);

    MakeChains(input1, input2, NULL);

    return result;
}
*/

/*
//  Returns true if 'maskedBm' was modified; false if there was no effect.
bool CGuideAlignment_Base::Mask(BlockModel& maskedBm, const vector<Block>& maskBlocks)
{
    vector<Block> maskedBlocks;
    vector<Block>& originalBlocks = maskedBm.getBlocks();
    unsigned int i, nOrigBlocks = originalBlocks.size();
    unsigned int j, nMaskBlocks = maskBlocks.size();
    int origTotalLength = maskedBm.getTotalBlockLength();
    int newBlockId = 0;
    int pos, start, len;
    int origStart, origEnd;
    int maskBlockStart, maskBlockEnd, maskFirst, maskLast;
    bool hasEffect;

    if (nOrigBlocks == 0 || nMaskBlocks == 0) return false;

    //  Collect all mask positions to simplify search code.
    set<int> maskPositions;
    set<int>::iterator maskPosEnd;
    for (i = 0; i < nMaskBlocks; ++i) {
        maskBlockStart = maskBlocks[i].getStart();
        maskBlockEnd = maskBlocks[i].getEnd();
        for (j = maskBlockStart; j <= maskBlockEnd; ++j) maskPositions.insert(j);
    }
    maskPosEnd = maskPositions.end();

    maskFirst = maskBlocks[0].getStart();
    maskLast = maskBlocks.back().getEnd();

    for (i = 0; i < nOrigBlocks; ++i) {
        origStart = originalBlocks[i].getStart();
        origEnd = originalBlocks[i].getEnd();

        //  If origBlock does not intersects the maskBlocks footprint, it is unmasked and can be directly copied.
        if (origEnd < maskFirst || origStart > maskLast) {
            maskedBlocks.push_back(originalBlocks[i]);
            maskedBlocks.back().setId(newBlockId);
            ++newBlockId;
            continue;
        }

        start = -1;
        len = 0;
        for (pos = origStart; pos <= origEnd; ++pos) {

            //  If position is masked; end current block.
            if (maskPositions.find(pos) != maskPosEnd) {
                if (len > 0) {
                    maskedBlocks.push_back(Block(start, len, newBlockId));
                    ++newBlockId;
                }
                len = 0;
                start = -1;
            }
            else
            {
                //  Found the first position in a new block.
                if (len == 0) {
                    start = pos;
                }
                ++len;
            }

        }  //  end loop on original block positions

        //  Make sure to include the block at the end...
        if (len > 0) {
            maskedBlocks.push_back(Block(start, len, newBlockId));
        }

    }  //  end loop on original blocks

    _ASSERT(maskedBm.getTotalBlockLength() <= origTotalLength);
    hasEffect = (maskedBm.getTotalBlockLength() != origTotalLength);
    if (hasEffect) {
        originalBlocks.clear();
        originalBlocks = maskedBlocks;
    }

    return hasEffect;
}
//  Returns true if 'maskedBm' was modified; false if there was no effect.
bool Mask(BlockModel& maskedBm, const BlockModel& mask)
{
    bool result = (SeqIdsMatch(maskedBm.getSeqId(), mask.getSeqId()));
    if (result) {
        result = CGuideAlignment_Base::Mask(maskedBm, mask.getBlocks());
    }
    return result;
}

//  Returns true if 'bmp' was modified; false if there was no effect.
//  Mask using blocks and not a BlockModel to factor out logic to verify
//  SeqIds are common between the pair and the mask.  Assumes this 
//  was already done.  Also assumes that the BlockModelPair is valid.
bool Mask(BlockModelPair& bmp, const vector<Block>& maskBlocks, bool maskBasedOnMaster)
{
    bool hasEffect = false;
    BlockModel& maskedBm = (maskBasedOnMaster) ? bmp.getMaster() : bmp.getSlave();
    vector<Block>& newBlocks = maskedBm.getBlocks();
    unsigned int i, nBlocks = newBlocks.size();
    int pos, mappedPos;

    //  First, mask the primary BlockModel in the pair.
//    if (maskBlocks.size() > 0 && nBlocks > 0 && CGuideAlignment_Base::Mask(maskedBm, maskBlocks)) {
    if (maskBlocks.size() > 0 && nBlocks > 0 && maskedBm.mask(maskBlocks)) {

        hasEffect = true;

        //  Then, fix up the other BlockModel in the pair to match the new masked primary BlockModel.
        vector<Block>& newOtherBlocks = (maskBasedOnMaster) ? bmp.getSlave().getBlocks() : bmp.getMaster().getBlocks(); 
        newOtherBlocks.clear();
        for (i = 0; i < nBlocks; ++i) {
            Block& b = newBlocks[i];
            pos = b.getStart();
            mappedPos = (maskBasedOnMaster) ? bmp.mapToSlave(pos) : bmp.mapToMaster(pos);
            if (mappedPos >= 0) {  // should never fail if bmp.isValid is true.
                newOtherBlocks.push_back(Block(mappedPos, b.getLen(), b.getId()));
            }
        }
    }
    return hasEffect;
}
*/


void CGuideAlignment_Base::Intersect(const BlockModel& bm, bool basedOnMasterWhenSameSeqIdInGuide)
{
    set<int> forcedBreaks;
    int lastAlignedGuide, lastAlignedTemplate, maxPos, mappedPos;

    //  This will fail for degenerate guides.
    if (!IsBMPValid()) return;

    //  First, figure out which block model in the pair matches the sequence from 'bm'.
    bool doReverse = false;
    bool bmMatchesMaster, bmMatchesSlave;
    unsigned int i, nBlocks;
    CRef<CSeq_id> bmSeqId = bm.getSeqId();
    bmMatchesMaster = SeqIdsMatch(bmSeqId, m_guideBlockModelPair.getMaster().getSeqId());
    bmMatchesSlave = SeqIdsMatch(bmSeqId, m_guideBlockModelPair.getSlave().getSeqId());

    if (!bmMatchesMaster && !bmMatchesSlave) {
        return;
    } else if (bmMatchesMaster && bmMatchesSlave) {
        if (!basedOnMasterWhenSameSeqIdInGuide) {
            doReverse = true;
        }
    } else if (bmMatchesSlave) {
        doReverse = true;
    }

    if (doReverse) m_guideBlockModelPair.reverse();

    lastAlignedGuide = m_guideBlockModelPair.getMaster().getLastAlignedPosition();
    lastAlignedTemplate = bm.getLastAlignedPosition();
    maxPos = max(lastAlignedGuide, lastAlignedTemplate);

    if (maxPos > 0) {

        //  Carry out the intersection...

        BlockIntersector intersector(maxPos);
        intersector.addOneAlignment(m_guideBlockModelPair.getMaster());
        intersector.addOneAlignment(bm);

        const vector<Block>& blocks = m_guideBlockModelPair.getMaster().getBlocks();
        nBlocks = blocks.size();
        for (i = 0; i < nBlocks - 1; ++i) {  //  '-1' as I don't care about the end of the C-terminal block
            forcedBreaks.insert(blocks[i].getEnd());
        }

        BlockModel* intersectedBm = intersector.getIntersectedAlignment(forcedBreaks);

        if (intersectedBm) {

            BlockModel newSlave;
            newSlave.setSeqId(m_guideBlockModelPair.getSlave().getSeqId());

            //  Fix up the slave to match the new intersected blocks.
            vector<Block>& newSlaveBlocks = newSlave.getBlocks();
            vector<Block>& intersectedBlocks = intersectedBm->getBlocks();
            nBlocks = intersectedBlocks.size();
            for (i = 0; i < nBlocks; ++i) {
                Block& b = intersectedBlocks[i];
                mappedPos = m_guideBlockModelPair.mapToSlave(b.getStart());
                if (mappedPos >= 0) {  // should never fail if bmp.isValid is true.
                    newSlaveBlocks.push_back(Block(mappedPos, b.getLen(), b.getId()));
                }
            }

            m_guideBlockModelPair.getSlave() = newSlave;
            m_guideBlockModelPair.getMaster() = *intersectedBm;
        }
        delete intersectedBm;
    }

    //  Put guide back in original orientation if necessary.
    if (doReverse) m_guideBlockModelPair.reverse();

    if (m_guideBlockModelPair.getMaster().getTotalBlockLength() <= 0) {
        m_isOK = false;
        m_errors.push_back("Intersect called with a non-intersecting BlockModel.");
    }

    return;
}


void CGuideAlignment_Base::Mask(const BlockModel& mask, bool basedOnMasterWhenSameSeqIdInGuide)
{
    //  This will fail for degenerate guides.
    if (!IsBMPValid()) return;

    //  First, figure out which block model in the pair matches the sequence from the mask.
    bool doReverse = false;
    bool maskMatchesMaster, maskMatchesSlave;
    CRef<CSeq_id> maskSeqId = mask.getSeqId();
    maskMatchesMaster = SeqIdsMatch(maskSeqId, m_guideBlockModelPair.getMaster().getSeqId());
    maskMatchesSlave = SeqIdsMatch(maskSeqId, m_guideBlockModelPair.getSlave().getSeqId());

    if (!maskMatchesMaster && !maskMatchesSlave) {
        return;
    } else if (maskMatchesMaster && maskMatchesSlave) {
        if (!basedOnMasterWhenSameSeqIdInGuide) {
            doReverse = true;
        }
    } else if (maskMatchesSlave) {
        doReverse = true;
    }

    if (doReverse) m_guideBlockModelPair.reverse();

    m_guideBlockModelPair.mask(mask.getBlocks(), true);

    //  Put guide back in original orientation if necessary.
    if (doReverse) m_guideBlockModelPair.reverse();

    if (m_guideBlockModelPair.getMaster().getTotalBlockLength() <= 0) {
        m_isOK = false;
        m_errors.push_back("Called Mask using a mask that completely covered this object.");
    }

    return;
}

void CGuideAlignment_Base::CopyBase(CGuideAlignment_Base* guideCopy) const
{
    BlockModel* bm;
    if (!guideCopy) return;

    guideCopy->m_isOK = m_isOK;
    guideCopy->m_errors = m_errors;

    guideCopy->m_guideBlockModelPair = m_guideBlockModelPair;

    guideCopy->m_chain1 = m_chain1;
    guideCopy->m_chain2 = m_chain2;
    guideCopy->m_families = m_families;

    //  CGuideAlignment_Base owns the BlockModel objects -> do a deep copy.
    guideCopy->m_blockModels.clear();
    if (m_blockModels.size() > 0) {
        for (unsigned int i = 0; i < m_blockModels.size(); ++i) {
            bm = (m_blockModels[i]) ? new BlockModel(*m_blockModels[i]) : NULL;
            guideCopy->m_blockModels.push_back(bm);
        }
    }
}

void CGuideAlignment_Base::Reverse()
{
    TGuideChain tmp = m_chain1;
    m_chain1 = m_chain2;
    m_chain2 = tmp;

    m_guideBlockModelPair.reverse();
}

void CGuideAlignment_Base::MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd)
{

    m_chain1.clear();
    m_chain1.push_back(SGuideAlignmentLink(guideInput1.cd, guideInput1.rowInCd));
    if (commonCd) {
        m_chain1.push_back(SGuideAlignmentLink(commonCd));
    }

    m_chain2.clear();
    m_chain2.push_back(SGuideAlignmentLink(guideInput2.cd, guideInput2.rowInCd));
    if (commonCd) {
        m_chain2.push_back(SGuideAlignmentLink(commonCd));
    }
}



bool CGuideAlignment_Base::ReplaceMaster(const CRef< CSeq_align >& seqAlign, bool useFirst)
{
    return Replace(true, seqAlign, useFirst);
}

bool CGuideAlignment_Base::ReplaceSlave(const CRef< CSeq_align >& seqAlign, bool useFirst)
{
    return Replace(false, seqAlign, useFirst);
}

bool CGuideAlignment_Base::Replace(bool replaceMaster, const CRef< CSeq_align >& seqAlign, bool useFirst)
{
    bool result = seqAlign.NotEmpty() && IsBMPValid();   //  m_guideBlockModelPair.isValid();
    int remasteredLen;

    if (result) {
        BlockModelPair guideCopy(m_guideBlockModelPair);
        BlockModelPair bmp(seqAlign);
        if (useFirst) bmp.reverse();  //  the row used from seqAlign needs to be the slave in the bmp

        if (!replaceMaster) guideCopy.reverse();  //  for slave, need to reverse to make it the master
        remasteredLen = guideCopy.remaster(bmp);

        if (remasteredLen > 0) {
            if (!replaceMaster) guideCopy.reverse();  //  put the slave back
            m_guideBlockModelPair = guideCopy;
        } else {
            result = false;
        }
    }
    return result;
}


int CGuideAlignment_Base::MapToMaster(unsigned int slavePos) const
{
    return m_guideBlockModelPair.mapToMaster((int) slavePos);
}

int CGuideAlignment_Base::MapToSlave(unsigned int masterPos) const
{
    m_guideBlockModelPair.reverse();
    int result = m_guideBlockModelPair.mapToMaster((int) masterPos);
    m_guideBlockModelPair.reverse();

    return result;
}

const CCdCore* CGuideAlignment_Base::GetMasterCD() const
{
    return (m_chain1.size() > 0) ? m_chain1[0].cd : NULL;
}

string CGuideAlignment_Base::GetMasterCDAcc() const 
{
    const CCdCore* cd = (m_chain1.size() > 0) ? m_chain1[0].cd : NULL;
    return (cd) ? cd->GetAccession() : "";
}

string CGuideAlignment_Base::GetMasterRowIdStr() const
{
    CRef< CSeq_id > seqId;
    string idStr = kEmptyStr;
    if (m_chain1.size() > 0 && m_chain1[0].cd->GetSeqIDFromAlignment(m_chain1[0].row, seqId)) {
        idStr = GetSeqIDStr(seqId);
    }
    return idStr;
}

unsigned int CGuideAlignment_Base::GetMasterCDRow() const
{
    return (m_chain1.size() > 0) ? m_chain1[0].row : kMax_UInt;
}

const CCdCore* CGuideAlignment_Base::GetSlaveCD() const
{
    return (m_chain2.size() > 0) ? m_chain2[0].cd : NULL;
}
string CGuideAlignment_Base::GetSlaveCDAcc() const
{
    const CCdCore* cd = (m_chain2.size() > 0) ? m_chain2[0].cd : NULL;
    return (cd) ? cd->GetAccession() : "";
}

string CGuideAlignment_Base::GetSlaveRowIdStr() const
{
    CRef< CSeq_id > seqId;
    string idStr = kEmptyStr;
    if (m_chain2.size() > 0 && m_chain2[0].cd->GetSeqIDFromAlignment(m_chain2[0].row, seqId)) {
        idStr = GetSeqIDStr(seqId);
    }
    return idStr;
}

unsigned int CGuideAlignment_Base::GetSlaveCDRow() const
{
    return (m_chain2.size() > 0) ? m_chain2[0].row : kMax_UInt;
}

const CCdCore* CGuideAlignment_Base::GetCommonCD() const
{
    unsigned int n = m_chain1.size(), m = m_chain2.size();
    const CCdCore* commonCd = (n > 0 && m>0 && m_chain1[n-1].cd == m_chain2[m-1].cd) ? m_chain1[n-1].cd : NULL;
    return commonCd;
}
string CGuideAlignment_Base::GetCommonCDAcc() const
{
    unsigned int n = m_chain1.size(), m = m_chain2.size();
    const CCdCore* commonCd = (n > 0 && m>0 && m_chain1[n-1].cd == m_chain2[m-1].cd) ? m_chain1[n-1].cd : NULL;
    return (commonCd) ? commonCd->GetAccession() : kEmptyStr;
}

string CGuideAlignment_Base::GetCommonCDRowIdStr(bool onMaster) const
{
    CRef< CSeq_id > seqId;
    string idStr = kEmptyStr;
    unsigned int n = (onMaster) ? m_chain1.size() : m_chain2.size();
    const CCdCore* commonCd = GetCommonCD();
    if (commonCd && n > 0) {
        if (onMaster && m_chain1[n-1].cd->GetSeqIDFromAlignment(m_chain1[n-1].row, seqId)) {
            idStr = GetSeqIDStr(seqId);
        } else if (!onMaster && m_chain2[n-1].cd->GetSeqIDFromAlignment(m_chain2[n-1].row, seqId)) {
            idStr = GetSeqIDStr(seqId);
        }
    }
    return idStr;
}

unsigned int CGuideAlignment_Base::GetCommonCDRow(bool onMaster) const
{
    unsigned int result = kMax_UInt, n = (onMaster) ? m_chain1.size() : m_chain2.size();
    const CCdCore* commonCd = GetCommonCD();
    if (commonCd && n > 0) {
        result = (onMaster) ? m_chain1[n-1].row : m_chain2[n-1].row;
    }
    return result;
}

string CGuideAlignment_Base::ToString() const
{
    string s;
    string masterInfo = "Master CD " + GetMasterCDAcc() + "; row " + NStr::UIntToString(GetMasterCDRow()) + " (" + GetMasterRowIdStr() + ")";
    string slaveInfo  = "Slave  CD " + GetSlaveCDAcc() + "; row " + NStr::UIntToString(GetSlaveCDRow()) + " (" + GetSlaveRowIdStr() + ")";
    string commonInfo;
    
    if (GetCommonCD()) {
        commonInfo = "Common CD " + GetCommonCDAcc() + "; master row " + NStr::UIntToString(GetCommonCDRow(true)) + " (" + GetCommonCDRowIdStr(true) + ")\n";
        commonInfo += "Common CD " + GetCommonCDAcc() + "; slave row " + NStr::UIntToString(GetCommonCDRow(false)) + " (" + GetCommonCDRowIdStr(false) + ")";
    } else {
        commonInfo = "No common CD\n";
    }

    s = masterInfo + "\n" + slaveInfo + "\n" + commonInfo + "\n";

    if (!m_isOK) {
        s += "    --> warning:  guide alignment is not marked 'OK'.\n";
    }

    const BlockModelPair& bmp = GetGuide();
    s += "master blocks:\n" + bmp.getMaster().toString();
    s += "slave  blocks:\n" + bmp.getSlave().toString();

    s+= "\nChain1:\n" + GuideChainToString(m_chain1);
    s+= "\nChain2:\n" + GuideChainToString(m_chain2);

    return s;
}

bool CGuideAlignment_Base::IsBMPValid() const
{
    return (m_guideBlockModelPair.getMaster().getBlocks().size() > 0 && m_guideBlockModelPair.isValid());
}

// ============================================
//      CDegenerateGuide implementation
// ============================================


string CDegenerateGuide::ToString() const
{
    string s;
    string masterInfo = "Master CD " + GetMasterCDAcc() + "; row " + NStr::UIntToString(GetMasterCDRow()) + " (" + GetMasterRowIdStr() + ")\n";
    string slaveInfo  = "Slave  CD " + GetSlaveCDAcc() + "; row " + NStr::UIntToString(GetSlaveCDRow()) + " (" + GetSlaveRowIdStr() + ")\n";
    string commonInfo;
    
    s = masterInfo + slaveInfo + "Degenerate Guide:  No mapping has been specified.\n";

    if (!m_isOK) {
        s += "    --> warning:  guide alignment is not marked 'OK'.\n";
    }
    return s;
}

CGuideAlignment_Base* CDegenerateGuide::Copy() const {
    CDegenerateGuide* guideCopy = new CDegenerateGuide();
    CopyBase(guideCopy);
    return guideCopy;
}

bool CDegenerateGuide::Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2)
{
    MakeChains(guideInput1, guideInput2, NULL);
    m_isOK = true;
    return true;
}

void CDegenerateGuide::MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd)
{

    m_chain1.clear();
    m_chain1.push_back(SGuideAlignmentLink(guideInput1.cd, guideInput1.rowInCd));

    m_chain2.clear();
    m_chain2.push_back(SGuideAlignmentLink(guideInput2.cd, guideInput2.rowInCd));
}



// ============================================
//      CFamilyBasedGuide implementation
// ============================================

const int CFamilyBasedGuide::m_defaultOverlapPercentage = 100;

void CFamilyBasedGuide::Initialize()
{
    Cleanup();

    //  Only put 2nd family info back to defaults.
    m_family2 = NULL;
    m_overlapPercentage2 = m_defaultOverlapPercentage;
    m_cleanupFamily2 = false;
}

void CFamilyBasedGuide::Cleanup()
{
    CGuideAlignment_Base::Cleanup();

    if (m_cleanupFamily1) {
        delete m_family1;
        m_family1 = NULL;
    }

    if (m_cleanupFamily2) {
        delete m_family2;
        m_family2 = NULL;
    }
}

bool CFamilyBasedGuide::SetOverlapPercentage(int percentage, bool firstFamily)
{
    bool result = (percentage > 0 && percentage <= 100);
    if  (result)
        if (firstFamily) {
            m_overlapPercentage1 = percentage;
        } else {
            m_overlapPercentage2 = percentage;
        }
    return result;
}

void CFamilyBasedGuide::SetSecondFamily(const ncbi::cd_utils::CDFamily* secondFamily, int percentage)
{
    if (m_family2 && m_cleanupFamily2) {
        delete m_family2;
    }
    m_family2 = secondFamily;
    m_cleanupFamily2 = false;
    SetOverlapPercentage(percentage, false);
}

bool CFamilyBasedGuide::InSameFamily(const CCdCore* cd1, const CCdCore* cd2, const CDFamily* family)
{
    bool result = (cd1 && cd2 && family);
    if (result) {
        CDFamilyIterator familyEnd = family->end();
        result = (family->findCD(const_cast<CCdCore*>(cd1)) != familyEnd && family->findCD(const_cast<CCdCore*>(cd2)) != familyEnd);
    }
    return result;
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE
