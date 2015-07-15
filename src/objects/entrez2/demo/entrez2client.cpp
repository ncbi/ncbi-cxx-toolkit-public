/* $Id$
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
 * Author:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>
#include <objects/entrez2/Entrez2_db_id.hpp>
#include <objects/entrez2/Entrez2_docsum_list.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_id.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/Entrez2_info.hpp>
#include <objects/entrez2/Entrez2_limits.hpp>
#include <objects/entrez2/Entrez2_link_count.hpp>
#include <objects/entrez2/Entrez2_link_count_list.hpp>
#include <objects/entrez2/entrez2_client.hpp>

#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);


class CEntrez2ClientApp : public CNcbiApplication
{
public:
    CEntrez2ClientApp();

    virtual void Init(void);
    virtual int  Run (void);

private:
    CRef<CEntrez2Client>     m_Client;
    CNcbiOstream*            m_Ostream;
    auto_ptr<CObjectOStream> m_ObjOstream;

    //
    // handlers for specific requests
    //

    void x_GetInfo         (CEntrez2Client& client);
    void x_GetCount        (CEntrez2Client& client,
                            const string& query, const string& db);
    void x_GetParsedQuery  (CEntrez2Client& client,
                            const string& query, const string& db);
    void x_GetUids         (CEntrez2Client& client,
                            const string& query, const string& db,
                            int start, int max_num);
    void x_GetDocsums      (CEntrez2Client& client,
                            const string& query, const string& db,
                            int start, int max_num);
    void x_GetTermPos      (CEntrez2Client& client,
                            const string& query, const string& db);
    void x_GetTermList     (CEntrez2Client& client,
                            const string& query, const string& db);
    void x_GetTermHierarchy(CEntrez2Client& client,
                            const string& query, const string& db);
    void x_GetLinks        (CEntrez2Client& client,
                            const string& query, const string& db);
    void x_GetLinked       (CEntrez2Client& client,
                            const string& query, const string& db);
    void x_GetLinkCounts   (CEntrez2Client& client,
                            const string& query, const string& db);

    // format a reply (basic formatting only)
    void x_FormatReply(CEntrez2_boolean_reply& reply);

    // main internal query function
    CEntrez2_boolean_reply* x_EvalBoolean(CEntrez2Client& client,
                                          const string& query,
                                          const string& db,
                                          bool parse, bool uids,
                                          int start = -1, int max_num = -1);
};


CEntrez2ClientApp::CEntrez2ClientApp()
{
    DisableArgDescriptions(fDisableStdArgs);
    // Hide meaningless options so not to confuse the public
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion | fHideDryRun);
}


void CEntrez2ClientApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("query", "QueryString", "Query to submit",
                     CArgDescriptions::eString);

    arg_desc->AddKey("db", "Database", "Name of database to query",
                     CArgDescriptions::eString);

    arg_desc->AddDefaultKey("lt", "Lookup", "Type of lookup to perform",
                            CArgDescriptions::eString, "count");
    arg_desc->SetConstraint("lt",
                            &(*new CArgAllow_Strings,
                              "info",     // get-info request
                              "count",    // eval-boolean: count only
                              "parse",    // eval-boolean: parsed query
                              "uids",     // eval-boolean: uid list
                              "docsum",   // get docsums for our query
                              "termpos",  // get term positions
                              "termhier", // get term hierarchy
                              "termlist", // get term list
                              "links",    // get links
                              "linked",   // get linked status
                              "linkct"    // get link count
                             ));

    arg_desc->AddDefaultKey("start", "StartPos",
                            "Starting point in the UID list for retrieval",
                            CArgDescriptions::eInteger, "-1");
    arg_desc->AddDefaultKey("max", "MaxNum",
                            "Maximum number of records (or UIDs) to retrieve",
                            CArgDescriptions::eInteger, "-1");

    arg_desc->AddDefaultKey("out", "OutputFile", "File to dump output to",
                            CArgDescriptions::eOutputFile, "-");

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Entrez2Client command-line demo application");

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());
}


int CEntrez2ClientApp::Run(void)
{
    const CArgs& args = GetArgs();

    string db          = args["db"].AsString();
    string query       = args["query"].AsString();
    string lt          = args["lt"].AsString();
    CNcbiOstream& ostr = args["out"].AsOutputFile();

    int start          = args["start"].AsInteger();
    int max_num        = args["max"].AsInteger();

    m_Client.Reset(new CEntrez2Client());
    m_Client->SetDefaultRequest().SetTool("Entrez2 Command-Line Test");
    m_ObjOstream.reset(CObjectOStream::Open(eSerial_AsnText, ostr));
    m_Ostream = &ostr;

    //
    // process each lookup type
    //

    if (lt == "info") {
        LOG_POST(Info << "performing look-up type: get-info");
        x_GetInfo(*m_Client);
    } else if (lt == "count") {
        LOG_POST(Info << "performing look-up type: eval-boolean "
                 "(count only)");
        x_GetCount(*m_Client, query, db);
    } else if (lt == "parse") {
        LOG_POST(Info << "performing look-up type: eval-boolean"
                 " (show parsed expression)");
        x_GetParsedQuery(*m_Client, query, db);
    } else if (lt == "uids") {
        LOG_POST(Info << "performing look-up type: eval-boolean"
                 " (UID list)");
        x_GetUids(*m_Client, query, db, start, max_num);
    } else if (lt == "docsum") {
        LOG_POST(Info << "performing look-up type: get-docsum");
        x_GetDocsums(*m_Client, query, db, start, max_num);
    } else if (lt == "termpos") {
        LOG_POST(Info << "performing look-up type: get-term-pos");
        x_GetTermPos(*m_Client, query, db);
    } else if (lt == "termlist") {
        LOG_POST(Info << "performing look-up type: get-term-list");
        x_GetTermList(*m_Client, query, db);
    } else if (lt == "termhier") {
        LOG_POST(Info << "performing look-up type: get-term-hierarchy");
        x_GetTermHierarchy(*m_Client, query, db);
    } else if (lt == "links") {
        LOG_POST(Info << "performing look-up type: get-links");
        x_GetLinks(*m_Client, query, db);
    } else if (lt == "linked") {
        LOG_POST(Info << "performing look-up type: get-linked");
        x_GetLinked(*m_Client, query, db);
    } else if (lt == "linkct") {
        LOG_POST(Info << "performing look-up type: get-link-counts");
        x_GetLinkCounts(*m_Client, query, db);
    } else {
        LOG_POST(Error << "unrecognized lookup type: " << lt);
        return 1;
    }

    return 0;
}


//
// get-info
// returns information about active databases
//
void CEntrez2ClientApp::x_GetInfo(CEntrez2Client& client)
{
    CRef<CEntrez2_info> info = client.AskGet_info();
    *m_ObjOstream << *info;
}


//
// display only the number fo records that match a query
//
void CEntrez2ClientApp::x_GetCount(CEntrez2Client& client,
                                   const string& query, const string& db)
{
    *m_Ostream << "query: " << query << endl;
    CRef<CEntrez2_boolean_reply> reply(x_EvalBoolean(client, query, db,
                                                     false, false));
    x_FormatReply(*reply);
}


//
// display only the number fo records that match a query
//
void CEntrez2ClientApp::x_GetUids(CEntrez2Client& client,
                                  const string& query, const string& db,
                                  int start, int max_num)
{
    CRef<CEntrez2_boolean_reply> reply(x_EvalBoolean(client, query, db,
                                                     true, true,
                                                     start, max_num));
    *m_Ostream << "query: " << query << endl;
    x_FormatReply(*reply);
}


//
// display only the number fo records that match a query
//
void CEntrez2ClientApp::x_GetParsedQuery(CEntrez2Client& client,
                                         const string& query,
                                         const string& db)
{
    CRef<CEntrez2_boolean_reply> reply(x_EvalBoolean(client, query, db,
                                                     true, false));
    x_FormatReply(*reply);
}


//
// display docsums for a given query
//
void CEntrez2ClientApp::x_GetDocsums(CEntrez2Client& client,
                                     const string& query,
                                     const string& db,
                                     int start, int max_num)
{
    // retrieve our list of UIDs
    CRef<CEntrez2_boolean_reply> reply(x_EvalBoolean(client, query, db,
                                                     false, true,
                                                     start, max_num));

    // retrieve the docsums
    CRef<CEntrez2_docsum_list> docsums =
        client.AskGet_docsum(reply->GetUids());

    *m_Ostream << reply->GetCount() << " records match" << endl;
    *m_Ostream << "docsums:" << endl;
    *m_Ostream << string(72, '-') << endl;
    *m_ObjOstream << *docsums;
    *m_Ostream << endl << string(72, '-') << endl;
}


//
// display term position for a given query
//
void CEntrez2ClientApp::x_GetTermPos(CEntrez2Client& /* client */,
                                     const string& /* query */,
                                     const string& /* db */)
{
    LOG_POST(Error << "get-term-pos query unimplemented");
}


