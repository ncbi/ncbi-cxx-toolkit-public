#ifndef OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP

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


/// @file seqdbimpl.hpp
/// The top level of the private implementation layer for SeqDB.
/// 
/// Defines classes:
///     CSeqDBImplFlush
///     CSeqDBImpl
/// 
/// Implemented for: UNIX, MS-Windows

#include "seqdbvolset.hpp"
#include "seqdbalias.hpp"
#include "seqdboidlist.hpp"
#include <objtools/blast/seqdb_reader/impl/seqdbcol.hpp>
#include "seqdbgimask.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;


/// CSeqDBImplFlush class
/// 
/// This functor like object provides a call back mechanism to return
/// memory holds to the atlas, from lease objects in the other objects
/// under CSeqDBImpl.  Without this class, CSeqDBAtlas and CSeqDBImpl
/// would be codependant, which is somewhat bad style, and creates
/// annoying cyclical dependencies among the include files.

class CSeqDBImplFlush : public CSeqDBFlushCB {
public:
    /// Constructor
    CSeqDBImplFlush()
        : m_Impl(0)
    {
    }
    
    /// Destructor
    virtual ~CSeqDBImplFlush()
    {
    }
    
    /// Specify the implementation layer object.
    ///
    /// This method sets the SeqDB implementation layer object
    /// pointer.  Until this pointer is set, this object will ignore
    /// attempts to flush unused data.  This pointer should not bet
    /// set until object construction is complete enough to permit the
    /// memory lease flushing to happen safely.
    /// 
    /// @param impl
    ///   A pointer to the implementation layer object.
    virtual void SetImpl(class CSeqDBImpl * impl)
    {
        m_Impl = impl;
    }
    
    /// Flush any held memory leases.
    ///
    /// At the beginning of garbage collection, this method is called
    /// to tell the implementation layer to release any held memory
    /// leases.  If the SetImpl() method has not been called, this
    /// method will do nothing.
    virtual void operator()();
    
private:
    /// A pointer to the SeqDB implementation layer.
    CSeqDBImpl * m_Impl;
};


/// Map user algorithm IDs to volume algorithm IDs.
///
/// Each volume has a mapping of algorithm IDs to descriptions.  This
/// class builds a global list of algorithm IDs for all algorithm
/// descriptions in all volumes.

class CSeqDB_IdRemapper {
public:
    /// Constructor.
    CSeqDB_IdRemapper();
    
    /// Register a volume's algorithm definition.
    void AddMapping(int vol_id, int id, const string & desc);
    
    /// Get a list of user (real) IDs available here.
    void GetIdList(vector<int> & algorithms);
    
    /// Is this object populated?
    bool Empty()
    {
        return m_Empty && m_IdToDesc.empty();
    }
    
    /// Is this object populated?
    bool GetDesc(int algorithm_id, string & desc);
    
    /// Is this object populated?
    void SetNotEmpty()
    {
        m_Empty = false;
    }

    /// Build a list of volume algorithm IDs.
    ///
    /// This method takes a list of algorithm IDs and translates them
    /// to volume IDs.  The translation is cached.  The Atlas lock
    /// should be held while calling this method and kept until the
    /// the returned data is no longer needed.
    ///
    /// @param vol_idx The index of the database volume.
    /// @param algo_id A list of (global) algorithms ID.
    /// @return The volume specific algorithm ID.
    int GetVolAlgo(int vol_idx, int algo_id);
    
    /// Translate a real algorithm ID to a volume algorithm ID.
    int RealToVol(int vol_idx, int algo_id);
    
    /// Translate a string algorithm ID to a numeric algorithm ID
    int GetAlgoId(const string & id);
    
private:
    /// Next unassigned synthetic ID.
    int m_NextId;
    
    /// Map of real IDs to descriptions.
    map<int, string> m_IdToDesc;
    
    /// Map of descriptions to real IDs.
    map<string, int> m_DescToId;
    
    /// Map of volume# to map of real id to volume-based id.
    map<int, map<int, int> > m_RealIdToVolumeId;
    
    /// Is this object initialized?
    bool m_Empty;
    
    /// Cached list of real algorithms for BuildVolAlgos.
    int m_CacheRealAlgo;
    
    /// Cached volume index for BuildVolAlgos.
    int m_CacheVolIndex;
    
    /// Cached list of volume algorithms for BuildVolAlgos.
    int m_CacheVolAlgo;
};


/// CSeqDBImpl class
/// 
/// This is the main implementation layer of the CSeqDB class.  This
/// is seperated from that class so that various implementation
/// details of CSeqDB are kept from the public interface.

class CSeqDBImpl {
    /// Import type to allow shorter name.
    typedef TSeqDBAliasFileValues TAliasFileValues;
    
public:
    /// Defines types of database summary information.
    typedef CSeqDB::ESummaryType ESummaryType;
    
    /// Standard Constructor
    ///
    /// This builds a CSeqDBImpl object from the provided parameters,
    /// which correspond to those of the longer CSeqDB constructor.
    ///
    /// @param db_name_list
    ///   A list of database or alias names, seperated by spaces.
    /// @param prot_nucl
    ///   Either kSeqTypeProt for a protein database, or kSeqTypeNucl
    ///   for nucleotide.  These can also be specified as 'p' or 'n'.
    /// @param oid_begin
    ///   Iterator will skip OIDs less than this value.  Only OIDs
    ///   found in the OID lists (if any) will be returned.
    /// @param oid_end
    ///   Iterator will return up to (but not including) this OID.
    /// @param use_mmap
    ///   If kSeqDBMMap is specified (the default), memory mapping is
    ///   attempted.  If kSeqDBNoMMap is specified, memory mapping
    ///   fails, or this platform does not support it, the less
    ///   efficient read and write calls are used instead.
    /// @param gi_list
    ///   If not null, specifies included deflines and OIDs.
    /// @param neg_list
    ///   If not null, specifies excluded deflines and OIDs.
    /// @param idset
    ///   Specifies included or excluded deflines and OIDs.
    CSeqDBImpl(const string       & db_name_list,
               char                 prot_nucl,
               int                  oid_begin,
               int                  oid_end,
               bool                 use_mmap,
               CSeqDBGiList       * gi_list,
               CSeqDBNegativeList * neg_list,
               CSeqDBIdSet          idset);
    
