#ifndef UTIL_COMPRESS__STREAM__HPP
#define UTIL_COMPRESS__STREAM__HPP

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
 * Author:  Vladimir Ivanov
 *
 * File Description:  CCompression based C++ I/O streams
 *
 */

#include <util/compress/compress.hpp>


/** @addtogroup CompressionStreams
 *
 * @{
 */


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CCompression based stream classes
//
//////////////////////////////////////////////////////////////////////////////
//
// All CCompression based streams uses a "stream processors" that works as
// adapters between current stream's I/O interface and a some other stream,
// specified in the constructor. Any stream processor can implement either
// compression or decompression.

// On reading from stream, data will be read from the underlying stream,
// processed by "read stream processor", and next processed data will be
// carried to called process.
// Also, all data written into CCompression[IO]Stream stream will be processed
// using "write stream processor" and further written into underlying stream. 
// 
// Note:
//   Notice that generally the input/output stream you pass to
//   CCompression[IO]Stream class constructor must be in binary mode, because
//   the compressed data always is binary (decompressed data also can be
//   binary) and the conversions which happen in text mode will corrupt it.
//
// Note:
//   CCompression[IO]Stream class objects must be finalized after use.
//   At least one IO operation should be accomplished before finalization.
//   Only after finalization all data put into stream will be processed
//   for sure. By default finalization called in the class destructor, however
//   it can be done in any time by call Finalize(). After finalization you
//   can only read from the stream (if it is derived from istream).
//   If you don't read that some data can be lost. 
//
// Note:
//   The compression streams don't write nothing into output, if no input data
//   is provided. This can be especially important for cases where output data 
//   should have any header/footer (like .gz files for example). So, for empty
//   input, you will have empty output, that may not be acceptable to external
//   tools like gunzip and etc.
//
// Note:
//   There is one special aspect of CCompression[I]OStream class. Basically
//   the compression algorithms works on blocks of data. They waits until
//   a block is full and then compresses it. As long as you only feed data
//   to the stream without flushing it this works normally. If you flush
//   the stream, you force a premature end of the data block. This will cause
//   a worse compression factor. You should avoid flushing an output buffer
//   to get a better compression ratio.
//
//   Accordingly, the using input stream with compression usually have worse
//   compression than output stream with compression. Because a stream needs
//   to flush data from compressor very often. Increasing compressor buffer
//   size can to amend this situation.
//


// Forward declaration
class CCompressionStreamProcessor;


//////////////////////////////////////////////////////////////////////////////
//
// CCompressionStream - base stream class
//

class NCBI_XUTIL_EXPORT CCompressionStream : virtual public CNcbiIos
{
public:
    /// Stream processing direction.
    enum EDirection {
        eRead,                      ///< Reading from stream
        eWrite,                     ///< Writing into stream
        eReadWrite                  ///< eRead + eWrite
    };

    /// Which of the objects (passed in the constructor) should be
    /// deleted on this object's destruction.
    /// NOTE:  if the reader and writer are in fact one object, it will
    ///        not be deleted twice.
    enum EOwnership {
        fOwnStream = (1<<1),        ///< Delete the underlying I/O stream.
        fOwnReader = (1<<2),        ///< Delete the reader.
        fOwnWriter = (1<<3),        ///< Delete the writer.
        fOwnProcessor = fOwnReader + fOwnWriter,
        fOwnAll       = fOwnStream + fOwnProcessor
    };
    typedef int TOwnership;         ///< Bitwise OR of EOwnership.

    /// Constructor
    ///
    /// If read/write stream processor is 0 (NULL), that read/write operations
    /// on this stream will be unsuccessful.
    CCompressionStream(CNcbiIos&                    stream,
                       CCompressionStreamProcessor* read_sp,
                       CCompressionStreamProcessor* write_sp,
                       TOwnership                   ownership = 0);

    /// Destructor
    virtual ~CCompressionStream(void);

