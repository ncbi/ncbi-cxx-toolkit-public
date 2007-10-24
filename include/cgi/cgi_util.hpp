#ifndef CGI___CGI_UTIL__HPP
#define CGI___CGI_UTIL__HPP

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
 */

/// @file cont_util.hpp
///
/// CGI related utility classes and functions.
///

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/version.hpp>

#include <map>
#include <memory>

/** @addtogroup CGI
 *
 * @{
 */

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

/// URL decode flags
enum EUrlDecode {
    eUrlDecode_All,
    eUrlDecode_Percent
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
URL_DecodeInPlace(string& str, EUrlDecode decode_flag = eUrlDecode_All);

/// URL-encode a string "str" to the "x-www-form-urlencoded" form;
/// return the result of encoding. If 
NCBI_XCGI_EXPORT
extern string
URL_EncodeString(const      string& str,
                 EUrlEncode encode_flag = eUrlEncode_SkipMarkChars);



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
};


/// Default encoder, uses the selected encoding for argument names/values
/// and eUrlEncode_Path for document path. Other parts of the URL are
/// not encoded.
class NCBI_XCGI_EXPORT CDefaultUrlEncoder : public CEmptyUrlEncoder
{
public:
    CDefaultUrlEncoder(EUrlEncode encode = eUrlEncode_SkipMarkChars)
        : m_Encode(encode) { return; }
    virtual string EncodePath(const string& path) const
        { return URL_EncodeString(path, eUrlEncode_Path); }
    virtual string DecodePath(const string& path) const
        { return URL_DecodeString(path, eUrlEncode_Path); }
    virtual string EncodeArgName(const string& name) const
        { return URL_EncodeString(name, m_Encode); }
    virtual string DecodeArgName(const string& name) const
        { return URL_DecodeString(name, m_Encode); }
    virtual string EncodeArgValue(const string& value) const
        { return URL_EncodeString(value, m_Encode); }
    virtual string DecodeArgValue(const string& value) const
        { return URL_DecodeString(value, m_Encode); }
private:
    EUrlEncode m_Encode;
};



/////////////////////////////////////////////////////////////////////////////
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
private:
    void x_SetIndexString(const string& query,
                          const IUrlEncoder& encoder);
};


/////////////////////////////////////////////////////////////////////////////
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
        SCgiArg(const string& aname, const string& avalue)
            : name(aname), value(avalue) { }
        string name;
        string value;
    };
    typedef SCgiArg               TArg;
    typedef list<TArg>            TArgs;
    typedef TArgs::iterator       iterator;
    typedef TArgs::const_iterator const_iterator;

    /// Check if an argument with the given name exists.
    bool IsSetValue(const string& name) const
        { return FindFirst(name) != m_Args.end(); }

    /// Get value for the given name. finds first of the arguments with the
    /// given name. If the name does not exist, is_found is set to false.
    /// If is_found is null, CCgiArgsException is thrown.
    const string& GetValue(const string& name, bool* is_found = 0) const;

    /// Set new value for the first argument with the given name or
    /// add a new argument.
    void SetValue(const string& name, const string& value);

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
/// URL parser. Uses CCgiArgs to parse arguments.
///

class NCBI_XCGI_EXPORT CUrl
{
public:
    /// Default constructor
    CUrl(void);

    /// Parse the URL.
    ///
    /// @param url
    ///   String to parse as URL:
    ///   Generic: [scheme://[user[:password]@]]host[:port][/path][?args]
    ///   Special: scheme:[path]
    ///   The leading '/', if any, is included in path value.
    /// @param encode_flag
    ///   Decode the string before parsing according to the flag.
    CUrl(const string& url, const IUrlEncoder* encoder = 0);

    /// Parse the URL.
    ///
    /// @param url
    ///   String to parse as URL
    /// @param encode_flag
    ///   Decode the string before parsing according to the flag.
    void SetUrl(const string& url, const IUrlEncoder* encoder = 0);

    /// Compose the URL.
    ///
    /// @param encode_flag
    ///   Encode the URL parts before composing the URL.
    string ComposeUrl(CCgiArgs::EAmpEncoding amp_enc,
                      const IUrlEncoder* encoder = 0) const;

    // Access parts of the URL

    string GetScheme(void) const            { return m_Scheme; }
    void   SetScheme(const string& value)   { m_Scheme = value; }

    string GetUser(void) const              { return m_User; }
    void   SetUser(const string& value)     { m_User = value; }

    string GetPassword(void) const          { return m_Password; }
    void   SetPassword(const string& value) { m_Password = value; }
    
    string GetHost(void) const              { return m_Host; }
    void   SetHost(const string& value)     { m_Host = value; }
    
    string GetPort(void) const              { return m_Port; }
    void   SetPort(const string& value)     { m_Port = value; }

    string GetPath(void) const              { return m_Path; }
    void   SetPath(const string& value)     { m_Path = value; }

    string GetFragment(void) const          { return m_Fragment; }
    void   SetFragment(const string& value) { m_Fragment = value; }

