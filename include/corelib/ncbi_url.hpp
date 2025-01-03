#ifndef CORELIB___NCBI_URL__HPP
#define CORELIB___NCBI_URL__HPP

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
 * Authors: Alexey Grichenko, Vladimir Ivanov
 *
 * File Description:   URL parsing classes
 *
 */

/// @file ncbi_url.hpp
///
/// URL parsing classes.
///

#include <corelib/ncbi_param.hpp>

/** @addtogroup UTIL
 *
 * @{
 */

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
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
    /// Encode URL argument name
    virtual string EncodeArgName(const string& name) const = 0;
    /// Decode URL argument name
    virtual string DecodeArgName(const string& name) const = 0;
    /// Encode URL argument value
    virtual string EncodeArgValue(const string& value) const = 0;
    /// Decode URL argument value
    virtual string DecodeArgValue(const string& value) const = 0;
    /// Encode fragment
    virtual string EncodeFragment(const string& value) const = 0;
    /// Decode fragment
    virtual string DecodeFragment(const string& value) const = 0;
};


/// Primitive encoder - all methods return the argument value.
/// Used as base class for other encoders.
class NCBI_XNCBI_EXPORT CEmptyUrlEncoder : public IUrlEncoder
{
public:
    virtual string EncodeUser(const string& user) const
        {  return user; }
    virtual string DecodeUser(const string& user) const
        {  return user; }
    virtual string EncodePassword(const string& password) const
        {  return password; }
    virtual string DecodePassword(const string& password) const
        {  return password; }
    virtual string EncodePath(const string& path) const
        {  return path; }
    virtual string DecodePath(const string& path) const
        {  return path; }
    virtual string EncodeArgName(const string& name) const
        {  return name; }
    virtual string DecodeArgName(const string& name) const
        {  return name; }
    virtual string EncodeArgValue(const string& value) const
        {  return value; }
    virtual string DecodeArgValue(const string& value) const
        {  return value; }
    virtual string EncodeFragment(const string& value) const
        {  return value; }
    virtual string DecodeFragment(const string& value) const
        {  return value; }
};


/// Default encoder, uses the selected encoding for argument names/values
/// and eUrlEncode_Path for document path. Other parts of the URL are
/// not encoded.
class NCBI_XNCBI_EXPORT CDefaultUrlEncoder : public CEmptyUrlEncoder
{
public:
    CDefaultUrlEncoder(NStr::EUrlEncode encode = NStr::eUrlEnc_SkipMarkChars)
        : m_Encode(NStr::EUrlEncode(encode)) { return; }
    virtual string EncodeUser(const string& user) const
        {  return NStr::URLEncode(user, NStr::eUrlEnc_URIUserinfo); }
    virtual string DecodeUser(const string& user) const
        {  return NStr::URLDecode(user, NStr::eUrlDec_Percent); }
    virtual string EncodePassword(const string& password) const
        {  return NStr::URLEncode(password, NStr::eUrlEnc_URIUserinfo); }
    virtual string DecodePassword(const string& password) const
        {  return NStr::URLDecode(password, NStr::eUrlDec_Percent); }
    virtual string EncodePath(const string& path) const
        { return NStr::URLEncode(path, NStr::eUrlEnc_URIPath); }
    virtual string DecodePath(const string& path) const
        { return NStr::URLDecode(path); }
    virtual string EncodeArgName(const string& name) const
        { return NStr::URLEncode(name, m_Encode); }
    virtual string DecodeArgName(const string& name) const
        { return NStr::URLDecode(name,
            m_Encode == NStr::eUrlEnc_PercentOnly ?
            NStr::eUrlDec_Percent : NStr::eUrlDec_All); }
    virtual string EncodeArgValue(const string& value) const
        { return NStr::URLEncode(value, m_Encode); }
    virtual string DecodeArgValue(const string& value) const
        { return NStr::URLDecode(value,
            m_Encode == NStr::eUrlEnc_PercentOnly ?
            NStr::eUrlDec_Percent : NStr::eUrlDec_All); }
    virtual string EncodeFragment(const string& value) const
        { return NStr::URLEncode(value, NStr::eUrlEnc_URIFragment); }
    virtual string DecodeFragment(const string& value) const
        { return NStr::URLDecode(value, NStr::eUrlDec_Percent); }
private:
    NStr::EUrlEncode m_Encode;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CUrlArgs_Parser::
///
/// Base class for arguments parsers.
///

class NCBI_XNCBI_EXPORT CUrlArgs_Parser
{
public:
    /// An inverted subset of CCgiRequest::TFlags
    /// @sa CCgiRequest
    typedef int TFlags;
    enum Flags {
        /// Treat semicolon as query string argument separator
        fSemicolonIsArgDelimiter    = (1 << 11),
        /// Enable parsing input as 'indexed' query (RFC3875) when no '=' is present
        fEnableParsingAsIndex       = (1 << 14),
    };

