#ifndef OBJTOOLS_DATA_LOADERS_BLASTDB___BLASTDB_ADAPTER__HPP
#define OBJTOOLS_DATA_LOADERS_BLASTDB___BLASTDB_ADAPTER__HPP

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
 *  ===========================================================================
 *
 *  Author: Christiam Camacho
 *
 * ===========================================================================
 */

/** @file blastdb_adapter.hpp
  * Interface definition of IBlastDbAdapter.
  */

#include <corelib/ncbistd.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// When fixed size slices are not used, each subsequent slice grows its size
/// by this factor
#define kSliceGrowthFactor 2

/// The sequence data will sliced into pieces of this size by default
enum {
    /// If sequence is shorter than this size, it will not be split and it will
    /// be loaded full as soon as its data is requested
    kFastSequenceLoadSize = 1024,
    /// If sequence is shorter than this size but greater than
    /// kFastSequenceLoadSize, it will be "split" into once
    /// piece and its sequence data will be loaded when the chunks are
    /// requested, otherwise, if it's larger than this, the sequence data will
    /// be split into multiple chunks and retrieved on demand.
    kSequenceSliceSize    = 65536,
    /// Same as above, but used for fetching sequences from remote BLAST
    /// databases
    kRmtSequenceSliceSize = kSequenceSliceSize * 2
};

/** Interface that provides a common interface to retrieve sequence data from
 * local vs. remote BLAST databases.
 */
class IBlastDbAdapter : public CObject
{
public:
    /// Virtual destructor
    virtual ~IBlastDbAdapter() {}

    /// Get the molecule type of this object (protein or nucleotide)
    /// @return The sequence type.
    virtual CSeqDB::ESeqType GetSequenceType() = 0;

    /// Get the length of the sequence.
    /// @param oid An ID for this sequence in this db.
    /// @return Sequence length (in bases).
    virtual int GetSeqLength(int oid) = 0;

    /// Convenience typedef for a list of CSeq_id-s
    typedef list< CRef<CSeq_id> > TSeqIdList;

    /// Get the list of Seq-ids for the given OID.
    virtual list< CRef<CSeq_id> > GetSeqIDs(int oid) = 0;
    
    /// Get a CBioseq for the requested oid, but without sequence data.
    ///
    /// If target is specified, that defline will be promoted to the
    /// top of the CBioseq object, if possible
    /// @note The current implementation of the remote BLAST database interface
    /// does not implement this promotion; the blast4 service will promote
    /// whichever Seq-id was used to fetch the OID, which in practice
    /// should be the same one.
    ///
    /// @param oid An ID for this sequence in this db.
    /// @param target_gi If non-zero, the target GI to filter the header
    /// information.
    /// @param target_id if non-NULL, the target ID is used to filter
    /// the header information.
    /// @return object corresponding to the sequence, but without
    ///   sequence data.
    virtual CRef<CBioseq> GetBioseqNoData(int oid, TGi target_gi = ZERO_GI, const CSeq_id * target_id = NULL) = 0;
    
    /// Get all or part of the sequence data as a Seq-data object.
    /// @param oid    Identifies which sequence to get.
    /// @param begin  Starting offset of the section to get.
    /// @param end    Ending offset of the section to get.
    /// @return       The sequence data.
    /// @note if the begin and end arguments are zero, the whole sequence will
    /// be returned
    virtual CRef<CSeq_data> 
    GetSequence(int oid, int begin = 0, int end = 0) = 0;
    
    /// Find a Seq-id in the database and get an OID if found.
    ///
    /// If the Seq-id is found, this method returns true, and the oid argument
    /// will be populated accordingly. This oid should be used in the other
    /// methods provided by this interface.
    ///
    /// @param id The Seq-id to find.
    /// @param oid An ID for this sequence (if it was found).
    /// @return True if the sequence was found in the database.
    virtual bool SeqidToOid(const CSeq_id & id, int & oid) = 0;
    
    /// Retrieve the taxonomy ID for the requested sequence identifier
    /// @param idh The Seq-id for which to get the taxonomy ID
    /// @return taxonomy ID if found, otherwise kInvalidSeqPos
    virtual int GetTaxId(const CSeq_id_Handle& idh) {
        return static_cast<int>(kInvalidSeqPos);
    }
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* OBJTOOLS_DATA_LOADERS_BLASTDB___BLASTDB_ADAPTER__HPP */
