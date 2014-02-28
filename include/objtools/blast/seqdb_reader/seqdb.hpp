#ifndef OBJTOOLS_BLAST_SEQDB_READER___SEQDB__HPP
#define OBJTOOLS_BLAST_SEQDB_READER___SEQDB__HPP

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


#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objtools/blast/seqdb_reader/seqdbblob.hpp>
#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/blastdb/defline_extra.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <util/sequtil/sequtil.hpp>
#include <util/range.hpp>
#include <set>
#include <objmgr/bioseq_handle.hpp>

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


/// Forward declaration of CSeqDBGiList base class.
class CSeqDBGiList;

/// Forward declaration of CSeqDBIdSet class.
class CSeqDBIdSet;


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
    /// Import type to allow shorter name.
    typedef TSeqDBAliasFileValues TAliasFileValues;
    
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

    /// Converts a CSeqDB sequence type into a human readable string
    static string ESeqType2String(ESeqType type);
    
    /// Types of summary information available.
    enum ESummaryType {
        /// Sum of all sequences, ignoring GI and OID lists and alias files.
        eUnfilteredAll,
        
        /// Values from alias files, or summation over all included sequences.
        eFilteredAll,
        
        /// Sum of included sequences with OIDs within the iteration range.
        eFilteredRange
    };
    
    /// Sequence type accepted and returned for OID indices.
    typedef int TOID;
    
    /// Sequence type accepted and returned for PIG indices.
    typedef int TPIG;
    
    /// Sequence type accepted and returned for GI indices.
    typedef int TGI;

    /// Structure to represent a range
    struct TOffsetPair {
        TSeqPos first;
        TSeqPos second;

        /// Default constructor
        TOffsetPair() : first(0), second(0) {}
        /// Convenient operator to convert to TSeqRange
        operator TSeqRange() const { return TSeqRange(first, second-1); }
    };

    /// List of sequence offset ranges.
    struct TSequenceRanges {
        typedef size_t size_type;
        typedef TOffsetPair value_type;
        typedef const value_type* const_iterator;

    private:
        size_type _size;
        size_type _capacity;
        TSeqPos* _data;

        void x_reset_all() {
            _size = 0;
            _capacity = 0;
            _data = NULL;
        }

        void x_reallocate_if_necessary() {
            static size_t kResizeFactor = 2;
            if (_size + 1 > _capacity) {
                reserve((_capacity + 1) * kResizeFactor -1);
            }
        }

    public:
        TSequenceRanges() {
            x_reset_all();
            reserve(7);   // must reserve at least 1 element
        }

        ~TSequenceRanges() {
            free(_data);
            x_reset_all();
        }

        void clear() { _size = 0; }

        bool empty() const { return _size == 0; }

        size_type size() const { return _size; }

        const_iterator begin() const { return const_iterator(&_data[1]); }

        const_iterator end() const { return const_iterator(&_data[1+ 2*_size]); }

        value_type& operator[](size_type i) { return (value_type &)_data[1+ 2*i]; }

        value_type * get_data() const { return (value_type *) _data; }

        /// Reserves capacity for at least num_elements elements
        /// @throw CSeqDBException in case of memory allocation failure
        void reserve(size_t num_elements) {
            if (num_elements > _capacity) {
                value_type* reallocation =
                    (value_type*) realloc(_data, (num_elements + 1) *
                                          sizeof(value_type));
                if ( !reallocation ) {
                    string msg("Failed to allocate ");
                    msg += NStr::SizetToString(num_elements + 1) + " elements";
                    NCBI_THROW(CSeqDBException, eMemErr, msg);
                }
                _data = (TSeqPos*) reallocation;
                _capacity = num_elements;
            }
        }

        /// Append extra elements at the end
        void append(const void *src, size_type num_elements) {
            reserve(_size + num_elements);
            memcpy(&_data[1+ 2*_size], src, num_elements * sizeof(value_type));
            _size += num_elements;
        }

        /// Append extra element at the end
        void push_back(const value_type& element) {
            x_reallocate_if_necessary();
            append(&element, 1);
        }
    };
    /// String containing the error message in exceptions thrown when a given
    /// OID cannot be found
    static const string kOidNotFound;
    
    /// Short Constructor
    /// 
    /// This version of the constructor assumes memory mapping and
    /// that the entire possible OID range will be included.  Please
    /// use quotes ("") around database names that contains space
    /// characters.
    /// 
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces
    /// @param seqtype
    ///   Specify eProtein, eNucleotide, or eUnknown.
    /// @param gilist
    ///   The database will be filtered by this GI list if non-null.
    CSeqDB(const string & dbname, ESeqType seqtype, CSeqDBGiList * gilist = 0);
    
    /// Short Constructor with Negative ID list.
    /// 
    /// This version of the constructor assumes the entire OID range
    /// will be included, and applies filtering by a negative ID list.
    /// Please use quotes ("") around database names that contains
    /// space characters.
    /// 
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces
    /// @param seqtype
    ///   Specify eProtein, eNucleotide, or eUnknown.
    /// @param nlist
    ///   The database will be filtered to not include these GIs or TIs.
    CSeqDB(const string       & dbname,
           ESeqType             seqtype,
           CSeqDBNegativeList * nlist);
    
    /// Short Constructor with Computed ID list.
    /// 
    /// This version of the constructor takes a computed CSeqDBIdSet
    /// list which can be positive or negative.  This is equivalent to
    /// building a positive or negative list from the IdSet object and
    /// and passing it into one of the previous constructors.
    /// 
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces
    /// @param seqtype
    ///   Specify eProtein, eNucleotide, or eUnknown.
    /// @param ids
    ///   The database will be filtered by this set of IDs.
    CSeqDB(const string & dbname, ESeqType seqtype, CSeqDBIdSet ids);
    
    /// Short Constructor
    /// 
    /// This version of the constructor assumes memory mapping and
    /// that the entire possible OID range will be included.
    /// 
    /// @param dbs
    ///   A list of database or alias names.
    /// @param seqtype
    ///   Specify eProtein, eNucleotide, or eUnknown.
    /// @param gilist
    ///   The database will be filtered by this GI list if non-null.
    CSeqDB(const vector<string> & dbs,
           ESeqType               seqtype,
           CSeqDBGiList         * gilist = 0);
    
    /// Constructor with MMap Flag and OID Range.
    ///
    /// If the oid_end value is specified as zero, or as a value
    /// larger than the number of OIDs, it will be adjusted to the
    /// number of OIDs in the database.  Specifying 0,0 for the start
    /// and end will cause inclusion of the entire database.  This
    /// version of the constructor is obsolete because the sequence
    /// type is specified as a character (eventually only the ESeqType
    /// version will exist).  Please use quotes ("") around database
    /// names that contains space characters.
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
    CSeqDB(const string & dbname,
           ESeqType       seqtype,
           int            oid_begin,
           int            oid_end,
           bool           use_mmap,
           CSeqDBGiList * gi_list = 0);
    
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
    ///   A list of database or alias names.
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
    CSeqDB(const vector<string> & dbname,
           ESeqType               seqtype,
           int                    oid_begin,
           int                    oid_end,
           bool                   use_mmap,
           CSeqDBGiList         * gi_list = 0);
    
    /// Destructor.
    ///
    /// This will return resources acquired by this object, including
    /// any gotten by the GetSequence() call, whether or not they have
    /// been returned by RetSequence().
    ~CSeqDB();
    
    /// Returns the default BLAST database search path
    /// configured for this local installation of BLAST
    static string GenerateSearchPath();
    
    /// Returns the sequence length in base pairs or residues.
    int GetSeqLength(int oid) const;

    /// Returns the first Gi (if any) of the sequence. This method does NOT
    /// check whether the OID in question belongs to the BLAST database after
    /// all filtering is applied (e.g.: GI list restriction or membership bit).
    /// If you need those checks, please use GetGis()
    /// @sa GetGis
    int GetSeqGI(int oid) const;
    
    /// Returns an unbiased, approximate sequence length.
    ///
    /// For protein DBs, this method is identical to GetSeqLength().
    /// In the nucleotide case, computing the exact length requires
    /// examination of the sequence data.  This method avoids doing
    /// that, returning an approximation ranging from L-3 to L+3
    /// (where L indicates the exact length), and unbiased on average.
    int GetSeqLengthApprox(int oid) const;
    
    /// Get the ASN.1 header for the sequence.
    ///
    /// Do not modify the object returned here (e.g. by removing some
    /// of the deflines), as the object is cached internally and
    /// future operations on this OID may be affected.
    ///
    /// @param oid The ordinal ID of the sequence.
    /// @return The blast deflines for this sequence.
    CRef<CBlast_def_line_set> GetHdr(int oid) const;
    
    /// Get taxid for an OID.
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
                   bool            persist = false) const;
    
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
                   bool          persist = false) const;
    
    /// Get a CBioseq for a sequence.
    ///
    /// This builds and returns the header and sequence data
    /// corresponding to the indicated sequence as a CBioseq.  If
    /// target_gi is non-zero or target_seq_id is non-null, the header 
    /// information will be filtered to only include the defline associated 
    /// with that gi/seq_id.
    /// 
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param target_gi
    ///   If nonzero, the target gi to filter the header information by.
    /// @param target_seq_id
    ///   The target seq_id to filter the header information by.
    /// @return
    ///   A CBioseq object corresponding to the sequence.
    CRef<CBioseq> GetBioseq(int             oid, 
                            int             target_gi = 0, 
                            const CSeq_id * target_seq_id = NULL) const;
    
    /// Get a CBioseq for a sequence without sequence data.
    /// 
    /// This builds and returns the data corresponding to the
    /// indicated sequence as a CBioseq, but without the sequence
    /// data.  It is used when processing large sequences, to avoid
    /// accessing unused parts of the sequence.
    /// 
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param target_gi
    ///   If nonzero, the target gi to filter the header information by.
    /// @param target_seq_id
    ///   The target seq_id to filter the header information by.
    /// @return
    ///   A CBioseq object corresponding to the sequence, but without
    ///   sequence data.
    CRef<CBioseq> GetBioseqNoData(int             oid, 
                                  int             target_gi = 0,
                                  const CSeq_id * target_seq_id = NULL) const;

    /// Extract a Blast-def-line-set object from a Bioseq retrieved by CSeqDB
    /// @param bioseq Bioseq retrieved from CSeqDB [in]
    static CRef<CBlast_def_line_set> 
    ExtractBlastDefline(const CBioseq & bioseq);
    /// Extract a Blast-def-line-set object from a Bioseq_Handle retrieved by 
    /// CSeqDB
    /// @param bioseq Bioseq retrieved from CSeqDB [in]
    static CRef<CBlast_def_line_set> 
    ExtractBlastDefline(const CBioseq_Handle& handle);
    
    /// Get a pointer to raw sequence data.
    ///
    /// Get the raw sequence (strand data).  When done, resources
    /// should be returned with RetSequence.  This data pointed to
    /// by *buffer is in read-only memory (where supported).
    /// 
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param buffer
    ///   A returned pointer to the data in the sequence.
    /// @return
    ///   The return value is the sequence length (in base pairs or
    ///   residues).  In case of an error, an exception is thrown.
    int GetSequence(int oid, const char ** buffer) const;
    
    /// Get a pointer to sequence data with ambiguities.
    ///
    /// In the protein case, this is identical to GetSequence().  In
    /// the nucleotide case, it stores 2 bases per byte instead of 4.
    /// The third parameter indicates the encoding for nucleotide
    /// data, either kSeqDBNuclNcbiNA8 or kSeqDBNuclBlastNA8, ignored
    /// if the sequence is a protein sequence.  When done, resources
    /// should be returned with RetSequence.
    /// 
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
    
    /// Get a pointer to a range of sequence data with ambiguities.
    /// 
    /// This is like GetAmbigSeq(), but only a range of the sequence
    /// data is computed and returned.  When done, resources should be
    /// returned with RetSequence.
    /// 
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param buffer
    ///   A returned pointer to the data in the sequence.
    /// @param nucl_code
    ///   The encoding to use for the returned sequence data.
    /// @param begin_offset
    ///   The zero-based offset at which to start translating.
    /// @param end_offset
    ///   The zero-based offset at which to end translation.
    /// @return
    ///   The return value is the subsequence length (in base pairs or
    ///   residues).  In case of an error, an exception is thrown.
    int GetAmbigSeq(int           oid,
                    const char ** buffer,
                    int           nucl_code,
                    int           begin_offset,
                    int           end_offset) const;
    
    /// Get a pointer to sequence data with ambiguities.
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
    /// @param masks
    ///   If not empty, the return sequence will be (hard) masked.
    ///   Masks are cleared on return.
    /// @return
    ///   The return value is the sequence length (in base pairs or
    ///   residues).  In case of an error, an exception is thrown.
    int GetAmbigSeqAlloc(int                oid,
                         char            ** buffer,
                         int                nucl_code,
                         ESeqDBAllocType    strategy,
                         TSequenceRanges  * masks = NULL) const;
    
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
    list< CRef<CSeq_id> > GetSeqIDs(int oid) const;
    
    /// Gets a list of GIs for an OID.
    /// 
    /// This returns the GIs associated with the sequence specified by
    /// the given OID.  If append is true, gis will be appended to the
    /// end of the provided vector; otherwise the vector will be
    /// emptied first.
    /// 
    /// @param oid
    ///   The oid of the sequence.
    /// @param gis
    ///   The returned list of gis.
    /// @param append
    ///   Specify true to append to gis, keeping existing elements.
    void GetGis(int oid, vector<int> & gis, bool append = false) const;
    
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
    /// multiple volumes were accessed, the latest date will
    /// be used.
    string GetDate() const;

    /// Format string for the date returned by CSeqDB::GetDate
    /// @sa CTime
    static const char* kBlastDbDateFormat;

    /// Returns the construction date of the database.
    /// 
    /// @param dbname
    ///   The database name.
    /// @param seqtype
    ///   The type of database (nucleotide or protein)
    /// @return
    ///   The latest date
    static CTime GetDate(const string & dbname,
                         ESeqType       seqtype);
    
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
    /// range.  If either of oid_count or total_length is passed NULL,
    /// that result is not returned.  In some cases, the results can
    /// be computed in constant time; other cases require iteration
    /// proportional to the length of the database or the included OID
    /// range (see SetIterationRange()).
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
                   bool           use_approx = true) const;
    
    /// Returns the length of the largest sequence in the database.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  This might be used to chose buffer sizes.
    int GetMaxLength() const;
    
    /// Returns the length of the shortest sequence in the database.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  This might be used to chose cutoff score.
    int GetMinLength() const;

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
    ///
    /// @param begin_chunk
    ///   First included oid (if eOidRange is returned).
    /// @param end_chunk
    ///   OID after last included (if eOidRange is returned).
    /// @param oid_size
    ///   Number of OID to retrieve (ignored in MT environment)
    /// @param oid_list
    ///   An empty list.  Will contain oid list if eOidList is returned.
    /// @param oid_state
    ///   Optional address of a state variable (for concurrent iterations).
    /// @return
    ///   eOidList in enumeration case, or eOidRange in begin/end range case.
    EOidListType
    GetNextOIDChunk(int         & begin_chunk,       // out
                    int         & end_chunk,         // out
                    int         oid_size,            // in
                    vector<int> & oid_list,          // out
                    int         * oid_state = NULL); // in+out

    /// Resets this object's internal chunk bookmark, which is used when the
    /// oid_state argument to GetNextOIDChunk is NULL. This allows for several
    /// iterations to be performed over the same CSeqDB object
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
    const CSeqDBGiList * GetGiList() const;
    
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
    CSeqDBIdSet GetIdSet() const;
    
    /// Set upper limit on memory and mapping slice size.
    /// 
    /// This sets a (not precisely enforced) upper limit on memory
    /// used by CSeqDB to memory map disk files (and for some large
    /// arrays).  Setting this to a low value may degrade performance.
    /// Setting it to too high a value may cause address space
    /// exhaustion.  Normally, SeqDB will start with a large bound and
    /// reduces it if memory exhaustion is detected.  Applications
    /// that use a lot of memory outside of SeqDB may want to call
    /// this method to scale back SeqDB's demands.  Note that slice
    /// size is no longer externally adjustable and may be removed in
    /// the future.  Also note that if SeqDB detects a map failure, it
    /// will reduce the memory bound.
    /// 
    /// @param membound Maximum memory for SeqDB.
    /// @param slice_size No longer used.
    void SetMemoryBound(Uint8 membound, Uint8 slice_size = 0);
    
    /// Translate a PIG to an OID.
    bool PigToOid(int pig, int & oid) const;
    
    /// Translate an OID to a PIG.
    bool OidToPig(int oid, int & pig) const;
    
    /// Translate a TI to an OID.
    bool TiToOid(Int8 ti, int & oid) const;
    
    /// Translate an OID to a GI.
    bool OidToGi(int oid, int & gi) const;
    
    /// Translate a GI to an OID.
    bool GiToOid(int gi, int & oid) const;
    
    /// Translate a GI to a PIG.
    bool GiToPig(int gi, int & pig) const;
    
    /// Translate a PIG to a GI.
    bool PigToGi(int pig, int & gi) const;
    
    /// Translate an Accession to a list of OIDs.
    void AccessionToOids(const string & acc, vector<int> & oids) const;
    
    /// Translate a Seq-id to a list of OIDs.
    void SeqidToOids(const CSeq_id & seqid, vector<int> & oids) const;
    
    /// Translate a Seq-id to any matching OID.
    bool SeqidToOid(const CSeq_id & seqid, int & oid) const;
    
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
    /// Find the base names of all volumes (and alias nodes).  This 
    /// method builds an alias hierarchy (which should be much faster 
    /// than constructing an entire CSeqDB object), and returns the 
    /// resolved volume/alias file base names from that hierarchy.
    ///
    /// @param dbname
    ///   The input name of the database
    /// @param seqtype
    ///   Specify eProtein, eNucleotide, or eUnknown.
    /// @param paths
    ///   The set of resolved database volume file names
    /// @param alias_paths
    ///   The set of resolved database alias file names
    /// @param recursive
    ///   If true, the search will traverse the full alias node tree
    /// @param expand_links
    ///   If true, the search will expand the soft links
    static void
    FindVolumePaths(const string   & dbname,
                    ESeqType         seqtype,
                    vector<string> & paths,
                    vector<string> * alias_paths = NULL,
                    bool             recursive = true,
                    bool             expand_links = true);
    
    /// Find volume paths
    ///
    /// Find the base names of all volumes.  This method returns the
    /// resolved base names of all referenced blast database volumes.
    ///
    /// @param paths
    ///   The returned set of resolved database path names
    /// @param recursive
    ///   If true, the search will traverse the full alias node tree
    void FindVolumePaths(vector<string> & paths, bool recursive=true) const;
    
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
    /// the filename of each alias file, to a vector of maps from
    /// variable name to value for each entry in that file.  For
    /// example, the value of the "DBLIST" entry in the "wgs.nal" file
    /// would be values["wgs"][0]["DBLIST"].  The lines returned have
    /// been processed somewhat by SeqDB, including normalizing tabs
    /// to whitespace, trimming leading and trailing whitespace, and
    /// removal of comments and other non-value lines.  Care should be
    /// taken when using the values returned by this method.  SeqDB
    /// uses an internal "virtual" alias file entry, which maps from a
    /// filename of "-" and contains a single entry mapping "DBLIST"
    /// to SeqDB's database name input.  This entry is the root of the
    /// alias file inclusion tree.  Also note that alias files that
    /// appear in several places in the alias file inclusion tree may
    /// be different -- SeqDB's internal editing distributes GI lists
    /// over sub-alias files, which is why the value type of the
    /// returned data is a vector.
    /// 
    /// @param afv
    ///   The alias file contents will be returned here.
    void GetAliasFileValues(TAliasFileValues & afv);
    
    /// Get taxonomy information
    /// 
    /// This method returns taxonomy information for a single taxid.
    /// This information does not vary with sequence type (protein
    /// vs. nucleotide) and is the same for all blast databases.  If
    /// the taxonomy database is not available or the taxid is not
    /// found, this method will throw an exception.
    /// 
    /// @param taxid
    ///   An integer identifying the taxid to fetch.
    /// @param info
    ///   A structure containing taxonomic description strings.
    static void GetTaxInfo(int taxid, SSeqDBTaxInfo & info);

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
    
    /// Get a sequence in a given encoding.
    ///
    /// This method gets the sequence data for the given OID, converts
    /// it to the specified encoding, and returns it in a string.  It
    /// supports all values of the CSeqUtil::ECoding enumeration (but
    /// the type must match the database type).  This method returns the
    /// same data as GetAmbigSeq() (or GetSequence() for protein), but
    /// may be less efficient due to the cost of translation and string
    /// allocation.
    ///
    /// @param oid The OID of the sequence to fetch.
    /// @param coding The encoding to use for the data.
    /// @param output The returned sequence data as a string.
    /// @param range The range of the sequence to retrieve, if empty, the
    /// entire sequence will be retrived [in]
    void GetSequenceAsString(int                 oid,
                             CSeqUtil::ECoding   coding,
                             string            & output,
                             TSeqRange           range = TSeqRange()) const;
    
    /// Get a sequence in a readable text encoding.
    ///
    /// This method gets the sequence data for an OID, converts it to a
    /// human-readable encoding (either Iupacaa for protein, or Iupacna
    /// for nucleotide), and returns it in a string.  This is equivalent
    /// to calling the three-argument versions of this method with those
    /// encodings.
    ///
    /// @param oid The OID of the sequence to fetch.
    /// @param output The returned sequence data as a string.
    /// @param range The range of the sequence to retrieve, if empty, the
    /// entire sequence will be retrived [in]
    void GetSequenceAsString(int oid, 
                             string & output,
                             TSeqRange range = TSeqRange()) const;
    

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
    
    /// Look up the value for a specific column metadata key.
    ///
    /// Columns can contain user-defined metadata as a list of
    /// key-value pairs.  For the specified column, this returns the
    /// value associated with one particular key.
    ///
    /// @param column_id The column id from GetColumnId. [in]
    /// @return The value corresponding to the specified key. [out]
    const string & GetColumnValue(int column_id, const string & key);
    
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
    
    /// Fetch the data blob for the given column and oid.
    /// @param col_id The column to fetch data from. [in]
    /// @param oid    The OID of the blob. [in]
    /// @param blob   The data will be returned here. [out]
    void GetColumnBlob(int col_id, int oid, CBlastDbBlob & blob);
    
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

    /// Get the numeric algorithm ID for a string. 
    /// @param algo_name The name of the filtering algorithm
    int GetMaskAlgorithmId(const string &algo_name) const;

    /// Returns a formatted string with the list of available masking
    /// algorithms in this database for display purposes (i.e.: help)
    string GetAvailableMaskAlgorithmDescriptions();
    
    /// Validates the algorithm IDs passed to this function, returning a vector
    /// of those algorithm IDs not present in this object
    vector<int> ValidateMaskAlgorithms(const vector<int>& algorithm_ids);
    
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
    /// @param algorithm_id The ID as from GetAvailableMaskAlgorithms [in]
    /// @param program The filtering program used (DUST, SEG, etc.) [out]
    /// @param program_name string representation of program [out]
    /// @param algo_opts Describes options passed to `program'. [out]
    void GetMaskAlgorithmDetails(int                 algorithm_id,
                                 objects::EBlast_filter_program & program,
                                 string            & program_name,
                                 string            & algo_opts);
    
    void GetMaskAlgorithmDetails(int                 algorithm_id,
                                 string            & program,
                                 string            & program_name,
                                 string            & algo_opts);
    
    /// Get masked ranges of a sequence.
    ///
    /// For the provided OID and list of algorithm IDs, this method
    /// gets a list of masked areas of those sequences for the first
    /// algorithm ID.  The list of masked areas is returned via the 
    /// ranges parameter.
    ///
    /// @param oid The ordinal ID of the sequence. [in]
    /// @param algo_id The algorithm ID to get data for. [in]
    /// @param ranges The list of sequence offset ranges. [out]
    NCBI_DEPRECATED 
    void GetMaskData(int                 oid,
                     const vector<int> & algo_ids,
                     TSequenceRanges   & ranges) 
    {
        GetMaskData(oid, algo_ids[0], ranges);
    }         

    /// Get masked ranges of a sequence.
    ///
    /// For the provided OID and algorithm ID, this method
    /// gets a list of masked areas of those sequences.  The list of
    /// masked areas is returned via the ranges parameter.
    ///
    /// @param oid The ordinal ID of the sequence. [in]
    /// @param algo_id The algorithm ID to get data for. [in]
    /// @param ranges The list of sequence offset ranges. [out]
    void GetMaskData(int              oid,
                     int              algo_id,
                     TSequenceRanges &ranges);
