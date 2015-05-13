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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   Sample for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <misc/eutils_client/eutils_client.hpp>


USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CTestEutilsClient::


class CTestEutilsClient : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CTestEutilsClient::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CEutilsClient demo program");

    arg_desc->AddKey("db", "Database",
                    "Entrez Database",
                    CArgDescriptions::eString);
    arg_desc->AddOptionalKey("query", "Query",
                     "Entrez Query",
                     CArgDescriptions::eString);
    arg_desc->AddOptionalKey("count", "Query",
                     "Entrez Query, return count of results",
                     CArgDescriptions::eString);
    arg_desc->AddOptionalKey("docsum", "Id",
                     "Entrez Ids, comma separated.",
                     CArgDescriptions::eString);

    arg_desc->AddDefaultKey("o", "Output",
            "Output stream, default to stdin.",
            CArgDescriptions::eOutputFile, "-", CArgDescriptions::fFileFlags);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)



int CTestEutilsClient::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CEutilsClient ecli;
    CNcbiOstream& ostr = args["o"].AsOutputFile();

    string db = args["db"].AsString();
    if (args["query"].HasValue()) {
        string query = args["query"].AsString();
        vector<int> uids;
        ecli.Search(db, query, uids);
        ITERATE(vector<int>, uidit, uids) {
            ostr << *uidit << NcbiEndl;
        }
    }
     
    if (args["count"].HasValue()) {
        string query = args["count"].AsString();
        vector<int> uids;
        ostr << ecli.Count(db, query) << NcbiEndl;
    }
     
    if (args["docsum"].HasValue()) {
        list<string> idstrs;
        NStr::Split(args["docsum"].AsString(), ",", idstrs);
        vector<int> uids;
        ITERATE(list<string>, id_it, idstrs) {
            uids.push_back(NStr::StringToInt(*id_it) );
        }
        xml::document docsums;
        ecli.Summary(db, uids, docsums);
        docsums.save_to_stream( ostr, xml::save_op_no_decl);
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CTestEutilsClient::Exit(void)
{
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTestEutilsClient().AppMain(argc, argv);
}
