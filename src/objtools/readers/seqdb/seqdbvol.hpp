#ifndef OBJTOOLS_READERS_SEQDB__SEQDBVOL_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBVOL_HPP

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

/// @file seqdbvol.hpp
/// Defines database volume access classes.
///
/// Defines classes:
///     CSeqDBVol
///
/// Implemented for: UNIX, MS-Windows

#include "seqdbatlas.hpp"
#include "seqdbgeneral.hpp"
#include "seqdbtax.hpp"
#include <objects/seq/seq__.hpp>

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

/// CSeqDBRangeList
///
/// This class maintains a list of ranges of sequence offsets that are
/// desired for performance optimization.  For large sequences that
/// need to be unpacked, this class describes the subsets of those
/// sequences that will actually be used.  Each instance of this class
/// corresponds to sequence data for one OID.

class CSeqDBRangeList : public CObject {
public:
    /// Constructor.
    /// @param atlas The SeqDB memory management layer.
    CSeqDBRangeList(CSeqDBAtlas & atlas)
        : m_Atlas     (atlas),
          m_CacheData (false),
          m_Sequence  (0),
          m_Length    (0),
          m_RefCount  (0)
    {
        // Sequence caching is not implemented yet.  It would increase
        // performance further, but requires some consideration of the
        // design with respect to locking and correctness.
    }
    
    /// Destructor
    ~CSeqDBRangeList()
    {
        FlushSequence();
    }
    
    /// Returns true if the sequence data is cached.
    bool IsCached()
    {
        return false;
    }
    
    /// List of sequence offset ranges.
    typedef set< pair<int, int> > TRangeList;
    
    /// Set ranges of the sequence that will be used.
    /// @param ranges Offset ranges of the sequence that are needed.
    /// @param append_ranges If true, combine new ranges with old.
    /// @param cache_data If true, SeqDB is allowed to cache data.
    void SetRanges(const TRangeList & ranges,
                   bool               append_ranges,
                   bool               cache_data);
    
    /// Get ranges of sequence offsets that will be used.
    const TRangeList & GetRanges()
    {
        return m_Ranges;
    }
    
    /// Flush cached sequence data (if any).
    void FlushSequence()
    {
    }
    
    /// Sequences shorter than this will not use ranges in any case.
    static int ImmediateLength()
    {
        return 10240;
    }
    
private:
    /// Memory management layer.
    CSeqDBAtlas & m_Atlas;
    
    /// Range of offsets needed for this sequence.
    TRangeList m_Ranges;
    
    /// True if caching of sequence data is required for this sequence.
    bool m_CacheData;
    
    /// Pointer to cached sequence data.
    const char * m_Sequence;
    
    /// Length of sequence.
    int m_Length;
    
    /// Number of user-held references to this sequence.
    int m_RefCount;
};

/// CSeqDBVol class
/// 
/// This object defines access to one database volume.  It aggregates
/// file objects associated with the sequence and header data, and
/// ISAM objects used for translation of GIs and PIGs for data in this
/// volume.  The extensions managed here include those with file
/// extensions (pin, phr, psq, nin, nhr, and nsq), plus the optional
/// ISAM objects via the CSeqDBIsam class.

class CSeqDBVol {
public:
    /// Import TIndx definition from the CSeqDBAtlas class.
    typedef CSeqDBAtlas::TIndx   TIndx;
    
    /// Constructor
    ///
    /// All files connected with the database volume will be opened,
    /// metadata about the volume will be read from the index file,
    /// and identifier translation indices will be opened.  The name
    /// of these files is the specified name of the volume plus an
    /// extension.
    /// 
    /// @param atlas
    ///   The memory management layer object.
    /// @param name
    ///   The base name of the volumes files.
    /// @param prot_nucl
    ///   The sequence type, kSeqTypeProt, or kSeqTypeNucl.
    /// @param user_gilist
    ///   If specified, will be used to filter deflines by GI.
    /// @param locked
    ///   The lock holder object for this thread.
    CSeqDBVol(CSeqDBAtlas    & atlas,
              const string   & name,
              char             prot_nucl,
              CSeqDBGiList   * user_gilist,
              CSeqDBLockHold & locked);
    
