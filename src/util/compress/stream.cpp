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


CCompressionStream::CCompressionStream(
        CCompressionProcessor* compressor,
        ECompressorType        compressor_type,
        TCompressionUIOS*      stream,
        EStreamType            stream_type,
        streamsize             in_buf_size,
        streamsize             out_buf_size,
        EDeleteCompressor      need_delete_compressor)

    : iostream(0), m_StreamBuf(0), m_Compressor(0)
{
    // Create new stream buffer
    auto_ptr<CCompressionStreambuf> sb(
        new CCompressionStreambuf(compressor, compressor_type,
                                  stream, stream_type,
                                  in_buf_size, out_buf_size));
    init(sb.get());
    m_StreamBuf = sb.release();
    if ( !m_StreamBuf->IsOkay() ) {
        setstate(badbit | eofbit);
    }
    // Set compressor deleter
    if ( need_delete_compressor == eDelete ) {
        m_Compressor = compressor;
    }
    return;
}


CCompressionStream::~CCompressionStream(void)
{
    // Delete stream buffer
#if !defined(HAVE_IOS_XALLOC) || defined(HAVE_BUGGY_IOS_CALLBACKS)
    streambuf* sb = rdbuf();
    delete sb;
    if ( sb != m_StreamBuf )
#endif
        delete m_StreamBuf;
#ifdef AUTOMATIC_STREAMBUF_DESTRUCTION
    rdbuf(0);
#endif
    if ( m_Compressor ) {
        delete m_Compressor;
    }
}


void CCompressionStream::Finalize(void) 
{
    if ( m_StreamBuf ) {
        m_StreamBuf->Finalize();
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/06/03 20:09:16  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.2  2003/04/11 19:55:28  ivanov
 * Move streambuf.hpp from 'include/...' to 'src/...'
 *
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
