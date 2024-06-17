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
 *   TEST for:  CUrl class
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_url.hpp>

#define BOOST_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <random>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


const char* kScheme = "scheme";
const char* kSchemeLB = NCBI_SCHEME_SERVICE;
const char* kUser = "user";
const char* kPassword = "password";
const char* kHostShort = "hostname";
const char* kHostFull = "host.net";
const char* kHostIPv6 = "[1:2:3:4]";
const char* kServiceSimple = "servicename";
const char* kServiceSpecial = "/service/name";
const char* kPort = "1234";
const char* kPathAbs = "/path/file";
const char* kPathRel = "rel/path/file";
const char* kArgs = "arg1=val1&arg2=val2";
const char* kFragment = "fragment";


enum EUrlParts {
    fHostNone       = 0,
    fHostShort      = 0x0001,
    fHostFull       = 0x0002,
    fHostIPv6       = 0x0003,
    fHostMask       = 0x0003,
    fService        = 0x0004,
    fServiceSimple  = 0,
    fServiceSpecial = 0x0001,
    fAuthorityMask  = 0x0007, // host or service

    fScheme         = 0x0008,
    fUser           = 0x0010,
    fPassword       = 0x0020,
    fPort           = 0x0040,

    fPathNone       = 0,
    fPathRoot       = 0x0080,
    fPathAbs        = 0x0100,
    fPathRel        = 0x0180,
    fPathMask       = 0x0180,

    fArgs           = 0x0200,
    fFragment       = 0x0400,

    fMax            = 0x0800
};


void s_TestUrl(string url_str, int flags)
{
    CUrl url(url_str);
    if (url.GetIsGeneric()) {
        BOOST_CHECK(url_str.find("//") != NPOS);
    }
    else {
        BOOST_CHECK(url_str.find("//") == NPOS);
    }

    if (flags & fScheme) {
        BOOST_CHECK_EQUAL(url.GetScheme(), kScheme);
    }
    else {
        BOOST_CHECK(url.GetScheme().empty());
    }

    if (flags & fService) {
        BOOST_CHECK(url.GetHost().empty());
        BOOST_CHECK(url.IsService());
        switch (flags & fHostMask) {
        case fServiceSimple:
            BOOST_CHECK_EQUAL(url.GetService(), kServiceSimple);
            break;
        case fServiceSpecial:
            BOOST_CHECK_EQUAL(url.GetService(), kServiceSpecial);
            break;
        default:
            BOOST_ERROR("Invalid service type flag");
        }
    }
    else {
        BOOST_CHECK(url.GetService().empty());
        switch (flags & fHostMask) {
        case fHostNone:
            BOOST_CHECK(url.GetHost().empty());
            break;
        case fHostShort:
            BOOST_CHECK_EQUAL(url.GetHost(), kHostShort);
            break;
        case fHostFull:
            BOOST_CHECK_EQUAL(url.GetHost(), kHostFull);
            break;
        case fHostIPv6:
            BOOST_CHECK_EQUAL(url.GetHost(), kHostIPv6);
            break;
        default:
            BOOST_ERROR("Invalid host type flag");
            break;
        }
    }

    if (flags & fUser) {
        BOOST_CHECK_EQUAL(url.GetUser(), kUser);
    }
    else {
        BOOST_CHECK(url.GetUser().empty());
    }

    if (flags & fPassword) {
        BOOST_CHECK_EQUAL(url.GetPassword(), kPassword);
    }
    else {
        BOOST_CHECK(url.GetPassword().empty());
    }

    if (flags & fPort) {
        BOOST_CHECK_EQUAL(url.GetPort(), kPort);
    }
    else {
        BOOST_CHECK(url.GetPort().empty());
    }

    switch (flags & fPathMask) {
    case fPathNone:
        BOOST_CHECK(url.GetPath().empty());
        break;
    case fPathRoot:
        BOOST_CHECK_EQUAL(url.GetPath(), "/");
        break;
    case fPathAbs:
        BOOST_CHECK_EQUAL(url.GetPath(), kPathAbs);
        break;
    case fPathRel:
        BOOST_CHECK_EQUAL(url.GetPath(), kPathRel);
        break;
    default:
        BOOST_ERROR("Invalid path type flag");
    }

    if (flags & fArgs) {
        BOOST_CHECK_EQUAL(url.GetArgs().GetQueryString(CUrlArgs::eAmp_Char), kArgs);
    }
    else {
        BOOST_CHECK(url.GetArgs().GetArgs().empty());
    }


    if (flags & fFragment) {
        BOOST_CHECK_EQUAL(url.GetFragment(), kFragment);
    }
    else {
        BOOST_CHECK(url.GetFragment().empty());
    }

    BOOST_CHECK_EQUAL(url_str, url.ComposeUrl(CUrlArgs::eAmp_Char));
}


BOOST_AUTO_TEST_CASE(s_UrlTestParsed)
{
    cout << "*** Testing parsed URLs" << endl;
    string surl;
    for (int flags = 0; flags < fMax; ++flags) {
        surl.clear();
        if (flags & fScheme) {
            // Skip scheme-only URLs.
            if (flags == fScheme) continue;
            surl = ((flags & fService) ? string(kScheme) + "+" + kSchemeLB : kScheme);
            // When testing relative path, do not include "//".
            surl += !(flags & (fHostMask | fService)) ? ":" : "://";
        }
        else if (flags & fService) {
            // Simple service URLs do not require ncbilb scheme.
            if (flags != fService) {
                surl = string(kSchemeLB) + "://";
            }
        }
        else if ((flags & fHostMask) != fHostNone) {
            // If host is present, the authority part must start with "//".
            surl = "//";
        }

        string authority;
        if (flags & fService) {
            // Port is not allowed for services.
            if (flags & fPort) continue;
            switch (flags & fHostMask) {
            case fServiceSimple:
                authority = kServiceSimple;
                break;
            case fServiceSpecial:
                authority = NStr::URLEncode(kServiceSpecial);
                break;
            default:
                continue; // invalid service type
            }
        }
        else {
            switch (flags & fHostMask) {
            case fHostNone:
                break;
            case fHostShort:
                authority = kHostShort;
                break;
            case fHostFull:
                authority = kHostFull;
                break;
            case fHostIPv6:
                authority = kHostIPv6;
                break;
            default:
                continue; // unused host type
            }
            if (flags & fPort) {
                authority += string(":") + kPort;
            }
        }

        // Do not test user info and port if host/service is missing
        if (!(flags & fAuthorityMask)) {
            if (flags & (fUser | fPassword | fPort)) continue;
        }

        string user_info;
        if (flags & fUser) {
            user_info = kUser;
        }
        if (flags & fPassword) {
            // TODO: Is password allowed without user?
            user_info += string(":") + kPassword;
        }
        if (!user_info.empty()) {
            authority = user_info + "@" + authority;
        }
        surl += authority;

        switch (flags & fPathMask) {
        case fPathNone:
            break;
        case fPathRoot:
            surl += "/";
            break;
        case fPathAbs:
            surl += kPathAbs;
            break;
        case fPathRel:
            // Relative path can be combined only with scheme
            if (flags & ~(fScheme | fPathRel)) continue;
            surl += kPathRel;
            break;
        default:
            continue;
        }

        if (flags & fArgs) {
            surl += string("?") + kArgs;
        }

        string fragment;
        if (flags & fFragment) {
            surl += string("#") + kFragment;
        }

        cout << "Testing: " << surl << endl;
        s_TestUrl(surl, flags);
    }
}


struct SCUrlTest {
    string  m_Expected;
    string  m_UrlString;
    string  m_Scheme;
    string  m_Service;
    string  m_Host;
    string  m_Path;
    bool    m_Generic;

    void Compare(const CUrl& url)
    {
        string result(url.ComposeUrl(CUrlArgs::eAmp_Char));
        BOOST_CHECK_EQUAL(m_Expected, result);
        BOOST_CHECK_EQUAL(m_Scheme, url.GetScheme());
        BOOST_CHECK_EQUAL(!m_Service.empty(), url.IsService());
        BOOST_CHECK_EQUAL(m_Service, url.GetService());
        BOOST_CHECK_EQUAL(m_Host, url.GetHost());
        BOOST_CHECK_EQUAL(m_Path, url.GetPath());
        BOOST_CHECK_EQUAL(m_Generic, url.GetIsGeneric());
    }
};


