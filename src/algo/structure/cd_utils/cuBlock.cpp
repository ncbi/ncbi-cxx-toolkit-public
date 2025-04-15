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
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cuAlign.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

//Block //////////////////////////////////

Block::Block (int start, int len, int id)
	:m_len(len), m_start(start), m_id(id)
{
}

Block::Block (int start, int len)
	:m_len(len), m_start(start), m_id(-1)
{
}

Block::Block ()
    :m_len(-1), m_start(-1), m_id(-1)
{}

Block::Block(const Block& rhs)
:m_len(rhs.m_len), m_start(rhs.m_start), m_id(rhs.m_id)
{
}

const Block& Block::operator=(const Block& rhs)
{
	m_start = rhs.m_start;
	m_len = rhs.m_len;
	m_id = rhs.m_id;
	return *this;
}

bool Block::operator==(const Block& rhs) const
{
	return (m_start == rhs.m_start) && (m_len == rhs.m_len);
}

bool Block::operator!=(const Block& rhs) const
{
	return !((*this) == rhs);
}

bool Block::contain(const Block& rhs) const
{
	return (m_start <= rhs.getStart()) && (getEnd() >= rhs.getEnd());
}

bool Block::isIntersecting(const Block& rhs)const 
{
	int x0 = m_start;
	int x1 = getEnd();
	int y0 = rhs.m_start;
	int y1 = rhs.getEnd();
	for (int y = y0; y <= y1; y++)
	{
		if ( y >= x0 && y <= x1)
			return true;
	}
	return false;
}

//shrink rhs to the intersection of rhs with this 
bool Block::intersect(Block& rhs)const 
{
	int x0 = m_start;
	int x1 = getEnd();
	int y0 = rhs.m_start;
	int y1 = rhs.getEnd();
	int yStart = -1;
	//int yEnd = -1;
	bool intersected = false;
	int y = y0;
	for (; y <= y1; y++)
	{
		if (!intersected)
		{
			if ( y >= x0 && y <= x1)
			{
				intersected = true;
				yStart = y;
			}
		}
		else
		{
			if ( y < x0 || y > x1)
			{
				//yEnd = y - 1;
				break;
			}
		}
	}
	if (intersected)
	{
		rhs.m_start = yStart;
		rhs.setEnd(y-1);
	}
	return intersected;
}

//return this + deltaBlock
//Block Block::applyDelta(const DeltaBlock& delta) const
Block Block::operator+(const DeltaBlock& delta) const
{
	return Block(m_start + delta.deltaStart, m_len + delta.deltaLen, delta.subjectBlockID);
}

/* DeltaBlock:
	int subjectBlockID;
	int objectBlockID;
	int deltaStart;
	int deltaLen;
*/
//return this-object
//DeltaBlock Block::getDelta (const Block& object) const
DeltaBlock Block::operator-(const Block& object)const
{
	DeltaBlock delta = {m_id, object.m_id, m_start - object.m_start, m_len - object.m_len};
	return delta;
}

Block Block::extend (int nExt, int cExt) const
{
	int start = m_start + nExt;
	int len = m_len + cExt - nExt;
	return Block(start, len, m_id);
}

void Block::extendSelf (int nExt, int cExt)
{
	m_start = m_start + nExt;
	m_len = m_len + cExt - nExt;
}

void Block::addOffset(int nExt)
{
	m_start = m_start + nExt;
}

bool Block::concatenate(const SortedBlocks& blocks, Block& comBlock)
{
	if (blocks.size() ==0)
		return false;
	SortedBlocks::const_iterator sit = blocks.begin();
	comBlock.m_id = sit->m_id;
	comBlock.m_start = sit->m_start;
	comBlock.m_len = sit->m_len;
	sit++;
	for (; sit != blocks.end(); ++sit)
	{
		if( (comBlock.getEnd() + 1) != sit->m_start)
			//there is a gap between two adjacent blocks, can't join them
			return false;
		else
			comBlock.m_len += sit->m_len;
	}
	return true;
}

//BlockModel-------------------------------------------------

BlockModel::BlockModel() 
	: m_blocks(), m_seqId()
{
}

