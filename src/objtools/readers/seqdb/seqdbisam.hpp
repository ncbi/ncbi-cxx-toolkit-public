#ifndef OBJTOOLS_READERS_SEQDB__SEQDBISAM_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBISAM_HPP

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

/// @file seqdbisam.hpp
/// ISAM index database access object.
/// 
/// Defines classes:
///     CSeqDBIsam
/// 
/// Implemented for: UNIX, MS-Windows


#include "seqdbfile.hpp"
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE

/// Bring the object directory definitions into this scope
USING_SCOPE(objects);

/// CSeqDBIsam
/// 
/// Manages one ISAM file, which will translate either PIGs, GIs, or
/// Accessions to OIDs.  Translation in the other direction is done in
/// the CSeqDBVol code.  Files managed by this class include those
/// with the extensions pni, pnd, ppi, ppd, psi, psd, nsi, nsd, nni,
/// and nnd.  Each instance of this object will manage one pair of
/// these files, including one whose name ends in 'i' and one whose
/// name ends in 'd'.

class CSeqDBIsam : public CObject {
public:
    /// Import the type representing one GI, OID association.
    typedef CSeqDBGiList::SGiOid TGiOid;
    
    /// Identifier formats used by this class.
    enum EIdentType {
        eGiId,     /// Genomic ID is a relatively stable numeric identifier for sequences.
        eTiId,     /// Trace ID is a numeric identifier for Trace sequences.
        ePigId,    /// Each PIG identifier refers to exactly one protein sequence.
        eStringId, /// Some sequence sources uses string identifiers.
        eHashId,   /// Lookup from sequence hash values to OIDs.
        eOID       /// The ordinal id indicates the order of the data in the volume's index file.
    };
    
    /// Types of database this class can access.
    enum EIsamDbType {
        eNumeric         = 0, /// Numeric database with Key/Value pairs in the index file.
        eNumericNoData   = 1, /// This type is not supported.
        eString          = 2, /// String database type used here.
        eStringDatabase  = 3, /// This type is not supported.
        eStringBin       = 4  /// This type is not supported.
    };
    
    /// Type which is large enough to span the bytes of an ISAM file.
    typedef CSeqDBAtlas::TIndx TIndx;
    
    /// This class works with OIDs relative to a specific volume.
    typedef int TOid;
    
    /// PIG identifiers for numeric indices over protein volumes.
    typedef int TPig;
    
    /// Genomic IDs, the most common numerical identifier.
    typedef int TGi;
    
    /// Identifier type for trace databases.
    typedef Int8 TTi;
    
    /// Type large enough to hold any numerical ID.
    typedef Int8 TId;
    
    /// Constructor
    /// 
    /// An ISAM file object corresponds to an index file and a data
    /// file, and converts identifiers (string, GI, or PIG) into OIDs
    /// relative to a particular database volume.
    /// 
    /// @param atlas
    ///   The memory management object. [in]
    /// @param dbname
    ///   The name of the volume's files (minus the extension). [in]
    /// @param prot_nucl
    ///   Whether the sequences are protein or nucleotide. [in]
    /// @param file_ext_char
    ///   This is 's', 'n', or 'p', for string, GI, or PIG, respectively. [in]
    /// @param ident_type
    ///   The type of identifiers this database translates. [in]
    CSeqDBIsam(CSeqDBAtlas  & atlas,
               const string & dbname,
               char           prot_nucl,
               char           file_ext_char,
               EIdentType     ident_type);
    
    /// Destructor
    ///
    /// Releases all resources associated with this object.
    ~CSeqDBIsam();
    
    /// PIG translation
    /// 
    /// A PIG identifier is translated to an OID.  PIG identifiers are
    /// used exclusively for protein sequences.  One PIG corresponds
    /// to exactly one sequences of amino acids, and vice versa.  They
    /// are also stable; the sequence a PIG points to will never be
    /// changed.
    /// 
    /// @param pig
    ///   The PIG to look up. [in]
    /// @param oid
    ///   The returned oid. [out]
    /// @param locked
    ///   The lock hold object for this thread. [in|out]
    /// @return
    ///   true if the PIG was found
    bool PigToOid(TPig pig, TOid & oid, CSeqDBLockHold & locked)
    {
        _ASSERT(m_IdentType == ePigId);
        return x_IdentToOid(pig, oid, locked);
    }
    
    /// GI or TI translation
    /// 
    /// A GI or TI identifier is translated to an OID.  GI identifiers
    /// are used for all types of sequences.  TI identifiers are used
    /// primarily for nucleotide data in the Trace DBs.  Multiple GIs
    /// may indicate the same sequence of bases and the same OID, but
    /// TIs are usually unique.
    /// 
    /// @param id
    ///   The GI or TI to look up. [in]
    /// @param oid
    ///   The returned oid. [out]
    /// @param locked
    ///   The lock hold object for this thread. [in|out]
    /// @return
    ///   true if the GI was found
    bool IdToOid(Int8 id, TOid & oid, CSeqDBLockHold & locked)
    {
        _ASSERT(m_IdentType == eGiId || m_IdentType == eTiId);
        return x_IdentToOid(id, oid, locked);
    }
    
    /// Translate Gis and Tis to Oids for the given ID list.
    ///
    /// This method iterates over a vector of Gi/OID and/or Ti/OID
    /// pairs.  For each pair where the OID is -1, the GI or TI will
    /// be looked up in the ISAM file, and (if found) the correct OID
    /// will be stored (otherwise the -1 will remain).  This method
    /// will normally be called once for each volume.
    ///
    /// @param vol_start
    ///   The starting OID of this volume. [in]
    /// @param vol_end
    ///   The fist OID past the end of this volume. [in]
    /// @param ids
    ///   The set of GI-OID or TI-OID pairs. [in|out]
    /// @param locked
    ///   The lock holder object for this thread. [in|out]
    void IdsToOids(int              vol_start,
                   int              vol_end,
                   CSeqDBGiList   & ids,
                   CSeqDBLockHold & locked);
    
