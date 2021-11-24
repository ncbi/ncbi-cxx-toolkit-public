#ifndef SPLIT_INFO_UTILS__HPP
#define SPLIT_INFO_UTILS__HPP

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
 * File Description: ID2 split-info processing utilities
 *
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>

BEGIN_NCBI_NAMESPACE;

BEGIN_NAMESPACE(objects);

class CSeq_id;
class CID2S_Split_Info;

END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);

USING_SCOPE(objects);
USING_IDBLOB_SCOPE;

class CDataChunkStream
{
public:
    CDataChunkStream();
    ~CDataChunkStream();
    
    void AddDataChunk(const unsigned char *  data,
                      unsigned int  size,
                      int  chunk_no);

    CRef<CID2S_Split_Info> DeserializeSplitInfo();

    CNcbiIstream& GetStream();

    void SetGzip(bool gzip)
        {
            m_Gzip = gzip;
        }
    bool IsGzip() const
        {
            return m_Gzip;
        }

private:
    map<int, vector<char>> m_Chunks;
    unique_ptr<CNcbiIstream> m_Stream;
    bool m_Gzip;
};

vector<int> GetBioseqChunks(const CSeq_id& seq_id,
                            const CID2S_Split_Info& split_info);

vector<int> GetBioseqChunks(const CSeq_id& seq_id,
                            const CBlobRecord& blob,
                            const unsigned char *  data,
                            unsigned int  size,
                            int  chunk_no);

END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif // SPLIT_INFO_UTILS__HPP
