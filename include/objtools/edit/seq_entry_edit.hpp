#ifndef OBJTOOLS_EDIT___SEQ_ENTRY_EDIT__HPP
#define OBJTOOLS_EDIT___SEQ_ENTRY_EDIT__HPP

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
* Author: Mati Shomrat, NCBI
*
* File Description:
*   High level Seq-entry edit, for meaningful combination of Seq-entries.
*/
#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Linkage_evidence.hpp>
#include <objects/submit/Seq_submit.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry_Handle;
class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_annot_Handle;


BEGIN_SCOPE(edit)


/// Attach one Seq-entry to another
///
/// @param to
///   Seq-entry to change
/// @param add
///   Seq-entry to add
/// @sa
///   Other forms of adding the content of one Seq-entry to another.
NCBI_XOBJEDIT_EXPORT
void AddSeqEntryToSeqEntry(const CSeq_entry_Handle& to, const CSeq_entry_Handle& add);

/// Attach one Bioseq to another
///
/// This function will add one Bioseq to another if:
///   1. 'to' is a nucleotide and 'add' is a protein. The result
///      is a nuc-prot set contating both elements.
///   2. both Bioseqs have the same molecular type. The result
///      is a segmented bioseq of which the two bioseqs serve as parts.
///   3. 'to' is a segmented bioseq and 'add' has the same molecular type
///      as 'to'. 'add' will be added as a new part of 'to'.
/// @param to
///   Bioseq to change
/// @param add
///   Bioseq to add
/// @sa
///   AddSeqEntryToSeqEntry()
NCBI_XOBJEDIT_EXPORT
void AddBioseqToBioseq(const CBioseq_Handle& to, const CBioseq_Handle& add);

/// Add a Bioseq to a Bioseq-set
///
/// This function will add the Bioseq to the set if:
///   1. The set is of class 'parts' and the Bioseq has the same
///      molecular type as the other parts.
///   2. 
/// @param bsst
///   Bioseq to change
/// @param seq
///   Bioseq to add
/// @sa
///   AddSeqEntryToSeqEntry()
NCBI_XOBJEDIT_EXPORT
void AddBioseqToBioseqSet(const CBioseq_set_Handle& bsst, const CBioseq_Handle& seq);


/// Create a Seq-entry from a Seq-submit
/// @param submit
///   Seq-submit to create Seq-entry from
NCBI_XOBJEDIT_EXPORT
CRef<CSeq_entry> SeqEntryFromSeqSubmit(const CSeq_submit& submit);

/// Split a Seq-entry, where the second part holds the given bioseqs.
/// There are various complex rules here that may not be obvious at first glance.
/// @param target
///   The Seq-entry to split
/// @param bioseq_handles
///   The array of bioseqs that should end up in the second part of the target, with the rest in the first part.
NCBI_XOBJEDIT_EXPORT
void SegregateSetsByBioseqList(const CSeq_entry_Handle & target, 
    const CScope::TBioseqHandles & bioseq_handles );    

typedef vector<CSeq_entry_Handle> TVecOfSeqEntryHandles;

/// Call this if the alignments directly under these seq-entries are 
/// all jumbled up between each other.
/// It will move each Seq-align into the proper location.
/// In particular, it looks at all the seq-ids in each seq-align.  If
/// none of them belong to any member of vecOfSeqEntryHandles, then
/// that Seq-align is copied to all members of vecOfSeqEntryHandles.
/// If it belongs to only one member of vecOfSeqEntryHandles, then it
/// goes there.  If the align belongs to more than one, it's destroyed.
///
/// @param vecOfSeqEntryHandles
///   The Seq-entries we're considering for alignments.
NCBI_XOBJEDIT_EXPORT
void DivvyUpAlignments(const TVecOfSeqEntryHandles & vecOfSeqEntryHandles);

