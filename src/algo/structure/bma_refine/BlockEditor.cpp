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
* Authors:  Chris Lanczycki
*
* File Description:
*      
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>

#include <algorithm>

#include <algo/structure/struct_util/struct_util.hpp>
#include <algo/structure/bma_refine/BlockEditor.hpp>

USING_NCBI_SCOPE;
BEGIN_SCOPE(align_refine)

CBlockedAlignmentEditor::CBlockedAlignmentEditor(AlignmentUtility* au) : m_bma(NULL) {
    if (au) {
        const BMA* bma = au->GetBlockMultipleAlignment();
        Init(bma);
    }
}

CBlockedAlignmentEditor::CBlockedAlignmentEditor(const BMA* bma) : m_bma(NULL) {
    Init(bma);
}

void CBlockedAlignmentEditor::Init(const BMA* bma) {
    if (m_bma) delete m_bma;
    m_bma = (bma) ? bma->Clone() : NULL;
    SetExtensionBounds();
}

unsigned int CBlockedAlignmentEditor::GetWidth() const {
    return ((m_bma) ? m_bma->AlignmentWidth() : 0);
}

void CBlockedAlignmentEditor::SetExtensionBounds() {

    unsigned i, j;
    unsigned int startPos = 0;  //  block start position in alignment index coordinate
    unsigned int nAligned = 0;
    BMA::ConstBlockList::const_iterator blit;
    const Block* prevBlock = NULL;

    m_extBound.clear();
    if (m_bma) {
        BMA::ConstBlockList blockList;
		m_bma->GetBlockList(blockList);
        nAligned = m_bma->NAlignedBlocks();
        m_extBound.resize(nAligned);
        blit = blockList.begin();

        //  Allocate the unaligned blocks' residues as possible extensions to the
        //  aligned blocks.
        for (unsigned int blockNum=0; blit != blockList.end(); ++blit) {
            if ((*blit)->IsAligned()) {
                m_extBound[blockNum].block = const_cast<Block*> (blit->GetPointer());
                if (prevBlock) {
                    //  if the previous block is unaligned, extend bound by half the width,
                    //  giving the extra position for an odd width to the block that's 
                    //  C-terminal to the unaligned block.  If at the first aligned block, can 
                    //  extend for the full block length.
                    m_extBound[blockNum].from = startPos;
                    m_extBound[blockNum].nBound = m_extBound[blockNum].from;
                    if (!prevBlock->IsAligned()) {
                        m_extBound[blockNum].nBound -= (blockNum==0) ? prevBlock->m_width : (prevBlock->m_width+1)/2;
                    }

                    //  initialize the cBound for this block to be the last position in the block;
                    //  if this is the last block or an aligned block is next, no need to extend 
                    //  further; otherwise add on extra residues if there's an unaligned block 
                    //  immediately following.  
                    m_extBound[blockNum].to = startPos + (*blit)->m_width - 1;
                    m_extBound[blockNum].cBound = m_extBound[blockNum].to;
                } else {
                    m_extBound[blockNum].from = 0;
                    m_extBound[blockNum].nBound = 0;
                    m_extBound[blockNum].to = (*blit)->m_width - 1;
                    m_extBound[blockNum].cBound = m_extBound[blockNum].to;
                }
                ++blockNum;
            } else if (prevBlock && prevBlock->IsAligned()) {
                m_extBound[blockNum - 1].cBound += (blockNum == nAligned) ? (*blit)->m_width : (*blit)->m_width/2;
            }
            startPos += (*blit)->m_width;
            prevBlock = blit->GetPointer();
        }

        //  Adjust the bounds to allow only extensions when each sequence has a valid residue.
        for (i = 0; i < nAligned; ++i) {
            BlockExtensionBound& beb = m_extBound[i];
            j = beb.from - 1;
            while (j >= beb.nBound && IsResidueAtIndexOnAllRows(j)) {
                --j;
            }
            beb.nBound = j + 1;

            j = beb.to + 1;
            while (j <= beb.cBound && IsResidueAtIndexOnAllRows(j)) {
                ++j;
            }
            beb.cBound = j - 1;
        }
    }
}

bool CBlockedAlignmentEditor::GetCharacterForColumn(unsigned alignmentIndex, unsigned row, char* residue) const {
    bool result = false;
    if (!m_bma || !residue) return result;

    BMA::eUnalignedJustification just;
    BMA::ConstBlockList blockList;
	m_bma->GetBlockList(blockList);
    BMA::ConstBlockList::const_iterator blit = blockList.begin();

    unsigned bNum = 0, pos = 0;
    unsigned nBlocks = blockList.size();

    for (;blit != blockList.end(); ++blit) {
        if (alignmentIndex < pos + (*blit)->m_width) {
            just = (bNum==0) ? BMA::eRight : ((bNum == nBlocks-1) ? BMA::eLeft : BMA::eSplit);
            result = m_bma->GetCharacterAt(alignmentIndex, row, just, residue);
            break;
        }
        ++bNum;
        pos += (*blit)->m_width;  //  first position on next block
    }
    return result;
}

