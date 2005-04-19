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

#ifndef CU_ALGALIGN_HPP
#define CU_ALGALIGN_HPP


// include ncbistd.hpp, ncbiobj.hpp, ncbi_limits.h, various stl containers
#include <corelib/ncbiargs.hpp>   
#include <algo/structure/cd_utils/cuGlobalDefs.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils) 

enum {
	DD_NOFLAG=0,
	DD_FOLLOWGUIDE=0x01,// use block structure of guide for remapping
	DD_FOLLOWSEQ=0x02 // use block structure of sequence for remapping
};


//   Both of these assume a typical CD-style seq_align with one master & one slave row.
bool GetSeqID(const CRef< CSeq_align >& seqAlign, CRef< CSeq_id >& SeqID, bool getSlave=true);
bool HasSeqID(const CRef< CSeq_align >& seqAlign, const CRef< CSeq_id >& SeqID, bool& isMaster);

//   A wrapper for ddRemap using CSeq_aligns vs. dense diags.
//   Using an intermediate sequence present in both 'source' and 'guide' to generate a remapped 'mappedAlign'
//   iSeq:    is the non-intermediate sequence index in 'source' 
//   iMaster: is the index of non-intermediate sequence in the 'guide'
//
//   Adds intervals to 'mappedAlign' where the master and child sequences are choseable:
//   iMasterNew, iSeqNew:  0 corresponds to intermediate sequence, 
//                         the 'guide'  master is 1,
//                         the 'source' slave sequence is 2
//
//   Flags:   this defines how to create blocks (if for example 'guide' has one continous block where 'source' has two glued DDs). 
//   	DD_NOFLAG     =0
//      DD_FOLLOWGUIDE=0x01:   use block structure of guide for remapping
//      DD_FOLLOWSEQ  =0x02:   use block structure of sequence for remapping
int  SeqAlignRemap(CRef< CSeq_align >& source, int iSeq, CRef< CSeq_align >& guide, int iMaster, CRef< CSeq_align >& mappedAlign, int iMasterNew, int iSeqNew, int flags, string& err);

//   Apply the mask to the original pairwise alignment, creating a new CSeq_align from all
//   of the overlapping residues.  Use the master of the mask by default (useMaskMaster = true).
//   If invertMask = true, the 'maskedAlign' contains all of those aligned residues *NOT*
//   also aligned in the mask.  If the specified seqId is not present in originalAlign, 
//   an empty cref is returned.
void MakeMaskedSeqAlign(const CRef< CSeq_align >& originalAlign, const CRef< CSeq_align >& maskAlign, CRef< CSeq_align >& maskedAlign, bool useMaskMaster = true, bool invertMask = false);

//   Look at sequence ids, block lengths, and starts for master & slave.  True if
//   everything is the same if checkMasters=true; if checkMaster=false, skip check
//   that the masters of the two alignments are the same (as per ddAreEquivalent).
bool SeqAlignsAreEquivalent(const CRef< CSeq_align >& align1, const CRef< CSeq_align >& align2, bool checkMasters);

//   Create a CSeq_align from seqAlign, swapping the master/slave positions.
void SeqAlignSwapMasterSlave(CRef< CSeq_align >& seqAlign, CRef< CSeq_align >& swappedSeqAlign);

//  coordinate mapping functions; return INVALID_POSITION on failure
int  MapPositionToMaster(int childPos, const CSeq_align&  align);
int  MapPositionToChild(int masterPos, const CSeq_align&  align);
int  MapPosition(const CSeq_align& seqAlign, int Position, CoordMapDir mapDir);  // from CCd::GetSeqPosition

bool IsPositionAligned(const CSeq_align& seqAlign, int Position, bool onMaster);
bool IsPositionAligned(const TDendiag*& dd,        int Position, bool onMaster);
int  GetAlignedPositions(const CRef< CSeq_align >& align1, const CRef< CSeq_align >& align2, vector<int>& alignedPositions, bool onMaster);
int  GetNumAlignedResidues(const CRef< CSeq_align >& align);  //  see alse ddLen; main part of CCd::GetAlignmentLength
int  GetLowerBound(const CRef< CSeq_align >& seqAlign, bool onMaster);
int  GetUpperBound(const CRef< CSeq_align >& seqAlign, bool onMaster);

//  returns a null pointer on failure
void SetAlignedResiduesOnSequence(const CRef< CSeq_align >& align, const string& sequenceString, char*& pAlignedRes, bool isMaster = false);

bool CheckSeqIdInDD(const CRef< CSeq_align >& seqAlign);
//  Query alignment block structure (assumes dense_diags in the seq_align)
int  GetBlockNumberForResidue (int residue, const CRef< CSeq_align >& seqAlign, bool onMaster, 
                               vector<int>* starts = NULL, vector<int>* lengths = NULL);  // -1 if not aligned
int  GetBlockCount  (const CRef< CSeq_align >& seqAlign);
int  GetBlockLengths(const CRef< CSeq_align >& seqAlign, vector<int>& lengths);
int  GetBlockStarts (const CRef< CSeq_align >& seqAlign, vector<int>& starts, bool onMaster);
int  GetBlockStartsForMaster(const CRef< CSeq_align >& seqAlign, vector<int>& starts);

