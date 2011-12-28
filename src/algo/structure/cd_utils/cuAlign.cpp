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
 * Author:  Adapted from CDTree-1 code by Chris Lanczycki
 *
 * File Description:
 *        
 *          Utility routines for manipulating alignments.
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuCppNCBI.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/general/Object_id.hpp>

#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>
#include <algo/structure/cd_utils/cuAlign.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

bool GetSeqID(const CRef< CSeq_align >& seqAlign, CRef< CSeq_id >& SeqID, bool getSlave)
{
	//-------------------------------------------------------------------------
	// get a SeqID.
	// first get the row'th DenDiag, then the Slave/Master's SeqID.
	//-------------------------------------------------------------------------
	CRef< CDense_diag > DenDiag;
	CDense_diag::TIds IdsSet;
	CDense_diag::TIds::iterator i;

    if (seqAlign.NotEmpty()) {
        if (seqAlign->GetSegs().IsDendiag() && GetFirstOrLastDenDiag(seqAlign, true, DenDiag)) {
		    IdsSet = DenDiag->GetIds();
        } else if (seqAlign->GetSegs().IsDenseg()) {
            IdsSet = seqAlign->GetSegs().GetDenseg().GetIds();
        }
		i = IdsSet.begin();
		if (getSlave) 
		{
			i++;
		}
		SeqID = (*i);
		return(true);
	}
	return(false);
}

bool HasSeqID(const CRef< CSeq_align >& seqAlign, const CRef< CSeq_id >& SeqID, bool& isMaster)
{
	//-------------------------------------------------------------------------
	// get a SeqID.
	// first get the row'th DenDiag, then the Slave/Master's SeqID.
	//-------------------------------------------------------------------------
    bool hasMatch = false;
	CRef< CDense_diag > DenDiag;
	CDense_diag::TIds IdsSet;
	CDense_diag::TIds::iterator i;

    if (seqAlign.NotEmpty()) {
        if (seqAlign->GetSegs().IsDendiag() && GetFirstOrLastDenDiag(seqAlign, true, DenDiag)) {
		    IdsSet = DenDiag->GetIds();
        } else if (seqAlign->GetSegs().IsDenseg()) {
            IdsSet = seqAlign->GetSegs().GetDenseg().GetIds();
        }
		i = IdsSet.begin();
        while (!hasMatch && i != IdsSet.end()) {
            if (SeqIdsMatch(SeqID, *i)) {
                hasMatch = true;
            }
            ++i;
        }
        isMaster = (hasMatch && (--i == IdsSet.begin()));
	}
	return(hasMatch);
}

int  SeqAlignRemap(CRef< CSeq_align >& source, int iSeq, CRef< CSeq_align >& guide, int iMaster, CRef< CSeq_align >& mappedAlign, int iMasterNew, int iSeqNew, int flags, string& err) {
    int nBlocks = 0;
    TDendiag mappedAlignDD;
    TDendiag *sourceDD, *guideDD;

    //  Sanity checks on input.
    err.erase();
    if (source.Empty()) {
        err = "SeqAlignRemap:  Empty alignment in source.\n";
    } else if (guide.Empty()) {
        err = "SeqAlignRemap:  Empty alignment in guide.\n";
    } else if (mappedAlign.Empty()) {
        err = "SeqAlignRemap:  Empty alignment in target mapped alignment.\n";
    }
    if (err.size() > 0) return nBlocks;

    if (source->GetDim() != guide->GetDim()) {
        err = "SeqAlignRemap:  Inconsistent dimensions for source and guide alignments.\n";
//    } else if (source->GetType() != guide->GetType()) {
//        err = "SeqAlignRemap:  Inconsistent types for source and guide alignments.\n";
    } else if (source->GetSegs().Which() != guide->GetSegs().Which()) {
        err = "SeqAlignRemap:  Inconsistent segment types for source and guide alignments.\n";
    }
    if (err.size() > 0) return nBlocks;

    /*
    bool dummy = false;
	if (dummy) {
		string err("");
		if (!WriteASNToFile("sourceAlign.aln", *source, false, &err)) {
			int i = 0;
		} 
		if (!WriteASNToFile("guideAlign.aln", *guide, false, &err)) {
			int i = 0;
		} 
	}
    */

    if (GetDDSetFromSeqAlign(*source, sourceDD) && GetDDSetFromSeqAlign(*guide, guideDD)) {
        mappedAlign->SetType(source->GetType());
        mappedAlign->SetDim(source->GetDim());
        mappedAlign->SetSegs().Select(source->GetSegs().Which());
        nBlocks = ddRemap(sourceDD, iSeq, guideDD, iMaster, &mappedAlignDD, iMasterNew, iSeqNew, flags, err);
        for (TDendiag::iterator ddIt = mappedAlignDD.begin(); ddIt != mappedAlignDD.end(); ++ddIt) {
            mappedAlign->SetSegs().SetDendiag().push_back(*ddIt);
        }
    }

    /*
    if (dummy) {
		string err("");
		if (!WriteASNToFile("mappedAlign.aln", *mappedAlign, false, &err)) {
			int i = 0;
		}
    } 
    */

    return nBlocks;
}


void MakeMaskedSeqAlign(const CRef< CSeq_align >& originalAlign, const CRef< CSeq_align >& maskAlign, CRef< CSeq_align >& maskedAlign, bool useMaskMaster, bool invertMask) {
    
    
    bool inputOK = true;
    bool isAligned, useOriginalMaster;
    TSeqPos newMasterStart, newSlaveStart, newLength;
    TSeqPos originalMasterStart, originalSlaveStart, originalLength, blockStart;
    
    CRef< CSeq_id > masterId, slaveId, maskId;
    const TDendiag* originalDDSet;
    TDendiag* maskedDDSet;
    TDendiag_cit originalBlock, originalBlock_end;
    
    if (originalAlign.Empty() || !GetSeqID(originalAlign, masterId, false) 
                              || !GetSeqID(originalAlign, slaveId, true)
                              || !GetDDSetFromSeqAlign(*originalAlign, originalDDSet)) {
        inputOK = false;
    } else if (maskAlign.Empty() || !GetSeqID(maskAlign, maskId, !useMaskMaster)) {
        inputOK = false;
    } else if (maskedAlign.Empty()) {
        inputOK = false;
    }
    
    if (SeqIdsMatch(masterId, maskId)) {
        useOriginalMaster = true;
    } else if (SeqIdsMatch(slaveId, maskId)) {
        useOriginalMaster = false;
    } else {
        useOriginalMaster = false;
        inputOK = false;
    }

    
    //  Set items from original alignment; optional 'score' and 'bounds'
    //  are not copied over as they're likely to be inconsistent once
    //  the mask has been applied.  Similarly, do not attempt to fill in
    //  the 'strands' and 'scores' fields of the individual new dense diags.
    maskedAlign->SetType(originalAlign->GetType());
    maskedAlign->SetDim(originalAlign->GetDim());
    maskedAlign->SetSegs().Select(originalAlign->GetSegs().Which());
    if (inputOK && GetDDSetFromSeqAlign(*maskedAlign, maskedDDSet)) {
        
        
        originalBlock_end = originalDDSet->end();
        for (originalBlock = originalDDSet->begin(); originalBlock != originalBlock_end; ++originalBlock) {
            
            originalMasterStart = (*originalBlock)->GetStarts().front();
            originalSlaveStart  = (*originalBlock)->GetStarts().back();
            originalLength      = (*originalBlock)->GetLen();
            newLength           = 0;
            newMasterStart      = 0;
            newSlaveStart       = 0;
            
            //  At each position on the original alignment, check if position is aligned.
            //  If so, start a new block on the masked alignment if not already in one.
            //  If not, add current block to masked alignment if it's the first unaligned
            //  residue after a string of aligned residues.  After scan block, check for
            //  a masked block stretching to the C-terminus of the original block.
            blockStart = (( useOriginalMaster) ? originalMasterStart : originalSlaveStart);
            for (TSeqPos blockPos = 0; blockPos < originalLength; ++blockPos) {
                isAligned = IsPositionAligned(*maskAlign, blockPos + blockStart, useMaskMaster);
                if ((isAligned && !invertMask) || (!isAligned && invertMask)) {
                    if (newLength == 0) {
                        newMasterStart = blockPos + blockStart;
                        newSlaveStart  = blockPos + ((!useOriginalMaster) ? originalMasterStart : originalSlaveStart);
                    }
                    ++newLength;
                } else if (newLength > 0) {
                    AddIntervalToDD(maskedDDSet, masterId, slaveId, newMasterStart, newSlaveStart, newLength);
                    newLength = 0;
                }
            }
            if (newLength > 0) {
                AddIntervalToDD(maskedDDSet, masterId, slaveId, newMasterStart, newSlaveStart, newLength);
            }
            
        }

    }

}


