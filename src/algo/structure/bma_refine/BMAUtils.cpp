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
*          Some utility functions on BlockMultipleAlignment objects.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <map>

#include <algo/structure/struct_util/su_sequence_set.hpp>
#include <algo/structure/struct_util/su_pssm.hpp>
#include <algo/structure/bma_refine/BMAUtils.hpp>

USING_NCBI_SCOPE;

BEGIN_SCOPE(align_refine)


//   Needs to work for both aligned columns and non-aligned columns in the PSSM.
bool BMAUtils::GetCharacterAndIndexForColumn(const BMA& bma, unsigned alignmentIndex, unsigned row, char* residue, unsigned int* seqIndex) {
    bool isAligned, result = false;
    unsigned int dummySeqIndex;
    const Sequence *sequence;

    if (!residue) return result;

    BMA::eUnalignedJustification just;
    BMA::ConstBlockList blockList;
	bma.GetBlockList(blockList);
    BMA::ConstBlockList::const_iterator blit = blockList.begin();

    unsigned bNum = 0, pos = 0;
    unsigned nBlocks = blockList.size();

    for (;blit != blockList.end(); ++blit) {
        if (alignmentIndex < pos + (*blit)->m_width) {
            just = (bNum==0) ? BMA::eRight : ((bNum == nBlocks-1) ? BMA::eLeft : BMA::eSplit);
            bma.GetSequenceAndIndexAt(alignmentIndex, row, just, &sequence, &dummySeqIndex, &isAligned);

            //  Note:  GetSequenceAndIndexAt does not guarantee eUndefined being returned
            //         when there is not a residue at a given position in an unaligned block
            result = (dummySeqIndex != BMA::eUndefined && 
                      (sequence && dummySeqIndex < sequence->m_sequenceString.size()));
            *residue = (result) ? sequence->m_sequenceString[dummySeqIndex] : '-';
//            cout << "return from bma.GetSequenceAndIndexAt:  " << alignmentIndex << "  " << row << "  aligned = "  << isAligned << "  residue = " << *residue << endl;

            if (seqIndex) {
                *seqIndex = (result) ? dummySeqIndex : BMA::eUndefined;
            }
//            result = bma.GetCharacterAt(alignmentIndex, row, just, residue);
            break;
        }
        ++bNum;
        pos += (*blit)->m_width;  //  first position on next block
    }
    return result;
}

//   Needs to work for both aligned columns and non-aligned columns in the PSSM.
bool BMAUtils::GetCharacterForColumn(const BMA& bma, unsigned alignmentIndex, unsigned row, char* residue) {
    unsigned int seqIndex;
    return GetCharacterAndIndexForColumn(bma, alignmentIndex, row, residue, &seqIndex);
}

int BMAUtils::GetSmallestValueInPssm(const BMA& bma) {
    unsigned int i, j, nRows, nCols;
    int scoreShift = kMin_Int;
    int** pssmMatrix = (bma.GetPSSM()) ? bma.GetPSSM()->matrix : NULL;
    if (pssmMatrix) {
        nRows = bma.GetPSSM()->rows;
        nCols = bma.GetPSSM()->columns;
        for (i = 0; i < nRows; ++i) {
            for (j = 0; j < nCols; ++j) {
                if (pssmMatrix[i][j] > -32768 && scoreShift > pssmMatrix[i][j]) {
                    scoreShift = pssmMatrix[i][j];
                }
            }
        }
    }
    return scoreShift;
}