BlockModel::BlockModel(const CRef< CSeq_align> seqAlign, bool forSlave) 
	: m_blocks(), m_seqId()
{
	GetSeqID(seqAlign, m_seqId, forSlave);
	vector<int> lens, starts;
	GetBlockLengths(seqAlign, lens);
	GetBlockStarts(seqAlign, starts, !forSlave);
	assert(lens.size() == starts.size());
	for (unsigned int i = 0; i < lens.size(); i++)
	{
		m_blocks.push_back(Block(starts[i], lens[i], i));
	}
}

BlockModel::BlockModel(CRef< CSeq_id > seqId, bool withOneBlock)
	: m_seqId(seqId)
{
	if (withOneBlock)
	{
		Block block(0,1,0);
		m_blocks.push_back(block);
	}
}

BlockModel::BlockModel(const BlockModel& rhs)
	: m_blocks(rhs.m_blocks), m_seqId(rhs.m_seqId) 
{
}

void BlockModel::addBlock(Block& block)
{
	block.setId(m_blocks.size());
	m_blocks.push_back(block);
}

BlockModel& BlockModel::operator=(const BlockModel& rhs)
{
	m_seqId = rhs.m_seqId;
	m_blocks.assign(rhs.m_blocks.begin(), rhs.m_blocks.end());
	return *this;
}

bool BlockModel::isAlike(const BlockModel& rhs) const
{
	if( SeqIdsMatch(m_seqId, rhs.m_seqId) &&( m_blocks.size() == rhs.m_blocks.size()) )
		return true;
	else
		return false;
}


bool BlockModel::operator==(const BlockModel& rhs) const
{
	if (!isAlike(rhs))
		return false;
	for ( unsigned int i = 0; i < m_blocks.size(); i++)
	{
		if (m_blocks[i] != rhs.m_blocks[i])
			return false;
	}
	return true;
}

bool BlockModel::blockMatch(const BlockModel& rhs) const
{
	if (m_blocks.size() != rhs.m_blocks.size())
		return false;
	for ( unsigned int i = 0; i < m_blocks.size(); i++)
	{
		if (m_blocks[i].getLen() != rhs.m_blocks[i].getLen())
			return false;
	}
	return true;
}