/// Moves descriptors down to children of the given bioseq-set.  Each child
/// gets a copy of all the descriptors.  It does NOT check for 
/// duplicate Seqdescs.
///
/// @param bioseq_set_h
///   This is the bioseq_set whose descriptors we're moving.
/// @param choices_to_delete
///   If non-empty, it indicates the types of CSeqdescs to delete instead
///   of propagating.
NCBI_XOBJEDIT_EXPORT
void BioseqSetDescriptorPropagateDown(
    const CBioseq_set_Handle & bioseq_set_h,
    const vector<CSeqdesc::E_Choice> &choices_to_delete = 
        vector<CSeqdesc::E_Choice>() );

/// Creates a User-object descriptor on every sequence that has a local ID
/// Contains the original local ID
NCBI_XOBJEDIT_EXPORT
void AddLocalIdUserObjects(CSeq_entry& entry);

/// Detects whether colliding IDs were fixed by comparing sequence IDs to
/// the contents of the OriginalID User-object descriptor
NCBI_XOBJEDIT_EXPORT
bool HasRepairedIDs(const CSeq_entry& entry);

NCBI_XOBJEDIT_EXPORT
void ConvertRawToDeltaByNs(CSeq_inst& inst, 
                           size_t min_unknown, int max_unknown, 
                           size_t min_known, int max_known,
                           bool is_assembly_gap = false, int gap_type = CSeq_gap::eType_unknown, int linkage = -1, int linkage_evidence = -1 );

NCBI_XOBJEDIT_EXPORT
void ConvertRawToDeltaByNs(CBioseq_Handle bsh, 
                           size_t min_unknown, int max_unknown, 
                           size_t min_known, int max_known,
                           bool is_assembly_gap = false, int gap_type = CSeq_gap::eType_unknown, int linkage = -1, int linkage_evidence = -1 );

typedef pair<size_t, int> TLocAdjustment;
typedef vector<TLocAdjustment> TLocAdjustmentVector;

NCBI_XOBJEDIT_EXPORT
TLocAdjustmentVector NormalizeUnknownLengthGaps(CSeq_inst& inst, size_t unknown_length = 100);

NCBI_XOBJEDIT_EXPORT
void SetLinkageType(CSeq_ext& ext, CSeq_gap::TType linkage_type);

NCBI_XOBJEDIT_EXPORT
void SetLinkageTypeScaffold(CSeq_ext& ext, CLinkage_evidence::TType evidence_type);

NCBI_XOBJEDIT_EXPORT
void SetLinkageTypeLinkedRepeat(CSeq_ext& ext, CLinkage_evidence::TType evidence_type);

NCBI_XOBJEDIT_EXPORT
void AddLinkageEvidence(CSeq_ext& ext, CLinkage_evidence::TType evidence_type);

NCBI_XOBJEDIT_EXPORT
void ResetLinkageEvidence(CSeq_ext& ext, CLinkage_evidence::TType evidence_type);

NCBI_XOBJEDIT_EXPORT
void SortSeqDescr(CSeq_entry& entry);

NCBI_XOBJEDIT_EXPORT
void SortSeqDescr(CSeq_descr& entry);


/*******************************************************************************
**** HIGH-LEVEL API
****
**** Trim functions
*******************************************************************************/

/// A list of trim coordinates 
typedef CRange<TSeqPos> TRange;
typedef vector<TRange> TCuts;

/// Any internal cut listed in TCuts will be converted to a terminal cut 
/// using one of these options.  The default is eTrimToClosestEnd.
enum EInternalTrimType {
    eTrimToClosestEnd = 0,  // default
    eTrimTo5PrimeEnd,
    eTrimTo3PrimeEnd
};

/// Trim sequence data and all associated annotation
NCBI_XOBJEDIT_EXPORT
void TrimSequenceAndAnnotation(CBioseq_Handle bsh, 
                               const TCuts& cuts,
                               EInternalTrimType internal_cut_conversion = eTrimToClosestEnd);