//   For the specified column, return the PSSM score for each row in the alignment.
//   Where can't get a row's character in 'alignmentIndex' column, return score for '-' 
//   in its place.  Also return the residues in the specified column if requested.
//   Needs to work for both aligned columns and non-aligned columns in the PSSM.
void BMAUtils::GetPSSMScoresForColumn(const BMA& bma, unsigned alignmentIndex, vector< int >& scores, vector< char >* residues) {

    char residue;
    unsigned int i, nRows, masterIndex;
    int score;

    scores.clear();
    if (!bma.GetPSSM()) {
        ERROR_MESSAGE_CL("Invalid PSSM for BlockMultipleAlignment object");
        return;
    }

    // Get residue on the master, and its sequence index; if there's a problem stop.
    if (!GetCharacterAndIndexForColumn(bma, alignmentIndex, 0, &residue, &masterIndex)) return;

    nRows = bma.NRows();
    for (i = 0; i < nRows; ++i) {

        if (i > 0 && !GetCharacterForColumn(bma, alignmentIndex, i, &residue)) {
            residue = '-';
        }

        score = GetPSSMScoreOfCharWithAverageOfBZ(bma.GetPSSM(), masterIndex, residue);
        TRACE_MESSAGE_CL("GetPSSMScoreForColumn " << masterIndex+1 << ":  (row, column, residue, score) = (" << i+1 << ", "  << alignmentIndex+1 << ", " << residue << ", " << score << ")\n");

        scores.push_back(score);
        if (residues) residues->push_back(residue);
    }

}

//   Where can't get a row's character in 'alignmentIndex' column, return '-' 
//   in its place.  Needs to work for both aligned columns and non-aligned 
//   columns in the PSSM.
void BMAUtils::GetResiduesForColumn(const BMA& bma, unsigned alignmentIndex, vector< char >& residues) {

    char residue;
    unsigned int i, nRows, masterIndex;


    residues.clear();
    if (!bma.GetPSSM()) {
        ERROR_MESSAGE_CL("Invalid PSSM for BlockMultipleAlignment object");
        return;
    }

    // Get residue on the master, and its sequence index; if there's a problem stop.
    if (!GetCharacterAndIndexForColumn(bma, alignmentIndex, 0, &residue, &masterIndex)) return;

    nRows = bma.NRows();
    for (i = 0; i < nRows; ++i) {

        if (!GetCharacterForColumn(bma, alignmentIndex, i, &residue)) {
            residue = '-';
        }
        residues.push_back(residue);
        TRACE_MESSAGE_CL("GetResiduesForColumn " << alignmentIndex+1 << ":  (row, residue) = (" << i+1 << ", "  << residue << ")\n");
    }

}

void BMAUtils::MapAlignmentIndexToSeqIndex(const BMA& bma, unsigned int row, map<unsigned int, unsigned int>& aI2sI) {
    char residue;
    unsigned int seqIndex = 0, alignmentIndex = 0;
    BMA::ConstBlockList blocks;
	bma.GetBlockList(blocks);
    BMA::ConstBlockList::const_iterator b = blocks.begin(), be = blocks.end();

    aI2sI.clear();
    if (row >= bma.NRows()) {
        ERROR_MESSAGE_CL("Invalid row number " << row << " specified.  Returning");
        return;
    }

    for (b=blocks.begin(); b!=be; ++b) {

/*        //  skip blocks which have wrong type
        if (!IsBlockConsistentWithType(*b, ctype)) {
            column += (*b)->m_width;
            ++blockNum;
            if (isAlignedBlock) ++alignedBlockNum;
            TRACE_MESSAGE_CL("Block inconsistent with type " << ctype);
            continue;
        }
*/
        for (unsigned int i=0; i<(*b)->m_width; ++i) {
            if (BMAUtils::GetCharacterAndIndexForColumn(bma, alignmentIndex, row, &residue, &seqIndex)) {
                aI2sI[alignmentIndex] = seqIndex;
                //cerr << " mapping:  ai/si " << alignmentIndex << "/" << seqIndex << " " << residue << endl;
            } else {
                aI2sI[alignmentIndex] = BMA::eUndefined;
                //cerr << " failed mapping:  ai/si " << alignmentIndex << "/" << seqIndex << endl;
            }
            ++alignmentIndex;
        }
    }
}