bool BlockModel::completeModelExtendsIntoUnallowedGappedRegion(const BlockModel& completeModel, int sequenceLength, const vector<int>* commonBlockExt) const 
{
    unsigned int nBlocks = m_blocks.size(), commonBlockExtSize = (commonBlockExt) ? commonBlockExt->size() : 0;
    unsigned int i, j, k;
    int seqLen = -1;
    int thisStart, thisLen, thisContiguousLen;
    int commonNTermBlockZeroExt, nTermShift;
    int completeBlockStart, completeBlockLen;
    int prevCTermBlockExt, allowedCTermBlockExt, lastCTermBlockExt;
    int blockNumberOnThisOfCompleteBlockStart;
    vector<int> commonCTermBlockExt, gapSizeAfterBlockInThis;
    bool useInputBlockExtensions = (commonBlockExt && commonBlockExtSize == nBlocks + 1);
    bool completeExtendsIntoGappedRegionInThis = false;
    BlockModel  thisCopy(*this);               //  so can use non-const methods
    BlockModel  completeCopy(completeModel);   //  so can use non-const methods

//    string thisRowBlockStr = toString();
//    string completeModelStr = completeModel.toString();

    commonNTermBlockZeroExt = (useInputBlockExtensions) ? (*commonBlockExt)[0] : 0;
    for (i = 0; i < nBlocks; ++i) 
    {
        if (i == nBlocks - 1) {
            seqLen = sequenceLength;
        }
        gapSizeAfterBlockInThis.push_back(getGapToCTerminal(i, seqLen));
        commonCTermBlockExt.push_back((useInputBlockExtensions) ? (*commonBlockExt)[i+1] : 0);
    }

    //  Make sure that there are enough residues for the complete model to 
    //  not introduce a shift into the mapped row.  Require that each complete block
    //  length not extend into more gaps than are common to all rows in the original block model.
    prevCTermBlockExt = 0;
    for (j = 0; j < completeCopy.getBlocks().size() && !completeExtendsIntoGappedRegionInThis; ++j) {
        completeBlockStart = completeCopy.getBlock(j).getStart();
        completeBlockLen   = completeCopy.getBlock(j).getLen();
        blockNumberOnThisOfCompleteBlockStart = thisCopy.getBlockNumber(completeBlockStart);

        //  If the completeBlock starts on an unaligned residue of this, getBlockNumber returns -1.
        //  In that case, find first aligned residue in the complete block and how many residues into
        //  N-terminal gap are needed for the mapping.
        nTermShift = 0;
        while (blockNumberOnThisOfCompleteBlockStart < 0 && nTermShift+1 < completeBlockLen) {
            ++nTermShift;
            blockNumberOnThisOfCompleteBlockStart = thisCopy.getBlockNumber(completeBlockStart + nTermShift);
        }
        //  make sure didn't previously use all residues on C-term extension; if not enough
        //  to do N-terminal extension, set nTermShift to zero.
        if (nTermShift > 0 && blockNumberOnThisOfCompleteBlockStart >= 0) {
            if (blockNumberOnThisOfCompleteBlockStart == 0) {
                if (commonNTermBlockZeroExt - nTermShift < 0) {
                    nTermShift = 0;
                }
            } else {
                if (gapSizeAfterBlockInThis[blockNumberOnThisOfCompleteBlockStart - 1] - prevCTermBlockExt < nTermShift) {
                    nTermShift = 0;
                }
            }
        } else if (blockNumberOnThisOfCompleteBlockStart < 0) {
            nTermShift = 0;
        }

        allowedCTermBlockExt = commonCTermBlockExt[blockNumberOnThisOfCompleteBlockStart];

        thisStart = thisCopy.getBlock(blockNumberOnThisOfCompleteBlockStart).getStart();
        thisLen   = thisCopy.getBlock(blockNumberOnThisOfCompleteBlockStart).getLen();

        /*
        //  There's no gap after this block; find next gap and total number of residues to it;
        //  an adjacent block is only possible if there is no allowed extension in the block.
        if (allowedCTermBlockExt == 0) {
            k = (unsigned) blockNumberOnThisOfCompleteBlockStart; 
            while (k + 1 < gapSizeAfterBlockInThis.size() && gapSizeAfterBlockInThis[k] == 0) {
                ++k;
                thisLen += thisCopy.getBlock(k).getLen();
            }
        }
        */

        //  If we've determined there aren't enough N-terminal residues to pad out this to 
        //  conform to the complete model, we've done an illegal extension into gapped region.
        if (thisStart - completeBlockStart > nTermShift) {
            completeExtendsIntoGappedRegionInThis = true;
        }

        //  There are enough aligned residues from the start position on child to 
        //  grow the block completely w/o extending into disallowed gap regions.
        else if (thisLen + allowedCTermBlockExt - (completeBlockStart - thisStart) >= completeBlockLen) {
            prevCTermBlockExt = completeBlockLen - thisLen + (completeBlockStart - thisStart);
            if (prevCTermBlockExt < 0) prevCTermBlockExt = 0;
//            prevCTermBlockExt = allowedCTermBlockExt;
        } 
        else {

            //  When complete block is large and covers several blocks in this, see if
            //  there is a long enough contiguous length so can safely map all blocks
            //  into the single large one.  Stops being contiguous when any gap in this is
            //  larger than the specified common gap.
            k = (unsigned) blockNumberOnThisOfCompleteBlockStart; 
            thisContiguousLen = thisLen + allowedCTermBlockExt;
            lastCTermBlockExt = allowedCTermBlockExt;
            while (k + 1 < gapSizeAfterBlockInThis.size() && gapSizeAfterBlockInThis[k] == commonCTermBlockExt[k]) {
                ++k;
                lastCTermBlockExt = gapSizeAfterBlockInThis[k];
                thisContiguousLen += thisCopy.getBlock(k).getLen() + lastCTermBlockExt;
            }


            if (thisContiguousLen - (completeBlockStart - thisStart) >= completeBlockLen) {
                //  this is case where have to extend a contiguous range in this to cover complete block;
                //  since it's contiguous, only need an n-terminal extension in next block if used the
                //  final gap in the contiguous length; otherwise, prevCTermBlockExt set to zero.
                //            prevCTermBlockExt = completeBlockLen - thisContiguousLen + (completeBlockStart - thisStart);
                prevCTermBlockExt = thisContiguousLen - completeBlockLen - (completeBlockStart - thisStart);
                if (prevCTermBlockExt > lastCTermBlockExt || prevCTermBlockExt < 0) {
                    prevCTermBlockExt = 0;
                }
            } else {
                completeExtendsIntoGappedRegionInThis = true;
            }
        }

    }

    return completeExtendsIntoGappedRegionInThis;
}