/*******************************************************************************
**** LOW-LEVEL API
****
**** Trim functions divided up into trimming separate distinct objects, i.e.,
**** the sequence data itself and all associated annotation.
****
**** Used by callers who need access to each edited object so that they can 
**** pass these edited objects to a command undo/redo framework, for example.
*******************************************************************************/

/// 1) Merge abutting and overlapping cuts.
/// 2) Adjust any internal cuts to terminal cuts according to option.
/// 3) Sort the cuts from greatest to least so that sequence
///    data and annotation will be deleted from greatest loc to smallest loc.
///    That way we don't have to adjust coordinate values after 
///    each cut.
NCBI_XOBJEDIT_EXPORT
void GetSortedCuts(CBioseq_Handle bsh, 
                   const TCuts& cuts,
                   TCuts& sorted_cuts,
                   EInternalTrimType internal_cut_conversion = eTrimToClosestEnd);

/// Trim sequence data
NCBI_XOBJEDIT_EXPORT
void TrimSeqData(CBioseq_Handle bsh, 
                 CRef<CSeq_inst> inst, 
                 const TCuts& sorted_cuts);

/// Trim Seq-graph annotation 
NCBI_XOBJEDIT_EXPORT
void TrimSeqGraph(CBioseq_Handle bsh, 
                  CRef<CSeq_graph> graph, 
                  const TCuts& sorted_cuts);

/// Trim Seq-align annotation
NCBI_XOBJEDIT_EXPORT
void TrimSeqAlign(CBioseq_Handle bsh, 
                  CRef<CSeq_align> align, 
                  const TCuts& sorted_cuts);

/// Trim Seq-feat annotation
NCBI_XOBJEDIT_EXPORT
void TrimSeqFeat(CRef<CSeq_feat> feat, 
                 const TCuts& sorted_cuts,
                 bool& bFeatureDeleted, 
                 bool& bFeatureTrimmed);

/// Secondary function needed after trimming Seq-feat.
/// If the trim completely covers the feature (boolean reference bFeatureDeleted
/// from TrimSeqFeat() returns true), then delete protein sequence and 
/// re-normalize nuc-prot set.
NCBI_XOBJEDIT_EXPORT
void DeleteProteinAndRenormalizeNucProtSet(const CSeq_feat_Handle& feat_h);

/// Secondary function needed after trimming Seq-feat.
/// If TrimSeqFeat()'s bFeatureTrimmed returns true, then adjust cdregion frame.
NCBI_XOBJEDIT_EXPORT
void AdjustCdregionFrame(TSeqPos original_nuc_len, 
                         CRef<CSeq_feat> cds,
                         const TCuts& sorted_cuts);

/// Secondary function needed after trimming Seq-feat.
/// If TrimSeqFeat()'s bFeatureTrimmed returns true, then make new protein
/// sequence.
NCBI_XOBJEDIT_EXPORT
CRef<CBioseq> SetNewProteinSequence(CScope& new_scope,
                                    CRef<CSeq_feat> cds,
                                    CRef<CSeq_inst> new_inst);

/// Secondary function needed after trimming Seq-feat.
/// If TrimSeqFeat()'s bFeatureTrimmed returns true, then retranslate cdregion.
NCBI_XOBJEDIT_EXPORT
void RetranslateCdregion(CBioseq_Handle nuc_bsh, 
                         CRef<CSeq_inst> trimmed_nuc_inst,
                         CRef<CSeq_feat> cds,
                         const TCuts& sorted_cuts);

/*******************************************************************************
**** LOW-LEVEL API
****
**** Trim functions 
*******************************************************************************/


// For Unverified descriptors
NCBI_XOBJEDIT_EXPORT CRef<CSeqdesc> FindUnverified(const CBioseq& seq);
NCBI_XOBJEDIT_EXPORT bool IsUnverifiedOrganism(const CBioseq& seq);
NCBI_XOBJEDIT_EXPORT bool IsUnverifiedFeature(const CBioseq& seq);

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_EDIT___SEQ_ENTRY_EDIT__HPP */
