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
    /// Type for elements of optional GI list.
    typedef CSeqDBGiList::SGiOid TGiOid;
    
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
    ///   The memory management layer object
    /// @param name
    ///   The base name of the volumes files
    /// @param prot_nucl
    ///   The sequence type, kSeqTypeProt, or kSeqTypeNucl
    /// @param locked
    ///   The lock holder object for this thread.
    CSeqDBVol(CSeqDBAtlas    & atlas,
              const string   & name,
              char             prot_nucl,
              CSeqDBLockHold & locked);
    
    /// Sequence length for protein databases
    /// 
    /// This method returns the length of the sequence in bases, and
    /// should only be called for protein sequences.  It does not
    /// require synchronization via the atlas object's lock.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @return
    ///   The length in bases of the sequence
    int GetSeqLengthProt(int oid) const;
    
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
    /// @return
    ///   The approximate length in bases of the sequence
    int GetSeqLengthApprox(int oid) const;
    
    /// Exact sequence length for nucleotide databases
    /// 
    /// This method returns the length of the sequence in bases, and
    /// should only be called for nucleotide sequences.  It requires
    /// synchronization via the atlas object's lock, which must be
    /// done in the calling code.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @return
    ///   The length in bases of the sequence
    int GetSeqLengthExact(int oid) const;
    
    /// Get sequence header information
    /// 
    /// This method returns the set of Blast-def-line objects stored
    /// for each sequence.  These contain descriptive information
    /// related to the sequence.
    /// 
    /// @param oid
    ///   The OID of the sequence
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The set of blast-def-lines describing this sequence
    CRef<CBlast_def_line_set> GetHdr(int oid, CSeqDBLockHold & locked) const;

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
    /// @param gi_list
    ///   If specified, will be used to filter the deflines by GI.
    /// @return
    ///   The set of blast-def-lines describing this sequence
    CRef<CBlast_def_line_set>
    GetFilteredHeader(int                  oid,
                      bool                 have_oidlist,
                      int                  membership_bit,
                      CRef<CSeqDBGiList>   gi_list,
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
    /// GI will be returned.
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
    /// @param gi_list
    ///   If specified, will be used to filter the deflines by GI.
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
              CRef<CSeqDBGiList>    gi_list,
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
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The length of this sequence in bases
    int GetAmbigSeq(int              oid,
                     char           ** buffer,
                     int               nucl_code,
                     ESeqDBAllocType   alloc_type,
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
    /// @param gi_list
    ///   If specified, will be used to filter the list of SeqIDs by GI.
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The list of Seq-id objects for this sequences
    list< CRef<CSeq_id> > GetSeqIDs(int                  oid,
                                    bool                 have_oidlist,
                                    int                  membership_bit,
                                    CRef<CSeqDBGiList>   gi_list,
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
    /// @return
    ///   The OID of the sequence nearest the specified residue
    int GetOidAtOffset(int first_seq, Uint8 residue) const;
    
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
    /// @param gis
    ///   The set of GI-OID pairs.
    /// @param locked
    ///   The lock holder object for this thread
    void GisToOids(int              vol_start,
                   CSeqDBGiList   & gis,
                   CSeqDBLockHold & locked) const;
    
private:
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
    /// @param locked
    ///   The lock holder object for this thread
    void x_GetHdrBinary(int              oid,
                        vector<char>   & hdr_data,
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
    /// @param gi_list
    ///   If specified, will be used to filter the deflines by GI.
    /// @param locked
    ///   The lock holder object for this thread
    void
    x_GetFilteredBinaryHeader(int              oid,
                              vector<char>   & hdr_data,
                              bool             have_oidlist,
                              int              membership_bit,
                              CRef<CSeqDBGiList>   gi_list,
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
    /// @param gi_list
    ///   If specified, will be used to filter the deflines by GI.
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The set of blast-def-lines describing this sequence
    CRef<CBlast_def_line_set>
    x_GetFilteredHeader(int                  oid,
                        bool                 have_oidlist,
                        int                  membership_bit,
                        CRef<CSeqDBGiList>   gi_list,
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
    /// @param gi_list
    ///   If specified, will be used to filter the deflines by GI.
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The CSeqdesc to include in the CBioseq
    CRef<CSeqdesc> x_GetAsnDefline(int                  oid,
                                   bool                 have_oidlist,
                                   int                  membership_bit,
                                   CRef<CSeqDBGiList>   gi_list,
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
    /// @param locked
    ///   The lock holder object for this thread
    /// @return
    ///   The length of this sequence in bases
    int x_GetAmbigSeq(int                oid,
                      char            ** buffer,
                      int                nucl_code,
                      ESeqDBAllocType    alloc_type,
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
    /// @param gi_list
    ///   If specified, will be used to filter the deflines by GI.
    /// @param locked
    ///     The lock holder object for this thread
    /// @return
    ///     The defline set for the specified oid
    CRef<CBlast_def_line_set>
    x_GetTaxDefline(int                  oid,
                    bool                 have_oidlist,
                    int                  membership_bit,
                    int                  preferred_gi,
                    CRef<CSeqDBGiList>   gi_list,
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
    /// @param gi_list
    ///     If specified, will be used to filter the deflines by GI.
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
                  CRef<CSeqDBGiList>    gi_list,
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
    /// @return
    ///     The offset in the volume of that sequence in bytes
    Uint8 x_GetSeqResidueOffset(int oid) const;
    
    /// The memory management layer
    CSeqDBAtlas & m_Atlas;
    
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
    mutable map< int, CRef<CSeqdesc> > m_TaxCache;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBVOL_HPP


