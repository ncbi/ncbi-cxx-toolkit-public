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

// forward declaration of BLAST_Matrix; #include <blastkar.h> to access
struct _blast_matrix;
typedef struct _blast_matrix BLAST_Matrix_;


BEGIN_SCOPE(struct_util)

class SequenceSet;
class AlignmentSet;
class BlockMultipleAlignment;

class AlignmentUtility
{
public:
    typedef std::list < ncbi::CRef < ncbi::objects::CSeq_entry > > SeqEntryList;
    typedef std::list< ncbi::CRef< ncbi::objects::CSeq_annot > > SeqAnnotList;

    AlignmentUtility(const ncbi::objects::CSeq_entry& seqEntry, const SeqAnnotList& seqAnnots);
    AlignmentUtility(const SeqEntryList& seqEntries, const SeqAnnotList& seqAnnots);
    ~AlignmentUtility();

    bool Okay(void) const { return m_okay; }

    // Get ASN data
    const SeqAnnotList& GetSeqAnnots(void);

	// Get the PSSM associated with this alignment (implies IBM); returns NULL on failure
	const BLAST_Matrix_ * GetPSSM(void);

    // Get the BlockMultipleAlignment (implies IBM); returns NULL on failure
    const BlockMultipleAlignment * GetBlockMultipleAlignment(void);

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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2004/07/15 13:52:14  thiessen
* calculate max loops before row extraction; add quertFrom/To parameters
*
* Revision 1.7  2004/06/14 13:50:28  thiessen
* make BlockMultipleAlignment and Sequence classes public; add GetBlockMultipleAlignment() and ScoreByPSSM()
*
* Revision 1.6  2004/06/10 14:18:27  thiessen
* add GetPSSM()
*
* Revision 1.5  2004/05/28 09:43:35  thiessen
* add comment about data/BLOSUM62
*
* Revision 1.4  2004/05/26 14:30:53  thiessen
* adjust handling of alingment data ; add row ordering
*
* Revision 1.3  2004/05/26 02:41:13  thiessen
* progress towards LOO - all but PSSM and row ordering
*
* Revision 1.2  2004/05/25 15:50:46  thiessen
* add BlockMultipleAlignment, IBM algorithm
*
* Revision 1.1  2004/05/24 23:05:10  thiessen
* initial checkin
*
*/

