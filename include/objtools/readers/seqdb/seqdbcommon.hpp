#ifndef CORELIB__SEQDB__SEQDBCOMMON_HPP
#define CORELIB__SEQDB__SEQDBCOMMON_HPP

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

/// @file seqdbcommon.hpp
/// Defines exception class and several constants for SeqDB.
/// 
/// Defines classes:
///     CSeqDBException
/// 
/// Implemented for: UNIX, MS-Windows

#include <ncbiconf.h>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE

/// Include definitions from the objects namespace.
USING_SCOPE(objects);


/// CSeqDBException
/// 
/// This exception class is thrown for SeqDB related errors such as
/// corrupted blast database or alias files, incorrect arguments to
/// SeqDB methods, and failures of SeqDB to accomplish tasks for other
/// reasons.  SeqDB may be used in applications with strong robustness
/// requirements, where it is considered better to fail an operation
/// and lose context information, than to terminate with a core dump,
/// and preserve it, so exceptions are the preferred mechanism for
/// most error scenarios.  SeqDB still uses assertions in cases where
/// memory corruption is suspected, or cleanup may not be possible.

class CSeqDBException : public CException {
public:
    /// Errors are classified into one of two types.
    enum EErrCode {
        /// Argument validation failed.
        eArgErr,
        
        /// Files were missing or contents were incorrect.
        eFileErr,
        
        /// Memory allocation failed.
        eMemErr
    };
    
    /// Get a message describing the situation leading to the throw.
    virtual const char* GetErrCodeString() const
    {
        switch ( GetErrCode() ) {
        case eArgErr:  return "eArgErr";
        case eFileErr: return "eFileErr";
        default:       return CException::GetErrCodeString();
        }
    }
    
    /// Include standard NCBI exception behavior.
    NCBI_EXCEPTION_DEFAULT(CSeqDBException,CException);
};


/// Used to request ambiguities in Ncbi/NA8 format.
const int kSeqDBNuclNcbiNA8  = 0;

/// Used to request ambiguities in BLAST/NA8 format.
const int kSeqDBNuclBlastNA8 = 1;


/// Certain methods have an "Alloc" version.  When these methods are
/// used, the following constants can be specified to indicate which
/// libraries to use to allocate returned data, so the corresponding
/// calls (delete[] vs. free()) can be used to delete the data.

enum ESeqDBAllocType {
    eAtlas = 0,
    eMalloc,
    eNew
};


/// CSeqDBGiList
/// 
/// This class defines an interface to a list of GI,OID pairs.  It is
/// used by the CSeqDB class for user specified GI lists.  This class
/// should not be instantiated directly, instead use a subclass of
/// this class.  Subclasses should provide a way to populate the
/// m_GisOids vector.

class NCBI_XOBJREAD_EXPORT CSeqDBGiList : public CObject {
public:
    /// Structure that holds GI,OID pairs.
    struct SGiOid {
        /// Constuct an SGiOid element from the given gi and oid.
        /// @param gi_in A GI, or 0 if none is available.
        /// @param oid_in An OID, or -1 if none is available.
        SGiOid(int gi_in = 0, int oid_in = -1)
            : gi(gi_in), oid(oid_in)
        {
        }
        
        /// The GI or 0 if unknown.
        int gi;
        
        /// The OID or -1 if unknown.
        int oid;
    };
    
    /// Structure that holds TI,OID pairs.
    struct STiOid {
        /// Constuct an STiOid element from the given TI (trace ID,
        /// expressed as a number) and oid.
        ///
        /// @param ti_in A TI, or 0 if none is available.
        /// @param oid_in An OID, or -1 if none is available.
        STiOid(Int8 ti_in = 0, int oid_in = -1)
            : ti(ti_in), oid(oid_in)
        {
        }
        
        /// The TI or 0 if unknown.
        Int8 ti;
        
        /// The OID or -1 if unknown.
        int oid;
    };
    
