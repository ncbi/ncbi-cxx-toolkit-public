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
 * Author: Eugene Vasilchenko, Denis Vakatov
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbienv.hpp>
#include <cgi/cgi_serial.hpp>


BEGIN_NCBI_SCOPE

CCgiEntry CContElemConverter<CCgiEntry>::FromString(const string& str)
{
    size_t pos = str.find('|');
    string ssize = str.substr(0,pos);
    size_t size = NStr::StringToUInt(ssize);
    string value = str.substr(pos+1,size);
    pos = pos + 1 + size;
    size_t pos1 = str.find('|', pos);
    ssize = str.substr(pos, pos1-pos);
    size = NStr::StringToUInt(ssize);
    pos1 = pos1+1+size;
    string fname = str.substr(pos1,size);
    pos = str.find('|', pos1);        
    ssize = str.substr(pos1, pos-pos1);
    size = NStr::StringToUInt(ssize);
    string type = str.substr(pos+1,size);
    ssize = str.substr(pos+1+size);
    unsigned int position = NStr::StringToUInt(ssize);
    return CCgiEntry(value,fname,position,type);
}

string CContElemConverter<CCgiEntry>::ToString  (const CCgiEntry&  elem)
{
    string ret = NStr::UIntToString(elem.GetValue().length()) + '|';
    ret += elem.GetValue();
    ret += NStr::UIntToString(elem.GetFilename().length()) + '|';
    ret += elem.GetFilename();
    ret += NStr::UIntToString(elem.GetContentType().length()) + '|';
    ret += elem.GetContentType();
    ret += NStr::UIntToString(elem.GetPosition());
    return ret;
}


//////////////////////////////////////////////////////////////////////////////
/// 

CNcbiOstream& WriteCgiCookies(CNcbiOstream& os, const CCgiCookies& cont)
{
    CNcbiOstrstream ostr;
    cont.Write(ostr, CCgiCookie::eHTTPRequest);
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

CNcbiIstream& ReadCgiCookies(CNcbiIstream& is, CCgiCookies& cont)
{
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
    cont.Clear();
    cont.Add(str);
    return is;
}

//////////////////////////////////////////////////////////////////////////////
/// 
CNcbiOstream& WriteEnvironment(CNcbiOstream& os, const CNcbiEnvironment& cont)
{
    list<string> names;
    cont.Enumerate(names);
    map<string,string> vars;
    ITERATE(list<string>, it, names) {
        vars[*it] = cont.Get(*it);
    }
    WriteMap(os, vars);
    return os;
}
CNcbiIstream& ReadEnvironment(CNcbiIstream& is, CNcbiEnvironment& cont)
{
    typedef map<string,string> TVars;
    TVars vars;
    cont.Reset();
    ReadMap(is, vars);
    ITERATE(TVars, it, vars) {
        cont.Set(it->first, it->second);
    }
    return is;
}


END_NCBI_SCOPE

/*
 * =========================================================================== 
 * $Log$
 * Revision 1.1  2005/05/17 19:49:50  didenko
 * Added Read/Write cookies and environment
 *
 * ===========================================================================
 */
