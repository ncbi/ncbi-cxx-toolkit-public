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

#include "streambuf.hpp"
#include <memory>


BEGIN_NCBI_SCOPE



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionStreamProcessor
//

CCompressionStreamProcessor::CCompressionStreamProcessor(
    CCompressionProcessor*  processor,
    EDeleteProcessor        need_delete,
    streamsize              in_bufsize,
    streamsize              out_bufsize)

    : m_Processor(processor), 
      m_InBuf(0),
      m_InBufSize(in_bufsize <= 1 ? kCompressionDefaultBufSize : in_bufsize),
      m_OutBuf(0),
      m_OutBufSize(out_bufsize <= 1 ? kCompressionDefaultBufSize :out_bufsize),
      m_LastOutAvail(0),
      m_Begin(0),
      m_End(0),
      m_NeedDelete(need_delete),
      m_LastStatus(CCompressionProcessor::eStatus_Error),
      m_Finalized(0)
{
    return;
}


CCompressionStreamProcessor::~CCompressionStreamProcessor(void)
{
    if ( m_Processor  &&  m_NeedDelete ) {
        delete m_Processor;
    }
    m_Processor = 0;
}



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionStream
//

CCompressionStream::CCompressionStream(CNcbiIos&                    stream,
                                       CCompressionStreamProcessor* read_sp,
                                       CCompressionStreamProcessor* write_sp)
    : CNcbiIos(0), m_Stream(&stream), m_StreamBuf(0),
      m_Reader(read_sp), m_Writer(write_sp)
      
{
    // Create a new stream buffer
    auto_ptr<CCompressionStreambuf> sb(
        new CCompressionStreambuf(&stream, read_sp, write_sp));
    init(sb.get());
    m_StreamBuf = sb.release();
    if ( !m_StreamBuf->IsOkay() ) {
        setstate(badbit | eofbit);
    }
}


CCompressionStream::~CCompressionStream(void)
{
    // Delete stream buffer
    streambuf* sb = rdbuf();
    delete sb;
    if ( sb != m_StreamBuf ) {
        delete m_StreamBuf;
    }
#ifdef AUTOMATIC_STREAMBUF_DESTRUCTION
    rdbuf(0);
#endif
}


void CCompressionStream::Finalize(CCompressionStream::EDirection dir) 
{
    if ( m_StreamBuf ) {
        m_StreamBuf->Finalize(dir);
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/01/20 20:38:34  lavr
 * Cease using HAVE_BUGGY_IOS_CALLBACKS in this file
 *
 * Revision 1.4  2003/06/17 15:45:05  ivanov
 * Added CCompressionStreamProcessor base class. Rewritten CCompressionStream
 * to use I/O stream processors of class CCompressionStreamProcessor.
 *
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