    /// Structure that holds Seq-id,OID pairs.
    struct SSeqIdOid {
        /// Constuct a SSeqIdOid element from the given Seq-id and oid.
        /// @param seqid_in A Seq-id, or NULL if none is available.
        /// @param oid_in An OID, or -1 if none is available.
        SSeqIdOid(CSeq_id * seqid_in = NULL, int oid_in = -1)
            : seqid(seqid_in), oid(oid_in)
        {
        }
        
        /// The Seq-id or NULL if unknown.
        CRef<objects::CSeq_id> seqid;
        
        /// The OID or -1 if unknown.
        int oid;
    };
    
    /// Possible sorting states
    enum ESortOrder {
        /// The array is unsorted or the sortedness is unknown.
        eNone,
        
        /// The array is sorted by GI.
        eGi
    };
    
    /// Constructor
    CSeqDBGiList();
    
    /// Destructor
    virtual ~CSeqDBGiList()
    {
    }
    
    /// Sort if necessary to insure order of elements.
    void InsureOrder(ESortOrder order);
    
    /// Test for existence of a GI.
    bool FindGi(int gi);
    
    /// Try to find a GI and return the associated OID.
    /// @param gi The gi for which to search. [in]
    /// @param oid The resulting oid if found. [out]
    /// @return True if the GI was found.
    bool GiToOid(int gi, int & oid);
    
    /// Find a GI, returning the index and the associated OID.
    /// @param gi The gi for which to search. [in]
    /// @param oid The resulting oid if found. [out]
    /// @param index The index of this GI (if found). [out]
    /// @return True if the GI was found.
    bool GiToOid(int gi, int & oid, int & index);
    
    /// Test for existence of a TI.
    bool FindTi(Int8 ti);
    
    /// Try to find a TI and return the associated OID.
    /// @param ti The ti for which to search. [in]
    /// @param oid The resulting oid if found. [out]
    /// @return True if the TI was found.
    bool TiToOid(Int8 ti, int & oid);
    
    /// Find a TI, returning the index and the associated OID.
    /// @param ti The ti for which to search. [in]
    /// @param oid The resulting oid if found. [out]
    /// @param index The index of this TI (if found). [out]
    /// @return True if the TI was found.
    bool TiToOid(Int8 ti, int & oid, int & index);
    
    /// Test for existence of a Seq-id.
    bool FindSeqId(const CSeq_id & seqid);
    
    /// Try to find a Seq-id and return the associated OID.
    /// @param seqid The Seq-id for which to search. [in]
    /// @param oid The resulting oid if found. [out]
    /// @return True if the Seq-id was found.
    bool SeqIdToOid(const CSeq_id & seqid, int & oid);
    
    /// Find a Seq-id, returning the index and the associated OID.
    /// @param seqid The Seq-id for which to search. [in]
    /// @param oid The resulting oid if found. [out]
    /// @param index The index of this Seq-id (if found). [out]
    /// @return True if the Seq-id was found.
    bool SeqIdToOid(const CSeq_id & seqid, int & oid, int & index);
    
    /// Test for existence of a Seq-id by type.
    /// 
    /// This method uses FindGi or FindTi if the input ID is a GI or
    /// TI.  If not, or if not found, it falls back to a Seq-id lookup
    /// to find the ID.  It returns true iff ID was found, otherwise
    /// it returns false.  This method is used by SeqDB to filter
    /// Blast Defline lists.
    ///
    /// @param id The identifier to find.
    /// @return true iff the id is found in the list.
    bool FindId(const CSeq_id & id);
    
    /// Access an element of the array.
    /// @param index The index of the element to access. [in]
    /// @return A reference to the GI/OID pair.
    const SGiOid & GetGiOid(int index) const
    {
        return m_GisOids[index];
    }
    
    /// Access an element of the array.
    /// @param index The index of the element to access. [in]
    /// @return A reference to the TI/OID pair.
    const STiOid & GetTiOid(int index) const
    {
        return m_TisOids[index];
    }
    
