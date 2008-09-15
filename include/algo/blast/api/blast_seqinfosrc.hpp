#ifndef ALGO_BLAST_API__BLAST_SEQINFOSRC__HPP
#define ALGO_BLAST_API__BLAST_SEQINFOSRC__HPP

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

/** @file blast_seqinfosrc.hpp
 * Defines interface for retrieving sequence identifiers.
 */

#include <corelib/ncbiobj.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/core/blast_export.h>
#include <list>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_id;
    class CSeq_loc;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// Abstract base class to encapsulate retrieval of sequence identifiers. 
/// Used for processing of results coming directly from the BLAST 
/// core engine, e.g. on-the-fly tabular output.
class IBlastSeqInfoSrc : public CObject {
public:
    virtual ~IBlastSeqInfoSrc() {}
    
    /// Method to retrieve a sequence identifier given its ordinal number.
    /// @param index the ordinal number to retrieve [in]
    virtual list< CRef<objects::CSeq_id> > GetId(Uint4 index) const = 0;

    /// Method to retrieve the sequence location given its ordinal number.
    /// @param index the ordinal number to retrieve [in]
    virtual CConstRef<objects::CSeq_loc> GetSeqLoc(Uint4 index) const = 0;

    /// Method to retrieve a sequence length given its ordinal number.
    /// @param index the ordinal number to retrieve [in]
    virtual Uint4 GetLength(Uint4 index) const = 0;

    /// Returns the size of the underlying container of sequences
    virtual size_t Size() const = 0;
    
    /// Returns true if the subject is restricted by a GI list.
    virtual bool HasGiList() const = 0;

    /// Retrieves the subject masks for the corresponding index
    /// @param index the ordinal number to retrieve [in]
    /// @param target_range range for which to return masks for. Empty ranges
    /// indicate that no masks should be retrieved, whole ranges mean that
    /// masks for the whole sequence should be retrieved [in]
    /// @param retval the masks will be returned through this parameter [out]
    /// @return true if there were masks returned in retval, otherwise false.
    virtual bool GetMasks(Uint4 index, 
                          const TSeqRange& target_range,
                          TMaskedSubjRegions& retval) const = 0;

    /// Allow implementations to provide a facility to release memory
    virtual void GarbageCollect() {};
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__BLAST_SEQINFOSRC_HPP */