    /// Sequence length for protein databases
    /// 
    /// This method returns the length of the sequence in bases, and
    /// should only be called for protein sequences.  It does not
    /// require synchronization via the atlas object's lock.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param locked
    ///     The lock holder object for this thread
    /// @return
    ///   The length in bases of the sequence
    int GetSeqLengthProt(int oid, CSeqDBLockHold & locked) const;
    
    /// Approximate sequence length for nucleotide databases
    /// 
    /// This method returns the length of the sequence using a fast
    /// method that may be off by as much as 4 bases.  The method is
    /// designed to be unbiased, meaning that the total length of
    /// large numbers of sequences will approximate what the exact
    /// length would be.  The approximate lengths will change if the
    /// database is regenerated.  It does not require synchronization.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param locked
    ///     The lock holder object for this thread
    /// @return
    ///   The approximate length in bases of the sequence
    int GetSeqLengthApprox(int oid, CSeqDBLockHold & locked) const;
    
    /// Exact sequence length for nucleotide databases
    /// 
    /// This method returns the length of the sequence in bases, and
    /// should only be called for nucleotide sequences.  It requires
    /// synchronization via the atlas object's lock, which must be
    /// done in the calling code.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param locked
    ///     The lock holder object for this thread
    /// @return
    ///   The length in bases of the sequence
    int GetSeqLengthExact(int oid, CSeqDBLockHold & locked) const;
    
    /// Get filtered sequence header information
    /// 
    /// This method returns the set of Blast-def-line objects stored
    /// for each sequence.  These contain descriptive information
    /// related to the sequence.  If have_oidlist is true, and
    /// memb_bit is nonzero, only deflines with that membership bit
    /// set will be returned.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param have_oidlist
    ///   True if the database is filtered
    /// @param membership_bit
    ///   Membership bit to filter deflines
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The set of blast-def-lines describing this sequence
    CRef<CBlast_def_line_set>
    GetFilteredHeader(int                  oid,
                      bool                 have_oidlist,
                      int                  membership_bit,
                      CSeqDBLockHold     & locked) const;
    
    /// Get the sequence type stored in this database
    /// 
    /// This method returns the type of sequences stored in this
    /// database, either kSeqTypeProt for protein, or kSeqTypeNucl for
    /// nucleotide.
    /// 
    /// @return
    ///   Either kSeqTypeProt for protein, or kSeqTypeNucl for nucleotide
    char GetSeqType() const;
    
    /// Get a CBioseq object for this sequence
    /// 
    /// This method builds and returns a Bioseq for this sequence.
    /// The taxonomy information is cached in this volume, so it
    /// should not be modified directly, or other Bioseqs from this
    /// SeqDB object may be affected.  If the CBioseq has an OID list,
    /// and it uses a membership bit, the deflines included in the
    /// CBioseq will be filtered based on the membership bit.  Zero
    /// for the membership bit means no filtering.  Filtering can also
    /// be done by a GI, in which case, only the defline matching that
    /// GI will be returned.  The seqdata parameter can be specified
    /// as false to indicate that sequence data should not be included
    /// in this object; in this case the CSeq_inst object attached to
    /// the bioseq will be configured to a "not set" state.  This is
    /// used to allow Bioseq summary data to be provided without the
    /// performance penalty of loading (possibly very large) sequence
    /// data from disk.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param have_oidlist
    ///   Specify true if the database is filtered by an oidlist
    /// @param memb_bit
    ///   If specified, only deflines matching this bit will be returned
    /// @param pref_gi
    ///   If specified, only deflines containing this GI will be returned
    /// @param tax_info
    ///   The taxonomy database object
    /// @param seqdata
    ///   Include sequence data in the returned Bioseq.
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   A CBioseq describing this sequence
    CRef<CBioseq>
    GetBioseq(int                   oid,
              bool                  have_oidlist,
              int                   memb_bit,
              int                   pref_gi,
              CRef<CSeqDBTaxInfo>   tax_info,
              bool                  seqdata,
              CSeqDBLockHold      & locked) const;
    
