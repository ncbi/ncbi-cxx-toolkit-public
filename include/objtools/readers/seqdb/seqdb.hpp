#ifndef OBJTOOLS_READERS_SEQDB__SEQDB_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDB_HPP

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

/// @file seqdb.hpp
/// Defines BLAST database access classes.
///
/// Defines classes:
///     CSeqDB
///     CSeqDBSequence
///
/// Implemented for: UNIX, MS-Windows


#include <objtools/readers/seqdb/seqdbcommon.hpp>
#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE

/// Include definitions from the objects namespace.
USING_SCOPE(objects);


/// Forward declaration of CSeqDB class
class CSeqDB;


/// CSeqDBIter
/// 
/// Small class to iterate over a seqdb database.
/// 
/// This serves something of the same role for a CSeqDB object that a
/// vector iterator might serve in the standard template library.

class NCBI_XOBJREAD_EXPORT CSeqDBIter {
public:
    /// Destructor
    virtual ~CSeqDBIter()
    {
        x_RetSeq();
    }
    
    /// Increment operator
    /// 
    /// Returns the currently held sequence and gets pointers to the
    /// next sequence.
    CSeqDBIter & operator++();
    
    /// Get the OID of the currently held sequence.
    int GetOID()
    {
        return m_OID;
    }
    
    /// Get the sequence data for the currently held sequence.
    const char * GetData()
    {
        return m_Data;
    }
    
    /// Get the length (in base pairs) of the currently held sequence.
    int GetLength()
    {
        return m_Length;
    }
    
    /// Returns true if the iterator points to a valid sequence.
    DECLARE_OPERATOR_BOOL(m_Length != -1);
    
    /// Construct one iterator from another.
    CSeqDBIter(const CSeqDBIter &);
    
    /// Copy one iterator to another.
    CSeqDBIter & operator =(const CSeqDBIter &);
    
private:
    /// Get data pointer and length for the current sequence.
    inline void x_GetSeq();
    
    /// Release hold on current sequence.
    inline void x_RetSeq();
    
    /// CSeqDB is a friend so it alone can create objects of this type.
    friend class CSeqDB;
    
    /// Build an iterator (called only from CSeqDB).
    CSeqDBIter(const CSeqDB *, int oid);
    
    /// The CSeqDB object which this object iterates over.
    const CSeqDB     * m_DB;
    
    /// The OID this iterator is currently accessing.
    int                m_OID;
    
    /// The sequence data for this OID.
    const char       * m_Data;
    
    /// The length of this OID.
    int                m_Length;
};


/// Forward declaration of ISeqDBGiList interface.
class CSeqDBGiList;


/// CSeqDB
///
/// User interface class for blast databases.
///
/// This class provides the top-level interface class for BLAST
/// database users.  It defines access to the database component by
/// calling methods on objects which represent the various database
/// files, such as the index, header, sequence, and alias files.

class NCBI_XOBJREAD_EXPORT CSeqDB : public CObject {
public:
    /// Indicates how block of OIDs was returned.
    enum EOidListType {
        eOidList,
        eOidRange
    };
    
    /// Sequence types (eUnknown tries protein, then nucleotide).
    enum ESeqType {
        eProtein,
        eNucleotide,
        eUnknown
    };
    
    /// Sequence type accepted and returned for OID indexes.
    typedef int TOID;
    
    /// Sequence type accepted and returned for PIG indexes.
    typedef int TPIG;
    
    /// Sequence type accepted and returned for GI indexes.
    typedef int TGI;
    
    /// @deprecated
    /// Short Constructor
    /// 
    /// This version of the constructor assumes memory mapping and
    /// that the entire possible OID range will be included.  [This
    /// version is obsolete, because the sequence type is specified as
    /// a character; eventually only the ESeqType version will exist.]
    /// 
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces.
    /// @param seqtype
    ///   Either kSeqTypeProt for a protein database, kSeqTypeNucl for
    ///   nucleotide, or kSeqTypeUnkn to ask CSeqDB to try each one.
    ///   These can also be specified as 'p', 'n', or '-'.
    CSeqDB(const string & dbname, char seqtype);
    
    /// Short Constructor
    /// 
    /// This version of the constructor assumes memory mapping and
    /// that the entire possible OID range will be included.
    /// 
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces
    /// @param seqtype
    ///   Either eProtein, eNucleotide, or eUnknown
    CSeqDB(const string & dbname, ESeqType seqtype);
    
