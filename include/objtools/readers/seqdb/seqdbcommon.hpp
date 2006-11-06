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

BEGIN_NCBI_SCOPE

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
    /// Structure that holds GI-OID pairs.
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
    
    /// Access an element of the array.
    /// @param index The index of the element to access. [in]
    /// @return A reference to the GI/OID pair.
    const SGiOid & operator[](int index) const
    {
        return m_GisOids[index];
    }
    
    /// Get the number of elements in the array.
    int Size() const
    {
        return (int) m_GisOids.size();
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

    /// Get the gi list
    void GetGiList(vector<int>& gis) const;
    
protected:
    /// Indicates the current sort order, if any, of this container.
    ESortOrder m_CurrentOrder;
    
    /// Pairs of GIs and OIDs.
    vector<SGiOid> m_GisOids;
    
private:
    // The following disabled methods are reasonable things to do in
    // some cases.  But I suspect they are more likely to happen
    // accidentally than deliberately; due to the high performance
    // cost, I have prevented them.  If this kind of deep copy is
    // desireable, it can easily be enabled for a subclass by
    // assigning both of the data fields in the protected section.
    
    /// Prevent copy constructor.
    CSeqDBGiList(const CSeqDBGiList & other);
    
    /// Prevent assignment.
    CSeqDBGiList & operator=(const CSeqDBGiList & other);
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

END_NCBI_SCOPE

#endif // CORELIB__SEQDB__SEQDBCOMMON_HPP

