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
 * Author:  Alex Kotliarov
 *
 * File Description: test for 16S_summaries, BOOST Test framework. 
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <misc/xmlwrapp/xmlwrapp.hpp>
#include <misc/eutils_client/eutils_client.hpp>

#include <sstream>


USING_NCBI_SCOPE;

const static char* query_asthma = "asthma AND 1780/01/01:1968/01/01[pdat]";

NCBITEST_AUTO_INIT()
{
}

NCBITEST_AUTO_FINI()
{
}

BOOST_AUTO_TEST_CASE(TestSearchHistory)
{
    CEutilsClient cli;
    stringstream content;

    cli.Search("pubmed", query_asthma, content, CEutilsClient::eUseHistoryEnabled);
    string body = content.str();
    xml::document doc(body.c_str(), body.size(), NULL);
    const xml::node& root = doc.get_root_node();

    xml::node_set items = root.run_xpath_query("./WebEnv");
    BOOST_CHECK_MESSAGE ( 1 == items.size() , "Expecting 1 WebEnv: " + body );

    items  = root.run_xpath_query("./QueryKey");
    BOOST_CHECK_MESSAGE ( 1 == items.size() , "Expecting 1 QueryKey: " + body );
}

BOOST_AUTO_TEST_CASE(TestSearchHistoryIterate)
{
    CEutilsClient cli;
    stringstream content;

    // PubMed search only supports queries that match up to 10,000 items.
    // This is regardless of any limit on the number of items to return;
    // one cannot use RetStart to overcome this limit.
    // See: https://ncbiinsights.ncbi.nlm.nih.gov/2022/11/22/updated-pubmed-eutilities-live/
    cli.SetMaxReturn(101); 
    cli.Search("pubmed", query_asthma, content, CEutilsClient::eUseHistoryEnabled);
    string body = content.str();
    xml::document doc(body.c_str(), body.size(), NULL);
    const xml::node& root = doc.get_root_node();

    xml::node_set items = root.run_xpath_query("./WebEnv/text()");
    BOOST_CHECK_MESSAGE ( 1 == items.size() , "Expecting 1 WebEnv: " + body );
    string web_env = ( items.size() > 0 ) ? items.begin()->get_content() : "";

    items  = root.run_xpath_query("./QueryKey/text()");
    BOOST_CHECK_MESSAGE ( 1 == items.size() , "Expecting 1 QueryKey: " + body );
    int query_key = (items.size() > 0) ? NStr::StringToNumeric<int>(items.begin()->get_content()) : 0;

    if ( !web_env.empty() && query_key > 0 ) {
        items = root.run_xpath_query("./Count/text()");
        BOOST_CHECK_MESSAGE ( 1 == items.size() , "Expecting 1 Count: " + body );
        int count = NStr::StringToNumeric<int>(items.size() ? items.begin()->get_content() : "0");

        items = root.run_xpath_query("./RetMax/text()");
        BOOST_CHECK_MESSAGE ( 1 == items.size() , "Expecting 1 RetMax: " + body );
        int retmax = NStr::StringToNumeric<int>(items.size() ? items.begin()->get_content() : "0");

        items = root.run_xpath_query("./RetStart/text()");
        BOOST_CHECK_MESSAGE ( 1 == items.size() , "Expecting 1 RetStart: " + body );

        int retstart = NStr::StringToNumeric<int>(items.size() ? items.begin()->get_content() : "0");
        BOOST_CHECK_MESSAGE ( retstart + retmax <= count , "Expecting RetStart + RetMax <= Count: " + body );

        // Get next chunk from the history server.
        if ( retstart + retmax < count ) {
            int next_start = count - retmax;
            stringstream next_chunk;
            cli.SearchHistory("pubmed", query_asthma, web_env, query_key, next_start, next_chunk);

            string body = next_chunk.str();
            xml::document doc(body.c_str(), body.size(), NULL);
            const xml::node& root = doc.get_root_node();
            const xml::node_set nodes = root.run_xpath_query("./RetStart/text()");
            BOOST_REQUIRE_MESSAGE ( nodes.size() == 1, "Expecting 1 RetStart: " + body );
            int retstart = NStr::StringToNumeric<int>(nodes.begin()->get_content());
            BOOST_CHECK_MESSAGE ( retstart == next_start , "Expecting RetStart = Nnext start: " + body );
        }
    }
}

