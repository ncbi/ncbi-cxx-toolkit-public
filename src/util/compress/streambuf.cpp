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
 * Authors:  Vladimir Ivanov
 *
 * File Description:  CCompression based C++ streambuf
 *
 */

#include <ncbi_pch.hpp>
#include "streambuf.hpp"
#include <util/compress/stream.hpp>
#include <memory>


BEGIN_NCBI_SCOPE


#define CP CCompressionProcessor


//////////////////////////////////////////////////////////////////////////////
//
// CCompressionStreambuf
//

CCompressionStreambuf::CCompressionStreambuf(
    CNcbiIos*                    stream,
    CCompressionStreamProcessor* read_sp,
    CCompressionStreamProcessor* write_sp)

    :  m_Stream(stream), m_Reader(read_sp), m_Writer(write_sp), m_Buf(0)
{
    // Check parameters
    if ( !stream  ||  
         !((read_sp   &&  read_sp->m_Processor) ||
           (write_sp  &&  write_sp->m_Processor))) {
        return;
    }

    // Get buffers sizes
    streamsize read_bufsize = 0, write_bufsize = 0;
    if ( m_Reader ) {
        read_bufsize = m_Reader->m_InBufSize + m_Reader->m_OutBufSize;
    }
    if ( m_Writer ) {
        write_bufsize = m_Writer->m_InBufSize + m_Writer->m_OutBufSize;
    }

    // Allocate memory for all buffers at one time
    auto_ptr<CT_CHAR_TYPE> bp(new CT_CHAR_TYPE[read_bufsize + write_bufsize]);
    m_Buf = bp.get();
    if ( !m_Buf ) {
        return;
    }

    // Init processors and set the buffer pointers
    if ( m_Reader ) {
        m_Reader->m_InBuf  = m_Buf;
        m_Reader->m_OutBuf = m_Buf + m_Reader->m_InBufSize;
        m_Reader->m_Begin  = m_Reader->m_InBuf;
        m_Reader->m_End    = m_Reader->m_InBuf;
        // We wish to have underflow() called at the first read
        setg(m_Reader->m_OutBuf, m_Reader->m_OutBuf, m_Reader->m_OutBuf);
        m_Reader->m_Processor->Init();
    } else {
        setg(0,0,0);
    }
    if ( m_Writer ) {
        m_Writer->m_InBuf  = m_Buf + read_bufsize;
        m_Writer->m_OutBuf = m_Writer->m_InBuf + m_Writer->m_InBufSize;
        m_Writer->m_Begin  = m_Writer->m_OutBuf;
        m_Writer->m_End    = m_Writer->m_OutBuf;
        // Use one character less for the input buffer than the really
        // available one (see overflow())
        setp(m_Writer->m_InBuf, m_Writer->m_InBuf + m_Writer->m_InBufSize - 1);
        m_Writer->m_Processor->Init();
    } else {
        setp(0,0);
    }
    bp.release();
}


CCompressionStreambuf::~CCompressionStreambuf()
{
    Finalize();
    delete[] m_Buf;
}


