#ifndef CORELIB__SEQDB__SEQDBFILE_HPP
#define CORELIB__SEQDB__SEQDBFILE_HPP

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

#include <seqdbcommon.hpp>
#include <corelib/ncbi_bswap.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbiobj.hpp>
#include <set>

BEGIN_NCBI_SCOPE

/// Raw file.
///
/// This is the lowest level; it controls basic (byte data) access to
/// the file, isolating higher levels from differences in handling
/// mmapped vs opened files.

class CSeqDBRawFile {
public:
    CSeqDBRawFile(bool use_mmap)
        : m_UseMMap(use_mmap),
          m_Mapped (0),
          m_Opened (false),
          m_Offset (0)
    {
    }

    ~CSeqDBRawFile()
    {
        Clear();
    }
    
    /// MMap or Open a file.
    bool Open(const string & name);
    
    void Clear(void);
    
    bool Valid(void)
    {
        return (m_Mapped || m_Opened) ? true : false;
    }
    
    const char * GetRegion(Uint4 start, Uint4 end);
    
    bool RetRegion(const char * buffer)
    {
        return x_DelRegionHandle(buffer);
    }
    
    void SetFileOffset(Uint4 offset)
    {
        m_Offset = offset;
    }
    
    Uint4 GetFileOffset(void)
    {
        return m_Offset;
    }
    
    Uint8 GetFileLength(void);
    
    Uint8 Swap8(const char * input);
    
    void ReadSwapped(Uint4 * z);
    void ReadSwapped(Uint8 * z);
    void ReadSwapped(string * z);
    
    // Does not modify (or use) internal file offset
    bool ReadBytes(char * z, Uint4 start, Uint4 end);
    
private:
    Int4 x_GetOpenedLength(void);
    
    bool x_ValidGet(Uint4 start, Uint4 end, Uint4 filesize)
    {
        return (start <= end) && (end <= filesize);
    }
    
    bool x_ReadFileRegion(char * region, Uint4 start, Uint4 end);
    
    void x_AddRegionHandle(char * region)
    {
        if (! m_Mapped) {
            _ASSERT(m_Regions.end() == m_Regions.find((Uint4*)region));
            ifdebug_rh << "RH insert region [" << int(region) << "]" << endl;
            m_Regions.insert((Uint4*) region);
        }
    }
    
    bool x_DelRegionHandle(const char * region)
    {
        if (! m_Mapped) {
            if (m_Regions.end() == m_Regions.find((Uint4*)region)) {
                return false;
            }
            
            ifdebug_rh << "RH delete region [" << int(region) << "]" << endl;
            delete [] region;
            m_Regions.erase((Uint4*) region);
        }
        
        return true;
    }
    
    void x_ClearRegionHandles(void)
    {
        for(TRHIter i = m_Regions.begin(); i != m_Regions.end(); i++) {
            Uint4 * rh = *i;
            ifdebug_rh << "RH delete(clear) [" << int(rh) << "]" << endl;
            delete [] rh;
        }
        
        ifdebug_rhsum << "RH deletion count=" << m_Regions.size() << endl;
        m_Regions.clear();
    }
    
    // Data
    
    typedef set<Uint4*>      TRHSet;
    typedef TRHSet::iterator TRHIter;
    
    string           m_Name;
    bool             m_UseMMap;
    CMemoryFile    * m_Mapped;
    bool             m_Opened;
    CNcbiIfstream    m_Stream;
    Uint4            m_Offset;
    TRHSet           m_Regions;
};

inline Uint8 CSeqDBRawFile::Swap8(const char * input)
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
    CSeqDBExtFile(const string & dbfilename, char prot_nucl, bool use_mmap)
        : m_FileName(dbfilename),
          m_File    (use_mmap)
    {
        x_SetFileType(prot_nucl);
    }
    
    virtual ~CSeqDBExtFile()
    {
    }
    
