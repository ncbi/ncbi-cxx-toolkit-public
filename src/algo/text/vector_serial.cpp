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


BEGIN_NCBI_SCOPE

template<>
void Serialize<TUid, TReal>(CNcbiOstream& ostr, const TRawScoreVector& vec)
{
    throw runtime_error("oops, implement serialization");
}


template<>
void Serialize<TUid, TReal>(CNcbiOstream& ostr, const TScoreVector& vec)
{
    throw runtime_error("oops, implement serialization");
}


template<>
void Deserialize<TUid, TReal>(CNcbiIstream& istr, TRawScoreVector& vec)
{
    throw runtime_error("oops, implement serialization");
}


template<>
void Deserialize<TUid, TReal>(CNcbiIstream& istr, TScoreVector& vec)
{
    throw runtime_error("oops, implement serialization");
}


/////////////////////////////////////////////////////////////////////////////


template <class Type>
char* s_Write(char* p, Type val)
{
    const char* src = reinterpret_cast<const char*>(&val);
    for (unsigned i = 0;  i < sizeof(Type);  ++i) {
        *p++ = *src++;
    }
    return p;
}


template<>
void Encode<TUid, TReal>(const TRawScoreVector& vec, vector<char>& data)
{
    data.clear();
    data.resize(sizeof(TUid) +
                sizeof(TRawScoreVector::TVector::value_type) * vec.size());

    char* p = &data[0];

    p = s_Write(p, vec.GetId());
    memcpy(p, &vec.Get()[0],
           sizeof(TRawScoreVector::TVector::value_type) * vec.Get().size());
}

template<>
void Encode<TUid, TReal>(const TRawScoreVector& vec, vector<unsigned char>& data)
{
    data.clear();
    data.resize(sizeof(TUid) +
                sizeof(TRawScoreVector::TVector::value_type) * vec.size());

    char* p = (char*)&data[0];

    p = s_Write(p, vec.GetId());
    memcpy(p, &vec.Get()[0],
           sizeof(TRawScoreVector::TVector::value_type) * vec.Get().size());
}

template<>
void Encode<TUid, TReal>(const TScoreVector& vec, vector<char>& data)
{
    TRawScoreVector raw(vec);
    Encode(raw, data);
}

template<>
void Encode<TUid, TReal>(const TScoreVector& vec, vector<unsigned char>& data)
{
    TRawScoreVector raw(vec);
    Encode(raw, data);
}



template<>
void Decode<TUid, TReal>(const void* data, size_t size, TRawScoreVector& vec)
{
    TRawScoreVector::TVector& vec_data = vec.Set();
    vec_data.clear();
    vec_data.reserve((size - sizeof(TUid)) /
                     sizeof(TRawScoreVector::TIdxScore));

    CNcbiIstrstream istr((const char*)data, size);

    /// uid should be guaranteed
    TUid uid = 0;
    istr.read((char*)&uid, sizeof(uid));
    vec.SetId(uid);

    if ( !uid ) {
        /// invalid UID; no data returned
        return;
    }

    /// data strip while valid
    while (istr) {
        TRawScoreVector::TIdxScore p;
        istr.read((char*)&p.first,  sizeof(p.first));
        if ( !istr ) {
            break;
        }
        istr.read((char*)&p.second, sizeof(p.second));
        vec_data.push_back(p);
    }
}

template<>
void Decode<TUid, TReal>(const vector<char>& data, TRawScoreVector& vec)
{
    Decode(&data[0], data.size(), vec);
}

template<>
void Decode<TUid, TReal>(const vector<unsigned char>& data, TRawScoreVector& vec)
{
    Decode(&data[0], data.size(), vec);
}

template<>
void Decode<TUid, TReal>(const vector<char>& data, TScoreVector& vec)
{
    TRawScoreVector raw;
    Decode(&data[0], data.size(), raw);
    vec = raw;
}

template<>
void Decode<TUid, TReal>(const vector<unsigned char>& data, TScoreVector& vec)
{
    TRawScoreVector raw;
    Decode(&data[0], data.size(), raw);
    vec = raw;
}

template<>
void Decode<TUid, TReal>(const void* data, size_t size, TScoreVector& vec)
{
    TRawScoreVector raw;
    Decode(data, size, raw);
    vec = raw;
}


/////////////////////////////////////////////////////////////////////////////

// define this to store compressed word frequency strings in the BerkeleyDB
// store
#define INDEX_USE_GZIP

template<>
void Serialize<string, TReal>(CNcbiOstream& ostr,
                              const CScoreVector<string, TReal>& vec)
{
    CZipStreamCompressor zipper(CZipCompressor::eLevel_Best,
                                16384, 16384,
                                15 /* window bits */,
                                9  /* memory bits */,
                                kZlibDefaultStrategy,
                                0);

    CCompressionOStream zip_str(ostr, &zipper);

    typedef CScoreVector<string, TReal> TVector;
    ITERATE (TVector, iter, vec) {
        unsigned len   = iter->first.size();
        zip_str.write((const char*)&len,   sizeof(unsigned));
        zip_str.write(iter->first.data(), iter->first.size());

        TReal score = iter->second;
        zip_str.write((const char*)&score, sizeof(TReal));
    }
}


template<>
void Deserialize<string, TReal>(CNcbiIstream& istr,
                                CScoreVector<string, TReal>& vec)
{
    vec.clear();
    CZipStreamDecompressor zipper(16384, 16384,
                                  15 /* window bits */,
                                  0);

    CCompressionIStream zip_istr(istr, &zipper);
    while (zip_istr) {
        unsigned len = 0;
        zip_istr.read((char*)&len,   sizeof(unsigned));
        if ( !zip_istr  ||  !len ) {
            break;
        }
        string word;
        word.resize(len);
        zip_istr.read(&word[0], word.size());

        TReal score = 0;
        zip_istr.read((char*)&score, sizeof(TReal));
        vec.insert(vec.end(), make_pair(word, score));
    }
}


template<>
void Encode<string, TReal>(const CScoreVector<string, TReal>& vec,
                           vector<char>& data)
{
    CConn_MemoryStream mem_str;
    Serialize(mem_str, vec);
    mem_str.flush();

    data.resize(mem_str.tellp() - CT_POS_TYPE(0));
    mem_str.read(&data[0], data.size());
}


template<>
void Encode<string, TReal>(const CScoreVector<string, TReal>& vec,
                           vector<unsigned char>& data)
{
    CConn_MemoryStream mem_str;
    Serialize(mem_str, vec);
    mem_str.flush();

    data.resize(mem_str.tellp() - CT_POS_TYPE(0));
    mem_str.read((char*)&data[0], data.size());
}


template<>
void Decode<string, TReal>(const void* data, size_t size,
                           CScoreVector<string, TReal>& vec)
{
    CNcbiIstrstream istr((const char*)data, size);
    Deserialize(istr, vec);
}


template<>
void Decode<string, TReal>(const vector<char>& data,
                           CScoreVector<string, TReal>& vec)
{
    Decode(&data[0], data.size(), vec);
}


template<>
void Decode<string, TReal>(const vector<unsigned char>& data,
                           CScoreVector<string, TReal>& vec)
{
    Decode(&data[0], data.size(), vec);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/12/17 14:13:20  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