static SCUrlTest s_CUrlTests[] = {
    { "service",                            "service",                              "",         "service",  "",     "",             false },
    { "Some/path",                          "Some/path",                            "",         "",         "",     "Some/path",    false },
    { "//host",                             "//host",                               "",         "",         "host", "",             true },
    { "//host/Some/path",                   "//host/Some/path",                     "",         "",         "host", "/Some/path",   true },
    { "http://host",                        "http://host",                          "http",     "",         "host", "",             true },
    { "http://host/Some/path",              "http://host/Some/path",                "http",     "",         "host", "/Some/path",   true },

    { "ncbilb://service",                   "ncbilb://service",                     "",         "service",  "",     "",             true },
    { "ncbilb://service/Some/path",         "ncbilb://service/Some/path",           "",         "service",  "",     "/Some/path",   true },
    { "http+ncbilb://service",              "http+ncbilb://service",                "http",     "service",  "",     "",             true },
    { "http+ncbilb://service/Some/path",    "http+ncbilb://service/Some/path",      "http",     "service",  "",     "/Some/path",   true },

    { "scheme:Some/path",                   "scheme:Some/path",                     "scheme",   "",         "",     "Some/path",    false },
    { "scheme://host",                      "scheme://host",                        "scheme",   "",         "host", "",             true },
    { "scheme://host/Some/path",            "scheme://host/Some/path",              "scheme",   "",         "host", "/Some/path",   true },
    { "scheme+ncbilb://service",            "scheme+ncbilb://service",              "scheme",   "service",  "",     "",             true },
    { "scheme+ncbilb://service/Some/path",  "scheme+ncbilb://service/Some/path",    "scheme",   "service",  "",     "/Some/path",   true }
};


BOOST_AUTO_TEST_CASE(s_UrlTestComposed)
{
    cout << "*** Testing composed URLs" << endl;
    for (auto test : s_CUrlTests) {
        cout << "Testing: " << test.m_UrlString << endl;
        CUrl purl(test.m_UrlString);
        test.Compare(purl);

        CUrl curl;
        if (!test.m_Scheme.empty())   curl.SetScheme(test.m_Scheme);
        if (!test.m_Service.empty())  curl.SetService(test.m_Service);
        if (!test.m_Host.empty())     curl.SetHost(test.m_Host);
        if (!test.m_Path.empty())     curl.SetPath(test.m_Path);
        curl.SetIsGeneric(test.m_Generic);
        test.Compare(curl);
    }
}


BOOST_AUTO_TEST_CASE(s_UrlTestSchemelessWithDoubleSlashes)
{
    CUrl url;
    url.SetUrl("psg22.be-md.ncbi.nlm.nih.gov:10010"
               "/ID/get?"
               "seq_id_type=11&"
               "seq_id=gnl|O1318|G1XDD-134-MONOMER+Putative+NADH+dehydrogenase//Transferred+entry:+1.6.5.9+73737..75335+Thiomonas+sp.+CB2+9462&"
               "tse=none&resend_timeout=0&"
               "include_hup=no&"
               "client_id=EB920B6C64B65361&"
               "disable_processor=CDD&"
               "disable_processor=OSG&"
               "disable_processor=SNP&"
               "disable_processor=WGS&"
               "hops=1");

    BOOST_CHECK_EQUAL(url.GetScheme(), "");
    BOOST_CHECK_EQUAL(url.IsService(), false);
    BOOST_CHECK_EQUAL(url.GetPort(), "10010");
    BOOST_CHECK_EQUAL(url.GetHost(), "psg22.be-md.ncbi.nlm.nih.gov");
    BOOST_CHECK_EQUAL(url.GetPath(), "/ID/get");
    BOOST_CHECK_EQUAL(url.HaveArgs(), true);

    auto &  args = url.GetArgs().GetArgs();
    BOOST_CHECK_EQUAL(args.size(), 11U);

    auto    it = args.begin();
    BOOST_CHECK_EQUAL(it->name, "seq_id_type");
    BOOST_CHECK_EQUAL(it->value, "11");

    ++it;
    BOOST_CHECK_EQUAL(it->name, "seq_id");
    BOOST_CHECK_EQUAL(it->value, "gnl|O1318|G1XDD-134-MONOMER Putative NADH dehydrogenase//Transferred entry: 1.6.5.9 73737..75335 Thiomonas sp. CB2 9462");

    ++it;
    BOOST_CHECK_EQUAL(it->name, "tse");
    BOOST_CHECK_EQUAL(it->value, "none");

    ++it;
    BOOST_CHECK_EQUAL(it->name, "resend_timeout");
    BOOST_CHECK_EQUAL(it->value, "0");

    ++it;
    BOOST_CHECK_EQUAL(it->name, "include_hup");
    BOOST_CHECK_EQUAL(it->value, "no");

    ++it;
    BOOST_CHECK_EQUAL(it->name, "client_id");
    BOOST_CHECK_EQUAL(it->value, "EB920B6C64B65361");

    ++it;
    BOOST_CHECK_EQUAL(it->name, "disable_processor");
    BOOST_CHECK_EQUAL(it->value, "CDD");

    ++it;
    BOOST_CHECK_EQUAL(it->name, "disable_processor");
    BOOST_CHECK_EQUAL(it->value, "OSG");

    ++it;
    BOOST_CHECK_EQUAL(it->name, "disable_processor");
    BOOST_CHECK_EQUAL(it->value, "SNP");

    ++it;
    BOOST_CHECK_EQUAL(it->name, "disable_processor");
    BOOST_CHECK_EQUAL(it->value, "WGS");

    ++it;
    BOOST_CHECK_EQUAL(it->name, "hops");
    BOOST_CHECK_EQUAL(it->value, "1");
}


