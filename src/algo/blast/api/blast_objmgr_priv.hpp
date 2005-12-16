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
 * Definitions which are dependant on the NCBI C++ Object Manager
 */

#ifndef ALGO_BLAST_API___BLAST_OBJMGR_PRIV__HPP
#define ALGO_BLAST_API___BLAST_OBJMGR_PRIV__HPP

#include "blast_setup.hpp"
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/blast_seqinfosrc.hpp>
#include <algo/blast/api/blast_types.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
    class CScope;
    class CBioseq;
    class CSeq_align_set;
    class CPssmWithParameters;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

class CPSIBlastOptionsHandle;

/// Implements the object manager dependant version of the IBlastQuerySource
class CBlastQuerySourceOM : public IBlastQuerySource {
public:
    /// Constructor which takes a TSeqLocVector
    /// @param v vector of SSeqLoc structures containing the queries [in]
    CBlastQuerySourceOM(const TSeqLocVector & v);
    
    /// Return strand for a sequence
    /// @param i index of the sequence in the sequence container [in]
    virtual objects::ENa_strand GetStrand(int i) const;
    /// Return the filtered (masked) regions for a sequence
    /// @param i index of the sequence in the sequence container [in]
    virtual CConstRef<objects::CSeq_loc> GetMask(int i) const;
    /// Return the CSeq_loc associated with a sequence
    /// @param i index of the sequence in the sequence container [in]
    virtual CConstRef<objects::CSeq_loc> GetSeqLoc(int i) const;
    /// Return the sequence data for a sequence
    /// @param i index of the sequence in the sequence container [in]
    /// @param encoding desired encoding [in]
    /// @param strand strand to fetch [in]
    /// @param sentinel specifies to use or not to use sentinel bytes around
    ///        sequence data. Note that this is ignored for proteins, as in the
    ///        CORE of BLAST, proteins always have sentinel bytes [in]
    /// @param warnings if not NULL, warnings will be returned in this string
    ///        [in|out]
    /// @return SBlastSequence structure containing sequence data requested
    virtual SBlastSequence GetBlastSequence(int i,
                                            EBlastEncoding encoding,
                                            objects::ENa_strand strand,
                                            ESentinelType sentinel,
                                            string* warnings = 0) const;
    /// Return the length of a sequence
    /// @param i index of the sequence in the sequence container [in]
    virtual TSeqPos GetLength(int i) const;
    /// Return the number of elements in the sequence container
    virtual TSeqPos Size() const;
    
private:
    /// Reference to input TSeqLocVector
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
               BlastQueryInfo** qinfo);

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
             TSearchMessages& messages);

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

/** Converts a TSeqLocVector into a CPacked_seqint. Note that a consequence of
 * this is that the CSeq_loc-s specified in the TSeqLocVector cannot be
 * of type mix or packed int
 * @param sequences data to convert from [in]
 * @return CPacked_seqint containing data from previous sequences
 */
CRef<objects::CPacked_seqint>
TSeqLocVector2Packed_seqint(const TSeqLocVector& sequences);


/////////////////////////////////////////////////////////////////////////////
// Functions to help in Seq-align construction
/////////////////////////////////////////////////////////////////////////////

/** Converts BlastHSPResults structure into a vector of CSeq_align_set classes
 * Returns one vector element per query sequence; all subject matches form a 
 * list of discontinuous CSeq_align's. 
 * @param results results from running the BLAST algorithm [in]
 * @param prog type of BLAST program [in]
 * @param query All query sequences [in]
 * @param seqinfo_src Source of subject sequences information [in]
 * @param is_gapped Is this a gapped search? [in]
 * @param is_ooframe Is it a search with out-of-frame gapping? [in]
 * @return Vector of seqalign sets (one set per query sequence).
 */
TSeqAlignVector
BLAST_Results2CSeqAlign(const BlastHSPResults* results, 
                        EBlastProgramType prog,
                        TSeqLocVector &query, 
                        const IBlastSeqInfoSrc* seqinfo_src, 
                        bool is_gapped=true, 
                        bool is_ooframe=false);

/// Converts PHI BLAST results into a Seq-align form. Results are 
/// split into separate Seq-align-sets corresponding to different
/// pattern occurrences in query, which are then wrapped into 
/// discontinuous Seq-aligns and then put into a top level Seq-align-set.
/// @param results All PHI BLAST results, with different query pattern
///                occurrences mixed together. [in]
/// @param prog Type of BLAST program (phiblastp or phiblastn) [in]
/// @param query Query Seq-loc, wrapped in a TSeqLocVector [in]
/// @param seqinfo_src Source of subject sequence information [in]
/// @param pattern_info Information about query pattern occurrences [in]
TSeqAlignVector 
PHIBlast_Results2CSeqAlign(const BlastHSPResults  * results,
                           EBlastProgramType        prog,
                           const TSeqLocVector    & query,
                           const IBlastSeqInfoSrc * seqinfo_src,
                           const SPHIQueryInfo    * pattern_info);

/////////////////////////////////////////////////////////////////////////////
// Functions to help in PSSM generation
/////////////////////////////////////////////////////////////////////////////

/** Computes a PSSM from the result of a PSI-BLAST iteration
 * @param query Query sequence [in]
 * @param alignment BLAST pairwise alignment obtained from the PSI-BLAST
 * iteration [in]
 * @param database_scope Scope from which the database sequences will be
 * retrieved [in]
 * @param opts_handle PSI-BLAST options [in]
 * @param diagnostics_req Optional requests for diagnostics data from the PSSM
 * engine [in]
 * @todo add overloaded function which takes a blast::SSeqLoc
 */
CRef<objects::CPssmWithParameters> 
PsiBlastComputePssmFromAlignment(const objects::CBioseq& query,
                                 CConstRef<objects::CSeq_align_set> alignment,
                                 CRef<objects::CScope> database_scope,
                                 const CPSIBlastOptionsHandle& opts_handle,
                                 PSIDiagnosticsRequest* diagnostics_req = 0);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BLAST_OBJMGR_PRIV__HPP */
