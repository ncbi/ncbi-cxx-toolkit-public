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
 * Author: Aleksey Grichenko
 *
 * File Description:  LDS v.2 URL handlers.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <util/checksum.hpp>
#include <util/format_guess.hpp>
#include <util/compress/compress.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>
#include <db/sqlite/sqlitewrapp.hpp>
#include <objtools/lds2/lds2_expt.hpp>
#include <objtools/lds2/lds2_handlers.hpp>


#define NCBI_USE_ERRCODE_X Objtools_LDS2

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// Base handler implementation

void CLDS2_UrlHandler_Base::FillInfo(SLDS2_File& info)
{
    info.format = GetFileFormat(info);
    info.crc = GetFileCRC(info);
    info.size = GetFileSize(info);
    info.time = GetFileTime(info);
}


SLDS2_File::TFormat
CLDS2_UrlHandler_Base::GetFileFormat(const SLDS2_File& file_info)
{
    auto_ptr<CNcbiIstream> in(OpenStream(file_info, 0, NULL));
    return CFormatGuess::Format(*in);
}


// Default (file) handler implementation

const string CLDS2_UrlHandler_File::s_GetHandlerName(void)
{
    return "file";
}


CLDS2_UrlHandler_File::CLDS2_UrlHandler_File(void)
    : CLDS2_UrlHandler_Base(s_GetHandlerName())
{
}


CNcbiIstream*
CLDS2_UrlHandler_File::OpenStream(const SLDS2_File& file_info,
                                  Int8              stream_pos,
                                  CLDS2_Database*   /*db*/)
{
    auto_ptr<CNcbiIfstream> in(
        new CNcbiIfstream(file_info.name.c_str(), ios::binary));
    // Chunks are not supported for regular files,
    /// offset is relative to the file start.
    if (stream_pos > 0) {
        in->seekg(NcbiInt8ToStreampos(stream_pos));
    }
    return in.release();
}


Int8 CLDS2_UrlHandler_File::GetFileSize(const SLDS2_File& file_info)
{
    CFile f(file_info.name);
    return f.Exists() ? f.GetLength() : -1;
}


Uint4 CLDS2_UrlHandler_File::GetFileCRC(const SLDS2_File& file_info)
{
    return ComputeFileCRC32(file_info.name);
}


Int8 CLDS2_UrlHandler_File::GetFileTime(const SLDS2_File& file_info)
{
    CFile f(file_info.name);
    CFile::SStat stat;
    return f.Stat(&stat) ? stat.mtime_nsec : 0;
}


// Default (file) handler implementation
const string CLDS2_UrlHandler_GZipFile::s_GetHandlerName(void)
{
    return "gzipfile";
}


CLDS2_UrlHandler_GZipFile::CLDS2_UrlHandler_GZipFile(void)
{
    SetHandlerName(s_GetHandlerName());
}


class CGZipChunkHandler : public IChunkHandler
{
public:
    CGZipChunkHandler(const SLDS2_File& file_info,
                      CLDS2_Database& db);
    virtual ~CGZipChunkHandler(void) {}

    virtual EAction OnChunk(TPosition raw_pos, TPosition data_pos);
private:
    const SLDS2_File& m_FileInfo;
    CLDS2_Database&   m_Db;
};


CGZipChunkHandler::CGZipChunkHandler(const SLDS2_File& file_info,
                                     CLDS2_Database& db)
    : m_FileInfo(file_info),
      m_Db(db)
{
}


CGZipChunkHandler::EAction
CGZipChunkHandler::OnChunk(TPosition raw_pos, TPosition data_pos)
{
    SLDS2_Chunk chunk(raw_pos, data_pos);
    m_Db.AddChunk(m_FileInfo, chunk);
    return eAction_Continue;
}


void CLDS2_UrlHandler_GZipFile::SaveChunks(const SLDS2_File& file_info,
                                           CLDS2_Database&   db)
{
    // Collect information about chunks, store in in the database.
    auto_ptr<CNcbiIfstream> in(
        new CNcbiIfstream(file_info.name.c_str(), ios::binary));
    if ( !in->is_open() ) {
        return;
    }
    CGZipChunkHandler chunk_handler(file_info, db);
    g_GZip_ScanForChunks(*in, chunk_handler);
}


CNcbiIstream*
CLDS2_UrlHandler_GZipFile::OpenStream(const SLDS2_File& file_info,
                                      Int8              stream_pos,
                                      CLDS2_Database*   db)
{
    auto_ptr<CNcbiIfstream> in(
        new CNcbiIfstream(file_info.name.c_str(), ios::binary));
    if ( !in->is_open() ) {
        return NULL;
    }
    if ( db ) {
        // Try to use chunks information to optimize loading
        SLDS2_Chunk chunk;
        if ( db->FindChunk(file_info, chunk, stream_pos) ) {
            if (chunk.raw_pos > 0) {
                in->seekg(NcbiInt8ToStreampos(chunk.raw_pos));
            }
            stream_pos -= chunk.stream_pos;
        }
    }
    auto_ptr<CCompressionIStream> zin(
        new CCompressionIStream(
        *in.release(),
        new CZipStreamDecompressor(CZipCompression::fGZip),
        CCompressionStream::fOwnAll));
    zin->ignore(NcbiInt8ToStreampos(stream_pos));
    return zin.release();
}


END_SCOPE(objects)
END_NCBI_SCOPE
