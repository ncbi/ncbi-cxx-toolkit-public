/* $Id$
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

#include <ncbi_pch.hpp>
#include "streambuf.hpp"
#include <util/compress/stream.hpp>
#include <util/error_codes.hpp>
#include <memory>


#define NCBI_USE_ERRCODE_X   Util_Compress


BEGIN_NCBI_SCOPE

// Abbreviation for long names
#define CP  CCompressionProcessor
#define CSP CCompressionStreamProcessor

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
    AutoArray<CT_CHAR_TYPE> bp(new CT_CHAR_TYPE[size_t(read_bufsize + write_bufsize)]);
    m_Buf = bp.get();
    if ( !m_Buf ) {
        return;
    }
    // Init processors and set the buffer pointers
    if ( m_Reader ) {
        m_Reader->Init();
        m_Reader->m_InBuf  = m_Buf;
        m_Reader->m_OutBuf = m_Buf + m_Reader->m_InBufSize;
        m_Reader->m_Begin  = m_Reader->m_InBuf;
        m_Reader->m_End    = m_Reader->m_InBuf;
        // We wish to have underflow() called at the first read
        setg(m_Reader->m_OutBuf, m_Reader->m_OutBuf, m_Reader->m_OutBuf);
    } else {
        setg(0,0,0);
    }
    if ( m_Writer ) {
        m_Writer->Init();
        m_Writer->m_InBuf  = m_Buf + read_bufsize;
        m_Writer->m_OutBuf = m_Writer->m_InBuf + m_Writer->m_InBufSize;
        m_Writer->m_Begin  = m_Writer->m_OutBuf;
        m_Writer->m_End    = m_Writer->m_OutBuf;
        // Use one character less for the input buffer than the really
        // available one (see overflow())
        setp(m_Writer->m_InBuf, m_Writer->m_InBuf + m_Writer->m_InBufSize - 1);
    } else {
        setp(0,0);
    }
    bp.release();
}


CCompressionStreambuf::~CCompressionStreambuf()
{
    // Finalize processors

    CSP* sp;
    #define msg_where    "CCompressionStreambuf::~CCompressionStreambuf: "
    #define msg_overflow "Overflow occurred, lost some processed data " \
                         "through call Finalize()"
    #define msg_error    "Finalize() failed"

    try {
        // Read processor
        sp = GetStreamProcessor(CCompressionStream::eRead);
        if ( sp ) {
            bool abandon = IsStreamProcessorHaveData(CCompressionStream::eRead);
            sp->m_Processor->End(abandon);
            sp->m_State = CSP::eDone;
        }
        // Write processor
        sp = GetStreamProcessor(CCompressionStream::eWrite);
        if ( sp ) {
            if ( sp->m_State == CSP::eInit  ||
                 sp->m_State == CSP::eActive ) {
                Finalize(CCompressionStream::eWrite);
                if ( sp->m_LastStatus == CP::eStatus_Overflow ) {
                    ERR_COMPRESS(72, msg_where << msg_overflow);
                }
                if ( sp->m_LastStatus == CP::eStatus_Error ) {
                    ERR_COMPRESS(73, msg_where << msg_error);
                }
            }
            if (IsStreamProcessorHaveData(CCompressionStream::eWrite)) {
                sp->m_Processor->End(); 
                sp->m_State = CSP::eDone;
                // Write remaining data from buffers to underlying stream
                WriteOutBufToStream(true /*force write*/);
            } else {
                sp->m_Processor->End(1 /*abandon state*/); 
                sp->m_State = CSP::eDone;
            }
        }
    }
    COMPRESS_HANDLE_EXCEPTIONS(86, msg_where);

    // Delete buffers
    delete[] m_Buf;
}


int CCompressionStreambuf::sync()
{
    if ( !IsOkay() ) {
        return -1;
    }
    int status = 0;
    // Sync write processor buffers
    CSP* sp = GetStreamProcessor(CCompressionStream::eWrite);
    if ( sp  &&  
         sp->m_State != CSP::eDone  &&
         !(sp->m_State == CSP::eFinalize  &&  sp->m_LastStatus == CP::eStatus_EndOfData)
        ) {
        if ( Sync(CCompressionStream::eWrite) != 0 ) {
            status = -1;
        }
    }
    // Sync the underlying stream
    status += m_Stream->rdbuf()->PUBSYNC();
    return (status < 0 ? -1 : 0);
}


int CCompressionStreambuf::Sync(CCompressionStream::EDirection dir)
{
    // Check processor's state
    if ( !IsStreamProcessorOkay(dir) ) {
        return -1;
    }
    CSP* sp = GetStreamProcessor(dir);
    // Check processor's status
    if ( sp->m_LastStatus == CP::eStatus_Error ) {
        return -1;
    }
    // Check that we have some data to process, before calling 
    // any compression/decompression methods, or we can get 
    // a garbage on the output.
    if ( !IsStreamProcessorHaveData(dir)  &&  sp->m_LastStatus != CP::eStatus_Overflow ) {
        return 0;
    }
    // Process remaining data in the preprocessing buffer
    if ( !Process(dir) ) {
        return -1;
    }
    // Flush
    return Flush(dir);
}


