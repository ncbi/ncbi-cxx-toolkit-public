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
    
    /// Constructor
    ///
    /// Builds a "raw" file object, which is the lowest level of the
    /// SeqDB file objects.  It provides byte swapping and reading
    /// methods, which are implemented via the atlas layer.
    ///
    /// @param atlas
    ///     The memory management layer object.
    CSeqDBRawFile(CSeqDBAtlas & atlas)
        : m_Atlas(atlas)

    {
    }
    
    /// MMap or Open a file.
    ///
    /// This serves to verify the existence of, open, and cache the
    /// length of a file.
    ///
    /// @param name
    ///   The filename to open.
    /// @return
    ///   true if the file was opened successfully.
    /// @param locked
    ///   The lock holder object for this thread.
    bool Open(const string & name, CSeqDBLockHold & locked)
    {
        bool success = m_Atlas.GetFileSize(name, m_Length, locked);
        
        if (success) {
            m_FileName = name;
        }
        
        return success;
    }
    
    /// Get a pointer to a section of the file.
    ///
    /// This method insures that the memory lease has a hold that
    /// includes the requested section of the file, and returns a
    /// pointer to the start offset.
    ///
    /// @param lease
    ///     The memory lease object for this file.
    /// @param start
    ///     The starting offset for the first byte of the region.
    /// @param end
    ///     The offset for the first byte after the region.
    /// @param locked
    ///     The lock holder object for this thread.
    /// @return
    ///     A pointer to the file data at the start offset.
    const char * GetRegion(CSeqDBMemLease & lease,
                           TIndx            start,
                           TIndx            end,
                           CSeqDBLockHold & locked) const
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
    
    /// Get a pointer to a section of the file.
    ///
    /// This method asks the atlas for a hold on the memory region
    /// that includes the requested section of the file, and returns a
    /// pointer to the start offset in that memory area.
    ///
    /// @param start
    ///     The starting offset for the first byte of the region.
    /// @param end
    ///     The offset for the first byte after the region.
    /// @param locked
    ///     The lock holder object for this thread.
    /// @return
    ///     A pointer to the file data.
    const char * GetRegion(TIndx            start,
                           TIndx            end,
                           CSeqDBLockHold & locked) const
    {
        _ASSERT(! m_FileName.empty());
        _ASSERT(start    <  end);
        _ASSERT(m_Length >= end);
        
        return m_Atlas.GetRegion(m_FileName, start, end, locked);
    }
    
    /// Get the length of the file.
    ///
    /// The file length is returned as a four byte integer, which is
    /// the current maximum size for the blastdb component files.
    ///
    /// @return
    ///     The length of the file.
    TIndx GetFileLength() const
    {
        return m_Length;
    }
    
    /// Read a four byte numerical object from the file
    ///
    /// Given a pointer to an object in memory, this reads a numerical
    /// value for it from the file.  The data in the file is assumed
    /// to be in network byte order, and the user version in the local
    /// default byte order (host order).  The size of the object is
    /// taken as sizeof(Uint4).
    ///
    /// @param lease
    ///     A memory lease object to use for the read.
    /// @param offset
    ///     The starting offset of the value in the file.
    /// @param value
    ///     A pointer to the object.
    /// @param locked
    ///     The lock holder object for this thread.
    /// @return
    ///     The offset of the first byte after the object.
    TIndx ReadSwapped(CSeqDBMemLease & lease,
                      TIndx            offset,
                      Uint4          * value,
                      CSeqDBLockHold & locked) const;

    /// Read an eight byte numerical object from the file
    ///
    /// Given a pointer to an object in memory, this reads a numerical
    /// value for it from the file.  The data in the file is assumed
    /// to be in network byte order, and the user version in the local
    /// default byte order (host order).  The size of the object is
    /// taken as sizeof(Uint8).
    ///
    /// @param lease
    ///     A memory lease object to use for the read.
    /// @param offset
    ///     The starting offset of the value in the file.
    /// @param value
    ///     A pointer to the object.
    /// @param locked
    ///     The lock holder object for this thread.
    /// @return
    ///     The offset of the first byte after the object.
    TIndx ReadSwapped(CSeqDBMemLease & lease,
                      TIndx            offset,
                      Uint8          * value,
                      CSeqDBLockHold & locked) const;

    /// Read a string object from the file
    ///
    /// Given a pointer to a string object, this reads a string value
    /// for it from the file.  The data in the file is assumed to be a
    /// four byte length in network byte order, followed by the bytes
    /// of the string.  The amount of data is this length + 4.
    ///
    /// @param lease
    ///     A memory lease object to use for the read.
    /// @param offset
    ///     The starting offset of the string length in the file.
    /// @param value
    ///     A pointer to the returned string.
    /// @param locked
    ///     The lock holder object for this thread.
    /// @return
    ///     The offset of the first byte after the string.
    TIndx ReadSwapped(CSeqDBMemLease & lease,
                      TIndx            offset,
                      string         * value,
                      CSeqDBLockHold & locked) const;

    /// Read part of the file into a buffer
    ///
    /// Copy the file data from offsets start to end into the array at
    /// buf, which is assumed to already have been allocated.  This
    /// method assumes the atlas lock is held.
    ///
    /// @param lease
    ///     A memory lease object to use for the read.
    /// @param buf
    ///     The destination for the data to be read.
    /// @param start
    ///     The starting offset for the first byte to read.
    /// @param end
    ///     The offset for the first byte after the area to read.
    inline void ReadBytes(CSeqDBMemLease & lease,
                          char           * buf,
                          TIndx            start,
                          TIndx            end) const;
    
