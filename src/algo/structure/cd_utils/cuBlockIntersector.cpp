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
 * Author:  Charlie
 *
 * File Description:
 *
 *       to find the intersection of the group of alignments on the same sequence
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuBlockIntersector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)


BlockIntersector::BlockIntersector(int seqLen) : m_seqLen(seqLen), m_totalRows(0), 
	m_firstBm(0), m_aligned(0)
{
	m_aligned = new unsigned[seqLen];
	for (int i = 0; i < seqLen; i++)
		m_aligned[i] = 0;
}

BlockIntersector::~BlockIntersector()
{
    delete m_firstBm;
	if (m_aligned)
		delete []m_aligned;
}

void BlockIntersector::addOneAlignment(const BlockModel& bm)
{
	if (!m_firstBm)
	{
//		m_firstBm = &bm;
        m_firstBm = new BlockModel(bm);  //  keep a copy; BlockExtender now passes a local variable

	}
	m_totalRows++;
	const std::vector<Block>& blocks = bm.getBlocks();
	for(unsigned int b = 0; b < blocks.size(); b++)
		for(int pos = blocks[b].getStart(); pos <= blocks[b].getEnd(); pos++)
			m_aligned[pos]++;
}

void BlockIntersector::removeOneAlignment(const BlockModel& bm)
{
	m_totalRows--;
	const std::vector<Block>& blocks = bm.getBlocks();
	for(unsigned int b = 0; b < blocks.size(); b++)
		for(int pos = blocks[b].getStart(); pos <= blocks[b].getEnd(); pos++)
			if (m_aligned[pos] > 0)
				m_aligned[pos]--;
}


BlockModel* BlockIntersector::getIntersectedAlignment(double rowFraction)
{
	BlockModel* result = new BlockModel(*m_firstBm);
	if (m_totalRows <= 1)
		return result;
	std::vector<Block>& blocks = result->getBlocks();
	blocks.clear();
	bool inBlock = false;
	int start = 0;
	int blockId = 0;
	int i = 0;

    if (rowFraction < 0.0 || rowFraction > 1.0) rowFraction = 1.0;
    double adjustedTotalRows = m_totalRows * rowFraction;

	for (; i < m_seqLen; i++)
	{
		if (!inBlock)
		{
			if (m_aligned[i] >= adjustedTotalRows)
			{
				start = i;
				inBlock = true;
			}
		}
		else
		{
			if (m_aligned[i] < adjustedTotalRows)
			{
				inBlock = false;
				blocks.push_back(Block(start, i - start, blockId));
				blockId++;
			}
		}
	}
	if (inBlock) //block goes to the end of the sequence
		blocks.push_back(Block(start, i - start, blockId));
	return result;
}

BlockModel* BlockIntersector::getIntersectedAlignment(const std::set<int>& forcedBreak, double rowFraction)
{
	BlockModel* result = new BlockModel(*m_firstBm);
	if (m_totalRows <= 1)
		return result;
	std::vector<Block>& blocks = result->getBlocks();
    std::set<int>::const_iterator setEnd = forcedBreak.end();
	blocks.clear();
	bool inBlock = false;
    bool forceNewBlock = false;
	int start = 0;
	int blockId = 0;
	int i = 0;

    if (rowFraction <= 0.0 || rowFraction > 1.0) rowFraction = 1.0;
    double adjustedTotalRows = m_totalRows * rowFraction;

	for (; i < m_seqLen; i++)
	{

        //  previous position was not in a block so forcedBreak is irrelevant
		if (!inBlock)
		{
			if (m_aligned[i] >= adjustedTotalRows)
			{
				start = i;
				inBlock = true;
			}
		}
		else
		{
            //  was the previous position a forced C-terminus?
            forceNewBlock = (i > 0 && forcedBreak.find(i - 1) != setEnd);

            //  it's a C-termini w/o any forcing
            if (m_aligned[i] < adjustedTotalRows)
			{
				inBlock = false;
				blocks.push_back(Block(start, i - start, blockId));
				blockId++;
			} 
            //  need to break this block and immediately start a new one
            else if (forceNewBlock)
            {
				blocks.push_back(Block(start, i - start, blockId));
				blockId++;
                start = i;
            }
		}
	}
	if (inBlock) //block goes to the end of the sequence
		blocks.push_back(Block(start, i - start, blockId));
	return result;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
