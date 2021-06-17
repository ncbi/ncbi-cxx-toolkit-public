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
