#ifndef OBJTOOLS_READERS_SEQDB__SEQDBFILE_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBFILE_HPP

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

/// @file seqdbfile.hpp
/// File access objects for CSeqDB.
///
/// Defines classes:
///     CSeqDBRawFile
///     CSeqDBExtFile
///     CSeqDBIdxFile
///     CSeqDBSeqFile
///     CSeqDBHdrFile
///
/// Implemented for: UNIX, MS-Windows

#include "seqdbgeneral.hpp"
#include "seqdbatlas.hpp"

#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_bswap.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/readers/seqdb/seqdbcommon.hpp>
#include <set>

BEGIN_NCBI_SCOPE

/// Raw file.
///
/// This is the lowest level of SeqDB file object.  It controls basic
/// (byte data) access to the file, isolating higher levels from
/// differences in handling mmapped vs opened files.  This has mostly
/// become a thin wrapper around the Atlas functionality.

class CSeqDBRawFile {
public:
    /// Type which spans possible file offsets.
    typedef CSeqDBAtlas::TIndx TIndx;
    
    CSeqDBRawFile(CSeqDBAtlas & atlas)
        : m_Atlas(atlas)

    {
    }
    
    /// MMap or Open a file.
    bool Open(const string & name)
    {
        bool success = m_Atlas.GetFileSize(name, m_Length);
        
        if (success) {
            m_FileName = name;
        }
        
        return success;
    }
    
    const char * GetRegion(CSeqDBMemLease & lease, TIndx start, TIndx end, CSeqDBLockHold & locked) const
    {
        _ASSERT(! m_FileName.empty());
        _ASSERT(start    <  end);
        _ASSERT(m_Length >= end);
        
        m_Atlas.Lock(locked);
        
        if (! lease.Contains(start, end)) {
            m_Atlas.GetRegion(lease, m_FileName, start, end);
        }
        
        return lease.GetPtr(start);
    }
    
    const char * GetRegion(TIndx start, TIndx end, CSeqDBLockHold & locked) const
    {
        _ASSERT(! m_FileName.empty());
        _ASSERT(start    <  end);
        _ASSERT(m_Length >= end);
        
        return m_Atlas.GetRegion(m_FileName, start, end, locked);
    }
    
    TIndx GetFileLength(void) const
    {
        return m_Length;
    }
    
    TIndx ReadSwapped(CSeqDBMemLease & lease, TIndx offset, Uint4  * z, CSeqDBLockHold & locked) const;
    TIndx ReadSwapped(CSeqDBMemLease & lease, TIndx offset, Uint8  * z, CSeqDBLockHold & locked) const;
    TIndx ReadSwapped(CSeqDBMemLease & lease, TIndx offset, string * z, CSeqDBLockHold & locked) const;
    
    // Assumes locked.
    
    inline bool ReadBytes(CSeqDBMemLease & lease,
                          char           * buf,
                          TIndx            start,
                          TIndx            end) const;
    
private:
    CSeqDBAtlas    & m_Atlas;
    string           m_FileName;
    TIndx            m_Length;
};



/// Database component file
///
/// This represents any database component file with an extension like
/// "pxx" or "nxx".  This finds the correct type (protein or
/// nucleotide) if that is unknown, and computes the filename based on
/// a filename template like "path/to/file/basename.-in".
///
/// This also provides a 'protected' interface to the specific db
/// files, and defines a few useful methods.

class CSeqDBExtFile : public CObject {
public:
    /// Type which spans possible file offsets.
    typedef CSeqDBAtlas::TIndx TIndx;
    
    /// Constructor
    ///
    /// This builds an object which has a few properties required by
    /// most or all database volume component files.  This object
    /// keeps a lease on the file from the first access until
    /// instructed not to, moving and expanding that lease to cover
    /// incoming requests.  By keeping a lease, lookups, file opens,
    /// and other expensive operations are usually avoided on
    /// subsequent calls.  This object also provides some methods to
    /// read data in a byte swapped or direct way.
    /// @param atlas
    ///   The memory management layer object.
    /// @param dbfilename
    ///   The name of the managed file.
    /// @param prot_nucl
    ///   The sequence type.
    CSeqDBExtFile(CSeqDBAtlas   & atlas,
                  const string  & dbfilename,
                  char            prot_nucl);
    
    /// Destructor
    virtual ~CSeqDBExtFile()
    {
    }
    