unsigned CBlockedAlignmentEditor::GetExtendableBlocks(vector<ExtendableBlock>& extBlocks, ExtensionLoc eloc) const {
    unsigned nAligned = m_extBound.size();
    ExtendableBlock eBlock;

    extBlocks.clear();
    for (unsigned i = 0; i < nAligned; ++i) {
        if (IsBlockExtendable(i, eloc) && m_extBound[i].block.NotEmpty()) {
            eBlock.blockNum = i;
            eBlock.from = m_extBound[i].from;
            eBlock.to   = m_extBound[i].to;
//            eBlock.seqFrom = m_extBound[i].block->GetRangeOfRow(0)->from;
//            eBlock.seqTo   = m_extBound[i].block->GetRangeOfRow(0)->to;
            eBlock.nExt = 0;
            eBlock.cExt = 0;
            if (eloc == eNTerm || eloc == eBoth || eloc == eEither || eloc == eAny) {
                eBlock.nExt = m_extBound[i].from - m_extBound[i].nBound;
            }
            if (eloc == eCTerm || eloc == eBoth || eloc == eEither || eloc == eAny) {
                eBlock.cExt = m_extBound[i].cBound - m_extBound[i].to;
            }
            extBlocks.push_back(eBlock);
        }
    }
    return extBlocks.size();
}

bool CBlockedAlignmentEditor::IsBlockExtendable(unsigned blockNum, ExtensionLoc eloc) const {
    bool result = false;
    if (blockNum < m_extBound.size()) {
        switch (eloc) {
        case eNTerm:
            result = (m_extBound[blockNum].from > m_extBound[blockNum].nBound);
            break;
        case eCTerm:
            result = (m_extBound[blockNum].cBound > m_extBound[blockNum].to);
            break;
        case eBoth:
            result = ((m_extBound[blockNum].from > m_extBound[blockNum].nBound) &&
                      (m_extBound[blockNum].cBound > m_extBound[blockNum].to));
            break;
        case eEither:
            result = ((m_extBound[blockNum].from > m_extBound[blockNum].nBound) ||
                      (m_extBound[blockNum].cBound > m_extBound[blockNum].to));
            break;
        case eNeither:
            result = ((m_extBound[blockNum].from <= m_extBound[blockNum].nBound) &&
                      (m_extBound[blockNum].cBound <= m_extBound[blockNum].to));
            break;
        case eAny:
            result = true;
        default:
            break;
        }
    }
    return result;
}