    /// Default Constructor
    /// 
    /// This builds a null version of the CSeqDBImpl object.  It is in
    /// support of the default constructor for the CSeqDBExpert class.
    CSeqDBImpl();
    
    /// Destructor
    ~CSeqDBImpl();
    
    /// Get the sequence length.
    ///
    /// This computes and returns the exact sequence length for the
    /// indicated sequence.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @return
    ///   The length of the sequence in bases.
    int GetSeqLength(int oid) const;
    
    /// Get the approximate sequence length.
    ///
    /// This computes and returns the approximate sequence length for
    /// the indicated sequence.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @return
    ///   The approximate length of the sequence in bases.
    int GetSeqLengthApprox(int oid) const;
    
    /// Get the sequence header data.
    ///
    /// This builds and returns the header data corresponding to the
    /// indicated sequence as a Blast-def-line-set.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @return
    ///   The length of the sequence in bases.
    CRef<CBlast_def_line_set> GetHdr(int oid);
    
    /// Get the sequence type.
    ///
    /// Return an enumerated value indicating which type of sequence
    /// data this instance of SeqDB stores.
    ///
    /// @return
    ///   The type of sequences stored here, either kSeqTypeProt or kSeqTypeNucl.
    char GetSeqType() const;
    
    /// Get gi to taxid map for an OID.
    ///
    /// This finds the TAXIDS associated with a given OID and computes
    /// a mapping from GI to taxid.  This mapping is added to the
    /// map<int,int> provided by the user.  If the "persist" flag is
    /// set to true, the new associations will simply be added to the
    /// map.  If it is false (the default), the map will be cleared
    /// first.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param gi_to_taxid
    ///   A returned mapping from GI to taxid.
    /// @param persist
    ///   If false, the map will be cleared before adding new entries.
    void GetTaxIDs(int             oid,
                   map<int, int> & gi_to_taxid,
                   bool            persist);
    
    /// Get taxids for an OID.
    ///
    /// This finds the TAXIDS associated with a given OID and returns
    /// them in a vector.  If the "persist" flag is set to true, the
    /// new taxids will simply be appended to the vector.  If it is
    /// false (the default), the vector will be cleared first.  One
    /// advantage of this interface over the map<int,int> version is
    /// that the vector interface works with databases with local IDs
    /// but lacking GIs.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param taxids
    ///   A returned list of taxids.
    /// @param persist
    ///   If false, the map will be cleared before adding new entries.
    void GetTaxIDs(int           oid,
                   vector<int> & taxids,
                   bool          persist);
    
    /// Get a CBioseq for a sequence.
    ///
    /// This builds and returns the header and sequence data
    /// corresponding to the indicated sequence as a CBioseq.  If
    /// target_gi is non-null, the header information will be filtered
    /// to only include the defline associated with that gi.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param target_gi
    ///   The target gi to filter the header information by.
    /// @param target_seq_id
    ///   The target seq_id to filter the header information by.
    /// @param seqdata
    ///   If true, sequence data will be provided.
    /// @return
    ///   A CBioseq object corresponding to the sequence.
    CRef<CBioseq> GetBioseq(int oid, 
                            int target_gi, 
                            const CSeq_id * target_seq_id,
                            bool seqdata);
    
    /// Get the sequence data for a sequence.
    ///
    /// Get the raw sequence (strand data).  When done, resources
    /// should be returned with RetSequence.  This data pointed to
    /// by *buffer is in read-only memory (where supported).
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param buffer
    ///   A returned pointer to the data in the sequence.
    /// @return
    ///   The return value is the sequence length (in base pairs or
    ///   residues).  In case of an error, an exception is thrown.
    int GetSequence(int oid, const char ** buffer) const;
    
    /// Get a pointer to a range of sequence data with ambiguities.
    ///
    /// This is like GetAmbigSeq(), but the allocated object should be
    /// deleted by the caller.  This is intended for users who are
    /// going to modify the sequence data, or are going to mix the
    /// data into a container with other data, and who are mixing data
    /// from multiple sources and want to free the data in the same
    /// way.  The fourth parameter should be given one of the values
    /// from EAllocStrategy; the corresponding method should be used
    /// to delete the object.  Note that "delete[]" should be used
    /// instead of "delete"
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param buffer
    ///   A returned pointer to the data in the sequence.
    /// @param nucl_code
    ///   The encoding to use for the returned sequence data.
    /// @param region
    ///   If non-null, the offset range to get.
    /// @param strategy
    ///   Selects which function is used to allocate the buffer.
    /// @return
    ///   The return value is the sequence length (in base pairs or
    ///   residues).  In case of an error, an exception is thrown.
    int GetAmbigSeq(int               oid,
                    char           ** buffer,
                    int               nucl_code,
                    SSeqDBSlice     * region,
                    ESeqDBAllocType   strategy,
                    CSeqDB::TSequenceRanges * masks = NULL ) const;
    