    /// Compute list of included OIDs based on a negative ID list.
    ///
    /// This method iterates over a vector of Gis or Tis, along with
    /// the corresponding ISAM file for this volume.  Each OID found
    /// in the ISAM file is marked in the negative ID list.  For those
    /// for which the GI or TI is not mentioned in the negative ID
    /// list, the OID will be marked as an 'included' OID in the ID
    /// list (that OID will be searched).  The OIDs for IDs that are
    /// not found in the ID list will be marked as 'visible' OIDs.
    /// When this process is done for all volumes, the SeqDB object
    /// will use all OIDs that are either marked as 'included' or NOT
    /// marked as 'visible'.  The 'visible' list is needed because
    /// otherwise iteration would skip IDs that are do not have GIs or
    /// TIs (whichever is being iterated).  To use this method, this
    /// volume must have an ISAM file matching the negative ID list's
    /// identifier type or an exception will be thrown.
    ///
    /// @param vol_start
    ///   The starting OID of this volume. [in]
    /// @param vol_end
    ///   The fist OID past the end of this volume. [in]
    /// @param ids
    ///   The set of GI-OID pairs. [in|out]
    /// @param locked
    ///   The lock holder object for this thread. [in|out]
    void IdsToOids(int                  vol_start,
                   int                  vol_end,
                   CSeqDBNegativeList & ids,
                   CSeqDBLockHold     & locked);
    
    /// String translation
    /// 
    /// A string identifier is translated to one or more OIDs.  String
    /// identifiers are used by some groups which produce sequence
    /// data.  In a few cases, the string may correspond to multiple
    /// OIDs with different sequences of bases.  For this reason, the
    /// list of OIDs is returned in a vector.  The string provided
    /// here is looked up in several ways.  If it contains pipe
    /// characters (ie "|") the data will be interpreted as a SeqID.
    /// This routine can use faster lookup mechanisms if the
    /// simplification routines were able to recognize the sequence.
    /// 
    /// @param acc
    ///   The string to look up. [in]
    /// @param oids
    ///   The returned oids. [out]
    /// @param adjusted
    ///   Whether the simplification adjusted the string. [in|out]
    /// @param locked
    ///   The lock hold object for this thread. [in|out]
    void StringToOids(const string   & acc,
                      vector<TOid>   & oids,
                      bool             adjusted,
                      CSeqDBLockHold & locked);
    
    /// Seq-id translation
    /// 
    /// A Seq-id identifier (serialized to a string) is translated
    /// into an OID.  This routine will attempt to simplify the seqid
    /// so as to use the faster numeric lookup techniques whenever
    /// possible.
    /// 
    /// @param acc
    ///   A string containing the Seq-id. [in]
    /// @param oid
    ///   The returned oid. [out]
    /// @param locked
    ///   The lock hold object for this thread. [in|out]
    bool SeqidToOid(const string & acc, TOid & oid, CSeqDBLockHold & locked);
    
    /// Sequence hash lookup
    /// 
    /// This methods tries to find sequences associated with a given
    /// sequence hash value.  The provided value is numeric but the
    /// ISAM file uses a string format, because string searches can
    /// return multiple results per key, and there may be multiple
    /// OIDs for a given hash value due to identical sequences and
    /// collisions.
    /// 
    /// @param hash
    ///   The sequence hash value to look up. [in]
    /// @param oids
    ///   The returned oids. [out]
    /// @param locked
    ///   The lock hold object for this thread. [in|out]
    void HashToOids(unsigned         hash,
                    vector<TOid>   & oids,
                    CSeqDBLockHold & locked);
    
    /// String id simplification.
    /// 
    /// This routine tries to produce a numerical type from a string
    /// identifier.  SeqDB can use faster lookup mechanisms if a PIG,
    /// GI, or OID type can be recognized in the string, for example.
    /// Even when the output is a string, it may be better formed for
    /// the purpose of lookup in the string ISAM file.
    /// 
    /// @param acc
    ///   The string to look up. [in]
    /// @param num_id
    ///   The returned identifier, if numeric. [out]
    /// @param str_id
    ///   The returned identifier, if a string. [out]
    /// @param simpler
    ///   Whether an adjustment was done at all. [out]
    /// @return
    ///   The resulting identifier type.
    static EIdentType
    TryToSimplifyAccession(const string & acc,
                           Int8         & num_id,
                           string       & str_id,
                           bool         & simpler);
    
    /// Return any memory held by this object to the atlas.
    void UnLease();
    
    /// Seq-id simplification.
    /// 
    /// Given a Seq-id, this routine devolves it to a GI or PIG if
    /// possible.  If not, it formats the Seq-id into a canonical form
    /// for lookup in the string ISAM files.  If the Seq-id was parsed
    /// from an accession, it can be provided in the "acc" parameter,
    /// and it will be used if the Seq-id is not in a form this code
    /// can recognize.  In the case that new Seq-id types are added,
    /// support for which has not been added to this code, this
    /// mechanism will try to use the original string.
    /// 
    /// @param bestid
    ///   The Seq-id to look up. [in]
    /// @param acc
    ///   The original string the Seq-id was created from (or NULL). [in]
    /// @param num_id
    ///   The returned identifier, if numeric. [out]
    /// @param str_id
    ///   The returned identifier, if a string. [out]
    /// @param simpler
    ///   Whether an adjustment was done at all. [out]
    /// @return
    ///   The resulting identifier type.
    static EIdentType
    SimplifySeqid(CSeq_id       & bestid,
                  const string  * acc,
                  Int8          & num_id,
                  string        & str_id,
                  bool          & simpler);
    
