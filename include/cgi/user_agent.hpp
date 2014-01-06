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
/// Basically, support only Mozilla 'compatible' format.
///
class NCBI_XCGI_EXPORT CCgiUserAgent
{
public:
    /// Comparison and parsing flags.
    enum EFlags {
        /// Case insensitive compare, by default it is case sensitive
        fNoCase            = (1 << 1),
        /// Use external pattern list from registry/environment to check
        /// on bots, off by default
        fUseBotPatterns    = (1 << 2),
        /// Use external pattern lists from registry/environment to check
        /// on phone/tablet/mobile device when parsing, off by default
        fUseDevicePatterns = (1 << 3)
    };
    typedef unsigned int TFlags;  ///< Binary OR of "EFlags"

    /// Default constructor.
    /// Parse environment variable HTTP_USER_AGENT.
    CCgiUserAgent(TFlags flags = 0);

    /// Constructor.
    /// Parse the user agent string passed into the constructor.
    /// @note
    ///   Some registry/environment parameters can affect user agent parsing.
    ///   All such features are off by default for better performance, see EFlags.
    ///   But they will be used regardless of specified flags in
    ///   IsBot/IsMobileDevice/IsTabletDevice methods.
    CCgiUserAgent(const string& user_agent, TFlags flags = 0);

    /// Parse new user agent string.
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

        /// Mobile devices (browsers and services for: telephones, smartphones, tablets and etc)
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

        // Mobile devices (telephones, smart phones, tablets and etc...)
        ePlatform_Android,              ///< Android
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
    ///   or browsers for mobile devices. Use it with caution.
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
    ///   If you want to use space or any other symbol from the delimiters list
    ///   as part of the pattern, then use multi-line pattern. In this case
    ///   each line contains a single pattern with any symbol inside it.
    ///   All patterns are case sensitive.
    ///   For details how to define registry/environment parameter see
    ///   CParam description.
    /// @exclude_patterns
    ///   This parameter and string from (section 'CGI', name 'NotBots') can be
    ///   used to remove any user agent signature from list of bots, if you
    ///   don't agree with parser's decision. IsBot() will return FALSE if 
    ///   the user agent string contains one of these patters.
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

    /// Flags to check device type.
    /// Zero value mean unknown device type, usually considered as desktop.
    /// @sa GetDeviceType, IsMobileDevice, IsTableDevice
    enum EDeviceFlags {
        fDevice_Phone        = (1<<1),   ///< Phone / not known tablet / mobile browser on desktop
        fDevice_Tablet       = (1<<2),   ///< Known tablet
        fDevice_Mobile       = fDevice_Phone | fDevice_Tablet
    };
    typedef unsigned int TDeviceFlags;    ///< Binary OR of "EDeviceFlags"

    /// Get device type.
    ///
    /// Use this method with caution, because it is impossible to detect
    /// resolution or form-factor of the device based on user agent string only.
    /// We can only make an assumptions here.
    /// @return
    ///   Bit mask with categories of devices that can have such user agent string.
    ///   Zero value mean unknown device type, usually considered as desktop.
    /// @note
    ///   Some registry/environment parameters can affect user agent parsing.
    ///   See IsPhoneDevice(), IsMobileDevice() and IsTabletDevice() for details.
    /// @sa IsMobileDevice, IsTableDevice
    TDeviceFlags GetDeviceType(void) const
        { return m_DeviceFlags; }


    /// Check that this is a known phone-size device.
    ///
    /// Use this method with caution, because it is impossible to detect
    /// resolution and form-factor of the device based on user agent string only,
    /// we can only make an assumptions here. Also, we cannot say can some
    /// device make calls or not.
    /// @include_patterns
    ///   List of additional patterns that can treat current user agent
    ///   as phone-size device if standard check fails, this string and/or
    ///   registry/environment parameter (section 'CGI', name 'PhoneDevices')
    ///   will be used. String value should have patterns for search in 
    ///   the user agent string, and should looks like: "Name1 Name2 Name3".
    ///   You can use any delimiters from next list " ;|~\t".
    ///   If you want to use space or any other symbol from the delimiters list
    ///   as part of the pattern, then use multi-line pattern. In this case
    ///   each line contains a single pattern with any symbol inside it.
    ///   All patterns are case sensitive by default unless fNoCase flag is specified.
    /// @exclude_patterns
    ///   This parameter and string from (section 'CGI', name 'NotPhoneDevices')
    ///   can be used to remove any user agent signature from list of phone-size
    ///   devices, if you don't agree with parser's decision. IsPhoneDevice()
    ///   will return FALSE if the user agent string contains one of these patters.
    /// @return
    ///    Return TRUE for all user agent string that have known signatures of
    ///    phone-size devices. We can detect only limited number of such devices,
    ///    so be aware.
    /// @note
    ///   Registry file:
    ///       [CGI]
    ///       PhoneDevices = ...
    ///       NotPhoneDevices = ...
    ///   Environment variables:
    ///       NCBI_CONFIG__CGI__PhoneDevices = ...
    ///       NCBI_CONFIG__CGI__NotPhoneDevices = ...
    /// @sa 
    ///   GetDeviceType, GetPlatform, EBrowserPlatform, CParam, IsTabletDevice, IsMobileDevice
    bool IsPhoneDevice(const string& include_patterns = kEmptyStr,
                       const string& exclude_patterns = kEmptyStr) const;

