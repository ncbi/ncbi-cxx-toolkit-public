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
    : m_Level(level), m_LastError(0)
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
    char* buffer = 0;
    FILE* file   = 0;
    try {
        buffer = new char[buf_size];
        if ( !buffer ) {
            throw;
        }
        file = fopen(src_file.c_str(),"rb");
        if ( !file ) {
            throw;
        }
        size_t nread, nwritten;
        while ( (nread = fread(buffer, 1, buf_size, file)) > 0 ) {
            nwritten = dst_file.Write(buffer, nread); 
            if (nwritten != nread) {
                throw;
            }
        }
        delete buffer;
        fclose(file);

    } catch (...) {
        if ( buffer ) {
            delete buffer;
        }
        if ( file ) {
            fclose(file);
        }
        return false;
    }
    return true;
}


bool CCompression::x_DecompressFile(CCompressionFile& src_file,
                                    const string&     dst_file,
                                    size_t            buf_size)
{
    if ( !buf_size ) {
        return false;
    }
    char* buffer = 0;
    FILE* file   = 0;
    try {
        buffer = new char[buf_size];
        if ( !buffer ) {
            throw;
        }
        file = fopen(dst_file.c_str(),"wb");
        if ( !file ) {
            throw;
        }
        int    nread;
        size_t nwritten;
        while ( (nread = src_file.Read(buffer, buf_size)) > 0 ) {
            nwritten = fwrite(buffer, 1, nread, file);
            if (nwritten != (size_t)nread) {
                throw;
            }
        }
        delete buffer;
        fclose(file);

    } catch (...) {
        if ( buffer ) {
            delete buffer;
        }
        if ( file ) {
            fclose(file);
        }
        return false;
    }
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionProcessor
//

CCompressionProcessor::CCompressionProcessor(void)
    : m_Busy(false)
{
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