    /// Get the sequence data
    /// 
    /// This method gets the sequence data, returning a pointer and
    /// the length of the sequence.  The atlas will be locked, but the
    /// lock may also be returned during this method.  The computation
    /// of the length of a nucleotide sequence involves a one byte
    /// read that is likely to cause a page fault.  Releasing the
    /// atlas lock before this (potential) page fault can help the
    /// average performance in the multithreaded case.  It is safe to
    /// release the lock because the sequence data is pinned down by
    /// the reference count we have acquired to return to the user.
    /// The returned sequence data is intended for blast searches, and
    /// will contain random values in any ambiguous regions.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param buffer
    ///   The returned sequence data
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The length of this sequence in bases
    int GetSequence(int oid, const char ** buffer, CSeqDBLockHold & locked) const
    {
        return x_GetSequence(oid, buffer, true, locked, true);
    }
    
    /// Get a sequence with ambiguous regions.
    /// 
    /// This method gets the sequence data, returning a pointer and
    /// the length of the sequence.  For nucleotide sequences, the
    /// data can be returned in one of two encodings.  Specify either
    /// (kSeqDBNuclNcbiNA8) for NCBI/NA8, or (kSeqDBNuclBlastNA8) for
    /// Blast/NA8.  The data can also be allocated in one of three
    /// ways, enumerated in ESeqDBAllocType.  Specify eAtlas to use
    /// the Atlas code, eMalloc to use the malloc() function, or eNew
    /// to use the new operator.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param buffer
    ///   The returned sequence data
    /// @param nucl_code
    ///   The encoding of the returned sequence data
    /// @param alloc_type
    ///   The allocation routine used
    /// @param region
    ///   If non-null, the offset range to get
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The length of this sequence in bases
    int GetAmbigSeq(int               oid,
                    char           ** buffer,
                    int               nucl_code,
                    ESeqDBAllocType   alloc_type,
                    SSeqDBSlice     * region,
                    CSeqDBLockHold  & locked) const;
    
    /// Get the Seq-ids associated with a sequence
    /// 
    /// This method returns a list containing all the CSeq_id objects
    /// associated with a sequence.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param have_oidlist
    ///   True if the database is filtered
    /// @param membership_bit
    ///   Membership bit to filter deflines
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The list of Seq-id objects for this sequences
    list< CRef<CSeq_id> > GetSeqIDs(int                  oid,
                                    bool                 have_oidlist,
                                    int                  membership_bit,
                                    CSeqDBLockHold     & locked) const;
    
    /// Get the volume title
    string GetTitle() const;
    
    /// Get the formatting date of the volume
    string GetDate() const;
    
    /// Get the number of OIDs for this volume
    int    GetNumOIDs() const;
    
    /// Get the total length of this volume (in bases)
    Uint8  GetVolumeLength() const;
    
    /// Get the length of the largest sequence in this volume
    int    GetMaxLength() const;
    
    /// Get the volume name
    string GetVolName() const
    {
        return m_VolName;
    }
    
    /// Return expendable resources held by this volume
    /// 
    /// This volume holds resources acquired via the atlas.  This
    /// method returns all such resources which can be automatically
    /// reacquired (but not, for example, the index file data).
    void UnLease();
    
    /// Find the OID given a PIG
    ///
    /// A lookup is done for the PIG, and if found, the corresponding
    /// OID is returned.
    ///
    /// @param pig
    ///   The pig to look up
    /// @param oid
    ///   The returned ordinal ID
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   True if the PIG was found
    bool PigToOid(int pig, int & oid, CSeqDBLockHold & locked) const;
    
    /// Find the PIG given an OID
    /// 
    /// If this OID is associated with a PIG, the PIG is returned.
    /// 
    /// @param oid
    ///   The oid of the sequence
    /// @param pig
    ///   The returned PIG
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   True if a PIG was returned
    bool GetPig(int oid, int & pig, CSeqDBLockHold & locked) const;
    