    /// Check that this is a known tablet device.
    ///
    /// Use this method with caution, because it is impossible to detect
    /// resolution or form-factor of the device based on user agent string only,
    /// we can only make an assumptions here.
    /// @include_patterns
    ///   List of additional patterns that can treat current user agent
    ///   as tablet device if standard check fails, this string and/or
    ///   registry/environment parameter (section 'CGI', name 'TabletDevices')
    ///   will be used. String value should have patterns for search in 
    ///   the user agent string, and should looks like: "Name1 Name2 Name3".
    ///   You can use any delimiters from next list " ;|~\t".
    ///   If you want to use space or any other symbol from the delimiters list
    ///   as part of the pattern, then use multi-line pattern. In this case
    ///   each line contains a single pattern with any symbol inside it.
    ///   All patterns are case sensitive by default unless fNoCase flag is specified.
    /// @exclude_patterns
    ///   This parameter and string from (section 'CGI', name 'NotTabletDevices')
    ///   can be used to remove any user agent signature from list of tablet
    ///   devices, if you don't agree with parser's decision. IsTabletDevice()
    ///   will return FALSE if the user agent string contains one of these patters.
    /// @note
    ///   Registry file:
    ///       [CGI]
    ///       TabletDevices = ...
    ///       NotTabletDevices = ...
    ///   Environment variables:
    ///       NCBI_CONFIG__CGI__TabletDevices = ...
    ///       NCBI_CONFIG__CGI__TabletDevices = ...
    /// @return
    ///    Return TRUE for devices that can be detected as tablets.
    ///    Usually, IsMobileDevice() also return TRUE for the same
    ///    user agent string. Not all devices can be detected as tablets,
    ///    only few combinations of new versions of browsers and OS provide
    ///    such informations in the UA-string, and limited number of device
    ///    names can be used for such detection.
    /// @sa 
    ///   GetDeviceType, CParam, IsMobileDevice, IsPhoneDevice
    bool IsTabletDevice(const string& include_patterns = kEmptyStr,
                        const string& exclude_patterns = kEmptyStr) const;

    /// Check that this is a known mobile device.
    ///
    /// Use this method with caution, because it is impossible to detect
    /// resolution or form-factor of the device based on user agent string only,
    /// we can only make an assumptions here.
    /// @include_patterns
    ///   List of additional patterns that can treat current user agent
    ///   as mobile device if standard check fails, this string and/or
    ///   registry/environment parameter (section 'CGI', name 'MobileDevices')
    ///   will be used. String value should have patterns for search in 
    ///   the user agent string, and should looks like: "Name1 Name2 Name3".
    ///   You can use any delimiters from next list " ;|~\t".
    ///   If you want to use space or any other symbol from the delimiters list
    ///   as part of the pattern, then use multi-line pattern. In this case
    ///   each line contains a single pattern with any symbol inside it.
    ///   All patterns are case sensitive by default unless fNoCase flag is specified.
    /// @exclude_patterns
    ///   This parameter and string from (section 'CGI', name 'NotMobileDevices')
    ///   can be used to remove any user agent signature from list of mobile
    ///   devices, if you don't agree with parser's decision. IsMobileDevice()
    ///   will return FALSE if the user agent string contains one of these patters.
    /// @return
    ///    Return TRUE for all devices with user agent strings that use mobile
    ///    version of browser or have any known mobile platform signatures.
    ///    The device can be a phone or tablet, or any other device with
    ///    the same keywords in the UA-string.
    /// @note
    ///   Registry file:
    ///       [CGI]
    ///       MobileDevices = ...
    ///       NotMobileDevices = ...
    ///   Environment variables:
    ///       NCBI_CONFIG__CGI__MobileDevices = ...
    ///       NCBI_CONFIG__CGI__NotMobileDevices = ...
    /// @sa 
    ///   GetDeviceType, GetPlatform, EBrowserPlatform, CParam, IsPhoneDevice, IsTabletDevice
    bool IsMobileDevice(const string& include_patterns = kEmptyStr,
                        const string& exclude_patterns = kEmptyStr) const;

protected:
    /// Init class members.
    void x_Init(void);
    /// Parse user agent string.
    void x_Parse(const string& user_agent);
    /// Parse token with browser name and version.
    bool x_ParseToken(const string& token, int where);
    /// Helper method to check UA-string against external pattern lists.
    bool x_CheckPattern(int what, bool current_status, bool use_patterns,
                        const string& include_patterns = kEmptyStr,
                        const string& exclude_patterns = kEmptyStr) const;
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
    TDeviceFlags      m_DeviceFlags;    ///< Device type flags
};


END_NCBI_SCOPE

#endif  /* CGI___USER_AGENT__HPP */