    /// @deprecated
    /// Constructor with MMap Flag and OID Range.
    ///
    /// If the oid_end value is specified as zero, or as a value
    /// larger than the number of OIDs, it will be adjusted to the
    /// number of OIDs in the database.  Specifying 0,0 for the start
    /// and end will cause inclusion of the entire database.  [This
    /// version of the constructor is obsolete because the sequence
    /// type is specified as a character; eventually only the ESeqType
    /// version will exist.]
    /// 
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces.
    /// @param seqtype
    ///   Either kSeqTypeProt for a protein database, or kSeqTypeNucl
    ///   for nucleotide.  These can also be specified as 'p' or 'n'.
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
    CSeqDB(const string & dbname,
           char           seqtype,
           int            oid_begin,
           int            oid_end,
           bool           use_mmap);
    
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
    ///   Either kSeqTypeProt for a protein database, or kSeqTypeNucl
    ///   for nucleotide.  These can also be specified as 'p' or 'n'.
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
    CSeqDB(const string & dbname,
           ESeqType       seqtype,
           int            oid_begin,
           int            oid_end,
           bool           use_mmap,
           CSeqDBGiList * gi_list = 0);
    
    /// Destructor.
    ///
    /// This will return resources acquired by this object, including
    /// any gotten by the GetSequence() call, whether or not they have
    /// been returned by RetSequence().
    ~CSeqDB();
    
    
    /// Returns the sequence length in base pairs or residues.
    int GetSeqLength(int oid) const;
    
    /// Returns an unbiased, approximate sequence length.
    ///
    /// For protein DBs, this method is identical to GetSeqLength().
    /// In the nucleotide case, computing the exact length requires
    /// examination of the sequence data.  This method avoids doing
    /// that, returning an approximation ranging from L-3 to L+3
    /// (where L indicates the exact length), and unbiased on average.
    int GetSeqLengthApprox(int oid) const;
    
    /// Get the ASN.1 header for the sequence.
    CRef<CBlast_def_line_set> GetHdr(int oid) const;
    
    /// Get a CBioseq for a sequence.
    ///
    /// This builds and returns the header and sequence data
    /// corresponding to the indicated sequence as a CBioseq.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @return
    ///   A CBioseq object corresponding to the sequence.
    CRef<CBioseq> GetBioseq(int oid) const;
    
    /// Get a CBioseq for a sequence.
    ///
    /// This builds and returns the header and sequence data
    /// corresponding to the indicated sequence as a CBioseq.  If
    /// target_gi is non-zero, the header information will be filtered
    /// to only include the defline associated with that gi.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param target_gi
    ///   The target gi to filter the header information by.
    /// @return
    ///   A CBioseq object corresponding to the sequence.
    CRef<CBioseq> GetBioseq(int oid, int target_gi) const;
    
    /// Get a pointer to raw sequence data.
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
    /// In the protein case, this is identical to GetSequence().  In
    /// the nucleotide case, it stores 2 bases per byte instead of 4.
    /// The third parameter indicates the encoding for nucleotide
    /// data, either kSeqDBNucl4NA or kSeqDBNuclBlastNA, ignored if
    /// the sequence is a protein sequence.  When done, resources
    /// should be returned with RetSequence.
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param buffer
    ///   A returned pointer to the data in the sequence.
    /// @param nucl_code
    ///   The encoding to use for the returned sequence data.
    /// @return
    ///   The return value is the sequence length (in base pairs or
    ///   residues).  In case of an error, an exception is thrown.
    int GetAmbigSeq(int oid, const char ** buffer, int nucl_code) const;
    
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
    ///   Ordinal ID.
    /// @param buffer
    ///   Address of a char pointer to access the sequence data.
    /// @param nucl_code
    ///   The NA encoding, kSeqDBNuclNcbiNA8 or kSeqDBNuclBlastNA8.
    /// @param strategy
    ///   Indicate which allocation strategy to use.
    /// @return
    ///   The return value is the sequence length (in base pairs or
    ///   residues).  In case of an error, an exception is thrown.
    int GetAmbigSeqAlloc(int                oid,
                         char            ** buffer,
                         int                nucl_code,
                         ESeqDBAllocType    strategy) const;
    
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
    
    /// Returns the type of database opened - protein or nucleotide..
    /// [This method is obsolete; use GetSequenceType() instead.]
    /// 
    /// This uses the same constants as the constructor.
    char GetSeqType() const;
    
    /// Returns the type of database opened - protein or nucleotide.
    /// 
    /// This uses the same constants as the constructor.
    ESeqType GetSequenceType() const;
    
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
    
