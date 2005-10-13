#ifndef CGI_UTIL__HPP
#define CGI_UTIL__HPP

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
* Author:  Aleksey Grichenko
*
* File Description:
*   NCBI C++ CGI utils
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <map>


BEGIN_NCBI_SCOPE

/// URL encode flags
enum EUrlEncode {
    eUrlEncode_None,             ///< Do not encode/decode string
    eUrlEncode_SkipMarkChars,    ///< Do not convert chars like '!', '(' etc.
    eUrlEncode_ProcessMarkChars, ///< Convert all non-alphanum chars,
                                 ///< spaces are converted to '+'
    eUrlEncode_PercentOnly,      ///< Convert all non-alphanum chars including
                                 ///< space and '%' to %## format
    eUrlEncode_Path              ///< Same as ProcessMarkChars but preserves
                                 ///< valid path characters ('/', '.')
};


/// Decode the URL-encoded string "str";  return the result of decoding
/// If "str" format is invalid then throw CParseException
NCBI_XCGI_EXPORT
extern string
URL_DecodeString(const string& str,
                 EUrlEncode    encode_flag = eUrlEncode_SkipMarkChars);

/// URL-decode string "str" into itself
/// Return 0 on success;  otherwise, return 1-based error position
NCBI_XCGI_EXPORT
extern SIZE_TYPE
URL_DecodeInPlace(string& str, bool percent_only = false);

/// URL-encode a string "str" to the "x-www-form-urlencoded" form;
/// return the result of encoding. If 
NCBI_XCGI_EXPORT
extern string
URL_EncodeString(const      string& str,
                 EUrlEncode encode_flag = eUrlEncode_SkipMarkChars);


///////////////////////////////////////////////////////
///
/// IUrlEncoder::
///
/// URL parts encoder/decoder interface. Used by CUrl.
///

class IUrlEncoder
{
public:
    virtual ~IUrlEncoder(void) {}
    virtual string EncodeUser(const string& user) const = 0;
    virtual string DecodeUser(const string& user) const = 0;
    virtual string EncodePassword(const string& password) const = 0;
    virtual string DecodePassword(const string& password) const = 0;
    virtual string EncodePath(const string& path) const = 0;
    virtual string DecodePath(const string& path) const = 0;
    virtual string EncodeArgName(const string& name) const = 0;
    virtual string DecodeArgName(const string& name) const = 0;
    virtual string EncodeArgValue(const string& value) const = 0;
    virtual string DecodeArgValue(const string& value) const = 0;
};


class NCBI_XCGI_EXPORT CEmptyUrlEncoder : public IUrlEncoder
{
public:
    virtual string EncodeUser(const string& user) const;
    virtual string DecodeUser(const string& user) const;
    virtual string EncodePassword(const string& password) const;
    virtual string DecodePassword(const string& password) const;
    virtual string EncodePath(const string& path) const;
    virtual string DecodePath(const string& path) const;
    virtual string EncodeArgName(const string& name) const;
    virtual string DecodeArgName(const string& name) const;
    virtual string EncodeArgValue(const string& value) const;
    virtual string DecodeArgValue(const string& value) const;
};


class NCBI_XCGI_EXPORT CDefaultUrlEncoder : public CEmptyUrlEncoder
{
public:
    /// Default encoder uses the selected encoding for argument names/values
    /// and eUrlEncode_Path for document path. Other parts of the URL are
    /// not encoded.
    CDefaultUrlEncoder(EUrlEncode encode);
    virtual string EncodePath(const string& path) const;
    virtual string DecodePath(const string& path) const;
    virtual string EncodeArgName(const string& name) const;
    virtual string DecodeArgName(const string& name) const;
    virtual string EncodeArgValue(const string& value) const;
    virtual string DecodeArgValue(const string& value) const;
private:
    EUrlEncode m_Encode;
};


///////////////////////////////////////////////////////
///
/// CCgiArgs::
///
/// CGI arguments parser.
///

class NCBI_XCGI_EXPORT CCgiArgs
{
public:
    CCgiArgs(void);
    CCgiArgs(const string& query, EUrlEncode decode);
    CCgiArgs(const string& query, const IUrlEncoder* encoder = 0);

    void SetQueryString(const string& query, EUrlEncode encode);
    void SetQueryString(const string& query,
                        const IUrlEncoder* encoder = 0);

    string GetQueryString(EUrlEncode encode) const;
    string GetQueryString(const IUrlEncoder* encoder = 0) const;