    CUrlArgs_Parser(TFlags flags = 0) : m_Flags(flags) {}
    virtual ~CUrlArgs_Parser(void) {}

    /// Parse query string, call AddArgument() to store each value.
    void SetQueryString(const string& query, NStr::EUrlEncode encode);
    /// Parse query string, call AddArgument() to store each value.
    void SetQueryString(const string& query,
                        const IUrlEncoder* encoder = 0);

    /// Treat semicolon as query string argument separator
    void SetSemicolonIsNotArgDelimiter(bool enable = true)
    {
        if (enable) {
            m_Flags &= ~fSemicolonIsArgDelimiter;
        } else {
            m_Flags |= fSemicolonIsArgDelimiter;
        }
    }

protected:
    /// Query type flag
    enum EArgType {
        eArg_Value, ///< Query contains name=value pairs
        eArg_Index  ///< Query contains a list of names: name1+name2+name3
    };

    /// Process next query argument. Must be overriden to process and store
    /// the arguments.
    /// @param position
    ///   1-based index of the argument in the query.
    /// @param name
    ///   Name of the argument.
    /// @param value
    ///   Contains argument value if query type is eArg_Value or
    ///   empty string for eArg_Index.
    /// @param arg_type
    ///   Query type flag.
    virtual void AddArgument(unsigned int  position,
                             const string& name,
                             const string& value,
                             EArgType      arg_type = eArg_Index) = 0;

    TFlags m_Flags;

private:
    void x_SetIndexString(const string& query,
                          const IUrlEncoder& encoder);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CUrlArgs::
///
/// URL arguments list.
///

class NCBI_XNCBI_EXPORT CUrlArgs : public CUrlArgs_Parser
{
public:
    /// Create an empty arguments set.
    CUrlArgs(TFlags flags = 0);
    /// Parse the query string, store the arguments.
    CUrlArgs(const string& query, NStr::EUrlEncode decode, TFlags flags = 0);
    /// Parse the query string, store the arguments.
    CUrlArgs(const string& query, const IUrlEncoder* encoder = 0, TFlags flags = 0);

    /// Clear the list.
    void clear(void) { m_IsIndex = false; m_Args.clear(); }

    /// Ampersand encoding for composed URLs
    enum EAmpEncoding {
        eAmp_Char,   ///< Use & to separate arguments
        eAmp_Entity  ///< Encode '&' as "&amp;"
    };

    /// Construct and return complete query string. Use selected amp
    /// and name/value encodings.
    string GetQueryString(EAmpEncoding amp_enc,
                          NStr::EUrlEncode encode) const;
    /// Construct and return complete query string. Use selected amp
    /// and name/value encodings.
    string GetQueryString(EAmpEncoding amp_enc,
                          const IUrlEncoder* encoder = 0) const;

    /// Name-value pair.
    struct SUrlArg
    {
        SUrlArg(const string& aname, const string& avalue)
            : name(aname), value(avalue) { }
        string name;
        string value;
    };
    typedef SUrlArg               TArg;
    typedef list<TArg>            TArgs;
    typedef TArgs::iterator       iterator;
    typedef TArgs::const_iterator const_iterator;