void CCompressionStreambuf::Finalize(CCompressionStream::EDirection dir)
{
    // Finalize read and write processors
    if (dir == CCompressionStream::eReadWrite ) {
        Finalize(CCompressionStream::eRead);
        Finalize(CCompressionStream::eWrite);
        return;
    }

    if ( !IsOkay() ) {
        return;
    }
    // Check processor status
    if ( !IsStreamProcessorOkay(dir) ) {
        return;
    }
    // Sync buffers
    Sync(dir);

    // Finish
    CCompressionStreamProcessor* sp = GetStreamProcessor(dir);
    if ( sp->m_LastStatus == CP::eStatus_EndOfData ) {
        return;
    }
    CT_CHAR_TYPE* buf = sp->m_OutBuf;
    unsigned long out_size = sp->m_OutBufSize, out_avail = 0;

    do {
        if ( dir == CCompressionStream::eRead ) {
            buf = egptr();
            out_size = sp->m_OutBuf + sp->m_OutBufSize - egptr();
        }
        sp->m_LastStatus = sp->m_Processor->Finish(buf, out_size, &out_avail);
        if ( sp->m_LastStatus == CP::eStatus_Error ) {
           break;
        }
        if ( dir == CCompressionStream::eRead ) {
            // Update the get's pointers
            setg(sp->m_OutBuf, gptr(), egptr() + out_avail);
        } else {
            // Write the data to the underlying stream
            if ( out_avail  &&
                 m_Stream->rdbuf()->sputn(sp->m_OutBuf, out_avail) !=
                 (streamsize)out_avail) {
                break;
            }
        }
    } while ( out_avail  &&  sp->m_LastStatus == CP::eStatus_Overflow);

    // Sync underlying stream buffers
    m_Stream->rdbuf()->PUBSYNC();

    // Cleanup
    sp->m_Processor->End();
    sp->m_Finalized = true;
}


int CCompressionStreambuf::sync()
{
    if ( !IsOkay() ) {
        return -1;
    }
    // Sync write processor buffers
    if ( Sync(CCompressionStream::eWrite) != 0 ) {
        return -1;
    }
    // Sync the underlying stream
    return m_Stream->rdbuf()->PUBSYNC();
}


int CCompressionStreambuf::Sync(CCompressionStream::EDirection dir)
{
    // Check processor status
    if ( !IsStreamProcessorOkay(dir) ) {
        return -1;
    }
    // Process remaining data in the preprocessing buffer
    if ( !Process(dir) ) {
        return -1;
    }

    // Flush
    CCompressionStreamProcessor* sp = GetStreamProcessor(dir);
    CT_CHAR_TYPE* buf = sp->m_OutBuf;
    unsigned long out_size = sp->m_OutBufSize, out_avail = 0;
    do {
        if ( dir == CCompressionStream::eRead ) {
            buf = egptr();
            out_size = sp->m_OutBuf + sp->m_OutBufSize - egptr();
        }
        sp->m_LastStatus = sp->m_Processor->Flush(buf, out_size, &out_avail);
        if ( sp->m_LastStatus == CP::eStatus_Error  ||
             sp->m_LastStatus == CP::eStatus_EndOfData ) {
           break;
        }
        if ( dir == CCompressionStream::eRead ) {
            // Update the get's pointers
            setg(sp->m_OutBuf, gptr(), egptr() + out_avail);
        } else {
            // Write the data to the underlying stream
            if ( out_avail  &&
                 m_Stream->rdbuf()->sputn(sp->m_OutBuf, out_avail) !=
                 (streamsize)out_avail) {
                return -1;
            }
        }
    } while ( out_avail  &&  sp->m_LastStatus == CP::eStatus_Overflow );

    // Check status
    if ( sp->m_LastStatus != CP::eStatus_Success ) {
        return -1;
    }
    return 0;
}


CT_INT_TYPE CCompressionStreambuf::overflow(CT_INT_TYPE c)
{
    // Check processor status
    if ( !IsStreamProcessorOkay(CCompressionStream::eWrite) ) {
        return CT_EOF;
    }
    if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        // Put this character in the last position
        // (this function is called when pptr() == eptr() but we
        // have reserved one byte more in the constructor, thus
        // *epptr() and now *pptr() point to valid positions)
        *pptr() = c;
        // Increment put pointer
        pbump(1);
    }
    if ( ProcessStreamWrite() ) {
        return CT_NOT_EOF(CT_EOF);
    }
    return CT_EOF;
}


CT_INT_TYPE CCompressionStreambuf::underflow(void)
{
    // Here we don't make a check for the streambuf finalization because
    // underflow() can be called after Finalize() to read a rest of
    // produced data.
    if ( !IsOkay()  ||  !m_Reader  ||  !m_Reader->m_Processor ) {
        return CT_EOF;
    }

    // Reset pointer to the processed data
    setg(m_Reader->m_OutBuf, m_Reader->m_OutBuf, m_Reader->m_OutBuf);

    // Try to process next data
    if ( !ProcessStreamRead()  ||  gptr() == egptr() ) {
        return CT_EOF;
    }
    return CT_TO_INT_TYPE(*gptr());
}