    /// Returns any resources associated with the sequence.
    ///
    /// Calls to GetSequence (but not GetBioseq())
    /// either increment a counter corresponding to a section of the
    /// database where the sequence data lives, or allocate a buffer
    /// to return to the user.  This method decrements that counter or
    /// frees the allocated buffer, so that the memory can be used by
    /// other processes.  Each allocating call should be paired with a
    /// returning call.  Note that this does not apply to GetBioseq(),
    /// or GetHdr(), for example.
    ///
    /// @param buffer
    ///   A pointer to the sequence data to release.
    void RetSequence(const char ** buffer) const;
    
    /// Returns any resources associated with the sequence.
    ///
    /// Calls to GetAmbigSeq (but not GetBioseq())
    /// either increment a counter corresponding to a section of the
    /// database where the sequence data lives, or allocate a buffer
    /// to return to the user.  This method decrements that counter or
    /// frees the allocated buffer, so that the memory can be used by
    /// other processes.  Each allocating call should be paired with a
    /// returning call.  Note that this does not apply to GetBioseq(),
    /// or GetHdr(), for example.
    ///
    /// @param buffer
    ///   A pointer to the sequence data to release.
    void RetAmbigSeq(const char ** buffer) const;
    
    /// Gets a list of sequence identifiers.
    /// 
    /// This returns the list of CSeq_id identifiers associated with
    /// the sequence specified by the given OID.
    ///
    /// @param oid
    ///   The oid of the sequence.
    /// @return
    ///   A list of Seq-id objects for this sequence.
    list< CRef<CSeq_id> > GetSeqIDs(int oid);

    /// Look up for the GI of a sequence
    /// 
    /// This returns the first GI (if any) that identifies the sequence
    /// specified by the given OID, or -1 if no GI is found.
    ///
    /// @param oid
    ///   The oid of the sequence.
    /// @return
    ///   GI or -1.
    int GetSeqGI(int oid);
    
    /// Returns the database title.
    ///
    /// This is usually read from database volumes or alias files.  If
    /// multiple databases were passed to the constructor, this will
    /// be a concatenation of those databases' titles.
    string GetTitle() const;
    
    /// Returns the construction date of the database.
    /// 
    /// This is encoded in the database.  If multiple databases or
    /// multiple volumes were accessed, the first available date will
    /// be used.
    string GetDate() const;
    
    /// Returns the number of sequences available.
    int GetNumSeqs() const;
    
    /// Returns the number of sequences available.
    ///
    /// This may be overridden by the STATS_NSEQ key.
    int GetNumSeqsStats() const;
    
    /// Returns the size of the (possibly sparse) OID range.
    int GetNumOIDs() const;
    
    /// Returns the sum of the lengths of all available sequences.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  It provides an approx value without iterating
    /// over individual sequences for cases when scanning the db is
    /// the only way to determine the exact total length
    Uint8 GetTotalLength() const;
    
    /// Returns the exact sum of the lengths of all available sequences.
    ///
    /// Calling this function may trigger a complete db scan if the
    /// total length of a db cannot be determined without iterating
    /// thorugh the sequences i.e. a db with gi list
    Uint8 GetExactTotalLength();

    /// Returns the sum of the lengths of all available sequences.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  It provides either an exact value or a value
    /// changed in the alias files by the STATS_TOTLEN key.
    Uint8 GetTotalLengthStats() const;
    
    /// Returns the sum of the lengths of all volumes.
    ///
    /// This uses summary information stored in the database volumes
    /// (but not the alias files).  It provides an exact value,
    /// without iterating over individual sequences.  It includes all
    /// OIDs regardless of inclusion by the filtering mechanisms of
    /// the alias files.
    Uint8 GetVolumeLength() const;
    
    /// Returns the length of the largest sequence in the database.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  This might be used to chose buffer sizes.
    int GetMaxLength() const;
    
    /// Returns the length of the smallest sequence in the database.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  This might be used to estimate cutoff scores.
    int GetMinLength() const;
    
    /// Find an included OID, incrementing next_oid if necessary.
    ///
    /// If the specified OID is not included in the set (i.e. the OID
    /// mask), the input parameter is incremented until one is found
    /// that is.  The user will probably want to increment between
    /// calls, if iterating over the db.
    ///
    /// @param next_oid The starting oid, the selected oid [in|out].
    /// @return True if a valid OID was found, false otherwise.
    bool CheckOrFindOID(int & next_oid);
    
    /// Return a chunk of OIDs, and update the OID bookmark.
    /// 
    /// This method allows the caller to iterate over the database by
    /// fetching batches of OIDs.  It will either return a list of OIDs in
    /// a vector, or set a pair of integers to indicate a range of OIDs.
    /// The return value will indicate which technique was used.  The
    /// caller sets the number of OIDs to get by setting the size of the
    /// vector.  If eOidRange is returned, the first included oid is
    /// oid_begin and oid_end is the oid after the last included oid.  If
    /// eOidList is returned, the vector contain the included OIDs, and may
    /// be resized to a smaller value if fewer entries are available (for
    /// the last chunk).  In some cases it may be desireable to have
    /// several concurrent, independent iterations over the same database
    /// object.  If this is required, the caller should specify the address
    /// of an int to the optional parameter oid_state.  This should be
    /// initialized to zero (before the iteration begins) but should
    /// otherwise not be modified by the calling code (except that it can
    /// be reset to zero to restart the iteration).  For the normal case of
    /// one iteration per program, this parameter can be omitted.
    /// @param begin_chunk
    ///   First included oid (if eOidRange is returned).
    /// @param end_chunk
    ///   OID after last included (if eOidRange is returned).
    /// @param oid_size
    ///   Number of OID to retrieve (ignored in MT environment).
    /// @param oid_list
    ///   Empty list to be filled if eOidList is returned.
    /// @param oid_state
    ///   Optional address of a state variable (for concurrent iterations).
    /// @return
    ///   eOidList in enumeration case, or eOidRange in begin/end range case.
    CSeqDB::EOidListType
    GetNextOIDChunk(int         & begin_chunk,
                    int         & end_chunk,
                    int         oid_size,
                    vector<int> & oid_list,
                    int         * oid_state);
    
