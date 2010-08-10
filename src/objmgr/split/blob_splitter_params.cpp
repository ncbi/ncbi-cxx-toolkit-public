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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objmgr/split/blob_splitter_params.hpp>
#include <objmgr/split/id2_compress.hpp>
#include <objmgr/split/split_exceptions.hpp>
#include <util/compress/zlib.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


#define DISABLE_SPLIT_DESCRIPTIONS false
#define DISABLE_SPLIT_ASSEMBLY     true


SSplitterParams::SSplitterParams(void)
    : m_MinChunkCount(kDefaultMinChunkCount),
      m_Compression(eCompression_none),
      m_Verbose(0),
      m_DisableSplitDescriptions(DISABLE_SPLIT_DESCRIPTIONS),
      m_DisableSplitSequence(false),
      m_DisableSplitAnnotations(false),
      m_DisableSplitAssembly(DISABLE_SPLIT_ASSEMBLY),
      m_JoinSmallChunks(false),
      m_SplitWholeBioseqs(true),
      m_SplitNonFeatureSeqTables(kDefaultSplitNonFeatureSeqTables)
{
    SetChunkSize(kDefaultChunkSize);
}


void SSplitterParams::SetChunkSize(size_t size)
{
    m_ChunkSize = size;
    m_MinChunkSize = size_t(double(size) * 0.8);
    m_MaxChunkSize = size_t(double(size) * 1.2);
}


static const size_t kChunkSize = 32*1024;


void CId2Compressor::Compress(const SSplitterParams& params,
                              list<vector<char>*>& dst,
                              const char* data, size_t size)
{
    vector<char>* vec;
    dst.push_back(vec = new vector<char>);
    CompressHeader(params, *vec, size);
    while ( size ) {
        size_t chunk_size = min(size, kChunkSize);
        CompressChunk(params, *vec, data, chunk_size);
        data += chunk_size;
        size -= chunk_size;
        if ( size ) { // another vector<char> for next chunk
            dst.push_back(vec = new vector<char>);
        }
    }
    CompressFooter(params, *vec, size);
}


void CId2Compressor::Compress(const SSplitterParams& params,
                              vector<char>& dst,
                              const char* data, size_t size)
{
    CompressHeader(params, dst, size);
    CompressChunk(params, dst, data, size);
    CompressFooter(params, dst, size);
}


void CId2Compressor::CompressChunk(const SSplitterParams& params,
                                   vector<char>& dst,
                                   const char* data, size_t size)
{
    switch ( params.m_Compression ) {
    case SSplitterParams::eCompression_none:
        sx_Append(dst, data, size);
        break;
    case SSplitterParams::eCompression_nlm_zip:
    {{
        size_t pos = dst.size();
        CZipCompression compr(CCompression::eLevel_Default);
        dst.resize(pos + 32 + size_t(double(size)*1.01));
        size_t real_size = 0;
        if ( !compr.CompressBuffer(data, size,
                                   &dst[pos+8], dst.size()-(pos+8),
                                   &real_size) ) {
            NCBI_THROW(CSplitException, eCompressionError,
                       "zip compression failed");
        }
        for ( size_t i = 0, s = real_size; i < 4; ++i, s <<= 8 ) {
            dst[pos+i] = char(s >> 24);
        }
        for ( size_t i = 0, s = size; i < 4; ++i, s <<= 8 ) {
            dst[pos+4+i] = char(s >> 24);
        }
        dst.resize(pos+8+real_size);
        break;
    }}
    default:
        NCBI_THROW(CSplitException, eNotImplemented,
                   "compression method is not implemented");
    }
}


void CId2Compressor::CompressHeader(const SSplitterParams& params,
                                    vector<char>& dst,
                                    size_t)
{
    switch ( params.m_Compression ) {
    case SSplitterParams::eCompression_none:
        break;
    case SSplitterParams::eCompression_nlm_zip:
        sx_Append(dst, "ZIP", 4);
        break;
    default:
        NCBI_THROW(CSplitException, eNotImplemented,
                   "compression method is not implemented");
    }
}


void CId2Compressor::CompressFooter(const SSplitterParams& ,
                                    vector<char>& ,
                                    size_t)
{
}


void CId2Compressor::sx_Append(vector<char>& dst,
                               const char* data, size_t size)
{
    size_t pos = dst.size();
    dst.resize(pos + size);
    memcpy(&dst[pos], data, size);
}


END_SCOPE(objects)
END_NCBI_SCOPE
