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
/// template<> CContElemConverter<CCgiEntry>
///
/// The specialization of CContElemConverter for the CCgiEntry class.
///
template<>
class NCBI_XCGI_EXPORT CContElemConverter<CCgiEntry>
{
public:
    static CCgiEntry  FromString(const string& str);
    static string ToString  (const CCgiEntry&  elem);
};


///////////////////////////////////////////////////////
///
/// COStreamHelper
///
/// The helper class for storing a data into to a stream
/// in the form 
/// data_size data
///
class COStreamHelper
{
public:
    COStreamHelper(CNcbiOstream& os) : m_Ostream(os), m_flushed(true) {}
    ~COStreamHelper() {  try { flush(); } catch (...) {}   }

    operator CNcbiOstream&() { m_flushed = false; return m_str; }

    template<typename T>
    COStreamHelper& operator<<(const T& t)
    {
        m_flushed = false;
        m_str << t;
        return *this;
    }

    void flush() 
    {
        if (m_flushed)
            return;
        m_flushed = true;
        try {
            m_str << ends;
            m_Ostream << m_str.pcount() << ' ' << m_str.str();
        } catch (...) {
            m_str.freeze(false);
            throw;
        }
        m_str.freeze(false);
        m_str.flush();
    }

private:
    CNcbiOstrstream m_str;
    CNcbiOstream& m_Ostream;
    bool m_flushed;
    
}; 


///////////////////////////////////////////////////////
///
/// Read a string from a stream. The string is following 
/// 
inline string ReadStringFromStream(CNcbiIstream& is)
{
    string str;
    size_t size;
    is >> size;
    if (size > 0) {
        AutoPtr<char, ArrayDeleter<char> > buf(new char[size]);
        is.read(buf.get(), size);
        size_t count = is.gcount();
        if (count > 0)
            str.append(buf.get()+1, count-1);
    }
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

    COStreamHelper ostr(os);
    ITERATE(typename TMap, it, cont) {
        if (it != cont.begin())
            ostr << '&';
        ostr << URL_EncodeString(TKeyConverter  ::ToString(it->first)) << '='
             << URL_EncodeString(TValueConverter::ToString(it->second));
    }
    ostr.flush();
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

    string str = ReadStringFromStream(is);

    vector<string> pairs;
    NStr::Tokenize(str, "&", pairs);

    cont.clear();
    ITERATE(vector<string>, it, pairs) {
        string key;
        string value;
        NStr::SplitInTwo(*it, "=", key, value);
        cont.insert(TMap::value_type(
                    TKeyConverter::FromString(URL_DecodeString(key)),
                    TValueConverter::FromString(URL_DecodeString(value)))
                   );
    }

    return is;
}

///////////////////////////////////////////////////////
///
/// Write a container to a stream
/// 
template<typename TCont>
CNcbiOstream& WriteContainer(CNcbiOstream& os, const TCont& cont)
{
    typedef CContElemConverter<typename TCont::value_type> TValueConverter;
    COStreamHelper ostr(os);
    ITERATE(typename TCont, it, cont) {
        if (it != cont.begin())
            ostr << '&';
        ostr << URL_EncodeString(TValueConverter::ToString(*it));
    }
    ostr.flush();
    return os;
}

///////////////////////////////////////////////////////
///
/// Read a container from a stream
/// 
template<typename TCont>
CNcbiIstream& ReadContainer(CNcbiIstream& is, TCont& cont)
{
    typedef CContElemConverter<typename TCont::value_type> TValueConverter;

    string str = ReadStringFromStream(is);

    vector<string> vstrings;
    NStr::Tokenize(str, "&", vstrings);

    cont.clear();
    ITERATE(vector<string>, it, vstrings) {
        cont.push_back( TValueConverter::FromString(URL_DecodeString(*it)));
    }

    return is;
}


///////////////////////////////////////////////////////
///
/// Write cgi cookeis to a stream
/// 
NCBI_XCGI_EXPORT
extern CNcbiOstream& 
WriteCgiCookies(CNcbiOstream& os, const CCgiCookies& cont);

///////////////////////////////////////////////////////
///
/// Read cgi cookeis from a stream
/// 
NCBI_XCGI_EXPORT
extern CNcbiIstream& 
ReadCgiCookies(CNcbiIstream& is, CCgiCookies& cont);

///////////////////////////////////////////////////////
///
/// Write an environment to a stream
/// 
NCBI_XCGI_EXPORT
extern CNcbiOstream& 
WriteEnvironment(CNcbiOstream& os, const CNcbiEnvironment& cont);

///////////////////////////////////////////////////////
///
/// Write an environment from a stream
/// 
NCBI_XCGI_EXPORT
extern CNcbiIstream& 
ReadEnvironment(CNcbiIstream& is, CNcbiEnvironment& cont);

END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.5  2005/05/23 17:01:51  ucko
* Tweak ReadMap for compatibility with WorkShop's gratuitously strict STL.
*
* Revision 1.4  2005/05/23 15:02:02  didenko
* Added Read/Write a container from/to a stream
* Code restructure
*
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