    /// Get the original (unparsed and undecoded) CGI query string
    string GetOriginalArgsString(void) const
        { return m_OrigArgs; }

    /// Check if the URL contains any arguments
    bool HaveArgs(void) const
        { return m_ArgsList.get() != 0; }

    /// Get const list of arguments
    const CCgiArgs& GetArgs(void) const;

    /// Get list of arguments
    CCgiArgs& GetArgs(void);

    CUrl(const CUrl& url);
    CUrl& operator=(const CUrl& url);

    /// Return default URL encoder.
    ///
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
    void x_SetFragment(const string& fragment, const IUrlEncoder& encoder);

    string  m_Scheme;
    bool    m_IsGeneric;  // generic schemes include '//' delimiter
    string  m_User;
    string  m_Password;
    string  m_Host;
    string  m_Port;
    string  m_Path;
    string  m_Fragment;
    string  m_OrigArgs;
    auto_ptr<CCgiArgs> m_ArgsList;
};



/// User agent version info
typedef CVersionInfo TUserAgentVersion;

/////////////////////////////////////////////////////////////////////////////
///
/// CCgiUserAgent --
///
/// Define class to parse user agent strings.
/// Basicaly, support only Mozilla 'compatible' format.

class NCBI_XCGI_EXPORT CCgiUserAgent
{
public:
    /// Default constructor.
    /// Parse environment variable HTTP_USER_AGENT.
    CCgiUserAgent(void);

    /// Constructor.
    /// Parse the user agent string passed into the constructor.
    CCgiUserAgent(const string& user_agent);

    /// Parse new user agent string
    void Reset(const string& user_agent);

    /// Browser types.
    enum EBrowser {
        eUnknown = 0,           ///< Unknown user agent

        eIE,                    ///< Microsoft Internet Explorer (www.microsoft.com/windows/ie)
        eiCab,                  ///< iCab       (www.icab.de)
        eKonqueror,             ///< Konqueror  (www.konqueror.org)
        eLynx,                  ///< Lynx       (lynx.browser.org)
        eNetscape,              ///< Netscape (Navigator), versions >=6 are Gecko-based (www.netscape.com)
        eOpera,                 ///< Opera      (www.opera.com)
        eOregano,               ///< Oregano    (www.castle.org.uk/oregano/)
        eW3m,                   ///< w3m        (www.w3m.org)
        eNagios,                ///< check_http/nagios-plugins (nagiosplugins.org)

        // Gecko-based browsers
        eBeonex,                ///< Beonex Communicator (www.beonex.com)
        eCamino,                ///< Camino     (www.caminobrowser.org)
        eChimera,               ///< Chimera    (chimera.mozdev.org)
        eEpiphany,              ///< Epiphany   (www.gnome.org/projects/epiphany)
        eFirefox,               ///< Firefox    (www.mozilla.org/products/firefox)
        eFlock,                 ///< Flock      (www.flock.com)
        eGaleon,                ///< Gakeon     (galeon.sourceforge.net)
        eKMeleon,               ///< K-Meleon   (kmeleon.sf.net)
        eMadfox,                ///< Madfox     (www.splyb.com/madfox)
        eMinimo,                ///< Minimo     (www.mozilla.org/projects/minimo)
        eMultiZilla,            ///< MultiZilla (multizilla.mozdev.org)
        eSeaMonkey,             ///< SeaMonkey  (www.mozilla.org/projects/seamonkey)

        // IE-based
        eAvantBrowser,          ///< Avant Browser  (www.avantbrowser.com)
        eCrazyBrowser,          ///< Crazy Browser  (www.crazybrowser.com)
        eEnigmaBrowser,         ///< Enigma Browser (www.suttondesigns.com)
        eIRider,                ///< iRider         (www.irider.com)
        eMaxthon,               ///< Maxthon/MyIE2  (www.maxthon.com)
        eNetCaptor,             ///< NetCaptor      (www.netcaptor.com)

        // AppleWebKit/KHTML based
        eOmniWeb,               ///< OmniWeb     (www.omnigroup.com/applications/omniweb)
        eNetNewsWire,           ///< NetNewsWire (www.apple.com)
        eSafari,                ///< Safari      (www.apple.com/safari)
        eShiira,                ///< Shiira      (hmdt-web.net/shiira/en)

        /// Search robots/bots/validators
        eCrawler,               ///< Class: crawlers / search robots
        eOfflineBrowser,        ///< Class: offline browsers
        eScript,                ///< Class: script tools (perl/php/...)
        eLinkChecker,           ///< Class: link checkers
        eWebValidator,          ///< Class: validators

        /// Any other Gecko-based not from the list above,
        /// Mozilla version >= 5.0
        eMozilla,                ///< Mozilla/other Gecko-based (www.mozilla.com)

        /// Any other not from list above.
        /// User agent string starts with "Mozilla/x.x (compatible;*".
        /// Not Gecko-based.
        eMozillaCompatible      ///< Mozilla-compatible
    };