    /// Get Numeric Bounds.
    /// 
    /// Fetch the lowest, highest, and total number of numeric keys in
    /// the database index.  If the operation fails, zero will be
    /// returned for count.
    /// 
    /// @param low_id Lowest numeric id value in database. [out]
    /// @param high_id Highest numeric id value in database. [out]
    /// @param count Number of numeric id values in database. [out]
    /// @param locked Lock holder object for this thread. [in]
    void GetIdBounds(Int8           & low_id,
                     Int8           & high_id,
                     int            & count,
                     CSeqDBLockHold & locked);
    
    /// Get String Bounds.
    /// 
    /// Fetch the lowest, highest, and total number of string keys in
    /// the database index.  If the operation fails, zero will be
    /// returned for count.
    /// 
    /// @param low_id Lowest string id value in database. [out]
    /// @param high_id Highest string id value in database. [out]
    /// @param count Number of string id values in database. [out]
    /// @param locked Lock holder object for this thread. [in]
    void GetIdBounds(string         & low_id,
                     string         & high_id,
                     int            & count,
                     CSeqDBLockHold & locked);
    
    /// Check if a given ISAM index exists.
    ///
    /// @param dbname Base name of the database volume.
    /// @param prot_nucl 'n' or 'p' for protein or nucleotide.
    /// @param file_ext_char Identifier symbol; 's' for string, etc.
    static bool IndexExists(const string & dbname,
                            char           prot_nucl,
                            char           file_ext_char);
    
private:
    /// Stores a key for an ISAM file.
    ///
    /// This class stores a key of either of the types used by ISAM
    /// files.  It provides functionality for ordering comparisons of
    /// keys.
    
    class SIsamKey {
    public:
        // If case insensitive string comparisons are desired, the
        // keys should be upcased before calling these methods.
        
        /// Constructor.
        SIsamKey()
            : m_IsSet(false), m_NKey(-1)
        {
        }
        
        /// Returns true if this object has an assigned value.
        bool IsSet()
        {
            return m_IsSet;
        }
        
        /// Assign a numeric value to this object.
        void SetNumeric(Int8 ident)
        {
            m_IsSet = true;
            m_NKey = ident;
        }
        
        /// Fetch the numeric value of this object.
        Int8 GetNumeric() const
        {
            return m_NKey;
        }
        
        /// Fetch the string value of this object.
        void SetString(const string & ident)
        {
            m_IsSet = true;
            m_SKey = ident;
        }
        
        /// Fetch the numeric value of this object.
        string GetString() const
        {
            return m_SKey;
        }
        
        /// Returns true if the provided integer compares as lower
        /// than the assigned lower boundary for this ISAM file.
        bool OutsideFirstBound(Int8 ident)
        {
            return (m_IsSet && (ident < m_NKey));
        }
        
        /// Returns true if the provided string compares as lower than
        /// the assigned lower boundary for this ISAM file.
        bool OutsideFirstBound(const string & ident)
        {
            return (m_IsSet && (ident < m_SKey));
        }
        
        /// Returns true if the provided integer compares as higher
        /// than the assigned upper boundary for this ISAM file.
        bool OutsideLastBound(Int8 ident)
        {
            return (m_IsSet && (ident > m_NKey));
        }
        
        /// Returns true if the provided string compares as lower than
        /// the assigned upper boundary for this ISAM file.
        bool OutsideLastBound(const string & ident)
        {
            return (m_IsSet && (ident > m_SKey));
        }
        
    private:
        /// True if this object has an assigned value.
        bool   m_IsSet;
        
        /// The key, if it is a number.
        Int8   m_NKey;
        
        /// The key, if it is a string.
        string m_SKey;
    };
    
    /// Exit conditions occurring in this code.
    enum EErrorCode {
        eNotFound        =  1,   /// The key was not found
        eNoError         =  0,   /// Lookup was successful
        eBadVersion      =  -10, /// The format version of the ISAM file is unsupported.
        eBadType         =  -11, /// The requested ISAM type did not match the file.
        eWrongFile       =  -12  /// The file was not found, or was the wrong length.
    };
    
    /// The key-value pair type for numerical indices
    struct SNumericKeyData4 {
        /// The key, a GI or PIG identifier.
        Uint4 key;
        
        /// The value, an OID.
        Uint4 data;
    };
    
    /// Numeric identifier lookup
    /// 
    /// Given a numeric identifier, this routine finds the OID.
    /// 
    /// @param id
    ///   The GI or PIG identifier to look up.
    /// @param oid
    ///   The returned oid.
    /// @param locked
    ///   The lock holder object for this thread.
    /// @return
    ///   true if the identifier was found.
    bool x_IdentToOid(Int8             id,
                      TOid           & oid,
                      CSeqDBLockHold & locked);
    
    /// Index file search
    /// 
    /// Given a numeric identifier, this routine finds the OID or the
    /// page in the data file where the OID can be found.
    /// 
    /// @param Number
    ///   The GI or PIG identifier to look up.
    /// @param Data
    ///   The returned OID.
    /// @param Index
    ///   The returned location in the ISAM table, or NULL.
    /// @param SampleNum
    ///   The returned location in the data file if not done.
    /// @param done
    ///   true if the OID was found.
    /// @param locked
    ///   The lock holder object for this thread.
    /// @return
    ///   A non-zero error on failure, or eNoError on success.
    EErrorCode
    x_SearchIndexNumeric(Int8             Number,
                         int            * Data,
                         Uint4          * Index,
                         Int4           & SampleNum,
                         bool           & done,
                         CSeqDBLockHold & locked);
    