    struct SCgiArg
    {
        SCgiArg(string aname, string avalue)
            : name(aname), value(avalue) { }
        string name;
        string value;
    };
    typedef SCgiArg    TArg;
    typedef list<TArg> TArgs;

    string GetValue(const string& name) const;
    void SetValue(const string& name, const string value);
    TArgs& GetArgs(void);

private:
    void x_SetIndexString(const string& query,
                          const IUrlEncoder& encoder);
    TArgs::iterator x_Find(const string& name);

    bool  m_IsIndex;
    TArgs m_Args;
};


///////////////////////////////////////////////////////
///
/// CUrl::
///
/// URL parser. Uses CCgiArgs to parse arguments.
///

class NCBI_XCGI_EXPORT CUrl
{
public:
    CUrl(void);
    /// Parse the URL.
    ///
    /// @param url
    ///    string to parse as URL:
    ///    Generic: [scheme://[user[:password]@]]host[:port][/path][?args]
    ///    Special: scheme:[path]
    ///    The leading '/', if any, is included in path value.
    /// @param encode_flag
    ///    decode the string before parsing according to the flag
    CUrl(const string& url, const IUrlEncoder* encoder = 0);

    /// Parse the URL.
    ///
    /// @param url
    ///    string to parse as URL
    /// @param encode_flag
    ///    decode the string before parsing according to the flag
    void SetUrl(const string& url, const IUrlEncoder* encoder = 0);

    /// Compose the URL
    /// @param encode_flag
    ///    encode the URL parts before composing the URL
    string ComposeUrl(const IUrlEncoder* encoder = 0) const;

    // Access parts of the URL
    string GetScheme(void) const;
    void SetScheme(const string& value);

    string GetUser(void) const;
    void SetUser(const string& value);

    string GetPassword(void) const;
    void SetPassword(const string& value);

    string GetHost(void) const;
    void SetHost(const string& value);

    string GetPort(void) const;
    void SetPort(const string& value);

    string GetPath(void) const;
    void SetPath(const string& value);

    /// Get the original (unparsed and undecoded) CGI arguments
    string GetOriginalArgsString(void) const;
    /// Get list of arguments
    bool HaveArgs(void) const;
    const CCgiArgs& GetArgs(void) const;
    CCgiArgs& GetArgs(void);

    CUrl(const CUrl& url);
    CUrl& operator=(const CUrl& url);

private:
    // Set values with verification
    void x_SetScheme(const string& scheme, const IUrlEncoder& encoder);
    void x_SetUser(const string& user, const IUrlEncoder& encoder);
    void x_SetPassword(const string& password, const IUrlEncoder& encoder);
    void x_SetHost(const string& host, const IUrlEncoder& encoder);
    void x_SetPort(const string& port, const IUrlEncoder& encoder);
    void x_SetPath(const string& path, const IUrlEncoder& encoder);
    void x_SetArgs(const string& args, const IUrlEncoder& encoder);

    string     m_Scheme;
    bool       m_IsGeneric;  // generic schemes include '//' delimiter
    string     m_User;
    string     m_Password;
    string     m_Host;
    string     m_Port;
    string     m_Path;
    string     m_OrigArgs;
    auto_ptr<CCgiArgs> m_ArgsList;
};


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////
//  CCgiArgs::
//

inline
string CCgiArgs::GetValue(const string& name) const
{
    return const_cast<CCgiArgs&>(*this).x_Find(name)->value;
}


inline
CCgiArgs::TArgs& CCgiArgs::GetArgs(void)
{
    return m_Args;
}


inline
const CCgiArgs& CUrl::GetArgs(void) const
{
    return *m_ArgsList;
}


inline
CCgiArgs& CUrl::GetArgs(void)
{
    return *m_ArgsList;
}


///////////////////////////////////////////////////////
//  CUrl::
//

inline
string CUrl::GetScheme(void) const
{
    return m_Scheme;
}


inline
string CUrl::GetUser(void) const
{
    return m_User;
}


inline
string CUrl::GetPassword(void) const
{
    return m_Password;
}


inline
string CUrl::GetHost(void) const
{
    return m_Host;
}


inline
string CUrl::GetPort(void) const
{
    return m_Port;
}


inline
string CUrl::GetPath(void) const
{
    return m_Path;
}


inline
bool CUrl::HaveArgs(void) const
{
    return m_ArgsList.get() != 0;
}


inline
string CUrl::GetOriginalArgsString(void) const
{
    return m_OrigArgs;
}


END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.1  2005/10/13 15:42:47  grichenk
* Initial revision
*
*
* ==========================================================================
*/

#endif  /* CGI_UTIL__HPP */