BOOST_AUTO_TEST_CASE(s_UrlTestSpecial)
{
    cout << "*** Testing special cases" << endl;
    // Service-only URL may include ncbilb scheme.
    string surl = string(kSchemeLB) + "://" + kServiceSimple;
    cout << "Testing: " << surl << endl;
    s_TestUrl(surl, fService);

    {
        cout << "Testing: ignore ncbilb scheme"<< endl;
        CUrl url;
        url.SetService(kServiceSimple);
        url.SetScheme(kSchemeLB);
        BOOST_CHECK(url.IsService());
        BOOST_CHECK_EQUAL(url.GetService(), kServiceSimple);
        BOOST_CHECK(url.GetScheme().empty());
    }
    {
        cout << "Testing: ignore ncbilb scheme / switch to service" << endl;
        CUrl url;
        url.SetHost(kServiceSimple);
        url.SetScheme(kSchemeLB);
        BOOST_CHECK(url.IsService());
        BOOST_CHECK_EQUAL(url.GetService(), kServiceSimple);
        BOOST_CHECK(url.GetScheme().empty());
    }
    {
        cout << "Testing: strip +ncbilb scheme" << endl;
        CUrl url;
        url.SetService(kServiceSimple);
        url.SetScheme(string(kScheme) + "+" + kSchemeLB);
        BOOST_CHECK(url.IsService());
        BOOST_CHECK_EQUAL(url.GetService(), kServiceSimple);
        BOOST_CHECK_EQUAL(url.GetScheme(), kScheme);
    }
    {
        cout << "Testing: strip ncbilb scheme / switch to service" << endl;
        CUrl url;
        url.SetHost(kServiceSimple);
        url.SetScheme(string(kScheme) + "+" + kSchemeLB);
        BOOST_CHECK(url.IsService());
        BOOST_CHECK_EQUAL(url.GetService(), kServiceSimple);
        BOOST_CHECK_EQUAL(url.GetScheme(), kScheme);
    }

    cout << "Testing: parse host:port/path" << endl;
    {
        CUrl url(string(kHostShort) + ":" + kPort);
        BOOST_CHECK(url.GetScheme().empty());
        BOOST_CHECK(url.GetUser().empty());
        BOOST_CHECK(url.GetPassword().empty());
        BOOST_CHECK_EQUAL(url.GetHost(), kHostShort);
        BOOST_CHECK_EQUAL(url.GetPort(), kPort);
        BOOST_CHECK(url.GetPath().empty());
    }
    {
        CUrl url(string(kHostFull) + ":" + kPort);
        BOOST_CHECK(url.GetScheme().empty());
        BOOST_CHECK(url.GetUser().empty());
        BOOST_CHECK(url.GetPassword().empty());
        BOOST_CHECK_EQUAL(url.GetHost(), kHostFull);
        BOOST_CHECK_EQUAL(url.GetPort(), kPort);
        BOOST_CHECK(url.GetPath().empty());
    }
    {
        CUrl url(string(kHostShort) + ":" + kPort + kPathAbs);
        BOOST_CHECK(url.GetScheme().empty());
        BOOST_CHECK(url.GetUser().empty());
        BOOST_CHECK(url.GetPassword().empty());
        BOOST_CHECK_EQUAL(url.GetHost(), kHostShort);
        BOOST_CHECK_EQUAL(url.GetPort(), kPort);
        BOOST_CHECK_EQUAL(url.GetPath(), kPathAbs);
    }
    {
        CUrl url(string(kHostFull) + ":" + kPort + kPathAbs);
        BOOST_CHECK(url.GetScheme().empty());
        BOOST_CHECK(url.GetUser().empty());
        BOOST_CHECK(url.GetPassword().empty());
        BOOST_CHECK_EQUAL(url.GetHost(), kHostFull);
        BOOST_CHECK_EQUAL(url.GetPort(), kPort);
        BOOST_CHECK_EQUAL(url.GetPath(), kPathAbs);
    }
    {
        CUrl url("http:123");
        BOOST_CHECK_EQUAL(url.GetScheme(), "http");
        BOOST_CHECK(url.GetUser().empty());
        BOOST_CHECK(url.GetPassword().empty());
        BOOST_CHECK(url.GetHost().empty());
        BOOST_CHECK(url.GetPort().empty());
        BOOST_CHECK_EQUAL(url.GetPath(), "123");
    }
    {
        CUrl url("https:123");
        BOOST_CHECK_EQUAL(url.GetScheme(), "https");
        BOOST_CHECK(url.GetUser().empty());
        BOOST_CHECK(url.GetPassword().empty());
        BOOST_CHECK(url.GetHost().empty());
        BOOST_CHECK(url.GetPort().empty());
        BOOST_CHECK_EQUAL(url.GetPath(), "123");
    }
    {
        CUrl url("file:123");
        BOOST_CHECK_EQUAL(url.GetScheme(), "file");
        BOOST_CHECK(url.GetUser().empty());
        BOOST_CHECK(url.GetPassword().empty());
        BOOST_CHECK(url.GetHost().empty());
        BOOST_CHECK(url.GetPort().empty());
        BOOST_CHECK_EQUAL(url.GetPath(), "123");
    }
    {
        CUrl url("ftp:123");
        BOOST_CHECK_EQUAL(url.GetScheme(), "ftp");
        BOOST_CHECK(url.GetUser().empty());
        BOOST_CHECK(url.GetPassword().empty());
        BOOST_CHECK(url.GetHost().empty());
        BOOST_CHECK(url.GetPort().empty());
        BOOST_CHECK_EQUAL(url.GetPath(), "123");
    }
    {
        CUrl url(string(kScheme) + ":0123");
        BOOST_CHECK_EQUAL(url.GetScheme(), kScheme);
        BOOST_CHECK(url.GetUser().empty());
        BOOST_CHECK(url.GetPassword().empty());
        BOOST_CHECK(url.GetHost().empty());
        BOOST_CHECK(url.GetPort().empty());
        BOOST_CHECK_EQUAL(url.GetPath(), "0123");
    }
    {
        CUrl url(string(kScheme) + ":76543");
        BOOST_CHECK_EQUAL(url.GetScheme(), kScheme);
        BOOST_CHECK(url.GetUser().empty());
        BOOST_CHECK(url.GetPassword().empty());
        BOOST_CHECK(url.GetHost().empty());
        BOOST_CHECK(url.GetPort().empty());
        BOOST_CHECK_EQUAL(url.GetPath(), "76543");
    }
}

struct SUrlTestAdjust
{
    enum EPartElement { eUrl, eOther, eFlags, eExpected };
    using TPart = tuple<string, string, CUrl::TAdjustFlags, string>;

    template <int element>
    static CUrl CreateUrl(
            const TPart& scheme,
            const TPart& user,
            const TPart& password,
            const char*  host,
            const char*  port,
            const TPart& path,
            const TPart& arg,
            const TPart& fragment)
    {
        CUrl rv;
        rv.SetIsGeneric(true);
        rv.SetScheme(  get<element>(scheme));
        rv.SetUser(    get<element>(user));
        rv.SetPassword(get<element>(password));
        rv.SetHost(host);
        rv.SetPort(port);
        rv.SetPath(    get<element>(path));
        rv.GetArgs() = get<element>(arg);
        rv.SetFragment(get<element>(fragment));
        return rv;
    }

    static void Test(
            const TPart& scheme,
            const TPart& user,
            const TPart& password,
            const TPart& path,
            const TPart& arg,
            const TPart& fragment)
    {
        auto url      = CreateUrl<eUrl>     (scheme, user, password, "host1", "1", path, arg, fragment);
        auto other    = CreateUrl<eOther>   (scheme, user, password, "host2", "2", path, arg, fragment);
        auto expected = CreateUrl<eExpected>(scheme, user, password, "host1", "1", path, arg, fragment);

        auto url_str      = url.     ComposeUrl(CUrlArgs::eAmp_Char);
        auto other_str    = other.   ComposeUrl(CUrlArgs::eAmp_Char);
        auto expected_str = expected.ComposeUrl(CUrlArgs::eAmp_Char);

        auto flags = get<eFlags>(scheme) | get<eFlags>(user) | get<eFlags>(password) | get<eFlags>(path) | get<eFlags>(arg) | get<eFlags>(fragment);
        url.Adjust(other, flags);

        auto adjusted_str = url.ComposeUrl(CUrlArgs::eAmp_Char);

        if (expected_str != adjusted_str) {
            BOOST_ERROR("Adjusted URL does not match expected one." <<
                    "\nurl:      " << url_str <<
                    "\nother:    " << other_str <<
                    "\nexpected: " << expected_str <<
                    "\nadjusted: " << adjusted_str <<
                    "\nflags:    " << flags << '\n');
        }
    }
};