void BMAUtils::PrintPSSM(const BMA& bma, bool rowMajor, string* stringOutput) {

    if (!bma.GetPSSM()) {
        ERROR_MESSAGE_CL("Invalid PSSM for BlockMultipleAlignment object");
        return;
    }

    CNcbiOstrstream oss;

    unsigned int ni = bma.GetPSSM()->rows, nj = bma.GetPSSM()->columns;
    int** pssmMatrix = bma.GetPSSM()->matrix;

    //  copied from su_pssm.cpp (NCBIStdaaResidues).
    const string BLASTResidues = "-ABCDEFGHIKLMNPQRSTVWXYZU*OJ";

    oss << endl << "Raw matrix ... dimensions " << ni << " " << nj << endl;
    if (rowMajor) {
        for (unsigned int i =0; i < ni; ++i) {
            for (unsigned int j = 0; j < nj; ++j) {
                oss << "Alignment pos " << i+1 << "; Residue " << j+1 << " (" << BLASTResidues[j] << "); matrix[i][j] = " << pssmMatrix[i][j] << endl;
            }
        }
    } else {
        for (unsigned int j = 0; j < nj; ++j) {
            for (unsigned int i =0; i < ni; ++i) {
                oss << "Alignment pos " << i+1 << "; Residue " << j+1 << " (" << BLASTResidues[j] << "); matrix[i][j] = " << pssmMatrix[i][j] << endl;
            }
        }
    }

    oss << '\0';

    if (stringOutput) {
        *stringOutput = oss.str();
    } else {
        EDiagSev oldPostLevel = SetDiagPostLevel(eDiag_Info);
        SetDiagPostFlag(eDPF_OmitInfoSev);

        LOG_MESSAGE_CL(oss.str());
        
        SetDiagPostLevel(oldPostLevel);
        UnsetDiagPostFlag(eDPF_OmitInfoSev);
    }

}

//  If dumpRawMatrix = true, print the raw matrix in addition to other output (can use to
//  ensure indices/values in proper correspondence and to help debugging).
//  If viewColumn = true, for each aligned residue print the score of every possible residue
//  at that position.
//  All indices in output are ONE-based.
//  Output is to the diagnostic stream defined at invokation.
void BMAUtils::PrintPSSMByRow(const BMA& bma, bool dumpRawMatrix, bool viewColumn, AlignmentCharacterType ctype) {
    unsigned int row, nRows = bma.NRows();

    EDiagSev oldPostLevel = SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_OmitInfoSev);

    LOG_MESSAGE_CL("printing pssm...:  dimensions " << bma.GetPSSM()->rows << " " << bma.GetPSSM()->columns << "\n");

    for (row = 0; row < nRows; ++row) {
        PrintPSSMForRow(bma, row, viewColumn, ctype);
    }


    if (dumpRawMatrix) {
        LOG_MESSAGE_CL("****************************************");
        LOG_MESSAGE_CL("****************************************");
        LOG_MESSAGE_CL("****************************************\n");

        PrintPSSM(bma);
    }

    SetDiagPostLevel(oldPostLevel);
    UnsetDiagPostFlag(eDPF_OmitInfoSev);
}