bool BlockModel::contain(const BlockModel& rhs) const
{
	if (!isAlike(rhs))
		return false;
	for ( unsigned int i = 0; i < m_blocks.size(); i++)
	{
		if (!m_blocks[i].contain(rhs.m_blocks[i]))
			return false;
	}
	return true;
}
//delta = bm - this
//bool BlockModel::getCastingDelta(const BlockModel& bm, DeltaBlockModel& delta) const
//return (this-delta), status).  status = true if complete
//complete means every block in this should be accounted for for at least once
pair<DeltaBlockModel*, bool> BlockModel::operator-(const BlockModel& bm) const
{
	DeltaBlockModel* delta = new DeltaBlockModel();
	set<DeltaBlock> uniqueDelta;
	for (unsigned int i = 0; i < bm.m_blocks.size(); i++)
	{
		minusOneBlock(bm.m_blocks[i], *delta);
	}
	for (DeltaBlockModel::iterator dit = delta->begin(); dit != delta->end(); dit++)
	{
		uniqueDelta.insert(*dit);
	}
	delta->clear();
	for (set<DeltaBlock>::iterator sit = uniqueDelta.begin(); sit != uniqueDelta.end(); sit++)
		delta->insert(*sit);
	return pair<DeltaBlockModel*, bool>(delta, delta->size() == m_blocks.size());
}

pair<DeltaBlockModel*, bool> BlockModel::intersect(const BlockModel& bm) const
{
	DeltaBlockModel* delta = new DeltaBlockModel();

	for (unsigned int i = 0; i < bm.m_blocks.size(); i++)
	{
		intersectOneBlock(bm.m_blocks[i], *delta);
	}
	return pair<DeltaBlockModel*, bool>(delta, delta->size() == m_blocks.size());
}


//return(this + delta, complete)
pair<BlockModel*, bool> BlockModel::operator+(const DeltaBlockModel& delta) const
{
	BlockModel* result = new BlockModel();
	DeltaBlockModel::const_iterator dt = delta.begin();
	result->m_seqId = m_seqId;
	int resultBlockID = 0;
	for (; dt != delta.end(); ++dt)
	{
		//const DeltaBlock& db = *dt;
		if (dt->objectBlockID < 0 || dt->objectBlockID >= (int) m_blocks.size())
		{
			delete result;
			return pair<BlockModel*, bool>(nullptr, false);
		}
		const Block& srcBlock = m_blocks[dt->objectBlockID];
		Block block = srcBlock + (*dt);
		if (block.getLen() > 0 && block.getStart() >=0)
			result->m_blocks.push_back(block);
		resultBlockID++;
	}
	//int eb;
	return pair<BlockModel*, bool>(result, (result->getBlocks().size() == delta.size()) );
}

//cast this to target
BlockModel* BlockModel::completeCastTo(const BlockModel& target) const
{
	pair<DeltaBlockModel*, bool> deltaStatus = target - (*this);
//    string dbmStr = DeltaBlockModelToString(*deltaStatus.first);
	if (!(deltaStatus.second)) //not complete
	{
		delete deltaStatus.first;
		return 0;
	}
	pair<BlockModel*, bool> bmStatus = (*this) + (*deltaStatus.first);
//    string bmStr = (*bmStatus.first).toString();
	delete deltaStatus.first;
	if (bmStatus.second)
	{
		if (target.contain(*(bmStatus.first)))
			return bmStatus.first;
	}
	//all other conditions are considered fail	
	delete bmStatus.first;
	return 0;
	
}

