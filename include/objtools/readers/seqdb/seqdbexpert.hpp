#ifndef OBJTOOLS_READERS_SEQDB__SEQDB_EXPERT_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDB_EXPERT_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbexpert.hpp
/// Defines `expert' version of CSeqDB interfaces.
///
/// Defines classes:
///     CSeqDBExpert
///
/// Implemented for: UNIX, MS-Windows

#include <objtools/readers/seqdb/seqdb.hpp>
#include <set>

BEGIN_NCBI_SCOPE

/// Include definitions from the objects namespace.
USING_SCOPE(objects);


/// CSeqDBExpert
///
/// User interface class for blast databases, including experimental
/// and advanced code for expert NCBI use.

class NCBI_XOBJREAD_EXPORT CSeqDBExpert : public CSeqDB {
public:
    /// Short Constructor
    /// 
    /// This version of the constructor assumes memory mapping and
    /// that the entire possible OID range will be included.
    /// 
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces
    /// @param seqtype
    ///   Specify eProtein, eNucleotide, or eUnknown.
    /// @param gilist
    ///   The database will be filtered by this GI list if non-null.
    CSeqDBExpert(const string & dbname,
                 ESeqType seqtype,
                 CSeqDBGiList * gilist = 0);
    
    /// Constructor with MMap Flag and OID Range.
    ///
    /// If the oid_end value is specified as zero, or as a value
    /// larger than the number of OIDs, it will be adjusted to the
    /// number of OIDs in the database.  Specifying 0,0 for the start
    /// and end will cause inclusion of the entire database.  This
    /// version of the constructor is obsolete because the sequence
    /// type is specified as a character (eventually only the ESeqType
    /// version will exist).
    /// 
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces.
    /// @param seqtype
    ///   Specify eProtein, eNucleotide, or eUnknown.
    /// @param oid_begin
    ///   Iterator will skip OIDs less than this value.  Only OIDs
    ///   found in the OID lists (if any) will be returned.
    /// @param oid_end
    ///   Iterator will return up to (but not including) this OID.
    /// @param use_mmap
    ///   If kSeqDBMMap is specified (the default), memory mapping is
    ///   attempted.  If kSeqDBNoMMap is specified, or memory mapping
    ///   fails, this platform does not support it, the less efficient
    ///   read and write calls are used instead.
    /// @param gi_list
    ///   The database will be filtered by this GI list if non-null.
    CSeqDBExpert(const string & dbname,
                 ESeqType       seqtype,
                 int            oid_begin,
                 int            oid_end,
                 bool           use_mmap,
                 CSeqDBGiList * gi_list = 0);
    
    /// Null Constructor
    /// 
    /// This version of the constructor does not open a specific blast
    /// database.  This is provided for cases where the application
    /// only needs 'global' resources like the taxonomy database.
    CSeqDBExpert();
    
    /// Destructor.
    ///
    /// This will return resources acquired by this object, including
    /// any gotten by the GetSequence() call, whether or not they have
    /// been returned by RetSequence().
    ~CSeqDBExpert();
    
    /// Raw Sequence and Ambiguity Data
    ///
    /// Get a pointer to the raw sequence and ambiguity data, and the
    /// length of each.  The encoding for these is not defined here
    /// and should not be relied on to be compatible between different
    /// database format versions.  NULL can be supplied for parameters
    /// that are not needed (except oid).  RetSequence() must be
    /// called with the pointer returned by 'buffer' if and only if
    /// that pointer is supplied as non-null by the user.  Protein
    /// sequences will never have ambiguity data.  Ambiguity data will
    /// be packed in the returned buffer at offset *seq_length.
    ///
    /// @param oid Ordinal id of the sequence.
    /// @param buffer Buffer of raw data.
    /// @param seq_length Returned length of the sequence data.
    /// @param ambig_length Returned length of the ambiguity data.
    void GetRawSeqAndAmbig(int           oid,
                           const char ** buffer,
                           int         * seq_length,
                           int         * ambig_length) const;
    
    /// Get GI Bounds.
    /// 
    /// Fetch the lowest, highest, and total number of GIs.  A value
    /// is returned for each non-null argument.  If the operation
    /// fails, an exception will be thrown, which probably indicates a
    /// missing index file.
    /// 
    /// @param low_id Lowest GI value in database. [out]
    /// @param high_id Highest GI value in database. [out]
    /// @param count Number of GI values in database. [out]
    void GetGiBounds(int * low_id, int * high_id, int * count);
    
