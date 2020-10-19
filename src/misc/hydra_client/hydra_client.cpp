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
* Author: Jonathan Kans, Aaron Ucko
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <misc/hydra_client/hydra_client.hpp>

#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <misc/xmlwrapp/xmlwrapp.hpp>

#include <cmath>

USING_NCBI_SCOPE;

class CHydraServer : public xml::event_parser
{
public:
    CHydraServer(vector<int>& uids);
    bool RunHydraSearch(const string& query,
                        CHydraSearch::ESearch search,
                        CHydraSearch::EScoreCutoff cutoff);

protected:
    bool error(const string& message);
    bool warning(const string& message);
    bool start_element(const string& name, const attrs_type& attrs);
    bool end_element(const string& name);
    bool text(const string& contents);

protected:
    string m_Score;
    vector<int>& m_Uids;
    CHydraSearch::EScoreCutoff m_Cutoff;
};

CHydraServer::CHydraServer(vector<int>& uids)
    : m_Uids(uids)
{
}

static string GetSearchType(CHydraSearch::ESearch search) 
{
    switch(search) {
        case CHydraSearch::ESearch::ePUBMED_TOP_20:  
            return "pubmed_search_citation_top_20.1";

        case CHydraSearch::ESearch::ePUBMED:         
            return "pubmed_search_citation.1";

        case CHydraSearch::ESearch::eCITATION:       
            return "citation";

        case CHydraSearch::ESearch::ePMC:            
            return "pmc_citation.1";

        case CHydraSearch::ESearch::ePMC_TOP_6:
            return "pmc_citation_top_6.1";

        default:
            return "pubmed_search_citation_top_20.1";
    }
}

bool CHydraServer::RunHydraSearch(const string& query,
                                  CHydraSearch::ESearch search,
                                  CHydraSearch::EScoreCutoff cutoff)
{
    string hostname = "www.ncbi.nlm.nih.gov";
    string path = "/projects/hydra/hydra_search.cgi";
    string args = "search=@SEARCHTYPE@&query=";

    m_Cutoff = cutoff;

    string searchtype = GetSearchType(search);
    NStr::ReplaceInPlace(args, "@SEARCHTYPE@", searchtype);

    string params = args + NStr::URLEncode(query);

    for (int attempt = 1;  attempt <= 5;  attempt++) {
        try {
            CConn_HttpStream istr(hostname, path, params);

            xml::error_messages msgs;
            parse_stream(istr, &msgs);

            if (msgs.has_errors()  ||  msgs.has_fatal_errors()) {
                ERR_POST(Warning << "error parsing xml: " << msgs.print());
                return false;
            } else {
                return m_Uids.size() > 0;
            }
        }
        catch (CException& e) {
            ERR_POST(Warning << "failed on attempt " << attempt
                     << ": " << e);
        }

        int sleep_secs = ::sqrt((double)attempt);
        if (sleep_secs) {
            SleepSec(sleep_secs);
        }
    }
    return false;
}

bool CHydraServer::error(const string& message)
{
    ERR_POST(Error << "parse error: " << message);
    return false;
}

bool CHydraServer::warning(const string& message)
{
    ERR_POST(Warning << "parse warning: " << message);
    return false;
}

bool CHydraServer::start_element(const string& name, const attrs_type& attrs)
{
    m_Score.clear();
    if (NStr::EqualNocase(name, "Id")) {
         attrs_type::const_iterator it = attrs.find("score");
         if (it != attrs.end()) {
             m_Score = it->second;
         }
    }

    return true;
}

bool CHydraServer::end_element(const string& name)
{
    m_Score.clear();
    return true;
}

bool CHydraServer::text(const string& contents)
{
    if ( m_Score.empty() ) return true;
    if ( contents.find_first_not_of(" \n\r\t") == NPOS ) return true;

    if (m_Score == "1") {
        // score 100 always gets reported
        m_Uids.push_back(NStr::StringToInt(contents));
        return true;
    }

    // convert m_Score string to double and then make comparisons to cutoff
    double score = NStr::StringToDouble(m_Score);

    if ((m_Cutoff == CHydraSearch::EScoreCutoff::eCutoff_VeryHigh) && score >= 0.99) {
        // score 0.99 or higher
        m_Uids.push_back(NStr::StringToInt(contents));
    }
    else if ((m_Cutoff == CHydraSearch::EScoreCutoff::eCutoff_High) && score >= 0.95) {
        // score 0.95 or higher
        m_Uids.push_back(NStr::StringToInt(contents));
    }
    else if ((m_Cutoff == CHydraSearch::EScoreCutoff::eCutoff_Medium) && score >= 0.90) {
        // score 0.90 or higher
        m_Uids.push_back(NStr::StringToInt(contents));
    }
    else if ((m_Cutoff == CHydraSearch::EScoreCutoff::eCutoff_Low) && score >= 0.80) {
        // score 0.80 or higher
        m_Uids.push_back(NStr::StringToInt(contents));
    }

    return true;
}

bool CHydraSearch::DoHydraSearch(const string& query, vector<int>& uids,
                                 ESearch search,
                                 EScoreCutoff cutoff)
{
    uids.clear();
    CHydraServer hydra(uids);
    return hydra.RunHydraSearch(query, search, cutoff);
}

