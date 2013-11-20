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
 * Author:  Vladimir Ivanov
 *
 * File Description:
 *   TEST for:  CCgiUserAgent -- API to parse user agent strings.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <cgi/user_agent.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


struct TVersion {
    int major;
    int minor;
    int patch;
};

struct SUserAgent {
    const char*                     str;        // in
    CCgiUserAgent::EBrowser         browser;    // out
    TVersion                        browser_v;  // out
    CCgiUserAgent::EBrowserEngine   engine;     // out
    TVersion                        engine_v;   // out
    TVersion                        mozilla_v;  // out
    CCgiUserAgent::EBrowserPlatform platform;   // out
};

const SUserAgent s_UserAgentTests[] = {

    // VendorProduct tests

    { "SomeUnknownBrowser",
        CCgiUserAgent::eUnknown,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_Unknown
    },
    { "SomeUnknownBrowser/1.0",
        CCgiUserAgent::eUnknown,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_Unknown
    },
    { "Mozilla/5.0 (Windows) Firefox",
        CCgiUserAgent::eFirefox,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Gecko,   {-1, -1, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/5.0 (Windows) Firefox;",
        CCgiUserAgent::eFirefox,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Gecko,   {-1, -1, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/5.0 (Windows) Firefox/1",
        CCgiUserAgent::eFirefox,        { 1, -1, -1},
        CCgiUserAgent::eEngine_Gecko,   {-1, -1, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/4.0 (BeOS R4.5;US) Opera 3.62  [en]",
        CCgiUserAgent::eOpera,          { 3, 62, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 4, 0, -1},
        CCgiUserAgent::ePlatform_Unknown
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.1; nl-NL; rv:1.7.5) Gecko/20041202 Firefox/1.0",
        CCgiUserAgent::eFirefox,        { 1,  0, -1},
        CCgiUserAgent::eEngine_Gecko,   { 1,  7,  5},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9.0.2) Gecko/2008091620 Firefox/3.0.2 (.NET CLR 3.5.30729)",
        CCgiUserAgent::eFirefox,        {  3,  0,  2},
        CCgiUserAgent::eEngine_Gecko,   {  1,  9,  0},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.7.5) Gecko/20041107 Googlebot/2.1",
        CCgiUserAgent::eCrawler,        { 2,  1, -1},
        CCgiUserAgent::eEngine_Bot,     {-1, -1, -1},
        { 5,  0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1b2) Gecko/20060821 SeaMonkey/1.1a",
        CCgiUserAgent::eSeaMonkey,      {  1,  1, -1},
        CCgiUserAgent::eEngine_Gecko,   {  1,  8,  1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/5.0 (Macintosh; U; PPC Mac OS X; it-it) AppleWebKit/412 (KHTML, like Gecko) Safari/412",
        CCgiUserAgent::eSafari,         {  2,  0, -1},
        CCgiUserAgent::eEngine_KHTML,   {412, -1, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Mac
    },
    { "Mozilla/5.0 (Macintosh; U; PPC Mac OS X; fr-fr) AppleWebKit/125.5.6 (KHTML, like Gecko) Safari/125.12",
        CCgiUserAgent::eSafari,         {  1,  2, -1},
        CCgiUserAgent::eEngine_KHTML,   {125,  5,  6},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Mac
    },
    { "Mozilla/5.0 (Macintosh; U; PPC Mac OS X; sv-se) AppleWebKit/85.7 (KHTML, like Gecko) Safari/85.5",
        CCgiUserAgent::eSafari,         {  1,  0, -1},
        CCgiUserAgent::eEngine_KHTML,   { 85,  7, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Mac
    },
    { "Mozilla/5.0 (Macintosh; U; PPC Mac OS X; en-US) AppleWebKit/125.4 (KHTML, like Gecko, Safari) OmniWeb/v563.51",
        CCgiUserAgent::eOmniWeb,        {563, 51, -1},
        CCgiUserAgent::eEngine_KHTML,   {125,  4, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Mac
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US) AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.2.149.27 Safari/525.13",
        CCgiUserAgent::eChrome,         {  0,  2,149},
        CCgiUserAgent::eEngine_KHTML,   {525, 13, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64; Trident/4.0; GTB7.1; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0; VER#99#80837681486745484888484867; BRI/2; .NET4.0C; 89870769803; Version/11.00284)",
        CCgiUserAgent::eIE,             {  8,  0, -1},
        CCgiUserAgent::eEngine_IE,      {  4,  0, -1},
        { 4, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },

    // AppComment tests

    { "Mozilla/5.0 (compatible; iCab 3.0.1; Macintosh; U; PPC Mac OS X)",
        CCgiUserAgent::eiCab,           { 3,  0,  1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Mac
    },
    { "Mozilla/5.0 (compatible; iCab 3.0.1)",
        CCgiUserAgent::eiCab,           { 3,  0,  1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Unknown
    },
    { "Mozilla/5.0 (compatible; Konqueror/3.1-rc3; i686 Linux; 20020515)",
        CCgiUserAgent::eKonqueror,      { 3,  1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Unix
    },
    { "Mozilla/4.0 (compatible; MSIE 6.0; MSN 2.5; Windows 98)",
        CCgiUserAgent::eIE,             { 6,  0, -1},
        CCgiUserAgent::eEngine_IE,      {-1, -1, -1},
        { 4, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/4.0 (compatible; MSIE 7.0b; Windows NT 6.0)",
        CCgiUserAgent::eIE,             { 7,  0, -1},
        CCgiUserAgent::eEngine_IE,      {-1, -1, -1},
        { 4, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/4.0 (compatible; MSIE 6.0(en); Windows NT 5.1; Avant Browser [avantbrowser.com]; iOpus-I-M; QXW03416; .NET CLR 1.1.4322)",
        CCgiUserAgent::eAvantBrowser,   {-1, -1, -1},
        CCgiUserAgent::eEngine_IE,      { 6,  0, -1},
        { 4, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/5.0 (Windows NT 6.3; Trident/7.0; rv:11.0) like Gecko", // Internet Explorer 11
        CCgiUserAgent::eIE,             {11,  0, -1},
        CCgiUserAgent::eEngine_IE,      { 7,  0, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },


    // Mozilla compatible

    { "Mozilla/5.0 (compatible; unknown; i686 Linux; 20020515)",
        CCgiUserAgent::eMozillaCompatible, {-1, -1, -1},
        CCgiUserAgent::eEngine_Unknown,    {-1, -1, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Unix
    },
    { "Mozilla/6.2 [en] (Windows NT 5.1; U)",
        CCgiUserAgent::eMozilla,        { 6,  2, -1},
        CCgiUserAgent::eEngine_Gecko,   {-1, -1, -1},
        { 6, 2, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/4.75 [en] (Win98; U)libwww-perl/5.41",
        CCgiUserAgent::eScript,         { 5, 41, -1},
        CCgiUserAgent::eEngine_Bot,     {-1, -1, -1},
        { 4, 75, -1},
        CCgiUserAgent::ePlatform_Windows
    },

     // Genuine Netscape/Mozilla

    { "Mozilla/4.7 [en] (WinNT; U)",
        CCgiUserAgent::eNetscape,       { 4,  7, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 4, 7, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/4.7C-CCK-MCD {C-UDP; EBM-APPLE} (Macintosh; I; PPC)",
        CCgiUserAgent::eNetscape,       { 4,  7, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 4, 7, -1},
        CCgiUserAgent::ePlatform_Mac
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; rv:1.0.1) Gecko/20020823 Netscape6/6.2.3",
        CCgiUserAgent::eNetscape,       { 6,  2,  3},
        CCgiUserAgent::eEngine_Gecko,   { 1,  0,  1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.4.1) Gecko/20031008",
        CCgiUserAgent::eMozilla,        { 5,  0, -1},
        CCgiUserAgent::eEngine_Gecko,   { 1,  4,  1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Unix
    },

     // AppProduct token tests

    { "Microsoft Internet Explorer/4.0b1 (Windows 95)",
        CCgiUserAgent::eIE,             { 4,  0, -1},
        CCgiUserAgent::eEngine_IE,      {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "Lynx/2.8.4rel.1 libwww-FM/2.14",
        CCgiUserAgent::eLynx,           { 2,  8,  4},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_Unknown
    },
    { "Avant Browser (http://www.avantbrowser.com)",
        CCgiUserAgent::eAvantBrowser,   {-1, -1, -1},
        CCgiUserAgent::eEngine_IE,      {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_Unknown
    },
    { "Opera/3.62 (Windows NT 5.0; U)  [en] (www.proxomitron.de)",
        CCgiUserAgent::eOpera,          { 3, 62, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_Windows
    },
    { "check_http/1.89 (nagios-plugins 1.4.3)",
        CCgiUserAgent::eNagios,         { 1,  89, -1},
        CCgiUserAgent::eEngine_Bot,     {-1,  -1, -1},
        { -1, -1, -1},
        CCgiUserAgent::ePlatform_Unix
    },

    // Mobile devices

    { "ASTEL/1.0/J-0511.00/c10/smel",
        CCgiUserAgent::eAirEdge,        { 1,  0, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "Mozilla/5.0 (compatible; AvantGo 3.2; ProxiNet; Danger hiptop 1.0)",
        CCgiUserAgent::eAvantGo,        { 3,  2, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 5,  0, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "DoCoMo/2.0 SH901iC(c100;TB;W24H12)",
        CCgiUserAgent::eDoCoMo,         { 2,  0, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "BlackBerry9630/4.7.1.40 Profile/MIDP-2.0 Configuration/CLDC-1.1 VendorID/105",
        CCgiUserAgent::eBlackberry,     {-1, -1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "Mozilla/5.0 (Linux; U; Android 1.6; en-fr; T-Mobile G1 Build/DRC83) AppleWebKit/528.5+ (KHTML, like Gecko) Version/3.1.2 Mobile Safari/525.20.1",
        CCgiUserAgent::eSafariMobile,   {  3, 1,  2},
        CCgiUserAgent::eEngine_KHTML,   {528, 5, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Android
    },
    { "Mozilla/5.0 (iPhone; U; CPU iPhone OS2_2 like Mac OS X;fr-fr) AppleWebKit/525.18.1 (KHTML, like Gecko) Version/3.1.1 Mobile/5G77 Safari/525.20",
        CCgiUserAgent::eSafariMobile,   {  3,  1, 1},
        CCgiUserAgent::eEngine_KHTML,   {525, 18, 1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "Mozilla/5.0 (Windows; U; Windows CE 5.1; rv:1.8.1a3) Gecko/20060610 Minimo/0.016",
        CCgiUserAgent::eMinimo,         { 0, 16, -1},
        CCgiUserAgent::eEngine_Gecko,   { 1,  8,  1},
        { 5,  0, -1},
        CCgiUserAgent::ePlatform_WindowsCE
    },
    { "Mozilla/4.0 (PDA; SL-C750/1.0,Embedix/Qtopia/1.3.0) NetFront/3.0 Zaurus C750",
        CCgiUserAgent::eNetFront,       { 3,  0, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 4,  0, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "ReqwirelessWeb/2.0.0 MIDP-1.0 CLDC-1.0 Nokia3650",
        CCgiUserAgent::eReqwireless,    { 2,  0,  0},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "Mozilla/4.0 (compatible; MSIE 6.0; Nokia7650) ReqwirelessWeb/2.0.0.0",
        CCgiUserAgent::eReqwireless,    { 2,  0,  0},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 4,  0, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "Opera/8.01 (J2ME/MIDP; Opera Mini/2.0.4509/1378; nl; U; ssr)",
        CCgiUserAgent::eOperaMini,      { 2,  0, 4509},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "Opera/9.51 Beta (Microsoft Windows; PPC; Opera Mobi/1718; U; en)",
        CCgiUserAgent::eOperaMobile,    {1718,  -1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "UP.Browser/3.04-TS14 UP.Link/3.4.4",
        CCgiUserAgent::eOpenWave,       { 3,  4, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "Mozilla/4.0 (compatible; MSIE 5.0; PalmOS) PLink 2.56b",
        CCgiUserAgent::ePocketLink,     { 2, 56, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 4,  0, -1},
        CCgiUserAgent::ePlatform_Palm
    },
    { "Vodafone/1.0/HTC_prophet/2.15.3.113/Mozilla/4.0 (compatible; MSIE 4.01; Windows CE; PPC; 240x320)",
        CCgiUserAgent::eVodafone,       { 1,  0, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_WindowsCE
    },
    { "Mozilla/5.0 (iPhone; U; CPU iPhone OS 3_0 like Mac OS X; en-us) AppleWebKit/420.1 (KHTML, like Gecko) Version/3.0 Mobile/1A542a Safari/419.3",
        CCgiUserAgent::eSafariMobile,   {  3,  0, -1},
        CCgiUserAgent::eEngine_KHTML,   {420,  1, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "Mozilla/5.0 (iPod; U; CPU iPhone OS 2_2_1 like Mac OS X; en-us) AppleWebKit/525.18.1 (KHTML, like Gecko) Mobile/5H11a",
        CCgiUserAgent::eMozilla,        { 5,  0,  -1},
        CCgiUserAgent::eEngine_KHTML,   {525, 18,  1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "Mozilla/5.0 (Linux; Android 4.2.1; Nexus 4 Build/JOP40D) AppleWebKit/535.19 (KHTML, like Gecko) Chrome/18.0.1025.166 Mobile Safari/535.19",
        CCgiUserAgent::eChrome,         { 18, 0,1025},
        CCgiUserAgent::eEngine_KHTML,   {535, 19, -1},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_Android
    },


    // Platform tests

    { "Mozilla/2.0 (compatible; MSIE 3.02; Windows CE; PPC; 240x320)",
        CCgiUserAgent::eIE,             { 3,  2, -1},
        CCgiUserAgent::eEngine_IE,      {-1, -1, -1},
        { 2,  0, -1},
        CCgiUserAgent::ePlatform_WindowsCE
    },
    { "Mozilla/4.76 [en] (PalmOS; U; WebPro/3.0.1a; Palm-Cct1)",
        CCgiUserAgent::eNetscape,       { 4, 76, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 4, 76, -1},
        CCgiUserAgent::ePlatform_Palm
    },
    { "Doris/1.86 [en] (Symbian)",
        CCgiUserAgent::eUnknown,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_Symbian
    },
    { "Mozilla/4.1 (compatible; MSIE 5.0; Symbian OS; Nokia 3650;424) Opera 6.10  [en]",
        CCgiUserAgent::eOpera,          { 6, 10, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 4,  1, -1},
        CCgiUserAgent::ePlatform_Symbian
    },
    { "LG/U8138/v2.0",
        CCgiUserAgent::eUnknown,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "MOT-L6/0A.52.45R MIB/2.2.1 Profile/MIDP-2.0 Configuration/CLDC-1.1",
        CCgiUserAgent::eUnknown,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "J-PHONE/5.0/V801SA/SN123456789012345 SA/0001JP Profile/MIDP-1.0",
        CCgiUserAgent::eUnknown,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    },
    { "Mozilla/5.0 (iPad; U; CPU OS 3_2 like Mac OS X; en-us) AppleWebKit/531.21.10 (KHTML, like Gecko) Version/4.0.4 Mobile/7B334b Safari/531.21.10",
        CCgiUserAgent::eSafariMobile,   {   4, 0,  4},
        CCgiUserAgent::eEngine_KHTML,   {531, 21, 10},
        { 5, 0, -1},
        CCgiUserAgent::ePlatform_MobileDevice
    }
};


void s_PrintUserAgentVersion(const string& name, TUserAgentVersion& v)
{
    cout << name;
    if ( v.GetMajor() >= 0 ) {
        cout << v.GetMajor();
        if ( v.GetMinor() >= 0 ) {
            cout << "." << v.GetMinor();
            if ( v.GetPatchLevel() >= 0 ) {
                cout << "." << v.GetPatchLevel();
            }
        }
    }
    cout << endl;
}


void TestUserAgent(CCgiUserAgent::TFlags flags)
{
    CCgiUserAgent agent(flags);

    for (size_t i=0; i<sizeof(s_UserAgentTests)/sizeof(s_UserAgentTests[0]); i++) {
        const SUserAgent* a = &s_UserAgentTests[i];
        cout << a->str << endl; 
        agent.Reset(a->str);

        TUserAgentVersion v;

        // Browser version

        CCgiUserAgent::EBrowser b = agent.GetBrowser();
        string b_name = agent.GetBrowserName();
        cout << "Browser        : " << (b_name.empty() ? string("n/a") : b_name) << " (" << b << ")" << endl;
        v = agent.GetBrowserVersion();
        s_PrintUserAgentVersion("Version        : ", v);
        assert(a->browser == b);
        assert(a->browser_v.major == v.GetMajor());
        assert(a->browser_v.minor == v.GetMinor());
        assert(a->browser_v.patch == v.GetPatchLevel());

        // Engine version

        CCgiUserAgent::EBrowserEngine e = agent.GetEngine();
        v = agent.GetEngineVersion();
        cout << "Engine         : " << e << endl;
        s_PrintUserAgentVersion("Engine version : ", v);
        assert(a->engine == e);
        assert(a->engine_v.major == v.GetMajor());
        assert(a->engine_v.minor == v.GetMinor());
        assert(a->engine_v.patch == v.GetPatchLevel());

        // Mozilla-compatible version

        v = agent.GetMozillaVersion();
        s_PrintUserAgentVersion("Mozilla version: ", v);
        assert(a->mozilla_v.major == v.GetMajor());
        assert(a->mozilla_v.minor == v.GetMinor());
        assert(a->mozilla_v.patch == v.GetPatchLevel());

        // Platform
        CCgiUserAgent::EBrowserPlatform p = agent.GetPlatform();
        cout << "Platform       : " << p << endl;
        assert(a->platform == p);

        cout << endl;
    }

    // IsBot() -- simple test
    {{
        agent.Reset("Mozilla/4.75 [en] (Win98; U)libwww-perl/5.41");
        assert(agent.GetBrowser() == CCgiUserAgent::eScript);
        assert(agent.GetEngine()  == CCgiUserAgent::eEngine_Bot);
        assert(agent.IsBot());
        // Treat all as bots, except scripts
        assert(!agent.IsBot(CCgiUserAgent::fBotAll & ~CCgiUserAgent::fBotScript));
        // Treat "libwww" as not bot
        assert(!agent.IsBot(CCgiUserAgent::fBotAll, kEmptyStr, "libwww"));

        agent.Reset("Mozilla/4.75 (SomeNewBot/1.2.3)");
        assert(agent.GetBrowser() == CCgiUserAgent::eNetscape);
        assert(agent.GetEngine()  == CCgiUserAgent::eEngine_Unknown);
        assert(!agent.IsBot());
        assert(agent.IsBot(CCgiUserAgent::fBotAll, "SomeNewCrawler SomeNewBot SomeOtherBot"));
    }}

    // Device type simple test
    {{
        agent.Reset("Mozilla/5.0 (compatible; AvantGo 3.2; ProxiNet; Danger hiptop 1.0)");
        assert(agent.GetBrowser() == CCgiUserAgent::eAvantGo);
        assert(agent.GetPlatform()== CCgiUserAgent::ePlatform_MobileDevice);
        assert(agent.IsPhoneDevice());
        assert(agent.IsTabletDevice());
        assert(agent.IsMobileDevice());

        agent.Reset("Mozilla/5.0 (Windows; U; Windows CE 5.1; rv:1.8.1a3) Gecko/20060610 Minimo/0.016");
        assert(agent.GetBrowser() == CCgiUserAgent::eMinimo);
        assert(agent.GetPlatform()== CCgiUserAgent::ePlatform_WindowsCE);
        assert(agent.IsPhoneDevice());
        assert(!agent.IsTabletDevice());
        assert(agent.IsMobileDevice());

        agent.Reset("Mozilla/4.0 (PDA; PalmOS/sony/model prmr/Revision:1.1.54 (en))");
        assert(agent.GetBrowser() == CCgiUserAgent::eNetscape);
        assert(agent.GetPlatform()== CCgiUserAgent::ePlatform_Palm);
        assert(agent.IsPhoneDevice());
        assert(!agent.IsTabletDevice());
        assert(agent.IsMobileDevice());
        assert(!agent.IsMobileDevice(kEmptyStr, "somestr prmr someother"));

        agent.Reset("Mozilla/5.0 (SomeNewSmartphone/1.2.3)");
        assert(agent.GetBrowser() == CCgiUserAgent::eMozilla);
        assert(agent.GetPlatform()== CCgiUserAgent::ePlatform_Unknown);
        assert(!agent.IsPhoneDevice());
        assert(agent.IsPhoneDevice("SomePDA SomeNewSmartphone iAnything"));
        assert(!agent.IsTabletDevice());
        assert(!agent.IsMobileDevice());
        assert(agent.IsMobileDevice("SomePDA SomeNewSmartphone iAnything"));

        agent.Reset("Mozilla/5.0 (Android; Tablet; rv:12.0) Gecko/12.0 Firefox/12.0");
        assert(agent.GetBrowser() == CCgiUserAgent::eFirefox);
        assert(agent.GetPlatform()== CCgiUserAgent::ePlatform_Android);
        assert(!agent.IsPhoneDevice());
        assert(agent.IsTabletDevice());
        assert(agent.IsMobileDevice());

        agent.Reset("Mozilla/5.0 (Linux; Android 4.2.2; Nexus Build/JDQ39) AppleWebKit/537.31 (KHTML, like Gecko) Chrome/26.0.1410.58 Safari/537.31");
        assert(agent.GetBrowser() == CCgiUserAgent::eChrome);
        assert(agent.GetPlatform()== CCgiUserAgent::ePlatform_Android);
        assert(!agent.IsPhoneDevice());
        assert(agent.IsTabletDevice());
        assert(agent.IsMobileDevice());

        agent.Reset("Mozilla/5.0 (iPad; U; CPU OS 3_2 like Mac OS X; en-us) AppleWebKit/531.21.10 (KHTML, like Gecko) Version/4.0.4 Mobile/7B334b Safari/531.21.10");
        assert(agent.GetBrowser() == CCgiUserAgent::eSafariMobile);
        assert(agent.GetPlatform()== CCgiUserAgent::ePlatform_MobileDevice);
        assert(!agent.IsPhoneDevice());
        assert(agent.IsTabletDevice());
        assert(agent.IsMobileDevice());
    }}
}


class CTestApplication : public CNcbiApplication
{
public:
    virtual ~CTestApplication(void);
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
    TestUserAgent(0);
    TestUserAgent(CCgiUserAgent::fNoCase);
    return 0;
}


CTestApplication::~CTestApplication()
{
    SetDiagStream(0);
}


int main(int argc, char** argv)
{
    return CTestApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