    /// Find the OID given a GI
    ///
    /// A lookup is done for the GI, and if found, the corresponding
    /// OID is returned.
    ///
    /// @param gi
    ///   The gi to look up
    /// @param oid
    ///   The returned ordinal ID
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   True if an OID was returned
    bool GiToOid(int gi, int & oid, CSeqDBLockHold & locked) const;
    
    /// Find the GI given an OID
    ///
    /// If this OID is associated with a GI, the GI is returned.
    ///
    /// @param oid
    ///   The oid of the sequence
    /// @param gi
    ///   The returned GI
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   True if a GI was returned
    bool GetGi(int oid, int & gi, CSeqDBLockHold & locked) const;
    
    /// Find OIDs for the specified accession or formatted Seq-id
    ///
    /// An attempt will be made to simplify the string by parsing it
    /// into a list of Seq-ids.  If this works, the best Seq-id (for
    /// lookup purposes) will be formatted and the resulting string
    /// will be looked up in the string ISAM file.  The resulting set
    /// of OIDs will be returned.  If the string is not found, the
    /// array will be left empty.  Most matches only produce one OID.
    ///
    /// @param acc
    ///   An accession string or formatted Seq-id for which to search
    /// @param oids
    ///   A set of OIDs found for this sequence
    /// @param locked
    ///   The lock holder object for this thread
    void AccessionToOids(const string   & acc,
                         vector<int>    & oids,
                         CSeqDBLockHold & locked) const;
    
    /// Find OIDs for the specified Seq-id
    ///
    /// The Seq-id will be formatted and the resulting string will be
    /// looked up in the string ISAM file.  The resulting set of OIDs
    /// will be returned.  If the string is not found, the array will
    /// be left empty.  Most matches only produce one OID.
    ///
    /// @param seqid
    ///   A Seq-id for which to search
    /// @param oids
    ///   A set of OIDs found for this sequence
    /// @param locked
    ///   The lock holder object for this thread
    void SeqidToOids(CSeq_id        & seqid,
                     vector<int>    & oids,
                     CSeqDBLockHold & locked) const;
    
    /// Find the OID at a given index into the database
    ///
    /// This method considers the database as one long array of bases,
    /// and finds the base at an offset into that array.  The sequence
    /// nearest that base is determined, and the sequence's OID is
    /// returned.  The OIDs are assigned to volumes in a different
    /// order than with the readdb library, which can be an issue when
    /// splitting the database for load balancing purposes.  When
    /// computing the OID range, be sure to use GetNumOIDs(), not
    /// GetNumSeqs().
    ///
    /// @param first_seq
    ///   This OID or later is always returned
    /// @param residue
    ///   The position to find relative to the total length
    /// @param locked
    ///   The lock holder object for this thread.
    /// @return
    ///   The OID of the sequence nearest the specified residue
    int GetOidAtOffset(int              first_seq,
                       Uint8            residue,
                       CSeqDBLockHold & locked) const;
    
    /// Translate Gis to Oids for the given vector of Gi/Oid pairs.
    ///
    /// This method iterates over a vector of Gi/Oid pairs.  For each
    /// pair where OID is -1, the GI will be looked up in the ISAM
    /// file, and (if found) the correct OID will be stored (otherwise
    /// the -1 will remain).  This method will normally be called once
    /// for each volume.
    ///
    /// @param vol_start
    ///   The starting OID of this volume.
    /// @param vol_end
    ///   The fist OID past the end of this volume.
    /// @param gis
    ///   The set of GI-OID pairs.
    /// @param locked
    ///   The lock holder object for this thread
    void GisToOids(int              vol_start,
                   int              vol_end,
                   CSeqDBGiList   & gis,
                   CSeqDBLockHold & locked) const;
    