bool CCompressionStreambuf::ProcessStreamRead()
{
    unsigned long in_len, in_avail, out_size, out_avail;
    streamsize    n_read;

    // End of stream has been detected
    if ( m_Reader->m_LastStatus == CP::eStatus_EndOfData ) {
        return false;
    }
    // Put data into the compressor until there is something
    // in the output buffer
    do {
        in_avail  = 0;
        out_avail = 0;
        out_size  = m_Reader->m_OutBuf + m_Reader->m_OutBufSize - egptr();
        
        // Refill the output buffer if necessary
        if ( m_Reader->m_LastStatus != CP::eStatus_Overflow ) {
            // Refill the input buffer if necessary
            if ( m_Reader->m_Begin == m_Reader->m_End  ||
                 !m_Reader->m_LastOutAvail) {
                n_read = m_Stream->rdbuf()->sgetn(m_Reader->m_InBuf,
                                                  m_Reader->m_InBufSize);
                if ( !n_read ) {
                    // We can't read more of data
                    return false;
                }
                // Update the input buffer pointers
                m_Reader->m_Begin = m_Reader->m_InBuf;
                m_Reader->m_End   = m_Reader->m_InBuf + n_read;
            }
            // Process next data portion
            in_len = m_Reader->m_End - m_Reader->m_Begin;
            m_Reader->m_LastStatus = m_Reader->m_Processor->Process(
				m_Reader->m_Begin, in_len, egptr(), out_size,
				&in_avail, &out_avail);
            if ( m_Reader->m_LastStatus != CP::eStatus_Success  &&
                 m_Reader->m_LastStatus != CP::eStatus_EndOfData ) {
                return false;
            }
            m_Reader->m_LastOutAvail = out_avail;
        } else {
            // Get unprocessed data size
            in_len = m_Reader->m_End - m_Reader->m_Begin;
        }
        
        // Try to flush the compressor if it has not produced a data
        // via Process()
        if ( !out_avail ) { 
            m_Reader->m_LastStatus = m_Reader->m_Processor->Flush(
				egptr(), out_size, &out_avail);
            if ( m_Reader->m_LastStatus == CP::eStatus_Error ) {
                return false;
            }
            m_Reader->m_LastOutAvail = out_avail;
        }
        // Update pointer to an unprocessed data
        m_Reader->m_Begin += (in_len - in_avail);
        // Update the get's pointers
        setg(m_Reader->m_OutBuf, gptr(), egptr() + out_avail);

    } while ( !out_avail );

    return true;
}


bool CCompressionStreambuf::ProcessStreamWrite()
{
    const char*      in_buf    = pbase();
    const streamsize count     = pptr() - pbase();
    unsigned long    in_avail  = count;

    // End of stream has been detected
    if ( m_Writer->m_LastStatus == CP::eStatus_EndOfData ) {
        return false;
    }
    // Loop until no data is left
    while ( in_avail ) {
        // Process next data piece
        unsigned long out_avail = 0;
        m_Writer->m_LastStatus = m_Writer->m_Processor->Process(
            in_buf + count - in_avail, in_avail,
            m_Writer->m_OutBuf, m_Writer->m_OutBufSize,
            &in_avail, &out_avail);
        if ( m_Writer->m_LastStatus != CP::eStatus_Success  &&
             m_Writer->m_LastStatus != CP::eStatus_EndOfData ) {
           return false;
        }
        // Write the data to the underlying stream
        if ( out_avail  &&
             m_Stream->rdbuf()->sputn(m_Writer->m_OutBuf, out_avail) !=
             (streamsize)out_avail) {
            return false;
        }
    }
    // Decrease the put pointer
    pbump(-count);
    return true;
}