    /// Access an element of the array.
    /// @param index The index of the element to access. [in]
    /// @return A reference to the Seq-id/OID pair.
    const SSeqIdOid & GetSeqIdOid(int index) const
    {
        return m_SeqIdsOids[index];
    }
    
    /// Access an element of the array.
    /// @param index The index of the element to access. [in]
    /// @return A reference to the GI/OID pair.
    const SGiOid & operator[](int index) const
    {
        return m_GisOids[index];
    }
    
    /// Get the number of GIs in the array.
    int GetNumGis() const
    {
        return (int) m_GisOids.size();
    }
    
    /// Get the number of TIs in the array.
    int GetNumTis() const
    {
        return (int) m_TisOids.size();
    }
    
    /// Get the number of Seq-ids in the array.
    int GetNumSeqIds() const
    {
        return (int) m_SeqIdsOids.size();
    }
    
    /// Get the number of GIs in the array.
    int Size() const
    {
        // this may become a deprecated method
        return (int) m_GisOids.size();
    }
    
    /// Return false if there are elements present.
    bool Empty() const
    {
        return ! (GetNumGis() || GetNumSeqIds() || GetNumTis());
    }
    
    /// Return true if there are elements present.
    bool NotEmpty() const
    {
        return ! Empty();
    }
    
    /// Specify the correct OID for a GI.
    ///
    /// When SeqDB translates a GI into an OID, this method is called
    /// to store the oid in the array.
    ///
    /// @param index
    ///   The location in the array of the GI, OID pair.
    /// @param oid
    ///   The oid to store in that element.
    void SetTranslation(int index, int oid)
    {
        m_GisOids[index].oid = oid;
    }
    
    /// Specify the correct OID for a TI.
    ///
    /// When SeqDB translates a TI into an OID, this method is called
    /// to store the oid in the array.
    ///
    /// @param index
    ///   The location in the array of the TI, OID pair.
    /// @param oid
    ///   The oid to store in that element.
    void SetTiTranslation(int index, int oid)
    {
        m_TisOids[index].oid = oid;
    }
    
    /// Specify the correct OID for a Seq-id.
    ///
    /// When SeqDB translates a Seq-id into an OID, this method is
    /// called to store the oid in the array.
    ///
    /// @param index
    ///   The location in the array of Seq-id, OID pairs.
    /// @param oid
    ///   The oid to store in that element.
    void SetSeqIdTranslation(int index, int oid)
    {
        m_SeqIdsOids[index].oid = oid;
    }
    
    /// Get the gi list
    void GetGiList(vector<int>& gis) const;
    
    /// Get the ti list
    void GetTiList(vector<Int8>& tis) const;
    
protected:
    /// Indicates the current sort order, if any, of this container.
    ESortOrder m_CurrentOrder;
    
    /// Pairs of GIs and OIDs.
    vector<SGiOid> m_GisOids;
    
    /// Pairs of GIs and OIDs.
    vector<STiOid> m_TisOids;
    
    /// Pairs of Seq-ids and OIDs.
    vector<SSeqIdOid> m_SeqIdsOids;
    
private:
    // The following disabled methods are reasonable things to do in
    // some cases.  But I suspect they are more likely to happen
    // accidentally than deliberately; due to the high performance
    // cost, I have prevented them.  If this kind of deep copy is
    // desireable, it can easily be enabled for a subclass by
    // assigning each of the data fields in the protected section.
    
    /// Prevent copy constructor.
    CSeqDBGiList(const CSeqDBGiList & other);
    
    /// Prevent assignment.
    CSeqDBGiList & operator=(const CSeqDBGiList & other);
};


/// CSeqDBBitVector
/// 
/// This class defines a bit vector that is similar to vector<bool>,
/// but with a differently designed API that performs better on at
/// least some platforms, and slightly altered semantics.

