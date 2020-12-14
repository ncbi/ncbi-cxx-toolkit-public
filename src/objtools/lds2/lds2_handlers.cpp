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
    shared_ptr<CNcbiIstream> in(OpenStream(file_info, 0, NULL));
    if (!in.get()) {
        return CFormatGuess::eUnknown;
    }
    return CFormatGuess::Format(*in);
}


// Default (file) handler implementation

const string CLDS2_UrlHandler_File::s_GetHandlerName(void)
{
    return "file";
}


CLDS2_UrlHandler_File::CLDS2_UrlHandler_File(void)
    : CLDS2_UrlHandler_Base(s_GetHandlerName()),
      m_StreamCache(new CTls<TStreamCache>)
{
}


shared_ptr<CNcbiIstream>
CLDS2_UrlHandler_File::OpenStream(const SLDS2_File& file_info,
                                  Int8              stream_pos,
                                  CLDS2_Database*   /*db*/)
{
    shared_ptr<CNcbiIstream> in = OpenOrGetStream(file_info);
    if (!in.get()) {
        return nullptr;
    }
    // Chunks are not supported for regular files,
    /// offset is relative to the file start.
    in->seekg(NcbiInt8ToStreampos(stream_pos));
    return in;
}


Int8 CLDS2_UrlHandler_File::GetFileSize(const SLDS2_File& file_info)
{
    CFile f(file_info.name);
    return f.Exists() ? f.GetLength() : -1;
}


Uint4 CLDS2_UrlHandler_File::GetFileCRC(const SLDS2_File& file_info)
{
    CChecksum crc(CChecksum::eCRC32);
    crc.AddFile(file_info.name);
    return crc.GetChecksum();
}


Int8 CLDS2_UrlHandler_File::GetFileTime(const SLDS2_File& file_info)
{
    CFile f(file_info.name);
    CFile::SStat stat;
    return f.Stat(&stat) ? stat.mtime_nsec : 0;
}


CLDS2_UrlHandler_File::TStreamCache& CLDS2_UrlHandler_File::x_GetStreamCache(void)
{
    TStreamCache* cache = m_StreamCache->GetValue();
    if (!cache) {
        cache = new TStreamCache;
        m_StreamCache->SetValue(cache, CTlsBase::DefaultCleanup<TStreamCache>);
    }
    return *cache;
}


NCBI_PARAM_DECL(size_t, LDS2, MAX_CACHED_STREAMS);
NCBI_PARAM_DEF_EX(size_t, LDS2, MAX_CACHED_STREAMS, 3, eParam_NoThread,
    LDS2_MAX_CACHED_STREAMS);
typedef NCBI_PARAM_TYPE(LDS2, MAX_CACHED_STREAMS) TMaxCachedStreams;


shared_ptr<CNcbiIstream> CLDS2_UrlHandler_File::OpenOrGetStream(const SLDS2_File& file_info)
{
    size_t max_streams = TMaxCachedStreams::GetDefault();
    if (max_streams == 0) {
        // Do not use cached streams at all.
        unique_ptr<CNcbiIfstream> fin(new CNcbiIfstream(file_info.name.c_str(), ios::binary));
        if (!fin->is_open()) {
            return nullptr;
        }
        return shared_ptr<CNcbiIstream>(fin.release());
    }

    TStreamCache& cache = x_GetStreamCache();
    TStreamCache::iterator found = cache.end();
    NON_CONST_ITERATE(TStreamCache, it, cache) {
        if (it->first == file_info.name) {
            found = it;
            break;
        }
    }
    TNamedStream str;
    if (found != cache.end()) {
        str = *found;
        cache.erase(found);
        cache.emplace_front(str);
    }
    else {
        // Not yet cached
        unique_ptr<CNcbiIfstream> fin(new CNcbiIfstream(file_info.name.c_str(), ios::binary));
        if (!fin->is_open()) {
            return nullptr;
        }
        str.first = file_info.name;
        str.second.reset(fin.release());
        while (!cache.empty() && cache.size() >= max_streams) {
            cache.pop_back();
        }
        cache.emplace_front(str);
    }
    return str.second;
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
    unique_ptr<CNcbiIfstream> in(
        new CNcbiIfstream(file_info.name.c_str(), ios::binary));
    if ( !in->is_open() ) {
        return;
    }
    CGZipChunkHandler chunk_handler(file_info, db);
    g_GZip_ScanForChunks(*in, chunk_handler);
}


shared_ptr<CNcbiIstream>
CLDS2_UrlHandler_GZipFile::OpenStream(const SLDS2_File& file_info,
                                      Int8              stream_pos,
                                      CLDS2_Database*   db)
{
    shared_ptr<CNcbiIstream> in = OpenOrGetStream(file_info);
    if (!in.get()) {
        return nullptr;
    }
    bool rewind = true;
    if ( db ) {
        // Try to use chunks information to optimize loading
        SLDS2_Chunk chunk;
        if ( db->FindChunk(file_info, chunk, stream_pos) ) {
            if (chunk.raw_pos > 0) {
                in->seekg(NcbiInt8ToStreampos(chunk.raw_pos));
                rewind = false;
            }
            stream_pos -= chunk.stream_pos;
        }
    }
    if ( rewind ) {
        in->seekg(0);
    }
    unique_ptr<CCompressionIStream> zin(
        new CCompressionIStream(
        *in,
        new CZipStreamDecompressor(CZipCompression::fGZip),
        CCompressionStream::fOwnProcessor | CCompressionStream::fOwnReader));
    zin->ignore(NcbiInt8ToStreampos(stream_pos));
    return shared_ptr<CNcbiIstream>(zin.release());
}


END_SCOPE(objects)
END_NCBI_SCOPE
