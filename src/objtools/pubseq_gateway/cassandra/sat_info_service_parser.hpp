#ifndef SAT_INFO_SERVICE_PARSER__HPP
#define SAT_INFO_SERVICE_PARSER__HPP

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
 * Authors: Dmitrii Saprykin
 *
 * File Description:
 *
 *   Parser for service connection strings
 *
 */

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>

#include <misc/jsonwrapp/jsonwrapp11.hpp>

BEGIN_IDBLOB_SCOPE

class CSatInfoServiceParser final
{
public:
    CSatInfoServiceParser() = default;
    string GetConnectionString(string const& service, string const& datacenter) const
    {
        if (service.find('{') == std::string::npos && service.find('}') == std::string::npos) {
            return service;
        }
        rapidjson::Document d;
        rapidjson::ParseResult ok = d.Parse<0, rapidjson::ASCII<>>(service);
        if (!ok) {
            string warning = string(GetParseError_En(ok.Code())) + "(" + to_string(ok.Offset()) + ")";
            ERR_POST(Warning << "Failed to parse JSON value for 'service' field: '"
                             << warning << "' - '" << service << "'");
            return {};
        }
        if (d.HasMember(datacenter)) {
            if (d[datacenter].IsString()) {
                return d[datacenter].GetString();
            }
            else {
                ERR_POST(Warning << "Failed to parse JSON value for 'service' field: 'Value for datacenter ("
                                 << datacenter << ") key is not a string' - '" << service << "'");
                return {};
            }
        }
        else {
            ERR_POST(Warning << "Failed to parse JSON value for 'service' field: 'Value for datacenter ("
                             << datacenter << ") not found' - '" << service << "'");
            return {};
        }
    }
};

END_IDBLOB_SCOPE

#endif  // SAT_INFO_SERVICE_PARSER__HPP