    /// Check if an argument with the given name exists.
    bool IsSetValue(const string& name) const
        { return FindFirst(name) != m_Args.end(); }

    /// Get value for the given name. finds first of the arguments with the
    /// given name. If the name does not exist, is_found is set to false.
    /// If is_found is null, CUrlArgsException is thrown.
    const string& GetValue(const string& name, bool* is_found = 0) const;

    /// Set new value for the first argument with the given name or
    /// add a new argument.
    void SetValue(const string& name, const string& value);

    /// Add new value even if an argument with the same name already exists.
    void AddValue(const string& name, const string& value);

    /// Set value, remove any other values for the name.
    void SetUniqueValue(const string& name, const string& value);

    /// Get the const list of arguments.
    const TArgs& GetArgs(void) const 
        { return m_Args; }

    /// Get the list of arguments.
    TArgs& GetArgs(void) 
        { return m_Args; }

    /// Find the first argument with the given name. If not found, return
    /// GetArgs().end().
    iterator FindFirst(const string& name);

    /// Take argument name from the iterator, find next argument with the same
    /// name, return GetArgs().end() if not found.
    iterator FindNext(const iterator& iter);

    /// Find the first argument with the given name. If not found, return
    /// GetArgs().end().
    const_iterator FindFirst(const string& name) const;

    /// Take argument name from the iterator, find next argument with the same
    /// name, return GetArgs().end() if not found.
    const_iterator FindNext(const const_iterator& iter) const;

    /// Select case sensitivity of arguments' names.
    void SetCase(NStr::ECase name_case)
        { m_Case = name_case; }

protected:
    virtual void AddArgument(unsigned int  position,
                             const string& name,
                             const string& value,
                             EArgType      arg_type);
private:
    iterator x_Find(const string& name, const iterator& start);
    const_iterator x_Find(const string& name,
                          const const_iterator& start) const;

    NStr::ECase m_Case;
    bool        m_IsIndex;
    TArgs       m_Args;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CUrl::
///
/// URL parser. Uses CUrlArgs to parse arguments.
///

#define NCBI_SCHEME_SERVICE "ncbilb"

class NCBI_XNCBI_EXPORT CUrl
{
public:
    /// Default constructor
    CUrl(void);

    /// Parse the URL.
    ///
    /// @param url
    ///   String to parse as URL:
    ///   Generic:
    ///      [scheme://[[user]:[password]@]]host[:port][/path][?args][#frag]
    ///   Special:
    ///      scheme:[path]
    ///   The leading '/', if any, is included in the path value.
    /// @param encoder
    ///   URL encoder object. If not set, the default encoder will be used.
    ///   @sa CDefaultUrlEncoder
    CUrl(const string& url, const IUrlEncoder* encoder = 0);

    CUrl(const char* url, const IUrlEncoder* encoder = 0)
        : CUrl(string(url), encoder)
    { }

    /// Parse the URL.
    ///
    /// @param url
    ///   String to parse as URL
    /// @param encoder
    ///   URL encoder object. If not set, the default encoder will be used.
    ///   @sa CDefaultUrlEncoder
    void SetUrl(const string& url, const IUrlEncoder* encoder = 0);

    /// Compose the URL.
    ///
    /// @param amp_enc
    ///   Method of encoding ampersand.
    ///   @sa CUrlArgs::EAmpEncoding
    /// @param encoder
    ///   URL encoder object. If not set, the default encoder will be used.
    ///   @sa CDefaultUrlEncoder
    string ComposeUrl(CUrlArgs::EAmpEncoding amp_enc,
                      const IUrlEncoder* encoder = 0) const;

    // Access parts of the URL

    const string& GetScheme(void) const             { return m_Scheme; }
    void          SetScheme(const string& value);

    /// Generic schemes use '//' prefix (after optional scheme).
    bool          GetIsGeneric(void) const          { return m_IsGeneric; }
    void          SetIsGeneric(bool value)          { m_IsGeneric = value; }

    const string& GetUser(void) const               { return m_User; }
    void          SetUser(const string& value)      { m_User = value; }

