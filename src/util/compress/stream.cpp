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

#include <ncbi_pch.hpp>
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
      m_InBufSize(in_bufsize <= 1 ? kCompressionDefaultBufSize : in_bufsize),
      m_OutBufSize(out_bufsize <= 1 ? kCompressionDefaultBufSize :out_bufsize),
      m_NeedDelete(need_delete), m_State(eDone)
{
    Init();
    return;
}


CCompressionStreamProcessor::~CCompressionStreamProcessor(void)
{
    if ( m_Processor  &&  m_NeedDelete == eDelete ) {
        delete m_Processor;
    }
    m_Processor = 0;
}


void CCompressionStreamProcessor::Init(void)
{
    if ( m_Processor ) {
        if ( m_State == eDone ) {
            m_Processor->Init();
        } else if (m_InBuf != 0) { // reinitializing
            m_Processor->End();    // avoid leaking memory
            m_Processor->Init();
        }
    }
    m_InBuf         = 0;
    m_OutBuf        = 0;
    m_LastOutAvail  = 0;
    m_Begin         = 0;
    m_End           = 0;
    m_LastStatus    = CCompressionProcessor::eStatus_Success;
    m_State         = eActive;
}


//////////////////////////////////////////////////////////////////////////////
//
// CCompressionStream
//

CCompressionStream::CCompressionStream(CNcbiIos&                    stream,
                                       CCompressionStreamProcessor* read_sp,
                                       CCompressionStreamProcessor* write_sp,
                                       TOwnership                   ownership)
    : CNcbiIos(0), m_Stream(&stream), m_StreamBuf(0),
      m_Reader(read_sp), m_Writer(write_sp), m_Ownership(ownership)
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
    // Delete owned objects
    if ( m_Stream   &&   m_Ownership & fOwnStream ) {
#if defined(NCBI_COMPILER_GCC)  &&  NCBI_COMPILER_VERSION < 300
        // On GCC 2.9x ios::~ios() is protected
#else
        delete m_Stream;
        m_Stream = 0;
#endif
    }
    if ( m_Reader  &&  m_Ownership & fOwnReader ) {
        if ( m_Reader == m_Writer  &&  m_Ownership & fOwnWriter ) {
            m_Writer = 0;
        }
        delete m_Reader;
        m_Reader = 0;
    }
    if ( m_Writer  &&  m_Ownership & fOwnWriter ) {
        delete m_Writer;
        m_Writer = 0;
    }
}


void CCompressionStream::Finalize(CCompressionStream::EDirection dir) 
{
    if ( m_StreamBuf ) {
        m_StreamBuf->Finalize(dir);
    }
}


CCompressionProcessor::EStatus 
CCompressionStream::x_GetStatus(CCompressionStream::EDirection dir)
{
    CCompressionStreamProcessor* sp = (dir == eRead) ? m_Reader : m_Writer;
    if ( !sp ) {
        return CCompressionProcessor::eStatus_Unknown;
    }
    return sp->m_LastStatus;
}


unsigned long CCompressionStream::x_GetProcessedSize(
                                  CCompressionStream::EDirection dir)
{
    CCompressionStreamProcessor* sp = (dir == eRead) ? m_Reader : m_Writer;
    if (!sp  ||  !sp->m_Processor) {
        return 0;
    }
    return sp->m_Processor->GetProcessedSize();
}


unsigned long CCompressionStream::x_GetOutputSize(
                                  CCompressionStream::EDirection dir)
{
    CCompressionStreamProcessor* sp = (dir == eRead) ? m_Reader : m_Writer;
    if (!sp  ||  !sp->m_Processor) {
        return 0;
    }
    return sp->m_Processor->GetOutputSize();
}


END_NCBI_SCOPE
