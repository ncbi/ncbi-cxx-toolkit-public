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

typedef CSeq_align::C_Segs::TDendiag TDendiag;
typedef TDendiag::iterator TDendiag_it;
typedef TDendiag::const_iterator TDendiag_cit;

//   Assumes a typical CD-style seq_align with one master & one slave row.
NCBI_CDUTILS_EXPORT 
bool GetSeqID(const CRef< CSeq_align >& seqAlign, CRef< CSeq_id >& SeqID, bool getSlave=true);

//   Replace the indicated seq-id in the CSeq_align with newSeqId.
NCBI_CDUTILS_EXPORT 
bool ChangeSeqIdInSeqAlign(CRef< CSeq_align>& sa, const CRef< CSeq_id >& newSeqId, bool onMaster);

//  coordinate mapping functions; return INVALID_POSITION on failure
NCBI_CDUTILS_EXPORT 
int  MapPositionToMaster(int childPos, const CSeq_align&  align);
NCBI_CDUTILS_EXPORT 
int  MapPositionToChild(int masterPos, const CSeq_align&  align);
NCBI_CDUTILS_EXPORT 
int  MapPosition(const CSeq_align& seqAlign, int Position, CoordMapDir mapDir);  // from CCd::GetSeqPosition

NCBI_CDUTILS_EXPORT 
bool IsPositionAligned(const CSeq_align& seqAlign, int Position, bool onMaster);
NCBI_CDUTILS_EXPORT 
bool IsPositionAligned(const TDendiag*& dd,        int Position, bool onMaster);
NCBI_CDUTILS_EXPORT 
int  GetAlignedPositions(const CRef< CSeq_align >& align1, const CRef< CSeq_align >& align2, vector<int>& alignedPositions, bool onMaster);
NCBI_CDUTILS_EXPORT 
int  GetNumAlignedResidues(const CRef< CSeq_align >& align);  //  see alse ddLen; main part of CCd::GetAlignmentLength
NCBI_CDUTILS_EXPORT 
int  GetLowerBound(const CRef< CSeq_align >& seqAlign, bool onMaster);
NCBI_CDUTILS_EXPORT 
int  GetUpperBound(const CRef< CSeq_align >& seqAlign, bool onMaster);

//  returns a null pointer on failure
NCBI_CDUTILS_EXPORT 
void SetAlignedResiduesOnSequence(const CRef< CSeq_align >& align, const string& sequenceString, char*& pAlignedRes, bool isMaster = false);

NCBI_CDUTILS_EXPORT 
bool CheckSeqIdInDD(const CRef< CSeq_align >& seqAlign);
//  Query alignment block structure (assumes dense_diags in the seq_align)
NCBI_CDUTILS_EXPORT 
int  GetBlockNumberForResidue (int residue, const CRef< CSeq_align >& seqAlign, bool onMaster, 
                               vector<int>* starts = NULL, vector<int>* lengths = NULL);  // -1 if not aligned
NCBI_CDUTILS_EXPORT 
int  GetBlockCount  (const CRef< CSeq_align >& seqAlign);
NCBI_CDUTILS_EXPORT 
int  GetBlockLengths(const CRef< CSeq_align >& seqAlign, vector<int>& lengths);
NCBI_CDUTILS_EXPORT 
int  GetBlockStarts (const CRef< CSeq_align >& seqAlign, vector<int>& starts, bool onMaster);
NCBI_CDUTILS_EXPORT 
int  GetBlockStartsForMaster(const CRef< CSeq_align >& seqAlign, vector<int>& starts);

//  Get DD from Seq_align 
NCBI_CDUTILS_EXPORT 
bool GetDDSetFromSeqAlign(const CSeq_align& align, const TDendiag*& dd);
NCBI_CDUTILS_EXPORT 
bool GetDDSetFromSeqAlign(CSeq_align& align, TDendiag*& dd);

//  Convert between DD and a SeqLoc (which will be contain a SeqInterval); from cdt_manipcd
NCBI_CDUTILS_EXPORT 
void MakeDDFromSeqLoc(CSeq_loc * pAl,TDendiag * pDD );
NCBI_CDUTILS_EXPORT 
void MakeSeqLocFromDD(const TDendiag * pDD, CSeq_loc * pAl);
NCBI_CDUTILS_EXPORT 
void AddIntervalToDD(TDendiag * pDD,CRef<CSeq_id> seqID1, CRef<CSeq_id> seqID2,TSeqPos st1,TSeqPos st2, TSeqPos lll);

//  GetFirstOrLastDenDiag was formerly CCd::GetDenDiag(int Row, bool First, CRef< CDense_diag >& DenDiag)
//  firstOrLast==true --> first den diag, otherwise last den diag
NCBI_CDUTILS_EXPORT 
bool GetFirstOrLastDenDiag(const CRef< CSeq_align >& seqAlign, bool firstOrLast, CRef<CDense_diag>& dd);
NCBI_CDUTILS_EXPORT 
bool GetDenDiagSet(const CRef< CSeq_annot >& seqAnnot, int row, const TDendiag*& pDenDiagSet);  // get dense-diag info for one row
NCBI_CDUTILS_EXPORT 
bool SetDenDiagSet(CRef< CSeq_annot >& seqAnnot, int row, TDendiag*& pddSet);

NCBI_CDUTILS_EXPORT 
bool EraseRow(CRef< CSeq_annot >& seqAnnot, int row);

//  Returns 'seqAlign' unless the input is wrapping a CSeq_align_set 
//  (has segs of type 'disc'), in which case the first seq-align found
//  will be returned.  Returns an empty CRef on failure.  
//  Note:  this is a recursive function.
//  Was 'extractOneSeqAlign' from cuBlast2Seq and cuSimpleB2SWrapper.
NCBI_CDUTILS_EXPORT
CRef< CSeq_align > ExtractFirstSeqAlign(CRef< CSeq_align > seqAlign);

//  Functions that manipulate or assume Dense_segs

//  Given a CSeq_align with a denseg alignment, return a new CSeq_align with an
//  equivalent dense-diag list.  If the input doesn't have a denseg, or if the
//  conversion fails, the returned CRef will be a copy of the input CRef.
NCBI_CDUTILS_EXPORT 
CRef<CSeq_align> Denseg2DenseDiagList(const CRef<CSeq_align>& denseSegSeqAlign);

//  Get DD list from a Dense_seg.
//  Function written by:  Kamen Todorov, NCBI
//  Part of the objtools/alnmgr project forked to avoid
//  adding extra library dependencies.
NCBI_CDUTILS_EXPORT 
void Denseg2DenseDiagList(const CDense_seg& ds, TDendiag& ddl);

//  Assumes that the Seq_align passed is a pairwise (dim = 2) Dense_seg alignment 
//  of a sequence to a pssm, where the pssm is the second Id.  Such alignments
//  are obtained via RPSBlast and provided by the CDart API, e.g.
//  Return 0 on failure.
NCBI_CDUTILS_EXPORT 
int  GetPssmIdFromSeqAlign(const CRef<CSeq_align >& seqAlign, string& err);

//  Return the GI of the master (i.e. first) sequence of the Seq_align.  If not a GI, 
//  or for other error, return 0.
NCBI_CDUTILS_EXPORT 
TGi GetMasterGIFromSeqAlign(const CRef< CSeq_align >& seqAlign, string& err);

//class CCd;
NCBI_CDUTILS_EXPORT 
bool GetPendingSeqId(CCdCore * pCD,int irow,CRef <CSeq_id> & seqID);

END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // ALGALIGN_HPP