bool SeqAlignsAreEquivalent(const CRef< CSeq_align >& align1, const CRef< CSeq_align >& align2, bool checkMasters) {
    bool result = false;
    const TDendiag* ddSet1;
    const TDendiag* ddSet2;

    if (GetDDSetFromSeqAlign(*align1, ddSet1) && GetDDSetFromSeqAlign(*align2, ddSet2)) {
        result = ddAreEquivalent(ddSet1, ddSet2, checkMasters);
    }
    return result;
}

void SeqAlignSwapMasterSlave(CRef< CSeq_align >& seqAlign, CRef< CSeq_align >& swappedSeqAlign) {

    TDendiag* originalDDSet;
    TDendiag* swappedDDSet;

    swappedSeqAlign->Assign(*seqAlign);  //  copies over the non-DD data
    if (GetDDSetFromSeqAlign(*seqAlign, originalDDSet) && GetDDSetFromSeqAlign(*swappedSeqAlign, swappedDDSet)) {
        swappedDDSet->clear();
        ddRecompose(originalDDSet, 1, 0, swappedDDSet);
    }
}

//  Assumes CDD-style seq-align using Dendiags with dimension 2.
bool ChangeSeqIdInSeqAlign(CRef< CSeq_align>& sa, const CRef< CSeq_id >& newSeqId, bool onMaster)
{
    bool result = (sa->SetSegs().IsDendiag() && sa->SetSegs().SetDendiag().size() > 0);
    TDendiag_it ddIt, ddEnd;
    unsigned int index = (onMaster) ? 0 : 1;

    //  Sanity check the dendiag...
    if (result) {
        ddIt  = sa->SetSegs().SetDendiag().begin();
        ddEnd = sa->SetSegs().SetDendiag().end();
        for (; ddIt != ddEnd; ++ddIt) {
            if ((*ddIt)->GetDim() != 2 || (*ddIt)->GetIds().size() != 2) {
                result = false;
                break;
            }
        }
    }

    if (result) {
        ddIt  = sa->SetSegs().SetDendiag().begin();
        ddEnd = sa->SetSegs().SetDendiag().end();
        CDense_diag::TIds ids;
        for (; ddIt != ddEnd; ++ddIt) {
            ids = (*ddIt)->SetIds();
            ids[index]->Assign(*newSeqId);
        }
    }

    return result;
}


//  convenience function
int MapPositionToMaster(int childPos, const CSeq_align&  align) {

    return MapPosition(align, childPos, CHILD_TO_MASTER);
}

//  convenience function
int MapPositionToChild(int masterPos, const CSeq_align&  align) {

    return MapPosition(align, masterPos, MASTER_TO_CHILD);
}

/*  RENAME  */   
//  Was CCdCore::GetSeqPosition(const TDendiag* ddlist, int Position, bool OnMasterRow) {
int MapPosition(const CSeq_align& seqAlign, int Position, CoordMapDir mapDir) {
//---------------------------------------------------------------------------
// If mapDir = MASTER_TO_CHILD, then get position on slave 
// row that corresponds to Position on master row.  Otherwise (i.e. CHILD_TO_MASTER),
// get position on master row that corresponds to Position on slave row.
// Assumes the Seq_align is a standard CD alignment of two sequences.
//---------------------------------------------------------------------------
  TDendiag_cit  i, ddend;
  CDense_diag::TStarts::const_iterator  k;
  int  Start, Len, OtherStart;

  const TDendiag* ddlist; // = new TDendiag;
  if (GetDDSetFromSeqAlign(seqAlign, ddlist)) {

    ddend = ddlist->end();
    for (i=ddlist->begin(); i!=ddend; i++) {
        k = (*i)->GetStarts().begin();
        Len = (*i)->GetLen();
        Start = (mapDir == MASTER_TO_CHILD) ? *k : *(++k);
//      Start = OnMasterRow ? *k : *(++k);
        k = (*i)->GetStarts().begin();
        OtherStart = (mapDir == MASTER_TO_CHILD) ? *(++k)  : *k;
//      OtherStart = OnMasterRow ? *(++k)  : *k;
        if ((Position >= Start) && (Position < (Start+Len))) {
            return(OtherStart + (Position-Start));
        }
    }
  }
//  delete ddlist;
  return(INVALID_POSITION);
}



/*  ADDED  */  
bool IsPositionAligned(const CSeq_align& seqAlign, int Position, bool onMaster) {
    bool result = false;

    if (Position == INVALID_POSITION) {
        return result;
    }

    const TDendiag* pDenDiagSet;  // = new TDendiag;
    if (GetDDSetFromSeqAlign(seqAlign, pDenDiagSet)) {
        result = IsPositionAligned(pDenDiagSet, Position, onMaster);
    }
    return result;
}

/*  ADDED  */  
bool IsPositionAligned(const TDendiag*& pDenDiagSet, int Position, bool onMaster) {
    bool result = false;
    int start, stop;
    TDendiag_cit i, iend;

    if (Position == INVALID_POSITION) {
        return result;
    }

    //  for each block, check if Position is in range
    if (pDenDiagSet) {
        iend = pDenDiagSet->end();
        for (i=pDenDiagSet->begin(); i!=iend; i++) {
            start = (onMaster) ? (*i)->GetStarts().front() : (*i)->GetStarts().back();
            stop  = start + (*i)->GetLen() - 1;
            if (Position >= start && Position <= stop) {
                result = true;
                break;
            }
        }
    }
    return result;
}

//   Return the number of positions of align1 also aligned on align2.
int  GetAlignedPositions(const CRef< CSeq_align >& align1, const CRef< CSeq_align >& align2, vector<int>& alignedPositions, bool onMaster) {

    int nBlocks, position;
    CRef< CSeq_id > align1Id,  align2Id;
    vector<int> align1Blocks, align1Starts;

    alignedPositions.clear();

    if (align1.NotEmpty() && align2.NotEmpty()) {
    
        //  The sequences need to be the same to check common positions.
        if (GetSeqID(align1, align1Id, !onMaster) && GetSeqID(align2, align2Id, !onMaster) && 
            SeqIdsMatch(align1Id, align2Id)) {
        
            GetBlockLengths(align1, align1Blocks);
            GetBlockStarts(align1, align1Starts, onMaster);
        
            //  Look for residues from align1 aligned on align2.
            nBlocks = align1Blocks.size();
            for (int i = 0; i < nBlocks; ++i) {
                position = align1Starts[i];
                for (int j = 0; j < align1Blocks[i]; ++j) {
                    if (IsPositionAligned(*align2, position, onMaster)) {
                        alignedPositions.push_back(position);
                    }
                    ++position;
                }
            }
        }
    }
    return alignedPositions.size();
}