#endif

    /// Invoke the garbage collector to free up memory
    void GarbageCollect(void);
    
    /***********************************************************************/
    /* BEGIN: support for partial sequence fetching                        */
    
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

    /// Remove any offset ranges for the given OID
    /// @param oid           OID of the sequence.
    void RemoveOffsetRanges(int oid);

    /// Flush all offset ranges cached
    void FlushOffsetRangeCache();

    /* END: support for partial sequence fetching                          */
    /***********************************************************************/

    /// Setting the number of threads
    ///
    /// This should be called by the master thread, before and after 
    /// multiple threads run.
    /// 
    /// @param num_threads   Number of threads
    void SetNumberOfThreads(int num_threads);

    /// Retrieve the current slice size used for mmap
    Int8 GetSliceSize() const;

    /// Retrieve the disk usage in bytes for this BLAST database
    Int8 GetDiskUsage() const;
protected:
    /// Implementation details are hidden.  (See seqdbimpl.hpp).
    class CSeqDBImpl * m_Impl;
    
    /// No-argument Constructor
    /// 
    /// This version of the constructor is used as an extension by the
    /// 'expert' interface in seqdbexpert.hpp.
    CSeqDB();
};

/// Structure to define basic information to initialize a BLAST DB
struct NCBI_XOBJREAD_EXPORT SSeqDBInitInfo : public CObject {
    /// The BLAST DB name
    string m_BlastDbName;
    /// The molecule type
    CSeqDB::ESeqType m_MoleculeType;