    /// GiList Translation
    /// 
    /// Given a GI list, this routine finds the OID for each ID in the
    /// list not already having a translation.
    /// 
    /// @param vol_start
    ///   The starting OID for this ISAM file's database volume.
    /// @param vol_end
    ///   The ending OID for this ISAM file's database volume.
    /// @param gis
    ///   The GI list to translate.
    /// @param use_tis
    ///   Iterate over the TI part of the GI list.
    /// @param locked
    ///   The lock holder object for this thread.
    void
    x_SearchIndexNumericMulti(int              vol_start,
                              int              vol_end,
                              CSeqDBGiList   & gis,
                              bool             use_tis,
                              CSeqDBLockHold & locked);
    
    /// Negative ID List Translation
    /// 
    /// Given a Negative ID list, this routine turns on the bits for
    /// the OIDs found in the volume but not in the negated ID list.
    /// 
    /// @param vol_start
    ///   The starting OID for this ISAM file's database volume.
    /// @param vol_end
    ///   The ending OID for this ISAM file's database volume.
    /// @param nlist
    ///   The Negative ID list to translate.
    /// @param use_tis
    ///   Iterate over TIs if true (GIs otherwise).
    /// @param locked
    ///   The lock holder object for this thread.
    void
    x_SearchNegativeMulti(int                  vol_start,
                          int                  vol_end,
                          CSeqDBNegativeList & gis,
                          bool                 use_tis,
                          CSeqDBLockHold     & locked);
    
    /// Data file search
    /// 
    /// Given a numeric identifier, this routine finds the OID in the
    /// data file.
    /// 
    /// @param Number
    ///   The GI or PIG identifier to look up.
    /// @param Data
    ///   The returned OID.
    /// @param Index
    ///   The returned location in the ISAM table, or NULL.
    /// @param SampleNum
    ///   The location of the page in the data file to search.
    /// @param locked
    ///   The lock holder object for this thread.
    /// @return
    ///   A non-zero error on failure, or eNoError on success.
    EErrorCode
    x_SearchDataNumeric(Int8             Number,
                        int            * Data,
                        Uint4          * Index,
                        Int4             SampleNum,
                        CSeqDBLockHold & locked);
    
    /// GiList translation for one page of a data file.
    /// 
    /// Given a GI list, this routine finds the OID for each GI in the
    /// list using the mappings in one page of the data file.  It
    /// updates the provided GI list index to skip past any GIs it
    /// translates.
    /// 
    /// @param vol_start
    ///   The starting OID for this ISAM file's database volume.
    /// @param vol_end
    ///   The fist OID past the end of this volume.
    /// @param gis
    ///   The GI list to translate.
    /// @param gilist_index
    ///   The index of the first unexamined GI in the GI list.
    /// @param sample_index
    ///   Indicates which data page should be used.
    /// @param use_tis
    ///   Use trace IDs instead of GIs.
    /// @param locked
    ///   The lock holder object for this thread.
    void
    x_SearchDataNumericMulti(int              vol_start,
                             int              vol_end,
                             CSeqDBGiList   & gis,
                             int            & gilist_index,
                             int              sample_index,
                             bool             use_tis,
                             CSeqDBLockHold & locked);
    
    /// Numeric identifier lookup
    /// 
    /// Given a numeric identifier, this routine finds the OID.
    /// 
    /// @param Number
    ///   The GI or PIG identifier to look up.
    /// @param Data
    ///   The returned OID.
    /// @param Index
    ///   The returned location in the ISAM table, or NULL.
    /// @param locked
    ///   The lock holder object for this thread.
    /// @return
    ///   A non-zero error on failure, or eNoError on success.
    EErrorCode
    x_NumericSearch(Int8             Number,
                    int            * Data,
                    Uint4          * Index,
                    CSeqDBLockHold & locked);
    
    /// String identifier lookup
    /// 
    /// Given a string identifier, this routine finds the OID(s).
    /// 
    /// @param term_in
    ///   The string identifier to look up.
    /// @param term_out
    ///   The returned keys (as strings).
    /// @param value_out
    ///   The returned oids (as strings).
    /// @param index_out
    ///   The locations where the matches were found.
    /// @param locked
    ///   The lock holder object for this thread.
    /// @return
    ///   A non-zero error on failure, or eNoError on success.
    EErrorCode
    x_StringSearch(const string   & term_in,
                   vector<string> & term_out,
                   vector<string> & value_out,
                   vector<TIndx>  & index_out,
                   CSeqDBLockHold & locked);
    
    /// Initialize the search object
    /// 
    /// The first identifier search sets up the object by calling this
    /// function, which reads the metadata from the index file and
    /// sets all the fields needed for ISAM lookups.
    /// 
    /// @param locked
    ///   The lock holder object for this thread.
    /// @return
    ///   A non-zero error on failure, or eNoError on success.
    EErrorCode
    x_InitSearch(CSeqDBLockHold & locked);
    
    /// Determine the number of elements in the data page.
    /// 
    /// The number of elements is determined based on whether this is
    /// the last page and the configured page size.
    /// 
    /// @param SampleNum
    ///   Which data page will be searched.
    /// @param Start
    ///   The returned index of the start of the page.
    /// @return
    ///   The number of elements in this data page.
    int x_GetPageNumElements(Int4   SampleNum,
                             Int4 * Start);
    