class NCBI_XOBJREAD_EXPORT CSeqDBBitVector {
public:
    /// Constructor
    CSeqDBBitVector()
        : m_Size(0)
    {
    }
    
    /// Destructor
    virtual ~CSeqDBBitVector()
    {
    }
    
    /// Set the inclusion of an OID.
    ///
    /// @param oid The OID in question. [in]
    void SetBit(int oid)
    {
        if (oid >= m_Size) {
            x_Resize(oid+1);
        }
        x_SetBit(oid);
    }
    
    /// Set the inclusion of an OID.
    ///
    /// @param oid The OID in question. [in]
    void ClearBit(int oid)
    {
        if (oid < m_Size) {
            return;
        }
        x_ClearBit(oid);
    }
    
    /// Get the inclusion status of an OID.
    ///
    /// @param oid The OID in question. [in]
    /// @return True if the OID is included by SeqDB.
    bool GetBit(int oid)
    {
        if (oid >= m_Size) {
            return false;
        }
        return x_GetBit(oid);
    }
    
    /// Get the size of the OID array.
    int Size()
    {
        return m_Size;
    }
    
private:
    /// Prevent copy constructor.
    CSeqDBBitVector(const CSeqDBBitVector & other);
    
    /// Prevent assignment.
    CSeqDBBitVector & operator=(const CSeqDBBitVector & other);
    
    /// Bit vector element.
    typedef int TBits;
    
    /// Bit vector.
    vector<TBits> m_Bitmap;
    
    /// Maximum enabled OID plus one.
    int m_Size;
    
    /// Resize the OID list.
    void x_Resize(int num)
    {
        int bits = 8*sizeof(TBits);
        int need = (num + bits - 1)/bits;
        
        if ((int)m_Bitmap.size() < need) {
            int new_size = 1024;
            
            while (new_size < need) {
                new_size *= 2;
            }
            
            m_Bitmap.resize(new_size);
        }
        
        m_Size = num;
    }
    
    /// Set a specific bit (to 1).
    void x_SetBit(int num)
    {
        int bits = 8*sizeof(TBits);
        
        m_Bitmap[num/bits] |= (1 << (num % bits));
    }
    
    /// Set a specific bit (to 1).
    bool x_GetBit(int num)
    {
        int bits = 8*sizeof(TBits);
        
        return !! (m_Bitmap[num/bits] & (1 << (num % bits)));
    }
    
    /// Clear a specific bit (to 0).
    void x_ClearBit(int num)
    {
        int bits = 8*sizeof(TBits);
        
        m_Bitmap[num/bits] &= ~(1 << (num % bits));
    }
};


/// CSeqDBNegativeList
/// 
/// This class defines a list of GIs or TIs of sequences that should
/// not be included in a SeqDB instance.  It is used by CSeqDB for
/// user specified negative ID lists.  This class can be subclassed to
/// allow more efficient population of the GI or TI list.

class NCBI_XOBJREAD_EXPORT CSeqDBNegativeList : public CObject {
public:
    /// Constructor
    CSeqDBNegativeList()
        : m_LastSortSize (0)
    {
    }
    
    /// Destructor
    virtual ~CSeqDBNegativeList()
    {
    }
    
    /// Sort list if not already sorted.
    void InsureOrder()
    {
        if (m_LastSortSize != (int)(m_Gis.size() + m_Tis.size())) {
            std::sort(m_Gis.begin(), m_Gis.end());
            std::sort(m_Tis.begin(), m_Tis.end());
            
            m_LastSortSize = m_Gis.size() + m_Tis.size();
        }
    }
    
    /// Test for existence of a GI.
    void AddGi(int gi)
    {
        m_Gis.push_back(gi);
    }
    
    /// Test for existence of a GI.
    void AddTi(Int8 ti)
    {
        m_Tis.push_back(ti);
    }
    
    /// Test for existence of a GI.
    bool FindGi(int gi);
    
    /// Test for existence of a TI.
    bool FindTi(Int8 ti);
    