//
// display term list for a given query
//
void CEntrez2ClientApp::x_GetTermList(CEntrez2Client& /* client */,
                                      const string& /* query */,
                                      const string& /* db */)
{
    LOG_POST(Error << "get-term-list query unimplemented");
}


//
// display term hierarchy for a given query
//
void CEntrez2ClientApp::x_GetTermHierarchy(CEntrez2Client& /* client */,
                                           const string& /* query */,
                                           const string& /* db */)
{
    LOG_POST(Error << "get-term-hierarchy query unimplemented");
}


//
// display links for a given query
//
void CEntrez2ClientApp::x_GetLinks(CEntrez2Client& /* client */,
                                   const string& /* query */,
                                   const string& /* db */)
{
    LOG_POST(Error << "get-links query unimplemented");
}


//
// display linked status for a given query
//
void CEntrez2ClientApp::x_GetLinked(CEntrez2Client& /* client */,
                                    const string& /* query */,
                                    const string& /* db */)
{
    LOG_POST(Error << "get-linked query unimplemented");
}


//
// display link counts for a given query
//
void CEntrez2ClientApp::x_GetLinkCounts(CEntrez2Client& client,
                                        const string& query,
                                        const string& db)
{
    *m_Ostream << "query: " << query << endl;

    CEntrez2_id req;

    req.SetDb(CEntrez2_db_id(db));
    req.SetUid(NStr::StringToInt(query));

    CRef<CEntrez2_link_count_list> reply = client.AskGet_link_counts(req);

    *m_Ostream << "Link counts: " << reply->GetLink_type_count() << endl;
    ITERATE(CEntrez2_link_count_list::TLinks, it, reply->GetLinks()) {
        *m_Ostream << "       Type: " << string((*it)->GetLink_type()) << endl;
        *m_Ostream << "      Count: " << (*it)->GetLink_count() << endl;
    }
}