private:
    /// The memory management layer object.
    CSeqDBAtlas & m_Atlas;
    
    /// The name of this file.
    string m_FileName;
    
    /// The length of this file.
    TIndx m_Length;
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
    ///   The sequence data type.
    /// @param locked
    ///   The lock holder object for this thread.
    CSeqDBExtFile(CSeqDBAtlas    & atlas,
                  const string   & dbfilename,
                  char             prot_nucl,
                  CSeqDBLockHold & locked);
    
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
    /// @param end
    ///     The offset for the first byte after the area to read.
    /// @param keep
    ///     True if an extra hold should be acquired on the data.
    /// @param locked
    ///     The lock holder object for this thread.
    const char * x_GetRegion(TIndx            start,
                             TIndx            end,
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
    
    /// Read part of the file into a buffer
    ///
    /// Copy the file data from offsets start to end into the array at
    /// buf, which is assumed to already have been allocated.  This
    /// method assumes the atlas lock is held.
    ///
    /// @param buf
    ///     The destination for the data to be read.
    /// @param start
    ///     The starting offset for the first byte to read.
    /// @param end
    ///     The offset for the first byte after the area to read.
    void x_ReadBytes(char  * buf,
                     TIndx   start,
                     TIndx   end) const
    {
        m_File.ReadBytes(m_Lease, buf, start, end);
    }
    
    /// Read a numerical object from the file
    ///
    /// Given a pointer to an object in memory, this reads a numerical
    /// value for it from the file.  The data in the file is assumed
    /// to be in network byte order, and the user version in the local
    /// default byte order (host order).  The offset of the data is
    /// provided, and the size of the object is taken as sizeof(T).
    ///
    /// @param lease
    ///     A memory lease object to use for the read.
    /// @param offset
    ///     The starting offset of the object in the file.
    /// @param value
    ///     A pointer to the object.
    /// @param locked
    ///     The lock holder object for this thread.
    /// @return
    ///     The offset of the first byte after the object.
    template<class T>
    TIndx x_ReadSwapped(CSeqDBMemLease & lease,
                        TIndx            offset,
                        T              * value,
                        CSeqDBLockHold & locked)
    {
        return m_File.ReadSwapped(lease, offset, value, locked);
    }
    
    /// Get the volume's sequence data type.
    ///
    /// This object knows which type of sequence data it deals with -
    /// this method returns that information.
    ///
    /// @return
    ///     The type of sequence data in use.
    char x_GetSeqType() const
    {
        return m_ProtNucl;
    }
    
    /// Sets the sequence data type.
    ///
    /// The sequence data will be set as protein or nucleotide.  An
    /// exception is thrown if an invalid type is provided.  The first
    /// character of the file extension will be modified to reflect
    /// the sequence data type.
    ///
    /// @param prot_nucl
    ///     Either 'p' or 'n' for protein or nucleotide.
    void x_SetFileType(char prot_nucl);
    
    // Data
    
    /// The memory layer management object.
    CSeqDBAtlas & m_Atlas;
    
    /// A memory lease used by this file.
    mutable CSeqDBMemLease m_Lease;

    /// The name of this file.
    string m_FileName;
    
    /// Either 'p' for protein or 'n' for nucleotide.
    char m_ProtNucl;
    
    /// The raw file object.
    CSeqDBRawFile m_File;
};