//this - aBlock
bool BlockModel::minusOneBlock(const Block& aBlock, DeltaBlockModel& delta) const
{
	//const Block& myBlock = m_blocks[myBlockNum];
	vector<int> blockIds;
	findIntersectingBlocks(aBlock, blockIds);
	if (blockIds.size() <= 0)
		return false;
	for (unsigned int j = 0; j < blockIds.size(); j++)
	{
		delta.insert(m_blocks[blockIds[j]] - aBlock);
	}
	return true;
}

//this intersect aBlock
bool BlockModel::intersectOneBlock(const Block& aBlock, DeltaBlockModel& delta) const
{
	//const Block& myBlock = m_blocks[myBlockNum];
	vector<int> blockIds;
	findIntersectingBlocks(aBlock, blockIds);
	if (blockIds.size() <= 0)
		return false;
	for (unsigned int j = 0; j < blockIds.size(); j++)
	{
		Block intersectedBlock(aBlock);
		if (m_blocks[blockIds[j]].intersect(intersectedBlock))
			delta.insert(intersectedBlock - aBlock);
	}
	return true;
}

void BlockModel::findIntersectingBlocks(const Block& target, vector<int>& result) const
{
	for (unsigned int i = 0; i < m_blocks.size(); i++)
	{
		if(target.isIntersecting(m_blocks[i]))
			result.push_back(i);
	}
}
/*
const BlockModel& BlockModel::operator+=(const BlockModel& delta)
{
	if (!isAlike(delta))
		return *this;
	for ( int i = 0; i < m_blocks.size(); i++)
	{
		m_blocks[i] += delta.m_blocks[i];
	}
	return *this;
}

BlockModel BlockModel::operator-(const BlockModel& rhs)
{
	if(!isAlike(rhs))
		return *this;
	BlockModel delta;
	for ( int i = 0; i < m_blocks.size(); i++)
	{
		delta.m_blocks.push_back(m_blocks[i] - rhs.m_blocks[i]);
	}
	return delta;
}
*/

CRef<CSeq_align> BlockModel::toSeqAlign(const BlockModel& master) const
{
	CRef<CSeq_align> sa;
	int eb;
	if (!master.isValid(-1, eb))
		return sa;
	if (!isValid(-1, eb))
		return sa;
	if (!blockMatch(master))
		return sa;
	
	sa = new CSeq_align();
	sa->Reset();
	sa->SetType(CSeq_align::eType_partial);
	sa->SetDim(2);
	TDendiag& ddList = sa->SetSegs().SetDendiag();
	for (unsigned int i = 0; i < m_blocks.size(); i++)
	{
		CRef< CDense_diag > dd(new CDense_diag());
		dd->SetDim(2);
		vector< CRef< CSeq_id > >& seqIds = dd->SetIds();

        CRef< CSeq_id > masterCopy(new CSeq_id), slaveCopy(new CSeq_id);
        masterCopy->Assign(*(master.getSeqId()));
        slaveCopy->Assign(*getSeqId());
		seqIds.push_back(masterCopy);
		seqIds.push_back(slaveCopy);

        CDense_diag::TStarts& starts = dd->SetStarts();
		starts.push_back(master.m_blocks[i].getStart());
		starts.push_back(m_blocks[i].getStart());
		dd->SetLen(m_blocks[i].getLen());
		
		ddList.push_back(dd);
	}
	
	return sa;
}

int BlockModel::getLastAlignedPosition()const
{
	const Block& lastBlock = *(m_blocks.rbegin());
	return lastBlock.getEnd();
}

int BlockModel::getFirstAlignedPosition()const
{
	const Block& firstBlock = *(m_blocks.begin());
	return firstBlock.getStart();
}

int BlockModel::getTotalBlockLength () const 
{
	int len = 0;
	for (unsigned int i = 0; i < m_blocks.size(); i++)
	{
		len += m_blocks[i].getLen();
	}
	return len;
}

int BlockModel::getGapToNTerminal(int bn) const
{
	int gap = 0;
	if (bn == 0)
		gap = m_blocks[bn].getStart();
	else
	{
		int delta = m_blocks[bn].getStart() - m_blocks[bn - 1].getEnd() - 1;
		if (delta >= 0)
			gap = delta;
	}
	return gap;
}

