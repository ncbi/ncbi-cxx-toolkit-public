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


/// Used to indicate protein sequences.
const char kSeqTypeProt = 'p';

/// Used to indicate nucleotide sequences.
const char kSeqTypeNucl = 'n';

/// Used to indicate that sequence type should be selected by SeqDB;
/// this is no longer supported and will probably be removed soon.
const char kSeqTypeUnkn = '-';


/// Used to request ambiguities in Ncbi/NA8 format.
const int kSeqDBNuclNcbiNA8  = 0;

/// Used to request ambiguities in BLAST/NA8 format.
const int kSeqDBNuclBlastNA8 = 1;


/// Used to request that memory mapping be used.
const bool kSeqDBMMap   = true;

/// Used to request that memory mapping not be used.
const bool kSeqDBNoMMap = false;


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
        SGiOid(int gi_in = 0, int oid_in = -1)
            : gi(gi_in), oid(oid_in)
        {
        }
        
        int gi;
        int oid;
    };
    
    /// Possible sorting states
    enum ESortOrder {
        eNone,
        eGi,
        eOid
    };
    
    /// Constructor
    CSeqDBGiList();
    
    /// Destructor
    virtual ~CSeqDBGiList()
    {
    }
    
    /// Sort if necessary to insure order of elements.
    void InsureOrder(ESortOrder order);
    
    /// Test for existence of an OID, GI pair.
    void FindGis(const vector<int> & oids, vector<int> & gis);
    
    /// Access an element of the array.
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
        _ASSERT(m_CurrentOrder != eOid);
        m_GisOids[index].oid = oid;
    }
    
protected:
    ESortOrder     m_CurrentOrder;
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
    
    /// Test for existence of an OID, GI pair.
    void x_FindOid(int oid, int & indexB, int & indexE);
};


NCBI_XOBJREAD_EXPORT
void SeqDB_ReadBinaryGiList(const string & fname, vector<int> & gis);

NCBI_XOBJREAD_EXPORT
void SeqDB_ReadMemoryGiList(const char                   * fbeginp,
                            const char                   * fendp,
                            vector<CSeqDBGiList::SGiOid> & gis,
                            bool                         * in_order = 0);

NCBI_XOBJREAD_EXPORT
void SeqDB_ReadGiList(const string                 & fname,
                      vector<CSeqDBGiList::SGiOid> & gis,
                      bool                         * in_order = 0);


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

END_NCBI_SCOPE

#endif // CORELIB__SEQDB__SEQDBCOMMON_HPP