    /// Filter this volume using the specified GI list.
    ///
    /// A volume can be filtered by a GI list.  This method attaches a
    /// GI list to the volume, in addition to any GI lists that are
    /// already attached.
    ///
    /// @param gilist
    ///   A list of GIs to use as a filter.
    void AttachVolumeGiList(CRef<CSeqDBGiList> gilist) const
    {
        m_VolumeGiLists.push_back(gilist);
    }
    
    /// Simplify the GI list configuration.
    ///
    /// When all user and volume GI lists have been attached, the user
    /// GI list may be removed; this is only possible if neither the
    /// user nor volume GI lists contain Seq-id data.
    void OptimizeGiLists() const;
    
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
    /// @param locked The lock holder object for this thread. [in|out]
    /// @return The sequence data as a Seq-data object.
    CRef<CSeq_data> GetSeqData(int              oid,
                               TSeqPos          begin,
                               TSeqPos          end,
                               CSeqDBLockHold & locked) const;
    
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
    /// @param seq_length Returned length of the ambiguity data.
    /// @param locked Lock holder object for this thread.
    void GetRawSeqAndAmbig(int              oid,
                           const char    ** buffer,
                           int            * seq_length,
                           int            * ambig_length,
                           CSeqDBLockHold & locked) const;
    
    /// Get GI Bounds.
    /// 
    /// Fetch the lowest, highest, and total number of GIs.  If the
    /// operation fails, zero will be returned for count.
    /// 
    /// @param low_id Lowest GI value in database. [out]
    /// @param high_id Highest GI value in database. [out]
    /// @param count Number of GI values in database. [out]
    /// @param locked Lock holder object for this thread. [in]
    void GetGiBounds(int            & low_id,
                     int            & high_id,
                     int            & count,
                     CSeqDBLockHold & locked) const;
    
    /// Get PIG Bounds.
    /// 
    /// Fetch the lowest, highest, and total number of PIGs.  If the
    /// operation fails, zero will be returned for count.
    /// 
    /// @param low_id Lowest PIG value in database. [out]
    /// @param high_id Highest PIG value in database. [out]
    /// @param count Number of PIG values in database. [out]
    /// @param locked Lock holder object for this thread. [in]
    void GetPigBounds(int            & low_id,
                      int            & high_id,
                      int            & count,
                      CSeqDBLockHold & locked) const;
    
    /// Get String Bounds.
    /// 
    /// Fetch the lowest, highest, and total number of string keys in
    /// the database index.  If the operation fails, zero will be
    /// returned for count.
    /// 
    /// @param low_id Lowest string value in database. [out]
    /// @param high_id Highest string value in database. [out]
    /// @param count Number of string values in database. [out]
    /// @param locked Lock holder object for this thread. [in]
    void GetStringBounds(string         & low_id,
                         string         & high_id,
                         int            & count,
                         CSeqDBLockHold & locked) const;
    
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
    /// @param locked        Lock holder object for this thread.
    void SetOffsetRanges(int                oid,
                         const TRangeList & offset_ranges,
                         bool               append_ranges,
                         bool               cache_data,
                         CSeqDBLockHold   & locked) const;
    
private:
    /// A set of GI lists.
    typedef vector< CRef<CSeqDBGiList> > TGiLists;
    
    /// Returns true if this volume has a GI list.
    bool x_HaveGiList() const
    {
        return ! (m_UserGiList.Empty() && m_VolumeGiLists.empty());
    }
    
    /// Returns true if this volume's GI list, if any, has this GI.
    void x_FilterHasId(const CSeq_id & id,
                       bool          & have_user,
                       bool          & have_vol) const
    {
        if (! have_user) {
            if (m_UserGiList.Empty()) {
                have_user = true;
            } else {
                have_user |= x_ListHasId(*m_UserGiList, id);
            }
        }
        
        if (! have_vol) {
            if (m_VolumeGiLists.empty()) {
                have_vol = true;
            } else {
                NON_CONST_ITERATE(TGiLists, gilist, m_VolumeGiLists) {
                    if (x_ListHasId(**gilist, id)) {
                        have_vol = true;
                        break;
                    }
                }
            }
        }
    }
    