int  GetNumAlignedResidues(const CRef< CSeq_align >& seqAlign) {

  TDendiag_cit i;
  int  Len=0;

  if (seqAlign.Empty()) {
      return Len;
  }

  // get den-diags for master row; sum lengths
  const TDendiag* pDenDiagSet;  // = new TDendiag;
  if (GetDDSetFromSeqAlign(*seqAlign, pDenDiagSet)) {
    for (i=pDenDiagSet->begin(); i!=pDenDiagSet->end(); i++) {
      Len += (*i)->GetLen();
    }
  }
  return(Len);

}

int  GetLowerBound(const CRef< CSeq_align >& seqAlign, bool onMaster) {

    int  lowerBound = -1;
    if (seqAlign.Empty()) {
        return lowerBound;
    }

    const TDendiag* pDenDiagSet;  // = new TDendiag;
    if (GetDDSetFromSeqAlign(*seqAlign, pDenDiagSet)) {
        lowerBound = (onMaster) ? pDenDiagSet->front()->GetStarts().front() : pDenDiagSet->front()->GetStarts().back();
    }
    return(lowerBound);

}

int  GetUpperBound(const CRef< CSeq_align >& seqAlign, bool onMaster) {
    int  upperBound = -1;
    if (seqAlign.Empty()) {
        return upperBound;
    }

    const TDendiag* pDenDiagSet;  // = new TDendiag;
    if (GetDDSetFromSeqAlign(*seqAlign, pDenDiagSet)) {
        upperBound = (onMaster) ? pDenDiagSet->back()->GetStarts().front() : pDenDiagSet->back()->GetStarts().back();
        upperBound += pDenDiagSet->back()->GetLen() - 1;
    }
    return(upperBound);

}

//  Use the alignment to extract as a single string those residues that are aligned.
//  If pAlignedRes hasn't been allocated, do so.
void SetAlignedResiduesOnSequence(const CRef< CSeq_align >& align, const string& sequenceString, char*& pAlignedRes, bool isMaster) {

    int length;
    int alignedResCtr = 0;
    int start = -1, stop = -1;
    CRef< CDense_diag > ddFirst, ddLast;
    
    if (align.Empty() || sequenceString.size() < 1) {
        return;
    }

    length = GetNumAlignedResidues(align);
    if (length < 1 || (int) sequenceString.size() < length) {
        return;
    } else {
        //  Allocate space for pAlignedRes if not already done
        if (!pAlignedRes) {
            pAlignedRes = new char[length];
            if (!pAlignedRes) return;
        }
    }

    if (GetFirstOrLastDenDiag(align, true, ddFirst) && GetFirstOrLastDenDiag(align, false, ddLast)) {
        if (ddFirst.NotEmpty() && ddLast.NotEmpty()) {
            start = (isMaster) ? ddFirst->GetStarts().front() : ddFirst->GetStarts().back();
            stop  = (isMaster) ? ddLast->GetStarts().front()  : ddLast->GetStarts().back();
            stop += ddLast->GetLen() - 1;
        }
    }

    alignedResCtr = 0;
    const TDendiag* pDenDiagSet;  // = new TDendiag;
    if (GetDDSetFromSeqAlign(*align, pDenDiagSet)) {
//        if (start >=0 && start < length && stop >=0 && stop < length) {
        if (start >=0 && start <= stop && stop < (int) sequenceString.size()) {
            for (int i = start; i <= stop; ++i) {
                if (IsPositionAligned(pDenDiagSet, i, isMaster) && alignedResCtr < length) {
                    //ASSERT(alignedResCtr < length);
                    pAlignedRes[alignedResCtr] = sequenceString[i];
                    ++alignedResCtr;
                }
            }
        }
    }

    //  problem if alignedResCtr != length; return null pointer
    if (alignedResCtr != length) {
        delete pAlignedRes;
        pAlignedRes = NULL;
    }
    
}


//===========================================
//  Queries on block structure of alignment
//===========================================

/*  ADDED  10/28/03 */
//  return block number containing residue, or -1 if not aligned or out of range.
int GetBlockNumberForResidue(int residue, const CRef< CSeq_align >& seqAlign, bool onMaster,
                             vector<int>* starts, vector<int>* lengths) {
    int i = 0;
    int result = -1, nBlocks;
    vector<int> vstarts, vlengths;

    if (residue >= 0 && GetBlockLengths(seqAlign, vlengths) > 0 && GetBlockStarts(seqAlign, vstarts, onMaster) > 0) {
        if (vlengths.size() == vstarts.size()) {
            nBlocks = vstarts.size();
            while (i < nBlocks && result < 0) {
                if (residue >= vstarts[i] && residue < vstarts[i] + vlengths[i]) {
                    result = i;
                }
                ++i;
            }
            if (starts != NULL) {
                starts->insert(starts->begin(), vstarts.begin(), vstarts.end());
            }
            if (lengths != NULL) {
                lengths->insert(lengths->begin(), vlengths.begin(), vlengths.end());
            }
        }
    }
    return result;
}

/*  ADDED  */
// return number of blocks in alignment (0 if no alignment, or not a Dense_diag)
int GetBlockCount(const CRef< CSeq_align >& seqAlign) {
    int nBlocks = 0;
    if (seqAlign.Empty()) {
        return nBlocks;
    }
    if (seqAlign->GetSegs().IsDendiag()) {
        nBlocks = seqAlign->GetSegs().GetDendiag().size();
    }
    return nBlocks;
}


//  return number of blocks on success; return 0 on error
int GetBlockLengths(const CRef< CSeq_align >& seqAlign, vector<int>& lengths) {
    int count = 0;
    int nBlocks = GetBlockCount(seqAlign);
    const TDendiag* pDenDiagSet = NULL;
    TDendiag_cit cit;

    if (seqAlign.NotEmpty() && nBlocks > 0) {
        lengths.clear();
        if (GetDDSetFromSeqAlign(*seqAlign, pDenDiagSet)) {
            for (cit = pDenDiagSet->begin(); cit != pDenDiagSet->end(); ++cit) {
                lengths.push_back((*cit)->GetLen());
                count++;
            }
        }
    } 
    count = (count == nBlocks) ? count: 0;
    return count;
}


//  convenience method; return number of blocks on success; return 0 on error
int GetBlockStartsForMaster(const CRef< CSeq_align >& seqAlign, vector<int>& starts) {
    return GetBlockStarts(seqAlign, starts, true);
}


//  return number of blocks on success; return 0 on error
int GetBlockStarts(const CRef< CSeq_align >& seqAlign, vector<int>& starts, bool onMaster) {
    int start;
    int count = 0;
    int nBlocks = GetBlockCount(seqAlign);
    const TDendiag* pDenDiagSet = NULL;
    TDendiag_cit cit;

    if (seqAlign.NotEmpty() && nBlocks > 0) {
        starts.clear();
        if (GetDDSetFromSeqAlign(*seqAlign, pDenDiagSet)) {
            for (cit = pDenDiagSet->begin(); cit != pDenDiagSet->end(); ++cit) {
                start = (onMaster) ? (*cit)->GetStarts().front() : (*cit)->GetStarts().back();
                starts.push_back(start);
                count++;
            }
        }
    } 
    count = (count == nBlocks) ? count: 0;
    return count;
}

bool GetDDSetFromSeqAlign(const CSeq_align& align, const TDendiag*& dd) {
    if (align.GetSegs().IsDendiag()) {
        dd = &(align.GetSegs().GetDendiag());
        return true;
    }
    return false;
}