unsigned int CBlockedAlignmentEditor::MoveBlockBoundaries(BlockBoundaryAlgorithm& algorithm, 
                                                          ExtensionLoc eLoc, const set<unsigned int>* editableBlocks, 
                                                          vector<ExtendableBlock>* changedBlocks) {
    bool moved;
    int nShift, cShift;
    unsigned int nChanged = 0, nEditable = 0, nMoved = 0, nDeleted = 0;
    ExtendableBlock thisBlock;
    vector<ExtendableBlock> extendableBlocks;
    set<unsigned int>::const_iterator itEnd;
    map<unsigned int, ExtendableBlock> localChangedBlocks;
    map<unsigned int, ExtendableBlock>::iterator mapIt;

    nEditable = GetExtendableBlocks(extendableBlocks, eLoc);
    if (editableBlocks) itEnd = editableBlocks->end();

    //  Find location of the new block boundaries...
    for (unsigned int i = 0; i < nEditable; ++i) {
        thisBlock = extendableBlocks[i];

        //  Skip any block not declared editable.
        if (editableBlocks && editableBlocks->find(thisBlock.blockNum) == itEnd) continue;

        //  No block in 'localChangedBlocks' can have nShift = cShift = 0.
        if (algorithm.GetNewBoundaries(thisBlock, *m_bma)) {
            ++nChanged;
            localChangedBlocks[i] = thisBlock;
            if (changedBlocks) changedBlocks->push_back(thisBlock);
        }
    }

    //  ... and modify the block structure accordingly.
    for (mapIt = localChangedBlocks.begin(); mapIt != localChangedBlocks.end(); ++mapIt) {
        thisBlock = mapIt->second;
        if (thisBlock.from > thisBlock.to) {  // delete the block!
            LOG_MESSAGE_CL("        MoveBlockBoundaries:  Deleting block " << thisBlock.blockNum+1);
            if (!m_bma->DeleteBlock(thisBlock.to)) {
                WARNING_MESSAGE_CL("        MoveBlockBoundaries:  Problem trying to delete block " << thisBlock.blockNum+1);
            } else {
                ++nDeleted;
            }
        } else {
            moved  = true;
            nShift = extendableBlocks[mapIt->first].from - thisBlock.from;
            cShift = thisBlock.to - extendableBlocks[mapIt->first].to;
            if (nShift != 0) {
                moved = MoveBlockBoundary(thisBlock.blockNum, eNTerm, nShift);
            }
            if (cShift != 0) {
                moved &= MoveBlockBoundary(thisBlock.blockNum, eCTerm, cShift);
            }

            if (!moved) {
                WARNING_MESSAGE_CL("        MoveBlockBoundaries:  Problem trying to move block " << thisBlock.blockNum+1
                                   << "\n            while attempting N/C terminal shifts of " << nShift << "/"
                                   << cShift);
            } else {
                ++nMoved;
                if (nShift > 0) {
                    TRACE_MESSAGE_CL("        MoveBlockBoundaries:  Extended N-terminal of block " << 
                                    thisBlock.blockNum+1 << " by " << nShift << " residues.");
                } else if (nShift < 0) {
                    TRACE_MESSAGE_CL("        MoveBlockBoundaries:  Shrunk N-terminal of block " << 
                                    thisBlock.blockNum+1 << " by " << -nShift << " residues.");
                }
                if (cShift > 0) {
                    TRACE_MESSAGE_CL("        MoveBlockBoundaries:  Extended C-terminal of block " << 
                                    thisBlock.blockNum+1 << " by " << cShift << " residues.");
                } else if (cShift < 0) {
                    TRACE_MESSAGE_CL("        MoveBlockBoundaries:  Shrunk C-terminal of block " << 
                                    thisBlock.blockNum+1 << " by " << -cShift << " residues.");
                }
            }
        }
    }

    TERSE_INFO_MESSAGE_CL("        Attempted to change bounds of " << nChanged << " blocks; moved bounds for " << nMoved << " of them");
    if (nDeleted > 0) {
        TERSE_INFO_MESSAGE_CL("                      and deleted " << nDeleted << " blocks.");
    }
    return nChanged;
}

bool CBlockedAlignmentEditor::MoveBlockBoundary(unsigned blockNum, ExtensionLoc eloc, int shift) {

    int maxShift;
    bool result = false;

    if (!m_bma || blockNum >= m_extBound.size() || shift == 0 ||
        eloc == eEither || eloc == eNeither || eloc == eAny) {
        WARNING_MESSAGE_CL("unexpected parameters:  overriding move in MoveBlockBoundary for shift " << shift << " at block " << blockNum + 1);
        return result;
    }

    if (eloc == eNTerm || eloc == eBoth) {

        //  Extend N-terminal end
        if (shift > 0 && m_extBound[blockNum].from - m_extBound[blockNum].nBound > 0) {
            maxShift = m_extBound[blockNum].from - m_extBound[blockNum].nBound;
            if (shift > maxShift) shift = maxShift;
            if (m_bma->MoveBlockBoundary(m_extBound[blockNum].from, m_extBound[blockNum].from - shift)) {
                m_extBound[blockNum].from -= shift;
                result = true;
            }
        //  Shrink N-terminal end; don't allow shift past the C-terminus
        } else if (shift < 0 && (unsigned int) (-shift) < m_extBound[blockNum].to - m_extBound[blockNum].from + 1) {
            TRACE_MESSAGE_CL("Try to shrink n-term by " << -shift << " on block " << blockNum+1);
            if (m_bma->MoveBlockBoundary(m_extBound[blockNum].from, m_extBound[blockNum].from - shift)) {
                m_extBound[blockNum].from -= shift;
                result = true;
                TRACE_MESSAGE_CL("        SUCCEEDED");
            }
        }
    } 

    //  Try C-terminal shift next
    if (eloc == eCTerm || eloc == eBoth) {

       //  Extend C-terminal end
        if (shift > 0 && m_extBound[blockNum].cBound - m_extBound[blockNum].to > 0) {
            maxShift = m_extBound[blockNum].cBound - m_extBound[blockNum].to;
            if (shift > maxShift) shift = maxShift;
            if (m_bma->MoveBlockBoundary(m_extBound[blockNum].to, m_extBound[blockNum].to + shift)) {
                m_extBound[blockNum].to += shift;
                if (eloc == eCTerm) {
                    result = true;
                } else {
                    result &= true;  // if eBoth, true only if true for N & C termini
                }
            }
        //  Shrink C-terminal end; don't allow shift past the N-terminus
        } else if (shift < 0 && (unsigned int) (-shift) < m_extBound[blockNum].to - m_extBound[blockNum].from + 1) {
            TRACE_MESSAGE_CL("Try to shrink c-term by " << -shift << " on block " << blockNum+1);
            if (m_bma->MoveBlockBoundary(m_extBound[blockNum].to, m_extBound[blockNum].to + shift)) {
                m_extBound[blockNum].from += shift;
                if (eloc == eCTerm) {
                    result = true;
                } else {
                    result &= true;  // if eBoth, true only if true for N & C termini
                }
                TRACE_MESSAGE_CL("        SUCCEEDED");
            }
        }
    }
    return result;
}

