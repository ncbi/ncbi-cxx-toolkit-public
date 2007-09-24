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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/text/vector.hpp>
#include <algo/text/vector_score.hpp>
#include <algorithm>
#include <math.h>
#include <set>

#include <connect/ncbi_conn_stream.hpp>
#include <util/compress/zlib.hpp>
#include <algo/text/vector_serial.hpp>

#include <util/compress/zlib.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <util/rwstream.hpp>


BEGIN_NCBI_SCOPE

template<>
void Serialize<Uint4, float>(CNcbiOstream& ostr,
                             const CRawScoreVector<Uint4, float>& vec)
{
    throw runtime_error("oops, implement serialization");
}


template<>
void Serialize<Uint4, float>(CNcbiOstream& ostr,
                             const CScoreVector<Uint4, float>& vec)
{
    throw runtime_error("oops, implement serialization");
}


template<>
void Deserialize<Uint4, float>(CNcbiIstream& istr,
                               CRawScoreVector<Uint4, float>& vec)
{
    throw runtime_error("oops, implement serialization");
}


template<>
void Deserialize<Uint4, float>(CNcbiIstream& istr,
                               CScoreVector<Uint4, float>& vec)
{
    throw runtime_error("oops, implement serialization");
}


/////////////////////////////////////////////////////////////////////////////

template <class Buffer>
class CBufferWriter : public IWriter
{
public:
    typedef Buffer TBuffer;

    CBufferWriter(TBuffer& buf, bool clear_buffer = true)
        : m_Buffer(buf)
        {
            if (clear_buffer) {
                m_Buffer.clear();
            }
        }

    ERW_Result Write(const void* buf, size_t count, size_t* bytes_written)
    {
        _ASSERT(buf  &&  count);
        size_t offs = m_Buffer.size();
        m_Buffer.resize(m_Buffer.size() + count);
        memcpy(&m_Buffer[0] + offs, buf, count);
        if (bytes_written) {
            *bytes_written = count;
        }

        return eRW_Success;
    }

    ERW_Result Flush(void)
    {
        return eRW_Success;
    }

private:
    TBuffer& m_Buffer;

    /// forbidden
    CBufferWriter(const CBufferWriter&);
    CBufferWriter& operator=(const CBufferWriter&);
};


template <class Buffer>
class CBufferWriterStream : public CWStream
{
public:
    CBufferWriterStream(CBufferWriter<Buffer>& writer)
        : CWStream(&writer)
        {
        }

private:
    /// forbidden
    CBufferWriterStream(const CBufferWriterStream&);
    CBufferWriterStream& operator=(const CBufferWriterStream&);
};



/////////////////////////////////////////////////////////////////////////////

#define ENCODE(Type) \
template<> \
void Encode<Uint4, float>(const CRawScoreVector<Uint4, float>& vec, \
                          Type& data) \
{ \
    data.clear(); \
    data.reserve(sizeof(Uint4) + \
                 sizeof(CRawScoreVector<Uint4, float>::TVector::value_type) * vec.size()); \
 \
    CBufferWriter< Type > bw(data); \
    CBufferWriterStream< Type > ostr(bw); \
 \
    Uint4 uid = vec.GetId(); \
    ostr.write((const char*)&uid, sizeof(uid)); \
    ostr.write((const char*)&vec.Get()[0],  \
               sizeof(CRawScoreVector<Uint4, float>::TVector::value_type) * vec.Get().size()); \
}
  
ENCODE(vector<char>);
ENCODE(vector<unsigned char>);
ENCODE(CSimpleBuffer);


template<>
void Encode<Uint4, float>(const CScoreVector<Uint4, float>& vec,
                          vector<char>& data)
{
    CRawScoreVector<Uint4, float> raw(vec);
    Encode(raw, data);
}

template<>
void Encode<Uint4, float>(const CScoreVector<Uint4, float>& vec,
                          CSimpleBuffer& data)
{
    CRawScoreVector<Uint4, float> raw(vec);
    Encode(raw, data);
}



template<>
void Decode<Uint4, float>(const void* data, size_t size,
                          CRawScoreVector<Uint4, float>& vec)
{
    CRawScoreVector<Uint4, float>::TVector& vec_data = vec.Set();
    vec_data.clear();
    vec_data.reserve((size - sizeof(Uint4)) /
                     sizeof(CRawScoreVector<Uint4, float>::TIdxScore));

    CNcbiIstrstream istr((const char*)data, size);

    /// uid should be guaranteed
    Uint4 uid = 0;
    istr.read((char*)&uid, sizeof(uid));
    vec.SetId(uid);

    if ( !uid ) {
        /// invalid UID; no data returned
        return;
    }

    /// data strip while valid
    while (istr) {
        CRawScoreVector<Uint4, float>::TIdxScore p;
        istr.read((char*)&p.first,  sizeof(p.first));
        if ( !istr ) {
            break;
        }
        istr.read((char*)&p.second, sizeof(p.second));
        vec_data.push_back(p);
    }

    /**
    typedef CRawScoreVector<Uint4, float> TVector;
    cout << "read vector: " << vec.GetId() << ": ";
    ITERATE (TVector, iter, vec) {
        cout << " (" << iter->first << "," << iter->second << ")";
    }
    cout << endl;
    **/
}