    /// Returns true if this volume's GI list, if any, has this GI.
    bool x_ListHasId(CSeqDBGiList & L, const CSeq_id & id) const
    {
        int oid = -1;
        
        if (id.IsGi()) {
            if (L.GiToOid(id.GetGi(), oid) && (oid != -1)) {
                return true;
            }
        }
        
        return L.SeqIdToOid(id, oid) && (oid != -1);
    }
    
    /// Get sequence header object
    /// 
    /// This method reads the sequence header information into an
    /// ASN.1 object and returns that object.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The Blast-def-line-set describing this sequence
    CRef<CBlast_def_line_set>
    x_GetHdrAsn1(int oid,
                 CSeqDBLockHold & locked) const;
    
    /// Get binary sequence header information
    /// 
    /// This method reads the sequence header information (as binary
    /// encoded ASN.1) into a supplied char vector.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param hdr_data
    ///   The returned binary ASN.1 of the Blast-def-line-set
    /// @param have_oidlist
    ///   True if the database is filtered
    /// @param membership_bit
    ///   Membership bit to filter deflines
    /// @param locked
    ///   The lock holder object for this thread
    void
    x_GetFilteredBinaryHeader(int              oid,
                              vector<char>   & hdr_data,
                              bool             have_oidlist,
                              int              membership_bit,
                              CSeqDBLockHold & locked) const;
    
    /// Get sequence header information
    /// 
    /// This method returns the set of Blast-def-line objects stored
    /// for each sequence.  These contain descriptive information
    /// related to the sequence.  If have_oidlist is true, and
    /// memb_bit is nonzero, only deflines with that membership bit
    /// set will be returned.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param have_oidlist
    ///   True if the database is filtered
    /// @param membership_bit
    ///   Membership bit to filter deflines
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The set of blast-def-lines describing this sequence
    CRef<CBlast_def_line_set>
    x_GetFilteredHeader(int                  oid,
                        bool                 have_oidlist,
                        int                  membership_bit,
                        CSeqDBLockHold     & locked) const;
    
    /// Get sequence header information structures
    /// 
    /// This method reads the sequence header information and returns
    /// a Seqdesc suitable for inclusion in a CBioseq.  This object
    /// will contain an opaque type, storing the sequence headers as
    /// binary ASN.1, wrapped in a C++ ASN.1 structure (CSeqdesc).
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param have_oidlist
    ///   True if the database is filtered
    /// @param membership_bit
    ///   Membership bit to filter deflines
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The CSeqdesc to include in the CBioseq
    CRef<CSeqdesc> x_GetAsnDefline(int                  oid,
                                   bool                 have_oidlist,
                                   int                  membership_bit,
                                   CSeqDBLockHold     & locked) const;
    
    /// Returns 'p' for protein databases, or 'n' for nucleotide.
    char x_GetSeqType() const;
    
    /// Get ambiguity information
    /// 
    /// This method is used to fetch the ambiguity data for sequences
    /// in a nucleotide database.  The ambiguity data describes
    /// sections of the nucleotide sequence for which more than one of
    /// 'A', 'C', 'G', or 'T' are possible.  The integers returned by
    /// this function contain a packed description of the ranges of
    /// the sequence which have such data.  This method only returns
    /// the array of integers, and does not interpret them, except for
    /// byte swapping.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param ambchars
    ///   The returned array of ambiguity descriptors
    /// @param locked
    ///   The lock holder object for this thread
    void x_GetAmbChar(int              oid,
                      vector<Int4>   & ambchars,
                      CSeqDBLockHold & locked) const;
    