    const string& GetPassword(void) const           { return m_Password; }
    void          SetPassword(const string& value)  { m_Password = value; }

    const string& GetHost(void) const               { return m_Host; }
    void          SetHost(const string& value);

    bool          IsService(void) const             {return!m_Service.empty();}
    const string& GetService(void) const            { return m_Service; }
    void          SetService(const string& value);
    
    const string& GetPort(void) const               { return m_Port; }
    void          SetPort(const string& value)      { m_Port = value; }

    const string& GetPath(void) const               { return m_Path; }
    void          SetPath(const string& value)      { m_Path = value; }

    const string& GetFragment(void) const           { return m_Fragment; }
    void          SetFragment(const string& value)  { m_Fragment = value; }

    /// Get the original (unparsed and undecoded) query string
    const string& GetOriginalArgsString(void) const { return m_OrigArgs; }

    /// Check if the URL contains any arguments
    bool HaveArgs(void) const
        { return m_ArgsList.get() != 0  &&  !m_ArgsList->GetArgs().empty(); }

    /// Get const list of arguments
    const CUrlArgs& GetArgs(void) const;

    /// Get list of arguments
    CUrlArgs& GetArgs(void);

    CUrl(const CUrl& url);
    CUrl& operator=(const CUrl& url);

    /// Return default URL encoder.
    ///
    /// @sa CDefaultUrlEncoder
    static IUrlEncoder* GetDefaultEncoder(void);

    bool IsEmpty(void) const;

    /// Flags controlling URL adjustment.
    /// @sa CUrl::Adjust
    enum EAdjustFlags {
        fUser_Replace            = 0x0001, ///< Replace user if set in 'other'
        fUser_ReplaceIfEmpty     = 0x0002, ///< Replace user only if not yet set
        fPassword_Replace        = 0x0004, ///< Replace password if set in 'other'
        fPassword_ReplaceIfEmpty = 0x0008, ///< Replace password only if not yet set
        fPath_Replace            = 0x0010, ///< Replace path
        fPath_Append             = 0x0020, ///< Append new path to the existing one
        fFragment_Replace        = 0x0040, ///< Replace fragment if set in 'other'
        fFragment_ReplaceIfEmpty = 0x0080, ///< Replace fragment only if not yet set
        fArgs_Replace            = 0x0100, ///< Discard all args, replace with args from 'other'
        fArgs_Append             = 0x0200, ///< Append args, allow duplicate names and values
        fArgs_Merge              = 0x0400, ///< Append new args; replace values of existing args,
                                           ///< do not allow to set multiple values with the same name
        fScheme_Replace          = 0x0800  ///< Replace scheme if set in 'other'
    };
    typedef int TAdjustFlags;

    /// Adjust this URL using information from 'other' URL.
    /// Host and port are never changed. Other parts can be replaced or merged
    /// depending on the flags.
    /// Throw CUrlException if the flags are inconsistent (e.g. both fPath_Replace and fPath_Append are set).
    void Adjust(const CUrl& other, TAdjustFlags flags);

private:
    // Set values with verification
    void x_SetScheme(const string& scheme, const IUrlEncoder& encoder);
    void x_SetUser(const string& user, const IUrlEncoder& encoder);
    void x_SetPassword(const string& password, const IUrlEncoder& encoder);
    void x_SetHost(const string& host, const IUrlEncoder& encoder);
    void x_SetService(const string& service);
    void x_SetPort(const string& port, const IUrlEncoder& encoder);
    void x_SetPath(const string& path, const IUrlEncoder& encoder);
    void x_SetArgs(const string& args, const IUrlEncoder& encoder);
    void x_SetFragment(const string& fragment, const IUrlEncoder& encoder);

    bool x_IsHostPort(const string& scheme, string& unparsed, const IUrlEncoder& encoder);