int BlockModel::getGapToCTerminal(int bn, int len)const 
{
	int gap = 0;
	if (bn == (int) (m_blocks.size() - 1)) //last blast
	{
		if (len > 0)
			gap = (len - 1) - m_blocks[bn].getEnd();
	}
	else
	{
		int delta = m_blocks[bn + 1].getStart() - m_blocks[bn].getEnd() - 1;
		if (delta >= 0)
			gap = delta;
	}
	return gap;
}

void BlockModel::addOffset(int nExt)
{
	for (unsigned int i = 1; i < m_blocks.size(); i++)
	{
		m_blocks[i].addOffset(nExt);
	}
}

bool  BlockModel::isValid(int seqLen, int& errBlock) const
{
	if (m_blocks.size() == 0)
		return false;
	if (seqLen > 1 && getLastAlignedPosition() >= seqLen)
	{
		errBlock = m_blocks.size() - 1;
		return false;
	}
	if (!m_blocks[0].isValid())
	{
		errBlock = 0;
		return false;
	}
	for (unsigned int i = 1; i < m_blocks.size(); i++)
	{
		if (!m_blocks[i].isValid())
		{
			errBlock = (int) i;
			return false;
		}
		if (m_blocks[i-1].getEnd() >= m_blocks[i].getStart())
		{
			errBlock = (int) i - 1;
			return false;
		}
	}
	return true;
}


bool BlockModel::overlap(const BlockModel& bm)const
{
	if (!SeqIdsMatch(m_seqId, bm.m_seqId))
		return false;
	int bmLo = bm.getFirstAlignedPosition();
	int bmHi = bm.getLastAlignedPosition();
	int lo = getFirstAlignedPosition();
	int hi = getLastAlignedPosition();
	if (lo <= bmLo)
		return hi >= bmLo;
	else
		return lo <= bmHi;
}

//return -1 if pos is unaligned
int BlockModel::getBlockNumber(int pos) const
{
	int i = 0;
	for (; i < (int) m_blocks.size(); i++)
	{
		if (pos >= m_blocks[i].getStart() && pos <= m_blocks[i].getEnd())
			break;
	}
	if (i >= (int) m_blocks.size())
		return -1;
	else
		return i;
}

bool BlockModel::mask(const BlockModel& maskBlockModel)
{
    bool result = (SeqIdsMatch(getSeqId(), maskBlockModel.getSeqId()));
    if (result) {
        result = mask(maskBlockModel.getBlocks());
    }
    return result;
}

//  Returns true if this object was modified; false if there was no effect.
bool BlockModel::mask(const vector<Block>& maskBlocks)
{
    vector<Block> maskedBlocks;
    vector<Block>& originalBlocks = getBlocks();
    unsigned int i, nOrigBlocks = originalBlocks.size();
    unsigned int nMaskBlocks = maskBlocks.size();
    int origTotalLength = getTotalBlockLength();
    int newBlockId = 0;
    int pos, start, len;
    int origStart, origEnd;
    int j, maskBlockStart, maskBlockEnd, maskFirst, maskLast;
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

    _ASSERT(getTotalBlockLength() <= origTotalLength);
    hasEffect = (getTotalBlockLength() != origTotalLength);
    if (hasEffect) {
        originalBlocks.clear();
        originalBlocks = maskedBlocks;
    }

    return hasEffect;
}

void BlockModel::clipToRange(unsigned int min, unsigned max)
{
    unsigned int nBlocks = getBlocks().size();
    if (nBlocks == 0) return;

    vector<Block> maskBlocks;
    int firstAligned = getBlocks().front().getStart();
    int lastAligned = getBlocks().back().getEnd();

    //  Add a masking block for all residues < min.
    if (firstAligned < (int) min) {
        maskBlocks.push_back(Block(firstAligned, min - firstAligned));
    }
    //  Add a masking block for all residues > max.
    if (lastAligned > (int) max) {
        maskBlocks.push_back(Block(max + 1, lastAligned - max));
    }

    mask(maskBlocks);
}