    /// Returns a sequence iterator.
    ///
    /// This gets an iterator designed to allow traversal of the
    /// database from beginning to end.
    CSeqDBIter Begin() const;
    
    /// Find an included OID, incrementing next_oid if necessary.
    ///
    /// If the specified OID is not included in the set (i.e. the OID
    /// mask), the input parameter is incremented until one is found
    /// that is.  The user will probably want to increment between
    /// calls, if iterating over the db.  [This version of this method
    /// is deprecated.]
    ///
    /// @return
    ///   True if a valid OID was found, false otherwise.
    bool CheckOrFindOID(Uint4 & next_oid) const;
    
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
    /// one iteration per program, this parameter can be omitted.  [This
    /// version of this method is deprecated.]
    ///
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
    EOidListType
    GetNextOIDChunk(Uint4         & begin_chunk,       // out
                    Uint4         & end_chunk,         // out
                    vector<Uint4> & oid_list,          // out
                    Uint4         * oid_state = NULL); // in+out
    
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
    ///
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
    EOidListType
    GetNextOIDChunk(int         & begin_chunk,       // out
                    int         & end_chunk,         // out
                    vector<int> & oid_list,          // out
                    int         * oid_state = NULL); // in+out
    
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
    /// Setting this to a very low value may degrade performance.
    /// Setting it to too high a value may cause stability issues (in
    /// the form of address space exhaustion) The appropriate range of
    /// values depends in part on the memory footprint of other parts
    /// of your application.
    void SetMemoryBound(Uint8 membound, Uint8 slice_size);
    
    /// Translate a PIG to an OID. [This version is deprecated.]
    bool PigToOid(Uint4 pig, Uint4 & oid) const;
    
    /// Translate a PIG to an OID.
    bool PigToOid(int pig, int & oid) const;
    
    /// Translate an OID to a PIG. [This version is deprecated.]
    bool OidToPig(Uint4 oid, Uint4 & pig) const;
    
    /// Translate an OID to a PIG.
    bool OidToPig(int oid, int & pig) const;
    
    /// Translate a GI to an OID. [This version is deprecated.]
    bool OidToGi(Uint4 oid, Uint4 & gi) const;
    
    /// Translate a GI to an OID.
    bool OidToGi(int oid, int & gi) const;
    
    /// Translate a GI to an OID. [This version is deprecated.]
    bool GiToOid(Uint4 gi, Uint4 & oid) const;
    
    /// Translate a GI to an OID.
    bool GiToOid(int gi, int & oid) const;
    
    /// Translate a GI to a PIG. [This version is deprecated.]
    bool GiToPig(Uint4 gi, Uint4 & pig) const;
    
    /// Translate a GI to a PIG.
    bool GiToPig(int gi, int & pig) const;
    
    /// Translate a PIG to a GI. [This version is deprecated.]
    bool PigToGi(Uint4 pig, Uint4 & gi) const;
    
    /// Translate a PIG to a GI.
    bool PigToGi(int pig, int & gi) const;
    
    /// Translate an Accession to a list of OIDs. [This version is deprecated.]
    void AccessionToOids(const string & acc, vector<Uint4> & oids) const;
    
    /// Translate an Accession to a list of OIDs.
    void AccessionToOids(const string & acc, vector<int> & oids) const;
    
    /// Translate a Seq-id to a list of OIDs. [This version is deprecated.]
    void SeqidToOids(const CSeq_id & seqid, vector<Uint4> & oids) const;
    
    /// Translate a Seq-id to a list of OIDs.
    void SeqidToOids(const CSeq_id & seqid, vector<int> & oids) const;
    
    /// Find the sequence closest to the given offset into the database.
    /// 
    /// The database volumes can be viewed as a single array of
    /// residues, partitioned into sequences by OID order.  The length
    /// of this array is given by GetTotalLength().  Given an offset
    /// between 0 and this length, this method returns the OID of the
    /// sequence at the given offset into the array.  It is normally
    /// used to split the database into sections with approximately
    /// equal numbers of residues.
    /// @param first_seq
    ///   First oid to consider (will always return this or higher).
    /// @param residue
    ///   The approximate number residues offset to search for.
    /// @return
    ///   An OID near the specified residue offset.
    int GetOidAtOffset(int first_seq, Uint8 residue) const;

    /// Get a CBioseq for a given GI
    ///
    /// This builds and returns the header and sequence data
    /// corresponding to the indicated GI as a CBioseq.
    ///
    /// @param gi
    ///   The GI of the sequence.
    /// @return
    ///   A CBioseq object corresponding to the sequence.
    CRef<CBioseq> GiToBioseq(int gi) const;
    
