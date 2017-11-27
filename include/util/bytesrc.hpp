#ifndef UTIL___BYTESRC__HPP
#define UTIL___BYTESRC__HPP

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
 */

/// @file bytesrc.hpp
///


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/reader_writer.hpp>


/** @addtogroup StreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CByteSource;
class CByteSourceReader;
class CSubSourceCollector;
class CIStreamBuffer;


class NCBI_XUTIL_EXPORT CByteSource : public CObject
{
public:
    CByteSource(void);
    virtual ~CByteSource(void);
    virtual CRef<CByteSourceReader> Open(void) = 0;
};


class NCBI_XUTIL_EXPORT CByteSourceReader : public CObject
{
public:
    CByteSourceReader(void);
    virtual ~CByteSourceReader(void);

    /// Read up to bufferLength bytes into buffer
    /// return amount of bytes read (if zero - see EndOfData())
    virtual size_t Read(char* buffer, size_t bufferLength) = 0;

    /// Call this method after Read returned zero to determine whether
    /// end of data reached or error occurred
    virtual bool EndOfData(void) const;

    virtual CRef<CSubSourceCollector> 
        SubSource(size_t prepend, CRef<CSubSourceCollector> parent);

    // push back some data in source, return true if successful
    virtual bool Pushback(const char* data, size_t size);

    // Set reader current position, when possible
    // (default implementation throws an exception)
    virtual void Seekg(CNcbiStreampos pos);

    virtual bool IsMultiPart(void) {
        return false;
    }
    virtual size_t GetNextPart(char** /*buffer*/, size_t /*copy_count*/) {
        return 0;
    }

private:
    CByteSourceReader(const CByteSourceReader&);
    CByteSourceReader& operator=(const CByteSourceReader&);
};


/// Abstract class for implementing "sub collectors".
///
/// Sub source collectors can accumulate data in memory, disk
/// or uany other media. This is used to temporarily 
/// store fragments of binary streams or BLOBs. 
/// Typically such collected data can be re-read by provided 
/// CByteSource interface.

class NCBI_XUTIL_EXPORT CSubSourceCollector : public CObject
{
public:
    /// Constructor.
    ///
    /// @param parent_collector
    ///   Pointer on parent(chained) collector.
    ///   CSubSourceCollector relays all AddChunk calls to the parent object
    ///   making possible having several sub-sources chained together.
    CSubSourceCollector(CRef<CSubSourceCollector> parent);

    // Destructor.
    virtual ~CSubSourceCollector(void);

    /// Add data to the sub-source. If parent pointer is
    /// set(m_ParentSubSource) call is redirected to the parent chain.
    virtual void AddChunk(const char* buffer, size_t bufferLength);

    /// Get CByteSource implementation.
    ///
    /// Calling program can try to re-read collected data using CByteSource and
    /// CByteSourceReader interfaces, though it is legal to return NULL pointer
    /// if CSubSourceCollector implementation does not support re-reading
    /// (for example if collector sends data away using network or just writes
    /// down logs to a write-only database).
    /// @sa
    ///   CByteSource, CByteSourceReader
    virtual CRef<CByteSource> GetSource(void) = 0;

    const CRef<CSubSourceCollector>& GetParentCollector(void) const
        {
            return m_ParentCollector;
        }

protected:
    /// Pointer on parent (or chained) collector.
    CRef<CSubSourceCollector> m_ParentCollector;
};


class NCBI_XUTIL_EXPORT CStreamByteSource : public CByteSource
{
public:
    CStreamByteSource(CNcbiIstream& in);
    ~CStreamByteSource(void);

    CRef<CByteSourceReader> Open(void);

protected:
    CNcbiIstream* m_Stream;
};


class NCBI_XUTIL_EXPORT CFStreamByteSource : public CStreamByteSource
{
public:
    CFStreamByteSource(CNcbiIstream& in);
    CFStreamByteSource(const string& fileName, bool binary);
    ~CFStreamByteSource(void);
};


class NCBI_XUTIL_EXPORT CFileByteSource : public CByteSource
{
public:
    CFileByteSource(const string& name, bool binary);
    CFileByteSource(const CFileByteSource& file);
    ~CFileByteSource(void);

    CRef<CByteSourceReader> Open(void);

    const string& GetFileName(void) const
        { return m_FileName; }

    bool IsBinary(void) const
        { return m_Binary; }

private:
    string m_FileName;
    bool   m_Binary;
};


class NCBI_XUTIL_EXPORT CMMapByteSource : public CByteSource
{
public:
    CMMapByteSource(const string& fileName, size_t num_blocks = 0);
    CMMapByteSource(const CMMapByteSource& other) = delete;
    virtual ~CMMapByteSource(void);

    virtual CRef<CByteSourceReader> Open(void);

protected:
    CMemoryFileMap m_FileMap;
    size_t m_CBlocks;
};