    /// Release memory held in the atlas layer by this object.
    void UnLease()
    {
        m_Lease.Clear();
    }
    
protected:
    /// Get a region of the file
    ///
    /// This method is called to load part of the file into the lease
    /// object.  If the keep argument is set, an additional hold is
    /// acquired on the object, so that the user will conceptually own
    /// a hold on the object.  Such a hold should be returned with the
    /// top level RetSequence() method.
    ///
    /// @param start
    ///   The beginning offset of the region.
    const char * x_GetRegion(Uint4            start,
                             Uint4            end,
                             bool             keep,
                             CSeqDBLockHold & locked) const
    {
        m_Atlas.Lock(locked);
        
        if (! m_Lease.Contains(start, end)) {
            m_Atlas.GetRegion(m_Lease, m_FileName, start, end);
        }
        
        if (keep) {
            m_Lease.IncrementRefCnt();
        }
        
        return m_Lease.GetPtr(start);
    }
    
    // Assumes locked.
    
    void x_ReadBytes(char  * buf,
                     Uint4   start,
                     Uint4   end) const
    {
        m_File.ReadBytes(m_Lease, buf, start, end);
    }
    
    template<class T>
    TIndx x_ReadSwapped(CSeqDBMemLease & lease, TIndx offset, T * value, CSeqDBLockHold & locked)
    {
        return m_File.ReadSwapped(lease, offset, value, locked);
    }
    
    char x_GetSeqType(void) const
    {
        return m_ProtNucl;
    }
    
    void x_SetFileType(char prot_nucl);
    
    // Data
    
    CSeqDBAtlas            & m_Atlas;
    mutable CSeqDBMemLease   m_Lease;
    string                   m_FileName;
    char                     m_ProtNucl;
    CSeqDBRawFile            m_File;
};

void inline CSeqDBExtFile::x_SetFileType(char prot_nucl)
{
    m_ProtNucl = prot_nucl;
    
    if ((m_ProtNucl != kSeqTypeProt) &&
        (m_ProtNucl != kSeqTypeNucl) &&
        (m_ProtNucl != kSeqTypeUnkn)) {
        
        NCBI_THROW(CSeqDBException, eArgErr,
                   "Invalid argument: seq type must be 'p' or 'n'.");
    }
    
    _ASSERT(m_FileName.size() >= 5);
    
    m_FileName[m_FileName.size() - 3] = m_ProtNucl;
}


/// Index file
///
/// This is the .pin or .nin file; it provides indices into the other
/// files.  The version, title, date, and other summary information is
/// also stored here.

class CSeqDBIdxFile : public CSeqDBExtFile {
public:
    CSeqDBIdxFile(CSeqDBAtlas    & atlas,
                  const string   & dbname,
                  char             prot_nucl);
    
    virtual ~CSeqDBIdxFile()
    {
        CSeqDBLockHold locked(m_Atlas);
        
        m_Atlas.Lock(locked);
        m_Atlas.RetRegion((const char *) m_HdrRegion);
        m_Atlas.RetRegion((const char *) m_SeqRegion);
        
        _ASSERT((m_AmbRegion == 0) == (m_ProtNucl == kSeqTypeProt));
        
        if (m_AmbRegion) {
            m_Atlas.RetRegion((const char *) m_AmbRegion);
        }
    }
    
    inline bool
    GetAmbStartEnd(Uint4   oid,
                   Uint4 & start,
                   Uint4 & end) const;
    
    inline bool
    GetHdrStartEnd(Uint4   oid,
                   Uint4 & start,
                   Uint4 & end) const;
    
    inline bool
    GetSeqStartEnd(Uint4   oid,
                   Uint4 & start,
                   Uint4 & end) const;
    
    inline bool
    GetSeqStart(Uint4   oid,
                Uint4 & start) const;
    
    char GetSeqType(void) const
    {
        return x_GetSeqType();
    }
    
    string GetTitle(void) const
    {
        return m_Title;
    }
    
    string GetDate(void) const
    {
        return m_Date;
    }
    
    Uint4 GetNumOIDs(void) const
    {
        return m_NumOIDs;
    }
    
    Uint8 GetVolumeLength(void) const
    {
        return m_VolLen;
    }
    
    Uint4 GetMaxLength(void) const
    {
        return m_MaxLen;
    }
    
    void UnLease()
    {
    }
    
private:
    // Swapped data from .[pn]in file
    
    string m_Title;
    string m_Date;
    Uint4  m_NumOIDs;
    Uint8  m_VolLen;
    Uint4  m_MaxLen;
    
    // Other pointers and indices
    
    // These can be mutable because they:
    // 1. Do not constitute true object state.
    // 2. Are modified only under lock (CSeqDBRawFile::m_Atlas.m_Lock).
    
