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
 * File Description:  The Compression API
 *
 */

#include <util/compress/compress.hpp>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CCompression
//


CCompression::CCompression(ELevel level)
    : m_Level(level), m_Busy(0)
{
    return;
}


CCompression::~CCompression(void)
{
    return;
}


CCompression::ELevel CCompression::GetLevel(void) const
{
    if ( m_Level == eLevel_Default) {
        return GetDefaultLevel();
    }
    return m_Level;
}


void CCompression::SetLevel(ELevel level)
{
    m_Level = level;
}


int CCompression::GetLastError(void) const
{
    return m_LastError;
}


void CCompression::SetLastError(int errcode)
{
    m_LastError = errcode;
}


bool CCompression::IsBusy(void)
{
    return m_Busy;
}


void CCompression::SetBusy(bool busy)
{
    if ( busy  &&  m_Busy ) {
        NCBI_THROW(CCompressionException, eBusy, "The compressor is busy now");
    }
    m_Busy = busy;
}


bool CCompression::CompressBuffer(const char* src_buf, unsigned long  src_len,
                                  char*       dst_buf, unsigned long  dst_size,
                                  /* out */            unsigned long* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        return false;
    }
    unsigned long in_avail  = 0;
    unsigned long out_avail = 0;

    // Init
    EStatus status = DeflateInit();
    if ( status != eStatus_Success ) {
        goto errhandler;
    }
    // Compress
    status = Deflate(src_buf, src_len, dst_buf, dst_size,
                     &in_avail, &out_avail);
    *dst_len = out_avail;
    if ( status != eStatus_Success  ||  in_avail != 0 ) {
        goto errhandler;
    }
    // Finishing
    status = DeflateFinish(dst_buf + out_avail, dst_size - out_avail,
                           &out_avail);
    *dst_len += out_avail;
    if ( status != eStatus_Success ) {
        goto errhandler;
    }
    // Termination
errhandler:
    DeflateEnd();
    return status == eStatus_Success;
}


bool CCompression::DecompressBuffer(
                   const char* src_buf, unsigned long  src_len,
                   char*       dst_buf, unsigned long  dst_size,
                   /* out */   unsigned long* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        return false;
    }
    unsigned long in_avail  = 0;
    unsigned long out_avail = 0;

    // Init
    EStatus status = InflateInit();
    if ( status != eStatus_Success ) {
        goto errhandler;
    }
    // Decompress
    status = Inflate(src_buf, src_len, dst_buf, dst_size,
                     &in_avail, &out_avail);
    *dst_len = out_avail;
    if ( in_avail != 0 ) {
        status = eStatus_Error;
    }
    // Termination
errhandler:
    InflateEnd();
    return status == eStatus_Success;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