    /// Get a sequence with ambiguous regions
    /// 
    /// This method gets the sequence data, returning a pointer and
    /// the length of the sequence.  For nucleotide sequences, the
    /// data can be returned in one of two encodings.  Specify either
    /// (kSeqDBNuclNcbiNA8) for NCBI/NA8, or (kSeqDBNuclBlastNA8) for
    /// Blast/NA8.  The data can also be allocated in one of three
    /// ways, enumerated in ESeqDBAllocType.  Specify eAtlas to use
    /// the Atlas code, eMalloc to use the malloc() function, or eNew
    /// to use the new operator.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param buffer
    ///   The returned sequence data
    /// @param nucl_code
    ///   The encoding of the returned sequence data
    /// @param alloc_type
    ///   The allocation routine used
    /// @param region
    ///   If non-null, the offset range to get
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The length of this sequence in bases
    int x_GetAmbigSeq(int                oid,
                      char            ** buffer,
                      int                nucl_code,
                      ESeqDBAllocType    alloc_type,
                      SSeqDBSlice      * region,
                      CSeqDBLockHold   & locked) const;
    
    /// Allocate memory in one of several ways
    ///
    /// This method provides functionality to allocate memory with the
    /// atlas layer, using malloc, or using the new [] operator.  The
    /// user is expected to return the data using the corresponding
    /// deallocation technique.
    ///
    /// @param length
    ///     The number of bytes to get
    /// @param alloc_type
    ///     The type of allocation routine to use
    /// @param locked
    ///     The lock holder object for this thread
    /// @return
    ///     A pointer to the allocated memory
    char * x_AllocType(size_t            length,
                       ESeqDBAllocType   alloc_type,
                       CSeqDBLockHold  & locked) const;
    
    /// Get sequence data
    ///
    /// The sequence data is found and returned for the specified
    /// sequence.  The caller owns the data and a hold on the
    /// underlying memory region.  There is a memory access in this
    /// code that tends to trigger a soft (and possibly hard) page
    /// fault in the nucleotide case.  If the can_release and keep
    /// flags are true, this code may return the lock holder object
    /// before that point to reduce lock contention in multithreaded
    /// code.
    /// 
    /// @param oid
    ///     The ordinal ID of the sequence to get
    /// @param buffer
    ///     The returned sequence data buffer
    /// @param keep
    ///     Specify true if the caller wants a hold on the sequence
    /// @param locked
    ///     The lock holder object for this thread
    /// @param can_release
    ///     Specify true if the atlas lock can be released
    /// @return
    ///     The length of the sequence in bases
    int x_GetSequence(int              oid,
                      const char    ** buffer,
                      bool             keep,
                      CSeqDBLockHold & locked,
                      bool             can_release) const;
    
    /// Get partial sequence data
    /// 
    /// The sequence data is found and returned for the specified oid
    /// and offset range.  If the region argument is non-null, the
    /// region endpoints are verified against the sequence endpoints.
    /// Otherwise, this method is the same as x_GetSequence().  Note
    /// that the code returns the length of the region in bases, but
    /// buffer is set to a pointer to the beginning of the sequence,
    /// not the beginning of the region.
    /// 
    /// @param oid
    ///   The ordinal ID of the sequence to get
    /// @param buffer
    ///   The returned sequence data buffer
    /// @param keep
    ///   Specify true if the caller wants a hold on the sequence
    /// @param locked
    ///   The lock holder object for this thread
    /// @param can_release
    ///   Specify true if the atlas lock can be released
    /// @param region
    ///   If non-null, the offset range to get
    /// @return
    ///   The length of the returned portion in bases
    int x_GetSequence(int              oid,
                      const char    ** buffer,
                      bool             keep,
                      CSeqDBLockHold & locked,
                      bool             can_release,
                      SSeqDBSlice    * region) const;
    
    /// Get defline filtered by several criteria
    ///
    /// This method returns the set of deflines for a sequence.  If
    /// there is an OID list and membership bit, these will be
    /// filtered by membership bit.  If there is a preferred GI is
    /// specified, the defline matching that GI (if found) will be
    /// moved to the front of the set.
    /// 
    /// @param oid
    ///     The ordinal ID of the sequence to get
    /// @param have_oidlist
    ///     Specify true if an OID list is available for this database
    /// @param membership_bit
    ///     Specify the value of the membership bit if one exists
    /// @param preferred_gi
    ///     This GI's defline (if found) will be put at the front of the list
    /// @param locked
    ///     The lock holder object for this thread
    /// @return
    ///     The defline set for the specified oid
    CRef<CBlast_def_line_set>
    x_GetTaxDefline(int                  oid,
                    bool                 have_oidlist,
                    int                  membership_bit,
                    int                  preferred_gi,
                    CSeqDBLockHold     & locked) const;
    