class NCBI_XUTIL_EXPORT CMMapByteSourceReader : public CByteSourceReader
{
public:
    CMMapByteSourceReader(const CByteSource* source, CMemoryFileMap* fmap, size_t num_blocks = 0);
    ~CMMapByteSourceReader(void);

    size_t Read(char* buffer, size_t bufferLength);
    bool EndOfData(void) const;
    bool Pushback(const char* data, size_t size);
    virtual void Seekg(CNcbiStreampos pos);

    virtual bool IsMultiPart(void) {
        return true;
    }
    virtual size_t GetNextPart(char** buffer, size_t copy_count);

protected:
    void x_GetNextChunkAt(size_t offset);
    CConstRef<CByteSource> m_Source;
    CMemoryFileMap*          m_Fmap;
    char* m_Ptr;
    size_t m_UnitSize, m_DefaultSize;
    size_t m_ChunkOffset, m_CurOffset, m_NextOffset, m_FileSize;
};

class NCBI_XUTIL_EXPORT CStreamByteSourceReader : public CByteSourceReader
{
public:
    CStreamByteSourceReader(const CByteSource* source, CNcbiIstream* stream);
    ~CStreamByteSourceReader(void);

    size_t Read(char* buffer, size_t bufferLength);
    bool EndOfData(void) const;
    bool Pushback(const char* data, size_t size);
    virtual void Seekg(CNcbiStreampos pos);

protected:
    CConstRef<CByteSource> m_Source;
    CNcbiIstream*          m_Stream;
};


/// Class adapter from IReader to CByteSourceReader.
///
class NCBI_XUTIL_EXPORT CIRByteSourceReader : public CByteSourceReader
{
public:
    CIRByteSourceReader(IReader* reader);
    ~CIRByteSourceReader(void);

    size_t Read(char* buffer, size_t bufferLength);
    bool EndOfData(void) const;

protected:
    IReader*   m_Reader;
    bool       m_EOF;
};


/// Stream based byte source reader.
/// 
/// Class works as a virtual class factory to create CWriterSourceCollector.
///
/// One of the projected uses is to update local BLOB cache.
/// @sa
///   SubSource().


class NCBI_XUTIL_EXPORT CWriterByteSourceReader 
    : public CStreamByteSourceReader
{
public:
    /// Constructor.
    ///
    /// @param stream
    ///   Readers source.
    /// @param writer
    ///   Destination interface pointer.
    CWriterByteSourceReader(CNcbiIstream* stream, IWriter* writer);
    ~CWriterByteSourceReader(void);

    /// Create CWriterSourceCollector.
    virtual CRef<CSubSourceCollector> 
        SubSource(size_t prepend, CRef<CSubSourceCollector> parent);

protected:
    IWriter* m_Writer;
};


class NCBI_XUTIL_EXPORT CWriterCopyByteSourceReader 
    : public CByteSourceReader
{
public:
    /// Constructor.
    ///
    /// @param reader
    ///   Source reader.
    /// @param writer
    ///   Destination interface pointer.
    CWriterCopyByteSourceReader(CByteSourceReader* reader, IWriter* writer);
    ~CWriterCopyByteSourceReader(void);

    /// Just call Read method on source reader.
    size_t Read(char* buffer, size_t bufferLength);

    /// Just call EndOfData method on source reader.
    bool EndOfData(void) const;

    /// Create CWriterSourceCollector.
    virtual CRef<CSubSourceCollector> 
        SubSource(size_t prepend, CRef<CSubSourceCollector> parent);

protected:
    CRef<CByteSourceReader> m_Reader;
    IWriter* m_Writer;
};


class NCBI_XUTIL_EXPORT CFileByteSourceReader : public CStreamByteSourceReader
{
public:
    CFileByteSourceReader(const CFileByteSource* source);
    ~CFileByteSourceReader(void);
   
    CRef<CSubSourceCollector> SubSource(size_t prepend, 
                                        CRef<CSubSourceCollector> parent);
private:
    CConstRef<CFileByteSource> m_FileSource;
    CNcbiIfstream              m_FStream;
};


class NCBI_XUTIL_EXPORT CMemoryChunk : public CObject 
{
public:
    CMemoryChunk(const char* data, size_t dataSize,
                 CRef<CMemoryChunk> prevChunk);
    ~CMemoryChunk(void);
    
    const char* GetData(size_t offset) const
        { return m_Data+offset; }
    size_t GetDataSize(void) const
        { return m_DataSize; }
    CRef<CMemoryChunk> GetNextChunk(void) const
        { return m_NextChunk; }

private:
    char*              m_Data;
    size_t             m_DataSize;
    CRef<CMemoryChunk> m_NextChunk;

private:
    CMemoryChunk(const CMemoryChunk&);
    void operator=(const CMemoryChunk&);
};