    /// Lookup a string in a sparse table
    /// 
    /// This does string lookup in a sparse string table.  There is no
    /// support (code) for this since there are currently no examples
    /// of this kind of table to test against.
    /// 
    /// @param acc
    ///   The string to look up.
    /// @param oids
    ///   The returned oids found by the search.
    /// @param adjusted
    ///   Whether the key was changed by the identifier simplification logic.
    /// @param locked
    ///   The lock holder object for this thread.
    /// @return
    ///   true if results were found
    bool x_SparseStringToOids(const string   & acc,
                              vector<int>    & oids,
                              bool             adjusted,
                              CSeqDBLockHold & locked);
    
    /// Find the first character to differ in two strings
    /// 
    /// This finds the index of the first character to differ in
    /// meaningful way between two strings.  One of the strings is a
    /// term that is passed in; the other is assumed to be located in
    /// the ISAM table, a lease to which is passed to this function.
    /// 
    /// @param term_in
    ///   The key string to compare against.
    /// @param lease
    ///   A lease to hold the data in the ISAM table file.
    /// @param file_name
    ///   The name of the ISAM file to work with.
    /// @param file_length
    ///   The length of the file named by file_name.
    /// @param at_least
    ///   Try to get at least this many bytes.
    /// @param KeyOffset
    ///   The location of the key in the leased file.
    /// @param ignore_case
    ///   Whether to treat the search as case-sensitive
    /// @param locked
    ///   The lock holder object for this thread.
    /// @return
    ///   The position of the first difference.
    int
    x_DiffCharLease(const string   & term_in,
                    CSeqDBMemLease & lease,
                    const string   & file_name,
                    TIndx            file_length,
                    Uint4            at_least,
                    TIndx            KeyOffset,
                    bool             ignore_case,
                    CSeqDBLockHold & locked);
    
    /// Find the first character to differ in two strings
    /// 
    /// This finds the index of the first character to differ in
    /// meaningful way between two strings.  One of the strings is a
    /// term that is passed in; the other is a range of memory
    /// represented by two pointers.
    /// 
    /// @param term_in
    ///   The key string to compare against.
    /// @param begin
    ///   A pointer to the start of the second string.
    /// @param end
    ///   A pointer to the end of the second string.
    /// @param ignore_case
    ///   Whether to treat the search as case-sensitive
    /// @return
    ///   The position of the first difference.
    int
    x_DiffChar(const string & term_in,
               const char   * begin,
               const char   * end,
               bool           ignore_case);
    
    /// Extract the data from a key-value pair in memory.
    /// 
    /// Given pointers to a location in mapped memory, and the end of
    /// the mapped data, this finds the key and data values for the
    /// object at that location.
    /// 
    /// @param key_start
    ///   A pointer to the beginning of the key-value pair in memory.
    /// @param entry_end
    ///   A pointer to the end of the mapped area of memory.
    /// @param key_out
    ///   A string holding the ISAM entry's key
    /// @param data_out
    ///   A string holding the ISAM entry's value
    void x_ExtractData(const char     * key_start,
                       const char     * entry_end,
                       vector<string> & key_out,
                       vector<string> & data_out);
    
    /// Get the offset of the specified sample.
    /// 
    /// For string ISAM indices, the index file contains a table of
    /// offsets of the index file samples.  This function gets the
    /// offset of the specified sample in the index file's table.
    /// 
    /// @param sample_offset
    ///   The offset into the file of the set of samples.
    /// @param sample_num
    ///   The index of the sample to get.
    /// @param locked
    ///   This thread's lock holder object.
    /// @return
    ///   The offset of the sample in the index file.
    TIndx x_GetIndexKeyOffset(TIndx            sample_offset,
                              Uint4            sample_num,
                              CSeqDBLockHold & locked);
    
    /// Read a string from the index file.
    /// 
    /// Given an offset into the index file, and a maximum length,
    /// this function returns the bytes in a string object.
    /// 
    /// @param key_offset
    ///   The offset into the file of the first byte.
    /// @param length
    ///   The maximum number of bytes to get.
    /// @param prefix
    ///   The string in which to return the data.
    /// @param trim_to_null
    ///   Whether to search for a null and return only that much data.
    /// @param locked
    ///   This thread's lock holder object.
    void x_GetIndexString(TIndx            key_offset,
                          int              length,
                          string         & prefix,
                          bool             trim_to_null,
                          CSeqDBLockHold & locked);
    
    /// Find the first character to differ in two strings
    /// 
    /// This finds the index of the first character to differ between
    /// two strings.  The first string is provided, the second is one
    /// of the sample strings, indicated by the index of that sample
    /// value.
    /// 
    /// @param term_in
    ///   The key string to compare against.
    /// @param SampleNum
    ///   Selects which sample to compare with.
    /// @param KeyOffset
    ///   The returned offset of the key that was used.
    /// @param locked
    ///   This thread's lock holder object.
    int x_DiffSample(const string   & term_in,
                     Uint4            SampleNum,
                     TIndx          & KeyOffset,
                     CSeqDBLockHold & locked);
    
    /// Find matches in the given page of a string ISAM file.
    /// 
    /// This searches the area around a specific page of the data file
    /// to find all matches to term_in.  The results are returned in
    /// vectors.  This method may search multiple pages.
    /// 
    /// @param term_in
    ///   The key string to compare against.
    /// @param sample_index
    ///   Selects which page to search.
    /// @param indices_out
    ///   The index of each match.
    /// @param keys_out
    ///   The key of each match.
    /// @param data_out
    ///   The value of each match.
    /// @param locked
    ///   This thread's lock holder object.
    void x_ExtractAllData(const string   & term_in,
                          TIndx            sample_index,
                          vector<TIndx>  & indices_out,
                          vector<string> & keys_out,
                          vector<string> & data_out,
                          CSeqDBLockHold & locked);
    