    /// Get PIG Bounds.
    /// 
    /// Fetch the lowest, highest, and total number of PIGs.  A value
    /// is returned for each non-null argument.  If the operation
    /// fails, an exception will be thrown, which probably indicates a
    /// missing index file.
    /// 
    /// @param low_id Lowest PIG value in database. [out]
    /// @param high_id Highest PIG value in database. [out]
    /// @param count Number of PIG values in database. [out]
    void GetPigBounds(int * low_id, int * high_id, int * count);
    
    /// Get String Bounds.
    /// 
    /// Fetch the lowest, highest, and total number of string keys in
    /// the database index.  A value is returned for each non-null
    /// argument.  If the operation fails, an exception will be
    /// thrown, which probably indicates a missing index file.  Note
    /// that the number of string keys does not directly correspond to
    /// the number of deflines, Seq-ids, or accessions.
    /// 
    /// @param low_id Lowest string value in database. [out]
    /// @param high_id Highest string value in database. [out]
    /// @param count Number of string values in database. [out]
    void GetStringBounds(string * low_id, string * high_id, int * count);
    
    /// List of sequence offset ranges.
    typedef set< pair<int, int> > TRangeList;
    
    /// Apply a range of offsets to a database sequence.
    ///
    /// The GetAmbigSeq() method requires an amount of work (and I/O)
    /// which is proportional to the size of the sequence data (more
    /// if ambiguities are present).  In some cases, only certain
    /// subranges of this data will be utilized.  This method allows
    /// the user to specify which parts of a sequence are actually
    /// needed by the user.  (Care should be taken if one SeqDB object
    /// is shared by several program components.)  (Note that offsets
    /// above the length of the sequence will not generate an error,
    /// and are replaced by the sequence length.)
    ///
    /// If ranges are specified for a sequence, data areas in
    /// specified sequences will be accurate, but data outside the
    /// specified ranges should not be accessed, and no guarantees are
    /// made about what data they will contain.  If the append_ranges
    /// flag is true, the range will be added to existing ranges.  If
    /// false, existing ranges will be flushed and replaced by new
    /// ranges.  To remove ranges, call this method with an empty list
    /// of ranges (and append_ranges == false); future calls will then
    /// return the complete sequence.
    ///
    /// If the cache_data flag is set, data for this sequence will be
    /// kept for the duration of SeqDB's lifetime.  To disable caching
    /// (and flush cached data) for this sequence, call the method
    /// again, but specify cache_data to be false.
    ///
    /// @param oid           OID of the sequence.
    /// @param offset_ranges Ranges of sequence data to return.
    /// @param append_ranges Append new ranges to existing list.
    /// @param cache_data    Keep sequence data for future callers.
    void SetOffsetRanges(int                oid,
                         const TRangeList & offset_ranges,
                         bool               append_ranges,
                         bool               cache_data);
    
    /// Get the sequence hash for a given OID.
    ///
    /// The sequence data is fetched and the sequence hash is
    /// computed and returned.
    ///
    /// @param oid The sequence to compute the hash of. [in]
    /// @return The sequence hash.
    unsigned GetSequenceHash(int oid);
    
    /// Get the OIDs for a given sequence hash.
    ///
    /// The OIDs corresponding to a hash value are found and returned.
    ///
    /// @param hash The sequence hash to look up. [in]
    /// @param oids OIDs of sequences with this hash. [out]
    /// @return True if the hash value was found, false otherwise.
    void HashToOids(unsigned hash, vector<int> & oids);
    
    /// Verify internal SeqDB data structures for consistency.
    void Verify();
};

/// Unpack an ambiguous nucleotide sequence.
///
/// This method provides a way to unpack nucleotide sequence data that
/// has been packed in blast database format.  One source of such data
/// is the GetRawSeqAndAmbig() method in the CSeqDBExpert class.  The
/// output format is ncbi8na.
///
/// @param sequence Sequence data in NA2 format with encoded length. [in]
/// @param ambiguities Sequence ambiguities packed in blastdb format. [in]
/// @param result Unpacked sequence in Ncbi NA8 format. [out]

void SeqDB_UnpackAmbiguities(const CTempString & sequence,
                             const CTempString & ambiguities,
                             string            & result);

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDB_EXPERT_HPP