protected:
    const char * x_GetRegion(Uint4 start, Uint4 end)
    {
        return m_File.GetRegion(start, end);
    }
    
    bool x_RetRegion(const char * region)
    {
        return m_File.RetRegion(region);
    }
    
    Uint4 x_GetFileOffset(void)
    {
        return m_File.GetFileOffset();
    }
    
    void x_SetFileOffset(Uint4 offset)
    {
        m_File.SetFileOffset(offset);
    }
    
    void x_ReadBytes(char * x, Uint4 start, Uint4 end)
    {
        m_File.ReadBytes(x, start, end);
    }
    
    template<class T>
    void x_ReadSwapped(T * x)
    {
        m_File.ReadSwapped(x);
    }
    
    bool x_NeedFile(void);
    
    char x_GetSeqType(void)
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
        
    _ASSERT((m_ProtNucl == kSeqTypeProt) ||
            (m_ProtNucl == kSeqTypeNucl) ||
            (m_ProtNucl == kSeqTypeUnkn));
        
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
    CSeqDBIdxFile(const string & dbname, char prot_nucl, bool use_mmap)
        : CSeqDBExtFile(dbname + ".-in", prot_nucl, use_mmap),
          m_Valid         (false),
          m_NumSeqs       (0),
          m_TotLen        (0),
          m_MaxLen        (0),
          m_HdrHandle     (0),
          m_SeqHandle     (0),
          m_AmbCharHandle (0)
    {
    }
    
    virtual ~CSeqDBIdxFile()
    {
    }
    
    bool GetSeqStartEnd(Uint4 oid, Uint4 & start, Uint4 & end);
    
    bool GetHdrStartEnd(Uint4 oid, Uint4 & start, Uint4 & end);
    
    bool GetAmbStartEnd(Uint4 oid, Uint4 & start, Uint4 & end);
    
    char GetSeqType(void);

    string GetTitle(void)
    {
        if (! x_NeedIndexFile()) {
            NCBI_THROW(CSeqDBException, eFileErr, "Could not open [index] file.");
        }
        
        return m_Title;
    }
    
    string GetDate(void)
    {
        if (! x_NeedIndexFile()) {
            NCBI_THROW(CSeqDBException, eFileErr, "Could not open [index] file.");
        }
        
        return m_Date;
    }
    
    Uint4 GetNumSeqs(void)
    {
        if (! x_NeedIndexFile()) {
            NCBI_THROW(CSeqDBException, eFileErr, "Could not open [index] file.");
        }
        
        return m_NumSeqs;
    }
    
    Uint8 GetTotalLength(void)
    {
        if (! x_NeedIndexFile()) {
            NCBI_THROW(CSeqDBException, eFileErr, "Could not open [index] file.");
        }
        
        return m_TotLen;
    }
    
    Uint4 GetMaxLength(void)
    {
        if (! x_NeedIndexFile()) {
            NCBI_THROW(CSeqDBException, eFileErr, "Could not open [index] file.");
        }
        
        return m_MaxLen;
    }
    
private:
    bool x_NeedIndexFile(void);
    
    Uint4 x_GetSeqOffset(int oid)
    {
	return SeqDB_GetStdOrd( (const Uint4 *)(m_SeqHandle + (oid * 4)) );
    }
    
    Uint4 x_GetAmbCharOffset(int oid)
    {
        return SeqDB_GetStdOrd( (const Uint4 *)(m_AmbCharHandle + (oid * 4)) );
    }
    
    Uint4 x_GetHdrOffset(int oid)
    {
        return SeqDB_GetStdOrd( (const Uint4 *)(m_HdrHandle + (oid * 4)) );
    }
    
    bool m_Valid;
    
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


bool inline CSeqDBIdxFile::GetSeqStartEnd(Uint4 oid, Uint4 & start, Uint4 & end)
{
    if (! x_NeedIndexFile()) {
        NCBI_THROW(CSeqDBException, eFileErr, "Could not open [index] file.");
    }
        
    start = x_GetSeqOffset(oid);
        
    if (kSeqTypeProt == x_GetSeqType()) {
        end = x_GetSeqOffset(oid + 1);
    } else {
        end = x_GetAmbCharOffset(oid);
    }
        
    return true;
}
    
