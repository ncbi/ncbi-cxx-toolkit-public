#ifndef HTTPCLIENTTRANSPORT__HPP
#define HTTPCLIENTTRANSPORT__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#define __STDC_FORMAT_MACROS

#include <string>
#include <stdexcept>
#include <memory>
#include <sstream>
#include <cassert>

#include <nghttp2/nghttp2.h>
#include <uv.h>

#include "DdRpcCommon.hpp"

namespace HCT {

#define ERR_HCT_DNS_RESOLV          0x10001
#define ERR_HCT_HTTP_CB             0x10003
#define ERR_HCT_UNEXP_CB            0x10004
#define ERR_HCT_OVERLAP             0x10005
#define ERR_HCT_CANCELLED           0x10006
#define ERR_HCT_SHUTDOWN            0x10007
#define ERR_HCT_EXCEPT              0x10008

using TagHCT = std::intptr_t;

const int eo_none = 0;
const int eo_tcp = 1;
const int eo_http = 2;
const int eo_generic = 3;


struct generic_error
{
    int error_origin;
    int error_code;
    std::string error_msg;
    generic_error() :
        error_origin(eo_none),
        error_code(0)
    {}
    void clear_error()
    {
        error_origin = eo_none;
        error_code = 0;
        error_msg.clear();
    }
    bool has_error() const noexcept
    {
        return error_code != 0;
    }
    void error(int errc, const char* details) noexcept
    {
        if (!has_error() && errc != 0) {
            try {
                error_origin = eo_generic;
                error_code = errc;
                std::stringstream ss;
                ss << "Error: " << details << ", code(" << errc << "), origin: generic";
                error_msg = ss.str();
            }
            catch (...) {
                error_code = ERR_HCT_EXCEPT;
            }
        }
    }

    void error(const generic_error& err) noexcept
    {
        if (!has_error() && err.error_code != 0) {
            try {
                error_origin = err.error_origin;
                error_code = err.error_code;
                error_msg = err.error_msg;
            }
            catch (...) {
                error_code = ERR_HCT_EXCEPT;
            }
        }
    }

    std::string get_error_description() const
    {
        return has_error() ? error_msg : std::string();
    }
    void raise_error() const
    {
        if (has_error())
            DDRPC::EDdRpcException::raise(get_error_description(), error_code);
    }
};

struct http2_error : public generic_error
{

    void error_nghttp2(int errc)
    {
        if (!has_error() && errc != 0) {
            try {
                error_origin = eo_http;
                error_code = errc;
                std::stringstream ss;
                ss << "Error: " << (errc < 0 ? nghttp2_strerror(errc) : nghttp2_http2_strerror(errc)) << ", code(" << errc << "), origin: http";
                error_msg = ss.str();
            }
            catch (...) {
                error_code = ERR_HCT_EXCEPT;
            }
        }
    }

    void error_libuv(int errc, const char* details)
    {
        if (!has_error() && errc != 0) {
            try {
                error_origin = eo_tcp;
                error_code = errc;
                std::stringstream ss;
                if (errc > 0)
                    ss << details << ": " << uv_strerror(errc);
                else
                    ss << details << ": " << uv_strerror(errc) << ", code(" << errc << "), origin: tcp";
                error_msg = ss.str();
            }
            catch (...) {
                error_code = ERR_HCT_EXCEPT;
            }
        }
    }
};


struct http2_end_point
{
    std::string schema;
    std::string authority;
    std::string path;
    generic_error error;
    ~http2_end_point()
    {
        assert(true);
    }
};


};

#endif