string BlockModel::toString() const
{
    string blockModelStr, tmp;
    unsigned int nBlocks = m_blocks.size();

    if (m_seqId.NotEmpty()) {
        blockModelStr = "Sequence:  " + GetSeqIDStr(m_seqId) + "\n";
    }

    for (unsigned int i = 0; i < nBlocks; ++i) {
        tmp  = "  Block Id = " + NStr::IntToString(m_blocks[i].getId());
        tmp += "; Block Range = [" + NStr::IntToString(m_blocks[i].getStart());
        tmp += ", " + NStr::IntToString(m_blocks[i].getEnd());
        tmp += "] (Length = " + NStr::IntToString(m_blocks[i].getLen()) + ")\n";
        blockModelStr += tmp;
    }
    return blockModelStr;
}

string DeltaBlockModelToString(const DeltaBlockModel& dbm)
{
    string deltaBlockModelStr, tmp;
    DeltaBlockModel::const_iterator dbm_cit = dbm.begin(), dbm_citEnd = dbm.end();

    for (; dbm_cit != dbm_citEnd; ++dbm_cit) {
        tmp  = "  Delta Block Subject Id = " + NStr::IntToString(dbm_cit->subjectBlockID);
        tmp += "; Delta Block Object Id = " + NStr::IntToString(dbm_cit->objectBlockID);
        tmp += "; Delta Block Start = " + NStr::IntToString(dbm_cit->deltaStart);
        tmp += "; Delta Block Len = " + NStr::IntToString(dbm_cit->deltaLen);
        tmp += "\n";
        deltaBlockModelStr += tmp;
    }
    return deltaBlockModelStr;
}

string BlockModel::toString(const BlockModel& bm) {
    return bm.toString();
}

//implement BlockModel Pair

BlockModelPair::BlockModelPair():
m_master(0), m_slave(0)
{
	m_master = new BlockModel();
	m_slave = new BlockModel();
}

BlockModelPair::BlockModelPair(const CRef< CSeq_align> seqAlign)
{
	m_master = new BlockModel(seqAlign, false);
	m_slave = new BlockModel(seqAlign, true);
}

//deep copy
BlockModelPair::BlockModelPair(const BlockModelPair& rhs)
{
    if (rhs.m_master) {
        m_master = new BlockModel(*rhs.m_master);
    }
    if (rhs.m_slave) {
        m_slave = new BlockModel(*rhs.m_slave);
    }
}

BlockModelPair& BlockModelPair::operator=(const BlockModelPair& rhs)
{
    //  The copy can fail if rhs has null pointers.
    delete m_master;
    delete m_slave;
    m_master = NULL;
    m_slave = NULL;
    if (rhs.m_master) {
        m_master = new BlockModel(*rhs.m_master);
    }
    if (rhs.m_slave) {
        m_slave = new BlockModel(*rhs.m_slave);
    }
	return *this;
}

BlockModelPair::~BlockModelPair()
{
	delete m_master;
	delete m_slave;
}

void BlockModelPair::reset()
{
    delete m_master;
    delete m_slave;
    m_master = new BlockModel();
    m_slave = new BlockModel();
}

BlockModel& BlockModelPair::getMaster()
{
	return *m_master;
}

const BlockModel& BlockModelPair::getMaster()const
{
	return *m_master;
}

BlockModel& BlockModelPair::getSlave()
{
	return *m_slave;
}

const BlockModel& BlockModelPair::getSlave()const
{
	return *m_slave;
}

void BlockModelPair::degap()
{
	assert(m_master->blockMatch(*m_slave));
	int num = m_master->getBlocks().size();
	for (int i = 0; i < num; i++)
	{
			extendMidway(i);
	}
}

