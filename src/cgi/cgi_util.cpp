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
 * Authors:  Alexey Grichenko, Vladimir Ivanov
 *
 * File Description:   CGI related utility classes and functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbienv.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/cgi_util.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <stdlib.h>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// Url encode/decode
//

extern SIZE_TYPE URL_DecodeInPlace(string& str, EUrlDecode decode_flag)
{
    NStr::URLDecodeInPlace(str, NStr::EUrlDecode(decode_flag));
    return 0;
}


extern string URL_DecodeString(const string& str,
                               EUrlEncode    encode_flag)
{
    if (encode_flag == eUrlEncode_None) {
        return str;
    }
    return NStr::URLDecode(str, encode_flag == eUrlEncode_PercentOnly ?
        NStr::eUrlDec_Percent : NStr::eUrlDec_All);
}


extern string URL_EncodeString(const string& str,
                               EUrlEncode encode_flag)
{
    return NStr::URLEncode(str, NStr::EUrlEncode(encode_flag));
}


//////////////////////////////////////////////////////////////////////////////
//
// CCgiUserAgent
//

CCgiUserAgent::CCgiUserAgent(void)
{
    CNcbiApplication* ncbi_app = CNcbiApplication::Instance();
    CCgiApplication* cgi_app   = CCgiApplication::Instance();
    string user_agent;
    if (cgi_app) {
        user_agent = cgi_app->GetContext().GetRequest()
            .GetProperty(eCgi_HttpUserAgent);
    } else if (ncbi_app) {
        user_agent = ncbi_app->GetEnvironment().Get("HTTP_USER_AGENT");
    } else {
        user_agent = getenv("HTTP_USER_AGENT");
    }
    if ( !user_agent.empty() ) {
        x_Parse(user_agent);
    }
}

CCgiUserAgent::CCgiUserAgent(const string& user_agent)
{
    x_Parse(user_agent);
}

void CCgiUserAgent::x_Init(void)
{
    m_UserAgent.erase();
    m_Browser = eUnknown;
    m_BrowserName = kEmptyStr;
    m_BrowserVersion.SetVersion(-1, -1, -1);
    m_Engine = eEngine_Unknown; 
    m_EngineVersion.SetVersion(-1, -1, -1);
    m_MozillaVersion.SetVersion(-1, -1, -1);
    m_Platform = ePlatform_Unknown;
}

void CCgiUserAgent::Reset(const string& user_agent)
{
    x_Parse(user_agent);
}


bool CCgiUserAgent::IsBrowser(void) const
{
    switch ( GetEngine() ) {
        case eEngine_IE:
        case eEngine_Gecko:
        case eEngine_KHTML:
            return true;
        case eEngine_Bot:
            return false;
        case eEngine_Unknown:
            ; // check browser, see below
    }
    switch ( GetBrowser() ) {
        // Browsers
        case eiCab:
        case eKonqueror:
        case eLynx:
        case eOregano:
        case eOpera:
        case eW3m:
        // Mobile devices
        case eAirEdge:
        case eAvantGo:
        case eBlackberry:
        case eDoCoMo:
        case eEudoraWeb:
        case eMinimo:
        case eNetFront:
        case eOpenWave:
        case eOperaMini:
        case eOperaMobile:
        case ePIE:
        case ePlucker:
        case ePocketLink:
        case ePolaris:
        case eReqwireless:
        case eSEMCBrowser:
        case eTelecaObigo:
        case euZardWeb:
        case eVodafone:
        case eXiino:
            return true;
        default:
            ; // false
    }
    return false;
}


// Declare the parameter to get additional bots names.
// Registry file:
//     [CGI]
//     Bots = ...
// Environment variable:
//     NCBI_CONFIG__CGI__Bots / NCBI_CONFIG__Bots
//
// The value should looks like: "Googlebot Scooter WebCrawler Slurp"
NCBI_PARAM_DECL(string, CGI, Bots); 
NCBI_PARAM_DEF (string, CGI, Bots, kEmptyStr);

bool CCgiUserAgent::IsBot(TBotFlags flags, const string& param_patterns) const
{
    const char* kDelim = " ;\t|~";

    // Default check
    if (GetEngine() == eEngine_Bot) {
        if (flags == fBotAll) {
            return true;
        }
        TBotFlags need_flag = 0;
        switch ( GetBrowser() ) {
            case eCrawler:
                need_flag = fBotCrawler;
                break;
            case eOfflineBrowser:
                need_flag = fBotOfflineBrowser;
                break;
            case eScript:
                need_flag = fBotScript;
                break;
            case eLinkChecker:
                need_flag = fBotLinkChecker;
                break;
            case eWebValidator:
                need_flag = fBotWebValidator;
                break;
            default:
                break;
        }
        if ( flags & need_flag ) {
            return true;
        }
    }

    // Get additional bots patterns
    string bots = NCBI_PARAM_TYPE(CGI,Bots)::GetDefault();

    // Split patterns strings
    list<string> patterns;
    if ( !bots.empty() ) {
        NStr::Split(bots, kDelim, patterns);
    }
    if ( !param_patterns.empty() ) {
        NStr::Split(param_patterns, kDelim, patterns);
    }
    // Search patterns
    ITERATE(list<string>, i, patterns) {
        if ( m_UserAgent.find(*i) !=  NPOS ) {
            return true;
        }
    }
    return false;
}