BOOST_AUTO_TEST_CASE(TestSummaryHistory)
{
    CEutilsClient cli;
    stringstream content;

    cli.SetMaxReturn(101); 
    cli.Search("pubmed", query_asthma, content, CEutilsClient::eUseHistoryEnabled);
    string body = content.str();
    xml::document doc(body.c_str(), body.size(), NULL);
    const xml::node& root = doc.get_root_node();

    xml::node_set items = root.run_xpath_query("./WebEnv/text()");
    BOOST_CHECK_MESSAGE ( items.size() != 0 , "Expecting 1 WebEnv: " + body );
    string web_env = ( items.size() > 0) ? items.begin()->get_content(): "";

    items  = root.run_xpath_query("./QueryKey/text()");
    BOOST_CHECK_MESSAGE ( items.size() != 0 , "Expecting 1 QueryKey: " + body );
    int query_key = (items.size() > 0) ? NStr::StringToNumeric<int>(items.begin()->get_content()) : 0;

    if ( !web_env.empty() && query_key > 0 ) {
        stringstream summary;
        cli.SummaryHistory("pubmed", web_env, query_key, 0, "2.0", summary);

        {{
             string body = summary.str();
             xml::document doc(body.c_str(), body.size(), NULL);
             const xml::node& root = doc.get_root_node();

             xml::node_set nodes = root.run_xpath_query("//DocumentSummary");
             BOOST_CHECK_MESSAGE ( 101 == nodes.size() , "Expecting 101 DocumentSummary nodes: " + body);
         }}
    }
}

BOOST_AUTO_TEST_CASE(TestFetchHistory)
{
    CEutilsClient cli;
    stringstream content;

    cli.SetMaxReturn(101);
    cli.Search("pubmed", query_asthma, content, CEutilsClient::eUseHistoryEnabled);
    string body = content.str();
    xml::document doc(body.c_str(), body.size(), NULL);
    const xml::node& root = doc.get_root_node();

    xml::node_set items = root.run_xpath_query("./WebEnv/text()");
    BOOST_CHECK_MESSAGE ( items.size() != 0 , "Expecting 1 WebEnv: " + body );
    string web_env = ( 1 == items.size() ) ? items.begin()->get_content() : "";

    items  = root.run_xpath_query("./QueryKey/text()");
    BOOST_CHECK_MESSAGE ( items.size() != 0 , "Expecting 1 QueryKey: " + body );
    int query_key = ( items.size() > 0 ) ? NStr::StringToNumeric<int>(items.begin()->get_content()) : 0;

    if ( !web_env.empty() && query_key > 0 ) {
        stringstream history;
        cli.FetchHistory("pubmed", web_env, query_key, 10, CEutilsClient::eContentType_xml, history);

        {{
             string body = history.str();
             xml::document doc(body.c_str(), body.size(), NULL);
             const xml::node& root = doc.get_root_node();

             BOOST_CHECK_MESSAGE ( 101 == root.run_xpath_query("//PubmedArticle").size() + root.run_xpath_query("//PubmedBookArticle").size() ,
                                   "Expecting 101 PubmedArticle and PubmedBokArticle: " + body);
         }}
    }
}

BOOST_AUTO_TEST_CASE(TestLinkHistory)
{
    CEutilsClient cli;
    stringstream content;

    vector<int> uids;
    uids.push_back(15718680);
    uids.push_back(157427902);

    cli.SetMaxReturn(101); 
    cli.Link("protein", "gene", uids, content, "neighbor_history");
    string body = content.str();
    xml::document doc(body.c_str(), body.size(), NULL);
    const xml::node& root = doc.get_root_node();

    xml::node_set items = root.run_xpath_query("//WebEnv/text()");
    BOOST_CHECK_MESSAGE ( 1 == items.size() , "Expecting 1 WebEnv: " + body );

    items  = root.run_xpath_query("//QueryKey/text()");
    BOOST_CHECK_MESSAGE ( 1 == items.size() , "Expecting 1 QueryKey: " + body );
}

BOOST_AUTO_TEST_CASE(TestLinkOut)
{
    CEutilsClient cli;
    xml::document doc;
    string body;

    {
        vector<TEntrezId> uids;
        uids.push_back(ENTREZ_ID_FROM(int, 124000572));
        uids.push_back(ENTREZ_ID_FROM(int, 124000574));

        cli.SetMaxReturn(101); 
        BOOST_REQUIRE_NO_THROW(cli.LinkOut("nucleotide", uids, doc, "llinks"));
        doc.save_to_string(body);
        const xml::node& root = doc.get_root_node();

        xml::node_set items = root.run_xpath_query("//eLinkResult/LinkSet/IdUrlList/IdUrlSet");
        BOOST_CHECK_MESSAGE ( 2 == items.size() , "Expecting 2 IdUrlSet nodes: " + body );
    }

    //The same with accessions
    {
        vector<objects::CSeq_id_Handle> acc = { objects::CSeq_id_Handle::GetHandle("DQ896796.2"), objects::CSeq_id_Handle::GetHandle("DQ896797.2") };
        cli.LinkOut("nucleotide", acc, doc, "llinkslib");
        doc.save_to_string(body);
        const xml::node& root = doc.get_root_node();

        xml::node_set items = root.run_xpath_query("//eLinkResult/LinkSet/IdUrlList/IdUrlSet");
        BOOST_CHECK_MESSAGE ( 2 == items.size() , "Expecting 2 IdUrlSet nodes: " + body );
    }    
}