bool GetDDSetFromSeqAlign(CSeq_align& align, TDendiag*& dd) {
    if (align.SetSegs().IsDendiag()) {
        dd = &(align.SetSegs().SetDendiag());
        return true;
    }
    return false;
}


bool GetFirstOrLastDenDiag(const CRef< CSeq_align >& seqAlign, bool First, CRef< CDense_diag >& DenDiag) {
//-------------------------------------------------------------------------
// get either the first or last dense-diag of the seqAlign
//-------------------------------------------------------------------------
  const TDendiag* pDenDiagSet;                     // (TDendiag = list<CRef<CDense_diag>>)
  TDendiag_cit k;

  if (seqAlign.NotEmpty() && GetDDSetFromSeqAlign(*seqAlign, pDenDiagSet)) {

    if (First) {
      k = pDenDiagSet->begin();
    }
    else {
      k = pDenDiagSet->end();
      k--;
    }
    DenDiag = (*k);
    return(true);
  }
  return(false);
}

bool CheckSeqIdInDD(const CRef< CSeq_align >& seqAlign)
{
    int iii;
	const TDendiag* pDenDiagSet;     // (TDendiag = list<CRef<CDense_diag>>)
	TDendiag_cit k;
	CDense_diag::TIds IdsSet;
	CDense_diag::TIds::iterator i;
	CRef< CSeq_id > master, slave, master2, slave2;
	if (seqAlign.NotEmpty() && GetDDSetFromSeqAlign(*seqAlign, pDenDiagSet)) 
	{
        iii=0;
		k = pDenDiagSet->begin();
		IdsSet = (*k)->GetIds();
		i = IdsSet.begin();
		master = *i;
		i++;
		slave = *i;
		k++;iii++;
		for (; k != pDenDiagSet->end(); k++, iii++)
		{
			IdsSet = (*k)->GetIds();
			i = IdsSet.begin();
			master2 = *i;
			i++;
			slave2 = *i;
			if (!(SeqIdsMatch(master, master2)) || !SeqIdsMatch(slave, slave2))
				return false;
		}
	}
	return true;
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/
_/
_/ DD<->SeqLoc transfer Functions
_/
_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/


void MakeDDFromSeqLoc(CSeq_loc * pAl,TDendiag * pDD ) {
        int from, to;

        if (!pAl) return;
        // make a DD from AlignAnnot
        //if (pAl->GetLocation().IsInt()) {
        if (pDD && pAl->IsInt()) {
                //CSeq_interval& interval = pAl->SetLocation().SetInt();
                CSeq_interval& interval = pAl->SetInt();
                from=interval.SetFrom();
                to=interval.SetTo();
                CRef< CSeq_id > RefID(new CSeq_id);
                RefID = &interval.SetId();
                AddIntervalToDD(pDD,RefID,RefID,from,from,to-from+1);
        //}  else if( pAl->GetLocation().IsPacked_int() ) {
        }  else if(pDD && pAl->IsPacked_int() ) {
                CPacked_seqint::Tdata::iterator s;        
                for (s=pAl->SetPacked_int().Set().begin(); s!=pAl->SetPacked_int().Set().end(); s++) {
                        //CSeq_interval& interval = (*s);
                        from=(*s)->GetFrom();
                        to=(*s)->GetTo();
                        CRef< CSeq_id > RefID(new CSeq_id);
                        RefID = &((*s)->SetId());
                        AddIntervalToDD(pDD,RefID,RefID ,from,from,to-from+1);
                }
        }
}


void MakeSeqLocFromDD(const TDendiag * pDD, CSeq_loc * pAl) {
        TDendiag_cit  pp;
        int iDst;
        CDense_diag::TStarts::const_iterator  pos;
        vector < CRef< CSeq_id > >::const_iterator pid;

        for (iDst=0,pp=pDD->begin(); pp!=pDD->end(); pp++,iDst++){
                pos=(*pp)->GetStarts().begin();
                TSeqPos len=((*pp)->GetLen());
                TSeqPos posStart=*pos;
                pid=(*pp)->GetIds().begin();
                //CRef<CSeq_id> SeqID=*(++pid);
                CRef<CSeq_id> SeqID=*(pid);
                
                if(pDD->size()==1){
                        pAl->SetInt().SetFrom(posStart);
                        pAl->SetInt().SetTo(posStart+len-1);
                        pAl->SetInt().SetId(*SeqID);
                }else {
                        //CSeq_interval * intrvl = new CSeq_interval();
                        //CRef< CSeq_interval > intrvl = new CSeq_interval();
                        CRef < CSeq_interval  > intrvl(new CSeq_interval());
                        intrvl->SetFrom(posStart);
                        intrvl->SetTo(posStart+len-1);
                        intrvl->SetId(*SeqID);
                        pAl->SetPacked_int().Set().push_back(intrvl);
                }
        }
}

void AddIntervalToDD(TDendiag * pDD,CRef<CSeq_id> seqID1, CRef<CSeq_id> seqID2,TSeqPos st1,TSeqPos st2, TSeqPos lll)
// Fake it 
{
        CRef< CSeq_id > idMaster(new CSeq_id);
        idMaster.Reset(seqID1);
        CRef< CSeq_id > idSeq(new CSeq_id);
        idSeq.Reset(seqID2);
                
        CRef<CDense_diag> newDD(new CDense_diag);
        newDD->SetDim(2);
        //newDD->SetIds().push_back(seqID1);
        //newDD->SetIds().push_back(seqID2);
        newDD->SetIds().push_back(idMaster);
        newDD->SetIds().push_back(idSeq);
        newDD->SetStarts().push_back(st1);
        newDD->SetStarts().push_back(st2);
        newDD->SetLen()=lll;
        pDD->push_back(newDD); // apend to the DensDiag List
}


bool GetDenDiagSet(const CRef< CSeq_annot >& seqAnnot, int Row, const TDendiag*& pDenDiagSet) {
//-------------------------------------------------------------------------
// the same as SetDenDiagSet, but insure that the returned
// den-diag-set is const.
//-------------------------------------------------------------------------
//  TDendiag* pTempDenDiagSet;
//  bool RetVal;
//  RetVal = SetDenDiagSet(seqAnnot, Row, pTempDenDiagSet);
//  pDenDiagSet = pTempDenDiagSet;
//  return(RetVal);
    list< CRef< CSeq_align > >::const_iterator j;

    if (seqAnnot->GetData().IsAlign()) {
       // figure out which dense-diag set to get (based on Row)
       if (Row == 0) j = seqAnnot->GetData().GetAlign().begin();
       else {
         int Count = 0;
         for (j= seqAnnot->GetData().GetAlign().begin();
              j!= seqAnnot->GetData().GetAlign().end(); j++) {
           if (++Count == Row) break;
         }
       }
       if ((*j)->GetSegs().IsDendiag()) {
         // get the dense-diag set
         pDenDiagSet = &((*j)->GetSegs().GetDendiag());
         return(true);
       }
    }
    return(false);
}

bool SetDenDiagSet(CRef< CSeq_annot >& seqAnnot, int Row, TDendiag*& pDenDiagSet) {
//-------------------------------------------------------------------------
// get a set of dense-diag's.  this is dense-diag info for a row.
// for Row = 0, and Row = 1, return the same DenDiagSet.
//-------------------------------------------------------------------------
    list< CRef< CSeq_align > >::iterator j;

    if (seqAnnot->GetData().IsAlign()) {
       // figure out which dense-diag set to get (based on Row)
       if (Row == 0) j = seqAnnot->SetData().SetAlign().begin();
       else {
         int Count = 0;
         for (j= seqAnnot->SetData().SetAlign().begin();
              j!= seqAnnot->SetData().SetAlign().end(); j++) {
           if (++Count == Row) break;
         }
       }
       if ((*j)->SetSegs().IsDendiag()) {
         // get the dense-diag set
         pDenDiagSet = &((*j)->SetSegs().SetDendiag());
         return(true);
       }
    }
    return(false);
}

//  Moved from the validator...
void BuildAdjacentDiags(const TDendiag_cit& begin_orig, const TDendiag_cit& end_orig, TDendiag* adj)
{
    
    //  Go through the dense_diags list and identify all adjacent blocks,
    //  namely those with no unaligned residues between them.  In the third
    //  argument 'adj', fill the dense list with only non-adjacent dense_diags,
    //  merging any adjacent ones found in the original list.
    
    int start, len, start_adj, len_adj;
    int start_slave, start_adj_slave;
    bool appended = false;
    
    CRef<CDense_diag> dd_cref;
    TDendiag_cit orig_ci;
    TDendiag_it adj_ci;
    CDense_diag::TStarts::iterator start_adj_i;
    
    //  loop over original set of starts on master
    for (orig_ci = begin_orig; orig_ci != end_orig; ++orig_ci) {
        appended = false;
        
        start = (*orig_ci)->GetStarts().front();
        start_slave = (*orig_ci)->GetStarts().back();
        len   = (*orig_ci)->GetLen();
        
        //  Look at list of new dense_diags and see if any are adjacent to original.
        //  Both aligned regions in the dense diag must be collapseable, unless
        //  one is validating a new master in the child.
        for (adj_ci = adj->begin(); adj_ci != adj->end(); ++adj_ci) {
            start_adj = (*adj_ci)->GetStarts().front();
            start_adj_slave = (*adj_ci)->GetStarts().back();
            len_adj   = (*adj_ci)->GetLen();
            
            if (start == start_adj + len_adj && start_slave == start_adj_slave + len_adj) {
                // append *orig_ci range to *adj_ci; starts remain unchanged
                (*adj_ci)->SetLen(len + len_adj);
                appended = true;
            } else if (start + len == start_adj && start_slave + len == start_adj_slave) {
                
                // prepend *orig_ci range to *adj_ci; need to update all starts
                for (start_adj_i  = (*adj_ci)->SetStarts().begin();
                start_adj_i != (*adj_ci)->SetStarts().end(); ++start_adj_i) {
                    *start_adj_i -= len;                    
                }
                (*adj_ci)->SetLen(len + len_adj);
                appended = true;
            }
            
        }
        if (!appended) {
			dd_cref = new CDense_diag();
			dd_cref->Assign(**orig_ci);
			adj->push_back(dd_cref);
		}
    }
}


bool EraseRow(CRef< CSeq_annot >& seqAnnot, int RowIndex) {
//-------------------------------------------------------------------------
// Erase the RowIndex-1 seq-align.  don't erase RowIndex 0.
//-------------------------------------------------------------------------
    list< CRef< CSeq_align > >::iterator j, jend;
    int  RowCount;

    if (RowIndex == 0) return(false);

    if (seqAnnot->GetData().IsAlign()) {
        RowCount = 1;
        jend = seqAnnot->SetData().SetAlign().end();
        for (j= seqAnnot->SetData().SetAlign().begin(); j != jend; j++) {
            if (RowCount == RowIndex) {
                seqAnnot->SetData().SetAlign().erase(j);
                return(true);
            }
            RowCount++;
            if (RowCount > RowIndex) break;
        }
    }
    return(false);
}

//input seqAlign may actually contain CSeq_align_set
CRef< CSeq_align > ExtractFirstSeqAlign(CRef< CSeq_align > seqAlign)
{
	if (seqAlign.Empty())
		return seqAlign;
	if (!seqAlign->GetSegs().IsDisc())
		return seqAlign;
	if (seqAlign->GetSegs().GetDisc().CanGet())
	{
		const list< CRef< CSeq_align > >& saList = seqAlign->GetSegs().GetDisc().Get();
		if (saList.begin() != saList.end())
			return ExtractFirstSeqAlign(*saList.begin());
	}
	CRef< CSeq_align > nullRef;
	return nullRef;
}


int ddLen(TDendiag * pDD)
{
        TDendiag_cit  pp;
        int staLen=0;
        
        for (pp=pDD->begin(); pp!=pDD->end(); pp++) 
        {
            staLen+=((*pp)->GetLen());
        }
        
        return staLen;
}


string ddAlignInfo(TDendiag * pGuideDD)
{
        TDendiag_cit  ppGuide;
        int iDst;
        CDense_diag::TStarts::const_iterator  pos;
        vector < CRef< CSeq_id > >::const_iterator pid;
        string ret="";
        char buf[1024];

        for (iDst=0,ppGuide=pGuideDD->begin(); ppGuide!=pGuideDD->end(); ppGuide++,iDst++){
                pos=(*ppGuide)->GetStarts().begin();
                TSeqPos lenGuide=((*ppGuide)->GetLen());
                TSeqPos posMasterGuide=*pos;
                TSeqPos posSeqGuide=*(++pos);

                pid=(*ppGuide)->GetIds().begin();
                CRef<CSeq_id> GuideMasterSeqID=*(pid);
                CRef<CSeq_id> GuideSeqID=*(++pid);
                
                sprintf(buf,"[%s]/[%s](%d)  ",GetSeqIDStr(GuideMasterSeqID).c_str(),GetSeqIDStr(GuideSeqID).c_str(),(int)pGuideDD->size());
                if(!iDst){
                    ret+=buf;
                }
                sprintf(buf,"#%d=[%d-%d]/[%d-%d](%d) ",iDst,posMasterGuide+1,posMasterGuide+lenGuide,posSeqGuide+1,posSeqGuide+lenGuide,lenGuide);
                ret+=buf;
        }
        return ret;
}

int ddRecompose(TDendiag * pGuideDD,int iMaster, int iSeq,TDendiag * pResultDD)
{
    TDendiag_it  ppGuide;
    int iDst;
    CDense_diag::TStarts::iterator  pos,ppos;
    vector < CRef< CSeq_id > >::iterator pid,ppid;

    for (iDst=0,ppGuide=pGuideDD->begin(); ppGuide!=pGuideDD->end(); ppGuide++,iDst++){
            ppos=pos=(*ppGuide)->SetStarts().begin();
            TSeqPos lenGuide=((*ppGuide)->GetLen());
            TSeqPos posMasterGuide=*pos;
            TSeqPos posSeqGuide=*(++pos);
            // exchange starts
            //*ppos=posSeqGuide;
            //*pos=posMasterGuide; 

            ppid=pid=(*ppGuide)->SetIds().begin();
            CRef<CSeq_id> GuideMasterSeqID=*(pid);
            CRef<CSeq_id> GuideSeqID=*(++pid);
            // exchage ids
            //*ppid=GuideSeqID;
            //*pid=GuideMasterSeqID;
            
            AddIntervalToDD(pResultDD,iMaster==0 ? GuideMasterSeqID : GuideSeqID , iSeq==0 ? GuideMasterSeqID : GuideSeqID ,iMaster==0 ? posMasterGuide : posSeqGuide ,iSeq==0 ? posMasterGuide : posSeqGuide , lenGuide);
    }
    return iDst;
}

int ddRenameSeqID(TDendiag * pGuideDD,int iNum, CRef< CSeq_id > & seqID)
{
    TDendiag_it  ppGuide;
    int iDst;
    vector < CRef< CSeq_id > >::iterator pid;

    for (iDst=0,ppGuide=pGuideDD->begin(); ppGuide!=pGuideDD->end(); ppGuide++,iDst++){
            
            CRef< CSeq_id > idCopy(new CSeq_id);
            idCopy.Reset(seqID);

            pid=(*ppGuide)->SetIds().begin();
            if(iNum)++pid;
            *(pid)=idCopy;
    }
    return iDst;
}


bool ddAreEquivalent(const TDendiag * pDD1, const TDendiag * pDD2, bool checkMasters)
{
    TDendiag_cit  pp1,pp2;
    CDense_diag::TStarts::const_iterator  pos1,pos2;
	vector < CRef< CSeq_id > >::const_iterator pid1,pid2;
	bool isSimilar=true;
	
    if(pDD1->size()!=pDD2->size())
            return false;

    for (pp1=pDD1->begin(),pp2=pDD2->begin(); pp1!=pDD1->end() && pp2!=pDD2->end(); pp1++,pp2++){
		pos1=(*pp1)->GetStarts().begin();
		TSeqPos lenGuide1=((*pp1)->GetLen());
		TSeqPos posMasterGuide1=*pos1;
		TSeqPos posSeqGuide1=*(++pos1);
		pid1=(*pp1)->GetIds().begin();
		CRef<CSeq_id> idMas1=*(pid1);
        CRef<CSeq_id> idSlv1=*(++pid1);
                
		pos2=(*pp2)->GetStarts().begin();
		TSeqPos lenGuide2=((*pp2)->GetLen());
		TSeqPos posMasterGuide2=*pos2;
		TSeqPos posSeqGuide2=*(++pos2);
		pid2=(*pp2)->GetIds().begin();
		CRef<CSeq_id> idMas2=*(pid2);
        CRef<CSeq_id> idSlv2=*(++pid2);
		
		
		if(	!SeqIdsMatch(idSlv1, idSlv2) ||
			lenGuide1!=lenGuide2 || 
			posMasterGuide1!=posMasterGuide2 || 
			posSeqGuide1!=posSeqGuide2){
			isSimilar=false;
			break;
		}

        if (checkMasters && !SeqIdsMatch(idMas1, idMas2)) {
            isSimilar=false;
            break;
        }
    }
    return isSimilar;
}

	




/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/
_/
_/ Alignment remapping functions
_/
_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
typedef struct {
        int hits;
        int crdSeq[4];
        CRef< CSeq_id > seqID[4];
        int secNum[4];
}ALICORD;

static int ddAcumAliCord(TDendiag * pDD, int interRow, ALICORD * acL,int seqRow)
{
        TDendiag_cit  pp;
        vector < CRef< CSeq_id > >::const_iterator pID;
        int maxPos=0,i,iSec;
        CDense_diag::TStarts::const_iterator  pos;


        for (iSec=0,pp=pDD->begin(); pp!=pDD->end(); pp++,iSec++) 
        {
                pos=(*pp)->GetStarts().begin();
                TSeqPos len=((*pp)->GetLen());
                TSeqPos posSeq=*(pos);
                TSeqPos posInter=*(++pos);

                pID=(*(pDD->begin()))->GetIds().begin();
                CRef< CSeq_id > idSeq=*(pID);
                CRef< CSeq_id > idInter=*(++pID);

                if(interRow==0){ // swap to the requested row 
                        TSeqPos tmp=posSeq;posSeq=posInter;posInter=tmp;
                        CRef< CSeq_id > tmi=idSeq;idSeq=idInter;idInter=tmi;
                }
                
                
                if(!acL){ // just get the maximum size - used to allocate the acL bufer in calling function
                    if(maxPos < (int) (posInter+len) )maxPos = posInter+len;
                }
                else {
                    for(i=posInter;i< (int) (posInter+len);i++){
                                acL[i].hits++;
                                acL[i].crdSeq[0]=i;
                                acL[i].crdSeq[seqRow]=posSeq+(i-posInter);
                                acL[i].seqID[0]=idInter;
                                acL[i].seqID[seqRow]=idSeq;
                                acL[i].secNum[seqRow]=iSec;
								acL[i].secNum[0]=iSec;
                        }
                        
                }
        }
        
        return maxPos;
}


static int ddScanAliCord(TDendiag * pDDList, ALICORD * acL, int maxLen,int rowMaster,int rowSeq,int iRowFollowStructure,int hitCnt)
{
        // restore overlap alignments   
        int iCnt=0,i,is,ie;

        for(i=0;i<maxLen;)
        {
                // determine  starts andend of overlap block
                if(acL[i].hits!=hitCnt){
                    //if(acL[i].hits>hitCnt)return 0; // error , this shouldn't happen
					if(acL[i].hits>3)return 0; // error , this shouldn't happen
					i++;continue;
                }
                is=i;
                //while(acL[i].hits==hitCnt && acL[i].secNum==acL[is].secNum){
                
                while(acL[i].hits==hitCnt){
						if(iRowFollowStructure!=-1){
							if(acL[i].secNum[iRowFollowStructure]!=acL[is].secNum[iRowFollowStructure])
								break;
						} else {
							if( acL[i].secNum[rowMaster]!=acL[is].secNum[rowMaster] || 
								 acL[i].secNum[rowSeq]!=acL[is].secNum[rowSeq] )
									break;
						}
						// if the sequence has a gap there 
						if(i>is && acL[i].crdSeq[rowSeq]!=acL[i-1].crdSeq[rowSeq]+1)
							break;
                        i++;
                }
                ie=i;
                
                TSeqPos posMasterNew=acL[is].crdSeq[rowMaster];
                TSeqPos posSeqNew=acL[is].crdSeq[rowSeq];
                TSeqPos lenNew=ie-is;
                //CRef< CSeq_id > idMaster=acL[is].seqID[rowMaster];
                //CRef< CSeq_id > idSeq=acL[is].seqID[rowSeq];
                CRef< CSeq_id > idMaster(new CSeq_id);
                idMaster.Reset(acL[is].seqID[rowMaster]);
                CRef< CSeq_id > idSeq(new CSeq_id);
                idSeq.Reset(acL[is].seqID[rowSeq]);
                

                // fill the new dens Diag block information
                {
                        CRef<CDense_diag> newDD(new CDense_diag);
                        newDD->SetDim(2);
                        newDD->SetIds().push_back(idMaster);
                        newDD->SetIds().push_back(idSeq);
                        newDD->SetStarts().push_back(posMasterNew);
                        newDD->SetStarts().push_back(posSeqNew);
                        newDD->SetLen()=lenNew;

                        pDDList->push_back(newDD); // apend to the DensDiag List
                        iCnt++;
                }

        }
        return iCnt;
}

int ddRemap(TDendiag * pSrcDD,int iSeq,TDendiag * pGuideDD, int iMaster,TDendiag * newDDlist,int iMasterNew, int iSeqNew,int flags,string err)
{
        // determine the length of the intermediate sequence alignment
        int maxLen2=ddAcumAliCord(pGuideDD,1-iMaster,0,1);
        int maxLen1=ddAcumAliCord(pSrcDD,1-iSeq,0,2);
        int maxLen=maxLen1>maxLen2 ? maxLen1 : maxLen2;maxLen++;
		int iFollow=-1;

        // allocate the buffer to keep coverage ALICORDs
        ALICORD* allArr=(ALICORD * )malloc(sizeof(ALICORD)* maxLen);
        if(!allArr) {
            err="remapDD error: couldn't allocate enough memory.";
            return 0;
        }

        memset(allArr,0,maxLen*sizeof(ALICORD));

        // accumulate ALICORDS
		//string debug1=ddAlignInfo(pSrcDD);
		//	string debug2=ddAlignInfo(pGuideDD);


		if(flags&DD_FOLLOWGUIDE){
			ddAcumAliCord(pSrcDD,1-iSeq,allArr,2);
			ddAcumAliCord(pGuideDD,1-iMaster,allArr,1);
			iFollow=0;
		}else {
			ddAcumAliCord(pGuideDD,1-iMaster,allArr,1);
			ddAcumAliCord(pSrcDD,1-iSeq,allArr,2);
			iFollow=0;
		}
        // restore overlap alignments   
        int iCnt=ddScanAliCord(newDDlist,allArr,maxLen,iMasterNew,iSeqNew,iFollow,2);

        free( (void * )allArr) ;
        return iCnt;
}

string ddDifferenceResidues(TDendiag * pSrcDD,TDendiag * pGuideDD,TDendiag * newDDlist)
{
		TDendiag DifferenceDD;
		if(!newDDlist)newDDlist=&DifferenceDD;
        // determine the length of the intermediate sequence alignment
        int maxLen2=ddAcumAliCord(pGuideDD,0,0,1);
        int maxLen1=ddAcumAliCord(pSrcDD,0,0,2);
        int maxLen=maxLen1>maxLen2 ? maxLen1 : maxLen2;maxLen++;
        // allocate the buffer to keep coverage ALICORDs
        ALICORD * allArr=(ALICORD * )malloc(sizeof(ALICORD)* maxLen);if(!allArr)return NULL;
        memset(allArr,0,maxLen*sizeof(ALICORD));

		ddAcumAliCord(pSrcDD,0,allArr,2);
		ddAcumAliCord(pGuideDD,0,allArr,1);

        // restore overlap alignments   
		ddScanAliCord(newDDlist,allArr,maxLen,0,0,-1,1);

		free( (void * )allArr) ;
        return ddAlignInfo(newDDlist);
}



/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/
_/
_/ Alignment sscanf/printf functions
_/
_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/

	#define scanTill( v_cond ) while(*ptr && *ptr!='\n' && (v_cond) )
bool sscanSeqId (const char * & ptr,CSeq_id & seqid)
{
	char typ[1024], id[1024];
	// empty th buffers 
	int ityp=0,iid=0;
	typ[ityp]=0;id[iid]=0;

	// scan the id 
	scanTill( *ptr==' ')ptr++; // skip spaces at the beginning of line 
	scanTill( *ptr!=' '){ // get the seqID type 
		typ[ityp++]=*ptr;
		ptr++;
	}typ[ityp]=0;
	scanTill( *ptr==' ')ptr++; // skip spaces between gi/pdb type and id itself
	scanTill( *ptr!=' '){ // get the seqID type 
		id[iid++]=*ptr;
		ptr++;
	}id[iid]=0;
	int gi;
	if( !strcmp(typ,"gi") && sscanf(id,"%d",&gi)==1 ){
		seqid.SetGi(gi);
	}
	else if( !strcmp(typ,"pdb") ){
		char * ss=strrchr(id,'_');
        //if there's a chain specified, separate it
		if( ss){
            *ss=0;
            ss++;
        	seqid.SetPdb().SetChain(*ss);
        } 
		seqid.SetPdb().SetMol().Set(id);
	}
	else return false;
	return true;
}

const char * sscanSeqLocIntervals(const char * ptr, CSeq_loc & sq)
{
	CSeq_id * sid=new CSeq_id();
	int howmany,from,to;

	//while(ptr && *ptr ){
		
		sscanSeqId (ptr,*sid);
		
		// scan each line
		scanTill(true){
			scanTill( *ptr==' ')ptr++; // skip spaces 
			if(!(howmany=sscanf(ptr,"%d-%d",&from,&to)))
				break;
			
			if (howmany==1)to=from+1;

			CRef < CSeq_interval  > intrvl(new CSeq_interval());
			intrvl->SetFrom(from);
			intrvl->SetTo(to);
			intrvl->SetId(*sid);
			sq.SetPacked_int().Set().push_back(intrvl);
			scanTill( *ptr!=' ')ptr++; // skip spaces 
		}

//		if(sq.GetPacked_int().Get().size())
//			sl.push_back(sq);

		// next line 
		ptr=strchr(ptr,'\n');
		if(ptr)ptr++;
	//}
	return ptr;
}

//==============================================
//  Query a SeqAlign for e-Values and bit scores
//==============================================

void ExtractScoreFromSeqAlign(const CRef< CSeq_align >& seqAlign, int flags, vector<double>&  scores) {
    ExtractScoreFromSeqAlign(seqAlign.GetPointer(), flags, scores);
}

void ExtractScoreFromSeqAlign(const CSeq_align* seqAlign, int flags, vector<double>&  scores) {

    int count=0;
    TDendiag_cit ddit;

    scores[0] = E_VAL_NOT_FOUND;
    scores[1] = SCORE_NOT_FOUND;  // raw score
    scores[2] = SCORE_NOT_FOUND;  // bit score
    scores[3] = SCORE_NOT_FOUND;  // number identical

    if (!seqAlign) {
        return;
    }

    if (seqAlign->IsSetScore()) {
        // score is at the top level of the seqAlign
        count = ExtractScoreFromScoreList(seqAlign->GetScore(), flags, scores);
    } else {
        // check individual dense-diags for score (this happens when
        // SeqAlignConvertDspToDdpList(...) from C-toolkit is called;
        // all dense_diags are set w/ the same score ... see make_seg_score())
        if (seqAlign->GetSegs().IsDendiag()) {
            const TDendiag ddList = seqAlign->GetSegs().GetDendiag();
            if (ddList.size() > 0) {
                ddit = ddList.begin();
                while (ddit != ddList.end() && count == 0) {
                    if ((*ddit)->IsSetScores()) {
                        count = ExtractScoreFromScoreList((*ddit)->GetScores(), flags, scores);
                    }
                    ++ddit;
                }
            }
        //  Convert Dense_seg to Dense_diag and check for scores.
        //  Couldn't combine as ddList must be non-const here and const for
        //  Dense_diag case above.
        } else if (seqAlign->GetSegs().IsDenseg()) {
            TDendiag ddList;
            Denseg2DenseDiagList(seqAlign->GetSegs().GetDenseg(), ddList);
            if (ddList.size() > 0) {
                ddit = ddList.begin();
                while (ddit != ddList.end() && count == 0) {
                    if ((*ddit)->IsSetScores()) {
                        count = ExtractScoreFromScoreList((*ddit)->GetScores(), flags, scores);
                    }
                    ++ddit;
                }
            }
        }

    }
}


int ExtractScoreFromScoreList(const CSeq_align::TScore& scores, int flags, vector<double>& values) {

	int count = 0;


	CSeq_align::TScore::const_iterator score_ci, score_ci_end = scores.end();
    for (score_ci=scores.begin(); score_ci!=score_ci_end; score_ci++) {
        if ((*score_ci)->IsSetId() && (*score_ci)->GetId().IsStr()) {			
            if ((flags&E_VALUE) && (*score_ci)->GetValue().IsReal() && (*score_ci)->GetId().GetStr() == "e_value") {
                values[0] = (*score_ci)->GetValue().GetReal();
                count++;
            }
            if ((flags&RAW_SCORE) && (*score_ci)->GetValue().IsInt() && (*score_ci)->GetId().GetStr() == "score") {
                values[1] = (*score_ci)->GetValue().GetInt();
                count++;
            }
            if ((flags&BIT_SCORE) && (*score_ci)->GetValue().IsReal() && (*score_ci)->GetId().GetStr() == "bit_score") {
                values[2] = (*score_ci)->GetValue().GetReal();
                count++;
            }
            if ((flags&N_IDENTICAL) && (*score_ci)->GetValue().IsInt() && (*score_ci)->GetId().GetStr() == "num_ident") {
                values[3] = (*score_ci)->GetValue().GetInt();
                count++;
            }
        }
    }
	return count;
}

//===========================================
//  Functions to manipulate Dense_segs
//===========================================

CRef<CSeq_align> Denseg2DenseDiagList(const CRef<CSeq_align>& denseSegSeqAlign)
{
    CRef<CSeq_align> newSa(new CSeq_align);
    newSa->Assign(*denseSegSeqAlign);

    if (denseSegSeqAlign.NotEmpty() && denseSegSeqAlign->GetSegs().IsDenseg()) {
        TDendiag ddList;
        Denseg2DenseDiagList(denseSegSeqAlign->GetSegs().GetDenseg(), ddList);
        newSa->SetSegs().SetDendiag() = ddList;
    }

    return newSa;
}

//  Function written by:  Kamen Todorov, NCBI
//  Part of the objtools/alnmgr project; forked to here to avoid
//  adding extra library dependencies.

void Denseg2DenseDiagList(const CDense_seg& ds, TDendiag& ddl)
{
    const CDense_seg::TIds&     ids     = ds.GetIds();
    const CDense_seg::TStarts&  starts  = ds.GetStarts();
    const CDense_seg::TStrands& strands = ds.GetStrands();
    const CDense_seg::TLens&    lens    = ds.GetLens();
    const CDense_seg::TScores&  scores  = ds.GetScores();
    const CDense_seg::TNumseg&  numsegs = ds.GetNumseg();
    const CDense_seg::TDim&     numrows = ds.GetDim();
    int                         total   = numrows * numsegs;
    int                         pos     = 0;

    int                         rows_per_seg;

    bool strands_exist = ((int) strands.size() == total);
    bool scores_exist = ((int) scores.size() == total);
    
    for (CDense_seg::TNumseg seg = 0; seg < numsegs; seg++) {
        rows_per_seg = 0;
        CRef<CDense_diag> dd (new CDense_diag);
        dd->SetLen(lens[seg]);
        for (CDense_seg::TDim row = 0; row < numrows; row++) {
            const TSignedSeqPos& start = starts[pos];
            if (start >=0) {
                rows_per_seg++;
                dd->SetIds().push_back(ids[row]);
                dd->SetStarts().push_back(start);
                if (strands_exist) {
                    dd->SetStrands().push_back(strands[pos]);
                }
                if (scores_exist) {
                    dd->SetScores().push_back(scores[pos]);
                }
            }
            pos++;
        }
        if (rows_per_seg >= 2) {
            dd->SetDim(rows_per_seg);
            ddl.push_back(dd);
        }
    }
}
// simple and easy : added by Vahan to avoid usage of bunch of algAlignment...  classes
bool GetPendingSeqId(CCdCore * pCD,int irow,CRef <CSeq_id> & seqID)
{
    int i ;
    list <CRef <CUpdate_align> > ::iterator pPen;
	for(i=0,pPen=pCD->SetPending().begin();pPen!=pCD->SetPending().end();pPen++,i++){
        if(i<irow)
            continue;
		CSeq_align * pAl = *((*pPen)->SetSeqannot().SetData().SetAlign().begin());
		CDense_diag * pDDPen=*(pAl->SetSegs().SetDendiag().begin());
		vector < CRef< CSeq_id > >::const_iterator pid=pDDPen->GetIds().begin();
		seqID=*(++pid);
        return true;
	}
    return false;
}

bool GetPendingFootPrint(CCdCore * pCD,int irow,int * from, int * to)
{
    int i ;
    list <CRef <CUpdate_align> > ::iterator pPen;
	TDendiag_cit pD ;
	CDense_diag::TStarts::const_iterator pid;
	CRef<CDense_diag > pDDPen;
	CSeq_align * pAl ;

	for(i=0,pPen=pCD->SetPending().begin();pPen!=pCD->SetPending().end();pPen++,i++){
        if(i<irow)
            continue;
		pAl = *((*pPen)->SetSeqannot().SetData().SetAlign().begin());
		pD= pAl->SetSegs().SetDendiag().begin();
		pDDPen=*(pD);
		pid=pDDPen->GetStarts().begin();
		*from=*(++pid);

		pD= pAl->SetSegs().SetDendiag().end();pD--;
		pDDPen=*(pD);
		pid=pDDPen->GetStarts().begin();
		(*to)=*(++pid);
		(*to)+=pDDPen->GetLen()-1;
        return true;
	}
    return false;
}
bool GetPendingDD(CCdCore * pCD,int irow,TDendiag* & pDenDiagSet)
{
    int i ;
    list <CRef <CUpdate_align> > ::iterator pPen;
	for(i=0,pPen=pCD->SetPending().begin();pPen!=pCD->SetPending().end();pPen++,i++){
        if(i<irow)
            continue;
		CSeq_align * pAl = *((*pPen)->SetSeqannot().SetData().SetAlign().begin());
		pDenDiagSet=&(pAl->SetSegs().SetDendiag());
		
		
        return true;
	}
    return false;
}

//  Assumes that the Seq_align passed is a pairwise (dim = 2) alignment of
//  a sequence to a pssm, where the pssm is the second Id.  Such alignments
//  are obtained via RPSBlast and provided by the CDart API.
int GetPssmIdFromSeqAlign(const CRef<CSeq_align >& seqAlign, string& err) {

    int pssmId = 0;

    err.erase();
    if (seqAlign.Empty()) {
        err = "GetPssmIdFromSeqAlign:  Empty Seq_align.\n";
    } else if (seqAlign->IsSetDim() && seqAlign->GetDim() != 2) {
        err = "GetPssmIdFromSeqAlign:  Only Seq_aligns with dim = 2 supported.\n";
    } else if (seqAlign->GetSegs().IsDenseg()) {
        const CRef< CSeq_id >& pssmSeqId = seqAlign->GetSegs().GetDenseg().GetIds().back();
        pssmId = GetCDDPssmIdFromSeqId(pssmSeqId);
    } else if (seqAlign->GetSegs().IsDendiag()) {
        err = "GetPssmIdFromSeqAlign:  Dense_diags not currently supported.\n";
    } else {
        err.append("GetPssmIdFromSeqAlign:  Seq_align is an unsupported type (%d).\n", seqAlign->GetType());
    }
    return pssmId;    
}

//  Return the GI of the master sequence of the Seq_align.  If not a GI, 
//  or for other error, return 0.
int GetMasterGIFromSeqAlign(const CRef< CSeq_align >& seqAlign, string& err) {

    int gi = 0;

    err.erase();
    if (seqAlign.Empty()) {
        err = "GetMasterGIFromSeqAlign:  Empty Seq_align.\n";
    } else if (seqAlign->GetSegs().IsDenseg()) {
        const CRef< CSeq_id >& seqId = seqAlign->GetSegs().GetDenseg().GetIds().front();
        if (seqId.NotEmpty() && seqId->IsGi()) {
            gi = seqId->GetGi();
        } else {
            err = "GetMasterGIFromSeqAlign:  Dense_seg's master sequence is empty or not of type 'GI'.\n";
        }
    } else if (seqAlign->GetSegs().IsDendiag()) {
        const CRef< CSeq_id >& seqId = seqAlign->GetSegs().GetDendiag().front()->GetIds().front();
        if (seqId.NotEmpty() && seqId->IsGi()) {
            gi = seqId->GetGi();
        } else {
            err = "GetMasterGIFromSeqAlign:  Dense_diag's master sequence is empty or not of type 'GI'.\n";
        }
    } else {
        err.append("GetMasterGIFromSeqAlign:  Seq_align is an unsupported type (%d).\n", seqAlign->GetType());
    }
    return gi;    
}


END_SCOPE(cd_utils) // namespace ncbi::objects::
END_NCBI_SCOPE