int CCompressionStreambuf::Finish(CCompressionStream::EDirection dir)
{
    // Check processor's state
    if ( !IsStreamProcessorOkay(dir) ) {
        return -1;
    }
    CSP* sp = GetStreamProcessor(dir);
    if ( sp->m_LastStatus == CP::eStatus_Error ) {
        return -1;
    }
    if ( sp->m_State == CSP::eFinalize ) {
        // Already finalized
        return 0;
    }
    // Check that we have some data to process, before calling 
    // any compression/decompression methods, or we can get 
    // a garbage in the output.
    if ( IsStreamProcessorHaveData(dir) ) {
        // Process remaining data in the preprocessing buffer
        Process(dir);
        if ( sp->m_LastStatus == CP::eStatus_Error ) {
            return -1;
        }
    }
    // Finish. Change state to 'finalized'.
    sp->m_State = CSP::eFinalize;
    return Flush(dir);
}


int CCompressionStreambuf::Flush(CCompressionStream::EDirection dir)
{
    CSP* sp = GetStreamProcessor(dir);

    // Check processor's status
    if ( sp->m_LastStatus == CP::eStatus_Error ) {
        return -1;
    }
    if ( sp->m_LastStatus == CP::eStatus_EndOfData ) {
        // Flush underlying stream (on write)
        if (dir == CCompressionStream::eWrite  &&  
            !WriteOutBufToStream(true /*force write*/)) {
            return -1;
        }
        // End of data, nothing to do
        return 0;
    }

    // Flush stream compressor
    CT_CHAR_TYPE* buf = 0;
    size_t out_size = 0, out_avail = 0;
    do {
        // Get pointer to the free space in the buffer
        if ( dir == CCompressionStream::eRead ) {
            buf = egptr();
        } else {
            buf = sp->m_End;
        }
        out_size = sp->m_OutBuf + sp->m_OutBufSize - buf;

        // Get data from processor
        out_avail = 0;
        if ( sp->m_State == CSP::eFinalize ) {
            // State is eFinalize
            sp->m_LastStatus = 
                sp->m_Processor->Finish(buf, out_size, &out_avail);
        } else {
            // State is eActive
            _VERIFY(sp->m_State == CSP::eActive);
            sp->m_LastStatus = 
                sp->m_Processor->Flush(buf, out_size, &out_avail);
            // No more data -- automatically finalize stream
            if ( sp->m_LastStatus == CP::eStatus_EndOfData ) {
                sp->m_State = CSP::eFinalize;
            }
        } 
        // Check on error
        if ( sp->m_LastStatus == CP::eStatus_Error ) {
            return -1;
        }
        if ( dir == CCompressionStream::eRead ) {
            // Update the get's pointers
            setg(sp->m_OutBuf, gptr(), egptr() + out_avail);
        } else { // CCompressionStream::eWrite
            // Update the output buffer pointer
            sp->m_End += out_avail;
            // Write data to the underlying stream only if the output buffer
            // is full or an overflow/endofdata occurs.
            if ( !WriteOutBufToStream() ) {
                return -1;
            }
        }
    } while (sp->m_LastStatus == CP::eStatus_Repeat  ||
            (out_avail  &&  (sp->m_LastStatus == CP::eStatus_Success || 
                             sp->m_LastStatus == CP::eStatus_Overflow))
            );

    // Flush underlying stream (on write)
    if (dir == CCompressionStream::eWrite) {
        if ( sp->m_LastStatus == CP::eStatus_EndOfData  ||
             sp->m_State == CSP::eFinalize) {
            if ( !WriteOutBufToStream(true /*force write*/) ) {
                return -1;
            }
        }
    }
    return 0;
}