streamsize CCompressionStreambuf::xsputn(const CT_CHAR_TYPE* buf,
                                         streamsize count)
{
    // Check processor status
    if ( !IsStreamProcessorOkay(CCompressionStream::eWrite) ) {
        return CT_EOF;
    }
    // Check parameters
    if ( !buf  ||  count <= 0 ) {
        return 0;
    }
    // The number of chars copied
    streamsize done = 0;

    // Loop until no data is left
    while ( done < count ) {
        // Get the number of chars to write in this iteration
        // (we've got one more char than epptr thinks)
        size_t block_size = min(size_t(count-done), size_t(epptr()-pptr()+1));

        // Write them
        memcpy(pptr(), buf + done, block_size);

        // Update the write pointer
        pbump(block_size);

        // Process block if necessary
        if ( pptr() >= epptr()  &&  !ProcessStreamWrite() ) {
            break;
        }
        done += block_size;
    }
    return done;
};


streamsize CCompressionStreambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize count)
{
    // We don't doing here a check for the streambuf finalization because
    // underflow() can be called after Finalize() to read a rest of
    // produced data.
    if ( !IsOkay()  ||  !m_Reader->m_Processor ) {
        return 0;
    }
    // Check parameters
    if ( !buf  ||  count <= 0 ) {
        return 0;
    }
    // The number of chars copied
    streamsize done = 0;

    // Loop until all data are not read yet
    for (;;) {
        // Get the number of chars to write in this iteration
        size_t block_size = min(size_t(count-done), size_t(egptr()-gptr()));
        // Copy them
        if ( block_size ) {
            memcpy(buf + done, gptr(), block_size);
            done += block_size;
            // Update get pointers.
            // Satisfy "usual backup condition", see standard: 27.5.2.4.3.13
            if ( block_size == size_t(egptr() - gptr()) ) {
                *m_Reader->m_OutBuf = buf[done - 1];
                setg(m_Reader->m_OutBuf, m_Reader->m_OutBuf + 1,
                     m_Reader->m_OutBuf + 1);
            } else {
                // Update the read pointer
                gbump(block_size);
            }
        }
        // Process block if necessary
        if ( done == count  ||  !ProcessStreamRead() ) {
            break;
        }
    }
    return done;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2005/02/24 14:57:03  ucko
 * Fix typo in previous revision.
 *
 * Revision 1.14  2005/02/24 14:53:32  ivanov
 * CCompressionStreambuf::Finalize(): do not forget to sync underlying
 * stream buffers
 *
 * Revision 1.13  2004/08/11 15:22:16  vakatov
 * Compilation warning fix
 *
 * Revision 1.12  2004/06/02 16:15:46  ivanov
 * Fixed updating get pointers in the xsgetn()
 *
 * Revision 1.11  2004/05/17 21:07:25  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.10  2004/05/10 11:56:08  ivanov
 * Added gzip file format support
 *
 * Revision 1.9  2004/01/20 20:37:35  lavr
 * Fix "Okay" spelling
 *
 * Revision 1.8  2003/09/25 17:51:08  dicuccio
 * Reordered headers to avoid compilation warning on MSVC
 *
 * Revision 1.7  2003/07/15 15:52:50  ivanov
 * Added correct handling end of stream state.
 *
 * Revision 1.6  2003/06/17 15:47:31  ivanov
 * The second Compression API redesign. Rewritten CCompressionStreambuf to use
 * I/O stream processors of class CCompressionStreamProcessor.
 *
 * Revision 1.5  2003/06/04 21:11:11  ivanov
 * Get rid of non-initialized variables in the ProcessStream[Read|Write]
 *
 * Revision 1.4  2003/06/03 20:09:16  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.3  2003/04/15 16:51:12  ivanov
 * Fixed error with flushing the streambuf after it finalizaton
 *
 * Revision 1.2  2003/04/11 19:55:28  ivanov
 * Move streambuf.hpp from 'include/...' to 'src/...'
 *
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
