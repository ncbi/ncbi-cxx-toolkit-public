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

#include <ncbi_pch.hpp>
#include <util/compress/compress.hpp>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CCompression
//

CCompression::CCompression(ELevel level)
    : m_Level(level), m_ErrorCode(0), m_ErrorMsg(kEmptyStr), m_Flags(0)
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


bool CCompression::x_CompressFile(const string&     src_file,
                                  CCompressionFile& dst_file,
                                  size_t            buf_size)
{
    if ( !buf_size ) {
        return false;
    }
    char* buffer;
    CNcbiIfstream is(src_file.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( !is.good() ) {
        return false;
    }
    buffer = new char[buf_size];
    if ( !buffer ) {
        return false;
    }
    while ( is ) {
        is.read(buffer, buf_size);
        size_t nread = is.gcount();
        size_t nwritten = dst_file.Write(buffer, nread); 
        if ( nwritten != nread ) {
            delete buffer;
            return false;
        }
    }
    delete buffer;
    return true;
}


bool CCompression::x_DecompressFile(CCompressionFile& src_file,
                                    const string&     dst_file,
                                    size_t            buf_size)
{
    if ( !buf_size ) {
        return false;
    }
    char* buffer;
    CNcbiOfstream os(dst_file.c_str(), IOS_BASE::out | IOS_BASE::binary);
    if ( !os.good() ) {
        return false;
    }
    buffer = new char[buf_size];
    if ( !buffer ) {
        return false;
    }
    size_t nread;
    while ( (nread = src_file.Read(buffer, buf_size)) > 0 ) {
        os.write(buffer, nread);
        if ( !os.good() ) {
            delete buffer;
            return false;
        }
    }
    delete buffer;
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionProcessor
//

CCompressionProcessor::CCompressionProcessor(void)
{
    Reset();
    return;
}


CCompressionProcessor::~CCompressionProcessor(void)
{
    return;
}


//////////////////////////////////////////////////////////////////////////////
//
// CCompressionFile
//

CCompressionFile::CCompressionFile(void)
    : m_File(0), m_Mode(eMode_Read)
{
    return;
}


CCompressionFile::~CCompressionFile(void)
{
    return;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2005/02/01 21:47:15  grichenk
 * Fixed warnings
 *
 * Revision 1.8  2004/08/04 18:42:45  ivanov
 * Use binary mode to open file streams in the x_[De]compressFile.
 *
 * Revision 1.7  2004/05/17 21:07:25  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.6  2004/05/10 11:56:08  ivanov
 * Added gzip file format support
 *
 * Revision 1.5  2003/07/15 15:50:50  ivanov
 * Improved error diagnostics
 *
 * Revision 1.4  2003/07/10 19:56:29  ivanov
 * Using classes CNcbi[I/O]fstream instead FILE* streams
 *
 * Revision 1.3  2003/07/10 16:24:26  ivanov
 * Added auxiliary file compression/decompression functions.
 *
 * Revision 1.2  2003/06/03 20:09:16  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