    /// Restart chunk iteration at the beginning of the database.
    void ResetInternalChunkBookmark();
    
    /// Get list of database names.
    ///
    /// This returns the database name list used at construction.
    /// @return
    ///   List of database names.
    const string & GetDBNameList() const;
    
    /// Get GI list attached to this database.
    ///
    /// This returns the GI list attached to this database, or NULL,
    /// if no GI list was used.  The effects of changing the contents
    /// of this GI list are undefined.  This method only deals with
    /// the GI list passed to the top level CSeqDB constructor; it
    /// does not consider volume GI lists.
    ///
    /// @return A pointer to the attached GI list, or NULL.
    const CSeqDBGiList * GetGiList() const
    {
        return m_UserGiList.GetPointerOrNull();
    }
    
    /// Get IdSet list attached to this database.
    ///
    /// This returns the ID set used to filter this database. If a
    /// CSeqDBGiList or CSeqDBNegativeList was used instead, then an
    /// ID set object will be constructed and returned (and cached
    /// here).  This method only deals with filtering applied to the
    /// top level CSeqDB constructor; it does not consider GI or TI
    /// lists attached from alias files.  If no filtering was used, a
    /// 'blank' list will be returned (an empty negative list).
    ///
    /// @return A pointer to the attached ID set, or NULL.
    CSeqDBIdSet GetIdSet();
    
    /// Set upper limit on memory and mapping slice size.
    /// 
    /// This sets an approximate upper limit on memory used by CSeqDB
    /// for file mappings and large arrays.  This will not be exactly
    /// enforced; the library will prefer to exceed the bound rather
    /// than return an error.  Setting this to a very low value will
    /// cause bad performance.  If this is not set, SeqDB picks a
    /// large value and lowers it if an allocation fails.
    /// 
    /// @param membound Maximum memory to use for file mappings.
    void SetMemoryBound(Uint8 membound)
    {
        CHECK_MARKER();
        m_Atlas.SetMemoryBound(membound);
    }
    
    /// Flush unnecessarily held memory
    ///
    /// This is used by the atlas garbage collection callback - when
    /// gc is running, it should not count references held by the
    /// volset.  Thus, the no-longer-in-action volumes can be flushed
    /// out (currently, idx files are still kept for all volumes).
    /// The atlas should be locked when calling this method.
    void FlushSeqMemory();
    
    /// Translate a PIG to an OID.
    bool PigToOid(int pig, int & oid) const;
    
    /// Translate a PIG to an OID.
    bool OidToPig(int oid, int & pig) const;
    
    /// Translate a TI to an OID.
    bool TiToOid(Int8 ti, int & oid);
    
    /// Translate a GI to an OID.
    bool GiToOid(int gi, int & oid) const;
    
    /// Translate a GI to an OID.
    bool OidToGi(int oid, int & gi);
    
    /// Find OIDs matching the specified string.
    void AccessionToOids(const string & acc,
                         vector<int>  & oids);
    
    /// Translate a CSeq-id to a list of OIDs.
    void SeqidToOids(const CSeq_id & seqid, vector<int> & oids, bool multi);
    
    /// Find the OID corresponding to the offset given in residues,
    /// into the database as a whole.
    int GetOidAtOffset(int first_seq, Uint8 residue) const;
    
    /// Find volume paths
    ///
    /// Find the base names of volumes/aliases.  This version of this
    /// method builds an alias hierarchy (which should be much faster
    /// than constructing an entire CSeqDBImpl object), and returns
    /// the resolved volume file base names from that hierarchy.
    ///
    /// @param dbname
    ///   The input name of the database
    /// @param prot_nucl
    ///   Indicates whether the database is protein or nucleotide
    /// @param paths
    ///   The returned set of resolved database volume path names
    /// @param alias_paths
    ///   The returned set of resolved database alias path names
    /// @param recursive
    ///   If true, search will traverse the full alias node tree
    /// @param expand_links
    ///   If true, search will expand soft links
    static void
    FindVolumePaths(const string   & dbname,
                    char             prot_nucl,
                    vector<string> & paths,
                    vector<string> * alias_paths,
                    bool             recursive,
                    bool             expand_links);
    
    /// Find volume paths
    ///
    /// Find the base names of all volumes.  This method version of
    /// this method returns the existing internal versions of the
    /// resolved volume file base names from that hierarchy.
    ///
    /// @param paths
    ///   The returned set of resolved database path names
    /// @param recursive
    ///   If true the search will traverse the full alias node tree
    void FindVolumePaths(vector<string> & paths, bool recursive) const;
    
    /// Set Iteration Range
    ///
    /// This method sets the iteration range as a pair of OIDs.
    /// Iteration proceeds from begin, up to but not including end.
    /// End will be adjusted to the number of OIDs in the case that it
    /// is 0, negative, or greater than the number of OIDs.
    ///
    /// @param oid_begin
    ///   Iterator will skip OIDs less than this value.  Only OIDs
    ///   found in the OID lists (if any) will be returned.
    /// @param oid_end
    ///   Iterator will return up to (but not including) this OID.
    void SetIterationRange(int oid_begin, int oid_end);
    
