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
 * Authors: Eugene Vasilchenko
 *
 * File Description: processor for data from OSG
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/rwstream.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <util/compress/compress.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqsplit/seqsplit__.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>

#include <strstream>

#include "split_info_utils.hpp"

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);


CDataChunkStream::CDataChunkStream()
    : m_Gzip(false)
{
}


CDataChunkStream::~CDataChunkStream()
{
}


void CDataChunkStream::AddDataChunk(const unsigned char *  data,
                                    unsigned int  size,
                                    int  chunk_no)
{
    m_Chunks[chunk_no].assign(data, data+size);
}


BEGIN_LOCAL_NAMESPACE;


class CDataChunkReader : public IReader
{
public:
    typedef map<int, vector<char>> TChunks;

    explicit
    CDataChunkReader(const TChunks& chunks)
        : m_EndOfChunks(chunks.end()),
          m_CurrentChunk(chunks.begin()),
          m_CurrentChunkPos(0)
        {
        }

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0) override
        {
            if ( x_Advance() ) {
                count = min(count, x_AvaliableCount());
                memcpy(buf, x_GetCurrentDataPtr(), count);
                m_CurrentChunkPos += count;
                *bytes_read = count;
                return eRW_Success;
            }
            else {
                *bytes_read = 0;
                return eRW_Eof;
            }
        }
    
    virtual ERW_Result PendingCount(size_t* count) override
        {
            if ( x_Advance() ) {
                *count = x_AvaliableCount();
                return eRW_Success;
            }
            else {
                *count = 0;
                return eRW_Eof;
            }
        }

private:
    const char* x_GetCurrentDataPtr() const
        {
            return m_CurrentChunk->second.data() + m_CurrentChunkPos;
        }
    size_t x_AvaliableCount() const
        {
            return m_CurrentChunk->second.size() - m_CurrentChunkPos;
        }
    bool x_Advance()
        {
            for ( ;; ) {
                if ( m_CurrentChunk == m_EndOfChunks ) {
                    return false;
                }
                else if ( m_CurrentChunkPos >= m_CurrentChunk->second.size() ) {
                    ++m_CurrentChunk;
                    m_CurrentChunkPos = 0;
                }
                else {
                    return true;
                }
            }
        }

    TChunks::const_iterator m_EndOfChunks;
    TChunks::const_iterator m_CurrentChunk;
    size_t m_CurrentChunkPos;
};

END_LOCAL_NAMESPACE;


CNcbiIstream& CDataChunkStream::GetStream()
{
    m_Stream = make_unique<CRStream>(new CDataChunkReader(m_Chunks),
                                     0, nullptr, CRWStreambuf::fOwnReader);
    return *m_Stream;
}


vector<int> GetBioseqChunks(const CSeq_id& seq_id,
                            const CBlobRecord& blob,
                            const unsigned char *  data,
                            unsigned int  size,
                            int  chunk_no)
{
    CDataChunkStream buf;
    buf.AddDataChunk(data, size, chunk_no);
    buf.SetGzip(blob.GetFlag(EBlobFlags::eGzip));
    return GetBioseqChunks(seq_id, *buf.DeserializeSplitInfo());
}


static
bool s_Matches(const CSeq_id& seq_id,
               const CID2S_Bioseq_Ids::C_E& id)
{
    if ( id.IsSeq_id() ) {
        if ( auto req_textid = seq_id.GetTextseq_Id() ) {
            if ( auto seq_textid = id.GetSeq_id().GetTextseq_Id() ) {
                if ( req_textid->IsSetAccession() && seq_textid->IsSetAccession() &&
                     NStr::EqualNocase(req_textid->GetAccession(), seq_textid->GetAccession()) ) {
                    if ( !req_textid->IsSetVersion() ||
                         (seq_textid->IsSetVersion() &&
                          req_textid->GetVersion() == seq_textid->GetVersion()) ) {
                        return true;
                    }
                }
                if ( req_textid->IsSetName() && seq_textid->IsSetName() &&
                     NStr::EqualNocase(req_textid->GetName(), seq_textid->GetName()) ) {
                    return true;
                }
            }
        }
        return seq_id.Equals(id.GetSeq_id());
    }
    else if ( !seq_id.IsGi() ) {
        return false;
    }
    if ( id.IsGi() ) {
        return seq_id.GetGi() == id.GetGi();
    }
    else {
        auto& gi_range = id.GetGi_range();
        return
            seq_id.GetGi() >= gi_range.GetStart() &&
            seq_id.GetGi() < GI_FROM(TIntId, GI_TO(TIntId, gi_range.GetStart())+gi_range.GetCount());
    }
}


static
bool s_ContainsBioseq(const CSeq_id& seq_id,
                      const CID2S_Chunk_Info& chunk)
{
    for ( auto& content : chunk.GetContent() ) {
        if ( content->IsBioseq_place() ) {
            for ( auto& place : content->GetBioseq_place() ) {
                for ( auto& id : place->GetSeq_ids().Get() ) {
                    if ( s_Matches(seq_id, *id) ) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}


vector<int> GetBioseqChunks(const CSeq_id& seq_id,
                            const CID2S_Split_Info& split_info)
{
    vector<int> ret;
    for ( auto& chunk : split_info.GetChunks() ) {
        if ( s_ContainsBioseq(seq_id, *chunk) ) {
            ret.push_back(chunk->GetId());
        }
    }
    return ret;
}


CRef<CID2S_Split_Info> CDataChunkStream::DeserializeSplitInfo()
{
    CRef<CID2S_Split_Info> info(new CID2S_Split_Info());
    CNcbiIstream* in = &GetStream();
    unique_ptr<CNcbiIstream> z_stream;
    if ( IsGzip() ) {
        z_stream.reset(new CCompressionIStream(*in,
                                               new CZipStreamDecompressor(CZipCompression::fGZip),
                                               CCompressionIStream::fOwnProcessor));
        in = z_stream.get();
    }
    unique_ptr<CObjectIStream> obj_stream(CObjectIStream::Open(eSerial_AsnBinary, *in));
    *obj_stream >> *info;
    return info;
}


END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