    /// Test for existence of a TI or GI here and report whether the
    /// ID was one of those types.
    /// 
    /// If the input ID is a GI or TI, this method sets match_type to
    /// true and returns the output of FindGi or FindTi.  If it is
    /// neither of those types, it sets match_type to false and
    /// returns false.  This method is used by SeqDB to filter Blast
    /// Defline lists.
    ///
    /// @param id The identifier to find.
    /// @param match_type The identifier is either a TI or GI.
    /// @return true iff the id is found in the list.
    bool FindId(const CSeq_id & id, bool & match_type);
    
    /// Test for existence of a TI or GI included here.
    bool FindId(const CSeq_id & id);
    
    /// Access an element of the GI array.
    /// @param index The index of the element to access. [in]
    /// @return The GI for that index.
    const int GetGi(int index) const
    {
        return m_Gis[index];
    }
    
    /// Access an element of the TI array.
    /// @param index The index of the element to access. [in]
    /// @return The TI for that index.
    const Int8 GetTi(int index) const
    {
        return m_Tis[index];
    }
    
    /// Get the number of GIs in the array.
    int GetNumGis() const
    {
        return (int) m_Gis.size();
    }
    
    /// Get the number of TIs in the array.
    int GetNumTis() const
    {
        return (int) m_Tis.size();
    }
    
    /// Return false if there are elements present.
    bool Empty() const
    {
        return ! (GetNumGis() || GetNumTis());
    }
    
    /// Return true if there are elements present.
    bool NotEmpty() const
    {
        return ! Empty();
    }
    
    /// Include an OID in the iteration.
    ///
    /// The OID will be included by SeqDB in the set returned to users
    /// by OID iteration.
    ///
    /// @param oid The OID in question. [in]
    void AddIncludedOid(int oid)
    {
        m_Included.SetBit(oid);
    }
    
    /// Indicate a visible OID.
    ///
    /// The OID will be marked as having been found in a GI or TI
    /// ISAM index (but possibly not included for iteration).
    ///
    /// @param oid The OID in question. [in]
    void AddVisibleOid(int oid)
    {
        m_Visible.SetBit(oid);
    }
    
    /// Get the inclusion status of an OID.
    ///
    /// This returns true for OIDs that were in the included set and
    /// for OIDs that were not found in the ISAM file at all.
    ///
    /// @param oid The OID in question. [in]
    /// @return True if the OID is included by SeqDB.
    bool GetOidStatus(int oid)
    {
        return m_Included.GetBit(oid) || (! m_Visible.GetBit(oid));
    }
    
    /// Get the size of the OID array.
    int GetNumOids()
    {
        return max(m_Visible.Size(), m_Included.Size());
    }
    
protected:
    /// GIs to exclude from the SeqDB instance.
    vector<int> m_Gis;
    
    /// TIs to exclude from the SeqDB instance.
    vector<Int8> m_Tis;
    
private:
    /// Prevent copy constructor.
    CSeqDBNegativeList(const CSeqDBNegativeList & other);
    
    /// Prevent assignment.
    CSeqDBNegativeList & operator=(const CSeqDBNegativeList & other);
    
    /// Included OID bitmap.
    CSeqDBBitVector m_Included;
    
    /// OIDs visible to the ISAM file.
    CSeqDBBitVector m_Visible;
    
    /// Zero if unsorted, or the size it had after the last sort.
    int m_LastSortSize;
};


/// Read a binary-format GI list from a file.
///
/// @param name The name of the file containing GIs. [in]
/// @param gis The GIs returned by this function. [out]
NCBI_XOBJREAD_EXPORT
void SeqDB_ReadBinaryGiList(const string & name, vector<int> & gis);

/// Read a text or binary GI list from an area of memory.
///
/// The GIs in a memory region are read into the provided SGiOid
/// vector.  The GI half of each element of the vector is assigned,
/// but the OID half will be left as -1.  If the in_order parameter is
/// not null, the function will test the GIs for orderedness.  It will
/// set the bool to which in_order points to true if so, false if not.
///
/// @param fbeginp The start of the memory region holding the GI list. [in]
/// @param fendp   The end of the memory region holding the GI list. [in]
/// @param gis     The GIs returned by this function. [out]
/// @param in_order If non-null, returns true iff the GIs were in order. [out]

NCBI_XOBJREAD_EXPORT
void SeqDB_ReadMemoryGiList(const char                   * fbeginp,
                            const char                   * fendp,
                            vector<CSeqDBGiList::SGiOid> & gis,
                            bool                         * in_order = 0);

/// Read a text or binary TI list from an area of memory.
///
/// The TIs in a memory region are read into the provided STiOid
/// vector.  The TI half of each element of the vector is assigned,
/// but the OID half will be left as -1.  If the in_order parameter is
/// not null, the function will test the TIs for orderedness.  It will
/// set the bool to which in_order points to true if so, false if not.
///
/// @param fbeginp The start of the memory region holding the TI list. [in]
/// @param fendp   The end of the memory region holding the TI list. [in]
/// @param tis     The TIs returned by this function. [out]
/// @param in_order If non-null, returns true iff the TIs were in order. [out]

NCBI_XOBJREAD_EXPORT
void SeqDB_ReadMemoryTiList(const char                   * fbeginp,
                            const char                   * fendp,
                            vector<CSeqDBGiList::STiOid> & tis,
                            bool                         * in_order = 0);

/// Read a text or binary GI list from a file.
///
/// The GIs in a file are read into the provided SGiOid vector.  The
/// GI half of each element of the vector is assigned, but the OID
/// half will be left as -1.  If the in_order parameter is not null,
/// the function will test the GIs for orderedness.  It will set the
/// bool to which in_order points to true if so, false if not.
///
/// @param fname    The name of the GI list file. [in]
/// @param gis      The GIs returned by this function. [out]
/// @param in_order If non-null, returns true iff the GIs were in order. [out]

NCBI_XOBJREAD_EXPORT
void SeqDB_ReadGiList(const string                 & fname,
                      vector<CSeqDBGiList::SGiOid> & gis,
                      bool                         * in_order = 0);

/// Read a text or binary TI list from a file.
///
/// The TIs in a file are read into the provided STiOid vector.  The
/// TI half of each element of the vector is assigned, but the OID
/// half will be left as -1.  If the in_order parameter is not null,
/// the function will test the TIs for orderedness.  It will set the
/// bool to which in_order points to true if so, false if not.
///
/// @param fname    The name of the TI list file. [in]
/// @param tis      The TIs returned by this function. [out]
/// @param in_order If non-null, returns true iff the TIs were in order. [out]

NCBI_XOBJREAD_EXPORT
void SeqDB_ReadTiList(const string                 & fname,
                      vector<CSeqDBGiList::STiOid> & tis,
                      bool                         * in_order = 0);

/// Read a text or binary GI list from a file.
///
/// The GIs in a file are read into the provided vector<int>.  If the
/// in_order parameter is not null, the function will test the GIs for
/// orderedness.  It will set the bool to which in_order points to
/// true if so, false if not.
///
/// @param fname    The name of the GI list file. [in]
/// @param gis      The GIs returned by this function. [out]
/// @param in_order If non-null, returns true iff the GIs were in order. [out]

NCBI_XOBJREAD_EXPORT
void SeqDB_ReadGiList(const string  & fname,
                      vector<int>   & gis,
                      bool          * in_order = 0);


/// CSeqDBFileGiList
/// 
/// This class defines a CSeqDBGiList subclass which reads a GI list
/// file given a filename.  It can read text or binary GI list files,
/// and will automatically distinguish between them.

class NCBI_XOBJREAD_EXPORT CSeqDBFileGiList : public CSeqDBGiList {
public:
    /// Build a GI list from a file.
    CSeqDBFileGiList(const string & fname);
};


/// GI list containing the intersection of two other lists of GIs.
///
/// This class takes a CSeqDBGiList and an integer vector and computes
/// the intersection of the two.  Note that both input arguments are
/// sorted to GI order in-place.

class NCBI_XOBJREAD_EXPORT CIntersectionGiList : public CSeqDBGiList {
public:
    /// Construct an intersection of two lists of GIs.
    ///
    /// The two lists of GIs are sorted and this class is computed as
    /// an intersection of them.  Note that both arguments to this
    /// function are potentially modified (sorted in place).
    CIntersectionGiList(CSeqDBGiList & gilist, vector<int> & gis);
};


/// GI list performing an 'X AND NOT Y' operation.
///
/// This class takes a CSeqDBGiList and an integer vector and computes
/// the intersection of the two.  Note that both input arguments are
/// sorted to GI order in-place.

class NCBI_XOBJREAD_EXPORT CAndNotGiList : public CSeqDBGiList {
public:
    /// Intersect positive and negative lists of GIs.
    ///
    /// The two lists of GIs are sorted and this class is computed as
    /// an intersection of the first with the negation of the second.
    /// Both arguments to this function are potentially modified when
    /// they are sorted in place.
    CAndNotGiList(CSeqDBGiList & gilist, vector<int> & gis);
};


// The "instance" concept in the following types refers to the fact
// that each alias file has a seperately instantiated node for each
// point where it appears in the alias file hierarchy.

/// Set of values found in one instance of one alias file.
typedef map<string, string> TSeqDBAliasFileInstance;

/// Contents of all instances of a particular alias file pathname.
typedef vector< TSeqDBAliasFileInstance > TSeqDBAliasFileVersions;

/// Contents of all alias file are returned in this type of container.
typedef map< string, TSeqDBAliasFileVersions > TSeqDBAliasFileValues;


/// SSeqDBTaxInfo
///
/// This structure contains the taxonomy information for a single
/// given taxid.

struct SSeqDBTaxInfo {
    /// Default constructor
    SSeqDBTaxInfo()
        : taxid(0)
    {
    }
    
    /// An identifier for this species or taxonomic group.
    int taxid;
    
    /// Scientific name, such as "Aotus vociferans".
    string scientific_name;
    
    /// Common name, such as "noisy night monkey".
    string common_name;
    
    /// A simple category name, such as "birds".
    string blast_name;
    
    /// A string of length 1 indicating the "Super Kingdom".
    string s_kingdom;
};


/// Resolve a file path using SeqDB's path algorithms.
///
/// This finds a file using the same algorithm used by SeqDB to find
/// blast database filenames.  The filename must include the extension
/// if any.  Paths which start with '/', '\', or a drive letter
/// (depending on operating system) will be treated as absolute paths.
/// If the file is not found an empty string will be returned.
///
/// @param filename Name of file to find.
/// @return Resolved path or empty string if not found.

NCBI_XOBJREAD_EXPORT
string SeqDB_ResolveDbPath(const string & filename);

/// Sequence Hashing
///
/// This computes a hash of a sequence.  The sequence is expected to
/// be in either ncbistdaa format (for protein) or ncbi8na format (for
/// nucleotide).  These formats are produced by CSeqDB::GetAmbigSeq()
/// if the kSeqDBNuclNcbiNA8 encoding is selected.
///
/// @param sequence A pointer to the sequence data. [in]
/// @param length The length of the sequence in bases. [in]
/// @return The 32 bit hash value.
unsigned SeqDB_SequenceHash(const char * sequence,
                            int          length);

/// Sequence Hashing For a CBioseq
///
/// This computes a hash of a sequence expressed as a CBioseq.
///
/// @param sequence The sequence. [in]
/// @return The 32 bit hash value.
unsigned SeqDB_SequenceHash(const CBioseq & sequence);

END_NCBI_SCOPE

#endif // CORELIB__SEQDB__SEQDBCOMMON_HPP