//
// eval-boolean
// returns a count of the number of items that match
//
CEntrez2_boolean_reply*
CEntrez2ClientApp::x_EvalBoolean(CEntrez2Client& client,
                                 const string& query, const string& db,
                                 bool parse, bool uids,
                                 int start, int max_num)
{
    CEntrez2_eval_boolean req;

    if (parse) {
        req.SetReturn_parse(true);
    }
    if (uids) {
        req.SetReturn_UIDs(true);
    }
    CEntrez2_boolean_exp& exp = req.SetQuery();

    if (start != -1) {
        exp.SetLimits().SetOffset_UIDs(start);
    }

    if (max_num != -1) {
        exp.SetLimits().SetMax_UIDs(max_num);
    }

    // set the database we're querying
    exp.SetDb().Set(db);

    // set the query
    CRef<CEntrez2_boolean_element> elem(new CEntrez2_boolean_element());
    elem->SetStr(query);
    exp.SetExp().push_back(elem);

    CRef<CEntrez2_boolean_reply> reply = client.AskEval_boolean(req);
    return reply.Release();
}


//
// x_FormatReply()
// dump results from the reply block
//
void CEntrez2ClientApp::x_FormatReply(CEntrez2_boolean_reply& reply)
{
    if (reply.CanGetCount()) {
        *m_Ostream << reply.GetCount() << " uid(s) match" << endl;
    }

    if (reply.IsSetQuery()) {
        *m_Ostream << "parsed query:" << endl;
        *m_Ostream << string(75, '-') << endl;
        *m_ObjOstream << reply.GetQuery();
        *m_Ostream << endl << string(75, '-') << endl;
    }

    if (reply.IsSetUids()) {
        *m_Ostream << "decoded UIDs:" << endl;
        CEntrez2_id_list::TConstUidIterator iter =
            reply.GetUids().GetConstUidIterator();
        for ( ; !iter.AtEnd();  ++iter) {
            *m_Ostream << "    " << *iter << endl;
        }
    }
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CEntrez2ClientApp().AppMain(argc, argv);
}