    /// Default constructor
    SSeqDBInitInfo() {
        m_MoleculeType = CSeqDB::eUnknown;
    }

    /// operator less to support sorting
    inline bool operator<(const SSeqDBInitInfo& rhs) const {
        if (m_BlastDbName < rhs.m_BlastDbName) {
            return true;
        } else if (m_BlastDbName > rhs.m_BlastDbName) {
            return false;
        } else {
            return ((int)m_MoleculeType < (int)rhs.m_MoleculeType);
        }
    }

    /// Create a new CSeqDB instance from this object
    CRef<CSeqDB> InitSeqDb() const {
        return CRef<CSeqDB>(new CSeqDB(m_BlastDbName, m_MoleculeType)); 
    }
};

/// Find BLAST DBs in the directory specified
/// @param path directory to search BLAST DBs [in]
/// @param dbtype BLAST DB molecule type, allowed values are 'prot', 'nucl',
/// and 'guess' (which means any) [in]
/// @param recurse whether BLAST DBs should be found recursively or not [in]
/// @param include_alias_files Should alias files be included also? [in]
/// @param remove_redundant_dbs Should BLASTDBs that are referenced by other
/// alias files in the return value be removed? [in]
NCBI_XOBJREAD_EXPORT 
vector<SSeqDBInitInfo>
FindBlastDBs(const string& path, const string& dbtype, bool recurse,
             bool include_alias_files = false,
             bool remove_redundant_dbs = false);

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

