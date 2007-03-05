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
* Authors:  Paul Thiessen
*
* File Description:
*       alignment utility algorithms
*
* ===========================================================================
*/

#ifndef STRUCT_UTIL__HPP
#define STRUCT_UTIL__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>


BEGIN_SCOPE(struct_util)

class SequenceSet;
class AlignmentSet;
class BlockMultipleAlignment;
class BLAST_Matrix;

class NCBI_STRUCTUTIL_EXPORT AlignmentUtility
{
public:
    typedef std::list < ncbi::CRef < ncbi::objects::CSeq_entry > > SeqEntryList;
    typedef std::list< ncbi::CRef< ncbi::objects::CSeq_annot > > SeqAnnotList;

    AlignmentUtility(const ncbi::objects::CSeq_entry& seqEntry, const SeqAnnotList& seqAnnots);
    AlignmentUtility(const SeqEntryList& seqEntries, const SeqAnnotList& seqAnnots);
    ~AlignmentUtility();

    bool Okay(void) const { return m_okay; }

    //  Use ctor to copy the SeqEntryList & SeqAnnotList.
    AlignmentUtility* Clone() const;

    // Get ASN data
    const SeqAnnotList& GetSeqAnnots(void);

    // Get SequenceSet (e.g. for use in AlignmentSet::CreateFromMultiple)
    const SequenceSet * GetSequenceSet(void) const {return m_sequenceSet;}

	// Get the PSSM associated with this alignment (implies IBM); returns NULL on failure
    const BLAST_Matrix * GetPSSM(void);

    // Get the BlockMultipleAlignment (implies IBM); returns NULL on failure
    const BlockMultipleAlignment * GetBlockMultipleAlignment(void);

    // Check if the BlockMultipleAlignment has been built.
    bool IsBlockMultipleAlignmentBuilt() const { return (m_currentMultiple != NULL);}

    // Configure object with a different alignment - clones the bma passed in.
    // New bma may have deleted or reordered rows, but for any rows having new
    // sequences those sequences must be explicitly added.
    void SetBlockMultipleAlignment(const BlockMultipleAlignment* bma);

    // compute the score-vs-PSSM for a row (implies IBM), as sum of scores of aligned residues;
    // returns kMin_Int on error
    int ScoreRowByPSSM(unsigned int row);

    // do the intersect-by-master (IBM) algorithm; returs true on success
    bool DoIBM(void);

    // do the leave-one-out (LOO) algorithm (implies IBM) using the block aligner.
    // Numbering in these arguments starts from zero. Note that this currently requires
    // the file "data/BLOSUM62" to be present, used for PSSM calculation.
	// Returns true on success.
    bool DoLeaveOneOut(
        unsigned int row, const std::vector < unsigned int >& blocksToRealign,  // what to realign
        double percentile, unsigned int extension, unsigned int cutoff,         // to calculate max loop lengths
        unsigned int queryFrom = 0, unsigned int queryTo = kMax_UInt);          // range on realigned row to search

    // Adaptation of DoLeaveOneOut.  Only one PSSM computation for all rows left out.
    //
    // For each row in 'rowsToRealign', block align against the PSSM computed
    // with all those rows removed from the multiple alignment.  After all are
    // individually re-aligned against the PSSM, re-insert the rows.
    // The leave-N-out (LNO) algorithm (implies IBM) using the block aligner.
    // Numbering in these arguments starts from zero. Note that this currently requires
    // the file "data/BLOSUM62" to be present, used for PSSM calculation.
	// Returns true on success.
    bool DoLeaveNOut(std::vector<unsigned int>& rowsToRealign, const std::vector < unsigned int >& blocksToRealign, // what to realign
        double percentile, unsigned int extension, unsigned int cutoff,         // to calculate max loop lengths
        std::vector<unsigned int>& queryFrom, std::vector<unsigned int>& queryTo);        // range on realigned row to search

    // Convenience methods to avoid explicit manipulation of the underlying BlockMultipleAlignment.
    // Avoid repeated calls to 'const' versions --> clones 'this' each time if the underlying
    // m_currentMultiple object does not already exist.
    // If necessary, non-const versions build m_currentMultiple as a side-effect.
    unsigned int GetNRows();
    unsigned int GetNRows() const;
    bool IsRowPDB(unsigned int row);
    bool IsRowPDB(unsigned int row) const;
    std::string GetSeqIdStringForRow(unsigned int row);
    std::string GetSeqIdStringForRow(unsigned int row) const;

private:
    // sequence data
    SeqEntryList m_seqEntries;
    SequenceSet *m_sequenceSet;

    // alignment data - the idea is that since these are redundant, we'll either have the
    // SeqAnnots+AlignmentSet or the BlockMultipleAlignment, not (usually) both. If one
    // changes, the other should be removed or recreated; if both are present, they must
    // contain the same data.
    SeqAnnotList m_seqAnnots;
    AlignmentSet *m_alignmentSet;
    BlockMultipleAlignment *m_currentMultiple;

    void Init(void);
    bool m_okay;

    // to delete parts of the data
    void RemoveMultiple(void);
    void RemoveAlignAnnot(void);
};

END_SCOPE(struct_util)

#endif // STRUCT_UTIL__HPP