BOOST_FIXTURE_TEST_CASE(s_UrlTestAdjust, SUrlTestAdjust)
{
    vector<TPart> schemes =
    {
        { "",        "scheme2", 0,                     ""        },
        { "",        "scheme2", CUrl::fScheme_Replace, "scheme2" },
        { "scheme1", "",        0,                     "scheme1" },
        { "scheme1", "",        CUrl::fScheme_Replace, "scheme1" },
        { "scheme1", "scheme2", 0,                     "scheme1" },
        { "scheme1", "scheme2", CUrl::fScheme_Replace, "scheme2" },
    };
    vector<TPart> users =
    {
        { "",      "user2", 0,                          ""      },
        { "",      "user2", CUrl::fUser_Replace,        "user2" },
        { "",      "user2", CUrl::fUser_ReplaceIfEmpty, "user2" },
        { "user1", "",      0,                          "user1" },
        { "user1", "",      CUrl::fUser_Replace,        "user1" },
        { "user1", "",      CUrl::fUser_ReplaceIfEmpty, "user1" },
        { "user1", "user2", 0,                          "user1" },
        { "user1", "user2", CUrl::fUser_Replace,        "user2" },
        { "user1", "user2", CUrl::fUser_ReplaceIfEmpty, "user1" },
    };
    vector<TPart> passwords =
    {
        { "",          "password2", 0,                              ""          },
        { "",          "password2", CUrl::fPassword_Replace,        "password2" },
        { "",          "password2", CUrl::fPassword_ReplaceIfEmpty, "password2" },
        { "password1", "",          0,                              "password1" },
        { "password1", "",          CUrl::fPassword_Replace,        "password1" },
        { "password1", "",          CUrl::fPassword_ReplaceIfEmpty, "password1" },
        { "password1", "password2", 0,                              "password1" },
        { "password1", "password2", CUrl::fPassword_Replace,        "password2" },
        { "password1", "password2", CUrl::fPassword_ReplaceIfEmpty, "password1" },
    };
    vector<TPart> paths =
    {
        { "",       "/",      0,                   ""             },
        { "",       "/",      CUrl::fPath_Replace, "/"            },
        { "",       "/",      CUrl::fPath_Append,  "/"            },
        { "",       "path2",  0,                   ""             },
        { "",       "path2",  CUrl::fPath_Replace, "path2"        },
        { "",       "path2",  CUrl::fPath_Append,  "path2"        },
        { "",       "/path2", 0,                   ""             },
        { "",       "/path2", CUrl::fPath_Replace, "/path2"       },
        { "",       "/path2", CUrl::fPath_Append,  "/path2"       },
        { "/",      "",       0,                   "/"            },
        { "/",      "",       CUrl::fPath_Replace, ""             },
        { "/",      "",       CUrl::fPath_Append,  "/"            },
        { "/",      "path2",  0,                   "/"            },
        { "/",      "path2",  CUrl::fPath_Replace, "path2"        },
        { "/",      "path2",  CUrl::fPath_Append,  "/path2"       },
        { "/",      "/path2", 0,                   "/"            },
        { "/",      "/path2", CUrl::fPath_Replace, "/path2"       },
        { "/",      "/path2", CUrl::fPath_Append,  "/path2"       },
        { "path1",  "",       0,                   "path1"        },
        { "path1",  "",       CUrl::fPath_Replace, ""             },
        { "path1",  "",       CUrl::fPath_Append,  "path1"        },
        { "path1",  "/",      0,                   "path1"        },
        { "path1",  "/",      CUrl::fPath_Replace, "/"            },
        { "path1",  "/",      CUrl::fPath_Append,  "path1/"       },
        { "path1",  "path2",  0,                   "path1"        },
        { "path1",  "path2",  CUrl::fPath_Replace, "path2"        },
        { "path1",  "path2",  CUrl::fPath_Append,  "path1/path2"  },
        { "path1",  "/path2", 0,                   "path1"        },
        { "path1",  "/path2", CUrl::fPath_Replace, "/path2"       },
        { "path1",  "/path2", CUrl::fPath_Append,  "path1/path2"  },
        { "/path1", "",       0,                   "/path1"       },
        { "/path1", "",       CUrl::fPath_Replace, ""             },
        { "/path1", "",       CUrl::fPath_Append,  "/path1"       },
        { "/path1", "/",      0,                   "/path1"       },
        { "/path1", "/",      CUrl::fPath_Replace, "/"            },
        { "/path1", "/",      CUrl::fPath_Append,  "/path1/"      },
        { "/path1", "path2",  0,                   "/path1"       },
        { "/path1", "path2",  CUrl::fPath_Replace, "path2"        },
        { "/path1", "path2",  CUrl::fPath_Append,  "/path1/path2" },
        { "/path1", "/path2", 0,                   "/path1"       },
        { "/path1", "/path2", CUrl::fPath_Replace, "/path2"       },
        { "/path1", "/path2", CUrl::fPath_Append,  "/path1/path2" },
    };
    vector<TPart> args =
    {
        { "",       "arg1=1", 0,                   ""              },
        { "",       "arg1=1", CUrl::fArgs_Replace, "arg1=1"        },
        { "",       "arg1=1", CUrl::fArgs_Append,  "arg1=1"        },
        { "",       "arg1=1", CUrl::fArgs_Merge,   "arg1=1"        },
        { "arg1=1", "",       0,                   "arg1=1"        },
        { "arg1=1", "",       CUrl::fArgs_Replace, ""              },
        { "arg1=1", "",       CUrl::fArgs_Append,  "arg1=1"        },
        { "arg1=1", "",       CUrl::fArgs_Merge,   "arg1=1"        },
        { "arg1=1", "arg1=1", 0,                   "arg1=1"        },
        { "arg1=1", "arg1=1", CUrl::fArgs_Replace, "arg1=1"        },
        { "arg1=1", "arg1=1", CUrl::fArgs_Append,  "arg1=1&arg1=1" },
        { "arg1=1", "arg1=1", CUrl::fArgs_Merge,   "arg1=1"        },
        { "arg1=1", "arg1=2", 0,                   "arg1=1"        },
        { "arg1=1", "arg1=2", CUrl::fArgs_Replace, "arg1=2"        },
        { "arg1=1", "arg1=2", CUrl::fArgs_Append,  "arg1=1&arg1=2" },
        { "arg1=1", "arg1=2", CUrl::fArgs_Merge,   "arg1=2"        },
        { "arg1=1", "arg2=2", 0,                   "arg1=1"        },
        { "arg1=1", "arg2=2", CUrl::fArgs_Replace, "arg2=2"        },
        { "arg1=1", "arg2=2", CUrl::fArgs_Append,  "arg1=1&arg2=2" },
        { "arg1=1", "arg2=2", CUrl::fArgs_Merge,   "arg1=1&arg2=2" },
    };
    vector<TPart> fragments =
    {
        { "",          "fragment2", 0,                              ""          },
        { "",          "fragment2", CUrl::fFragment_Replace,        "fragment2" },
        { "",          "fragment2", CUrl::fFragment_ReplaceIfEmpty, "fragment2" },
        { "fragment1", "",          0,                              "fragment1" },
        { "fragment1", "",          CUrl::fFragment_Replace,        "fragment1" },
        { "fragment1", "",          CUrl::fFragment_ReplaceIfEmpty, "fragment1" },
        { "fragment1", "fragment2", 0,                              "fragment1" },
        { "fragment1", "fragment2", CUrl::fFragment_Replace,        "fragment2" },
        { "fragment1", "fragment2", CUrl::fFragment_ReplaceIfEmpty, "fragment1" },
    };

    auto eng = default_random_engine(random_device()());
    uniform_int_distribution<size_t> schemes_dist  (0, schemes.size()   - 1);
    uniform_int_distribution<size_t> users_dist    (0, users.size()     - 1);
    uniform_int_distribution<size_t> passwords_dist(0, passwords.size() - 1);
    uniform_int_distribution<size_t> paths_dist    (0, paths.size()     - 1);
    uniform_int_distribution<size_t> args_dist     (0, args.size()      - 1);
    uniform_int_distribution<size_t> fragments_dist(0, fragments.size() - 1);

    for (const auto& scheme : schemes) {
        Test(
                scheme,
                users    [users_dist    (eng)],
                passwords[passwords_dist(eng)],
                paths    [paths_dist    (eng)],
                args     [args_dist     (eng)],
                fragments[fragments_dist(eng)]);
    }

    for (const auto& user : users) {
        Test(
                schemes  [schemes_dist  (eng)],
                user,
                passwords[passwords_dist(eng)],
                paths    [paths_dist    (eng)],
                args     [args_dist     (eng)],
                fragments[fragments_dist(eng)]);
    }

    for (const auto& password : passwords) {
        Test(
                schemes  [schemes_dist  (eng)],
                users    [users_dist    (eng)],
                password,
                paths    [paths_dist    (eng)],
                args     [args_dist     (eng)],
                fragments[fragments_dist(eng)]);
    }

    for (const auto& path : paths) {
        Test(
                schemes  [schemes_dist  (eng)],
                users    [users_dist    (eng)],
                passwords[passwords_dist(eng)],
                path,
                args     [args_dist     (eng)],
                fragments[fragments_dist(eng)]);
    }

    for (const auto& arg : args) {
        Test(
                schemes  [schemes_dist  (eng)],
                users    [users_dist    (eng)],
                passwords[passwords_dist(eng)],
                paths    [paths_dist    (eng)],
                arg,
                fragments[fragments_dist(eng)]);
    }

    for (const auto& fragment : fragments) {
        Test(
                schemes  [schemes_dist  (eng)],
                users    [users_dist    (eng)],
                passwords[passwords_dist(eng)],
                paths    [paths_dist    (eng)],
                args     [args_dist     (eng)],
                fragment);
    }
}

