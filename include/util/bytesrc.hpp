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
#include <util/reader_writer.hpp>


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
    virtual void Seekg(CNcbiStreamoff pos);

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

protected:
    /// Pointer on parent (or chained) collector.
    CRef<CSubSourceCollector> m_ParentSubSource;
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


class NCBI_XUTIL_EXPORT CStreamByteSourceReader : public CByteSourceReader
{
public:
    CStreamByteSourceReader(const CByteSource* source, CNcbiIstream* stream);
    ~CStreamByteSourceReader(void);

    size_t Read(char* buffer, size_t bufferLength);
    bool EndOfData(void) const;
    bool Pushback(const char* data, size_t size);
    virtual void Seekg(CNcbiStreamoff pos);

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
    void SetNextChunk(CRef<CMemoryChunk> chunk);

private:
    char*              m_Data;
    size_t             m_DataSize;
    CRef<CMemoryChunk> m_NextChunk;
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
#ifdef HAVE_NO_IOS_BASE
    typedef streampos TFilePos;
    typedef streamoff TFileOff;
#else
    typedef CNcbiIstream::pos_type TFilePos;
    typedef CNcbiIstream::off_type TFileOff;
#endif

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


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.25  2004/08/30 18:14:23  gouriano
 * use CNcbiStreamoff instead of size_t for stream offset operations
 *
 * Revision 1.24  2004/08/04 14:32:18  vasilche
 * Fixed bug detected by MSVC 7.
 *
 * Revision 1.23  2003/12/31 20:52:17  gouriano
 * added possibility to seek (when possible) in CByteSourceReader
 *
 * Revision 1.22  2003/11/19 15:40:09  vasilche
 * Added possibility to pushback data to CByteSourceReader.
 *
 * Revision 1.21  2003/10/14 18:28:33  vasilche
 * Added full set of explicit constructors/destructors to all readers and sources.
 * Added CWriterCopyByteSourceReader for copying data from another reader object.
 *
 * Revision 1.20  2003/10/01 21:08:37  ivanov
 * Formal code rearrangement
 *
 * Revision 1.19  2003/10/01 18:45:12  kuznets
 * + CIRByteSourceReader
 *
 * Revision 1.18  2003/09/30 20:37:35  kuznets
 * Class names clean up (Removed I from CI prefix for classes based in
 * interfaces)
 *
 * Revision 1.17  2003/09/30 20:23:39  kuznets
 * +CIWriterByteSourceReader
 *
 * Revision 1.16  2003/09/30 20:07:02  kuznets
 * Added CStreamRedirectByteSourceReader class (byte source reader
 * with stream redirection into IWriter).
 *
 * Revision 1.15  2003/09/26 15:23:03  kuznets
 * Documented CSubSourceCollector and it's relations with CByteSource
 * and CByteSourceReader
 *
 * Revision 1.14  2003/09/25 16:37:47  kuznets
 * + IWriter based sub source collector
 *
 * Revision 1.13  2003/09/25 13:59:35  ucko
 * Pass C(Const)Ref by value, not reference!
 * Move CVS log to end.
 *
 * Revision 1.12  2003/09/25 12:46:28  kuznets
 * CSubSourceCollector(s) are changed so they can be chained
 * (CByteSourceReader can have more than one CSubSourceCollector)
 *
 * Revision 1.11  2003/04/17 17:50:11  siyan
 * Added doxygen support
 *
 * Revision 1.10  2002/12/19 14:51:00  dicuccio
 * Added export specifier for Win32 DLL builds.
 *
 * Revision 1.9  2001/05/17 15:01:19  lavr
 * Typos corrected
 *
 * Revision 1.8  2001/05/16 17:55:36  grichenk
 * Redesigned support for non-blocking stream read operations
 *
 * Revision 1.7  2001/05/11 20:41:14  grichenk
 * Added support for non-blocking stream reading
 *
 * Revision 1.6  2001/01/05 20:08:52  vasilche
 * Added util directory for various algorithms and utility classes.
 *
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

#endif  /* BUTIL___BYTESRC__HPP */
