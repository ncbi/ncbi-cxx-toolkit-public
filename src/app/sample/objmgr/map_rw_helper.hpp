#ifndef __MAP_RW_HELPER__HPP
#define __MAP_RW_HELPER__HPP

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
* Author: Maxim Didenko
*
* File Description:
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>

#include <vector>

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////
///
/// template<typename TElem> CContElemConverter
///
/// The helper class for storing/retrieving a call to/from a string
///
template<typename TElem>
class CContElemConverter
{
public:
    static TElem  FromString(const string& str)   { return TElem(str);   }
    static string ToString  (const TElem&  elem)  { return string(elem); }
};


///////////////////////////////////////////////////////
///
/// template<> CContElemConverter<string>
///
/// The specialization of CContElemConverter for the string class.
///
template<>
class CContElemConverter<string>
{
public:
    static const string& FromString(const string& str)  { return str; }
    static const string& ToString  (const string& str)  { return str; }
};

///////////////////////////////////////////////////////
///
/// Read a string from a stream. The string is following 
/// by its size
/// 
inline string ReadStringFromStream(CNcbiIstream& is)
{
    string str;
    if (!is.good() || is.eof())
        return str;

    size_t size;
    is >> size;
    if (!is.good() || is.eof())
        return str;
    vector<char> buf(size+1, 0);
    is.read(&*buf.begin(), size+1);
    size_t count = is.gcount();
    if (count > 0)
        str.append((&*buf.begin())+1, count-1);

    return str;
}

///////////////////////////////////////////////////////
///
/// Write a map to a stream
/// 
template<typename TMap>
CNcbiOstream& WriteMap(CNcbiOstream& os, const TMap& cont)
{
    typedef CContElemConverter<typename TMap::key_type>    TKeyConverter;
    typedef CContElemConverter<typename TMap::mapped_type> TValueConverter;

    os << cont.size() << ' ';

    ITERATE(typename TMap, it, cont) {
        string key = TKeyConverter  ::ToString(it->first);
        string value = TValueConverter::ToString(it->second);   
        os << key.size() << ' ' << key;
        os << value.size() << ' ' << value;
    }

    return os;
}


///////////////////////////////////////////////////////
///
/// Read a map from a stream
/// 
template<typename TMap>
CNcbiIstream& ReadMap(CNcbiIstream& is, TMap& cont)
{
    typedef CContElemConverter<typename TMap::key_type>    TKeyConverter;
    typedef CContElemConverter<typename TMap::mapped_type> TValueConverter;


    string str;
    if (!is.good() || is.eof())
        return is;

    size_t size;
    is >> size;
    if (!is.good() || is.eof())
        return is;
    for( size_t i = 0; i < size; ++i) {

        string key = ReadStringFromStream(is);
        string value = ReadStringFromStream(is);

        cont.insert(typename TMap::value_type(
                             TKeyConverter::FromString(key),
                             TValueConverter::FromString(value))
                    );
    }

    return is;
}


END_NCBI_SCOPE

#endif // __MAP_RW_HELPER__HPP
