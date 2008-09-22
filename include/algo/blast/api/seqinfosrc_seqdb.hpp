#ifndef ALGO_BLAST_API__SEQINFOSRC_SEQDB__HPP
#define ALGO_BLAST_API__SEQINFOSRC_SEQDB__HPP

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

/** @file seqinfosrc_seqdb.hpp
 * Defines a concrete strategy for the IBlastSeqInfoSrc interface for
 * sequence identifiers retrieval from a CSeqDB class.
 */

#include <algo/blast/core/blast_export.h>
#include <algo/blast/api/blast_seqinfosrc.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Implementation of the IBlastSeqInfoSrc interface to encapsulate retrieval of
/// sequence identifiers and lengths from a BLAST database. 
class NCBI_XBLAST_EXPORT CSeqDbSeqInfoSrc : public IBlastSeqInfoSrc
{
public:
    /// Constructor: includes initializing the CSeqDB object.
    /// @param dbname Name of the BLAST database [in]
    /// @param is_protein Is this a protein or nucleotide database? [in]
    CSeqDbSeqInfoSrc(const string& dbname, bool is_protein);

    /// Constructor from an already existing CSeqDB object.
    /// @param seqdb Pointer to the CSeqDB object to use.
    CSeqDbSeqInfoSrc(ncbi::CSeqDB* seqdb);

    /// Our destructor
    virtual ~CSeqDbSeqInfoSrc();

    /// Sets the filtering algorithm IDs used in the search
    /// @param algo_ids filtering algorithm IDs [in]
    void SetFilteringAlgorithmIds(const vector<int>& algo_ids);

    /// Retrieve a sequence identifier given its ordinal number.
    /// @param index the ordinal number to retrieve [in]
    virtual list< CRef<objects::CSeq_id> > GetId(Uint4 oid) const;

    /// Method to retrieve the sequence location given its ordinal number.
    /// @param index the ordinal number to retrieve [in]
    virtual CConstRef<objects::CSeq_loc> GetSeqLoc(Uint4 oid) const;

    /// Retrieve sequence length given its ordinal number.
    /// @param index the ordinal number to retrieve [in]
    virtual Uint4 GetLength(Uint4 oid) const;

    /// Returns the size of the underlying container of sequences
    virtual size_t Size() const;
    
    /// Returns true if the subject is restricted by a GI list.
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

    /// Invoke CSeqDB's garbage collector
    virtual void GarbageCollect();
private:
    mutable CRef<CSeqDB> m_iSeqDb; ///< BLAST database object
    /// Algorithm IDs for filtering algorithms used with these subjects
    vector<int> m_FilteringAlgoIds;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__SEQINFOSRC_SEQDB_HPP */
