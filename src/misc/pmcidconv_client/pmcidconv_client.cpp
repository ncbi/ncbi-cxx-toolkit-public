/*
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
* Author: Colleen Bollin, based on a file by Jonathan Kans and Aaron Ucko
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <misc/pmcidconv_client/pmcidconv_client.hpp>

#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <misc/xmlwrapp/xmlwrapp.hpp>

#include <cmath>

USING_NCBI_SCOPE;

class CPMCIDConverterServer : public xml::event_parser
{
public:
    CPMCIDConverterServer(CPMCIDSearch::TResults& results);
    bool GetPmids(const vector<string>& uids);

protected:
    bool error(const string& message);
    bool warning(const string& message);
    bool start_element(const string& name, const attrs_type& attrs);
    bool end_element(const string& name);
    bool text(const string& contents);

protected:
    CPMCIDSearch::TResults& m_Results;
    size_t m_Offset;
};

CPMCIDConverterServer::CPMCIDConverterServer(CPMCIDSearch::TResults& results)
    : m_Results(results), m_Offset(0)
{
}


bool CPMCIDConverterServer::GetPmids(const vector<string>& uids)
{
    string hostname = "www.ncbi.nlm.nih.gov";
    string path = "/pmc/utils/idconv/v1.0/";
    string args = "tool=NCBIToolkit&email=bollin@ncbi.nlm.nih.gov&versions=no&";

    m_Results.clear();

    // If size greater than 200, make separate requests
    m_Offset = 0;
    while (m_Offset < uids.size()) {
        string params = args + "ids=";
        size_t expected = uids.size() - m_Offset;
        if (expected > 200) {
            expected = 200;
        }
        for (size_t i = 0; i + m_Offset < uids.size() && i < 200; i++) {
            if (i > 0) {
                params += ",";
            }
            params += NStr::URLEncode(uids[i]);
            if (params.length() > 500) {
                // need to make separate requests if params will be too long
                expected = i + 1;
                break;
            }
        }
        bool success = false;
        for (int attempt = 1; !success && attempt <= 5; attempt++) {
            try {
                CConn_HttpStream istr(hostname, path, params);

                xml::error_messages msgs;
                parse_stream(istr, &msgs);

                if (msgs.has_errors() || msgs.has_fatal_errors()) {
                    ERR_POST(Warning << "error parsing xml: " << msgs.print());
                }
                if (m_Results.size() - m_Offset < expected) {
                    return false;
                }
                if (m_Results.size() - m_Offset > expected) {
                    ERR_POST(Warning << "Too many results returned");
                    return false;
                }
                success = true;
            } catch (CException& e) {
                ERR_POST(Warning << "failed on attempt " << attempt
                    << ": " << e);
            }
            if (!success) {
                int sleep_secs = ::sqrt((double)attempt);
                if (sleep_secs) {
                    SleepSec(sleep_secs);
                }
            }
        }
        if (!success) {
            return false;
        }
        m_Offset += expected;
    }
    return true;
}

bool CPMCIDConverterServer::error(const string& message)
{
    ERR_POST(Error << "parse error: " << message);
    return false;
}

bool CPMCIDConverterServer::warning(const string& message)
{
    ERR_POST(Warning << "parse warning: " << message);
    return false;
}

bool CPMCIDConverterServer::start_element(const string& name, const attrs_type& attrs)
{
    if (NStr::EqualNocase(name, "record")) {
         attrs_type::const_iterator it = attrs.find("pmid");
         if (it != attrs.end()) {
             m_Results.push_back(NStr::StringToLong(it->second));
         } else {
             m_Results.push_back((long)0);
         }
    }

    return true;
}

bool CPMCIDConverterServer::end_element(const string& name)
{
    return true;
}

bool CPMCIDConverterServer::text(const string& contents)
{
    return true;
}

bool CPMCIDSearch::DoPMCIDSearch(const vector<string>& query_ids, TResults& results)
{
    results.clear();
    CPMCIDConverterServer converter(results);
    return converter.GetPmids(query_ids);
}

