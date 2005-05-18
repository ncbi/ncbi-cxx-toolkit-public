#ifndef CGI___CGI_SERIAL__HPP
#define CGI___CGI_SERIAL__HPP

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
 * Author:  Maxim Didneko
 *
 */

/// @file cont_serial.hpp

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <cgi/ncbicgi.hpp>

#include <vector>

BEGIN_NCBI_SCOPE

template<typename TElem>
class CContElemConverter
{
public:
    static TElem  FromString(const string& str)   { return TElem(str);   }
    static string ToString  (const TElem&  elem)  { return string(elem); }
};


template<>
class CContElemConverter<string>
{
public:
    static const string& FromString(const string& str)  { return str; }
    static const string& ToString  (const string& str)  { return str; }
};

template<>
class NCBI_XCGI_EXPORT CContElemConverter<CCgiEntry>
{
public:
    static CCgiEntry  FromString(const string& str);
    static string ToString  (const CCgiEntry&  elem);
};

template<typename TMap>
CNcbiOstream& WriteMap(CNcbiOstream& os, const TMap& cont)
{
    typedef CContElemConverter<typename TMap::key_type>    TKeyConverter;
    typedef CContElemConverter<typename TMap::mapped_type> TValueConverter;

    CNcbiOstrstream ostr;
    ITERATE(typename TMap, it, cont) {
        if (it != cont.begin())
            ostr << '&';
        ostr << URL_EncodeString(TKeyConverter  ::ToString(it->first)) << '='
             << URL_EncodeString(TValueConverter::ToString(it->second));
    }
    ostr << ends;
    try {
        os << ostr.pcount() << ' ' << ostr.str();
    } catch (...) {
        ostr.freeze(false);
        throw;
    }
    ostr.freeze(false);
    
    return os;
}


template<typename TMap>
CNcbiIstream& ReadMap(CNcbiIstream& is, TMap& cont)
{
    typedef CContElemConverter<typename TMap::key_type>    TKeyConverter;
    typedef CContElemConverter<typename TMap::mapped_type> TValueConverter;

    string str;
    {{
        size_t size;
        is >> size;
        if (size > 0) {
            AutoPtr<char, ArrayDeleter<char> > buf(new char[size]);
            is.read(buf.get(), size);
            size_t count = is.gcount();
            if (count > 0)
                str.append(buf.get()+1, count-1);
        }
    }}

    vector<string> pairs;
    NStr::Tokenize(str, "&", pairs);

    cont.clear();
    ITERATE(vector<string>, it, pairs) {
        string key;
        string value;
        NStr::SplitInTwo(*it, "=", key, value);
        cont.insert(make_pair(
                    TKeyConverter::FromString(URL_DecodeString(key)),
                    TValueConverter::FromString(URL_DecodeString(value)))
                   );
    }

    return is;
}
NCBI_XCGI_EXPORT
extern CNcbiOstream& 
WriteCgiCookies(CNcbiOstream& os, const CCgiCookies& cont);

NCBI_XCGI_EXPORT
extern CNcbiIstream& 
ReadCgiCookies(CNcbiIstream& is, CCgiCookies& cont);


NCBI_XCGI_EXPORT
extern CNcbiOstream& 
WriteEnvironment(CNcbiOstream& os, const CNcbiEnvironment& cont);

NCBI_XCGI_EXPORT
extern CNcbiIstream& 
ReadEnvironment(CNcbiIstream& is, CNcbiEnvironment& cont);


END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.3  2005/05/18 13:27:55  didenko
* Added NCBI_XCGI_EXPORT to the new functions
*
* Revision 1.2  2005/05/17 19:49:50  didenko
* Added Read/Write cookies and environment
*
* Revision 1.1  2005/04/25 20:01:59  didenko
* Added Cgi Entries serialization
*
* ===========================================================================
*/

#endif  /* CGI___CGI_SERIAL__HPP */