//  If viewColumn = true, for each aligned residue print the score of every possible residue
//  at that position.
//  All indices in output are ONE-based.
//  Output is to the diagnostic stream defined at invokation.
void BMAUtils::PrintPSSMForRow(const BMA& bma, unsigned int row, bool viewColumn, AlignmentCharacterType ctype) {

    bool isAlignedBlock, isInPssm;
    char residue, masterResidue;
    unsigned int seqIndex, masterSeqIndex, alphabetSize;
    unsigned int column = 0, blockNum = 0, alignedBlockNum = 0, nRows = bma.NRows();
    int thisScore, score = 0;

    BMA::ConstBlockList blocks;
	bma.GetBlockList(blocks);
    BMA::ConstBlockList::const_iterator b, be;
    if (blocks.size() == 0) {
        ERROR_MESSAGE_CL("PrintPSSMForRow() - alignment has no blocks\n");
        return;
    }
    if (row >= nRows) {
        ERROR_MESSAGE_CL("Invalid row " << row << "; CD has " << nRows << " rows.\n");
        return;
    }
    if (!bma.GetPSSM()) {
        ERROR_MESSAGE_CL("Invalid PSSM for BlockMultipleAlignment object");
        return;
    }

    EDiagSev oldPostLevel = SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_OmitInfoSev);


    CNcbiOstrstream oss;
    IOS_BASE::fmtflags initFlags = oss.flags();
    oss << endl;

    alphabetSize = (unsigned int) bma.GetPSSM()->columns;

    be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        isAlignedBlock = (*b)->IsAligned();

        //  skip blocks which have wrong type
        if (!IsBlockConsistentWithType(*b, ctype)) {
            column += (*b)->m_width;
            ++blockNum;
            if (isAlignedBlock) ++alignedBlockNum;
            TRACE_MESSAGE_CL("Block inconsistent with type " << ctype);
            continue;
        }

        for (unsigned int i=0; i<(*b)->m_width; ++i) {

            if (!IsColumnOfType(bma, column, ctype, isInPssm, *b)) {
                TRACE_MESSAGE_CL(" Column " << column << " not of type " << ctype);
                ++column;
                continue;
            } else {
                TRACE_MESSAGE_CL(" is column " << column << " of type " << ctype << " in pssm? " << isInPssm);
            }

            oss << "Row " << setw(4) << row+1 << "; alignment index " << setw(4) << column+1 << "; ";
            if (isAlignedBlock) {
                oss << "Aligned Block " << setw(4) << alignedBlockNum+1;
            } else {
                oss << "Block " << setw(4) << blockNum+1;
            }
            oss << "; Res " << setw(4) << i+1 << " of " << setw(4) << (*b)->m_width;

            if (BMAUtils::GetCharacterAndIndexForColumn(bma, column, row, &residue, &seqIndex)) {
//                thisScore = GetPSSMScoreOfCharWithAverageOfBZ(pssmMatrix, masterRange->from + i, sequence->m_sequenceString[range->from + i]);
                if (isInPssm && BMAUtils::GetCharacterAndIndexForColumn(bma, column, 0, &masterResidue, &masterSeqIndex)) {
                    
                    thisScore = GetPSSMScoreOfCharWithAverageOfBZ(bma.GetPSSM(), masterSeqIndex, residue);
                    score += thisScore;
                    oss << "; master/slave sequence pos: " << setw(4) << masterSeqIndex+1 << "/";

                    oss.setf(IOS_BASE::left, IOS_BASE::adjustfield);
                    oss << setw(4) << seqIndex+1 << " slave residue ";

                    oss.setf(IOS_BASE::right, IOS_BASE::adjustfield);
                    oss << residue << " score " << setw(5) << thisScore;

                    oss.setf(initFlags, IOS_BASE::adjustfield);

                    if (viewColumn) {
                        oss << " residue number " << setw(4) << LookupNCBIStdaaNumberFromCharacter(residue) << endl;
                        for (unsigned int k=0; k < alphabetSize; ++k) {
                            oss << "  " << k+1 << "  " << bma.GetPSSM()->matrix[masterSeqIndex][k];
                        }
                    }
                } else {
                    oss << "; sequence pos: " << setw(4) << seqIndex+1 << " residue " << residue;
                }

            } else {
                oss << "; character " << residue;
            }
            oss << endl;
            ++column;
        }
        ++blockNum;
        if (isAlignedBlock) ++alignedBlockNum;
    }

    oss << '\0';
    LOG_MESSAGE_CL(oss.str());

    SetDiagPostLevel(oldPostLevel);
    UnsetDiagPostFlag(eDPF_OmitInfoSev);
}