    /// Get Name/Value Data From Alias Files
    ///
    /// SeqDB treats each alias file as a map from a variable name to
    /// a value.  This method will return a map from the basename of
    /// the filename of each alias file, to a mapping from variable
    /// name to value for each entry in that file.  For example, the
    /// value of the "DBLIST" entry in the "wgs.nal" file would be
    /// values["wgs"]["DBLIST"].  The lines returned have been
    /// processed somewhat by SeqDB, including normalizing tabs to
    /// whitespace, trimming leading and trailing whitespace, and
    /// removal of comments and other non-value lines.  Care should be
    /// taken when using the values returned by this method.  SeqDB
    /// uses an internal "virtual" alias file entry to aggregate the
    /// values passed into SeqDB by the user.  This mapping uses a
    /// filename of "-" and contains a single entry mapping "DBLIST"
    /// to SeqDB's database name input.  This entry is the root of the
    /// alias file inclusion tree.  Also note that alias files that
    /// appear in several places in the alias file inclusion tree only
    /// have one entry in the returned map (and are only parsed once
    /// by SeqDB).
    /// 
    /// @param afv
    ///   The alias file values will be returned here.
    void GetAliasFileValues(TAliasFileValues & afv);
    
    /// Verify consistency of the memory management (atlas) layer.
    void Verify()
    {
        m_Atlas.Verify(false);
    }
    
    /// Get taxonomy information
    /// 
    /// This method returns the taxonomy information for a single
    /// given taxid.  This information does not vary with sequence
    /// type (protein vs. nucleotide) and should be the same,
    /// regardless of which blast database is loaded.
    /// 
    /// @param taxid
    ///   An integer identifying the taxid to fetch.
    /// @param info
    ///   A structure containing taxonomic description strings.
    static void GetTaxInfo(int taxid, SSeqDBTaxInfo & info);
    
    /// Returns the sum of the sequence lengths.
    ///
    /// This uses summary information and iteration to compute the
    /// total length and number of sequences for some subset of the
    /// database.  If eUnfilteredAll is specified, it uses information
    /// from the underlying database volumes, without filtering.  If
    /// eFilteredAll is specified, all of the included sequences are
    /// used, for all possible OIDs.  If eFilteredRange is specified,
    /// the returned values correspond to the sum over only those
    /// sequences that survive filtering, and are within the iteration
    /// range.  In some cases, the results can be computed in constant
    /// time; other cases require iteration proportional to the length
    /// of the database or the included OID range (see
    /// SetIterationRange()).
    ///
    /// @param sumtype
    ///   Specifies the subset of sequences to include.
    /// @param oid_count
    ///   The returned number of included OIDs.
    /// @param total_length
    ///   The returned sum of included sequence lengths.
    /// @param use_approx
    ///   Whether to use approximate lengths for nucleotide.
    void GetTotals(ESummaryType   sumtype,
                   int          * oid_count,
                   Uint8        * total_length,
                   bool           use_approx);
    
    /// Fetch data as a CSeq_data object.
    ///
    /// All or part of the sequence is fetched in a CSeq_data object.
    /// The portion of the sequence returned is specified by begin and
    /// end.  An exception will be thrown if begin is greater than or
    /// equal to end, or if end is greater than or equal to the length
    /// of the sequence.  Begin and end should be specified in bases;
    /// a range like (0,1) specifies 1 base, not 2.  Nucleotide data
    /// will always be returned in ncbi4na format.
    ///
    /// @param oid    Specifies the sequence to fetch.
    /// @param begin  Specifies the start of the data to get. [in]
    /// @param end    Specifies the end of the data to get.   [in]
    /// @return The sequence data as a Seq-data object.
    CRef<CSeq_data> GetSeqData(int     oid,
                               TSeqPos begin,
                               TSeqPos end) const;
    
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
    /// made about what data they will contain.  If the keep_current
    /// flag is true, the range will be added to existing ranges.  If
    /// false, existing ranges will be flushed and replaced by new
    /// ranges.  To remove ranges, call this method with an empty list
    /// of ranges; future calls will return the complete sequence.
    ///
    /// If the cache_data flag is provided, data for this sequence
    /// will be kept for the duration of SeqDB's lifetime.  To disable
    /// caching (and flush cached data) for this sequence, call the
    /// method again, but specify cache_data to be false.
    ///
    /// @param oid           OID of the sequence.
    /// @param offset_ranges Ranges of sequence data to return.
    /// @param append_ranges Append new ranges to existing list.
    /// @param cache_data    Keep sequence data for future callers.
    void SetOffsetRanges(int                oid,
                         const TRangeList & offset_ranges,
                         bool               append_ranges,
                         bool               cache_data);
    
    /// Flush all offset ranges cached
    void FlushOffsetRangeCache();

    /// Set global default memory bound for SeqDB.
    ///
    /// The memory bound for individual SeqDB objects can be adjusted
    /// with SetMemoryBound(), but this cannot be called until after
    /// the object is constructed.  Until that time, the value used is
    /// set from a global default.  This method allows that global
    /// default value to be changed.  Any SeqDB object constructed
    /// after this method is called will use this value as the initial
    /// memory bound.  If zero is specified, an appropriate default
    /// will be selected based on system information.
    static void SetDefaultMemoryBound(Uint8 bytes);
    
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
    void HashToOids(unsigned hash, vector<int> & oids);
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// List columns titles found in this database.
    ///
    /// This returns a list of the column titles of all user created
    /// (and system generated) columns found in any of this database's
    /// volumes.  Column titles appearing in more than one volume are
    /// only listed here once.
    ///
    /// @param titles Column titles are returned here. [out]
    void ListColumns(vector<string> & titles);
    
