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
 * Author:  Christiam Camacho / Kevin Bealer
 *
 */

/** @file blast_objmgr_priv.hpp
 */

#ifndef ALGO_BLAST_API___BLAST_OBJMGR_PRIV__HPP
#define ALGO_BLAST_API___BLAST_OBJMGR_PRIV__HPP

#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
    class CScope;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

class CBlastQuerySourceOM : public IBlastQuerySource {
public:
    CBlastQuerySourceOM(const TSeqLocVector & v);
    
    objects::ENa_strand GetStrand(int j) const;
    CConstRef<objects::CSeq_loc> GetMask(int j) const;
    CConstRef<objects::CSeq_loc> GetSeqLoc(int j) const;
    SBlastSequence GetBlastSequence(int j,
                                    EBlastEncoding encoding,
                                    objects::ENa_strand strand,
                                    ESentinelType sentinel,
                                    string* warnings) const;
    TSeqPos GetLength(int j) const;
    TSeqPos Size() const;
    
private:
    const TSeqLocVector& m_TSeqLocVector;
};

/** Allocates the query information structure and fills the context 
 * offsets, in case of multiple queries, frames or strands. If query seqids
 * cannot be resolved, they will be ignored as warnings will be issued in
 * blast::SetupQueries.
 * NB: effective length will be assigned inside the engine.
 * @param queries Vector of query locations [in]
 * @param prog program type from the CORE's point of view [in]
 * @param strand_opt Unless the strand option is set to single strand, the 
 * actual CSeq_locs in the TSeqLocVector dictacte which strand to use
 * during the search [in]
 * @param qinfo Allocated query info structure [out]
 */
void
SetupQueryInfo(const TSeqLocVector& queries, 
               EBlastProgramType prog,
               objects::ENa_strand strand_opt,
               BlastQueryInfo** qinfo); // out

/// Populates BLAST_SequenceBlk with sequence data for use in CORE BLAST
/// @param queries vector of blast::SSeqLoc structures [in]
/// @param qinfo BlastQueryInfo structure to obtain context information [in]
/// @param seqblk Structure to save sequence data, allocated in this 
/// function [out]
/// @param blast_msg Structure to save warnings/errors, allocated in this
/// function [out]
/// @param prog program type from the CORE's point of view [in]
/// @param strand_opt Unless the strand option is set to single strand, the 
/// actual CSeq_locs in the TSeqLocVector dictacte which strand to use
/// during the search [in]
/// @param genetic_code genetic code string as returned by
/// blast::FindGeneticCode()

void
SetupQueries(const TSeqLocVector& queries,
             const BlastQueryInfo* qinfo, BLAST_SequenceBlk** seqblk,
             EBlastProgramType prog, 
             objects::ENa_strand strand_opt,
             const Uint1* genetic_code,
             Blast_Message** blast_msg);

/** Sets up internal subject data structure for the BLAST search.
 * @param subjects Vector of subject locations [in]
 * @param program BLAST program [in]
 * @param seqblk_vec Vector of subject sequence data structures [out]
 * @param max_subjlen Maximal length of the subject sequences [out]
 */
void
SetupSubjects(const TSeqLocVector& subjects, 
              EBlastProgramType program,
              vector<BLAST_SequenceBlk*>* seqblk_vec, 
              unsigned int* max_subjlen);

/** Retrieves a sequence using the object manager.
 * @param sl seqloc of the sequence to obtain [in]
 * @param encoding encoding for the sequence retrieved.
 *        Supported encodings include: eBlastEncodingNcbi2na, 
 *        eBlastEncodingNcbi4na, eBlastEncodingNucleotide, and 
 *        eBlastEncodingProtein. [in]
 * @param scope Scope from which the sequences are retrieved [in]
 * @param strand strand to retrieve (applies to nucleotide only).
 *        N.B.: When requesting the eBlastEncodingNcbi2na, only the plus strand
 *        is retrieved, because BLAST only requires one strand on the subject
 *        sequences (as in BLAST databases). [in]
 * @param sentinel Use eSentinels to guard nucleotide sequence with sentinel 
 *        bytes (ignored for protein sequences, which always have sentinels) 
 *        When using eBlastEncodingNcbi2na, this argument should be set to
 *        eNoSentinels as a sentinel byte cannot be represented in this 
 *        encoding. [in]
 * @param warnings Used to emit warnings when fetching sequence (e.g.:
 *        replacement of invalid characters). Parameter must be allocated by
 *        caller of this function and warnings will be appended. [out]
 * @throws CBlastException, CSeqVectorException, CException
 * @return pair containing the buffer and its length. 
 */
SBlastSequence
GetSequence(const objects::CSeq_loc& sl, EBlastEncoding encoding, 
            objects::CScope* scope,
            objects::ENa_strand strand = objects::eNa_strand_plus, 
            ESentinelType sentinel = eSentinels,
            std::string* warnings = NULL);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BLAST_OBJMGR_PRIV__HPP */