class NCBI_XUTIL_EXPORT CMemoryByteSource : public CByteSource
{
public:
    CMemoryByteSource(CConstRef<CMemoryChunk> bytes);
    ~CMemoryByteSource(void);

    CRef<CByteSourceReader> Open(void);

private:
    CConstRef<CMemoryChunk> m_Bytes;
};


class NCBI_XUTIL_EXPORT CMemoryByteSourceReader : public CByteSourceReader
{
public:
    CMemoryByteSourceReader(CConstRef<CMemoryChunk> bytes);
    ~CMemoryByteSourceReader(void);
    
    size_t Read(char* buffer, size_t bufferLength);
    bool EndOfData(void) const;

private:
    size_t GetCurrentChunkAvailable(void) const
        {
            return m_CurrentChunk->GetDataSize() - m_CurrentChunkOffset;
        }

private:
    CConstRef<CMemoryChunk> m_CurrentChunk;
    size_t                  m_CurrentChunkOffset;
};


class NCBI_XUTIL_EXPORT CMemorySourceCollector : public CSubSourceCollector
{
public:
    CMemorySourceCollector(CRef<CSubSourceCollector>
                           parent = CRef<CSubSourceCollector>());
    ~CMemorySourceCollector(void);

    virtual void AddChunk(const char* buffer, size_t bufferLength);
    virtual CRef<CByteSource> GetSource(void);

private:
    CConstRef<CMemoryChunk> m_FirstChunk;
    CRef<CMemoryChunk>      m_LastChunk;
};


/// Class adapter IWriter - CSubSourceCollector.
class NCBI_XUTIL_EXPORT CWriterSourceCollector : public CSubSourceCollector
{
public:
    /// Constructor.
    ///
    /// @param writer
    ///   Pointer on adapted IWriter interface.
    /// @param own
    ///   Flag to take ownership on the writer (delete on destruction).
    /// @param parent
    ///   Chained sub-source.
    CWriterSourceCollector(IWriter*                  writer, 
                           EOwnership                own, 
                           CRef<CSubSourceCollector> parent);
    virtual ~CWriterSourceCollector();

    /// Reset the destination IWriter interface.
    ///
    /// @param writer
    ///   Pointer on adapted IWriter interface.
    /// @param own
    ///   Flag to take ownership on the writer (delete on destruction).
    void SetWriter(IWriter* writer, EOwnership own);

    virtual void AddChunk(const char* buffer, size_t bufferLength);

    /// Return pointer on "reader" interface. In this implementation
    /// returns NULL, since IWriter is a one way (write only interface).
    virtual CRef<CByteSource> GetSource(void);

private:
    IWriter*    m_Writer; ///< Destination interface pointer.
    EOwnership  m_Own;    ///< Flag to delete IWriter on destruction.
};


class NCBI_XUTIL_EXPORT CFileSourceCollector : public CSubSourceCollector
{
public:
    typedef CNcbiIstream::pos_type TFilePos;
    typedef CNcbiIstream::off_type TFileOff;

    CFileSourceCollector(CConstRef<CFileByteSource> source,
                         TFilePos                   start,
                         CRef<CSubSourceCollector>  parent);
    ~CFileSourceCollector(void);

    virtual void AddChunk(const char* buffer, size_t bufferLength);
    virtual CRef<CByteSource> GetSource(void);

private:
    CConstRef<CFileByteSource> m_FileSource;
    TFilePos                   m_Start;
    TFileOff                   m_Length;
};


class NCBI_XUTIL_EXPORT CSubFileByteSource : public CFileByteSource
{
    typedef CFileByteSource CParent;
public:
    typedef CFileSourceCollector::TFilePos TFilePos;
    typedef CFileSourceCollector::TFileOff TFileOff;
    CSubFileByteSource(const CFileByteSource& file,
                       TFilePos start, TFileOff length);
    ~CSubFileByteSource(void);

    CRef<CByteSourceReader> Open(void);

    const TFilePos& GetStart(void) const
        { return m_Start; }
    const TFileOff& GetLength(void) const
        { return m_Length; }

private:
    TFilePos m_Start;
    TFileOff m_Length;
};


class NCBI_XUTIL_EXPORT CSubFileByteSourceReader : public CFileByteSourceReader
{
    typedef CFileByteSourceReader CParent;
public:
    typedef CFileSourceCollector::TFilePos TFilePos;
    typedef CFileSourceCollector::TFileOff TFileOff;

    CSubFileByteSourceReader(const CFileByteSource* source,
                             TFilePos start, TFileOff length);
    ~CSubFileByteSourceReader(void);

    size_t Read(char* buffer, size_t bufferLength);
    bool EndOfData(void) const;
    
private:
    TFileOff m_Length;
};


/* @} */


END_NCBI_SCOPE

#endif  /* BUTIL___BYTESRC__HPP */