    /// Browser engine types.
    enum EBrowserEngine {
        eEngine_Unknown = eUnknown,     ///< Unknown engine
        eEngine_IE      = eIE,          ///< Microsoft Internet Explorer
        eEngine_Gecko   = eMozilla,     ///< Gecko-based
        eEngine_KHTML   = eSafari,      ///< Apple WebKit
        eEngine_Bot     = eCrawler      ///< Search robot/bot/checker/...
    };

    /// Platform types
    enum EBrowserPlatform {
        ePlatform_Unknown = eUnknown,   ///< Unknown OS
        ePlatform_Windows,              ///< Microsoft Windows
        ePlatform_Mac,                  ///< MacOS
        ePlatform_Unix                  ///< Unix
    };

    /// Get user agent string.
    string GetUserAgentStr(void) const
        { return m_UserAgent; }

    /// Get browser type.
    EBrowser GetBrowser(void) const
        { return m_Browser; }

    /// Get browser name.
    ///
    /// @return
    ///   Browser name or empty string for unknown browser
    /// @sa GetBrowser
    const string& GetBrowserName(void) const
        { return m_BrowserName; }

    /// Get browser engine type.
    /// @sa EBrowserEngine 
    EBrowserEngine GetEngine(void) const 
        { return m_Engine; }

    /// Get platform (OS) type.
    /// @sa EPlatform
    EBrowserPlatform GetPlatform(void) const 
        { return m_Platform; }

    /// Get browser version information.
    ///
    /// If version field (major, minor, patch level) equal -1 that
    /// it is not defined.
    const TUserAgentVersion& GetBrowserVersion(void) const
        { return m_BrowserVersion; }
    const TUserAgentVersion& GetEngineVersion(void) const
        { return m_EngineVersion; }
    const TUserAgentVersion& GetMozillaVersion(void) const
        { return m_MozillaVersion; }


    /// Bots check flags (what consider to be a bot).
    /// @sa EBrowser, EBrowserEngine
    enum EBotFlags {
        fBotCrawler         = (1<<1), 
        fBotOfflineBrowser  = (1<<2), 
        fBotScript          = (1<<3), 
        fBotLinkChecker     = (1<<4), 
        fBotWebValidator    = (1<<5), 
        fBotAll             = 0xFF
    };
    typedef unsigned int TBotFlags;    ///< Binary OR of "EBotFlags"

    /// Check that this is known search robot/bot.
    ///
    /// By default it use GetBrowser() value to check on known bots,
    /// and only here 'flags' parameter can be used. If standard check fails,
    /// additonal parsing parameters from string and/or registry/environment
    /// parameter (section 'CGI', name 'Bots') will be used.
    /// String value should have patterns for search in the user agent string,
    /// and should looks like:
    ///     "Googlebot Scooter WebCrawler Slurp"
    /// You can use any delimeters from next list " ;|~\t".
    /// All patterns are case sensitive.
    /// For details how to define registry/environment parameter see CParam
    /// description.
    /// @sa GetBrowser, GetEngine, CParam
    bool IsBot(TBotFlags flags = fBotAll, const string& patterns = kEmptyStr) const;

protected:
    /// Init class members.
    void x_Init(void);
    /// Parse user agent string.
    void x_Parse(const string& user_agent);
    /// Parse token with browser name and version.
    bool x_ParseToken(const string& token, int where);

protected:
    string            m_UserAgent;      ///< User-Agent string
    EBrowser          m_Browser;        ///< Browser type
    string            m_BrowserName;    ///< Browser name
    TUserAgentVersion m_BrowserVersion; ///< Browser version info
    EBrowserEngine    m_Engine;         ///< Browser engine type
    TUserAgentVersion m_EngineVersion;  ///< Browser engine version
    TUserAgentVersion m_MozillaVersion; ///< Browser mozilla version
    EBrowserPlatform  m_Platform;       ///< Platform type
};



//////////////////////////////////////////////////////////////////////////////
//
// Inline functions
//
//////////////////////////////////////////////////////////////////////////////


// CUrl

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
    m_Fragment = encoder.DecodePath(fragment);
}

inline
void CUrl::x_SetArgs(const string& args,
                     const IUrlEncoder& encoder)
{
    m_OrigArgs = args;
    m_ArgsList.reset(new CCgiArgs(m_OrigArgs, &encoder));
}


inline
CCgiArgs& CUrl::GetArgs(void)
{
    if ( !m_ArgsList.get() ) {
        x_SetArgs(kEmptyStr, *GetDefaultEncoder());
    }
    return *m_ArgsList;
}


inline
CCgiArgs::const_iterator CCgiArgs::FindFirst(const string& name) const
{
    return x_Find(name, m_Args.begin());
}


inline
CCgiArgs::iterator CCgiArgs::FindFirst(const string& name)
{
    return x_Find(name, m_Args.begin());
}


inline
CCgiArgs::const_iterator CCgiArgs::FindNext(const const_iterator& iter) const
{
    return x_Find(iter->name, iter);
}


inline
CCgiArgs::iterator CCgiArgs::FindNext(const iterator& iter)
{
    return x_Find(iter->name, iter);
}


END_NCBI_SCOPE

#endif  /* CGI___CGI_UTIL__HPP */