unsigned CBlockedAlignmentEditor::GetSeqIndexForColumn(unsigned blockIndex, unsigned row, const Block* block, unsigned blockNum) const {
    
    if (!m_bma || !block) return BMA::eUndefined;

    unsigned nBlocks = m_bma->NBlocks();  // if can guarantee m_bma not changing can make static

    BMA::eUnalignedJustification just;
    just = (blockNum==0) ? BMA::eRight : ((blockNum == nBlocks-1) ? BMA::eLeft : BMA::eSplit);
    return block->GetIndexAt(blockIndex, row, just);
}

unsigned CBlockedAlignmentEditor::GetSeqIndexForColumn(unsigned alignmentIndex, unsigned row) const {
    unsigned result = BMA::eUndefined;
    if (!m_bma) return result;

    unsigned blockNum = 0, pos = 0;
//    unsigned nBlocks = m_bma->NBlocks(), nRows = m_bma->NRows();

    BMA::ConstBlockList blockList;
	m_bma->GetBlockList(blockList);
    BMA::ConstBlockList::const_iterator blit = blockList.begin();

    for (;blit != blockList.end(); ++blit) {
        if (alignmentIndex < pos + (*blit)->m_width) {
            result = GetSeqIndexForColumn(alignmentIndex - pos, row, (*blit).GetPointer(), blockNum);
            break;
        }
        ++blockNum;
        pos += (*blit)->m_width;  //  first position on next block
    }
    return result;
}

bool CBlockedAlignmentEditor::IsResidueAtIndexOnAllRows(unsigned alignmentIndex) const {
    bool result = false, found = false;
    if (!m_bma) return result;

    unsigned i;
    unsigned blockNum = 0, pos = 0;
    unsigned nRows = m_bma->NRows();
//    unsigned nBlocks = m_bma->NBlocks(), nRows = m_bma->NRows();

    BMA::ConstBlockList blockList;
	m_bma->GetBlockList(blockList);
    BMA::ConstBlockList::const_iterator blit = blockList.begin();

    for (;blit != blockList.end() && !found; ++blit) {
        if (alignmentIndex < pos + (*blit)->m_width) {
            found = true;
            result = true;
            for (i = 0; i < nRows; ++i) {
                if (GetSeqIndexForColumn(alignmentIndex - pos, i, (*blit).GetPointer(), blockNum) == BMA::eUndefined) {
                    result = false;
                    break;
                }
            }
        }
        ++blockNum;
        pos += (*blit)->m_width;  //  first position on next block
    }
    return result;
}


string CBlockedAlignmentEditor::BoundsToString(unsigned int pad) const {

    CNcbiOstrstream strStrm;
    IOS_BASE::fmtflags initFlags = strStrm.flags();

    string padding(pad, ' ');
    unsigned int i, nAligned = m_extBound.size();

    strStrm << padding << "**********************************************\n";
    for (i = 0; i < nAligned; ++i) {
        strStrm.setf(IOS_BASE::left, IOS_BASE::adjustfield);
        strStrm << padding << "BLOCK " << setw(4) << i << ":  [from, to] = [";

        strStrm.setf(IOS_BASE::right, IOS_BASE::adjustfield);
        strStrm << setw(4) << m_extBound[i].from << ", " << setw(4) << m_extBound[i].to << "]; ";
        strStrm << " [nBound, cBound] = [" << setw(4) << m_extBound[i].nBound << ", " << setw(4) << m_extBound[i].cBound << "]\n";
    }
    strStrm << padding << "**********************************************\n";
    strStrm.setf(initFlags, IOS_BASE::adjustfield);

    return strStrm.str();
}

END_SCOPE(align_refine)