void BMAUtils::PrintPSSMByColumn(const BMA& bma, bool dumpRawMatrix, bool viewColumn, AlignmentCharacterType ctype) {

    unsigned int alignmentIndex, aWidth;
//    map<unsigned int, const Block::Range*> ranges;
//    map<unsigned int, const Sequence*> sequences;

//    const BMA::ConstBlockList& blocks = bma.GetBlockList();
//    BMA::ConstBlockList::const_iterator b = blocks.begin(), be = blocks.end();

    EDiagSev oldPostLevel = SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_OmitInfoSev);

    TRACE_MESSAGE_CL("printing pssm by column:  dimensions " << bma.GetPSSM()->rows << " " << bma.GetPSSM()->columns << "\n");

    aWidth = bma.AlignmentWidth();
    for (alignmentIndex = 0; alignmentIndex < aWidth; ++alignmentIndex) {
        PrintPSSMForColumn(bma, alignmentIndex, viewColumn, ctype);
    }

    if (dumpRawMatrix) {
        LOG_MESSAGE_CL("****************************************");
        LOG_MESSAGE_CL("****************************************");
        LOG_MESSAGE_CL("****************************************\n");

        PrintPSSM(bma);
    }

    SetDiagPostLevel(oldPostLevel);
    UnsetDiagPostFlag(eDPF_OmitInfoSev);

}

void BMAUtils::PrintPSSMForColumn(const BMA& bma, unsigned int column, bool viewColumn, AlignmentCharacterType ctype) {

    if (!bma.GetPSSM()) {
        ERROR_MESSAGE_CL("Invalid PSSM for BlockMultipleAlignment object");
        return;
    }

    bool isAlignedBlock = false;
    bool columnInPssm;
    char residue;
    int thisScore, score = 0;
    unsigned int alignmentIndex = 0, alignedBlockNum = 0, blockNum = 0;
    unsigned int alphabetSize, blockIndex, seqIndex, masterSeqIndex, nRows, row;


    BMA::ConstBlockList blocks;
	bma.GetBlockList(blocks);
    BMA::ConstBlockList::const_iterator b = blocks.begin(), be = blocks.end();
    const Block* thisBlock = NULL;

    //  Find which block this column is in.
    for (; b!=be; ++b) {
        isAlignedBlock = (*b)->IsAligned();

        if (column < alignmentIndex + (*b)->m_width) {
            thisBlock = *b;
            blockIndex = column - alignmentIndex;
            break;
        }
        alignmentIndex += (*b)->m_width;
        if (isAlignedBlock) ++alignedBlockNum;
        ++blockNum;
    }

    if (!thisBlock) {
        ERROR_MESSAGE_CL("No block found for alignment index " << column);
        return;
    }

    //  Don't do anything if the column is of the wrong type.
    if (!IsColumnOfType(bma, column, ctype, columnInPssm, thisBlock)) return;


    EDiagSev oldPostLevel = SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_OmitInfoSev);

    CNcbiOstrstream oss;
    oss << endl;  

    masterSeqIndex = 0;
    nRows = bma.NRows();
    alphabetSize = (unsigned int) bma.GetPSSM()->columns;

    for (row = 0; row < nRows; ++row) {

        //  returns false if the seqIndex is invalid (typically, the position is a gap),
        BMAUtils::GetCharacterAndIndexForColumn(bma, column, row, &residue, &seqIndex);
        if (row == 0) masterSeqIndex = seqIndex;

        //cout << "    blockIndex " << blockIndex+1 << "  residue " << residue << "  seqIndex " << seqIndex+1 << " is Aligned " << isAlignedBlock << " (return from GCAIFC: " << validSeqIndex << ")\n";

        if (columnInPssm) {
            thisScore = GetPSSMScoreOfCharWithAverageOfBZ(bma.GetPSSM(), masterSeqIndex, residue);
            score += thisScore;
        } else {
            thisScore = kMax_Int;
        }

        if (isAlignedBlock) {
            oss << "Block " << setw(4) << alignedBlockNum+1 << "; Res ";
        } else {
            oss << "Unaligned Block; Res ";
        }
        oss << setw(4) << blockIndex+1 << " of " << setw(4) << thisBlock->m_width << "; master/slave pos: " 
            << setw(4) << masterSeqIndex + 1<< " " << setw(4) << seqIndex + 1
            << "; Row " << setw(4) << row+1 << " char " << residue;

        if (columnInPssm) {
            oss << " score " << setw(4) << thisScore;
            if (viewColumn) {
                oss << " residue number " << setw(4) << LookupNCBIStdaaNumberFromCharacter(residue);
                for (unsigned int k=0; k < alphabetSize; ++k) {
                    oss << endl << "  " << k+1 << "  " << bma.GetPSSM()->matrix[masterSeqIndex][k];
                }
            }
        }
        oss << endl;
    }
    oss << '\0';
    ERR_POST(ncbi::Info << oss.str());

    SetDiagPostLevel(oldPostLevel);
    UnsetDiagPostFlag(eDPF_OmitInfoSev);
}