    /// Finalize stream's compression/decompression process for read/write.
    /// This function just calls a streambuf Finalize().
    virtual void Finalize(CCompressionStream::EDirection dir =
                          CCompressionStream::eReadWrite);

protected:
    /// Default constructor.
    ///
    /// Default constructor allow to create stream with specific
    /// characteristics later, not necessary in the constructor.
    /// Can be used in derived classes.
    /// @sa Create()
    CCompressionStream(void);

    /// Create stream with specific characteristics later,
    /// not necessary in the constructor. 
    /// Do nothing, if stream is already initialized.
    void Create(CNcbiIos&                    stream,
                CCompressionStreamProcessor* read_sp,
                CCompressionStreamProcessor* write_sp,
                TOwnership                   ownership = 0);

protected:
    /// Get status of last compression/decompression stream operation.
    CCompressionProcessor::EStatus x_GetStatus(CCompressionStream::EDirection dir);
    /// Get error code and description of last compressor/decompressor stream operation.
    /// Return TRUE if information obtained successfully.
    bool x_GetError(CCompressionStream::EDirection dir,
                    int& status, string& description);
    /// Return number of processed bytes.
    unsigned long x_GetProcessedSize(CCompressionStream::EDirection dir);
    /// Return number of output bytes.
    unsigned long x_GetOutputSize(CCompressionStream::EDirection dir);


protected:
    CNcbiIos*                    m_Stream;    ///< Underlying stream.
    CCompressionStreambuf*       m_StreamBuf; ///< Stream buffer.
    CCompressionStreamProcessor* m_Reader;    ///< Read processor.
    CCompressionStreamProcessor* m_Writer;    ///< Write processor.
    TOwnership                   m_Ownership; ///< Bitwise OR of EOwnership.

private:
    /// Private copy constructor to prohibit copy.
    CCompressionStream(const CCompressionStream&);
    /// Private assignment operator to prohibit assignment.
    CCompressionStream& operator= (const CCompressionStream&);
};



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionStreamProcessor class
//
// Container class for storing a stream's processor read/write info.
//

class NCBI_XUTIL_EXPORT CCompressionStreamProcessor 
{
public:
    /// If to delete the used compression processor in the destructor.
    enum EDeleteProcessor {
        eDelete,            ///< Do     delete processor object.
        eNoDelete           ///< Do not delete processor object.
    };

    /// Stream processor state.
    enum EState {
        eInit,              ///< Init() is done, but no data to process.
        eActive,            ///< Processor ready to read/write.
        eFinalize,          ///< Finalize() already done, but End() not yet.
        eDone               ///< End() done, processor cannot process data.
    };

    /// Constructor.
    CCompressionStreamProcessor(
        CCompressionProcessor* processor,
        EDeleteProcessor       need_delete  = eNoDelete,
        streamsize             in_bufsize   = kCompressionDefaultBufSize,
        streamsize             out_bufsize  = kCompressionDefaultBufSize
    );

    /// Destructor.
    virtual ~CCompressionStreamProcessor(void);

    /// (Re)Initialize stream processor.
    void Init(void);

    // Get stream processor's status
    bool IsOkay(void) const {
        return m_Processor  &&  m_Processor->IsBusy()  &&  m_State != eDone;
    }

private:
    CCompressionProcessor* m_Processor;   ///< (De)compression processor.
    CT_CHAR_TYPE*          m_InBuf;       ///< Buffer of unprocessed data.
    streamsize             m_InBufSize;   ///< Unprocessed data buffer size.
    CT_CHAR_TYPE*          m_OutBuf;      ///< Buffer of processed data.
    streamsize             m_OutBufSize;  ///< Processed data buffer size.
    CT_CHAR_TYPE*          m_Begin;       ///< Begin and end of the pre/post
    CT_CHAR_TYPE*          m_End;         ///< processed data in the buffer.
    EDeleteProcessor       m_NeedDelete;  ///< m_Processor auto-deleting flag.
    CCompressionProcessor::EStatus
                           m_LastStatus;  ///< Last compressor status.
    EState                 m_State;       ///< Stream processor state.

    // Friend classes
    friend class CCompressionStream;
    friend class CCompressionStreambuf;

private:
    /// Private copy constructor to prohibit copy.
    CCompressionStreamProcessor(const CCompressionStreamProcessor&);
    /// Private assignment operator to prohibit assignment.
    CCompressionStreamProcessor& operator= (const CCompressionStreamProcessor&);
};



//////////////////////////////////////////////////////////////////////////////
//
// I/O stream classes
//

class NCBI_XUTIL_EXPORT CCompressionIStream : public CNcbiIstream,
                                              public CCompressionStream
{
public:
    CCompressionIStream(CNcbiIos&                    stream,
                        CCompressionStreamProcessor* stream_processor,
                        TOwnership                   ownership = 0)
        : CNcbiIstream(0),
          CCompressionStream(stream, stream_processor, 0, ownership)
    {}

    /// Get status of last compression/decompression stream operation.
    CCompressionProcessor::EStatus GetStatus(void) {
        return CCompressionStream::x_GetStatus(eRead);
    }
    /// Get error code and description of last compressor/decompressor stream operation.
    /// Return TRUE if information obtained successfully.
    bool GetError(int& status, string& description) {
        return CCompressionStream::x_GetError(eRead, status, description);
    }
    /// Get total number of bytes processed by "stream_processor".
    /// This method don't count bytes cached in the internal buffers
    /// and waiting to be compressed/decompressed. Usually, only after
    /// stream finalization by Finalize() it will be equal a number of 
    /// bytes read from underlying stream.
    unsigned long GetProcessedSize(void) {
        return CCompressionStream::x_GetProcessedSize(eRead);
    };
    /// Get total number of bytes, that "stream_processor" returns.
    /// This method don't equal a number of bytes read from stream.
    /// Some data can be still cashed in the internal buffer.
    /// Usually, only after stream finalization by Finalize() it 
    /// will be equal a size of decompressed data in underlying stream.
    unsigned long GetOutputSize(void) {
        return CCompressionStream::x_GetOutputSize(eRead);
    };

protected:
    /// Default constructor.
    ///
    /// Default constructor allow to create stream with specific
    /// characteristics later, not necessary in the constructor.
    /// Can be used in derived classes.
    /// @sa CCompressionStream, Create()
    CCompressionIStream(void) : CNcbiIstream(0) { }

    /// Create stream with specific characteristics later,
    /// not necessary in the constructor. 
    /// Do nothing, if stream is already initialized.
    /// @sa CCompressionStream
    void Create(CNcbiIos&                    stream,
                CCompressionStreamProcessor* stream_processor,
                TOwnership                   ownership = 0)
    {
        CCompressionStream::Create(stream, stream_processor, 0, ownership);
    }

private:
    /// Disable operator<<(bool)
    void operator<<(bool) const;

};


class NCBI_XUTIL_EXPORT CCompressionOStream : public CNcbiOstream,
                                              public CCompressionStream
{
public:
    CCompressionOStream(CNcbiIos&                    stream,
                        CCompressionStreamProcessor* stream_processor,
                        TOwnership                   ownership = 0)
        : CNcbiOstream(0),
          CCompressionStream(stream, 0, stream_processor, ownership)
    {}

    /// Get status of last compression/decompression stream operation.
    CCompressionProcessor::EStatus GetStatus(void) {
        return CCompressionStream::x_GetStatus(eWrite);
    }
    /// Get error code and description of last compressor/decompressor stream operation.
    /// Return TRUE if information obtained successfully.
    bool GetError(int& status, string& description) {
        return CCompressionStream::x_GetError(eWrite, status, description);
    }
    /// Get total number of bytes processed by "stream_processor".
    /// This method don't count bytes cached in the internal buffers
    /// and waiting to be compressed/decompressed. Usually, only after
    /// stream finalization by Finalize() it will be equal a number of
    /// bytes written into stream.
    unsigned long GetProcessedSize(void) {
        return CCompressionStream::x_GetProcessedSize(eWrite);
    };
    /// Get total number of bytes, that "stream_processor" returns.
    /// This method don't equal a number of bytes written to underlying
    /// stream, some data can be still cashed in the internal buffer
    /// for better I/O performance. Usually, only after stream
    /// finalization by Finalize() these numvbers will be equal.
    unsigned long GetOutputSize(void) {
        return CCompressionStream::x_GetOutputSize(eWrite);
    };
    /// Finalize stream's compression/decompression process for read/write.
    /// This function just calls a streambuf Finalize().
    virtual void Finalize(CCompressionStream::EDirection dir 
        = CCompressionStream::eWrite) {
        if ( m_StreamBuf ) {
            CCompressionStream::Finalize(dir);
            flush();
        }
    };

protected:
    /// Default constructor.
    ///
    /// Default constructor allow to create stream with specific
    /// characteristics later, not necessary in the constructor.
    /// Can be used in derived classes.
    /// @sa CCompressionStream, Create()
    CCompressionOStream(void) : CNcbiOstream(0) { }

    /// Create stream with specific characteristics later,
    /// not necessary in the constructor. 
    /// Do nothing, if stream is already initialized.
    /// @sa CCompressionStream
    void Create(CNcbiIos&                    stream,
                CCompressionStreamProcessor* stream_processor,
                TOwnership                   ownership = 0)
    {
        CCompressionStream::Create(stream, 0, stream_processor, ownership);
    }

private:
    /// Disable operator>>(bool)
    void operator>>(bool) const;
};


class NCBI_XUTIL_EXPORT CCompressionIOStream : public CNcbiIostream,
                                               public CCompressionStream
{
public:
    CCompressionIOStream(CNcbiIos&                    stream,
                         CCompressionStreamProcessor* read_sp,
                         CCompressionStreamProcessor* write_sp,
                         TOwnership                   ownership = 0)
        : CNcbiIostream(0),
          CCompressionStream(stream, read_sp, write_sp, ownership)
    { }

    /// Get status of last compression/decompression stream operation.
    CCompressionProcessor::EStatus
    GetStatus(CCompressionStream::EDirection dir) {
        return CCompressionStream::x_GetStatus(dir);
    }
    /// Get error code and description of last compressor/decompressor stream operation.
    /// Return TRUE if information obtained successfully.
    bool GetError(CCompressionStream::EDirection dir,
                  int& status, string& description) {
        return CCompressionStream::x_GetError(dir, status, description);
    }
    /// Get total number of bytes processed by specified "stream_processor".
    /// @sa CCompressionIStream, CCompressionOStream
    unsigned long GetProcessedSize(CCompressionStream::EDirection dir) {
        return CCompressionStream::x_GetProcessedSize(dir);
    };
    /// Get total number of bytes, that "stream_processor" returns.
    /// @sa CCompressionIStream, CCompressionOStream
    unsigned long GetOutputSize(CCompressionStream::EDirection dir) {
        return CCompressionStream::x_GetOutputSize(dir);
    };
    /// Finalize stream's compression/decompression process for read/write.
    /// This function just calls a streambuf Finalize().
    virtual void Finalize(CCompressionStream::EDirection dir 
        = CCompressionStream::eReadWrite) {
        if ( m_StreamBuf ) {
            CCompressionStream::Finalize(dir);
            flush();
        }
    };

protected:
    /// Default constructor.
    ///
    /// Default constructor allow to create stream with specific
    /// characteristics later, not necessary in the constructor.
    /// Can be used in derived classes.
    /// @sa CCompressionStream, Create()
    CCompressionIOStream(void) : CNcbiIostream(0) { }
};


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL_COMPRESS__STREAM__HPP */