    /// Get an ID number for a given column title.
    ///
    /// For a given column title, this returns an ID that can be used
    /// to access that column in the future.  The returned ID number
    /// is specific to this instance of SeqDB.  If the database does
    /// not have a column with this name, -1 will be returned.
    ///
    /// @param title Column title to search for. [in]
    /// @return Column ID number for this column, or -1. [in]
    int GetColumnId(const string & title);
    
    /// Get all metadata for the specified column.
    ///
    /// Columns may contain user-defined metadata as a list of
    /// key-value pairs.  For the specified column, this returns that
    /// column's metadata in the provided map.  If multiple volumes
    /// are present, and they define contradictory meta data (this is
    /// more common when multiple databases are opened at once), this
    /// method returns the first value it finds for each metadata key.
    /// If this is unsatisfactory, the two-argument version of this
    /// method may be used to get more precise values for specific
    /// volumes.
    /// 
    /// @param column_id The column id from GetColumnId. [in]
    /// @return The map of metadata for this column. [out]
    const map<string,string> & GetColumnMetaData(int column_id);
    
    /// Get all metadata for the specified column.
    ///
    /// Columns may contain user-defined metadata as a list of
    /// key-value pairs.  For the specified database volume and column
    /// id, this returns that column's metadata (as defined for that
    /// volume) in the provided map.  The volume name should match
    /// the string returned by FindVolumePaths(vector<string>&).
    /// 
    /// @param column_id The column id from GetColumnId. [in]
    /// @param volname   The volume to get metadata for. [in]
    /// @return The map of metadata for this column + volume. [out]
    const map<string,string> &
    GetColumnMetaData(int            column_id,
                      const string & volname);
    
    /// Get all metadata for the specified column.
    ///
    /// Columns can contain user-defined metadata as a list of
    /// key-value pairs.  For the specified column, this returns all
    /// of that column's metadata in the provided map.  If there are
    /// multiple volumes, and those volumes define contradictory meta
    /// data (this is more common if multiple databases are opened at
    /// once), this method will return the first value it finds for
    /// each metadata key.  In these cases, GetVolumeColumnProperties
    /// may be used instead.
    ///
    /// @param column_id The column id from GetColumnId. [in]
    /// @return The value corresponding to the specified key. [out]
    string GetColumnValue(int column_id, const string & key);
    
    /// Fetch the data blob for the given column and oid.
    /// @param col_id The column to fetch data from. [in]
    /// @param oid    The OID of the blob. [in]
    /// @param keep   Whether to add a hold on this blob's memory. [in]
    /// @param blob   The data will be returned here. [out]
    void GetColumnBlob(int col_id, int oid, bool keep, CBlastDbBlob & blob);
    
    // Mask data support.
    
    /// Get a list of algorithm IDs for which mask data exists.
    ///
    /// Multiple sources of masking data may be used when building
    /// blast databases.  This method retrieves a list of the IDs used
    /// to identify those types of filtering data to SeqDB.  If the
    /// blast database volumes used by this instance of SeqDB were
    /// built with conflicting algorithm ID definitions, SeqDB will
    /// resolve the conflicts by renumbering some of the conflicting
    /// descriptions.  For this reason, the IDs reported here may not
    /// match what was given to WriteDB when the database was created.
    ///
    /// @param algorithms List of algorithm ids. [out]
    void GetAvailableMaskAlgorithms(vector<int> & algorithms);

    /// Get the numeric ID for a algorithm name
    int GetMaskAlgorithmId(const string &algo_name);

    /// Returns a formatted string with the list of available masking
    /// algorithms in this database for display purposes (i.e.: help)
    string GetAvailableMaskAlgorithmDescriptions();
    
    /// Get information about one type of masking available here.
    ///
    /// For a given algorithm_id, this method fetches information
    /// describing the basic algorithm used, as well as options passed
    /// to that algorithm to generate the data stored here.  Each
    /// sequence in the database can provide sequence masking data
    /// from one or more sources.  There can also be multiple types of
    /// masking data from the same algorithm (such as DUST), but
    /// generated with different sets of input parameters.
    /// 
    /// @param algorithm_id The ID as from GetAvailableMaskAlgorithms. [in]
    /// @param program Program used to produce this masking data (SEG, DUST,
    /// etc) [out]
    /// @param program_name string representation of program argument [out]
    /// @param algo_opts Describes options passed to `program'. [out]
    void GetMaskAlgorithmDetails(int                 algorithm_id,
                                 string            & program,
                                 string            & program_name,
                                 string            & algo_opts);
    
    
    /// Get masked ranges of a sequence.
    ///
    /// For the provided OID and list of algorithm IDs, this method
    /// gets a list of masked areas of those sequences.  The list of
    /// masked areas is returned via the ranges parameter.  If the
    /// `inverted' parameter is true, the list of ranges returned is
    /// actually the list of non-maksed (rather than masked) ranges.
    ///
    /// @param oid The ordinal ID of the sequence. [in]
    /// @param algo_id The algorithm ID to get data for. [in]
    /// @param inverted If true, return a list of included ranges. [in]
    /// @param ranges The list of sequence offset ranges. [out]
    void GetMaskData(int                         oid,
                     int                         algo_id,
                     CSeqDB::TSequenceRanges   & ranges);
#endif

    /// Invoke the garbage collector to free up memory
    void GarbageCollect(void);
    
    /// Set number of threads
    void SetNumberOfThreads(int num_threads);

    /// Retrieve the slice size used in internal mmap
    Int8 GetSliceSize() const{
        return m_Atlas.GetSliceSize();
    }

private:
    CLASS_MARKER_FIELD("IMPL")
    