BOOST_AUTO_TEST_CASE(s_UrlArgsTest)
{
    enum { eQuery, eFlags, eExpected };
    const auto S = CUrlArgs::fSemicolonIsArgDelimiter;
    const auto I = CUrlArgs::fEnableParsingAsIndex;

    vector<tuple<string, CUrlArgs::TFlags, CUrlArgs::TArgs>> checks =
    {
        { "",            0,     {                                                             } },
        { "a=",          0,     { { "a",     ""     },                                        } },
        { "a=0",         0,     { { "a",     "0"    },                                        } },
        { "&",           0,     {                                                             } },
        { "a&",          0,     { { "a",     ""     },                                        } },
        { "a=&",         0,     { { "a",     ""     },                                        } },
        { "a=0&",        0,     { { "a",     "0"    },                                        } },
        { "&b",          0,     {                      { "b",    ""      },                   } },
        { "a&b",         0,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=&b",        0,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=0&b",       0,     { { "a",     "0"    }, { "b",    ""      },                   } },
        { "&b=",         0,     {                      { "b",    ""      },                   } },
        { "a&b=",        0,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=&b=",       0,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=0&b=",      0,     { { "a",     "0"    }, { "b",    ""      },                   } },
        { "&b=0",        0,     {                      { "b",    "0"     },                   } },
        { "a&b=0",       0,     { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a=&b=0",      0,     { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a=0&b=0",     0,     { { "a",     "0"    }, { "b",    "0"     },                   } },
        { ";",           0,     {                      { ";",    ""      },                   } },
        { "b;",          0,     {                      { "b;",   ""      },                   } },
        { "b=;",         0,     {                      { "b",    ";"     },                   } },
        { "b=0;",        0,     {                      { "b",    "0;"    },                   } },
        { ";c",          0,     {                                           { ";c",  ""     } } },
        { "b;c",         0,     {                      { "b;c",  ""      },                   } },
        { "b=;c",        0,     {                      { "b",    ";c"    },                   } },
        { "b=0;c",       0,     {                      { "b",    "0;c"   },                   } },
        { ";c=",         0,     {                                           { ";c",  ""     } } },
        { "b;c=",        0,     {                      { "b;c",  ""      }                    } },
        { "b=;c=",       0,     {                      { "b",    ";c="   },                   } },
        { "b=0;c=",      0,     {                      { "b",    "0;c="  },                   } },
        { ";c=0",        0,     {                                           { ";c",  "0"    } } },
        { "b;c=0",       0,     {                      { "b;c",  "0"     },                   } },
        { "b=;c=0",      0,     {                      { "b",    ";c=0"  },                   } },
        { "b=0;c=0",     0,     {                      { "b",    "0;c=0" },                   } },
        { "&;",          0,     {                      { ";",    ""      },                   } },
        { "&b;",         0,     {                      { "b;",   ""      },                   } },
        { "&b=;",        0,     {                      { "b",    ";"     },                   } },
        { "&b=0;",       0,     {                      { "b",    "0;"    },                   } },
        { "&;c",         0,     {                                           { ";c",  ""     } } },
        { "&b;c",        0,     {                      { "b;c",  ""      },                   } },
        { "&b=;c",       0,     {                      { "b",    ";c"    },                   } },
        { "&b=0;c",      0,     {                      { "b",    "0;c"   },                   } },
        { "&;c=",        0,     {                                           { ";c",  ""     } } },
        { "&b;c=",       0,     {                      { "b;c",   ""     },                   } },
        { "&b=;c=",      0,     {                      { "b",    ";c="   },                   } },
        { "&b=0;c=",     0,     {                      { "b",    "0;c="  },                   } },
        { "&;c=0",       0,     {                                           { ";c",  "0"    } } },
        { "&b;c=0",      0,     {                      { "b;c",  "0"     },                   } },
        { "&b=;c=0",     0,     {                      { "b",    ";c=0"  },                   } },
        { "&b=0;c=0",    0,     {                      { "b",    "0;c=0" },                   } },
        { "a&;",         0,     { { "a",     ""     }, { ";",    ""      },                   } },
        { "a&b;",        0,     { { "a",     ""     }, { "b;",   ""      },                   } },
        { "a&b=;",       0,     { { "a",     ""     }, { "b",    ";"     },                   } },
        { "a&b=0;",      0,     { { "a",     ""     }, { "b",    "0;"    },                   } },
        { "a&;c",        0,     { { "a",     ""     },                      { ";c",  ""     } } },
        { "a&b;c",       0,     { { "a",     ""     }, { "b;c",  ""      },                   } },
        { "a&b=;c",      0,     { { "a",     ""     }, { "b",    ";c"    },                   } },
        { "a&b=0;c",     0,     { { "a",     ""     }, { "b",    "0;c"   },                   } },
        { "a&;c=",       0,     { { "a",     ""     },                      { ";c",  ""     } } },
        { "a&b;c=",      0,     { { "a",     ""     }, { "b;c",  ""      },                   } },
        { "a&b=;c=",     0,     { { "a",     ""     }, { "b",    ";c="   },                   } },
        { "a&b=0;c=",    0,     { { "a",     ""     }, { "b",    "0;c="  },                   } },
        { "a&;c=0",      0,     { { "a",     ""     },                      { ";c",  "0"    } } },
        { "a&b;c=0",     0,     { { "a",     ""     }, { "b;c",  "0"     },                   } },
        { "a&b=;c=0",    0,     { { "a",     ""     }, { "b",    ";c=0"  },                   } },
        { "a&b=0;c=0",   0,     { { "a",     ""     }, { "b",    "0;c=0" },                   } },
        { "a=&;",        0,     { { "a",     ""     }, { ";",    ""      },                   } },
        { "a=&b;",       0,     { { "a",     ""     }, { "b;",   ""      },                   } },
        { "a=&b=;",      0,     { { "a",     ""     }, { "b",    ";"     },                   } },
        { "a=&b=0;",     0,     { { "a",     ""     }, { "b",    "0;"    },                   } },
        { "a=&;c",       0,     { { "a",     ""     },                      { ";c",  ""     } } },
        { "a=&b;c",      0,     { { "a",     ""     }, { "b;c",  ""      },                   } },
        { "a=&b=;c",     0,     { { "a",     ""     }, { "b",    ";c"    },                   } },
        { "a=&b=0;c",    0,     { { "a",     ""     }, { "b",    "0;c"   },                   } },
        { "a=&;c=",      0,     { { "a",     ""     },                      { ";c",  ""     } } },
        { "a=&b;c=",     0,     { { "a",     ""     }, { "b;c",  ""      },                   } },
        { "a=&b=;c=",    0,     { { "a",     ""     }, { "b",    ";c="   },                   } },
        { "a=&b=0;c=",   0,     { { "a",     ""     }, { "b",    "0;c="  },                   } },
        { "a=&;c=0",     0,     { { "a",     ""     },                      { ";c",  "0"    } } },
        { "a=&b;c=0",    0,     { { "a",     ""     }, { "b;c",  "0"     },                   } },
        { "a=&b=;c=0",   0,     { { "a",     ""     }, { "b",    ";c=0"  },                   } },
        { "a=&b=0;c=0",  0,     { { "a",     ""     }, { "b",    "0;c=0" },                   } },
        { "a=0&;",       0,     { { "a",     "0"    }, { ";",    ""      },                   } },
        { "a=0&b;",      0,     { { "a",     "0"    }, { "b;",   ""      },                   } },
        { "a=0&b=;",     0,     { { "a",     "0"    }, { "b",    ";"     },                   } },
        { "a=0&b=0;",    0,     { { "a",     "0"    }, { "b",    "0;"    },                   } },
        { "a=0&;c",      0,     { { "a",     "0"    },                      { ";c",  ""     } } },
        { "a=0&b;c",     0,     { { "a",     "0"    }, { "b;c",  ""      },                   } },
        { "a=0&b=;c",    0,     { { "a",     "0"    }, { "b",    ";c"    },                   } },
        { "a=0&b=0;c",   0,     { { "a",     "0"    }, { "b",    "0;c"   },                   } },
        { "a=0&;c=",     0,     { { "a",     "0"    },                      { ";c",  ""     } } },
        { "a=0&b;c=",    0,     { { "a",     "0"    }, { "b;c",  ""      },                   } },
        { "a=0&b=;c=",   0,     { { "a",     "0"    }, { "b",    ";c="   },                   } },
        { "a=0&b=0;c=",  0,     { { "a",     "0"    }, { "b",    "0;c="  },                   } },
        { "a=0&;c=0",    0,     { { "a",     "0"    },                      { ";c",  "0"    } } },
        { "a=0&b;c=0",   0,     { { "a",     "0"    }, { "b;c",  "0"     },                   } },
        { "a=0&b=;c=0",  0,     { { "a",     "0"    }, { "b",    ";c=0"  },                   } },
        { "a=0&b=0;c=0", 0,     { { "a",     "0"    }, { "b",    "0;c=0" },                   } },
        { "",            S,     {                                                             } },
        { "a=",          S,     { { "a",     ""     },                                        } },
        { "a=0",         S,     { { "a",     "0"    },                                        } },
        { "&",           S,     {                                                             } },
        { "a&",          S,     { { "a",     ""     },                                        } },
        { "a=&",         S,     { { "a",     ""     },                                        } },
        { "a=0&",        S,     { { "a",     "0"    },                                        } },
        { "&b",          S,     {                      { "b",    ""      },                   } },
        { "a&b",         S,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=&b",        S,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=0&b",       S,     { { "a",     "0"    }, { "b",    ""      },                   } },
        { "&b=",         S,     {                      { "b",    ""      },                   } },
        { "a&b=",        S,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=&b=",       S,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=0&b=",      S,     { { "a",     "0"    }, { "b",    ""      },                   } },
        { "&b=0",        S,     {                      { "b",    "0"     },                   } },
        { "a&b=0",       S,     { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a=&b=0",      S,     { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a=0&b=0",     S,     { { "a",     "0"    }, { "b",    "0"     },                   } },
        { ";",           S,     {                                                             } },
        { "b;",          S,     {                      { "b",    ""      },                   } },
        { "b=;",         S,     {                      { "b",    ""      },                   } },
        { "b=0;",        S,     {                      { "b",    "0"     },                   } },
        { ";c",          S,     {                                           { "c",   ""     } } },
        { "b;c",         S,     {                      { "b",    ""      }, { "c",   ""     } } },
        { "b=;c",        S,     {                      { "b",    ""      }, { "c",   ""     } } },
        { "b=0;c",       S,     {                      { "b",    "0"     }, { "c",   ""     } } },
        { ";c=",         S,     {                                           { "c",   ""     } } },
        { "b;c=",        S,     {                      { "b",    ""      }, { "c",   ""     } } },
        { "b=;c=",       S,     {                      { "b",    ""      }, { "c",   ""     } } },
        { "b=0;c=",      S,     {                      { "b",    "0"     }, { "c",   ""     } } },
        { ";c=0",        S,     {                                           { "c",   "0"    } } },
        { "b;c=0",       S,     {                      { "b",    ""      }, { "c",   "0"    } } },
        { "b=;c=0",      S,     {                      { "b",    ""      }, { "c",   "0"    } } },
        { "b=0;c=0",     S,     {                      { "b",    "0"     }, { "c",   "0"    } } },
        { "&;",          S,     {                                                             } },
        { "&b;",         S,     {                      { "b",    ""      },                   } },
        { "&b=;",        S,     {                      { "b",    ""      },                   } },
        { "&b=0;",       S,     {                      { "b",    "0"     },                   } },
        { "&;c",         S,     {                                           { "c",   ""     } } },
        { "&b;c",        S,     {                      { "b",    ""      }, { "c",   ""     } } },
        { "&b=;c",       S,     {                      { "b",    ""      }, { "c",   ""     } } },
        { "&b=0;c",      S,     {                      { "b",    "0"     }, { "c",   ""     } } },
        { "&;c=",        S,     {                                           { "c",   ""     } } },
        { "&b;c=",       S,     {                      { "b",    ""      }, { "c",   ""     } } },
        { "&b=;c=",      S,     {                      { "b",    ""      }, { "c",   ""     } } },
        { "&b=0;c=",     S,     {                      { "b",    "0"     }, { "c",   ""     } } },
        { "&;c=0",       S,     {                                           { "c",   "0"    } } },
        { "&b;c=0",      S,     {                      { "b",    ""      }, { "c",   "0"    } } },
        { "&b=;c=0",     S,     {                      { "b",    ""      }, { "c",   "0"    } } },
        { "&b=0;c=0",    S,     {                      { "b",    "0"     }, { "c",   "0"    } } },
        { "a&;",         S,     { { "a",     ""     },                                        } },
        { "a&b;",        S,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a&b=;",       S,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a&b=0;",      S,     { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a&;c",        S,     { { "a",     ""     },                      { "c",   ""     } } },
        { "a&b;c",       S,     { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a&b=;c",      S,     { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a&b=0;c",     S,     { { "a",     ""     }, { "b",    "0"     }, { "c",   ""     } } },
        { "a&;c=",       S,     { { "a",     ""     },                      { "c",   ""     } } },
        { "a&b;c=",      S,     { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a&b=;c=",     S,     { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a&b=0;c=",    S,     { { "a",     ""     }, { "b",    "0"     }, { "c",   ""     } } },
        { "a&;c=0",      S,     { { "a",     ""     },                      { "c",   "0"    } } },
        { "a&b;c=0",     S,     { { "a",     ""     }, { "b",    ""      }, { "c",   "0"    } } },
        { "a&b=;c=0",    S,     { { "a",     ""     }, { "b",    ""      }, { "c",   "0"    } } },
        { "a&b=0;c=0",   S,     { { "a",     ""     }, { "b",    "0"     }, { "c",   "0"    } } },
        { "a=&;",        S,     { { "a",     ""     },                                        } },
        { "a=&b;",       S,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=&b=;",      S,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=&b=0;",     S,     { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a=&;c",       S,     { { "a",     ""     },                      { "c",   ""     } } },
        { "a=&b;c",      S,     { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a=&b=;c",     S,     { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a=&b=0;c",    S,     { { "a",     ""     }, { "b",    "0"     }, { "c",   ""     } } },
        { "a=&;c=",      S,     { { "a",     ""     },                      { "c",   ""     } } },
        { "a=&b;c=",     S,     { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a=&b=;c=",    S,     { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a=&b=0;c=",   S,     { { "a",     ""     }, { "b",    "0"     }, { "c",   ""     } } },
        { "a=&;c=0",     S,     { { "a",     ""     },                      { "c",   "0"    } } },
        { "a=&b;c=0",    S,     { { "a",     ""     }, { "b",    ""      }, { "c",   "0"    } } },
        { "a=&b=;c=0",   S,     { { "a",     ""     }, { "b",    ""      }, { "c",   "0"    } } },
        { "a=&b=0;c=0",  S,     { { "a",     ""     }, { "b",    "0"     }, { "c",   "0"    } } },
        { "a=0&;",       S,     { { "a",     "0"    },                                        } },
        { "a=0&b;",      S,     { { "a",     "0"    }, { "b",    ""      },                   } },
        { "a=0&b=;",     S,     { { "a",     "0"    }, { "b",    ""      },                   } },
        { "a=0&b=0;",    S,     { { "a",     "0"    }, { "b",    "0"     },                   } },
        { "a=0&;c",      S,     { { "a",     "0"    },                      { "c",   ""     } } },
        { "a=0&b;c",     S,     { { "a",     "0"    }, { "b",    ""      }, { "c",   ""     } } },
        { "a=0&b=;c",    S,     { { "a",     "0"    }, { "b",    ""      }, { "c",   ""     } } },
        { "a=0&b=0;c",   S,     { { "a",     "0"    }, { "b",    "0"     }, { "c",   ""     } } },
        { "a=0&;c=",     S,     { { "a",     "0"    },                      { "c",   ""     } } },
        { "a=0&b;c=",    S,     { { "a",     "0"    }, { "b",    ""      }, { "c",   ""     } } },
        { "a=0&b=;c=",   S,     { { "a",     "0"    }, { "b",    ""      }, { "c",   ""     } } },
        { "a=0&b=0;c=",  S,     { { "a",     "0"    }, { "b",    "0"     }, { "c",   ""     } } },
        { "a=0&;c=0",    S,     { { "a",     "0"    },                      { "c",   "0"    } } },
        { "a=0&b;c=0",   S,     { { "a",     "0"    }, { "b",    ""      }, { "c",   "0"    } } },
        { "a=0&b=;c=0",  S,     { { "a",     "0"    }, { "b",    ""      }, { "c",   "0"    } } },
        { "a=0&b=0;c=0", S,     { { "a",     "0"    }, { "b",    "0"     }, { "c",   "0"    } } },
        { "",            I,     {                                                             } },
        { "a=",          I,     { { "a",     ""     },                                        } },
        { "a=0",         I,     { { "a",     "0"    },                                        } },
        { "&",           I,     { { "&",     ""     },                                        } },
        { "a&",          I,     { { "a&",    ""     },                                        } },
        { "a=&",         I,     { { "a",     ""     },                                        } },
        { "a=0&",        I,     { { "a",     "0"    },                                        } },
        { "&b",          I,     {                      { "&b",   ""      },                   } },
        { "a&b",         I,     { { "a&b",   ""     },                                        } },
        { "a=&b",        I,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=0&b",       I,     { { "a",     "0"    }, { "b",    ""      },                   } },
        { "&b=",         I,     {                      { "b",    ""      },                   } },
        { "a&b=",        I,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=&b=",       I,     { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=0&b=",      I,     { { "a",     "0"    }, { "b",    ""      },                   } },
        { "&b=0",        I,     {                      { "b",    "0"     },                   } },
        { "a&b=0",       I,     { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a=&b=0",      I,     { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a=0&b=0",     I,     { { "a",     "0"    }, { "b",    "0"     },                   } },
        { ";",           I,     {                      { ";",    ""      },                   } },
        { "b;",          I,     {                      { "b;",   ""      },                   } },
        { "b=;",         I,     {                      { "b",    ";"     },                   } },
        { "b=0;",        I,     {                      { "b",    "0;"    },                   } },
        { ";c",          I,     {                                           { ";c",  ""     } } },
        { "b;c",         I,     {                      { "b;c",  ""      },                   } },
        { "b=;c",        I,     {                      { "b",    ";c"    },                   } },
        { "b=0;c",       I,     {                      { "b",    "0;c"   },                   } },
        { ";c=",         I,     {                                           { ";c",  ""     } } },
        { "b;c=",        I,     {                      { "b;c",  ""      }                    } },
        { "b=;c=",       I,     {                      { "b",    ";c="   },                   } },
        { "b=0;c=",      I,     {                      { "b",    "0;c="  },                   } },
        { ";c=0",        I,     {                                           { ";c",  "0"    } } },
        { "b;c=0",       I,     {                      { "b;c",  "0"     },                   } },
        { "b=;c=0",      I,     {                      { "b",    ";c=0"  },                   } },
        { "b=0;c=0",     I,     {                      { "b",    "0;c=0" },                   } },
        { "&;",          I,     {                      { "&;",   ""      },                   } },
        { "&b;",         I,     {                      { "&b;",  ""      },                   } },
        { "&b=;",        I,     {                      { "b",    ";"     },                   } },
        { "&b=0;",       I,     {                      { "b",    "0;"    },                   } },
        { "&;c",         I,     {                                           { "&;c", ""     } } },
        { "&b;c",        I,     {                      { "&b;c", ""      },                   } },
        { "&b=;c",       I,     {                      { "b",    ";c"    },                   } },
        { "&b=0;c",      I,     {                      { "b",    "0;c"   },                   } },
        { "&;c=",        I,     {                                           { ";c",  ""     } } },
        { "&b;c=",       I,     {                      { "b;c",  ""      },                   } },
        { "&b=;c=",      I,     {                      { "b",    ";c="   },                   } },
        { "&b=0;c=",     I,     {                      { "b",    "0;c="  },                   } },
        { "&;c=0",       I,     {                                           { ";c",  "0"    } } },
        { "&b;c=0",      I,     {                      { "b;c",  "0"     },                   } },
        { "&b=;c=0",     I,     {                      { "b",    ";c=0"  },                   } },
        { "&b=0;c=0",    I,     {                      { "b",    "0;c=0" },                   } },
        { "a&;",         I,     { { "a&;",   ""     },                                        } },
        { "a&b;",        I,     { { "a&b;",  ""     },                                        } },
        { "a&b=;",       I,     { { "a",     ""     }, { "b",    ";"     },                   } },
        { "a&b=0;",      I,     { { "a",     ""     }, { "b",    "0;"    },                   } },
        { "a&;c",        I,     { { "a&;c",  ""     },                                        } },
        { "a&b;c",       I,     { { "a&b;c", ""     },                                        } },
        { "a&b=;c",      I,     { { "a",     ""     }, { "b",    ";c"    },                   } },
        { "a&b=0;c",     I,     { { "a",     ""     }, { "b",    "0;c"   },                   } },
        { "a&;c=",       I,     { { "a",     ""     },                      { ";c",  ""     } } },
        { "a&b;c=",      I,     { { "a",     ""     }, { "b;c",  ""      },                   } },
        { "a&b=;c=",     I,     { { "a",     ""     }, { "b",    ";c="   },                   } },
        { "a&b=0;c=",    I,     { { "a",     ""     }, { "b",    "0;c="  },                   } },
        { "a&;c=0",      I,     { { "a",     ""     },                      { ";c",  "0"    } } },
        { "a&b;c=0",     I,     { { "a",     ""     }, { "b;c",  "0"     },                   } },
        { "a&b=;c=0",    I,     { { "a",     ""     }, { "b",    ";c=0"  },                   } },
        { "a&b=0;c=0",   I,     { { "a",     ""     }, { "b",    "0;c=0" },                   } },
        { "a=&;",        I,     { { "a",     ""     }, { ";",    ""      },                   } },
        { "a=&b;",       I,     { { "a",     ""     }, { "b;",   ""      },                   } },
        { "a=&b=;",      I,     { { "a",     ""     }, { "b",    ";"     },                   } },
        { "a=&b=0;",     I,     { { "a",     ""     }, { "b",    "0;"    },                   } },
        { "a=&;c",       I,     { { "a",     ""     },                      { ";c",  ""     } } },
        { "a=&b;c",      I,     { { "a",     ""     }, { "b;c",  ""      },                   } },
        { "a=&b=;c",     I,     { { "a",     ""     }, { "b",    ";c"    },                   } },
        { "a=&b=0;c",    I,     { { "a",     ""     }, { "b",    "0;c"   },                   } },
        { "a=&;c=",      I,     { { "a",     ""     },                      { ";c",  ""     } } },
        { "a=&b;c=",     I,     { { "a",     ""     }, { "b;c",  ""      },                   } },
        { "a=&b=;c=",    I,     { { "a",     ""     }, { "b",    ";c="   },                   } },
        { "a=&b=0;c=",   I,     { { "a",     ""     }, { "b",    "0;c="  },                   } },
        { "a=&;c=0",     I,     { { "a",     ""     },                      { ";c",  "0"    } } },
        { "a=&b;c=0",    I,     { { "a",     ""     }, { "b;c",  "0"     },                   } },
        { "a=&b=;c=0",   I,     { { "a",     ""     }, { "b",    ";c=0"  },                   } },
        { "a=&b=0;c=0",  I,     { { "a",     ""     }, { "b",    "0;c=0" },                   } },
        { "a=0&;",       I,     { { "a",     "0"    }, { ";",    ""      },                   } },
        { "a=0&b;",      I,     { { "a",     "0"    }, { "b;",   ""      },                   } },
        { "a=0&b=;",     I,     { { "a",     "0"    }, { "b",    ";"     },                   } },
        { "a=0&b=0;",    I,     { { "a",     "0"    }, { "b",    "0;"    },                   } },
        { "a=0&;c",      I,     { { "a",     "0"    },                      { ";c",  ""     } } },
        { "a=0&b;c",     I,     { { "a",     "0"    }, { "b;c",  ""      },                   } },
        { "a=0&b=;c",    I,     { { "a",     "0"    }, { "b",    ";c"    },                   } },
        { "a=0&b=0;c",   I,     { { "a",     "0"    }, { "b",    "0;c"   },                   } },
        { "a=0&;c=",     I,     { { "a",     "0"    },                      { ";c",  ""     } } },
        { "a=0&b;c=",    I,     { { "a",     "0"    }, { "b;c",  ""      },                   } },
        { "a=0&b=;c=",   I,     { { "a",     "0"    }, { "b",    ";c="   },                   } },
        { "a=0&b=0;c=",  I,     { { "a",     "0"    }, { "b",    "0;c="  },                   } },
        { "a=0&;c=0",    I,     { { "a",     "0"    },                      { ";c",  "0"    } } },
        { "a=0&b;c=0",   I,     { { "a",     "0"    }, { "b;c",  "0"     },                   } },
        { "a=0&b=;c=0",  I,     { { "a",     "0"    }, { "b",    ";c=0"  },                   } },
        { "a=0&b=0;c=0", I,     { { "a",     "0"    }, { "b",    "0;c=0" },                   } },
        { "",            S | I, {                                                             } },
        { "a=",          S | I, { { "a",     ""     },                                        } },
        { "a=0",         S | I, { { "a",     "0"    },                                        } },
        { "&",           S | I, { { "&",     ""     },                                        } },
        { "a&",          S | I, { { "a&",    ""     },                                        } },
        { "a=&",         S | I, { { "a",     ""     },                                        } },
        { "a=0&",        S | I, { { "a",     "0"    },                                        } },
        { "&b",          S | I, {                      { "&b",   ""      },                   } },
        { "a&b",         S | I, { { "a&b",   ""     },                                        } },
        { "a=&b",        S | I, { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=0&b",       S | I, { { "a",     "0"    }, { "b",    ""      },                   } },
        { "&b=",         S | I, {                      { "b",    ""      },                   } },
        { "a&b=",        S | I, { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=&b=",       S | I, { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=0&b=",      S | I, { { "a",     "0"    }, { "b",    ""      },                   } },
        { "&b=0",        S | I, {                      { "b",    "0"     },                   } },
        { "a&b=0",       S | I, { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a=&b=0",      S | I, { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a=0&b=0",     S | I, { { "a",     "0"    }, { "b",    "0"     },                   } },
        { ";",           S | I, {                      { ";",    ""      },                   } },
        { "b;",          S | I, {                      { "b;",   ""      },                   } },
        { "b=;",         S | I, {                      { "b",    ""      },                   } },
        { "b=0;",        S | I, {                      { "b",    "0"     },                   } },
        { ";c",          S | I, {                                           { ";c",  ""     } } },
        { "b;c",         S | I, {                      { "b;c",  ""      },                   } },
        { "b=;c",        S | I, {                      { "b",    ""      }, { "c",   ""     } } },
        { "b=0;c",       S | I, {                      { "b",    "0"     }, { "c",   ""     } } },
        { ";c=",         S | I, {                                           { "c",   ""     } } },
        { "b;c=",        S | I, {                      { "b",    ""      }, { "c",   ""     } } },
        { "b=;c=",       S | I, {                      { "b",    ""      }, { "c",   ""     } } },
        { "b=0;c=",      S | I, {                      { "b",    "0"     }, { "c",   ""     } } },
        { ";c=0",        S | I, {                                           { "c",   "0"    } } },
        { "b;c=0",       S | I, {                      { "b",    ""      }, { "c",   "0"    } } },
        { "b=;c=0",      S | I, {                      { "b",    ""      }, { "c",   "0"    } } },
        { "b=0;c=0",     S | I, {                      { "b",    "0"     }, { "c",   "0"    } } },
        { "&;",          S | I, {                      { "&;",   ""      },                   } },
        { "&b;",         S | I, {                      { "&b;",  ""      },                   } },
        { "&b=;",        S | I, {                      { "b",    ""      },                   } },
        { "&b=0;",       S | I, {                      { "b",    "0"     },                   } },
        { "&;c",         S | I, {                                           { "&;c", ""     } } },
        { "&b;c",        S | I, {                      { "&b;c", ""      },                   } },
        { "&b=;c",       S | I, {                      { "b",    ""      }, { "c",   ""     } } },
        { "&b=0;c",      S | I, {                      { "b",    "0"     }, { "c",   ""     } } },
        { "&;c=",        S | I, {                                           { "c",   ""     } } },
        { "&b;c=",       S | I, {                      { "b",    ""      }, { "c",   ""     } } },
        { "&b=;c=",      S | I, {                      { "b",    ""      }, { "c",   ""     } } },
        { "&b=0;c=",     S | I, {                      { "b",    "0"     }, { "c",   ""     } } },
        { "&;c=0",       S | I, {                                           { "c",   "0"    } } },
        { "&b;c=0",      S | I, {                      { "b",    ""      }, { "c",   "0"    } } },
        { "&b=;c=0",     S | I, {                      { "b",    ""      }, { "c",   "0"    } } },
        { "&b=0;c=0",    S | I, {                      { "b",    "0"     }, { "c",   "0"    } } },
        { "a&;",         S | I, { { "a&;",   ""     },                                        } },
        { "a&b;",        S | I, { { "a&b;",  ""     },                                        } },
        { "a&b=;",       S | I, { { "a",     ""     }, { "b",    ""      },                   } },
        { "a&b=0;",      S | I, { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a&;c",        S | I, { { "a&;c",  ""     },                                        } },
        { "a&b;c",       S | I, { { "a&b;c", ""     },                                        } },
        { "a&b=;c",      S | I, { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a&b=0;c",     S | I, { { "a",     ""     }, { "b",    "0"     }, { "c",   ""     } } },
        { "a&;c=",       S | I, { { "a",     ""     },                      { "c",   ""     } } },
        { "a&b;c=",      S | I, { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a&b=;c=",     S | I, { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a&b=0;c=",    S | I, { { "a",     ""     }, { "b",    "0"     }, { "c",   ""     } } },
        { "a&;c=0",      S | I, { { "a",     ""     },                      { "c",   "0"    } } },
        { "a&b;c=0",     S | I, { { "a",     ""     }, { "b",    ""      }, { "c",   "0"    } } },
        { "a&b=;c=0",    S | I, { { "a",     ""     }, { "b",    ""      }, { "c",   "0"    } } },
        { "a&b=0;c=0",   S | I, { { "a",     ""     }, { "b",    "0"     }, { "c",   "0"    } } },
        { "a=&;",        S | I, { { "a",     ""     },                                        } },
        { "a=&b;",       S | I, { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=&b=;",      S | I, { { "a",     ""     }, { "b",    ""      },                   } },
        { "a=&b=0;",     S | I, { { "a",     ""     }, { "b",    "0"     },                   } },
        { "a=&;c",       S | I, { { "a",     ""     },                      { "c",   ""     } } },
        { "a=&b;c",      S | I, { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a=&b=;c",     S | I, { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a=&b=0;c",    S | I, { { "a",     ""     }, { "b",    "0"     }, { "c",   ""     } } },
        { "a=&;c=",      S | I, { { "a",     ""     },                      { "c",   ""     } } },
        { "a=&b;c=",     S | I, { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a=&b=;c=",    S | I, { { "a",     ""     }, { "b",    ""      }, { "c",   ""     } } },
        { "a=&b=0;c=",   S | I, { { "a",     ""     }, { "b",    "0"     }, { "c",   ""     } } },
        { "a=&;c=0",     S | I, { { "a",     ""     },                      { "c",   "0"    } } },
        { "a=&b;c=0",    S | I, { { "a",     ""     }, { "b",    ""      }, { "c",   "0"    } } },
        { "a=&b=;c=0",   S | I, { { "a",     ""     }, { "b",    ""      }, { "c",   "0"    } } },
        { "a=&b=0;c=0",  S | I, { { "a",     ""     }, { "b",    "0"     }, { "c",   "0"    } } },
        { "a=0&;",       S | I, { { "a",     "0"    },                                        } },
        { "a=0&b;",      S | I, { { "a",     "0"    }, { "b",    ""      },                   } },
        { "a=0&b=;",     S | I, { { "a",     "0"    }, { "b",    ""      },                   } },
        { "a=0&b=0;",    S | I, { { "a",     "0"    }, { "b",    "0"     },                   } },
        { "a=0&;c",      S | I, { { "a",     "0"    },                      { "c",   ""     } } },
        { "a=0&b;c",     S | I, { { "a",     "0"    }, { "b",    ""      }, { "c",   ""     } } },
        { "a=0&b=;c",    S | I, { { "a",     "0"    }, { "b",    ""      }, { "c",   ""     } } },
        { "a=0&b=0;c",   S | I, { { "a",     "0"    }, { "b",    "0"     }, { "c",   ""     } } },
        { "a=0&;c=",     S | I, { { "a",     "0"    },                      { "c",   ""     } } },
        { "a=0&b;c=",    S | I, { { "a",     "0"    }, { "b",    ""      }, { "c",   ""     } } },
        { "a=0&b=;c=",   S | I, { { "a",     "0"    }, { "b",    ""      }, { "c",   ""     } } },
        { "a=0&b=0;c=",  S | I, { { "a",     "0"    }, { "b",    "0"     }, { "c",   ""     } } },
        { "a=0&;c=0",    S | I, { { "a",     "0"    },                      { "c",   "0"    } } },
        { "a=0&b;c=0",   S | I, { { "a",     "0"    }, { "b",    ""      }, { "c",   "0"    } } },
        { "a=0&b=;c=0",  S | I, { { "a",     "0"    }, { "b",    ""      }, { "c",   "0"    } } },
        { "a=0&b=0;c=0", S | I, { { "a",     "0"    }, { "b",    "0"     }, { "c",   "0"    } } },
    };

    for (const auto& check : checks) {
        const auto& source = get<eQuery>(check);
        const auto flags = get<eFlags>(check);
        const auto& expected = get<eExpected>(check);
        CUrlArgs args(source, nullptr, flags);
        const auto& parsed = args.GetArgs();

        if (parsed.size() != expected.size()) {
            BOOST_ERROR("Parsed args size (" << parsed.size() << ") does not match expected one (" <<
                    expected.size() << ") for '" << source << "' and flags=" << flags);
        } else {
            auto l = [](const CUrlArgs::SUrlArg& l, const CUrlArgs::SUrlArg& r) { return l.name == r.name && l.value == r.value; };
            auto r = mismatch(expected.begin(), expected.end(), parsed.begin(), l);

            if (r.first != expected.end()) {
                BOOST_ERROR("Parsed does not match expected, first mismatch: '" <<
                        r.second->name << "'='" << r.second->value << "' vs '" <<
                        r.first->name << "'='" << r.first->value << "' for '" << source << "' and flags=" << flags);
            }
        }
    }
}