    /// Find matches in the given memory area of a string ISAM file.
    /// 
    /// This searches the specified section of memory to find all
    /// matches to term_in.  The results are returned in vectors.
    /// 
    /// @param term_in
    ///   The key string to compare against.
    /// @param page_index
    ///   Selects which page to search.
    /// @param beginp
    ///   Pointer to the start of the memory area
    /// @param endp
    ///   Pointer to the end of the memory area
    /// @param indices_out
    ///   The index of each match.
    /// @param keys_out
    ///   The key of each match.
    /// @param data_out
    ///   The value of each match.
    void x_ExtractPageData(const string   & term_in,
                           TIndx            page_index,
                           const char     * beginp,
                           const char     * endp,
                           vector<TIndx>  & indices_out,
                           vector<string> & keys_out,
                           vector<string> & data_out);
    
    /// Map a page into memory
    /// 
    /// Given two indexes, this method maps into memory the area
    /// starting at the beginning of the first index and extending to
    /// the end of the other.  (If the indexes are equal, only one
    /// page would be mapped.)
    /// 
    /// @param SampleNum1
    ///   The first page index.
    /// @param SampleNum2
    ///   The second page index.
    /// @param beginp
    ///   The returned starting offset of the mapped area.
    /// @param endp
    ///   The returned ending offset of the mapped area.
    /// @param locked
    ///   This thread's lock holder object.
    void x_LoadPage(TIndx             SampleNum1,
                    TIndx             SampleNum2,
                    const char     ** beginp,
                    const char     ** endp,
                    CSeqDBLockHold &  locked);
    
    /// Test a sample key value from a numeric index.
    ///
    /// This method reads the key value of an index file sample
    /// element from a numeric index file.  The calling code should
    /// insure that the data is mapped in, and that the file type is
    /// correct.  The key value found will be compared to the search
    /// key.  This method will return 0 for an exact match, -1 if the
    /// key is less than the sample, or 1 if the key is greater.  If
    /// the match is exact, it will also return the data in data_out.
    ///
    /// @param index_lease
    ///   The memory lease to use with the index file.
    /// @param index
    ///   The index of the sample to get.
    /// @param key_in
    ///   The key for which the user is searching.
    /// @param key_out
    ///   The key found will be returned here.
    /// @param data_out
    ///   If an exact match, the data found will be returned here.
    /// @return
    ///   -1, 0 or 1 when key_in is less, equal greater than key_out.
    int x_TestNumericSample(CSeqDBMemLease & index_lease,
                            int              index,
                            Int8             key_in,
                            Int8           & key_out,
                            int            & data_out);
    
    /// Get a sample key value from a numeric index.
    ///
    /// Given the index of a sample value, this code will get the key.
    /// If data values are stored in the index file, the corresponding
    /// data value will also be returned.  The offset of the data
    /// block is computed and returned as well.
    ///
    /// @param index_lease
    ///   The memory lease to use with the index file.
    /// @param index
    ///   The index of the sample to get.
    /// @param key_out
    ///   The key found will be returned here.
    /// @param data_out
    ///   If an exact match, the data found will be returned here.
    void x_GetNumericSample(CSeqDBMemLease & index_lease,
                            int              index,
                            Int8           & key_out,
                            int            & data_out);
    
    /// Advance the GI list
    ///
    /// Skip over any GIs in the GI list that are less than the key,
    /// translate any that are equal to it, and skip past any GI/OID
    /// pairs that have already been translated.  Uses the parabolic
    /// binary search technique.
    ///
    /// @param vol_start
    ///   The starting OID of the volume.
    /// @param vol_end
    ///   The ending OID of the volume.
    /// @param gis
    ///   The GI list to advance through.
    /// @param index
    ///   The working index into the GI list.
    /// @param key
    ///   The current key in the ISAM file.
    /// @param data
    ///   The data corresponding to key.
    /// @param use_tis
    ///   Use trace IDs instead of GIs.
    /// @return
    ///   Returns true if any advancement was possible.
    bool x_AdvanceGiList(int            vol_start,
                         int            vol_end,
                         CSeqDBGiList & gis,
                         int          & index,
                         Int8           key,
                         int            data,
                         bool           use_tis);
    
    /// Find ID in the negative GI list using PBS.
    ///
    /// Use parabolic binary search to find the specified ID in the
    /// negative ID list.  The 'index' value is the index to start the
    /// search at (this must refer to an index at or before the target
    /// data if the search is to succeed).  Whether the search was
    /// successful or not, the index will be moved forward past any
    /// elements with values less than 'key'.
    ///
    /// @param ids     Negative ID list. [in|out]
    /// @param index   Index into negative ID list. [in|out]
    /// @param key     Key for which to search. [in]
    /// @param use_tis If true, search for a TI, else for a GI. [in]
    /// @return True if the search found the ID.
    inline bool
    x_FindInNegativeList(CSeqDBNegativeList & ids,
                         int                & index,
                         Int8                 key,
                         bool                 use_tis);
    
    /// Advance the ISAM file
    ///
    /// Skip over any GI/OID pairs in the ISAM file that are less than
    /// the target_gi.  Uses the parabolic binary search technique.
    ///
    /// @param index_lease
    ///   The memory lease to use with the index file.
    /// @param index
    ///   The working index into the ISAM file.
    /// @param target_gi
    ///   The GI from the GI list for which we are searching.
    /// @param isam_key
    ///   The key of the current location in the ISAM file.
    /// @param isam_data
    ///   The data corresponding to isam_key.
    /// @return
    ///   Returns true if any advancement was possible.
    bool x_AdvanceIsamIndex(CSeqDBMemLease & index_lease,
                            int            & index,
                            Int8             target_gi,
                            Int8           & isam_key,
                            int            & isam_data);
    