// Declare the parameter to get additional mobile devices names.
// Registry file:
//     [CGI]
//     MobileDevices = ...
// Environment variable:
//     NCBI_CONFIG__CGI__MobileDevices / NCBI_CONFIG__MobileDevices
//
// The value should looks like: "AvantGo DoCoMo Minimo"
NCBI_PARAM_DECL(string, CGI, MobileDevices); 
NCBI_PARAM_DEF (string, CGI, MobileDevices, kEmptyStr);

bool CCgiUserAgent::IsMobileDevice(const string& param_patterns) const
{
    // Default check
    switch ( GetPlatform() ) {
        case ePlatform_Palm:
        case ePlatform_Symbian:
        case ePlatform_WindowsCE:
        case ePlatform_MobileDevice:
            return true;
        default:
            break;
    }
    const char* kDelim = " ;\t|~";

    // Get additional bots patterns
    string str = NCBI_PARAM_TYPE(CGI,MobileDevices)::GetDefault();

    // Split patterns strings
    list<string> patterns;
    if ( !str.empty() ) {
        NStr::Split(str, kDelim, patterns);
    }
    if ( !param_patterns.empty() ) {
        NStr::Split(param_patterns, kDelim, patterns);
    }
    // Search patterns
    ITERATE(list<string>, i, patterns) {
        if ( m_UserAgent.find(*i) !=  NPOS ) {
            return true;
        }
    }
    return false;
}


//
// Mozilla-compatible user agent always have next format:
//     AppProduct (AppComment) * VendorProduct [(VendorComment)]
//

// Search flags
enum EUASearchFlags {
    fAppProduct    = (1<<1), 
    fAppComment    = (1<<2), 
    fVendorProduct = (1<<3),
    fVendorComment = (1<<4),
    fProduct       = fAppProduct    | fVendorProduct,
    fApp           = fAppProduct    | fAppComment,
    fVendor        = fVendorProduct | fVendorComment,
    fAny           = fApp | fVendor
};
typedef int TUASearchFlags; // Binary OR of "ESearchFlags"


// Browser search information
struct SBrowser {
    CCgiUserAgent::EBrowser         type;     // Browser type
    const char*                     name;     // Browser name
    const char*                     key;      // Search key
    CCgiUserAgent::EBrowserEngine   engine;   // Engine type
    CCgiUserAgent::EBrowserPlatform platform; // Platform type (almost for mobile devices)
    TUASearchFlags                  flags;    // Search flags
};