CT_INT_TYPE CCompressionStreambuf::overflow(CT_INT_TYPE c)
{
    // Check processor's state
    if ( !IsStreamProcessorOkay(CCompressionStream::eWrite) ) {
        return CT_EOF;
    }
    if ( m_Writer->m_State == CSP::eFinalize ) {
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
    // Check processor's state
    if ( !IsStreamProcessorOkay(CCompressionStream::eRead) ) {
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
    size_t     in_len, in_avail, out_size, out_avail;
    streamsize n_read;

    // End of stream has been detected
    if ( m_Reader->m_LastStatus == CP::eStatus_EndOfData ) {
        return false;
    }

    // Flush remaining data from compression stream if it has finalized
    if ( m_Reader->m_State == CSP::eFinalize ) {
        return Flush(CCompressionStream::eRead) == 0;
    }

    // Put data into the (de)compressor until there is something
    // in the output buffer
    do {
        in_avail  = 0;
        out_avail = 0;
        out_size  = m_Reader->m_OutBuf + m_Reader->m_OutBufSize - egptr();

        // Refill the output buffer if necessary
        if ( m_Reader->m_LastStatus != CP::eStatus_Overflow ) {

            // Refill the input buffer if necessary
            if ( m_Reader->m_Begin == m_Reader->m_End ) {
                n_read = m_Stream->rdbuf()->sgetn(m_Reader->m_InBuf,
                                                  m_Reader->m_InBufSize);
#ifdef NCBI_COMPILER_WORKSHOP
                if (n_read < 0) {
                    n_read = 0; // WS6 is known to return -1 from sgetn() :-/
                }
#endif //NCBI_COMPILER_WORKSHOP
                if ( !n_read ) {
                    // We can't read more of data.
                    // Automatically 'finalize' (de)compressor.
                    m_Reader->m_State = CSP::eFinalize;
                    return Flush(CCompressionStream::eRead) == 0;
                }
                if ( m_Reader->m_State == CSP::eInit ) {
                    m_Reader->m_State = CSP::eActive;
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
        } else {
            // Check available space in the output buffer
            if ( !out_size ) {
                return false;
            }
            // Get unprocessed data size
            in_len = m_Reader->m_End - m_Reader->m_Begin;
            in_avail = in_len;
            m_Reader->m_LastStatus = 
                m_Reader->m_Processor->Flush(egptr(), out_size, &out_avail);
        }
        if ( m_Reader->m_LastStatus == CP::eStatus_Error ) {
            return false;
        }
        // No more data -- automatically finalize stream
        if ( m_Reader->m_LastStatus == CP::eStatus_EndOfData ) {
            m_Reader->m_State = CSP::eFinalize;
        }

        // Update pointer to an unprocessed data
        m_Reader->m_Begin += (in_len - in_avail);
        // Update the get's pointers
        setg(m_Reader->m_OutBuf, gptr(), egptr() + out_avail);

        if ( m_Reader->m_LastStatus == CP::eStatus_EndOfData   &&  !out_avail ) { 
            return false;
        }

    } while ( !out_avail );

    return true;
}


bool CCompressionStreambuf::ProcessStreamWrite()
{
    const char*  in_buf    = pbase();
    const size_t count     = pptr() - pbase();
    size_t       in_avail  = count;

    // Nothing was written into processor yet
    if ( m_Writer->m_State == CSP::eInit ) {
        if ( !count )
            return false;
        // Reset state to eActive
        m_Writer->m_State = CSP::eActive;
    }
    // End of stream has been detected
    if ( m_Writer->m_LastStatus == CP::eStatus_EndOfData ) {
        return false;
    }
    // Flush remaining data from compression stream if it is finalized
    if ( m_Writer->m_State == CSP::eFinalize ) {
        return Flush(CCompressionStream::eWrite) == 0;
    }

    // Loop until no data is left
    while ( in_avail ) {
        // Process next data portion
        size_t out_avail = 0;
        size_t out_size = m_Writer->m_OutBuf + 
                          m_Writer->m_OutBufSize - m_Writer->m_End;
        m_Writer->m_LastStatus = m_Writer->m_Processor->Process(
            in_buf + count - in_avail, in_avail, m_Writer->m_End, out_size,
            &in_avail, &out_avail);

        // Check on error / small output buffer
        if ( m_Writer->m_LastStatus == CP::eStatus_Error ) {
            return false;
        }
        // No more data -- automatically finalize stream
        if ( m_Writer->m_LastStatus == CP::eStatus_EndOfData ) {
            m_Writer->m_State = CSP::eFinalize;
        }
        // Update the output buffer pointer
        m_Writer->m_End += out_avail;

        // Write data to the underlying stream only if the output buffer
        // is full or an overflow occurs.
        if ( !WriteOutBufToStream() ) {
            return false;
        }
    }
    // Decrease the put pointer
    pbump(-(int)count);
    return true;
}


bool CCompressionStreambuf::WriteOutBufToStream(bool force_write)
{
    // Write data from out buffer to the underlying stream only if the buffer
    // is full or an overflow/endofdata occurs, or 'force_write' is TRUE.
    if ( force_write  || 
         (m_Writer->m_End == m_Writer->m_OutBuf + m_Writer->m_OutBufSize)  ||
         m_Writer->m_LastStatus == CP::eStatus_Overflow  ||
         m_Writer->m_LastStatus == CP::eStatus_EndOfData ) {

        streamsize to_write = m_Writer->m_End - m_Writer->m_Begin;
        if ( to_write ) {
            if ( !m_Stream->good() ) {
                return false;
            }
            streamsize n_write = m_Stream->rdbuf()->sputn(m_Writer->m_Begin, to_write);
            if ( n_write != to_write ) {
                m_Writer->m_Begin += n_write;
                return false;
            }
            // Update the output buffer pointers
            m_Writer->m_Begin = m_Writer->m_OutBuf;
            m_Writer->m_End   = m_Writer->m_OutBuf;
        }
    }
    return true;
}


streamsize CCompressionStreambuf::xsputn(const CT_CHAR_TYPE* buf,
                                         streamsize count)
{
    // Check processor's state
    if ( !IsStreamProcessorOkay(CCompressionStream::eWrite) ) {
        return CT_EOF;
    }
    if ( m_Writer->m_State == CSP::eFinalize ) {
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
        pbump((int)block_size);
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
                gbump((int)block_size);
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