    /// Adjust string length to offset of first embedded NUL byte.
    ///
    /// This is a work-around for bad data in the database; probably
    /// the fault of formatdb.  The problem is that the database date
    /// field has an incorrect length - possibly a general problem
    /// with string handling in formatdb?  In any case, this method
    /// trims a string to the minimum of its length and the position
    /// of the first NULL.  This technique will not work if the date
    /// field is not null terminated, but apparently it usually or
    /// always is, in spite of the length bug.
    ///  
    /// @param s
    ///   A string which may contain a NUL byte.
    /// @return
    ///   The non-NUL-containing prefix of the input string.
    string x_FixString(const string & s) const;
    
    /// Returns the number of sequences available.
    int x_GetNumSeqs() const;
    
    /// Returns the number of sequences available.
    int x_GetNumSeqsStats() const;
    
    /// Returns the size of the (possibly sparse) OID range.
    int x_GetNumOIDs() const;
    
    /// Returns the sum of the lengths of all available sequences.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  It provides an exact value, without iterating
    /// over individual sequences.  This private version uses the
    /// alias files, and is used to populate a field that caches the
    /// value; the corresponding public method uses that cached value.
    Uint8 x_GetTotalLength() const;
    
    /// Returns the sum of the lengths of all available sequences.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  It provides an exact value, without iterating
    /// over individual sequences.  This private version uses the
    /// alias files, and is used to populate a field that caches the
    /// value; the corresponding public method uses that cached value.
    /// It works just like x_GetTotalLengthStats() except that it uses
    /// the STATS_TOTLEN alias key.
    Uint8 x_GetTotalLengthStats() const;
    
    /// Returns the sum of the lengths of all volumes.
    ///
    /// This uses summary information stored in the database volumes
    /// (but not the alias files).  It provides an exact value,
    /// without iterating over individual sequences.  It includes all
    /// OIDs regardless of inclusion by the filtering mechanisms of
    /// the alias files.  This private version uses the alias files,
    /// and is used to populate a field that caches the value; the
    /// corresponding public method uses that cached value.
    Uint8 x_GetVolumeLength() const;


    /// Returns the longest sequence lengths of all volumes.
    int x_GetMaxLength() const;
    
    /// Returns the shortest sequence lengths of all volumes.
    int x_GetMinLength() const;
    
    /// Build the OID list
    ///
    /// OID list setup is done once, but not until needed.
    /// 
    /// @param locked
    ///   The lock hold object for this thread.
    void x_GetOidList(CSeqDBLockHold & locked);
    
    /// Get the next included oid
    ///
    /// This method checks if the OID list has the specified OID, and
    /// if not, finds the next one it does have, if any.
    /// 
    /// @param next_oid
    ///   The next oid to check and to return to the user.
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   true if an OID was found, false otherwise.
    bool x_CheckOrFindOID(int & next_oid, CSeqDBLockHold & locked);
    
    /// Get the sequence header data.
    ///
    /// This builds and returns the header data corresponding to the
    /// indicated sequence as a Blast-def-line-set.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   The length of the sequence in bases.
    CRef<CBlast_def_line_set> x_GetHdr(int oid, CSeqDBLockHold & locked);
    
    /// Look up for the GI of a sequence
    /// 
    /// This returns the first GI (if any) that identifies the sequence
    /// specified by the given OID, or -1 if no GI is found.
    ///
    /// @param oid
    ///   The oid of the sequence.
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   GI or -1.
    int x_GetSeqGI(int oid, CSeqDBLockHold & locked);
    
    /// Compute totals via iteration
    ///
    /// This method loops over the database, computing the total
    /// number of (included) sequences and bases from the OID mask and
    /// sequence lengths.  It stores the computed values in m_NumSeqs
    /// and m_TotalLength.
    ///
    /// @param approx
    ///   Specify true if approximate lengths are okay.
    /// @param seq_count
    ///   Returned count of included OIDs in this range.
    /// @param base_count
    ///   Returned sum of lengths of included sequences.
    /// @param max_count
    ///   Returned longest of included sequences.
    /// @param min_count
    ///   Returned shortest of included sequences.
    /// @param locked
    ///   The lock hold object for this thread.
    void x_ScanTotals(bool             approx,
                      int            * seq_count,
                      Uint8          * base_count, 
                      int            * max_count, 
                      int            * min_count, 
                      CSeqDBLockHold & locked);
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Get the Column ID for the column with the specified title.
    /// @param title  The column title.
    /// @param locked The lock hold object for this thread. [in]
    /// @return A positive ID (if found), a negative code otherwise.
    int x_GetColumnId(const string   & title,
                      CSeqDBLockHold & locked);
    
    /// Open the mask data column (if necessary) and return its id.
    /// @param locked The lock hold object for this thread. [in]
    /// @return The column id of the mask data column.
    int x_GetMaskDataColumn(CSeqDBLockHold & locked);
    
    /// Get a list of algorithm IDs for which mask data exists.
    ///
    /// Multiple sources of masking data may be used when building
    /// blast databases.  This method retrieves a list of the IDs used
    /// to identify those types of filtering data to SeqDB.  If the
    /// blast database volumes used by this instance of SeqDB were
    /// built with conflicting algorithm ID definitions, SeqDB will
    /// resolve the conflicts by renumbering some of the conflicting
    /// descriptions.  For this reason, the IDs reported here may not
    /// match what was given to WriteDB when the database was created.
    ///
    /// @param locked The lock hold object for this thread. [in]
    void x_BuildMaskAlgorithmList(CSeqDBLockHold & locked);
#endif
    