//  Get DD from Seq_align 
bool GetDDSetFromSeqAlign(const CSeq_align& align, const TDendiag*& dd);
bool GetDDSetFromSeqAlign(CSeq_align& align, TDendiag*& dd);

//  Convert between DD and a SeqLoc (which will be contain a SeqInterval); from cdt_manipcd
void MakeDDFromSeqLoc(CSeq_loc * pAl,TDendiag * pDD );
void MakeSeqLocFromDD(const TDendiag * pDD, CSeq_loc * pAl);
void AddIntervalToDD(TDendiag * pDD,CRef<CSeq_id> seqID1, CRef<CSeq_id> seqID2,TSeqPos st1,TSeqPos st2, TSeqPos lll);

//  GetFirstOrLastDenDiag was formerly CCd::GetDenDiag(int Row, bool First, CRef< CDense_diag >& DenDiag)
//  firstOrLast==true --> first den diag, otherwise last den diag
bool GetFirstOrLastDenDiag(const CRef< CSeq_align >& seqAlign, bool firstOrLast, CRef<CDense_diag>& dd);
bool GetDenDiagSet(const CRef< CSeq_annot >& seqAnnot, int row, const TDendiag*& pDenDiagSet);  // get dense-diag info for one row
bool SetDenDiagSet(CRef< CSeq_annot >& seqAnnot, int row, TDendiag*& pddSet);

//  Go through a dense_diags list and identify all adjacent blocks,
//  namely those with no unaligned residues between them.  In the third
//  argument 'adj', fill the dense list with only non-adjacent dense_diags,
//  merging any adjacent ones found in the original list.  Pass in iterators so
//  this can be used for any consecutive set of blocks.  
//  Moved from the validator code.
void BuildAdjacentDiags(const TDendiag_cit& begin_orig, const TDendiag_cit& end_orig, TDendiag* adj);

bool EraseRow(CRef< CSeq_annot >& seqAnnot, int row);


//  Functions moved from algDD  

int    ddLen(TDendiag * pDD);  
string ddAlignInfo(TDendiag * pGuideDD);
int ddRecompose(TDendiag * pGuideDD,int iMaster, int iSeq,TDendiag * pResultDD);
//  Doesn't check the sequence ID of the master sequences by default.
bool   ddAreEquivalent(const TDendiag * pDD1,const TDendiag * pDD2, bool checkMasters=false);
//int    ddFindBySeqId(CCd * pCD,CRef<CSeq_id>& SeqID,TDendiag * & ResultDD,TDendiag * pNeedOverlapDD, int isSelf,int istart);

//  See description of arguments above in SeqAlignRemap.
int    ddRemap(TDendiag * pSrcDD,int iSeq,TDendiag * pGuideDD, int iMaster,TDendiag * newDDlist,int iMasterNew, int iSeqNew,int flags,string err);
string ddDifferenceResidues(TDendiag * pSrcDD,TDendiag * pGuideDD,TDendiag * newDDlist);
int ddRenameSeqID(TDendiag * pGuideDD,int iNum, CRef< CSeq_id > & seqID);

// functions to sscanf/sprintf alignment info
bool sscanSeqId (const char * & ptr,CSeq_id & seqid);
const char * sscanSeqLocIntervals(const char * ptr, CSeq_loc & sq);

//  Query a SeqAlign for e-Values, raw scores, bit scores and # identical residues.
//  Logical OR flags to get multiple score types.
//  Invalid value left in corresponding vector element if not present or flag not set.
void ExtractScoreFromSeqAlign(const CRef< CSeq_align >& seqAlign, int flags, vector<double>& values);
void ExtractScoreFromSeqAlign(const CSeq_align* seqAlign, int flags, vector<double>& values);
//  Set scores to E_VAL_NOT_FOUND, SCORE_NOT_FOUND if errors or score type wasn't found.
//  Return value is the number of scores requested in flags (expect to be in (0,4]).
int  ExtractScoreFromScoreList(const CSeq_align::TScore& scores, int flags, vector<double>& values);


//  Functions that manipulate or assume Dense_segs

//  Get DD list from a Dense_seg.
//  Function written by:  Kamen Todorov, NCBI
//  Part of the objtools/alnmgr project forked to avoid
//  adding extra library dependencies.
void Denseg2DenseDiagList(const CDense_seg& ds, TDendiag& ddl);

//  Assumes that the Seq_align passed is a pairwise (dim = 2) Dense_seg alignment 
//  of a sequence to a pssm, where the pssm is the second Id.  Such alignments
//  are obtained via RPSBlast and provided by the CDart API, e.g.
//  Return 0 on failure.
int  GetPssmIdFromSeqAlign(const CRef<CSeq_align >& seqAlign, string& err);

//  Return the GI of the master (i.e. first) sequence of the Seq_align.  If not a GI, 
//  or for other error, return 0.
int GetMasterGIFromSeqAlign(const CRef< CSeq_align >& seqAlign, string& err);

//class CCd;
bool GetPendingSeqId(CCdCore * pCD,int irow,CRef <CSeq_id> & seqID);
bool GetPendingDD(CCdCore * pCD,int irow,TDendiag* & pDenDiagSet);
bool GetPendingFootPrint(CCdCore * pCD,int irow,int * from, int * to);

END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // ALGALIGN_HPP

/* 
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:00  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
