#ifndef BYTESRC__HPP
#define BYTESRC__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2000/11/01 20:35:27  vasilche
* Removed ECanDelete enum and related constructors.
*
* Revision 1.4  2000/07/03 18:42:32  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.3  2000/05/03 14:38:05  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.2  2000/04/28 19:14:30  vasilche
* Fixed stream position and offset typedefs.
*
* Revision 1.1  2000/04/28 16:58:00  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

class CByteSource;
class CByteSourceReader;
class CSubSourceCollector;

class CIStreamBuffer;

class CByteSource : public CObject
{
public:
    virtual ~CByteSource(void);

    virtual CRef<CByteSourceReader> Open(void) = 0;
};

class CByteSourceReader : public CObject
{
public:
    virtual ~CByteSourceReader(void);

    // read up to bufferLength bytes into buffer
    // return amount of bytes read (if zero - see EndOfData())
    virtual size_t Read(char* buffer, size_t bufferLength) = 0;

    // call this method after Read returned zero to determine whether
    // end of data reached or error occured
    virtual bool EndOfData(void) const;

    virtual CRef<CSubSourceCollector> SubSource(size_t prepend);
};

class CSubSourceCollector : public CObject
{
public:
    virtual ~CSubSourceCollector(void);

    virtual void AddChunk(const char* buffer, size_t bufferLength) = 0;
    virtual CRef<CByteSource> GetSource(void) = 0;
};

class CStreamByteSource : public CByteSource
{
public:
    CStreamByteSource(CNcbiIstream& in)
        : m_Stream(&in)
        {
        }

    CRef<CByteSourceReader> Open(void);

protected:
    CNcbiIstream* m_Stream;
};

class CFStreamByteSource : public CStreamByteSource
{
public:
    CFStreamByteSource(CNcbiIstream& in)
        : CStreamByteSource(in)
        {
        }
    CFStreamByteSource(const string& fileName, bool binary);
    ~CFStreamByteSource(void);
};

class CFileByteSource : public CByteSource
{
public:
    CFileByteSource(const string& name, bool binary);
    CFileByteSource(const CFileByteSource& file);

    CRef<CByteSourceReader> Open(void);

    const string& GetFileName(void) const
        {
            return m_FileName;
        }
    bool IsBinary(void) const
        {
            return m_Binary;
        }

private:
    string m_FileName;
    bool m_Binary;
};

class CStreamByteSourceReader : public CByteSourceReader
{
public:
    CStreamByteSourceReader(const CByteSource* source, CNcbiIstream* stream)
        : m_Source(source), m_Stream(stream)
        {
        }

    size_t Read(char* buffer, size_t bufferLength);
    bool EndOfData(void) const;

protected:
    CConstRef<CByteSource> m_Source;
    CNcbiIstream* m_Stream;
};

class CFileByteSourceReader : public CStreamByteSourceReader
{
public:
    CFileByteSourceReader(const CFileByteSource* source);

    CRef<CSubSourceCollector> SubSource(size_t prepend);
private:
    CConstRef<CFileByteSource> m_FileSource;
    CNcbiIfstream m_FStream;
};

class CMemoryChunk : public CObject {
public:
    CMemoryChunk(const char* data, size_t dataSize,
                 CRef<CMemoryChunk>& prevChunk);
    ~CMemoryChunk(void);
    
    const char* GetData(size_t offset) const
        {
            return m_Data+offset;
        }
    size_t GetDataSize(void) const
        {
            return m_DataSize;
        }
    const CRef<CMemoryChunk>& GetNextChunk(void) const
        {
            return m_NextChunk;
        }

    void SetNextChunk(const CRef<CMemoryChunk>& chunk);

private:
    char* m_Data;
    size_t m_DataSize;
    CRef<CMemoryChunk> m_NextChunk;
};

class CMemoryByteSource : public CByteSource
{
public:
    CMemoryByteSource(const CConstRef<CMemoryChunk>& bytes)
        : m_Bytes(bytes)
        {
        }
    ~CMemoryByteSource(void);

    CRef<CByteSourceReader> Open(void);

private:
    CConstRef<CMemoryChunk> m_Bytes;
};

class CMemoryByteSourceReader : public CByteSourceReader
{
public:
    CMemoryByteSourceReader(const CConstRef<CMemoryChunk>& bytes)
        : m_CurrentChunk(bytes), m_CurrentChunkOffset(0)
        {
        }
    
    size_t Read(char* buffer, size_t bufferLength);
    bool EndOfData(void) const;

private:
    size_t GetCurrentChunkAvailable(void) const;
    
    CConstRef<CMemoryChunk> m_CurrentChunk;
    size_t m_CurrentChunkOffset;
};

class CMemorySourceCollector : public CSubSourceCollector
{
public:
    virtual void AddChunk(const char* buffer, size_t bufferLength);
    virtual CRef<CByteSource> GetSource(void);

private:
    CConstRef<CMemoryChunk> m_FirstChunk;
    CRef<CMemoryChunk> m_LastChunk;
};

class CFileSourceCollector : public CSubSourceCollector
{
public:
#ifdef HAVE_NO_IOS_BASE
    typedef streampos TFilePos;
    typedef streamoff TFileOff;
#else
    typedef CNcbiIstream::pos_type TFilePos;
    typedef CNcbiIstream::off_type TFileOff;
#endif

    CFileSourceCollector(const CConstRef<CFileByteSource>& source,
                         TFilePos start);

    virtual void AddChunk(const char* buffer, size_t bufferLength);
    virtual CRef<CByteSource> GetSource(void);

private:
    CConstRef<CFileByteSource> m_FileSource;
    TFilePos m_Start;
    TFileOff m_Length;
};

class CSubFileByteSource : public CFileByteSource
{
    typedef CFileByteSource CParent;
public:
    typedef CFileSourceCollector::TFilePos TFilePos;
    typedef CFileSourceCollector::TFileOff TFileOff;
    CSubFileByteSource(const CFileByteSource& file,
                       TFilePos start, TFileOff length);

    CRef<CByteSourceReader> Open(void);

    const TFilePos& GetStart(void) const
        {
            return m_Start;
        }
    const TFileOff& GetLength(void) const
        {
            return m_Length;
        }

private:
    TFilePos m_Start;
    TFileOff m_Length;
};

class CSubFileByteSourceReader : public CFileByteSourceReader
{
    typedef CFileByteSourceReader CParent;
public:
    typedef CFileSourceCollector::TFilePos TFilePos;
    typedef CFileSourceCollector::TFileOff TFileOff;

    CSubFileByteSourceReader(const CFileByteSource* source,
                             TFilePos start, TFileOff length);

    size_t Read(char* buffer, size_t bufferLength);
    bool EndOfData(void) const;

private:
    TFileOff m_Length;
};

//#include <serial/bytesrc.inl>

END_NCBI_SCOPE

#endif  /* BYTESRC__HPP */