bool inline CSeqDBIdxFile::GetHdrStartEnd(Uint4 oid, Uint4 & start, Uint4 & end)
{
    if (! x_NeedIndexFile()) {
        NCBI_THROW(CSeqDBException, eFileErr, "Could not open [index] file.");
    }
        
    start = x_GetHdrOffset(oid);
    end   = x_GetHdrOffset(oid + 1);
        
    return true;
}
    
bool inline CSeqDBIdxFile::GetAmbStartEnd(Uint4 oid, Uint4 & start, Uint4 & end)
{
    if (x_NeedIndexFile() && kSeqTypeNucl == x_GetSeqType()) {
        start = x_GetAmbCharOffset(oid);
        end   = x_GetSeqOffset(oid + 1);
        
        if (start <= end) {
            return true;
        }
    }
    
    return false;
}

char inline CSeqDBIdxFile::GetSeqType(void)
{
    char ch = x_GetSeqType();
        
    if (kSeqTypeUnkn == ch) {
        x_NeedIndexFile();
        ch = x_GetSeqType();
    }
        
    return ch;
}


class CSeqDBSeqFile : public CSeqDBExtFile {
public:
    CSeqDBSeqFile(const string & dbname, char prot_nucl, bool use_mmap)
        : CSeqDBExtFile(dbname + ".-sq", prot_nucl, use_mmap),
          m_Valid      (false)
    {
    }
    
    virtual ~CSeqDBSeqFile()
    {
    }
    
    void ReadBytes(char * x, Uint4 start, Uint4 end)
    {
        if (! x_NeedSeqFile()) {
            NCBI_THROW(CSeqDBException, eFileErr, "Could not open [sequence data] file.");
        }
        
        return x_ReadBytes(x, start, end);
    }
    
    const char * GetRegion(Uint4 start, Uint4 end)
    {
        if (! x_NeedSeqFile()) {
            NCBI_THROW(CSeqDBException, eFileErr, "Could not open [sequence data] file.");
        }
        
        return x_GetRegion(start, end);
    }
    
    bool RetRegion(const char * buffer)
    {
        // Should never happen in this case..
        if (! x_NeedSeqFile()) {
            NCBI_THROW(CSeqDBException, eFileErr, "Could not open [sequence data] file.");
        }
        
        return x_RetRegion(buffer);
    }
    
private:
    bool x_NeedSeqFile(void);
    
    bool m_Valid;
};


bool inline CSeqDBSeqFile::x_NeedSeqFile(void)
{
    if (m_Valid) {
        return true;
    }
        
    if (! x_NeedFile()) {
        return false;
    }
        
    return (m_Valid = true);
}


class CSeqDBHdrFile : public CSeqDBExtFile {
public:
    CSeqDBHdrFile(const string & dbname, char prot_nucl, bool use_mmap)
        : CSeqDBExtFile(dbname + ".-hr", prot_nucl, use_mmap),
          m_Valid      (false)
    {
    }
    
    virtual ~CSeqDBHdrFile()
    {
    }
    
    void ReadBytes(char * x, Uint4 start, Uint4 end)
    {
        if (! x_NeedHdrFile()) {
            NCBI_THROW(CSeqDBException, eFileErr, "Could not open [header data] file.");
        }
        
        return x_ReadBytes(x, start, end);
    }
    
    const char * GetRegion(Uint4 start, Uint4 end)
    {
        if (! x_NeedHdrFile()) {
            NCBI_THROW(CSeqDBException, eFileErr, "Could not open [header data] file.");
        }
        
        return x_GetRegion(start, end);
    }
    
    bool RetRegion(const char * buffer)
    {
        // Should never happen in this case..
        if (! x_NeedHdrFile()) {
            NCBI_THROW(CSeqDBException, eFileErr, "Could not open [sequence data] file.");
        }
        
        return x_RetRegion(buffer);
    }
    
private:
    bool x_NeedHdrFile(void);
    
    bool m_Valid;
};


bool inline CSeqDBHdrFile::x_NeedHdrFile(void)
{
    if (m_Valid) {
        return true;
    }
        
    if (! x_NeedFile()) {
        return false;
    }
        
    return (m_Valid = true);
}


END_NCBI_SCOPE

#endif // CORELIB__SEQDB__SEQDBFILE_HPP


