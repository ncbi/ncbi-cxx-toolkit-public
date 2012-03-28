#ifndef CGI___USER_AGENT__HPP
#define CGI___USER_AGENT__HPP

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
 * Authors: Vladimir Ivanov
 *
 */

/// @file user_agent.hpp
/// API to parse user agent strings.
///

#include <corelib/version.hpp>

/** @addtogroup CGI
 *
 * @{
 */

BEGIN_NCBI_SCOPE


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
    /// Comparison and parsing flags.
    enum EFlags {
        fNoCase  = (1 << 1)       ///< Case insensitive compare, by default it is case sensitive
    };
    typedef unsigned int TFlags;  ///< Binary OR of "EFlags"

    /// Default constructor.
    /// Parse environment variable HTTP_USER_AGENT.
    CCgiUserAgent(TFlags flags = 0);

    /// Constructor.
    /// Parse the user agent string passed into the constructor.
    CCgiUserAgent(const string& user_agent, TFlags flags = 0);

    /// Parse new user agent string
    void Reset(const string& user_agent);

    /// Browser types.
    enum EBrowser {
        eUnknown = 0,           ///< Unknown user agent

        eIE,                    ///< Microsoft Internet Explorer (www.microsoft.com/windows/ie)
        eiCab,                  ///< iCab       (www.icab.de)
        eKonqueror,             ///< Konqueror  (www.konqueror.org) (KHTML based since v3.2 ?)
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
        eFirefox,               ///< Firefox    (www.mozilla.org/products/firefox)
        eFlock,                 ///< Flock      (www.flock.com)
        eIceCat,                ///< GNU IceCat (http://www.gnu.org/software/gnuzilla)
        eIceweasel,             ///< Debian Iceweasel   (www.geticeweasel.org)
        eGaleon,                ///< Galeon     (galeon.sourceforge.net)
        eGranParadiso,          ///< GranParadiso (www.mozilla.org)
        eKazehakase,            ///< Kazehakase (kazehakase.sourceforge.jp)
        eKMeleon,               ///< K-Meleon   (kmeleon.sf.net)
        eKNinja,                ///< K-Ninja Samurai (k-ninja-samurai.en.softonic.com)
        eMadfox,                ///< Madfox     (www.splyb.com/madfox)
        eMultiZilla,            ///< MultiZilla (multizilla.mozdev.org)
        eSeaMonkey,             ///< SeaMonkey  (www.mozilla.org/projects/seamonkey)

        // IE-based
        eAcooBrowser,           ///< Acoo Browser   (www.acoobrowser.com)
        eAOL,                   ///< America Online Browser (www.aol.com)
        eAvantBrowser,          ///< Avant Browser  (www.avantbrowser.com)
        eCrazyBrowser,          ///< Crazy Browser  (www.crazybrowser.com)
        eEnigmaBrowser,         ///< Enigma Browser (www.suttondesigns.com)
        eIRider,                ///< iRider         (www.irider.com)
        eMaxthon,               ///< Maxthon/MyIE2  (www.maxthon.com)
        eNetCaptor,             ///< NetCaptor      (www.netcaptor.com)

        // AppleWebKit/KHTML based
        eChrome,                ///< Google Chrome  (www.google.com/chrome)
        eFluid,                 ///< Fluid       (fluidapp.com)
        eMidori,                ///< Midori
        eNetNewsWire,           ///< NetNewsWire (www.apple.com)
        eOmniWeb,               ///< OmniWeb     (www.omnigroup.com/applications/omniweb)
        eQtWeb,                 ///< QtWeb       (www.qtweb.net)
        eSafari,                ///< Safari      (www.apple.com/safari)
        eShiira,                ///< Shiira      (hmdt-web.net/shiira/en)
        eStainless,             ///< Stainless   (www.stainlessapp.com)

        /// Search robots/bots/validators
        eCrawler,               ///< Class: crawlers / search robots
        eOfflineBrowser,        ///< Class: offline browsers
        eScript,                ///< Class: script tools (perl/php/...)
        eLinkChecker,           ///< Class: link checkers
        eWebValidator,          ///< Class: validators

        /// Mobile devices (browsers and services for: telephones, smartphones, communicators, PDAs and etc)
        /// Some mobile devices use standard browsers, like Opera or Safari -- see browser platform,
        /// if you need a check on mobile device.

        // See: http://www.zytrax.com/tech/web/mobile_ids.html

        eAirEdge,               ///< AIR-EDGE     (www.willcom-inc.com/en/)
        eAvantGo,               ///< AvantGo      (www.sybase.com/avantgo)
        eBlackberry,            ///< Blackberry   (www.blackberry.com)
        eDoCoMo,                ///< DoCoMo       (www.nttdocomo.com)
        eEudoraWeb,             ///< EudoraWeb    (www.eudora.com)
        eMinimo,                ///< Minimo       (www.mozilla.org/projects/minimo)
        eNetFront,              ///< NetFront     (www.access-company.com)
        eOperaMini,             ///< Opera Mini   (www.opera.com/mini)
        eOperaMobile,           ///< Opera Mobile (www.opera.com/mobile)
        eOpenWave,              ///< OpenWave/UP.Browser (www.openwave.com)
        ePIE,                   ///< Pocket IE    (www.reensoft.com/PIEPlus)
        ePlucker,               ///< Plucker      (www.plkr.org)
        ePocketLink,            ///< PocketLink   (www.mobilefan.net)
        ePolaris,               ///< Polaris Browser (www.infraware.co.kr)
        eReqwireless,           ///< Reqwireless Webviewer
        eSafariMobile,          ///< Mobile Safari (www.apple.com/safari)
        eSEMCBrowser,           ///< Sony Ericsson SEMC-Browser (www.sonyericsson.com)
        eTelecaObigo,           ///< Teleca/Obigo  (www.teleca.com / www.obigo.com)
        euZardWeb,              ///< uZard Web     (www.uzard.com)
        eVodafone,              ///< Ex J-Phone, now Vodafone Live! (www.vodafone.com)
        eXiino,                 ///< Xiino        (www.ilinx.co.jp/en/)

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
        eEngine_IE      = eIE,          ///< Microsoft Internet Explorer (Trident and etc)
        eEngine_Gecko   = eMozilla,     ///< Gecko-based
        eEngine_KHTML   = eSafari,      ///< Apple KHTML (WebKit) -based
        eEngine_Bot     = eCrawler      ///< Search robot/bot/checker/...
    };

    /// Platform types
    enum EBrowserPlatform {
        ePlatform_Unknown = eUnknown,   ///< Unknown OS
        ePlatform_Windows,              ///< Microsoft Windows
        ePlatform_Mac,                  ///< MacOS
        ePlatform_Unix,                 ///< Unix

        // Mobile devices (telephones, smartphones, communicators, PDA's and etc...)
        ePlatform_Palm,                 ///< PalmOS
        ePlatform_Symbian,              ///< SymbianOS
        ePlatform_WindowsCE,            ///< Microsoft Windows CE (+ Windows Mobile)
        ePlatform_MobileDevice          ///< Other mobile devices or services 
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

    /// Get browser engine type and name.
    /// @sa EBrowserEngine 
    EBrowserEngine GetEngine(void) const 
        { return m_Engine; }
    string GetEngineName(void) const;

    /// Get platform (OS) type and name.
    /// @sa EPlatform
    EBrowserPlatform GetPlatform(void) const 
        { return m_Platform; }
    string GetPlatformName(void) const;

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

    /// Check that this is known browser.
    ///
    /// @note
    ///   This method can return FALSE for old or unknown browsers,
    ///   or browsers for mobile devices.
    /// @sa GetBrowser, GetEngine
    bool IsBrowser(void) const;

    /// Check that this is known search robot/bot.
    ///
    /// By default it use GetEngine() and GetBrowser() value to check on
    /// known bots, and only here 'flags' parameter can be used. 
    /// @include_patterns
    ///   List of additional patterns that can treat current user agent
    ///   as bot. If standard check fails, this string and/or 
    ///   registry/environment parameters (section 'CGI', name 'Bots') 
    ///   will be used. String value should have patterns for search in 
    ///   the user agent string, and should looks like:
    ///       "Googlebot Scooter WebCrawler Slurp"
    ///   You can use any delimiters from next list " ;|~\t".
    ///   All patterns are case sensitive. 
    ///   For details how to define registry/environment parameter see
    ///   CParam description.
    /// @exclude_patterns
    ///   This parameter and string from (section 'CGI', name 'NotBots') can be
    ///   used to remove any user agent signature from list of bots, if you
    ///   don't agree with parser's decision. IsBot() will return FALSE if 
    ///   the user agent string contains one of these patters.
    /// @note
    ///   These parameters affect only IsBot() function, GetEngine() can still
    ///   return eEngine_Bot, or any other value, as detected.
    /// @note
    ///   Registry file:
    ///       [CGI]
    ///       Bots = ...
    ///       NotBots = ...
    ///   Environment variables:
    ///       NCBI_CONFIG__CGI__Bots  = ...
    ///       NCBI_CONFIG__CGI__NotBots  = ...
    /// @sa 
    ///   GetBrowser, GetEngine, CParam
    bool IsBot(TBotFlags flags = fBotAll,
               const string& include_patterns = kEmptyStr,
               const string& exclude_patterns = kEmptyStr) const;

    /// Check that this is known mobile device.
    ///
    /// By default it use GetPlatform() value to check on known mobile
    /// platforms. 
    /// @include_patterns
    ///   List of additional patterns that can treat current user agent
    ///   as mobile device If standard check fails, this string and/or
    ///   registry/environment parameter (section 'CGI', name 'MobileDevices')
    ///   will be used. String value should have patterns for search in 
    ///   the user agent string, and should looks like:
    ///       "AvantGo DoCoMo Minimo"
    ///   You can use any delimiters from next list " ;|~\t".
    ///   All patterns are case sensitive.
    /// @exclude_patterns
    ///   This parameter and string from (section 'CGI', name 'NotMobileDevices')
    ///   can be used to remove any user agent signature from list of mobile
    ///   devices, if you don't agree with parser's decision. IsMobileDevice()
    ///   will return FALSE if the user agent string contains one of these patters.
    /// @note
    ///   These parameters affect only IsBot() function, GetEngine() can still
    ///   return eEngine_Bot, or any other value, as detected.
    /// @note
    ///   Registry file:
    ///       [CGI]
    ///       MobileDevices = ...
    ///       NotMobileDevices = ...
    ///   Environment variables:
    ///       NCBI_CONFIG__CGI__MobileDevices = ...
    ///       NCBI_CONFIG__CGI__NotMobileDevices = ...
    /// @sa 
    ///   GetPlatform, EBrowserPlatform, CParam
    bool IsMobileDevice(const string& include_patterns = kEmptyStr,
                        const string& exclude_patterns = kEmptyStr) const;

protected:
    /// Init class members.
    void x_Init(void);
    /// Parse user agent string.
    void x_Parse(const string& user_agent);
    /// Parse token with browser name and version.
    bool x_ParseToken(const string& token, int where);

protected:
    string            m_UserAgent;      ///< User-Agent string
    TFlags            m_Flags;          ///< Comparison and parsing flags
    EBrowser          m_Browser;        ///< Browser type
    string            m_BrowserName;    ///< Browser name
    TUserAgentVersion m_BrowserVersion; ///< Browser version info
    EBrowserEngine    m_Engine;         ///< Browser engine type
    TUserAgentVersion m_EngineVersion;  ///< Browser engine version
    TUserAgentVersion m_MozillaVersion; ///< Browser mozilla version
    EBrowserPlatform  m_Platform;       ///< Platform type
};


END_NCBI_SCOPE

#endif  /* CGI___USER_AGENT__HPP */
