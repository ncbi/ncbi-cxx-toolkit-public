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
    /// Identifier formats used by this class.
    enum EIdentType {
        eGi,       /// Genomic ID is a relatively stable numeric identifier for sequences.
        ePig,      /// Each PIG identifier refers to exactly one protein sequence.
        eStringID, /// Some sequence sources uses string identifiers.
        eOID       /// The ordinal id indicates the order of the data in the volume's index file.
    };
    
    /// Types of database this class can access.
    enum EIsamDbType {
        eNumeric         = 0, /// Numeric database with Key/Value pairs in the index file.
        eNumericNoData   = 1, /// Numeric database without Key/Value pairs in the index file.
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
    
    /// Constructor
    /// 
    /// An ISAM file object corresponds to an index file and a data
    /// file, and converts identifiers (string, GI, or PIG) into OIDs
    /// relative to a particular database volume.
    /// 
    /// @param atlas
    ///   The memory management object.
    /// @param dbname
    ///   The name of the volume's files (minus the extension).
    /// @param prot_nucl
    ///   Whether the sequences are protein or nucleotide.
    /// @param file_ext_char
    ///   This is 's', 'n', or 'p', for string, GI, or PIG, respectively.
    /// @param ident_type
    ///   The type of identifiers this database translates.
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
    ///   The PIG to look up.
    /// @param oid
    ///   The returned oid.
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   true if the PIG was found
    bool PigToOid(TPig pig, TOid & oid, CSeqDBLockHold & locked)
    {
        _ASSERT(m_IdentType == ePig);
        return x_IdentToOid(pig, oid, locked);
    }
    
    /// GI translation
    /// 
    /// A GI identifier is translated to an OID.  GI identifiers are
    /// used exclusively for all types of sequences.  Multiple GIs may
    /// indicate the same sequence of bases (ie. if several research
    /// projects sequenced a gene and found the same results).
    /// 
    /// @param gi
    ///   The GI to look up.
    /// @param oid
    ///   The returned oid.
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   true if the GI was found
    bool GiToOid(TGi gi, TOid & oid, CSeqDBLockHold & locked)
    {
        _ASSERT(m_IdentType == eGi);
        return x_IdentToOid(gi, oid, locked);
    }
    
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
    ///   The string to look up.
    /// @param oids
    ///   The returned oids.
    /// @param adjusted
    ///   Whether the simplification routines adjusted the string.
    /// @param locked
    ///   The lock hold object for this thread.
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
    ///   A string containing the Seq-id.
    /// @param oid
    ///   The returned oid.
    /// @param locked
    ///   The lock hold object for this thread.
    bool SeqidToOid(const string & acc, TOid & oid, CSeqDBLockHold & locked);
    
    /// String id simplification.
    /// 
    /// This routine tries to produce a numerical type from a string
    /// identifier.  SeqDB can use faster lookup mechanisms if a PIG,
    /// GI, or OID type can be recognized in the string, for example.
    /// Even when the output is a string, it may be better formed for
    /// the purpose of lookup in the string ISAM file.
    /// 
    /// @param acc
    ///   The string to look up.
    /// @param num_id
    ///   The returned identifier, if numeric.
    /// @param str_id
    ///   The returned identifier, if a string.
    /// @param simpler
    ///   Whether an adjustment was done at all.
    /// @return
    ///   The resulting identifier type.
    static EIdentType
    TryToSimplifyAccession(const string & acc,
                           int          & num_id,
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
    ///   The Seq-id to look up.
    /// @param acc
    ///   The original string the Seq-id was created from (or NULL).
    /// @param num_id
    ///   The returned identifier, if numeric.
    /// @param str_id
    ///   The returned identifier, if a string.
    /// @param simpler
    ///   Whether an adjustment was done at all.
    /// @return
    ///   The resulting identifier type.
    static EIdentType
    SimplifySeqid(CSeq_id       & bestid,
                  const string  * acc,
                  int           & num_id,
                  string        & str_id,
                  bool          & simpler);
    
private:
    /// Exit conditions occurring in this code.
    enum EErrorCode {
        eNotFound        =  1,   /// The key was not found
        eNoError         =  0,   /// Lookup was successful
        eBadVersion      =  -10, /// The format version of the ISAM file is unsupported.
        eBadType         =  -11, /// The requested ISAM type did not match the file.
        eWrongFile       =  -12  /// The file was not found, or was the wrong length.
    };
    
    /// The key-value pair type for numerical indices
    struct SNumericKeyData {
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
    bool x_IdentToOid(int              id,
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
    x_SearchIndexNumeric(int              Number, 
                         int            * Data,
                         Uint4          * Index,
                         Int4           & SampleNum,
                         bool           & done,
                         CSeqDBLockHold & locked);
    
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
    x_SearchDataNumeric(int              Number,
                        int            * Data,
                        Uint4          * Index,
                        Int4             SampleNum,
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
    x_NumericSearch(int              Number,
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
    
    // Data
    
    /// The memory management layer
    CSeqDBAtlas & m_Atlas;
    
    /// The type of identifier this class uses
    EIdentType m_IdentType;
    
    /// A persistent lease on the ISAM index file.
    CSeqDBMemLease m_IndexLease;
    
    /// A persistent lease on the ISAM data file.
    CSeqDBMemLease m_DataLease;
    
    /// The format type of database files found.
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
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBFILE_HPP