    string  m_Scheme;
    bool    m_IsGeneric;  // generic schemes include '//' delimiter
    string  m_User;
    string  m_Password;
    string  m_Host;
    string  m_Service;
    string  m_Port;
    string  m_Path;
    string  m_Fragment;
    string  m_OrigArgs;
    unique_ptr<CUrlArgs> m_ArgsList;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CUrlException --
///
///   Exceptions to be used by CUrl.
///

class CUrlException : public CException
{
public:
    enum EErrCode {
        eName,       //< Argument does not exist
        eNoArgs,     //< CUrl contains no arguments
        eFlags       //< Inconsistent flags passed to Adjust()
    };
    virtual const char* GetErrCodeString(void) const override
    {
        switch ( GetErrCode() ) {
        case eName:    return "Unknown argument name";
        case eNoArgs:  return "Arguments list is empty";
        case eFlags:   return "Inconsistent flags set";
        default:       return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CUrlException, CException);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CUrlParserException --
///
///   Exceptions used by the URL parser

class CUrlParserException : public CParseTemplException<CUrlException>
{
public:
    enum EErrCode {
        eFormat    //< Invalid URL format
    };

    virtual const char* GetErrCodeString(void) const override
    {
        switch (GetErrCode()) {
        case eFormat:    return "Url format error";
        default:        return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT2
    (CUrlParserException, CParseTemplException<CUrlException>,
     std::string::size_type);
};


//////////////////////////////////////////////////////////////////////////////
//
// Inline functions
//
//////////////////////////////////////////////////////////////////////////////


// CUrl

inline
void CUrl::SetHost(const string& host)
{
    m_Service.clear();
    m_Host = host;
}

inline
void CUrl::SetService(const string& service)
{
    m_Host.clear();
    m_Service = service;
    m_IsGeneric = true; // services are always generic
}

inline
void CUrl::x_SetScheme(const string& scheme,
                       const IUrlEncoder& /*encoder*/)
{
    m_Scheme = scheme;
}

inline
void CUrl::x_SetUser(const string& user,
                     const IUrlEncoder& encoder)
{
    m_User = encoder.DecodeUser(user);
}

inline
void CUrl::x_SetPassword(const string& password,
                         const IUrlEncoder& encoder)
{
    m_Password = encoder.DecodePassword(password);
}

inline
void CUrl::x_SetHost(const string& host,
                     const IUrlEncoder& /*encoder*/)
{
    m_Host = host;
    m_Service.clear();
}

inline
void CUrl::x_SetService(const string& service)
{
    m_Service = NStr::URLDecode(service);
}

inline
void CUrl::x_SetPort(const string& port,
                     const IUrlEncoder& /*encoder*/)
{
    NStr::StringToInt(port);
    m_Port = port;
}

inline
void CUrl::x_SetPath(const string& path,
                     const IUrlEncoder& encoder)
{
    m_Path = encoder.DecodePath(path);
}

inline
void CUrl::x_SetFragment(const string& fragment,
                         const IUrlEncoder& encoder)
{
    m_Fragment = encoder.DecodeFragment(fragment);
}

inline
void CUrl::x_SetArgs(const string& args,
                     const IUrlEncoder& encoder)
{
    m_OrigArgs = args;
    m_ArgsList.reset(new CUrlArgs(m_OrigArgs, &encoder));
}


inline
CUrlArgs& CUrl::GetArgs(void)
{
    if ( !m_ArgsList.get() ) {
        x_SetArgs(kEmptyStr, *GetDefaultEncoder());
    }
    return *m_ArgsList;
}


inline
CUrlArgs::const_iterator CUrlArgs::FindFirst(const string& name) const
{
    return x_Find(name, m_Args.begin());
}


inline
CUrlArgs::iterator CUrlArgs::FindFirst(const string& name)
{
    return x_Find(name, m_Args.begin());
}


inline
CUrlArgs::const_iterator CUrlArgs::FindNext(const const_iterator& iter) const
{
    const_iterator next = iter;
    ++next;
    return x_Find(iter->name, next);
}


inline
CUrlArgs::iterator CUrlArgs::FindNext(const iterator& iter)
{
    iterator next = iter;
    ++next;
    return x_Find(iter->name, next);
}

/* @} */

END_NCBI_SCOPE

#endif  /* CORELIB___NCBI_URL__HPP */