/// Convert a string to a CSeqDB ESeqType object
/// @param str string containing the molecule type (e.g.: prot, nucl, guess)
NCBI_XOBJREAD_EXPORT 
CSeqDB::ESeqType ParseMoleculeTypeString(const string& str);

/// Deletes all files associated with a BLAST database
/// @param dbpath BLAST database file path [in]
/// @param seq_type Sequence type [in]
/// @return true if relevant files were deleted, else false
NCBI_XOBJREAD_EXPORT 
bool DeleteBlastDb(const string& dbpath, CSeqDB::ESeqType seq_type);

/// Class to facilitate trimming down the list of WGS BLAST DBs so to those
/// relevant to the GIs provided. (WB-1206, WB-1069)
/// @note this class relies on the environment variable WGS_GILIST_DIR 
class NCBI_XOBJREAD_EXPORT CWgsDbTrimmer {
public:
    /// Constructor which takes a space separated list of WGS BLAST DBs
    CWgsDbTrimmer(const string& wgs_db_list);
    /// Add a gi that appears in the BLAST results, so that the resulting
    /// WGS BLAST DB list can be trimmed on the basis of this
    void AddGi(TGi gi) { m_Gis.insert(gi); }
    /// Builds a space separated, trimmed list of WGS BLAST DBs which contain
    /// all of the GIs, provided to this object until this method is called.
    /// Those GIs that aren't found in the original list of WGS BLASTDBs
    /// provided in the constructor are silently ignored.
    /// @note the list of BLAST DBs will be returned in alphabetical order
    string GetDbList();
    /// Returns the BLASTDB list originally provided in the constructor
    string GetOrigDbList() const { return m_OrigWgsList; }
private:
    typedef map<string, vector<int> > TGiLists;
    /// Reads the gi lists for each of the BLAST DBs provided in the
    /// constructor, ignoring those that can't be found
    TGiLists x_ReadGiListsForDbs();
    /// Returns the WGS BLAST DBs provided in the constructor into a set
    set<string> x_ExtractOriginalWgsDbs();
    string m_OrigWgsList;
    set<TGi> m_Gis;
    /// Path where the WGS GI list files are cached (pointed to by WGS_GILIST_DIR)
    string m_Path;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_BLAST_SEQDB_READER___SEQDB__HPP

