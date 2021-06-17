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
TGi GetMasterGIFromSeqAlign(const CRef< CSeq_align >& seqAlign, string& err) {

    TGi gi = ZERO_GI;

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
