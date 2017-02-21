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


const char* kSchemeHttp = "http";
const char* kSchemeLB = NCBI_SCHEME_SERVICE;
const char* kUser = "user";
const char* kPassword = "password";
const char* kHostShort = "hostname";
const char* kHostFull = "host.net";
const char* kHostIPv6 = "[1:2:3:4]";
const char* kServiceSimple = "servicename";
const char* kServiceSpecial = "/service/name";
const char* kPort = "1234";
const char* kPath = "/path/file";
const char* kArgs = "arg1=val1&arg2=val2";
const char* kFragment = "fragment";
const char* kRelativePath = "relative/path/file";
const char* kNonGenericScheme = "scheme";
const char* kNonGenericPath = "nongeneric:path";


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
    fPathFull       = 0x0100,
    fPathRel        = 0x0180,
    fPathMask       = 0x0180,

    fArgs           = 0x0200,
    fFragment       = 0x0400,
    
    fMax            = 0x0800
};


void s_TestUrl(string url_str, int flags)
{
    CUrl url(url_str);
    bool custom_scheme = false;
    if (flags & fScheme) {
        if (url.GetScheme() == kNonGenericScheme) {
            BOOST_CHECK_EQUAL(url.GetPath(), kNonGenericPath);
            BOOST_CHECK(url.GetHost().empty());
            BOOST_CHECK(url.GetService().empty());
            BOOST_CHECK(url.GetUser().empty());
            BOOST_CHECK(url.GetPassword().empty());
            BOOST_CHECK(url.GetPort().empty());
            BOOST_CHECK(url.GetArgs().GetArgs().empty());
            BOOST_CHECK(url.GetFragment().empty());
            return;
        }
        if (flags & fService) {
            BOOST_CHECK_EQUAL(url.GetScheme(), kSchemeLB);
        }
        else {
            BOOST_CHECK_EQUAL(url.GetScheme(), kSchemeHttp);
        }
    }
    else {
        BOOST_CHECK(url.GetScheme().empty());
    }

    if (flags & fService) {
        BOOST_CHECK(url.GetHost().empty());
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
    case fPathFull:
        BOOST_CHECK_EQUAL(url.GetPath(), kPath);
        break;
    case fPathRel:
        BOOST_CHECK_EQUAL(url.GetPath(), kRelativePath);
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


BOOST_AUTO_TEST_CASE(s_UrlParseCompose)
{
    for (int flags = 0; flags < fMax; ++flags) {
        string surl;
        if (flags & fScheme) {
            surl = ((flags & fService) ? kSchemeLB : kSchemeHttp);
            surl += "://";
        }
        else if (!(flags & fService)) {
            // Scheme-less non-service URLs start with //
            surl = "//";
        }

        string authority;
        if (flags & fService) {
            if (flags & fPort) continue; // no port for services
            switch (flags & fHostMask) {
            case fServiceSimple:
                authority = kServiceSimple;
                break;
            case fServiceSpecial:
                authority = NStr::URLEncode(kServiceSpecial);
                break;
            default:
                continue; // unused service type
            }
            if (!(flags & fScheme)) {
                // Without scheme only service name is allowed
                if ((flags & fAuthorityMask) == flags) {
                    s_TestUrl(authority, flags & fAuthorityMask);
                }
                continue;
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

        string user_info;
        if (flags & fUser) {
            user_info = kUser;
        }
        if (flags & fPassword) {
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
        case fPathFull:
            surl += kPath;
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

    // Absolute path only
    s_TestUrl(kPath, fPathFull);
    // Relative path only
    s_TestUrl(kRelativePath, fPathRel);
    // Other non-generic url
    s_TestUrl(string(kNonGenericScheme) + string(":") + string(kNonGenericPath), fScheme | fPathFull);
}
