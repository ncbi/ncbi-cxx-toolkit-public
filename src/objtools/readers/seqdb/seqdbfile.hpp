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

/// File access objects for CSeqDB
///
/// These objects define access to the various database component
/// files, such as name.pin, name.phr, name.psq, and so on.

#include "seqdbmempool.hpp"
#include "seqdbgeneral.hpp"

#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_bswap.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/readers/seqdb/seqdbcommon.hpp>
#include <set>

BEGIN_NCBI_SCOPE

/// Raw file.
///
/// This is the lowest level; it controls basic (byte data) access to
/// the file, isolating higher levels from differences in handling
/// mmapped vs opened files.

class CSeqDBRawFile : public CObject {
public:
    CSeqDBRawFile(CSeqDBMemPool & mempool, bool use_mmap)
        : m_Length (0),
          m_Mapped (0),
          m_MemPool(mempool),
          m_Offset (0),
          m_Opened (false),
          m_UseMMap(use_mmap)
    {
    }
    
    ~CSeqDBRawFile()
    {
        Clear();
    }
    
    /// MMap or Open a file.
    bool Open(const string & name);
    
    void Clear(void);
    
    bool Valid(void) const
    {
        return (m_Mapped || m_Opened) ? true : false;
    }
    
    const char * GetRegion(Uint4 start, Uint4 end) const;
    
    void SetFileOffset(Uint4 offset)
    {
        m_Offset = offset;
    }
    
    Uint4 GetFileOffset(void) const
    {
        return m_Offset;
    }
    
    Uint8 GetFileLength(void) const
    {
        return m_Length;
    }
    
    Uint8 Swap8(const char * input) const;
    
    void ReadSwapped(Uint4 * z);
    void ReadSwapped(Uint8 * z);
    void ReadSwapped(string * z);
    
    // Does not modify (or use) internal file offset
    bool ReadBytes(char * z, Uint4 start, Uint4 end) const;
    
private:
    bool x_ValidGet(Uint4 start, Uint4 end, Uint4 filesize) const
    {
        return (start <= end) && (end <= filesize);
    }
    
    bool x_ReadFileRegion(char * region, Uint4 start, Uint4 end) const;
    
    void x_SetLength(void);
    
    
    // Data
    
    mutable CFastMutex    m_FileLock;
    Uint8                 m_Length;
    CMemoryFile         * m_Mapped;
    CSeqDBMemPool       & m_MemPool;
    string                m_Name;
    Uint4                 m_Offset;
    bool                  m_Opened;
    mutable CNcbiIfstream m_Stream;
    bool                  m_UseMMap;
};

inline Uint8 CSeqDBRawFile::Swap8(const char * input) const
{
    unsigned char * bytes = (unsigned char *) input;
        
    Uint8 value = bytes[7];
    for(Int4 i = 7; i >= 0; i--) {
        value += bytes[i];
        if(i) value <<= 8;
    }
        
    value = 0;
    for(Int4 i = 7; i >= 0; i--) {
        value += bytes[i];
        if(i) value <<= 8;
    }
        
    return value;
}


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
    CSeqDBExtFile(CSeqDBMemPool & mempool,
                  const string  & dbfilename,
                  char            prot_nucl,
                  bool            use_mmap);
    
    virtual ~CSeqDBExtFile()
    {
    }
    
protected:
    const char * x_GetRegion(Uint4 start, Uint4 end) const
    {
        return m_File.GetRegion(start, end);
    }
    
    Uint4 x_GetFileOffset(void) const
    {
        return m_File.GetFileOffset();
    }
    
    void x_SetFileOffset(Uint4 offset)
    {
        m_File.SetFileOffset(offset);
    }
    
    void x_ReadBytes(char * x, Uint4 start, Uint4 end) const
    {
        m_File.ReadBytes(x, start, end);
    }
    
    template<class T>
    void x_ReadSwapped(T * x)
    {
        m_File.ReadSwapped(x);
    }
    
    char x_GetSeqType(void) const
    {
        return m_ProtNucl;
    }
    
private:
    void x_SetFileType(char prot_nucl);
    
    // Data
    
    string        m_FileName;
    char          m_ProtNucl;
    CSeqDBRawFile m_File;
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


/// Index files
///
/// This is the .pin or .nin file; it provides indices into the other
/// files.  The version, title, date, and other summary information is
/// also stored here.