    /// Get taxonomic descriptions of a sequence
    ///
    /// This method builds a set of CSeqdesc objects from taxonomic
    /// information and blast deflines.  If there is an OID list and
    /// membership bit, the deflines will be filtered by membership
    /// bit.  If there is a preferred GI is specified, the defline
    /// matching that GI (if found) will be moved to the front of the
    /// set.  This method is called as part of the processing for
    /// building a CBioseq object.
    /// 
    /// @param oid
    ///     The ordinal ID of the sequence to get
    /// @param have_oidlist
    ///     Specify true if an OID list is available for this database
    /// @param membership_bit
    ///     Specify the value of the membership bit if one exists
    /// @param preferred_gi
    ///     This GI's defline (if found) will be put at the front of the list
    /// @param tax_info
    ///     Taxonomic info to encode
    /// @param locked
    ///     The lock holder object for this thread
    /// @return
    ///     A list of CSeqdesc objects for the specified oid
    list< CRef<CSeqdesc> >
    x_GetTaxonomy(int                   oid,
                  bool                  have_oidlist,
                  int                   membership_bit,
                  int                   preferred_gi,
                  CRef<CSeqDBTaxInfo>   tax_info,
                  CSeqDBLockHold      & locked) const;
    
    /// Returns the base-offset of the specified oid
    ///
    /// This method finds the starting offset of the OID relative to
    /// the start of the volume, and returns that distance as a number
    /// of bytes.  The range of the return value should be from zero
    /// to the size of the sequence file in bytes.  Note that the
    /// total volume length in bytes can be found by submitting the
    /// OID count as the input oid, because the index file contains
    /// one more array element than there are sequences.
    ///
    /// @param oid
    ///     The sequence of which to get the starting offset
    /// @param locked
    ///     The lock holder object for this thread
    /// @return
    ///     The offset in the volume of that sequence in bytes
    Uint8 x_GetSeqResidueOffset(int oid, CSeqDBLockHold & locked) const;
    
    /// The memory management layer
    CSeqDBAtlas & m_Atlas;
    
    /// True if the volume is protein, false for nucleotide.
    bool m_IsAA;
    
    /// The name of this volume
    string m_VolName;
    
    /// Indexes the sequence, header, and ambiguity data
    CSeqDBIdxFile m_Idx;
    
    /// Contains sequence data for this volume
    CSeqDBSeqFile m_Seq;
    
    /// Contains header (defline) information for this volume
    CSeqDBHdrFile m_Hdr;
    
    // These are mutable because they defer initialization.
    
    /// Handles translation of GIs to OIDs
    mutable CRef<CSeqDBIsam> m_IsamPig;
    
    /// Handles translation of GIs to OIDs
    mutable CRef<CSeqDBIsam> m_IsamGi;
    
    /// Handles translation of strings (accessions) to OIDs
    mutable CRef<CSeqDBIsam> m_IsamStr;
    
    /// This cache allows CBioseqs to share taxonomic objects.
    mutable CSeqDBSimpleCache< int, CRef<CSeqdesc> > m_TaxCache;
    
    /// The user GI list, if one exists.
    mutable CRef<CSeqDBGiList> m_UserGiList;
    
    /// The volume GI lists, if any exist.
    mutable TGiLists m_VolumeGiLists;
    
    /// Cached/ranged sequence info type.
    typedef map<int, CRef<CSeqDBRangeList> > TRangeCache;
    
    /// Cached/ranged sequence info.
    mutable TRangeCache m_RangeCache;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBVOL_HPP


