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

class CBlastOptions;
class CPSIBlastOptionsHandle;

/// Implements the object manager dependant version of the IBlastQuerySource
class NCBI_XBLAST_EXPORT CBlastQuerySourceOM : public IBlastQuerySource {
public:
    /// Constructor which takes a TSeqLocVector
    /// 
    /// This version assumes the masking information (if any) was
    /// provided with the TSeqLocVector.
    /// 
    /// @param v vector of SSeqLoc structures containing the queries [in]
    /// @param prog program type of this search [in]
    CBlastQuerySourceOM(TSeqLocVector & v, EBlastProgramType prog);
    
    /// Constructor which takes a TSeqLocVector
    /// 
    /// This version will compute masking information with dust.
    /// 
    /// @param v vector of SSeqLoc structures containing the queries [in]
    /// @param opts BLAST algorithm options [in]
    /// @note that the v argument might be changed with the filtering locations
    CBlastQuerySourceOM(TSeqLocVector & v, const CBlastOptions* opts);

    /// Constructor which takes a CBlastQueryVector
    /// 
    /// This version assumes the masking information (if any) was
    /// provided with the CBlastQueryVector.
    /// 
    /// @param v Object containing the queries, scopes and masking info [in]
    /// @param prog type of program to run [in]
    CBlastQuerySourceOM(CBlastQueryVector & v, EBlastProgramType prog);
    
    /// Constructor which takes a CBlastQueryVector
    /// 
    /// This version will compute masking information with dust.
    ///
    /// @param v Object containing the queries, scopes and masking info [in]
    /// @param opts BLAST algorithm options [in]
    CBlastQuerySourceOM(CBlastQueryVector & v, const CBlastOptions* opts);
    
    /// dtor which determines if the internal pointer to its data should be
    /// deleted or not.
    virtual ~CBlastQuerySourceOM();
    
    /// Return strand for a sequence
    /// @param i index of the sequence in the sequence container [in]
    virtual objects::ENa_strand GetStrand(int i) const;
    
    /// Return the filtered (masked) regions for a sequence
    /// @param i index of the sequence in the sequence container [in]
    virtual CConstRef<objects::CSeq_loc> GetMask(int i);
    
    /// Return the filtered (masked) regions for a sequence
    /// @param i index of the sequence in the sequence container [in]
    virtual TMaskedQueryRegions GetMaskedRegions(int i);
    
    /// Return the CSeq_loc associated with a sequence
    /// @param i index of the sequence in the sequence container [in]
    virtual CConstRef<objects::CSeq_loc> GetSeqLoc(int i) const;
    
    /// Return the sequence identifier associated with a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual const objects::CSeq_id* GetSeqId(int index) const;
    
    /// Retrieve the genetic code associated with a sequence
    /// @param index index of the sequence in the sequence container [in]
    virtual Uint4 GetGeneticCodeId(int index) const;
    
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
    
protected:
    /// Reference to input CBlastQueryVector (or empty if not used)
    CRef<CBlastQueryVector> m_QueryVector;
    
    /// Reference to input TSeqLocVector (or NULL if not used)
    TSeqLocVector* m_TSeqLocVector;
    
    /// flag to determine if the member above should or not be deleted in the
    /// destructor
    bool m_OwnTSeqLocVector;
    
    /// BLAST algorithm options
    const CBlastOptions* m_Options;
    
private:
    /// this flag allows for lazy initialization of the masking locations
    bool m_CalculatedMasks;

    /// BLAST program variable
    EBlastProgramType   m_Program;
    
    /// Performs filtering on the query sequences to calculate the masked
    /// locations
    void x_CalculateMasks();

    /// Tries to extract the genetic code using the CScope, if it succeeds,
    /// it supercedes what's specified in the
    /// {SSeqLoc,CBlastSearchQuery}::genetic_code_id field
    void x_AutoDetectGeneticCodes(void);
};

/** Allocates the query information structure and fills the context 
 * offsets, in case of multiple queries, frames or strands. If query seqids
 * cannot be resolved, they will be ignored as warnings will be issued in
 * blast::SetupQueries.  This version takes a TSeqLocVector.
 * NB: effective length will be assigned inside the engine.
 * @param queries Vector of query locations [in]
 * @param prog program type from the CORE's point of view [in]
 * @param strand_opt Unless the strand option is set to single strand, the 
 * actual CSeq_locs in the TSeqLocVector dictate which strand to use
 * during the search [in]
 * @param qinfo Allocated query info structure [out]
 */
NCBI_XBLAST_EXPORT
void
SetupQueryInfo(TSeqLocVector& queries, 
               EBlastProgramType prog,
               objects::ENa_strand strand_opt,
               BlastQueryInfo** qinfo);

/** Allocates the query information structure and fills the context 
 * offsets, in case of multiple queries, frames or strands. If query seqids
 * cannot be resolved, they will be ignored as warnings will be issued in
 * blast::SetupQueries.  This version takes a CBlastQueryVector.
 * NB: effective length will be assigned inside the engine.
 * @param queries Vector of query locations [in]
 * @param prog program type from the CORE's point of view [in]
 * @param strand_opt Unless the strand option is set to single strand, the 
 * actual CSeq_locs in the CBlastQueryVector dictate which strand to use
 * during the search [in]
 * @param qinfo Allocated query info structure [out]
 */
NCBI_XBLAST_EXPORT
void
SetupQueryInfo(const CBlastQueryVector & queries, 
               EBlastProgramType prog,
               objects::ENa_strand strand_opt,
               BlastQueryInfo** qinfo);

/// Populates BLAST_SequenceBlk with sequence data for use in CORE BLAST
///
/// @param queries vector of blast::SSeqLoc structures [in]
/// @param qinfo BlastQueryInfo structure to obtain context information [in]
/// @param seqblk Structure to save sequence data, allocated in this 
/// function [out]
/// @param messages object to save warnings/errors for all queries [out]
/// @param prog program type from the CORE's point of view [in]
/// @param strand_opt Unless the strand option is set to single strand, the 
/// actual CSeq_locs in the TSeqLocVector dictate which strand to use
/// during the search [in]

NCBI_XBLAST_EXPORT
void
SetupQueries(TSeqLocVector& queries,
             BlastQueryInfo* qinfo, 
             BLAST_SequenceBlk** seqblk,
             EBlastProgramType prog, 
             objects::ENa_strand strand_opt,
             TSearchMessages& messages);

/** Sets up internal subject data structure for the BLAST search.
 *
 * This uses the TSeqLocVector to create subject data structures.
 * Note that the TSeqLocVector may contain masking information, but the
 * current versions of the blast code do not use this information for
 * subject sequences.
 *
 * @param subjects Vector of subject locations [in]
 * @param program BLAST program [in]
 * @param seqblk_vec Vector of subject sequence data structures [out]
 * @param max_subjlen Maximal length of the subject sequences [out]
 */
NCBI_XBLAST_EXPORT
void
SetupSubjects(TSeqLocVector& subjects, 
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
NCBI_XBLAST_EXPORT
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
NCBI_XBLAST_EXPORT
CRef<objects::CPacked_seqint>
TSeqLocVector2Packed_seqint(const TSeqLocVector& sequences);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BLAST_OBJMGR_PRIV__HPP */