void inline CSeqDBExtFile::x_SetFileType(char prot_nucl)
{
    m_ProtNucl = prot_nucl;
    
    if ((m_ProtNucl != kSeqTypeProt) &&
        (m_ProtNucl != kSeqTypeNucl)) {
        
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
    /// Constructor
    ///
    /// This builds an object which provides access to the index file
    /// for a volume.  The index file contains metadata about the
    /// volume, such as the title and construction date.  The index
    /// file also contains indexes into the header and sequence data
    /// files.  Because these offsets are four byte integers, all
    /// volumes have a size of no more than 2^32 bytes, but in
    /// practice, they are usually kept under 2^30 bytes.
    /// @param atlas
    ///   The memory management layer object.
    /// @param dbname
    ///   The name of the database volume.
    /// @param prot_nucl
    ///   The sequence data type.
    /// @param locked
    ///   The lock holder object for this thread.
    CSeqDBIdxFile(CSeqDBAtlas    & atlas,
                  const string   & dbname,
                  char             prot_nucl,
                  CSeqDBLockHold & locked);
    
    /// Destructor
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
    
    /// Get the location of a sequence's ambiguity data
    ///
    /// This method returns the offsets of the start and end of the
    /// ambiguity data for a specific nucleotide sequence.  If this
    /// range is non-empty, then this sequence has ambiguous regions,
    /// which are encoded as a series of instructions for modifying
    /// the compressed 4 base/byte nucleotide data.  The ambiguity
    /// data is encoded as randomized noise, with the intention of
    /// minimizing accidental matches.
    ///
    /// @param oid
    ///   The sequence to get data for.
    /// @param start
    ///   The returned start offset of the sequence.
    /// @param end
    ///   The returned end offset of the sequence.
    /// @return
    ///   true if the sequence has ambiguity data.
    inline bool
    GetAmbStartEnd(int     oid,
                   TIndx & start,
                   TIndx & end) const;
    
    /// Get the location of a sequence's header data
    ///
    /// This method returns the offsets of the start and end of the
    /// header data for a specific database sequence.  The header data
    /// is a Blast-def-line-set in binary ASN.1.  This data includes
    /// associated taxonomy data, Seq-ids, and membership bits.
    ///
    /// @param oid
    ///   The sequence to get data for.
    /// @param start
    ///   The returned start offset of the sequence.
    /// @param end
    ///   The returned end offset of the sequence.
    inline void
    GetHdrStartEnd(int     oid,
                   TIndx & start,
                   TIndx & end) const;
    
    /// Get the location of a sequence's packed sequence data
    ///
    /// This method returns the offsets of the start and end of the
    /// packed sequence data for a specific database sequence.  For
    /// protein data, the packed version is the only supported
    /// encoding, and is stored at one base per byte.  The header data
    /// is encoded as a Blast-def-line-set in binary ASN.1.  This data
    /// includes taxonomy information, Seq-ids for this sequence, and
    /// membership bits.
    ///
    /// @param oid
    ///   The sequence to get data for.
    /// @param start
    ///   The returned start offset of the sequence.
    /// @param end
    ///   The returned end offset of the sequence.
    inline void
    GetSeqStartEnd(int     oid,
                   TIndx & start,
                   TIndx & end) const;
    
    /// Get the location of a sequence's packed sequence data
    ///
    /// This method returns the offsets of the start and end of the
    /// packed sequence data for a specific database sequence.  For
    /// protein data, the packed version is the only supported
    /// encoding, and is stored at one base per byte.  The header data
    /// is encoded as a Blast-def-line-set in binary ASN.1.  This data
    /// includes taxonomy information, Seq-ids for this sequence, and
    /// membership bits.
    ///
    /// @param oid
    ///   The sequence to get data for.
    /// @param start
    ///   The returned start offset of the sequence.
    inline void
    GetSeqStart(int     oid,
                TIndx & start) const;
    
    /// Get the sequence data type.
    char GetSeqType() const
    {
        return x_GetSeqType();
    }
    
    /// Get the volume title.
    string GetTitle() const
    {
        return m_Title;
    }
    
    /// Get the construction date of the volume.
    string GetDate() const
    {
        return m_Date;
    }
    
    /// Get the number of oids in this volume.
    int GetNumOIDs() const
    {
        return m_NumOIDs;
    }
    
    /// Get the length of the volume (in bases).
    Uint8 GetVolumeLength() const
    {
        return m_VolLen;
    }
    
    /// Get the length of the longest sequence in this volume.
    int GetMaxLength() const
    {
        return m_MaxLen;
    }
    
    /// Release any memory leases temporarily held here.
    ///
    /// This is a placeholder - the current design does not use a
    /// memory lease for this particular file.
    void UnLease()
    {
    }
    
private:
    // Swapped data from .[pn]in file
    
    /// The volume title.
    string m_Title;
    
    /// The construction date of the volume.
    string m_Date;
    
    /// The number of oids in this volume.
    Uint4 m_NumOIDs;
    
    /// The length of the volume (in bases).
    Uint8 m_VolLen;
    
    /// The length of the longest sequence in this volume.
    Uint4 m_MaxLen;
    
    // Other pointers and indices
    
    // These can be mutable because they:
    // 1. Do not constitute true object state.
    // 2. Are modified only under lock (CSeqDBRawFile::m_Atlas.m_Lock).
    
    /// Pointer to the array of header file offsets.
    Uint4 * m_HdrRegion;
    
    /// Pointer to the array of sequence file offsets.
    Uint4 * m_SeqRegion;
    
    /// Pointer to the array of offsets of the ambiguity data (for
    /// nucleotide volumes only).
    Uint4 * m_AmbRegion;
};

bool
CSeqDBIdxFile::GetAmbStartEnd(int oid, TIndx & start, TIndx & end) const
{
    if (kSeqTypeNucl == x_GetSeqType()) {
        start = SeqDB_GetStdOrd(& m_AmbRegion[oid]);
        end   = SeqDB_GetStdOrd(& m_SeqRegion[oid+1]);
        
        return (start <= end);
    }
    
    return false;
}

void
CSeqDBIdxFile::GetHdrStartEnd(int oid, TIndx & start, TIndx & end) const
{
    start = SeqDB_GetStdOrd(& m_HdrRegion[oid]);
    end   = SeqDB_GetStdOrd(& m_HdrRegion[oid+1]);
}

void
CSeqDBIdxFile::GetSeqStartEnd(int oid, TIndx & start, TIndx & end) const
{
    start = SeqDB_GetStdOrd(& m_SeqRegion[oid]);
    
    if (kSeqTypeProt == x_GetSeqType()) {
        end = SeqDB_GetStdOrd(& m_SeqRegion[oid+1]);
    } else {
        end = SeqDB_GetStdOrd(& m_AmbRegion[oid]);
    }
}

void
CSeqDBIdxFile::GetSeqStart(int oid, TIndx & start) const
{
    start = SeqDB_GetStdOrd(& m_SeqRegion[oid]);
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
    
    /// Constructor
    ///
    /// This builds an object which provides access to the sequence
    /// data file for a volume.  This file is simply a concatenation
    /// of all the sequence data for the database sequences.  In a
    /// protein file, these are just the database sequences seperated
    /// by NUL bytes.  In a nucleotide volume, the packed data for
    /// each sequence is followed by ambiguity data for that sequence
    /// (if any such data exists).
    ///
    /// @param atlas
    ///   The memory management layer object.
    /// @param dbname
    ///   The name of the database volume.
    /// @param prot_nucl
    ///   The sequence data type.
    /// @param locked
    ///   The lock holder object for this thread.
    CSeqDBSeqFile(CSeqDBAtlas    & atlas,
                  const string   & dbname,
                  char             prot_nucl,
                  CSeqDBLockHold & locked)
        : CSeqDBExtFile(atlas, dbname + ".-sq", prot_nucl, locked)
    {
    }
    
    /// Destructor
    virtual ~CSeqDBSeqFile()
    {
    }
    
    /// Read part of the file into a buffer
    ///
    /// Copy the sequence data from offsets start to end into the
    /// array at buf, which is assumed to already have been allocated.
    /// This method assumes the atlas lock is held.
    ///
    /// @param buf
    ///     The destination for the data to be read.
    /// @param start
    ///     The starting offset for the first byte to read.
    /// @param end
    ///     The offset for the first byte after the area to read.
    void ReadBytes(char  * buf,
                   TIndx   start,
                   TIndx   end) const
    {
        x_ReadBytes(buf, start, end);
    }
    
    /// Get a pointer into the file contents.
    ///
    /// Copy the sequence data from offsets start to end into the
    /// array at buf, which is assumed to already have been allocated.
    /// This method assumes the atlas lock is held.  If the user will
    /// take ownership of the memory region hold, the keep argument
    /// should be specified as true.
    ///
    /// @param start
    ///     The starting offset for the first byte to read.
    /// @param end
    ///     The offset for the first byte after the area to read.
    /// @param keep
    ///     True if an extra hold should be acquired on the data.
    /// @param locked
    ///     The lock holder object for this thread.
    /// @return
    ///     A pointer into the file data.
    const char * GetRegion(TIndx            start,
                           TIndx            end,
                           bool             keep,
                           CSeqDBLockHold & locked) const
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
    
    /// Constructor
    ///
    /// This builds an object which provides access to the header data
    /// file for a volume.  This file is simply a concatenation of the
    /// header data for each object, stored as a Blast-def-line-set
    /// objects in binary ASN.1.
    ///
    /// @param atlas
    ///   The memory management layer object.
    /// @param dbname
    ///   The name of the database volume.
    /// @param prot_nucl
    ///   The sequence data type.
    /// @param locked
    ///   The lock holder object for this thread.
    CSeqDBHdrFile(CSeqDBAtlas    & atlas,
                  const string   & dbname,
                  char             prot_nucl,
                  CSeqDBLockHold & locked)
        : CSeqDBExtFile(atlas, dbname + ".-hr", prot_nucl, locked)
    {
    }
    
    /// Destructor
    virtual ~CSeqDBHdrFile()
    {
    }
    
    /// Read part of the file into a buffer
    ///
    /// Copy the sequence data from offsets start to end into the
    /// array at buf, which is assumed to already have been allocated.
    /// This method assumes the atlas lock is held.  If the user will
    /// take ownership of the memory region hold, the keep argument
    /// should be specified as true.
    ///
    /// @param buf
    ///     The buffer to receive the data.
    /// @param start
    ///     The starting offset for the first byte to read.
    /// @param end
    ///     The offset for the first byte after the area to read.
    void ReadBytes(char  * buf,
                   TIndx   start,
                   TIndx   end) const
    {
        x_ReadBytes(buf, start, end);
    }
    
    /// Read part of the file into a buffer
    ///
    /// Copy the sequence data from offsets start to end into the
    /// array at buf, which is assumed to already have been allocated.
    /// This method assumes the atlas lock is held.  If the user will
    /// take ownership of the memory region hold, the keep argument
    /// should be specified as true.
    ///
    /// @param start
    ///     The starting offset for the first byte to read.
    /// @param end
    ///     The offset for the first byte after the area to read.
    /// @param locked
    ///     The lock holder object for this thread.
    /// @return
    ///     A pointer into the file data.
    const char * GetRegion(TIndx            start,
                           TIndx            end,
                           CSeqDBLockHold & locked) const
    {
        return x_GetRegion(start, end, false, locked);
    }
};


// Does not modify (or use) internal file offset

// Assumes locked.

void CSeqDBRawFile::ReadBytes(CSeqDBMemLease & lease,
                              char           * buf,
                              TIndx            start,
                              TIndx            end) const
{
    if (! lease.Contains(start, end)) {
        m_Atlas.GetRegion(lease, m_FileName, start, end);
    }
    
    memcpy(buf, lease.GetPtr(start), end-start);
}

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBFILE_HPP