    /// Map a data page.
    ///
    /// The caller provides an index into the sample file.  The page
    /// of data is mapped, and a pointer is returned.  In addition,
    /// the starting index (start) of the data is returned, along with
    /// the number of elements in that page.
    ///
    /// @param sample_index Index into the index (i.e. pni) file. [in]
    /// @param start Index of first element of the page.          [out]
    /// @param num_elements Number of elements in the page.       [out]
    /// @param data_page_begin Pointer to the returned data.      [out]
    /// @param locked The lock holder object for this thread.     [out]
    void x_MapDataPage(int                 sample_index,
                       int               & start,
                       int               & num_elements,
                       SNumericKeyData4 ** data_page_begin,
                       CSeqDBLockHold    & locked);
    
    /// Get a particular data element from a data page.
    /// @param dpage A pointer to that page in memory.  [in]
    /// @param index The index of the element to fetch. [in]
    /// @param key   The returned key.   [out]
    /// @param data  The returned value. [out]
    void x_GetDataElement(SNumericKeyData4 * dpage,
                          int               index,
                          Int8            & key,
                          int             & data);
    
    /// Find the least and greatest keys in this ISAM file.
    void x_FindIndexBounds(CSeqDBLockHold & locked);
    
    /// Check whether a numeric key is within this volume's bounds.
    /// @param key The key for which to do the check.
    /// @param locked The lock holder object for this thread.
    bool x_OutOfBounds(Int8 key, CSeqDBLockHold & locked);
    
    /// Check whether a string key is within this volume's bounds.
    /// @param key The key for which to do the check.
    /// @param locked The lock holder object for this thread.
    bool x_OutOfBounds(string key, CSeqDBLockHold & locked);
    
    /// Converts a string to upper case.
    static void x_Upper(string & s)
    {
        for(size_t i = 0; i < s.size(); i++) {
            s[i] = toupper(s[i]);
        }
    }
    
    /// Fetch a GI or TI from a GI list.
    static Int8 x_GetId(CSeqDBGiList & ids, int index, bool use_tis)
    {
        return (use_tis
                ? ids.GetTiOid(index).ti
                : ids.GetGiOid(index).gi);
    }
    
    /// Fetch an OID from the GI or TI vector in a GI list.
    static Int8 x_GetOid(CSeqDBGiList & ids, int index, bool use_tis)
    {
        return (use_tis
                ? ids.GetTiOid(index).oid
                : ids.GetGiOid(index).oid);
    }
    
    /// Fetch a GI or TI from a GI list.
    static Int8 x_GetId(CSeqDBNegativeList & ids, int index, bool use_tis)
    {
        return (use_tis
                ? ids.GetTi(index)
                : ids.GetGi(index));
    }
    
    /// Make filenames for ISAM file.
    ///
    /// @param dbname Base name of the database volume.
    /// @param prot_nucl 'n' or 'p' for protein or nucleotide.
    /// @param file_ext_char Identifier symbol; 's' for string, etc.
    /// @param index_name Filename of ISAM index file.
    /// @param index_name Filename of ISAM data file.
    static void x_MakeFilenames(const string & dbname,
                                char           prot_nucl,
                                char           file_ext_char,
                                string       & index_name,
                                string       & data_name);
    
    // Data
    
    /// The memory management layer
    CSeqDBAtlas & m_Atlas;
    
    /// The type of identifier this class uses
    EIdentType m_IdentType;
    
    /// A persistent lease on the ISAM index file.
    CSeqDBMemLease m_IndexLease;
    
    /// A persistent lease on the ISAM data file.
    CSeqDBMemLease m_DataLease;
    
    /// The format type of database files found (eNumeric or eString).
    int m_Type;
    
    /// The filename of the ISAM data file.
    string m_DataFname;
    
    /// The filename of the ISAM index file.
    string m_IndexFname;
    
    /// The length of the ISAM data file.
    TIndx m_DataFileLength;
    
    /// The length of the ISAM index file.
    TIndx m_IndexFileLength;
    
    /// Number of terms in database
    Int4 m_NumTerms;
    
    /// Number of terms in ISAM index
    Int4 m_NumSamples;
    
    /// Page size of ISAM index
    Int4 m_PageSize;
    
    /// Maximum string length in the database
    Int4 m_MaxLineSize;
    
    /// Options set by upper layer
    Int4 m_IdxOption;
    
    /// Flag indicating whether initialization has been done.
    bool m_Initialized;
    
    /// Offset of samples in index file.
    TIndx m_KeySampleOffset;
    
    /// Check if data for String ISAM sorted
    bool m_TestNonUnique;
    
    /// Pointer to index file if no memmap.
    char * m_FileStart;
    
    /// First and last offset's of last page.
    Int4 m_FirstOffset;
    
    /// First and last offset's of last page.
    Int4 m_LastOffset;
    
    /// First volume key
    SIsamKey m_FirstKey;
    
    /// Last volume key
    SIsamKey m_LastKey;
};

inline int
CSeqDBIsam::x_TestNumericSample(CSeqDBMemLease & index_lease,
                                int              index,
                                Int8             key_in,
                                Int8           & key_out,
                                int            & data_out)
{
    // Only implements 4 byte keys.
    
    SNumericKeyData4 * keydatap = 0;
    
    int obj_size = (int) sizeof(SNumericKeyData4);
    TIndx offset_begin = m_KeySampleOffset + (obj_size * index);
    
    keydatap = (SNumericKeyData4*) index_lease.GetPtr(offset_begin);
    key_out = (int) SeqDB_GetStdOrd(& (keydatap->key));
    
    int rv = 0;
    
    if (key_in < key_out) {
        rv = -1;
    } else if (key_in > key_out) {
        rv = 1;
    } else {
        rv = 0;
        data_out = (int) SeqDB_GetStdOrd(& (keydatap->data));
    }
    
    return rv;
}