void BlockModelPair::extendMidway(int blockNum)
{
	int ngap = m_master->getGapToNTerminal(blockNum); 
	if (blockNum == 0) //do N-extend the first block
		ngap = 0;
	int ngapSlave = m_slave->getGapToNTerminal(blockNum);
	if (ngap > ngapSlave)
		ngap = ngapSlave;
	int cgap = m_master->getGapToCTerminal(blockNum);
	int cgapSlave = m_slave->getGapToCTerminal(blockNum);
	if (cgap > cgapSlave)
		cgap = cgapSlave;

	//c-ext of last block has taken half of the gap, so take what's left here
	// negative for moving to N-end.
	int nExt = -ngap; 
	int cExt = 0;
	if (blockNum != (int) (m_master->getBlocks().size()-1) ) //do not C-extend the last block
	{
		if ((cgap % 2) == 0)
			cExt = cgap/2;
		else
			cExt = cgap/2 + 1;
	}
	m_master->getBlock(blockNum).extendSelf(nExt, cExt);
	m_slave->getBlock(blockNum).extendSelf(nExt, cExt);
}

CRef<CSeq_align> BlockModelPair::toSeqAlign() const
{
	return m_slave->toSeqAlign(*m_master);
}


int BlockModelPair::mapToMaster(int slavePos) const
{
	int bn = m_slave->getBlockNumber(slavePos);
	if (bn < 0)
		return -1;
	return m_master->getBlock(bn).getStart() + (slavePos - m_slave->getBlock(bn).getStart());
}

int BlockModelPair::mapToSlave(int masterPos) const
{
	int bn = m_master->getBlockNumber(masterPos);
	if (bn < 0)
		return -1;
	return m_slave->getBlock(bn).getStart() + (masterPos - m_master->getBlock(bn).getStart());
}

bool BlockModelPair::isValid()const
{
	return m_master->blockMatch(*m_slave);
}

//assume this.master is the same as guide.master
//change this.master to guide.slave
//
int BlockModelPair::remaster(const BlockModelPair& guide)
{
	if (!SeqIdsMatch(getMaster().getSeqId(),guide.getMaster().getSeqId()))
		return 0;

	//convert guide.slave to the intersection of this.master and guide.master
	pair<DeltaBlockModel*, bool> deltaGuideToThis = m_master->intersect(guide.getMaster());
	pair<BlockModel*, bool> intersectedGuideSlave = guide.getSlave() + *(deltaGuideToThis.first);
    
	//convert this.slave to the intersection of this.master and guide.master
	pair<DeltaBlockModel*, bool> deltaThisToGuide = guide.getMaster().intersect(*m_master);
	pair<BlockModel*, bool> intersectedThisSlave = (*m_slave) + *(deltaThisToGuide.first);
    
    if (intersectedGuideSlave.first && intersectedThisSlave.first) {
        
        assert((intersectedGuideSlave.first)->blockMatch(*(intersectedThisSlave.first)));

        delete m_master;
        delete m_slave;

        m_master = intersectedGuideSlave.first;
        m_slave = intersectedThisSlave.first;

        return m_master->getTotalBlockLength();

    } else {
        return 0;
    }
}

bool BlockModelPair::mask(const vector<Block>& maskBlocks, bool maskBasedOnMaster)
{
    if (!m_master || !m_slave || !isValid()) return false;

    bool hasEffect = false;
    BlockModel& maskedBm = (maskBasedOnMaster) ? getMaster() : getSlave();
    vector<Block>& newBlocks = maskedBm.getBlocks();
    unsigned int i, nBlocks = newBlocks.size();
    int pos, mappedPos;

    //  First, mask the primary BlockModel in the pair.
    if (maskBlocks.size() > 0 && nBlocks > 0 && maskedBm.mask(maskBlocks)) {

        hasEffect = true;

        //  Then, fix up the other BlockModel in the pair to match the new masked primary BlockModel.
        vector<Block>& newOtherBlocks = (maskBasedOnMaster) ? getSlave().getBlocks() : getMaster().getBlocks(); 
        newOtherBlocks.clear();
        for (i = 0; i < nBlocks; ++i) {
            Block& b = newBlocks[i];
            pos = b.getStart();
            mappedPos = (maskBasedOnMaster) ? mapToSlave(pos) : mapToMaster(pos);
            if (mappedPos >= 0) {  // should never fail if bmp.isValid is true.
                newOtherBlocks.push_back(Block(mappedPos, b.getLen(), b.getId()));
            }
        }
    }
    return hasEffect;
}

//reverse the master vs slave
void BlockModelPair::reverse()
{
	BlockModel* bm = m_master;
	m_master = m_slave;
	m_slave = bm;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
