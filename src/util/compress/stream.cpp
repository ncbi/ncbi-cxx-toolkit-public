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
 * File Description:  CCompression based C++ I/O streams
 *
 */

#include <util/compress/stream.hpp>
#include <memory>
#include "streambuf.hpp"


BEGIN_NCBI_SCOPE



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionBaseStream
//


CCompressionBaseStream::CCompressionBaseStream(void)
    : m_StreamBuf(0), m_Compressor(0)
{
    return;
}


CCompressionBaseStream::~CCompressionBaseStream(void)
{
    if ( m_Compressor )
        delete m_Compressor;
}


void CCompressionBaseStream::SetCompressorForDeletion(CCompression* compressor)
{ 
    m_Compressor = compressor;
}


void CCompressionBaseStream::Finalize(void) 
{
    if ( m_StreamBuf )
        m_StreamBuf->Finalize();
}



//////////////////////////////////////////////////////////////////////////////
//
// CCompressOStream
//

CCompressOStream::CCompressOStream(CCompression*     compressor,
                                   streambuf*        out_streambuf,
                                   streamsize        in_buf_size,
                                   streamsize        out_buf_size,
                                   EDeleteCompressor need_delete_compressor)
    : ostream(0), CCompressionBaseStream()
{
    // Create new stream buffer
    auto_ptr<CCompressionOStreambuf> sb(
        new CCompressionOStreambuf(compressor, out_streambuf,
                                   in_buf_size, out_buf_size));
    init(sb.get());
    m_StreamBuf = sb.release();
    // Set compressor deleter
    if ( need_delete_compressor == eDelete ) {
        SetCompressorForDeletion(compressor);
    }
}


CCompressOStream::~CCompressOStream(void)
{
    // Delete stream buffer
    streambuf* sb = rdbuf();
    delete sb;
    if ( sb != m_StreamBuf ) {
        delete m_StreamBuf;
    }
}



//////////////////////////////////////////////////////////////////////////////
//
// CCompressIStream
//

CCompressIStream::CCompressIStream(CCompression*     compressor,
                                   streambuf*        in_stream_buf,
                                   streamsize        in_buf_size,
                                   streamsize        out_buf_size,
                                   EDeleteCompressor need_delete_compressor)
    : istream(0), CCompressionBaseStream()
{
    // Create new stream buffer
    auto_ptr<CCompressionIStreambuf> sb(
        new CCompressionIStreambuf(compressor, in_stream_buf,
                                   in_buf_size, out_buf_size));
    init(sb.get());
    m_StreamBuf = sb.release();
    // Set compressor deleter
    if ( need_delete_compressor == eDelete ) {
        SetCompressorForDeletion(compressor);
    }
}


CCompressIStream::~CCompressIStream(void)
{
    // Delete stream buffer
    streambuf* sb = rdbuf();
    delete sb;
    if ( sb != m_StreamBuf ) {
        delete m_StreamBuf;
    }
}



//////////////////////////////////////////////////////////////////////////////
//
// CDecompressOStream
//

CDecompressOStream::CDecompressOStream(CCompression*     compressor,
                                       streambuf*        out_streambuf,
                                       streamsize        in_buf_size,
                                       streamsize        out_buf_size,
                                       EDeleteCompressor need_del_compressor)
    : ostream(0), CCompressionBaseStream()
{
    // Create new stream buffer
    auto_ptr<CDecompressionOStreambuf> sb(
        new CDecompressionOStreambuf(compressor, out_streambuf,
                                     in_buf_size, out_buf_size));
    init(sb.get());
    m_StreamBuf = sb.release();
    // Set compressor deleter
    if ( need_del_compressor == eDelete ) {
        SetCompressorForDeletion(compressor);
    }
}


CDecompressOStream::~CDecompressOStream(void)
{
    // Delete stream buffer
    streambuf* sb = rdbuf();
    delete sb;
    if ( sb != m_StreamBuf ) {
        delete m_StreamBuf;
    }
}



//////////////////////////////////////////////////////////////////////////////
//
// CDecompressIStream
//

CDecompressIStream::CDecompressIStream(CCompression*     compressor,
                                       streambuf*        in_stream_buf,
                                       streamsize        in_buf_size,
                                       streamsize        out_buf_size,
                                       EDeleteCompressor need_del_compressor)
    : istream(0), CCompressionBaseStream()
{
    // Create new stream buffer
    auto_ptr<CDecompressionIStreambuf> sb(
        new CDecompressionIStreambuf(compressor, in_stream_buf,
                                     in_buf_size, out_buf_size));
    init(sb.get());
    m_StreamBuf = sb.release();
    // Set compressor deleter
    if ( need_del_compressor == eDelete ) {
        SetCompressorForDeletion(compressor);
    }
}


CDecompressIStream::~CDecompressIStream(void)
{
    // Delete stream buffer
    streambuf* sb = rdbuf();
    delete sb;
    if ( sb != m_StreamBuf ) {
        delete m_StreamBuf;
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/04/11 19:55:28  ivanov
 * Move streambuf.hpp from 'include/...' to 'src/...'
 *
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