inline void
CSeqDBIsam::x_GetNumericSample(CSeqDBMemLease & index_lease,
                               int              index,
                               Int8           & key_out,
                               int            & data_out)
{
    // Only implements 4 byte keys.
    
    SNumericKeyData4 * keydatap = 0;
    
    int obj_size = (int) sizeof(SNumericKeyData4);
    TIndx offset_begin = m_KeySampleOffset + (obj_size * index);
    
    keydatap = (SNumericKeyData4*) index_lease.GetPtr(offset_begin);
    key_out = (int) SeqDB_GetStdOrd(& (keydatap->key));
    data_out = (int) SeqDB_GetStdOrd(& (keydatap->data));
}

inline bool
CSeqDBIsam::x_AdvanceGiList(int            vol_start,
                            int            vol_end,
                            CSeqDBGiList & gis,
                            int          & index,
                            Int8           key,
                            int            data,
                            bool           use_tis)
{
    // Skip any that are less than key.
    
    bool advanced = false;
    int gis_size = use_tis ? gis.GetNumTis() : gis.GetNumGis();
    
    while((index < gis_size) && (x_GetId(gis, index, use_tis) < key)) {
        advanced = true;
        index++;
        
        int jump = 2;
        
        while((index + jump) < gis_size &&
              x_GetId(gis, index + jump, use_tis) < key) {
            index += jump;
            jump += jump;
        }
    }
    
    // Translate any that are equal to key (and not yet translated).
    
    // If the sample is an exact match to one (or more) GIs, apply
    // the translation (if we have it) for those GIs.
    
    while((index < gis_size) && (x_GetId(gis,index,use_tis) == key)) {
        if (x_GetOid(gis, index, use_tis) == -1) {
            if ((data + vol_start) < vol_end) {
                if (use_tis) {
                    gis.SetTiTranslation(index, data + vol_start);
                } else {
                    gis.SetTranslation(index, data + vol_start);
                }
            }
        }
        
        advanced = true;
        index ++;
    }
    
    // Continue skipping to eliminate any gi/oid pairs that are
    // already translated.
    
    while((index < gis_size) &&
          (x_GetOid(gis, index, use_tis) != -1)) {
        
        advanced = true;
        index++;
    }
    
    return advanced;
}

inline bool
CSeqDBIsam::x_FindInNegativeList(CSeqDBNegativeList & ids,
                                 int                & index,
                                 Int8                 key,
                                 bool                 use_tis)
{
    bool found = false;
    
    // Skip any that are less than key.
    
    int ids_size = use_tis ? ids.GetNumTis() : ids.GetNumGis();
    
    while((index < ids_size) && (x_GetId(ids, index, use_tis) < key)) {
        index++;
        
        int jump = 2;
        
        while((index + jump) < ids_size &&
              x_GetId(ids, index + jump, use_tis) < key) {
            index += jump;
            jump += jump;
        }
    }
    
    // Check whether the GI or TI was found.
    
    if ((index < ids_size) && (x_GetId(ids,index,use_tis) == key)) {
        found = true;
    }
    
    return found;
}


inline bool
CSeqDBIsam::x_AdvanceIsamIndex(CSeqDBMemLease & index_lease,
                               int            & index,
                               Int8             target_id,
                               Int8           & isam_key,
                               int            & isam_data)
{
    bool advanced = false;
    
    int num_samples = m_NumSamples;
    
    // Use Parabolic Binary Search to find the largest sample that
    // is less-than-or-equal-to the first untranslated target GI.
    
    // The following is basically equivalent to (but faster than):
    //   while(first_gi >= sample[i+1]) i++;
    
    Int8 post_key(0);
    int post_data(0);
    
    while(((index + 1) < num_samples) &&
          (x_TestNumericSample(index_lease,
                               index + 1,
                               target_id,
                               post_key,
                               post_data) >= 0)) {
        
        advanced = true;
        index ++;
        
        isam_key = post_key;
        isam_data = post_data;
        
        // The starting value for jump could be tuned by counting
        // the total number of "test numeric" calls made.
        
        int jump = 2;
        
        while(((index + jump + 1) < num_samples) &&
              (x_TestNumericSample(index_lease,
                                   index + jump + 1,
                                   target_id,
                                   post_key,
                                   post_data) >= 0)) {
            index += jump + 1;
            jump += jump;
            
            isam_key = post_key;
            isam_data = post_data;
        }
    }
    
    return advanced;
}

inline void
CSeqDBIsam::x_MapDataPage(int                sample_index,
                          int              & start,
                          int              & num_elements,
                          SNumericKeyData4 ** data_page_begin,
                          CSeqDBLockHold   & locked)
{
    num_elements =
        x_GetPageNumElements(sample_index, & start);
    
    TIndx offset_begin = start * sizeof(SNumericKeyData4);
    TIndx offset_end =
        offset_begin + sizeof(SNumericKeyData4) * num_elements;
    
    m_Atlas.Lock(locked);
    
    if (! m_DataLease.Contains(offset_begin, offset_end)) {
        m_Atlas.GetRegion(m_DataLease,
                          m_DataFname,
                          offset_begin,
                          offset_end);
    }
    
    *data_page_begin = (SNumericKeyData4*) m_DataLease.GetPtr(offset_begin);
}

inline void
CSeqDBIsam::x_GetDataElement(SNumericKeyData4 * dpage,
                             int                index,
                             Int8             & key,
                             int              & data)
{
    // Only implements 4 byte keys.
    
    key = (int) SeqDB_GetStdOrd((Int4*)  & dpage[index].key);
    data = (int) SeqDB_GetStdOrd((Int4*) & dpage[index].data);
}

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBFILE_HPP


