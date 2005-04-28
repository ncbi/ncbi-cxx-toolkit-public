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


/// CSeqDBImpl class
/// 
/// This is the main implementation layer of the CSeqDB class.  This
/// is seperated from that class so that various implementation
/// details of CSeqDB are kept from the public interface.

class CSeqDBImpl {
public:
    /// Numeric type that can span all OIDs for one instance of SeqDB.
    typedef int TOID;
    
    /// Constructor
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
    ///   If not null, will be used to filter deflines and OIDs.
    CSeqDBImpl(const string & db_name_list,
               char           prot_nucl,
               int            oid_begin,
               int            oid_end,
               bool           use_mmap,
               CSeqDBGiList * gi_list);
    
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
    CRef<CBlast_def_line_set> GetHdr(int oid) const;
    
    /// Get the sequence type.
    ///
    /// Return an enumerated value indicating which type of sequence
    /// data this instance of SeqDB stores.
    ///
    /// @return
    ///   The type of sequences stored here, either kSeqTypeProt or kSeqTypeNucl.
    char GetSeqType() const;
    
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
    /// @return
    ///   A CBioseq object corresponding to the sequence.
    CRef<CBioseq> GetBioseq(int oid, int target_gi) const;
    
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
    
    /// Get a pointer to sequence data with embedded ambiguities.
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
    /// @param strategy
    ///   Selects which function is used to allocate the buffer.
    /// @return
    ///   The return value is the sequence length (in base pairs or
    ///   residues).  In case of an error, an exception is thrown.
    int GetAmbigSeq(int             oid,
                      char         ** buffer,
                      int             nucl_code,
                      ESeqDBAllocType strategy) const;
    
    /// Returns any resources associated with the sequence.
    ///
    /// Calls to GetSequence and GetAmbigSeq (but not GetBioseq())
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
    
    /// Gets a list of sequence identifiers.
    /// 
    /// This returns the list of CSeq_id identifiers associated with
    /// the sequence specified by the given OID.
    ///
    /// @param oid
    ///   The oid of the sequence.
    /// @return
    ///   A list of Seq-id objects for this sequence.
    list< CRef<CSeq_id> > GetSeqIDs(int oid) const;
    
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
    
    /// Returns the size of the (possibly sparse) OID range.
    int GetNumOIDs() const;
    
    /// Returns the sum of the lengths of all available sequences.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  It provides an exact value, without iterating
    /// over individual sequences.
    Uint8 GetTotalLength() const;
    
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
    
    /// Find an included OID, incrementing next_oid if necessary.
    ///
    /// If the specified OID is not included in the set (i.e. the OID
    /// mask), the input parameter is incremented until one is found
    /// that is.  The user will probably want to increment between
    /// calls, if iterating over the db.
    ///
    /// @return
    ///   True if a valid OID was found, false otherwise.
    bool CheckOrFindOID(int & next_oid) const;
    
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
    /// @param oid_list
    ///   List of oids (if eOidList is returned).  Set size before call.
    /// @param oid_state
    ///   Optional address of a state variable (for concurrent iterations).
    /// @return
    ///   eOidList in enumeration case, or eOidRange in begin/end range case.
    CSeqDB::EOidListType
    GetNextOIDChunk(TOID         & begin_chunk,
                    TOID         & end_chunk,
                    vector<TOID> & oid_list,
                    int          * oid_state);
    
    /// Get list of database names.
    ///
    /// This returns the database name list used at construction.
    /// @return
    ///   List of database names.
    const string & GetDBNameList() const;
    
    /// Set upper limit on memory and mapping slice size.
    /// 
    /// This sets an approximate upper limit on memory used by CSeqDB.
    /// This will not be exactly enforced, and the library will prefer
    /// to exceed the bound if necessary rather than return an error.
    /// Setting this to a very low value will probably cause bad
    /// performance.
    void SetMemoryBound(Uint8 membound, Uint8 slicesize)
    {
        CHECK_MARKER();
        m_Atlas.SetMemoryBound(membound, slicesize);
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
    
    /// Translate a GI to an OID.
    bool GiToOid(int gi, int & oid) const;
    
    /// Translate a GI to an OID.
    bool OidToGi(int oid, int & gi) const;
    
    /// Find OIDs matching the specified string.
    void AccessionToOids(const string & acc, vector<TOID> & oids) const;
    
    /// Translate a CSeq-id to a list of OIDs.
    void SeqidToOids(const CSeq_id & seqid, vector<TOID> & oids) const;
    
    /// Find the OID corresponding to the offset given in residues,
    /// into the database as a whole.
    int GetOidAtOffset(int first_seq, Uint8 residue) const;
    
    /// Find volume paths
    ///
    /// Find the base names of all volumes.  This version of this
    /// method builds an alias hierarchy (which should be much faster
    /// than constructing an entire CSeqDBImpl object), and returns
    /// the resolved volume file base names from that hierarchy.
    ///
    /// @param dbname
    ///   The input name of the database
    /// @param prot_nucl
    ///   Indicates whether the database is protein or nucleotide
    /// @param paths
    ///   The returned set of resolved database path names
    static void
    FindVolumePaths(const string   & dbname,
                    char             prot_nucl,
                    vector<string> & paths);
    
    /// Find volume paths
    ///
    /// Find the base names of all volumes.  This method version of
    /// this method returns the existing internal versions of the
    /// resolved volume file base names from that hierarchy.
    ///
    /// @param paths
    ///   The returned set of resolved database path names
    void FindVolumePaths(vector<string> & paths) const;
    
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
    
private:
    CLASS_MARKER_FIELD("IMPL");
    
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
    
    /// Build the OID list
    ///
    /// OID list setup is done once, but not until needed.
    /// 
    /// @param locked
    ///   The lock hold object for this thread.
    void x_GetOidList(CSeqDBLockHold & locked) const;
    
    /// Get the next included oid
    ///
    /// This method checks if the OID list has the specified OID, and
    /// if not, finds the next one it does have, if any.
    /// 
    /// @param next_oid
    ///   The next oid to check and to return to the user.
    /// @param locked
    ///   The lock hold object for this thread.
    bool x_CheckOrFindOID(int & next_oid, CSeqDBLockHold & locked) const;
    
    /// This callback functor allows the atlas code flush any cached
    /// region holds prior to garbage collection.
    CSeqDBImplFlush m_FlushCB;
    
    /// Memory management layer.
    mutable CSeqDBAtlas m_Atlas;
    
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
    
    /// Size of databases OID range.
    int m_NumOIDs;
    
    /// Total length of database (in bases).
    Uint8 m_TotalLength;
    
    /// Total length of all database volumes combined (in bases).
    Uint8 m_VolumeLength;
    
    /// Type of sequences used by this instance.
    char m_SeqType;
    
    /// True if OID list setup is done (or was not required).
    mutable bool m_OidListSetup;
    
    /// The User GI list for the entire CSeqDB object.
    mutable CRef<CSeqDBGiList> m_UserGiList;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP

