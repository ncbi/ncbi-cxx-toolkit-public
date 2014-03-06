#ifndef OBJTOOLS_BLAST_SEQDB_READER___SEQDBCOMMON__HPP
#define OBJTOOLS_BLAST_SEQDB_READER___SEQDBCOMMON__HPP

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

class NCBI_XOBJREAD_EXPORT CSeqDBException : public CException {
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

/// The name of the group alias file name expected at each directory
/// For more documentation, see "Group Alias Files" in
/// source/src/objtools/blast/seqdb_reader/alias_files.txt
NCBI_XOBJREAD_EXPORT extern const string kSeqDBGroupAliasFileName;

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
    struct SSiOid {
        /// Constuct a SSiOid element from the given Seq-id and oid.
        /// @param seqid_in A Seq-id, or NULL if none is available.
        /// @param oid_in An OID, or -1 if none is available.
        SSiOid(const string &si_in = "", int oid_in = -1)
            : si(si_in), oid(oid_in)
        {
            // make sure to lower case as this is what's indexed in ISAM
            NStr::ToLower(si);
        }
        
        /// The String-id or "" if unknown.
        string si;
        
        /// The OID or -1 if unknown.
        int oid;
    };
    
    /// Possible sorting states
    enum ESortOrder {
        /// The array is unsorted or the sortedness is unknown.
        eNone,
        
        /// The array is sorted by GI.
        eGi

        /// TODO should we define eTi and eSi?
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
    bool FindGi(int gi) const;
    
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
    bool FindTi(Int8 ti) const;
    
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

    
    bool FindSi(const string & si) const;
    bool SiToOid(const string &si, int & oid);
    bool SiToOid(const string &si, int & oid, int & index);
    
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
    const SSiOid & GetSiOid(int index) const
    {
        return m_SisOids[index];
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
    int GetNumSis() const
    {
        return (int) m_SisOids.size();
    }
    
    /// Return false if there are elements present.
    bool Empty() const
    {
        return ! (GetNumGis() || GetNumSis() || GetNumTis());
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
    void SetGiTranslation(int index, int oid)
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
    void SetSiTranslation(int index, int oid)
    {
        m_SisOids[index].oid = oid;
    }
    
    int Size() const
    {
        return (int) m_GisOids.size();
    }

    template <class T>
    int GetSize() const
    {
        return (int) m_GisOids.size();
    }

    template <class T>
    const T & GetKey(int index) const
    {
        return m_GisOids[index].gi;
    }

    template <class T>
    bool IsValueSet(int index) const
    {
        return (m_GisOids[index].oid != -1);
    }

    template <class T>
    void SetValue(int index, int oid) 
    {
        m_GisOids[index].oid = oid;
    }

    /// Get the gi list
    void GetGiList(vector<int>& gis) const;
    
    /// Get the ti list
    void GetTiList(vector<Int8>& tis) const;

    /// TODO Get the seqid list?
    
    /// Add a new GI to the list.
    void AddGi(int gi)
    {
        m_GisOids.push_back(gi);
    }
    
    /// Add a new TI to the list.
    void AddTi(Int8 ti)
    {
        m_TisOids.push_back(ti);
    }
    
    /// Add a new SeqId to the list.
    void AddSi(const string &si)
    {
        m_SisOids.push_back(si);
    }

    /// Reserve space for GIs.
    void ReserveGis(size_t n)
    {
        m_GisOids.reserve(n);
    }
    
    /// Reserve space for TIs.
    void ReserveTis(size_t n)
    {
        m_TisOids.reserve(n);
    }
    
    /// TODO Reserve space for seqids?
protected:
    /// Indicates the current sort order, if any, of this container.
    ESortOrder m_CurrentOrder;
    
    /// Pairs of GIs and OIDs.
    vector<SGiOid> m_GisOids;
    
    /// Pairs of GIs and OIDs.
    vector<STiOid> m_TisOids;
    
    /// Pairs of Seq-ids and OIDs.
    vector<SSiOid> m_SisOids;
    
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

template < >
inline int CSeqDBGiList::GetSize<Int8>() const
{
    return (int) m_TisOids.size();
}

template < >
inline const Int8 & CSeqDBGiList::GetKey<Int8>(int index) const
{
    return m_TisOids[index].ti;
}

template < >
inline bool CSeqDBGiList::IsValueSet<Int8>(int index) const
{
    return (m_TisOids[index].oid != -1);
}

template < >
inline void CSeqDBGiList::SetValue<Int8>(int index, int oid)
{
    m_TisOids[index].oid = oid;
}

template < >
inline int CSeqDBGiList::GetSize<string>() const
{
    return (int) m_SisOids.size();
}

template < >
inline const string & CSeqDBGiList::GetKey<string>(int index) const
{
    return m_SisOids[index].si;
}

template < >
inline bool CSeqDBGiList::IsValueSet<string>(int index) const
{
    return (m_SisOids[index].oid != -1);
}

template < >
inline void CSeqDBGiList::SetValue<string>(int index, int oid)
{
    m_SisOids[index].oid = oid;
}

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
    int Size() const
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
        if (m_LastSortSize != (int)(m_Gis.size() + m_Tis.size() +m_Sis.size())) {
            std::sort(m_Gis.begin(), m_Gis.end());
            std::sort(m_Tis.begin(), m_Tis.end());
            std::sort(m_Sis.begin(), m_Sis.end());
            
            m_LastSortSize = m_Gis.size() + m_Tis.size() + m_Sis.size();
        }
    }
    
    /// Add a new GI to the list.
    void AddGi(int gi)
    {
        m_Gis.push_back(gi);
    }
    
    /// Add a new TI to the list.
    void AddTi(Int8 ti)
    {
        m_Tis.push_back(ti);
    }
    
    /// Add a new SeqId to the list.
    void AddSi(const string &si)
    {
        m_Sis.push_back(si);
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
    int GetGi(int index) const
    {
        return m_Gis[index];
    }
    
    /// Access an element of the TI array.
    /// @param index The index of the element to access. [in]
    /// @return The TI for that index.
    Int8 GetTi(int index) const
    {
        return m_Tis[index];
    }
    
    /// Access an element of the SeqId array.
    /// @param index The index of the element to access. [in]
    /// @return The TI for that index.
    const string GetSi(int index) const
    {
        return m_Sis[index];
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
    
    /// Get the number of SeqIds in the array.
    int GetNumSis() const
    {
        return (int) m_Sis.size();
    }
    
    /// Return false if there are elements present.
    bool Empty() const
    {
        return ! (GetNumGis() || GetNumTis() || GetNumSis());
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
    
    /// Reserve space for GIs.
    void ReserveGis(size_t n)
    {
        m_Gis.reserve(n);
    }
    
    /// Reserve space for TIs.
    void ReserveTis(size_t n)
    {
        m_Tis.reserve(n);
    }
    
    /// Build ID set for this negative list.
    const vector<int> & GetGiList()
    {
        return m_Gis;
    }
    
    /// Set ID set for this negative list.
    void SetGiList( const vector<int> & new_list ) 
    {
	m_Gis.clear();
	m_Gis.reserve( new_list.size() );
        m_Gis = new_list;
    }
    /// Build ID set for this negative list.
    const vector<Int8> & GetTiList()
    {
        return m_Tis;
    }
    /// Get list size
    int Size(void)
    {
	return (int)m_Gis.size();
    }
protected:
    /// GIs to exclude from the SeqDB instance.
    vector<int> m_Gis;
    
    /// TIs to exclude from the SeqDB instance.
    vector<Int8> m_Tis;
    
    /// SeqIds to exclude from the SeqDB instance.
    vector<string> m_Sis;
    
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

/// Read a text SeqID list from an area of memory.
///
/// The Seqids in a memory region are read into the provided SSeqIdOid
/// vector.  The SeqId half of each element of the vector is assigned,
/// but the OID half will be left as -1.  If the in_order parameter is
/// not null, the function will test the SeqIds for orderedness.  It will
/// set the bool to which in_order points to true if so, false if not.
///
/// @param fbeginp The start of the memory region holding the SeqId list. [in]
/// @param fendp   The end of the memory region holding the SeqId list. [in]
/// @param seqids  The SeqId returned by this function. [out]
/// @param in_order If non-null, returns true iff the seqids were in order. [out]

NCBI_XOBJREAD_EXPORT
void SeqDB_ReadMemorySiList(const char                   * fbeginp,
                            const char                   * fendp,
                            vector<CSeqDBGiList::SSiOid> & sis,
                            bool                         * in_order = 0);

/// Read an ID list (mixed type) from an area of memory.
///
/// The Seq ids in a memory region are read into the provided SSeqIdOid
/// vector.  The gi, ti or seqid half of each element of the vector is assigned,
/// but the OID half will be left as -1.  If the in_order parameter is
/// not null, the function will test the SeqIds for orderedness.  It will
/// set the bool to which in_order points to true if so, false if not.
///
/// @param fbeginp The start of the memory region holding the SeqId list. [in]
/// @param fendp   The end of the memory region holding the SeqId list. [in]
/// @param gis     The gis returned by this function. [out]
/// @param tis     The tis returned by this function. [out]
/// @param sis     The seqids returned by this function. [out]
/// @param in_order If non-null, returns true iff the seqids were in order. [out]

NCBI_XOBJREAD_EXPORT
void SeqDB_ReadMemoryMixList(const char * fbeginp,
                            const char * fendp,
                            vector<CSeqDBGiList::SGiOid> & gis,
                            vector<CSeqDBGiList::STiOid> & tis,
                            vector<CSeqDBGiList::SSiOid> & sis,
                            bool * in_order);

/// Combine and quote a list of database names.
///
/// SeqDB permits multiple databases to be opened by a single CSeqDB
/// instance, by passing the database names as a space-delimited list
/// to the CSeqDB constructor.  To support paths and filenames with
/// embedded spaces, surround any space-containing names with double
/// quotes ('"').  Filenames not containing spaces may be quoted
/// safely with no effect.  (This solution prevents the use of names
/// containing embedded double quotes.)
///
/// This method combines a list of database names into a string
/// encoded in this way.
///
/// @param dbname Combined database name.
/// @param dbs Database names to combine.

NCBI_XOBJREAD_EXPORT
void SeqDB_CombineAndQuote(const vector<string> & dbs,
                           string               & dbname);

/// Split a (possibly) quoted list of database names into pieces.
///
/// SeqDB permits multiple databases to be opened by a single CSeqDB
/// instance, by passing the database names as a space-delimited list
/// to the CSeqDB constructor.  To support paths and filenames with
/// embedded spaces, surround any space-containing names with double
/// quotes ('"').  Filenames not containing spaces may be quoted
/// safely with no effect.  (This solution prevents the use of names
/// containing embedded double quotes.)
///
/// This method splits a string encoded in this way into individual
/// database names.  Note that the resulting vector's objects are
/// CTempString "slice" objects, and are only valid while the original
/// (encoded) string is unchanged.
///
/// @param dbname Combined database name.
/// @param dbs Database names to combine.

NCBI_XOBJREAD_EXPORT
void SeqDB_SplitQuoted(const string        & dbname,
                       vector<CTempString> & dbs);

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

/// Read a text SeqId list from a file.
///
/// The Seqids in a file are read into the provided SSeqIdOid vector.  The
/// SeqId half of each element of the vector is assigned, but the OID
/// half will be left as -1.  If the in_order parameter is not null,
/// the function will test the SeqIds for orderedness.  It will set the
/// bool to which in_order points to true if so, false if not.
///
/// @param fname    The name of the SeqId list file. [in]
/// @param gis      The GIs returned by this function. [out]
/// @param sis      The SeqIds returned by this function. [out]
/// @param in_order If non-null, returns true iff the SeqIds were in order. [out]

NCBI_XOBJREAD_EXPORT
void SeqDB_ReadSiList(const string                 & fname,
                      vector<CSeqDBGiList::SSiOid> & sis,
                      bool                         * in_order = 0);

/// Read a text SeqId list from a file.
///
/// The Seqids in a file are read into the provided SSeqIdOid vector.  The
/// Gi/Ti/Si half of each element of the vector is assigned, but the OID
/// half will be left as -1.  If the in_order parameter is not null,
/// the function will test the SeqIds for orderedness.  It will set the
/// bool to which in_order points to true if so, false if not.
///
/// @param fname    The name of the SeqId list file. [in]
/// @param tis      The TIs returned by this function. [out]
/// @param sis      The SeqIds returned by this function. [out]
/// @param in_order If non-null, returns true iff the SeqIds were in order. [out]

NCBI_XOBJREAD_EXPORT
void SeqDB_ReadMixList(const string & fname,
		               vector<CSeqDBGiList::SGiOid> & gis,
		               vector<CSeqDBGiList::STiOid> & tis,
		               vector<CSeqDBGiList::SSiOid> & sis,
		               bool * in_order);

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

/// Read a text or binary SeqId list from a file.
///
/// The SeqIds in a file are read into the provided vector<string>.  If the
/// in_order parameter is not null, the function will test the SeqIds for
/// orderedness.  It will set the bool to which in_order points to
/// true if so, false if not.
///
/// @param fname    The name of the SeqId list file. [in]
/// @param sis      The SeqIds returned by this function. [out]
/// @param in_order If non-null, returns true iff the SeqIds were in order. [out]

///NCBI_XOBJREAD_EXPORT
///void SeqDB_ReadSeqIdList(const string     & fname,
///                         vector<string>   & sis,
///                         bool             * in_order = 0);

/// Returns true if the file name passed contains a binary gi list
///
/// @param fname    The name of the GI list file. [in]
/// @throws CSeqDBException if file is invalid or empty
NCBI_XOBJREAD_EXPORT
bool SeqDB_IsBinaryGiList(const string  & fname);

/// Returns true if the file name passed contains a binary TI list
///
/// @param fname    The name of the TI list file. [in]
/// @throws CSeqDBException if file is invalid or empty
NCBI_XOBJREAD_EXPORT
bool SeqDB_IsBinaryTiList(const string  & fname);

/// CSeqDBFileGiList
/// 
/// This class defines a CSeqDBGiList subclass which reads a GI list
/// file given a filename.  It can read text or binary GI list files,
/// and will automatically distinguish between them.

class NCBI_XOBJREAD_EXPORT CSeqDBFileGiList : public CSeqDBGiList {
public:
    enum EIdType {
        eGiList,
        eTiList,
        eSiList,
        eMixList
    };

    /// Build a GI list from a file.
    CSeqDBFileGiList(const string & fname, EIdType idtype=eGiList);

    /// Build a GI list from multiple files.  (only support eSiList)
    CSeqDBFileGiList(vector<string> fnames, EIdType idtype=eGiList);
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

    /// The two lists of GIs are sorted and this class is computed as
    /// an intersection of them. Since gilist is negative this means
    /// all gi's in the vector that are NOT in the negative list.
    /// Note that both arguments to this
    /// function are potentially modified (sorted in place).
    CIntersectionGiList(CSeqDBNegativeList & gilist, vector<int> & gis);
};


/// Helper class to allow copy-on-write semantics for CSeqDBIdSet.
///
/// This class owns the actual vector of IDs for the CSeqDBIdSet list.

class CSeqDBIdSet_Vector : public CObject {
public:
    /// Default constructor.
    CSeqDBIdSet_Vector()
    {
    }
    
    /// Construct from an 'int' set.
    CSeqDBIdSet_Vector(const vector<int> & ids)
    {
        ITERATE(vector<int>, iter, ids) {
            m_Ids.push_back(*iter);
        }
    }
    
    /// Construct from an 'Int8' set.
    CSeqDBIdSet_Vector(const vector<Int8> & ids)
    {
        m_Ids = ids;
    }
    
    /// Access the Int8 set.
    vector<Int8> & Set()
    {
        return m_Ids;
    }
    
    /// Access the Int8 set.
    const vector<Int8> & Get() const
    {
        return m_Ids;
    }
    
    /// Get the number of elements stored here.
    size_t Size() const
    {
        return m_Ids.size();
    }
    
private:
    /// The actual list elements.
    vector<Int8> m_Ids;
    
    /// Prevent copy construction.
    CSeqDBIdSet_Vector(CSeqDBIdSet_Vector &);
    
    /// Prevent copy assignment.
    CSeqDBIdSet_Vector & operator=(CSeqDBIdSet_Vector &);
};


/// SeqDB ID list for performing boolean set operations.
///
/// This class permits boolean operations on lists of numeric IDs,
/// and can be passed to CSeqDB in the same way as a CSeqDBGiList.
/// CSeqDBGiList or CSeqDBNegativeList objects can be constructed as
/// well.  Logical operations supported include AND, OR, XOR, and NOT.
/// Internally this uses a CRef based copy-on-write scheme, so these
/// objects can be copied in constant time.

class NCBI_XOBJREAD_EXPORT CSeqDBIdSet : public CObject {
public:
    /// Types of operations that may be performed on GI lists.
    enum EOperation {
        eAnd, // Found in both X and Y
        eXor, // Found in X or Y, but not both
        eOr   // Found in either X or Y
    };
    
    /// Type of IDs stored here.
    enum EIdType {
        eGi,  // Found in both X and Y
        eTi   // Found in X or Y, but not both
    };
    
    /// Construct a 'blank' CSeqDBIdSet object.
    ///
    /// This produces a blank ID set object, which (if applied) would
    /// not cause any filtering to occur.  This is represented here as
    /// a negative ID list with no elements.
    ///
    CSeqDBIdSet();
    
    /// Build a computed ID list given an initial set of IDs.
    ///
    /// This initializes a list with an initial set of IDs of the
    /// specified type.  All further logic operations on the list
    /// should use vectors of IDs or CSeqDBIdSet objects
    /// initialized with the same EIdType enumeration.
    ///
    /// @param ids These IDs will be added to the list.
    /// @param t The IDs are assumed to be of this type.
    /// @param positive True for a positive ID list, false for negative.
    CSeqDBIdSet(const vector<int> & ids, EIdType t, bool positive = true);
    
    /// Build a computed ID list given an initial set of IDs.
    ///
    /// This initializes a list with an initial set of IDs of the
    /// specified type.  All further logic operations on the list
    /// should use vectors of IDs or CSeqDBIdSet objects
    /// initialized with the same EIdType enumeration.
    ///
    /// @param ids These IDs will be added to the list.
    /// @param t The IDs are assumed to be of this type.
    /// @param positive True for a positive ID list, false for negative.
    CSeqDBIdSet(const vector<Int8> & ids, EIdType t, bool positive = true);
    
    /// Virtual destructor.
    virtual ~CSeqDBIdSet()
    {
    }
    
    /// Invert the current list.
    void Negate();
    
    /// Perform a logical operation on a list.
    /// 
    /// The logical operation is performed between the current list
    /// and the ids parameter, and the 'positive' flag is used to
    /// determine if the new input list should be treated as a
    /// positive or negative list.  For example, using op == eOr and
    /// positive == false would perform the operation (X OR NOT Y).
    /// 
    /// @param op Logical operation to perform.
    /// @param ids List of ids for the second argument.
    /// @param positive True for positive lists, false for negative.
    void Compute(EOperation          op,
                 const vector<int> & ids,
                 bool                positive = true);
    
    /// Perform a logical operation on a list.
    /// 
    /// The logical operation is performed between the current list
    /// and the ids parameter, and the 'positive' flag is used to
    /// determine if the new input list should be treated as a
    /// positive or negative list.  For example, using op == eOr and
    /// positive == false would perform the operation (X OR NOT Y).
    /// 
    /// @param op Logical operation to perform.
    /// @param ids List of ids for the second argument.
    /// @param positive If true, ids represent 'negative' ids.
    void Compute(EOperation           op,
                 const vector<Int8> & ids,
                 bool                 positive = true);
    
    /// Perform a logical operation on a list.
    /// 
    /// The logical operation is performed between the current list
    /// and the ids parameter.  For example if 'eOr' is specified, the
    /// operation performed will be 'X OR Y'.  The 'ids' list will not
    /// be modified by this operation.
    /// 
    /// @param op Logical operation to perform.
    /// @param ids List of ids for the second argument.
    void Compute(EOperation op, const CSeqDBIdSet & ids);
    
    /// Checks whether a positive GI list was produced.
    ///
    /// If this method returns true, a positive list was produced, and
    /// can be retrieved with GetPositiveList().  If it returns false,
    /// a negative list was produced and can be retrieved with
    /// GetNegativeList().
    /// 
    /// @return true If the produced GI list is positive.
    bool IsPositive()
    {
        return m_Positive;
    }
    
    /// Retrieve a positive GI list.
    ///
    /// If IsPositive() returned true, this method should be used to
    /// retrieve a positive GI list.  If IsPositive() returned false,
    /// this method will throw an exception.
    CRef<CSeqDBGiList> GetPositiveList();
    
    /// Retrieve a negative GI list.
    ///
    /// If IsPositive() returned false, this method should be used to
    /// retrieve a positive GI list.  If IsPositive() returned true,
    /// this method will throw an exception.
    ///
    /// @return A negative GI list.
    CRef<CSeqDBNegativeList> GetNegativeList();
    
    /// Check if an ID list is blank.
    /// 
    /// An ID list is considered 'blank' iff it is a negative list
    /// with no elements.  Constructing a database with such a list is
    /// equivalent to not specifying a list.  Blank lists are produced
    /// by the default constructor, by specifying a negative list and
    /// providing an empty vector, or by computation (an intersection
    /// of disjoint negative lists, for example).  This method returns
    /// true in those cases; otherwise it returns false.
    ///
    /// @return True if this list is blank.
    bool Blank() const;
    
private:
    /// Sort and unique the internal set.
    static void x_SortAndUnique(vector<Int8> & ids);
    
    /// Compute inclusion flags for a boolean operation.
    ///
    /// This takes a logical operator (AND, OR, or XOR) and a flag
    /// indicating whether each input lists is positive or negative,
    /// and produces a flag indicating whether the resulting list will
    /// be positive or negative and three flags used to control the
    /// set merging operation.
    ///
    /// @param op The operation to perform (OR, AND, or XOR). [in]
    /// @param A_pos True if the first list is positive. [in]
    /// @param B_pos True if the second list is positive. [in]
    /// @param result_pos True if the result is a positive list. [out]
    /// @param incl_A True if ids found only in list A are kept. [out]
    /// @param incl_B True if ids found only in list B are kept. [out]
    /// @param incl_AB True if ids found in both lists are kept. [out]
    static void x_SummarizeBooleanOp(EOperation op,
                                     bool       A_pos,
                                     bool       B_pos,
                                     bool     & result_pos,
                                     bool     & incl_A,
                                     bool     & incl_B,
                                     bool     & incl_AB);
    
    /// Compute boolean operation on two vectors.
    ///
    /// This takes a logical operator (AND, OR, or XOR) and two
    /// positive or negative lists, and produces a positive or
    /// negative list representing that operation applied to those
    /// lists.
    ///
    /// @param op The operation to perform (OR, AND, or XOR). [in]
    /// @param A The first input list. [in]
    /// @param A_pos True if the first list is positive. [in]
    /// @param B The second input list. [in]
    /// @param B_pos True if the second list is positive. [in]
    /// @param result The resulting list of identifiers. [out]
    /// @param result_pos True if the result is a positive list. [out]
    void x_BooleanSetOperation(EOperation           op,
                               const vector<Int8> & A,
                               bool                 A_pos,
                               const vector<Int8> & B,
                               bool                 B_pos,
                               vector<Int8>       & result,
                               bool               & result_pos);
    
    /// True if the current list is positive.
    bool m_Positive;
    
    /// Id type.
    EIdType m_IdType;
    
    /// Ids stored here.
    CRef<CSeqDBIdSet_Vector> m_Ids;
    
    /// Cached positive list.
    CRef<CSeqDBGiList> m_CachedPositive;
    
    /// Cached negative list.
    CRef<CSeqDBNegativeList> m_CachedNegative;
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
    /// @param t the taxonomy ID to set for this structure
    SSeqDBTaxInfo(int t = 0)
        : taxid(t)
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

    friend ostream& operator<<(ostream& out, const SSeqDBTaxInfo& rhs) {
        out << "Taxid=" << rhs.taxid
            << "\tSciName=" << rhs.scientific_name
            << "\tCommonName=" << rhs.common_name
            << "\tBlastName=" << rhs.blast_name
            << "\tSuperKingdom=" << rhs.s_kingdom;
        return out;
    }
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

/// Resolve a file path using SeqDB's path algorithms.
///
/// Identical to SeqDB_ResolveDbPath with the exception that this function does
/// not require the extension to be provided. This is intended to check whether
/// a BLAST DB exists or not. 
///
/// @param filename Name of file to find.
/// @param dbtype Determines whether the BLAST DB is protein ('p'), nucleotide
/// ('n'), or whether the algorithm should guess it ('-')
/// @return Resolved path or empty string if not found.
NCBI_XOBJREAD_EXPORT
string SeqDB_ResolveDbPathNoExtension(const string & filename, 
                                      char dbtype = '-');

/// Resolve a file path using SeqDB's path algorithms.
///
/// Identical to SeqDB_ResolveDbPathNoExtension with the exception that this
/// function searches for ISAM files, specifically those storing numeric and 
/// string data (for LinkoutDB; i.e.: p[ns][id]).
/// This is intended to check whether a pair of ISAM files used in LinkoutDB
/// exists or not. 
///
/// @param filename Name of file to find.
/// @return Resolved path or empty string if not found.
NCBI_XOBJREAD_EXPORT
string SeqDB_ResolveDbPathForLinkoutDB(const string & filename);

/// Compares two volume file names and determine the volume order
///
/// @param volpath1 The 1st volume path 
/// @param volpath2 The 2nd volume path 
/// @return true if vol1 should appear before vol2
NCBI_XOBJREAD_EXPORT
bool SeqDB_CompareVolume(const string & volpath1, 
                         const string & volpath2);

/// Returns a path minus filename.
///
/// Substring version of the above.  This returns the part of a file
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
NCBI_XOBJREAD_EXPORT
unsigned SeqDB_SequenceHash(const char * sequence,
                            int          length);

/// Sequence Hashing For a CBioseq
///
/// This computes a hash of a sequence expressed as a CBioseq.
///
/// @param sequence The sequence. [in]
/// @return The 32 bit hash value.
NCBI_XOBJREAD_EXPORT
unsigned SeqDB_SequenceHash(const CBioseq & sequence);

/// Various identifier formats used in Id lookup
enum ESeqDBIdType {
    eGiId,     /// Genomic ID is a relatively stable numeric identifier for sequences.
    eTiId,     /// Trace ID is a numeric identifier for Trace sequences.
    ePigId,    /// Each PIG identifier refers to exactly one protein sequence.
    eStringId, /// Some sequence sources uses string identifiers.
    eHashId,   /// Lookup from sequence hash values to OIDs.
    eOID       /// The ordinal id indicates the order of the data in the volume's index file.
};

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
NCBI_XOBJREAD_EXPORT ESeqDBIdType 
SeqDB_SimplifySeqid(CSeq_id       & bestid,
                    const string  * acc,                                                                     
                    Int8          & num_id,                                                                  
                    string        & str_id,                                                                  
                    bool          & simpler);       
    
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
NCBI_XOBJREAD_EXPORT ESeqDBIdType 
SeqDB_SimplifyAccession(const string & acc,
                        Int8         & num_id,
                        string       & str_id,
                        bool         & simpler);

/// String id simplification.
///
/// This simpler version will convert string id to the standard
/// ISAM form, and return "" if the conversion fails.
///
/// @param acc
///   The string to look up. [in]
/// @return
///   The resulting converted id.
NCBI_XOBJREAD_EXPORT const string
SeqDB_SimplifyAccession(const string &acc);

/// Retrieves a list of all supported file extensions for BLAST databases
/// @param db_is_protein set to true if the database is protein else false [in]
/// @param extensions where the return value will be stored [in|out]
NCBI_XOBJREAD_EXPORT 
void SeqDB_GetFileExtensions(bool db_is_protein,
                             vector<string>& extensions);

END_NCBI_SCOPE

#endif // OBJTOOLS_BLAST_SEQDB_READER___SEQDBCOMMON__HPP