    /// Get the sequence length.
    ///
    /// This computes and returns the exact sequence length for the
    /// indicated sequence.
    ///
    /// @param oid The ordinal id of the sequence. [in]
    /// @param locked The lock holder object for this thread. [in]
    /// @return The length of the sequence in bases.
    int x_GetSeqLength(int oid, CSeqDBLockHold & locked) const;
    
    /// Get local cache ID for current thread.
    ///
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   The mapped local cache ID
    int x_GetCacheID(CSeqDBLockHold &locked) const {
        int threadID = CThread::GetSelf();

        if (m_NextCacheID < 0) 
            return m_CacheID[threadID];

        int retval;
        m_Atlas.Lock(locked);

        if (m_CacheID.find(threadID) == m_CacheID.end()) {
            m_CacheID[threadID] = m_NextCacheID++;
        }
        retval = m_CacheID[threadID];
        if (m_NextCacheID == m_NumThreads) {
            m_NextCacheID = -1;
        }

        m_Atlas.Unlock(locked);
        return retval;
    }

    /// This callback functor allows the atlas code flush any cached
    /// region holds prior to garbage collection.
    CSeqDBImplFlush m_FlushCB;
    
    /// Memory management layer guard (RIIA) object.
    CSeqDBAtlasHolder m_AtlasHolder;
    
    /// Reference to memory management layer.
    CSeqDBAtlas & m_Atlas;
    
    /// The list of database names provided to the constructor.
    string m_DBNames;
    
    /// Alias node hierarchy management object.
    CSeqDBAliasFile m_Aliases;
    
    /// Set of volumes used by this database instance.
    CSeqDBVolSet m_VolSet;
    
    /// The list of included OIDs (construction is deferred).
    mutable CRef<CSeqDBOIDList> m_OIDList;
    
    /// Taxonomic information.
    CRef<CSeqDBTaxInfo> m_TaxInfo;
    
    /// Starting OID as provided to the constructor.
    int m_RestrictBegin;
    
    /// Ending OID as provided to the constructor.
    int m_RestrictEnd;
    
    /// Mutex which synchronizes access to the OID list.
    CFastMutex m_OIDLock;
    
    /// "Bookmark" for multithreaded chunk-type OID iteration.
    int m_NextChunkOID;
    
    /// Number of sequences in the overall database.
    int m_NumSeqs;
    
    /// Number of sequences in the overall database.
    int m_NumSeqsStats;
    
    /// Size of databases OID range.
    int m_NumOIDs;
    
    /// Total length of database (in bases).
    Uint8 m_TotalLength;
    
    /// Total length of database (in bases).
    Uint8 m_ExactTotalLength;

    /// Total length of database (in bases).
    Uint8 m_TotalLengthStats;
    
    /// Total length of all database volumes combined (in bases).
    Uint8 m_VolumeLength;
    
    /// Longest database sequence
    int m_MaxLength;

    /// Shortest database sequence
    int m_MinLength;

    /// Type of sequences used by this instance.
    char m_SeqType;
    
    /// True if OID list setup is done (or was not required).
    mutable bool m_OidListSetup;
    
    /// The User GI list for the entire CSeqDB object.
    mutable CRef<CSeqDBGiList> m_UserGiList;
    
    /// The Negative ID list for the entire CSeqDB object.
    mutable CRef<CSeqDBNegativeList> m_NegativeList;
    
    /// The positive or negative ID list for the entire CSeqDB object.
    CSeqDBIdSet m_IdSet;
    
    /// True if this configuration cannot deduce totals without a scan.
    bool m_NeedTotalsScan;
    
    /// Cached most recent date string for GetDate().
    mutable string m_Date;
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Map assigned global column IDs to column information.
    vector< CRef<CSeqDB_ColumnEntry> > m_ColumnInfo;
#endif
    
    /// Map string column titles to global column IDs.
    map<string, int> m_ColumnTitleMap;
    
    /// Status of a database column title.
    enum EColumnStatus {
        /// This column is not heard of yet.
        kUnknownTitle = -1,
        
        /// This column does not exist (we checked).
        kColumnNotFound = -2
    };
    
    /// Which type of masks are we using?
    bool m_UseGiMask;
   
    /// Gi-based mask.
    CRef<CSeqDBGiMask> m_GiMask;

    /// Column ID for mask data column.
    int m_MaskDataColumn;
    
    /// Algorithm ID mapping.
    CSeqDB_IdRemapper m_AlgorithmIds;
    
    /// number of thread clients
    int m_NumThreads;

    /// mapping thread ID to storage ID
    mutable std::map<int, int> m_CacheID;
    mutable int m_NextCacheID;

    /// Structure to keep sequence retrieval results
    struct SSeqRes {
        int length;   // length of the sequence
        const char* address;// mapped memory start
    };

    /// Structure to buffer multiple TSeqRes
    struct SSeqResBuffer {
        int oid_start;    // starting oid for this buffer
        int checked_out;  // number of sequences checked out
        vector<SSeqRes> results;
        SSeqResBuffer() : oid_start(0), checked_out(0) {
            results.reserve(2 << 20);
        }
    };

    /// Cached sequences.
    mutable vector<SSeqResBuffer *> m_CachedSeqs;

    /// Fill up the buffer
    void x_FillSeqBuffer(SSeqResBuffer * buffer, int oid, CSeqDBLockHold &locked) const;

    /// Get sequence from buffer
    int x_GetSeqBuffer(SSeqResBuffer * buffer, int oid, const char ** seq) const;

    /// Return sequence to buffer
    void x_RetSeqBuffer(SSeqResBuffer * buffer, CSeqDBLockHold & locked) const;

};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP

