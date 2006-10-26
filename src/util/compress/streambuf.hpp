#ifndef UTIL_COMPRESS__STREAMBUF__HPP
#define UTIL_COMPRESS__STREAMBUF__HPP

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
 * File Description:  CCompression based C++ streambuf
 *
 */

#include <util/compress/stream.hpp>


BEGIN_NCBI_SCOPE


/** @addtogroup CompressionStreams
 *
 * @{
 */


//////////////////////////////////////////////////////////////////////////////
//
// CCompression based streambuf class
//

class NCBI_XUTIL_EXPORT CCompressionStreambuf : public CNcbiStreambuf
{
public:
    // Constructor.
    // If read/write stream processor is 0, that read/write operation
    // will fail on this stream.
    CCompressionStreambuf(
        CNcbiIos*                    stream,
        CCompressionStreamProcessor* read_stream_processor,
        CCompressionStreamProcessor* write_stream_processor
    );
    // Destructor. Calls processor's End() function.
    virtual ~CCompressionStreambuf(void);

    // Get streambuf status
    bool IsOkay(void) const;

    // Finalize stream's compression/decompression process for read/write.
    // This function calls a processor's Finish() function.
    void Finalize(CCompressionStream::EDirection dir =
                                     CCompressionStream::eReadWrite);

protected:
    // Streambuf overloaded functions
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual CT_INT_TYPE underflow(void);
    virtual int         sync(void);
    virtual streamsize  xsputn(const CT_CHAR_TYPE* buf, streamsize count);
    virtual streamsize  xsgetn(CT_CHAR_TYPE* buf, streamsize n);

    // This method is declared here to be disabled (exception) at run-time
    virtual streambuf*  setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

protected:
    // Get compression/stream processor for specified I/O direction.
    // Return 0 (NULL) if a processor for the specified direction does not
    // installed.
    CCompressionProcessor* GetCompressionProcessor(
         CCompressionStream::EDirection dir) const;
    CCompressionStreamProcessor* GetStreamProcessor(
         CCompressionStream::EDirection dir) const;

    // Get stream processor's status
    bool IsStreamProcessorOkay(CCompressionStream::EDirection dir) const;

    // Sync I/O buffers for the specified direction.
    // Return 0 if no error; otherwise, returns -1 (EOF).
    int Sync(CCompressionStream::EDirection dir);

    // Finalize stream's compression/decompression processor.
    // Return 0 if no error; otherwise,  returns -1 (EOF).
    int Finish(CCompressionStream::EDirection dir);

    // Flush stream processor for the specified direction.
    // Return 0 if no error; otherwise, returns -1 (EOF).
    // Helper method for Sync/Finish.
    int Flush(CCompressionStream::EDirection dir);

    // Process a data from the input buffer and put result into the out.buffer
    bool Process(CCompressionStream::EDirection dir);
    bool ProcessStreamRead(void);
    bool ProcessStreamWrite(void);

    // Write data from the out buffer to the underlying stream.
    // NOTE: for writing processor only (m_Writer).
    // Helper method for ProcessStreamWrite/Sync/Finish.
    bool WriteOutBufToStream(bool force_write = false);

protected:
    CNcbiIos*     m_Stream;   // Underlying I/O stream
    CCompressionStreamProcessor* 
                  m_Reader;   // Processor's info for reading from m_Stream
    CCompressionStreamProcessor*
                  m_Writer;   // Processor's info for writing into m_Stream
    CT_CHAR_TYPE* m_Buf;      // Pointer to internal buffers
};


/* @} */


//
// Inline function
//

inline 
void CCompressionStreambuf::Finalize(CCompressionStream::EDirection dir)
{
    // Finalize read and write processors
    if (dir == CCompressionStream::eReadWrite ) {
        Finish(CCompressionStream::eRead);
        Finish(CCompressionStream::eWrite);
        return;
    }
    Finish(dir);
}

inline CCompressionProcessor*
    CCompressionStreambuf::GetCompressionProcessor(
        CCompressionStream::EDirection dir) const
{
    return dir == CCompressionStream::eRead ? m_Reader->m_Processor :
                                              m_Writer->m_Processor;
}


inline CCompressionStreamProcessor*
    CCompressionStreambuf::GetStreamProcessor(
        CCompressionStream::EDirection dir) const
{
    return dir == CCompressionStream::eRead ? m_Reader : m_Writer;
}


inline bool CCompressionStreambuf::IsOkay(void) const
{
    return !!m_Stream  &&  !!m_Buf;
}


inline bool CCompressionStreambuf::IsStreamProcessorOkay(
            CCompressionStream::EDirection dir) const
{
    CCompressionStreamProcessor* sp = GetStreamProcessor(dir);
    return IsOkay()  &&  sp  &&   sp->m_Processor  &&
           sp->m_Processor->IsBusy()  &&
           sp->m_State != CCompressionStreamProcessor::eDone;
}


inline bool CCompressionStreambuf::Process(CCompressionStream::EDirection dir)
{
    return dir == CCompressionStream::eRead ?
                  ProcessStreamRead() :  ProcessStreamWrite();
}


inline streambuf* CCompressionStreambuf::setbuf(CT_CHAR_TYPE* /* buf */,
                                                streamsize    /* buf_size */)
{
    NCBI_THROW(CCompressionException, eCompression,
               "CCompressionStreambuf::setbuf() not allowed");
    return this;  // notreached
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2006/10/26 15:34:04  ivanov
 * Added automatic finalization for input streams, if no more data
 * in the underlying stream
 *
 * Revision 1.10  2006/04/06 18:11:37  ivanov
 * Finalize() -- fixed bug with possible loss of already processed data
 * in an output streams.
 *
 * Revision 1.9  2006/01/23 13:27:19  ivanov
 * Minor formatting
 *
 * Revision 1.8  2005/06/27 13:47:30  ivanov
 * + CCompressionStreambuf::WriteOutBufToStream()
 *
 * Revision 1.7  2004/01/20 21:07:59  ucko
 * Fix typo in previous revision.
 *
 * Revision 1.6  2004/01/20 20:38:14  lavr
 * Proper Doxygen vs ncbi namespace group nesting
 *
 * Revision 1.5  2004/01/06 18:59:39  ivanov
 * Removed the extra semicolon
 *
 * Revision 1.4  2003/06/17 15:47:31  ivanov
 * The second Compression API redesign. Rewritten CCompressionStreambuf to use
 * I/O stream processors of class CCompressionStreamProcessor.
 *
 * Revision 1.3  2003/06/03 20:09:16  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.2  2003/04/15 16:51:12  ivanov
 * Fixed error with flushing the streambuf after it finalizaton
 *
 * Revision 1.1  2003/04/11 19:54:47  ivanov
 * Move streambuf.hpp from 'include/...' to 'src/...'
 *
 * Revision 1.1  2003/04/07 20:42:11  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL_COMPRESS__STREAMBUF__HPP */