class CSeqDBIdxFile : public CSeqDBExtFile {
public:
    CSeqDBIdxFile(CSeqDBMemPool & mempool,
                  const string  & dbname,
                  char            prot_nucl,
                  bool            use_mmap);
    
    virtual ~CSeqDBIdxFile()
    {
    }
    
    bool GetSeqStartEnd(Uint4 oid, Uint4 & start, Uint4 & end) const;
    
    bool GetHdrStartEnd(Uint4 oid, Uint4 & start, Uint4 & end) const;
    
    bool GetAmbStartEnd(Uint4 oid, Uint4 & start, Uint4 & end) const;
    
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
    
    Uint4 GetNumSeqs(void) const
    {
        return m_NumSeqs;
    }
    
    Uint8 GetTotalLength(void) const
    {
        return m_TotLen;
    }
    
    Uint4 GetMaxLength(void) const
    {
        return m_MaxLen;
    }
    
private:
    Uint4 x_GetSeqOffset(int oid) const
    {
	return SeqDB_GetStdOrd( (const Uint4 *)(m_SeqHandle + (oid * 4)) );
    }
    
    Uint4 x_GetAmbCharOffset(int oid) const
    {
        return SeqDB_GetStdOrd( (const Uint4 *)(m_AmbCharHandle + (oid * 4)) );
    }
    
    Uint4 x_GetHdrOffset(int oid) const
    {
        return SeqDB_GetStdOrd( (const Uint4 *)(m_HdrHandle + (oid * 4)) );
    }
    
    // Swapped data from .[pn]in file
    
    string m_Title;   // L3083,3087
    string m_Date;    // L3094,3097
    Uint4  m_NumSeqs; // L3101
    Uint8  m_TotLen;  // L3110
    Uint4  m_MaxLen;  // L3126
    
    // Other pointers and indices
    
    const char * m_HdrHandle;
    const char * m_SeqHandle;
    const char * m_AmbCharHandle;
};

bool inline
CSeqDBIdxFile::GetSeqStartEnd(Uint4 oid, Uint4 & start, Uint4 & end) const
{
    start = x_GetSeqOffset(oid);
        
    if (kSeqTypeProt == x_GetSeqType()) {
        end = x_GetSeqOffset(oid + 1);
    } else {
        end = x_GetAmbCharOffset(oid);
    }
        
    return true;
}
    
bool inline
CSeqDBIdxFile::GetHdrStartEnd(Uint4 oid, Uint4 & start, Uint4 & end) const
{
    start = x_GetHdrOffset(oid);
    end   = x_GetHdrOffset(oid + 1);
        
    return true;
}
    
bool inline
CSeqDBIdxFile::GetAmbStartEnd(Uint4 oid, Uint4 & start, Uint4 & end) const
{
    if (kSeqTypeNucl == x_GetSeqType()) {
        start = x_GetAmbCharOffset(oid);
        end   = x_GetSeqOffset(oid + 1);
        
        if (start <= end) {
            return true;
        }
    }
    
    return false;
}


class CSeqDBSeqFile : public CSeqDBExtFile {
public:
    CSeqDBSeqFile(CSeqDBMemPool & mempool,
                  const string  & dbname,
                  char            prot_nucl,
                  bool            use_mmap)
        : CSeqDBExtFile(mempool, dbname + ".-sq", prot_nucl, use_mmap)
    {
    }
    
    virtual ~CSeqDBSeqFile()
    {
    }
    
    void ReadBytes(char * x, Uint4 start, Uint4 end) const
    {
        x_ReadBytes(x, start, end);
    }
    
    const char * GetRegion(Uint4 start, Uint4 end) const
    {
        return x_GetRegion(start, end);
    }
};


class CSeqDBHdrFile : public CSeqDBExtFile {
public:
    CSeqDBHdrFile(CSeqDBMemPool & mempool,
                  const string  & dbname,
                  char            prot_nucl,
                  bool            use_mmap)
        : CSeqDBExtFile(mempool, dbname + ".-hr", prot_nucl, use_mmap)
    {
    }
    
    virtual ~CSeqDBHdrFile()
    {
    }
    
    void ReadBytes(char * x, Uint4 start, Uint4 end) const
    {
        x_ReadBytes(x, start, end);
    }
    
    const char * GetRegion(Uint4 start, Uint4 end) const
    {
        return x_GetRegion(start, end);
    }
};


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBFILE_HPP


