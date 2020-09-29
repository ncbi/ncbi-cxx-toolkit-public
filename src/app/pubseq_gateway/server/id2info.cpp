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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   PSG ID2 info wrapper
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
USING_NCBI_SCOPE;

#include "id2info.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway.hpp"


CPSGFlavorId2Info::CPSGFlavorId2Info(const string &  id2_info,
                                     bool  count_errors) :
    m_Sat(0), m_Info(0), m_Chunks(0), m_SplitVersion(0)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    // id2_info: "sat.info.nchunks[.splitversion]"
    vector<string>          parts;
    NStr::Split(id2_info, ".", parts);

    if (parts.size() < 3) {
        if (count_errors)
            app->GetErrorCounters().IncInvalidId2InfoError();
        NCBI_THROW(CPubseqGatewayException, eInvalidId2Info,
                   "Invalid id2 info '" + id2_info +
                   "'. Expected 3 or more parts, found " +
                   to_string(parts.size()) + " parts.");
    }

    try {
        m_Sat = NStr::StringToInt(parts[0]);
        m_Info = NStr::StringToInt(parts[1]);
        m_Chunks = NStr::StringToInt(parts[2]);

        if (parts.size() >= 4)
            m_SplitVersion = NStr::StringToInt(parts[3]);
    } catch (...) {
        if (count_errors)
            app->GetErrorCounters().IncInvalidId2InfoError();
        NCBI_THROW(CPubseqGatewayException, eInvalidId2Info,
                   "Invalid id2 info '" + id2_info +
                   "'. Cannot convert parts into integers.");
    }


    // Validate
    string      validate_message;
    if (m_Sat <= 0)
        validate_message = "Invalid id2 info SAT value. "
            "Expected to be > 0. Received: " +
            to_string(m_Sat) + ".";

    if (m_Info <= 0) {
        if (!validate_message.empty())
            validate_message += " ";
        validate_message += "Invalid id2 info INFO value. "
            "Expected to be > 0. Received: " +
            to_string(m_Info) + ".";
    }
    if (m_Chunks <= 0) {
        if (!validate_message.empty())
            validate_message += " ";
        validate_message += "Invalid id2 info NCHUNKS value. "
            "Expected to be > 0. Received: " +
            to_string(m_Chunks) + ".";
    }

    if (!validate_message.empty()) {
        if (count_errors)
            app->GetErrorCounters().IncInvalidId2InfoError();
        NCBI_THROW(CPubseqGatewayException, eInvalidId2Info,
                   validate_message);
    }
}