// Browser search table (the order does matter!)
const SBrowser s_Browsers[] = {

    // Bots (crawlers, offline-browsers, checkers, validators, ...)
    // Check bots first, because they often sham to be IE or Mozilla.

    // type                         name                        key                         engine                          platform                               search flags
    { CCgiUserAgent::eCrawler,      "ABACHOBot",                "ABACHOBot",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "Accoona-AI-Agent",         "Accoona-AI-Agent",         CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "AbiLogicBot",              "www.abilogic.com",         CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Advanced Email Extractor", "Advanced Email Extractor", CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "AnsearchBot",              "AnsearchBot",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Alexa/Internet Archive",   "ia_archiver",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Almaden",                  "www.almaden.ibm.com",      CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "AltaVista Scooter",        "Scooter",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Amfibibot",                "Amfibibot",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "AnyApexBot",               "www.anyapex.com",          CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "AnswerBus",                "AnswerBus",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Appie spider",             "www.walhello.com",         CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Arachmo",                  "Arachmo",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Ask Jeeves",               "Ask Jeeves",               CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "ASPseek",                  "ASPseek",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "BaiduSpider",              "BaiDuSpider",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "BaiduSpider",              "www.baidu.com",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "BDFetch",                  "BDFetch",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "BecomeBot",                "www.become.com",           CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Bimbot",                   "Bimbot",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "BlitzBOT",                 "B-l-i-t-z-B-O-T",          CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "BlitzBOT",                 "BlitzBot",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "BlitzBOT",                 "BlitzBOT",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "Boitho search robot",      "boitho.com",               CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "BrailleBot",               "BrailleBot",               CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "BruinBot",                 "BruinBot",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "btbot",                    "www.btbot.com",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Cerberian Drtrs",          "Cerberian Drtrs",          CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "Charlotte",                "Charlotte",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "ConveraCrawler",           "ConveraCrawler",           CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "cosmos",                   "robot@xyleme.com",         CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "DataparkSearch",           "DataparkSearch",           CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "DiamondBot",               "DiamondBot",               CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Discobot",                 "discoveryengine.com",      CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "EmailSiphon",              "EmailSiphon",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "EmeraldShield.com",        "www.emeraldshield.com",    CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Envolk",                   "www.envolk.com",           CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "EsperanzaBot",             "EsperanzaBot",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Exabot",                   "Exabot",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "FAST Enterprise Crawler",  "FAST Enterprise Crawler",  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "FAST-WebCrawler",          "FAST-WebCrawler",          CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "FDSE robot",               "FDSE robot",               CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "FindLinks",                "findlinks",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "FurlBot",                  "www.furl.net",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "FusionBot",                "www.galaxy.com/info/crawler", CCgiUserAgent::eEngine_Bot,  CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "FyberSpider",              "FyberSpider",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Gaisbot",                  "Gaisbot",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "GalaxyBot",                "www.galaxy.com/galaxybot", CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "genieBot",                 "genieBot",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "genieBot",                 "geniebot",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Gigabot",                  "Gigabot",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Girafabot",                "www.girafa.com",           CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Googlebot-Image",          "Googlebot-Image",          CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "Googlebot",                "Googlebot",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "Googlebot",                "www.googlebot.com",        CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "GSA crawler",              "gsa-crawler",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Hatena Antenna",           "Hatena Antenna",           CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Heritrix",                 "archive.org_bot",          CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "hl_ftien_spider",          "hl_ftien_spider",          CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "ht://Dig",                 "htdig",                    CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "ia_archiver",              "ia_archiver",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "ichiro",                   "ichiro",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Iltrovatore-Setaccio",     "Iltrovatore-Setaccio",     CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "InfoSeek Sidewinder",      "InfoSeek Sidewinder",      CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "IRLbot",                   "IRLbot",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "IssueCrawler",             "IssueCrawler",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Jyxobot",                  "Jyxobot",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "LapozzBot",                "LapozzBot",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Larbin",                   "larbin",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "Larbin",                   "LARBIN",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "Larbin",                   "LarbinWebCrawler",         CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "Lycos Spider",             "Lycos_Spider",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "lmspider",                 "lmspider",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "mabontland",               "www.mabontland.com",       CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "magpie-crawler",           "magpie-crawler",           CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "Maxamine Web Analyst",     "maxamine.com",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "Mediapartners",            "Mediapartners-Google",     CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Metacarta.com",            "metacarta.com",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "MJ12bot",                  "MJ12bot",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Mnogosearch",              "Mnogosearch",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "mogimogi",                 "mogimogi",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "MojeekBot",                "www.mojeek.com",           CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Morning Paper",            "Morning Paper",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "MSNBot",                   "msnbot",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "MS Sharepoint Portal Server","MS Search",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "MSIECrawler",              "MSIECrawler",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "MSRBOT",                   "MSRBOT",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "NaverBot",                 "NaverBot",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "NetResearchServer",        "NetResearchServer",        CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "NG-Search",                "NG-Search",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "nicebot",                  "nicebot",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "noxtrumbot",               "noxtrumbot",               CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "NuSearch Spider",          "NuSearch Spider",          CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "NutchCVS",                 "NutchCVS",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "obot",                     "; obot",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "OmniExplorer",             "OmniExplorer_Bot",         CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "OOZBOT",                   "OOZBOT",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Orbiter",                  "www.dailyorbit.com",       CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "PolyBot",                  "/polybot/",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Pompos",                   "Pompos",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Picsearch",                "psbot",                    CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Picsearch",                "www.picsearch.com",        CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "RAMPyBot",                 "giveRAMP.com",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "RufusBot",                 "RufusBot",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "SandCrawler",              "SandCrawler",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "SBIder",                   ".sitesell.com",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "Scrubby",                  "www.scrubtheweb.com",      CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "SearchSight",              "SearchSight",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "Seekbot",                  "Seekbot",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Seekbot",                  "www.seekbot.net",          CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "semanticdiscovery",        "semanticdiscovery",        CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Sensis Web Crawler",       "Sensis Web Crawler",       CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "SEOChat::Bot",             "SEOChat::Bot",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "Shim-Crawler",             "Shim-Crawler",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "ShopWiki",                 "ShopWiki",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Shoula robot",             "Shoula robot",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Simpy",                    "www.simpy.com/bot",        CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Slurp",                    "/slurp.html",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Snappy",                   "www.urltrends.com",        CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "sogou spider",             "sogou spider",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "Speedy Spider",            "www.entireweb.com",        CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Sqworm",                   "Sqworm",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "StackRambler",             "StackRambler",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "SurveyBot",                "SurveyBot",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Susie",                    "www.sync2it.com",          CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "SynooBot",                 "SynooBot",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "TerrawizBot",              "TerrawizBot",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "TheSuBot",                 "TheSuBot",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Thumbnail.CZ robot",       "Thumbnail.CZ robot",       CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "TinEye",                   "TinEye",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "TurnitinBot",              "TurnitinBot",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "TurnitinBot",              "www.turnitin.com/robot",   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Updated! search robot",    "updated.com",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Vagabondo",                "Vagabondo",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "Verity Ultraseek",         "k2spider",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "VoilaBot",                 "VoilaBot",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "Vortex",                   "marty.anstey.ca/robots",   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "Vspider",                  "vspider",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "VYU2",                     "VYU2",                     CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "W3CRobot",                 "W3CRobot",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "webcollage",               "webcollage",               CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "WebSearch",                "www.WebSearch.com.au",     CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eCrawler,      "Websquash.com",            "Websquash.com",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "wf84",                     "[wf84]",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "WoFindeIch Robot",         "WoFindeIch Robot",         CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Xaldon_WebSpider",         "Xaldon_WebSpider",         CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "ZyBorg",                   "www.wisenutbot.com",       CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "yacy",                     "yacy.net",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrawler,      "Yahoo! Slurp",             "Yahoo! Slurp",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "YahooSeeker",              "YahooSeeker",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "Zao",                      "www.kototoi.org/zao/",     CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "zspider",                  "zspider",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eCrawler,      "ZyBorg",                   "ZyBorg",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },

    { CCgiUserAgent::eCrawler,      "",                         "webcrawler",               CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "",                         "/robot.html",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eCrawler,      "",                         "/crawler.html",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },

    { CCgiUserAgent::eOfflineBrowser, "HTMLParser",             "HTMLParser",               CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eOfflineBrowser, "Offline Explorer",       "Offline Explorer",         CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eOfflineBrowser, "SuperBot",               "SuperBot",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eOfflineBrowser, "Web Downloader",         "Web Downloader",           CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eOfflineBrowser, "WebCopier",              "WebCopier",                CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eOfflineBrowser, "WebZIP",                 "WebZIP",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },

    { CCgiUserAgent::eLinkChecker,  "AbiLogicBot",              "AbiLogicBot",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eLinkChecker,  "Dead-Links.com",           "www.dead-links.com",       CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eLinkChecker,  "InfoWizards",              "www.infowizards.com",      CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eLinkChecker,  "Html Link Validator",      "www.lithopssoft.com",      CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eLinkChecker,  "Link Sleuth",              "Link Sleuth",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "Link Valet",               "Link Valet",               CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eLinkChecker,  "Link Validity Check",      "www.w3dir.com",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eLinkChecker,  "Linkbot",                  "Linkbot",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eLinkChecker,  "LinksManager.com_bot",     "linksmanager.com",         CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eLinkChecker,  "LinkWalker",               "LinkWalker",               CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "Mojoo Robot",              "www.mojoo.com",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eLinkChecker,  "Notifixious",              "Notifixious",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "REL Link Checker Lite",    "REL Link Checker Lite",    CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "SafariBookmarkChecker",    "SafariBookmarkChecker",    CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "SiteBar",                  "SiteBar",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "Vivante Link Checker",     "www.vivante.com",          CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eLinkChecker,  "W3C-checklink",            "W3C-checklink",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "Xenu Link Sleuth",         "Xenu Link Sleuth",         CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "Zealbot",                  "Zealbot",                  CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },

    { CCgiUserAgent::eWebValidator, "CSE HTML Validator",       "htmlvalidator.com",        CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eWebValidator, "CSSCheck",                 "CSSCheck",                 CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eWebValidator, "P3P Validator",            "P3P Validator",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eWebValidator, "W3C_CSS_Validator",        "W3C_CSS_Validator",        CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eWebValidator, "W3C_Validator",            "W3C_Validator",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eWebValidator, "WDG_Validator",            "WDG_Validator",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },

    { CCgiUserAgent::eScript,       "DomainsDB.net",            "domainsdb.net",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eScript,       "Jakarta Commons HTTP Client","Jakarta Commons-HttpClient",  CCgiUserAgent::eEngine_Bot,CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eScript,       "Snoopy",                   "Snoopy",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eScript,       "libwww-perl",              "libwww-perl",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eScript,       "LWP",                      "LWP::Simple",              CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eScript,       "lwp-trivial",              "lwp-",                     CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eScript,       "Microsoft Data Access",    "Microsoft Data Access",    CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eScript,       "Microsoft URL Control",    "Microsoft URL Control",    CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eScript,       "Microsoft-ATL-Native",     "Microsoft-ATL-Native",     CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eScript,       "PycURL",                   "PycURL",                   CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eScript,       "Python-urllib",            "Python-urllib",            CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eScript,       "Wget",                     "Wget",                     CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct },

    // Mobile devices

    { CCgiUserAgent::eAirEdge,      "AIR-EDGE",                 "DDIPOCKET",                CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAppComment },
    { CCgiUserAgent::eAirEdge,      "AIR-EDGE",                 "PDXGW",                    CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAppProduct },
    { CCgiUserAgent::eAirEdge,      "AIR-EDGE",                 "ASTEL",                    CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAppProduct },
    { CCgiUserAgent::eAvantGo,      "AvantGo",                  "AvantGo",                  CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::eBlackberry,   "Blackberry",               "BlackBerry",               CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAppProduct },
    { CCgiUserAgent::eDoCoMo,       "DoCoMo",                   "DoCoMo",                   CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::eEudoraWeb,    "EudoraWeb",                "EudoraWeb",                CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Palm,         fAny },
    { CCgiUserAgent::eMinimo,       "Minimo",                   "Minimo",                   CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_MobileDevice, fVendorProduct },
    { CCgiUserAgent::eNetFront,     "NetFront",                 "NetFront",                 CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::eOpenWave,     "OpenWave/UP.Browser",      "UP.Browser",               CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::eOperaMini,    "Opera Mini",               "Opera Mini",               CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fApp },
    { CCgiUserAgent::eOperaMobile,  "Opera Mobile",             "Opera Mobi",               CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fApp },
    { CCgiUserAgent::ePIE,          "Pocket Internet Explorer", "MSPIE",                    CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::ePIE,          "Pocket Internet Explorer", "PIE",                      CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::ePlucker,      "Plucker",                  "Plucker",                  CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::ePocketLink,   "PocketLink",               "PocketLink",               CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::ePocketLink,   "PocketLink",               "PLink",                    CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::ePolaris,      "Polaris Browser",          "POLARIS",                  CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::eReqwireless,  "Reqwireless Webviewer",    "ReqwirelessWeb",           CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::eReqwireless,  "Reqwireless Webviewer",    "Reqwireless",              CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::eSEMCBrowser,  "Sony Ericsson SEMC-Browser","SEMC-Browser",            CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::eTelecaObigo,  "Teleca/Obigo",             "Teleca",                   CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAppComment },
    { CCgiUserAgent::eTelecaObigo,  "Teleca/Obigo",             "AU-MIC-",                  CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::eTelecaObigo,  "Teleca/Obigo",             "AU-OBIGO",                 CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAny },
    { CCgiUserAgent::euZardWeb,     "uZard Web",                "uZardWeb",                 CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fAppComment },
    { CCgiUserAgent::eVodafone,     "Vodafone Live!",           "Vodafone",                 CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_MobileDevice, fApp },
    { CCgiUserAgent::eXiino,        "Xiino",                    "Xiino",                    CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Palm,         fApp },


    // Gecko-based                                              

    { CCgiUserAgent::eBeonex,       "Beonex",                   "Beonex",                   CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eCamino,       "Camino",                   "Camino",                   CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eChimera,      "Chimera",                  "Chimera",                  CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eFirefox,      "Firefox",                  "Firefox",                  CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eFirefox,      "Firebird", /*ex-Firefox*/  "Firebird",                 CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eFlock,        "Flock",                    "Flock",                    CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eIceCat,       "IceCat",                   "IceCat",                   CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eIceweasel,    "Iceweasel",                "Iceweasel",                CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eGaleon,       "Galeon",                   "Galeon",                   CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eGranParadiso, "GranParadiso",             "GranParadiso",             CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eKazehakase,   "Kazehakase",               "Kazehakase",               CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eKMeleon,      "K-Meleon",                 "K-Meleon",                 CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eKNinja,       "K-Ninja",                  "K-Ninja",                  CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eMadfox,       "Madfox",                   "Madfox",                   CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eMultiZilla,   "MultiZilla",               "MultiZilla",               CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eNetscape,     "Netscape",                 "Netscape6",                CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eNetscape,     "Netscape",                 "Netscape7",                CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eNetscape,     "Netscape",                 "Netscape",                 CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eNetscape,     "Netscape",                 "NS8",                      CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eSeaMonkey,    "SeaMonkey",                "SeaMonkey",                CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eSeaMonkey,    "SeaMonkey",                "Seamonkey",                CCgiUserAgent::eEngine_Gecko,   CCgiUserAgent::ePlatform_Unknown,      fAppProduct },

    // IE-based                                                 

    { CCgiUserAgent::eAcooBrowser,  "Acoo Browser",             "Acoo Browser",             CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eAOL,          "AOL Browser",              "America Online Browser",   CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eAOL,          "AOL Browser",              " AOL",                     CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eAvantBrowser, "Avant Browser",            "Avant Browser",            CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eCrazyBrowser, "Crazy Browser",            "Crazy Browser",            CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eEnigmaBrowser,"Enigma Browser",           "Enigma Browser",           CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eIRider,       "iRider",                   "iRider",                   CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eMaxthon,      "Maxthon",                  "Maxthon",                  CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eMaxthon,      "MyIE2",                    "MyIE2",                    CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eNetCaptor,    "NetCaptor",                "NetCaptor",                CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    
    // Check IE last, after all IE-based browsers               ased browsers
    { CCgiUserAgent::eIE,           "Internet Explorer",        "Internet Explorer",        CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fProduct },
    { CCgiUserAgent::eIE,           "Internet Explorer",        "MSIE",                     CCgiUserAgent::eEngine_IE,      CCgiUserAgent::ePlatform_Unknown,      fApp },

    // AppleQWebKit/KHTML-based                                 

    { CCgiUserAgent::eChrome,       "Google Chrome",            "Chrome",                   CCgiUserAgent::eEngine_KHTML,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eFluid,        "Fluid",                    "Fluid",                    CCgiUserAgent::eEngine_KHTML,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eMidori,       "Midori",                   "Midori",                   CCgiUserAgent::eEngine_KHTML,   CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eMidori,       "Midori",                   "midori",                   CCgiUserAgent::eEngine_KHTML,   CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eNetNewsWire,  "NetNewsWire",              "NetNewsWire",              CCgiUserAgent::eEngine_KHTML,   CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eOmniWeb,      "OmniWeb",                  "OmniWeb",                  CCgiUserAgent::eEngine_KHTML,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eQtWeb,        "QtWeb",                    "QtWeb Internet Browser",   CCgiUserAgent::eEngine_KHTML,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eSafari,       "Safari",                   "Mobile Safari",            CCgiUserAgent::eEngine_KHTML,   CCgiUserAgent::ePlatform_MobileDevice, fVendorProduct },
    { CCgiUserAgent::eSafari,       "Safari",                   "Safari",                   CCgiUserAgent::eEngine_KHTML,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eShiira,       "Shiira",                   "Shiira",                   CCgiUserAgent::eEngine_KHTML,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },
    { CCgiUserAgent::eStainless,    "Stainless",                "Stainless",                CCgiUserAgent::eEngine_KHTML,   CCgiUserAgent::ePlatform_Unknown,      fVendorProduct },

    // Other                                                    

    { CCgiUserAgent::eiCab,         "iCab",                     "iCab",                     CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eKonqueror,    "Konqueror",                "Konqueror",                CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Unknown,      fApp },
    { CCgiUserAgent::eLynx,         "Lynx",                     "Lynx",                     CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eLynx,         "ELynx", /* Linx based */   "ELynx",                    CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eOregano,      "Oregano",                  "Oregano2",                 CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eOregano,      "Oregano",                  "Oregano",                  CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Unknown,      fAppComment },
    { CCgiUserAgent::eOpera,        "Opera",                    "Opera",                    CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Unknown,      fAny },
    { CCgiUserAgent::eW3m,          "w3m",                      "w3m",                      CCgiUserAgent::eEngine_Unknown, CCgiUserAgent::ePlatform_Unknown,      fAppProduct },
    { CCgiUserAgent::eNagios,       "check_http (nagios-plugins)","check_http",             CCgiUserAgent::eEngine_Bot,     CCgiUserAgent::ePlatform_Unknown,      fAppProduct }

    // We have special case to detect Mozilla/Mozilla-based
};
const size_t kBrowsers = sizeof(s_Browsers)/sizeof(s_Browsers[0]);


// Returns position first non-digit in the string, or string length.
SIZE_TYPE s_SkipDigits(const string& str, SIZE_TYPE pos)
{
    SIZE_TYPE len = str.length();
    while ( pos < len  &&  isdigit((unsigned char)str[pos]) ) {
        pos++;
    }
    _ASSERT( pos <= len );
    return pos;
}

void s_ParseVersion(const string& token, SIZE_TYPE start_pos,
                    TUserAgentVersion* version)
{
    SIZE_TYPE len = token.length();
    if ( start_pos >= len ) {
        return;
    }
    // Some browsers have 'v' before version number
    if ( token[start_pos] == 'v' ) {
        start_pos++;
    }
    if ( (start_pos >= len) || 
        !isdigit((unsigned char)token[start_pos]) ) {
        return;
    }
    // Init version numbers
    int major = -1;
    int minor = -1;
    int patch = -1;

    // Parse version
    SIZE_TYPE pos = s_SkipDigits(token, start_pos + 1);
    if ( (pos < len-1)  && (token[pos] == '.') ) {
        minor = atoi(token.c_str() + pos + 1);
        pos = s_SkipDigits(token, pos + 1);
        if ( (pos < len-1)  &&  (token[pos] == '.') ) {
            patch = atoi(token.c_str() + pos + 1);
        }
    }
    major = atoi(token.c_str() + start_pos);
    // Store version
    version->SetVersion(major, minor, patch);
}


void CCgiUserAgent::x_Parse(const string& user_agent)
{
    string search;

    // Initialization
    x_Init();
    m_UserAgent = NStr::TruncateSpaces(user_agent);
    SIZE_TYPE len = m_UserAgent.length();

    // Check VendorProduct token first.
    // If it matched some browser name, return it.

    SIZE_TYPE pos = m_UserAgent.rfind(")", NPOS);
    if (pos != NPOS) {
        // Have VendorProduct only
        if (pos < len-1) {
            string token = m_UserAgent.substr(pos+1);
            x_ParseToken(token, fVendorProduct);
        } 
        // Have VendorComment also, cut it off before parsing VendorProduct token
        else if ((pos == len-1)  &&
                 (len >= 5 /* min possible for string with VendorComment */)) { 
            // AppComment token should be present also
            pos = m_UserAgent.rfind(")", pos-1);
            if (pos != NPOS) {
                pos++;
                SIZE_TYPE pos_comment = m_UserAgent.find("(", pos);
                if (pos_comment != NPOS) {
                    string token = m_UserAgent.substr(pos, pos_comment - pos);
                    x_ParseToken(token, fVendorProduct);
                }
            }
        }
        // Possible, we already have browser name and version, but
        // try to determine Mozilla and engine versions (below),
        // and only than return.
    }

    // Handles browsers declaring Mozilla-compatible

    if ( NStr::MatchesMask(m_UserAgent, "Mozilla/*") ) {
        // Get Mozilla version
        search = "Mozilla/";
        s_ParseVersion(m_UserAgent, search.length(), &m_MozillaVersion);

        // Get Mozilla engine version (except bots)
        if ( m_Engine != eEngine_Bot ) {
            search = "; rv:";
            pos = m_UserAgent.find(search);
            if (pos != NPOS) {
                m_Engine = eEngine_Gecko;
                pos += search.length();
                s_ParseVersion(m_UserAgent, pos, &m_EngineVersion);
            }
        }
        // Ignore next code if the browser type already detected
        if ( m_Browser == eUnknown ) {

            // Check Mozilla-compatible
            if ( NStr::MatchesMask(m_UserAgent, "Mozilla/*(compatible;*") ) {
                // Browser.
                m_Browser = eMozillaCompatible;
                // Try to determine real browser using second entry
                // in the AppComment token.
                search = "(compatible;";
                pos = m_UserAgent.find(search);
                if (pos != NPOS) {
                    pos += search.length();
                    // Extract remains of AppComment
                    // (can contain nested parenthesis)
                    int par = 1;
                    SIZE_TYPE end = pos;
                    while (end < len  &&  par) {
                        if ( m_UserAgent[end] == ')' )
                            par--;
                        else if ( m_UserAgent[end] == '(' )
                            par++;
                        end++;
                    }
                    if ( end <= len ) {
                        string token = m_UserAgent.substr(pos, end-pos-1);
                        x_ParseToken(token, fAppComment);
                    }
                }
                // Real browser name not found
                // Continue below to check product name
            } 
            
            // Handles the real Mozilla (or old Netscape if version < 5.0)
            else {
                m_BrowserVersion = m_MozillaVersion;
                // If product version < 5.0 -- we have Netscape
                int major = m_BrowserVersion.GetMajor();
                if ( (major < 0)  ||  (major < 5) ) {
                    m_Browser     = eNetscape;
                    m_BrowserName = "Netscape";
                } else { 
                    m_Browser     = eMozilla;
                    m_BrowserName = "Mozilla";
                    m_Engine      = eEngine_Gecko;
                }
                // Stop
                // return;
            }
        }
    }

    // If none of the above matches, uses first product token in list

    if ( m_Browser == eUnknown ) {
        x_ParseToken(m_UserAgent, fAppProduct);
    }

    // Try to get engine version for IE-based browsers
    if ( m_Engine == eEngine_IE ) {
        if ( m_Browser == eIE ) {
            m_EngineVersion = m_BrowserVersion;
        } else {
            search = " MSIE ";
            pos = m_UserAgent.find(search);
            if (pos != NPOS) {
                pos += search.length();
                s_ParseVersion(m_UserAgent, pos, &m_EngineVersion);
            }
        }
    }

    // Determine engine for new Netscape's
    if ( m_Browser == eNetscape ) {
        // Netscape 6.0 November 14, 2000 (based on Mozilla 0.7)
        // Netscape 6.01 February 9, 2001(based on Mozilla 0.7)
        // Netscape 6.1 August 8, 2001 (based on Mozilla 0.9.2.1)
        // Netscape 6.2 October 30, 2001 (based on Mozilla 0.9.4.1)
        // Netscape 6.2.1 (based on Mozilla 0.9.4.1)
        // Netscape 6.2.2 (based on Mozilla 0.9.4.1)
        // Netscape 6.2.3 May 15, 2002 (based on Mozilla 0.9.4.1)
        // Netscape 7.0 August 29, 2002 (based on Mozilla 1.0.1)
        // Netscape 7.01 December 10, 2002 (based on Mozilla 1.0.2)
        // Netscape 7.02 February 18, 2003 (based on Mozilla 1.0.2)
        // Netscape 7.1 June 30, 2003 (based on Mozilla 1.4)
        // Netscape 7.2 August 17, 2004 (based on Mozilla 1.7)
        // Netscape Browser 0.5.6+ November 30, 2004 (based on Mozilla Firefox 0.9.3)
        // Netscape Browser 0.6.4 January 7, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 0.9.4 (8.0 Pre-Beta) February 17, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 0.9.5 (8.0 Pre-Beta) February 23, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 0.9.6 (8.0 Beta) March 3, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 8.0 May 19, 2005 (based on Mozilla Firefox 1.0.3)
        // Netscape Browser 8.0.1 May 19, 2005 (based on Mozilla Firefox 1.0.4)
        // Netscape Browser 8.0.2 June 17, 2005 (based on Mozilla Firefox 1.0.4)
        // Netscape Browser 8.0.3.1 July 25, 2005 (based on Mozilla Firefox 1.0.6)
        // Netscape Browser 8.0.3.3 August 8, 2005 (based on Mozilla Firefox 1.0.6)
        // Netscape Browser 8.0.4 October 19, 2005 (based on Mozilla Firefox 1.0.7)

        int major = m_BrowserVersion.GetMajor();
        if ( major > 0 ) {
            if ( (major < 1) || (major > 5) ) {
                m_Engine = eEngine_Gecko;
            }
        }
    }

    // Try to get engine version for KHTML-based browsers
    search = " AppleWebKit/";
    pos = m_UserAgent.find(search);
    if (pos != NPOS) {
        m_Engine = eEngine_KHTML;
        pos += search.length();
        s_ParseVersion(m_UserAgent, pos, &m_EngineVersion);
    }

    // Hack for some browsers (like Safari) that use Version/x.x.x rather than
    // Safari/x.x.x for real browser version (for Safari, numbers after browser
    // name represent a build version).
    if ( m_Browser != eUnknown ) {
        search = " Version/";
        pos = m_UserAgent.find(search);
        if (pos != NPOS) {
            pos += search.length();
            s_ParseVersion(m_UserAgent, pos, &m_BrowserVersion);
        } else {
            // Safari (old version) -- try to get browser version
            // depending on engine (WebKit) version (very approximately).
            if ( m_Browser == eSafari  &&  m_Engine == eEngine_KHTML) { 
                // See http://www.useragentstring.com/pages/Safari/
                int rev = m_EngineVersion.GetMajor();
                if (rev < 85 ) {
                    m_BrowserVersion.SetVersion(-1,-1,-1);  // too early version
                } else if (rev < 124 ) {
                    m_BrowserVersion.SetVersion(1,0,-1);    // 1.0
                } else if (rev < 312 ) {
                    m_BrowserVersion.SetVersion(1,2,-1);    // 1.2
                } else if (rev < 412 ) {
                    m_BrowserVersion.SetVersion(1,3,-1);    // 1.3
                } else if (rev < 420 ) {
                    m_BrowserVersion.SetVersion(2,0,-1);    // 2.0.x
                } else if (rev < 525 ) {
                    m_BrowserVersion.SetVersion(3,0,-1);    // 3.0.x
                } else if (rev < 528 ) {
                    // We should never get here, because all Safari
                    // newer that 3.x should have "Version/" tag.
                    m_BrowserVersion.SetVersion(3,-1,-1);   // 3.x
                }
            }
        }
    }


    // Very crude algorithm to get platform type...
    // See also x_ParseToken() for specific platform types from the table.

    // Check mobile devices first (more precise for ePlatform_MobileDevice)
    if ( m_Platform == ePlatform_Unknown  ||
         m_Platform == ePlatform_MobileDevice ) {
        if (m_UserAgent.find("PalmSource")   != NPOS  ||
            m_UserAgent.find("PalmOS")       != NPOS  ||
            m_UserAgent.find("webOS")        != NPOS ) {
            m_Platform = ePlatform_Palm;
        } else
        if (m_UserAgent.find("Symbian")      != NPOS) {
            m_Platform = ePlatform_Symbian;
        } else
        if (m_UserAgent.find("Windows CE")   != NPOS  ||
            m_UserAgent.find("IEMobile")     != NPOS  ||
            m_UserAgent.find("Window Mobile")!= NPOS) {
            m_Platform = ePlatform_WindowsCE;
        }
    }
    // Make additional check if platform is still undefined
    if ( m_Platform == ePlatform_Unknown ) {
        if (m_UserAgent.find("Nokia")        != NPOS  ||  // Nokia
            //m_UserAgent.find("HTC-")       != NPOS  ||  // HTC
            //m_UserAgent.find("HTC_")       != NPOS  ||  // HTC
            m_UserAgent.find("iPod")         != NPOS  ||  // Apple iPod 
            m_UserAgent.find("iPhone")       != NPOS  ||  // Apple iPhone
            m_UserAgent.find("LGE-")         != NPOS  ||  // LG
            m_UserAgent.find("LG/U")         != NPOS  ||  // LG
            m_UserAgent.find("MOT-")         != NPOS  ||  // Motorola
            m_UserAgent.find("Samsung")      != NPOS  ||  // Samsung
            m_UserAgent.find("SonyEricsson") != NPOS  ||  // SonyEricsson
            m_UserAgent.find("J-PHONE")      != NPOS  ||  // Ex J-Phone, now Vodafone Live!
            m_UserAgent.find("HP iPAQ")      != NPOS  ||
            m_UserAgent.find("UP.Link")      != NPOS  ||
            m_UserAgent.find("PlayStation Portable") != NPOS) {
            m_Platform = ePlatform_MobileDevice;
        } else
        if (m_UserAgent.find("MacOS")        != NPOS  || 
            m_UserAgent.find("Mac OS")       != NPOS  ||
            m_UserAgent.find("Macintosh")    != NPOS  ||
            m_UserAgent.find("Mac_PowerPC")  != NPOS) {
            m_Platform = ePlatform_Mac;
        } else
        if (m_UserAgent.find("SunOS")        != NPOS  || 
            m_UserAgent.find("Linux")        != NPOS  ||
            m_UserAgent.find("FreeBSD")      != NPOS  ||
            m_UserAgent.find("NetBSD")       != NPOS  ||
            m_UserAgent.find("OpenBSD")      != NPOS  ||
            m_UserAgent.find("IRIX")         != NPOS  ||
            m_UserAgent.find("nagios-plugins") != NPOS) {
            m_Platform = ePlatform_Unix;
        } else
        // Check Windows last, its signature is too short
        if (m_UserAgent.find("Win")          != NPOS) {
            m_Platform = ePlatform_Windows;
        }
    }

    return;
}


bool CCgiUserAgent::x_ParseToken(const string& token, int where)
{
    SIZE_TYPE len = token.length();
    // Check all user agent signatures
    for (size_t i = 0; i < kBrowsers; i++) {
        if ( !(s_Browsers[i].flags & where) ) {
            continue;
        }
        string key = s_Browsers[i].key;
        SIZE_TYPE pos = token.find(key);
        if ( pos != NPOS ) {
            pos += key.length();
            // Browser
            m_Browser     = s_Browsers[i].type;
            m_BrowserName = s_Browsers[i].name;
            m_Engine      = s_Browsers[i].engine;
            // Set platform only if it is unambiguously defined for this browser
            if (s_Browsers[i].platform != ePlatform_Unknown) {
                m_Platform = s_Browsers[i].platform;
            }
            // Version.
            // Second entry in token after space or '/'.
            if ( (pos < len-1 /* have at least 1 symbol before EOL */) &&
                    ((token[pos] == ' ')  || (token[pos] == '/')) ) {
                s_ParseVersion(token, pos+1, &m_BrowserVersion);
            }
            // User agent found and parsed
            return true;
        }
    }
    // Not found
    return false;
}


END_NCBI_SCOPE