    Uint4 * m_HdrRegion;
    Uint4 * m_SeqRegion;
    Uint4 * m_AmbRegion;
};

bool
CSeqDBIdxFile::GetAmbStartEnd(Uint4 oid, Uint4 & start, Uint4 & end) const
{
    if (kSeqTypeNucl == x_GetSeqType()) {
        start = SeqDB_GetStdOrd(& m_AmbRegion[oid]);
        end   = SeqDB_GetStdOrd(& m_SeqRegion[oid+1]);
        
        return (start <= end);
    }
    
    return false;
}

bool
CSeqDBIdxFile::GetHdrStartEnd(Uint4 oid, Uint4 & start, Uint4 & end) const
{
    start = SeqDB_GetStdOrd(& m_HdrRegion[oid]);
    end   = SeqDB_GetStdOrd(& m_HdrRegion[oid+1]);
    
    return true;
}

bool
CSeqDBIdxFile::GetSeqStartEnd(Uint4 oid, Uint4 & start, Uint4 & end) const
{
    start = SeqDB_GetStdOrd(& m_SeqRegion[oid]);
    
    if (kSeqTypeProt == x_GetSeqType()) {
        end = SeqDB_GetStdOrd(& m_SeqRegion[oid+1]);
    } else {
        end = SeqDB_GetStdOrd(& m_AmbRegion[oid]);
    }
    
    return true;
}

bool
CSeqDBIdxFile::GetSeqStart(Uint4 oid, Uint4 & start) const
{
    start = SeqDB_GetStdOrd(& m_SeqRegion[oid]);
    
    return true;
}


/// Sequence data file
///
/// This is the .psq or .nsq file; it provides the raw sequence data,
/// and for nucleotide sequences, ambiguity data.  For nucleotide
/// sequences, the last byte will contain a two bit marker with a
/// number from 0-3, which indicates how much of the rest of that byte
/// is filled with base information (0-3 bases, which is 0-6 bits).
/// For ambiguous regions, the sequence data is normally randomized in
/// this file, to reduce the number of accidental false positives
/// during the search.  The ambiguity data encodes the location of,
/// and actual data for, those regions.

class CSeqDBSeqFile : public CSeqDBExtFile {
public:
    /// Type which spans possible file offsets.
    typedef CSeqDBAtlas::TIndx TIndx;
    
    CSeqDBSeqFile(CSeqDBAtlas   & atlas,
                  const string  & dbname,
                  char            prot_nucl)
        : CSeqDBExtFile(atlas, dbname + ".-sq", prot_nucl)
    {
    }
    
    virtual ~CSeqDBSeqFile()
    {
    }
    
    void ReadBytes(char  * buf,
                   TIndx   start,
                   TIndx   end) const
    {
        x_ReadBytes(buf, start, end);
    }
    
    const char * GetRegion(TIndx start, TIndx end, bool keep, CSeqDBLockHold & locked) const
    {
        return x_GetRegion(start, end, keep, locked);
    }
};


/// Header file
///
/// This is the .phr or .nhr file.  It contains descriptive data for
/// each sequence, including taxonomic information and identifiers for
/// sequence files.  The version, title, date, and other summary
/// information is also stored here.

class CSeqDBHdrFile : public CSeqDBExtFile {
public:
    /// Type which spans possible file offsets.
    typedef CSeqDBAtlas::TIndx TIndx;
    
    CSeqDBHdrFile(CSeqDBAtlas   & atlas,
                  const string  & dbname,
                  char            prot_nucl)
        : CSeqDBExtFile(atlas, dbname + ".-hr", prot_nucl)
    {
    }
    
    virtual ~CSeqDBHdrFile()
    {
    }
    
    void ReadBytes(char  * buf,
                   TIndx   start,
                   TIndx   end) const
    {
        x_ReadBytes(buf, start, end);
    }
    
    const char * GetRegion(TIndx start, TIndx end, CSeqDBLockHold & locked) const
    {
        return x_GetRegion(start, end, false, locked);
    }
};


// Does not modify (or use) internal file offset

// Assumes locked.

bool CSeqDBRawFile::ReadBytes(CSeqDBMemLease & lease,
                              char           * buf,
                              TIndx            start,
                              TIndx            end) const
{
    if (! lease.Contains(start, end)) {
        m_Atlas.GetRegion(lease, m_FileName, start, end);
    }
    
    memcpy(buf, lease.GetPtr(start), end-start);
    
    return true;
}

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBFILE_HPP


