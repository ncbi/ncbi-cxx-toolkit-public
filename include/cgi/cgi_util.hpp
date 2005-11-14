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
#include <memory>


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

enum EUrlDecode {
    eUrlDecode_All,
    eUrlDecode_Percent
};
/// URL-decode string "str" into itself
/// Return 0 on success;  otherwise, return 1-based error position
NCBI_XCGI_EXPORT
extern SIZE_TYPE
URL_DecodeInPlace(string& str, EUrlDecode decode_flag = eUrlDecode_All);

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
    /// Encode user name
    virtual string EncodeUser(const string& user) const = 0;
    /// Decode user name
    virtual string DecodeUser(const string& user) const = 0;
    /// Encode password
    virtual string EncodePassword(const string& password) const = 0;
    /// Decode password
    virtual string DecodePassword(const string& password) const = 0;
    /// Encode path on server
    virtual string EncodePath(const string& path) const = 0;
    /// Decode path on server
    virtual string DecodePath(const string& path) const = 0;
    /// Encode CGI argument name
    virtual string EncodeArgName(const string& name) const = 0;
    /// Decode CGI argument name
    virtual string DecodeArgName(const string& name) const = 0;
    /// Encode CGI argument value
    virtual string EncodeArgValue(const string& value) const = 0;
    /// Decode CGI argument value
    virtual string DecodeArgValue(const string& value) const = 0;
};


/// Primitive encoder - all methods return the argument value.
/// Used as base class for other encoders.
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


/// Default encoder, uses the selected encoding for argument names/values
/// and eUrlEncode_Path for document path. Other parts of the URL are
/// not encoded.
class NCBI_XCGI_EXPORT CDefaultUrlEncoder : public CEmptyUrlEncoder
{
public:
    CDefaultUrlEncoder(EUrlEncode encode = eUrlEncode_SkipMarkChars);
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
/// CCgiArgs_Parser::
///
/// CGI base class for arguments parsers.
///

class NCBI_XCGI_EXPORT CCgiArgs_Parser
{
public:
    virtual ~CCgiArgs_Parser(void) {}

    /// Parse query string, call AddArgument() to store each value.
    void SetQueryString(const string& query, EUrlEncode encode);
    /// Parse query string, call AddArgument() to store each value.
    void SetQueryString(const string& query,
                        const IUrlEncoder* encoder = 0);

protected:
    /// Query type flag
    enum EArgType {
        eArg_Value, ///< Query contains name=value pairs
        eArg_Index  ///< Query contains a list of names: name1+name2+name3
    };
    /// Process next query argument. Must be overriden to process and store
    /// the arguments.
    /// @param position
    ///   1-based index of the argument in the query
    /// @param name
    ///   Name of the argument.
    /// @param value
    ///   Contains argument value if query type is eArg_Value or
    ///   empty string for eArg_Index.
    /// @param arg_type
    ///   Query type flag.
    virtual void AddArgument(unsigned int position,
                             const string& name,
                             const string& value,
                             EArgType arg_type = eArg_Index) = 0;

private:
    void x_SetIndexString(const string& query,
                          const IUrlEncoder& encoder);
};


///////////////////////////////////////////////////////
///
/// CCgiArgs::
///
/// CGI arguments list.
///

class NCBI_XCGI_EXPORT CCgiArgs : public CCgiArgs_Parser
{
public:
    /// Create an empty arguments set.
    CCgiArgs(void);
    /// Parse the query string, store the arguments.
    CCgiArgs(const string& query, EUrlEncode decode);
    /// Parse the query string, store the arguments.
    CCgiArgs(const string& query, const IUrlEncoder* encoder = 0);

    /// Ampersand encoding for composed URLs
    enum EAmpEncoding {
        eAmp_Char,   ///< Use & to separate arguments
        eAmp_Entity  ///< Encode '&' as "&amp;"
    };

    /// Construct and return complete query string. Use selected amp
    /// and name/value encodings.
    string GetQueryString(EAmpEncoding amp_enc,
                          EUrlEncode encode) const;
    /// Construct and return complete query string. Use selected amp
    /// and name/value encodings.
    string GetQueryString(EAmpEncoding amp_enc,
                          const IUrlEncoder* encoder = 0) const;

    /// Name-value pair.
    struct SCgiArg
    {
        SCgiArg(string aname, string avalue)
            : name(aname), value(avalue) { }
        string name;
        string value;
    };
    typedef SCgiArg    TArg;
    typedef list<TArg> TArgs;

    /// Check if an argument with the given name exists.
    bool IsSetValue(const string& name) const;
    /// Get value for the given name. Undefined behaviour if the name
    /// does not exist.
    const string& GetValue(const string& name) const;
    /// Set new value for the existing argument or add a new argument.
    void SetValue(const string& name, const string value);
    /// Get the const list of arguments.
    const TArgs& GetArgs(void) const;
    /// Get the list of arguments.
    TArgs& GetArgs(void);

    /// Select case sensitivity of arguments' names.
    void SetCase(NStr::ECase name_case);

protected:
    virtual void AddArgument(unsigned int position,
                             const string& name,
                             const string& value,
                             EArgType arg_type);

private:
    TArgs::iterator x_Find(const string& name);
    TArgs::const_iterator x_Find(const string& name) const;

    NStr::ECase m_Case;
    bool        m_IsIndex;
    TArgs       m_Args;
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
    string ComposeUrl(CCgiArgs::EAmpEncoding amp_enc,
                      const IUrlEncoder* encoder = 0) const;

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

    /// Get the original (unparsed and undecoded) CGI query string
    string GetOriginalArgsString(void) const;

    /// Check if the URL contains any arguments
    bool HaveArgs(void) const;
    /// Get const list of arguments
    const CCgiArgs& GetArgs(void) const;
    /// Get list of arguments
    CCgiArgs& GetArgs(void);

    CUrl(const CUrl& url);
    CUrl& operator=(const CUrl& url);

    /// Return default URL encoder
    /// @sa CDefaultUrlEncoder
    static IUrlEncoder* GetDefaultEncoder(void);

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
bool CCgiArgs::IsSetValue(const string& name) const
{
    return x_Find(name) != m_Args.end();
}


inline
const string& CCgiArgs::GetValue(const string& name) const
{
    return x_Find(name)->value;
}


inline
CCgiArgs::TArgs& CCgiArgs::GetArgs(void)
{
    return m_Args;
}


inline
const CCgiArgs::TArgs& CCgiArgs::GetArgs(void) const
{
    return m_Args;
}


inline
void CCgiArgs::SetCase(NStr::ECase name_case)
{
    m_Case = name_case;
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


END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.6  2005/11/14 16:58:31  grichenk
* More Doxygen comments
*
* Revision 1.5  2005/11/08 20:30:04  grichenk
* Added ampersand encoding flag
*
* Revision 1.4  2005/10/17 20:47:01  grichenk
* +CCgiArgs::IsSetValue()
*
* Revision 1.3  2005/10/17 16:46:40  grichenk
* Added CCgiArgs_Parser base class.
* Redesigned CCgiRequest to use CCgiArgs_Parser.
* Replaced CUrlException with CCgiParseException.
*
* Revision 1.2  2005/10/14 16:15:36  ucko
* +<memory> for auto_ptr<>
*
* Revision 1.1  2005/10/13 15:42:47  grichenk
* Initial revision
*
*
* ==========================================================================
*/

#endif  /* CGI_UTIL__HPP */