    /// Get a CBioseq for a given PIG
    ///
    /// This builds and returns the header and sequence data
    /// corresponding to the indicated PIG (a numeric identifier used
    /// for proteins) as a CBioseq.
    ///
    /// @param pig
    ///   The protein identifier group id of the sequence.
    /// @return
    ///   A CBioseq object corresponding to the sequence.
    CRef<CBioseq> PigToBioseq(int pig) const;
    
    /// Get a CBioseq for a given Seq-id
    ///
    /// This builds and returns the header and sequence data
    /// corresponding to the indicated Seq-id as a CBioseq.  Note that
    /// certain forms of Seq-id map to more than one OID.  If this is
    /// the case for the provided Seq-id, the first matching OID will
    /// be used.
    ///
    /// @param seqid
    ///   The Seq-id identifier of the sequence.
    /// @return
    ///   A CBioseq object corresponding to the sequence.
    CRef<CBioseq> SeqidToBioseq(const CSeq_id & seqid) const;
    
    /// Find volume paths
    ///
    /// Find the base names of all volumes.  This method builds an
    /// alias hierarchy (which should be much faster than constructing
    /// an entire CSeqDB object), and returns the resolved volume file
    /// base names from that hierarchy.  [This version of the method
    /// is obsolete because the sequence type is specified as a
    /// character; eventually only the ESeqType version will exist.]
    ///
    /// @param dbname
    ///   The input name of the database
    /// @param seqtype
    ///   Indicates whether the database is protein or nucleotide
    /// @param paths
    ///   The set of resolved database path names
    static void
    FindVolumePaths(const string   & dbname,
                    char             seqtype,
                    vector<string> & paths);
    
    /// Find volume paths
    ///
    /// Find the base names of all volumes.  This method builds an
    /// alias hierarchy (which should be much faster than constructing
    /// an entire CSeqDB object), and returns the resolved volume file
    /// base names from that hierarchy.
    ///
    /// @param dbname
    ///   The input name of the database
    /// @param seqtype
    ///   Indicates whether the database is protein or nucleotide
    /// @param paths
    ///   The set of resolved database path names
    static void
    FindVolumePaths(const string   & dbname,
                    ESeqType         seqtype,
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
    
private:
    /// Implementation details are hidden.  (See seqdbimpl.hpp).
    class CSeqDBImpl * m_Impl;
};


/// CSeqDBSequence --
///
/// Small class to implement RIAA for sequences.
/// 
/// The CSeqDB class requires that sequences be returned at some point
/// after they are gotten.  This class provides that service via the
/// destructor.  It also insures that the database itself stays around
/// for at least the duration of its lifetime, by holding a CRef<> to
/// that object.  CSeqDB::GetSequence may be used directly to avoid
/// the small overhead of this class, provided care is taken to call
/// CSeqDB::RetSequence.  The data referred to by this object is not
/// modifyable, and is memory mapped (read only) where supported.

class NCBI_XOBJREAD_EXPORT CSeqDBSequence {
public:
    /// Defines the type used to select which sequence to get.
    typedef CSeqDB::TOID TOID;
    
    /// Get a hold a database sequence.
    CSeqDBSequence(CSeqDB * db, int oid)
        : m_DB    (db),
          m_Data  (0),
          m_Length(0)
    {
        m_Length = m_DB->GetSequence(oid, & m_Data);
    }
    
    /// Destructor, returns the sequence.
    ~CSeqDBSequence()
    {
        if (m_Data) {
            m_DB->RetSequence(& m_Data);
        }
    }
    
    /// Get pointer to sequence data.
    const char * GetData()
    {
        return m_Data;
    }
    
    /// Get sequence length.
    int GetLength()
    {
        return m_Length;
    }
    
private:
    /// Prevent copy construct.
    CSeqDBSequence(const CSeqDBSequence &);
    
    /// Prevent copy.
    CSeqDBSequence & operator=(const CSeqDBSequence &);
    
    /// The CSeqDB object this sequence is from.
    CRef<CSeqDB> m_DB;
    
    /// The sequence data for this sequence.
    const char * m_Data;
    
    /// The length of this sequence.
    int          m_Length;
};

// Inline methods for CSeqDBIter

void CSeqDBIter::x_GetSeq()
{
    m_Length = m_DB->GetSequence(m_OID, & m_Data);
}

void CSeqDBIter::x_RetSeq()
{
    if (m_Data)
        m_DB->RetSequence(& m_Data);
}

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDB_HPP

