#ifndef ALGO_TEXT___VECTOR_SERIAL__HPP
#define ALGO_TEXT___VECTOR_SERIAL__HPP

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

#include <corelib/ncbistd.hpp>
#include <util/simple_buffer.hpp>

BEGIN_NCBI_SCOPE

/// @name Serialization interfaces
/// @{

///
/// Generics
/// These throw an exception; we must implement serialization for each type
///
template <class Key, class Score>
void Serialize(CNcbiOstream& ostr, const CRawScoreVector<Key, Score>& vec)
{
    NCBI_THROW(CException, eUnknown,
               "Serialize(): Serialization type unknown");
}

template <class Key, class Score>
void Deserialize(CNcbiIstream& istr, CRawScoreVector<Key, Score>& vec)
{
    NCBI_THROW(CException, eUnknown,
               "Deserialize(): Deserialization type unknown");
}


template <class Key, class Score>
void Serialize(CNcbiOstream& ostr, const CScoreVector<Key, Score>& vec)
{
    NCBI_THROW(CException, eUnknown,
               "Serialize(): Serialization type unknown");
}

template <class Key, class Score>
void Deserialize(CNcbiIstream& istr, CScoreVector<Key, Score>& vec)
{
    NCBI_THROW(CException, eUnknown,
               "Deserialize(): Deserialization type unknown");
}



template <class Key, class Score>
void Encode(const CRawScoreVector<Key, Score>& vec,
            vector<char>& data)
{
    NCBI_THROW(CException, eUnknown,
               "Encode(): Serialization type unknown");
}

template <class Key, class Score>
void Encode(const CRawScoreVector<Key, Score>& vec,
            vector<unsigned char>& data)
{
    NCBI_THROW(CException, eUnknown,
               "Encode(): Serialization type unknown");
}

template <class Key, class Score>
void Encode(const CRawScoreVector<Key, Score>& vec,
            CSimpleBuffer& data)
{
    NCBI_THROW(CException, eUnknown,
               "Encode(): Serialization type unknown");
}


template <class Key, class Score>
void Decode(const vector<char>& data, CRawScoreVector<Key, Score>& vec)
{
    NCBI_THROW(CException, eUnknown,
               "Decode(): Deserialization type unknown");
}

template <class Key, class Score>
void Decode(const vector<unsigned char>& data, CRawScoreVector<Key, Score>& vec)
{
    NCBI_THROW(CException, eUnknown,
               "Decode(): Deserialization type unknown");
}

template <class Key, class Score>
void Decode(const void* data, size_t size, CRawScoreVector<Key, Score>& vec)
{
    NCBI_THROW(CException, eUnknown,
               "Decode(): Deserialization type unknown");
}


template <class Key, class Score>
void Encode(const CScoreVector<Key, Score>& vec,
            vector<char>& data)
{
    NCBI_THROW(CException, eUnknown,
               "Encode(): Serialization type unknown");
}

template <class Key, class Score>
void Encode(const CScoreVector<Key, Score>& vec,
            vector<unsigned char>& data)
{
    NCBI_THROW(CException, eUnknown,
               "Encode(): Serialization type unknown");
}

template <class Key, class Score>
void Encode(const CScoreVector<Key, Score>& vec,
            CSimpleBuffer& data)
{
    NCBI_THROW(CException, eUnknown,
               "Encode(): Serialization type unknown");
}


template <class Key, class Score>
void Decode(const vector<char>& data, CScoreVector<Key, Score>& vec)
{
    NCBI_THROW(CException, eUnknown,
               "Decode(): Deserialization type unknown");
}

template <class Key, class Score>
void Decode(const vector<unsigned char>& data, CScoreVector<Key, Score>& vec)
{
    NCBI_THROW(CException, eUnknown,
               "Decode(): Deserialization type unknown");
}

template <class Key, class Score>
void Decode(const void* data, size_t size, CScoreVector<Key, Score>& vec)
{
    NCBI_THROW(CException, eUnknown,
               "Decode(): Deserialization type unknown");
}


///
/// Specializations for known types
///



///
/// Uint4-based keys
///

template<>
void Serialize<Uint4, float>(CNcbiOstream& ostr,
                             const CRawScoreVector<Uint4, float>& vec);
template<>
void Deserialize<Uint4, float>(CNcbiIstream& istr,
                               CRawScoreVector<Uint4, float>& vec);

template<>
void Encode<Uint4, float>(const CRawScoreVector<Uint4, float>& vec,
                          vector<unsigned char>& data);
template<>
void Encode<Uint4, float>(const CRawScoreVector<Uint4, float>& vec,
                          vector<char>& data);
template<>
void Encode<Uint4, float>(const CRawScoreVector<Uint4, float>& vec,
                          CSimpleBuffer& data);
template<>
void Decode<Uint4, float>(const vector<unsigned char>& data,
                          CRawScoreVector<Uint4, float>& vec);
template<>
void Decode<Uint4, float>(const vector<char>& data,
                          CRawScoreVector<Uint4, float>& vec);
template<>
void Decode<Uint4, float>(const void* data, size_t size,
                          CRawScoreVector<Uint4, float>& vec);


template<>
void Serialize<Uint4, float>(CNcbiOstream& ostr,
                             const CScoreVector<Uint4, float>& vec);
template<>
void Deserialize<Uint4, float>(CNcbiIstream& istr,
                               CScoreVector<Uint4, float>& vec);

template<>
void Encode<Uint4, float>(const CScoreVector<Uint4, float>& vec,
                          vector<char>& data);
template<>
void Encode<Uint4, float>(const CScoreVector<Uint4, float>& vec,
                          vector<unsigned char>& data);
template<>
void Encode<Uint4, float>(const CScoreVector<Uint4, float>& vec,
                          CSimpleBuffer& data);
template<>
void Decode<Uint4, float>(const vector<char>& data,
                          CScoreVector<Uint4, float>& vec);
template<>
void Decode<Uint4, float>(const vector<unsigned char>& data,
                          CScoreVector<Uint4, float>& vec);
template<>
void Decode<Uint4, float>(const void* data, size_t size,
                          CScoreVector<Uint4, float>& vec);


///
/// String based keys
/// Raw vector encoding is not supported
///

template<>
void Serialize<string, float>(CNcbiOstream& ostr,
                              const CScoreVector<string, float>& vec);
template<>
void Deserialize<string, float>(CNcbiIstream& istr,
                                CScoreVector<string, float>& vec);

template<>
void Encode<string, float>(const CScoreVector<string, float>& vec,
                           vector<unsigned char>& data);
template<>
void Encode<string, float>(const CScoreVector<string, float>& vec,
                           vector<char>& data);
template<>
void Decode<string, float>(const vector<unsigned char>& data,
                           CScoreVector<string, float>& vec);
template<>
void Decode<string, float>(const vector<char>& data,
                           CScoreVector<string, float>& vec);
template<>
void Decode<string, float>(const void* data, size_t size,
                           CScoreVector<string, float>& vec);


/// @}


END_NCBI_SCOPE

#endif  // ALGO_TEXT___VECTOR_SERIAL__HPP
