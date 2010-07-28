#ifndef ALGO_BLAST_API__SEQINFOSRC_SEQVEC__HPP
#define ALGO_BLAST_API__SEQINFOSRC_SEQVEC__HPP

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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file seqinfosrc_seqvec.hpp
 * Defines a concrete strategy for the IBlastSeqInfoSrc interface for
 * sequence identifiers retrieval from a CSeqDB class.
 */

#include <algo/blast/api/blast_seqinfosrc.hpp>
#include <algo/blast/api/sseqloc.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Implementation of the IBlastSeqInfoSrc interface to encapsulate retrieval
/// of sequence identifiers and lengths from a vector of Seq-locs. 
class NCBI_XBLAST_EXPORT CSeqVecSeqInfoSrc : public IBlastSeqInfoSrc
{
public:
    /// Constructor from a vector of sequence locations.
    CSeqVecSeqInfoSrc(const TSeqLocVector& seqv);

    /// Destructor
    virtual ~CSeqVecSeqInfoSrc();

    /// Retrieve a sequence identifier given its index in the vector.
    /// @param index the ordinal number to retrieve [in]
    virtual list< CRef<objects::CSeq_id> > GetId(Uint4 index) const;

    /// Method to retrieve the sequence location given its ordinal number.
    /// @param index the ordinal number to retrieve [in]
    virtual CConstRef<objects::CSeq_loc> GetSeqLoc(Uint4 index) const;

    /// Retrieve sequence length given its index in the vector.
    /// @param index the ordinal number to retrieve [in]
    virtual Uint4 GetLength(Uint4 index) const;

    /// Returns the size of the underlying container of sequences
    virtual size_t Size() const;
    
    /// Is the subject restricted by a GI list?  (Always returns false).
    virtual bool HasGiList() const;
    
    /// Retrieves the subject masks for the corresponding index
    /// @param index the ordinal number to retrieve [in]
    /// @param target_range range for which to return masks for. Empty ranges
    /// indicate that no masks should be retrieved, whole ranges mean that masks
    /// for the whole sequence should be retrieved [in]
    /// @param retval the masks will be returned through this parameter [out]
    /// @return true if there were masks returned in retval, otherwise false.
    virtual bool GetMasks(Uint4 index, 
                          const TSeqRange& target_range,
                          TMaskedSubjRegions& retval) const;

    /// Retrieves the subject masks for the corresponding index
    /// @param index the ordinal number to retrieve [in]
    /// @param target_ranges range for which to return masks for. Empty ranges
    /// indicate that no masks should be retrieved, whole ranges mean that masks
    /// for the whole sequence should be retrieved [in]
    /// @param retval the masks will be returned through this parameter [out]
    /// @return true if there were masks returned in retval, otherwise false.
    virtual bool GetMasks(Uint4 index, 
                          const vector<TSeqRange>& target_ranges,
                          TMaskedSubjRegions& retval) const;
private:
    TSeqLocVector m_SeqVec; ///< Vector of subject sequence locations to get 
                            /// information from

};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__SEQINFOSRC_SEQVEC_HPP */