void BMAUtils::PrintUnalignedBlocksByColumn(const BMA& bma, bool allowGapsInColumn, bool viewColumn) {

    if (allowGapsInColumn) {
        PrintPSSMByColumn(bma, false, viewColumn, eUnalignedCharacters);
    } else {
        PrintPSSMByColumn(bma, false, viewColumn, eUnalignedInPssm);
    }
}

bool BMAUtils::IsColumnOfType(const BMA& bma, unsigned int column, AlignmentCharacterType ctype, bool& isInPssm, const Block* block) {

    unsigned int alignmentIndex = 0;

    //  See if the column is in the PSSM (don't need to look if eAlignedResidues)
    //  Column is in PSSM if:  eAlignedResidues, eUnalignedInPssm, eInPssm.
    //  Column is not in PSSM if:  eNotInPssm.
    //  Column may or may not be in PSSM if:  eUnalignedCharacters, eAllCharacters

    isInPssm = IsColumnInPSSM(bma, column);
    if (((ctype == eAlignedResidues || ctype == eUnalignedInPssm || ctype == eInPssm) && !isInPssm) || 
        (ctype == eNotInPssm && isInPssm)) {
        TRACE_MESSAGE_CL("Alignment index " << column << ":  in-pssm property (" << isInPssm << ") wrong for requested type " << ctype);
        return false;
    }
    TRACE_MESSAGE_CL("Alignment index " << column << ":  in-pssm property of column = " << isInPssm << "; requested type " << ctype);

    BMA::ConstBlockList blocks;
	bma.GetBlockList(blocks);
    BMA::ConstBlockList::const_iterator b = blocks.begin(), be = blocks.end();

    //  If not supplied, find the block this column is in; verify column is in the block if was passed
    for (; b!=be; ++b) {
        if (column < alignmentIndex + (*b)->m_width) {
            if (block) {
                if (block->GetRangeOfRow(0)->from != (*b)->GetRangeOfRow(0)->from) {
                    ERROR_MESSAGE_CL("inconsistent block error:  column " << column << "; block->from/width " << block->GetRangeOfRow(0)->from << "/" << block->m_width << "; found from/width " << (*b)->GetRangeOfRow(0)->from << "/" << (*b)->m_width);
                    return false;
                }
            } else {
                block = *b;
            }
            break;
        }
        alignmentIndex += (*b)->m_width;
    }
    if (!block) {
        ERROR_MESSAGE_CL("No block found for alignment index " << column);
        return false;
    }

    //  Check if the requested column is of the specified block type.
    return IsBlockConsistentWithType(block, ctype);
}


bool BMAUtils::IsColumnInPSSM(const BMA& bma, unsigned int column) {

    bool result = false;
    char residue;

    result = GetCharacterForColumn(bma, column, 0, &residue);

    TRACE_MESSAGE_CL("Alignment index " << column << ":  in-pssm property of column = " << result);
    return result;
}

bool BMAUtils::IsBlockConsistentWithType(const Block* block, AlignmentCharacterType ctype) {

    if (!block) return false;

    bool result = true;
    bool isAlignedBlock = block->IsAligned();

    if ((!isAlignedBlock && ctype == eAlignedResidues) || 
        (isAlignedBlock && (ctype == eNotInPssm || ctype == eUnalignedInPssm || ctype == eUnalignedCharacters))) {
        result = false;
    }

    return result;
}

END_SCOPE(align_refine)