template<>
void Decode<Uint4, float>(const vector<char>& data,
                          CRawScoreVector<Uint4, float>& vec)
{
    Decode(&data[0], data.size(), vec);
}

template<>
void Decode<Uint4, float>(const vector<unsigned char>& data,
                          CRawScoreVector<Uint4, float>& vec)
{
    Decode(&data[0], data.size(), vec);
}

template<>
void Decode<Uint4, float>(const CSimpleBuffer& data,
                          CRawScoreVector<Uint4, float>& vec)
{
    Decode(&data[0], data.size(), vec);
}

template<>
void Decode<Uint4, float>(const vector<char>& data,
                          CScoreVector<Uint4, float>& vec)
{
    CRawScoreVector<Uint4, float> raw;
    Decode(&data[0], data.size(), raw);
    vec = raw;
}

template<>
void Decode<Uint4, float>(const vector<unsigned char>& data,
                          CScoreVector<Uint4, float>& vec)
{
    CRawScoreVector<Uint4, float> raw;
    Decode(&data[0], data.size(), raw);
    vec = raw;
}

template<>
void Decode<Uint4, float>(const CSimpleBuffer& data,
                          CScoreVector<Uint4, float>& vec)
{
    CRawScoreVector<Uint4, float> raw;
    Decode(&data[0], data.size(), raw);
    vec = raw;
}

template<>
void Decode<Uint4, float>(const void* data, size_t size,
                          CScoreVector<Uint4, float>& vec)
{
    CRawScoreVector<Uint4, float> raw;
    Decode(data, size, raw);
    vec = raw;
}


/////////////////////////////////////////////////////////////////////////////

// define this to store compressed word frequency strings in the BerkeleyDB
// store
#define INDEX_USE_GZIP

template<>
void Serialize<string, float>(CNcbiOstream& ostr,
                              const CScoreVector<string, float>& vec)
{
    CZipStreamCompressor zipper(CZipCompressor::eLevel_Best,
                                16384, 16384,
                                15 /* window bits */,
                                9  /* memory bits */,
                                kZlibDefaultStrategy,
                                0);

    CCompressionOStream zip_str(ostr, &zipper);

    typedef CScoreVector<string, float> TVector;
    ITERATE (TVector, iter, vec) {
        unsigned len   = iter->first.size();
        zip_str.write((const char*)&len,   sizeof(unsigned));
        zip_str.write(iter->first.data(), iter->first.size());

        float score = iter->second;
        zip_str.write((const char*)&score, sizeof(float));
    }
}


template<>
void Deserialize<string, float>(CNcbiIstream& istr,
                                CScoreVector<string, float>& vec)
{
    vec.clear();
    CZipStreamDecompressor zipper(16384, 16384,
                                  15 /* window bits */,
                                  0);

    CCompressionIStream zip_istr(istr, &zipper);
    while ((bool)zip_istr) {
        unsigned len = 0;
        zip_istr.read((char*)&len,   sizeof(unsigned));
        if ( !zip_istr  ||  !len ) {
            break;
        }
        string word;
        word.resize(len);
        zip_istr.read(&word[0], word.size());

        float score = 0;
        zip_istr.read((char*)&score, sizeof(float));
        vec.insert(vec.end(),
                   CScoreVector<string, float>::value_type(word, score));
    }
}


template<>
void Encode<string, float>(const CScoreVector<string, float>& vec,
                           vector<char>& data)
{
    CConn_MemoryStream mem_str;
    Serialize(mem_str, vec);
    mem_str.flush();

    data.resize(mem_str.tellp() - CT_POS_TYPE(0));
    mem_str.read(&data[0], data.size());
}


template<>
void Encode<string, float>(const CScoreVector<string, float>& vec,
                           vector<unsigned char>& data)
{
    CConn_MemoryStream mem_str;
    Serialize(mem_str, vec);
    mem_str.flush();

    data.resize(mem_str.tellp() - CT_POS_TYPE(0));
    mem_str.read((char*)&data[0], data.size());
}


template<>
void Decode<string, float>(const void* data, size_t size,
                           CScoreVector<string, float>& vec)
{
    CNcbiIstrstream istr((const char*)data, size);
    Deserialize(istr, vec);
}


template<>
void Decode<string, float>(const vector<char>& data,
                           CScoreVector<string, float>& vec)
{
    Decode(&data[0], data.size(), vec);
}


template<>
void Decode<string, float>(const vector<unsigned char>& data,
                           CScoreVector<string, float>& vec)
{
    Decode(&data[0], data.size(), vec);
}


END_NCBI_SCOPE
