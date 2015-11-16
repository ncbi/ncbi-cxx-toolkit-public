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
 * Author:  Maxim Didenko
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
    COStreamHelper(CNcbiOstream& os) : m_Ostream(os), m_str(NULL) {}
    ~COStreamHelper() {  try { flush(); } catch (...) {}   }

    operator CNcbiOstream&() { return x_GetStrm(); }

    template<typename T>
    COStreamHelper& operator<<(const T& t)
    {
        x_GetStrm() << t;
        return *this;
    }

    void flush(bool write_empty_data = false) 
    {
        if (m_str.get() != NULL) {
            auto_ptr<CNcbiOstrstream> strm(m_str.release());
            string s = CNcbiOstrstreamToString(*strm);
            // Historically counted, but did not output, a final \0.
            m_Ostream << (s.size() + 1) << ' ' << s;
        } else if (write_empty_data)
            m_Ostream << 1 << ' ';
    }

private:
    CNcbiOstream& x_GetStrm() {
        if (m_str.get() == NULL) {
            m_str.reset(new CNcbiOstrstream);
        }
        return *m_str;
    }
    CNcbiOstream& m_Ostream;
    auto_ptr<CNcbiOstrstream> m_str;
    
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
    if (size > 0) {
        AutoPtr<char, ArrayDeleter<char> > buf(new char[size]);
        is.read(buf.get(), size);
        size_t count = (size_t)is.gcount();
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
        ostr << NStr::URLEncode(TKeyConverter::ToString(it->first)) << '='
             << NStr::URLEncode(TValueConverter::ToString(it->second));
    }
    ostr.flush(true);
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

    string input = ReadStringFromStream(is);

    vector<CTempString> pairs;
    NStr::Split(input, "&", pairs,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    cont.clear();
    ITERATE(vector<CTempString>, it, pairs) {
        CTempString key;
        CTempString value;
        NStr::SplitInTwo(*it, "=", key, value);
        cont.insert(typename TMap::value_type(
                    TKeyConverter::FromString(NStr::URLDecode(key)),
                    TValueConverter::FromString(NStr::URLDecode(value)))
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
        ostr << NStr::URLEncode(TValueConverter::ToString(*it));
    }
    ostr.flush(true);
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

    string input = ReadStringFromStream(is);

    vector<CTempString> strings;
    NStr::Split(input, "&", strings,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    cont.clear();
    ITERATE(vector<CTempString>, it, strings) {
        cont.push_back(TValueConverter::FromString(NStr::URLDecode(*it)));
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

#endif  /* CGI___CGI_SERIAL__HPP */
