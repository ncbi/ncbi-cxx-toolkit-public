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
#include <connect/ncbi_core_cxx.hpp>
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
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/Entrez2_info.hpp>
#include <objects/entrez2/Entrez2_limits.hpp>
#include <objects/entrez2/entrez2_client.hpp>

#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

using namespace ncbi;
using namespace objects;


class CEntrez2ClientApp : public CNcbiApplication
{
public:
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
                            const string& query, const string& db);
    void x_GetDocsums      (CEntrez2Client& client,
                            const string& query, const string& db,
                            int start, int max_num);

    void x_GetTermPositions(CEntrez2Client& client,
                            const string& query, const string& db);
    void x_GetTermList(CEntrez2Client& client,
                       const string& query, const string& db);
    void x_GetTermHierarchy(CEntrez2Client& client,
                            const string& query, const string& db);
    void x_GetLinks(CEntrez2Client& client,
                    const string& query, const string& db);
    void x_GetLinked(CEntrez2Client& client,
                     const string& query, const string& db);
    void x_GetLinkCounts(CEntrez2Client& client,
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


void CEntrez2ClientApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("query", "QueryString", "Query to submit",
                     CArgDescriptions::eString);

    arg_desc->AddKey("db", "Database", "Name of database to query",
                     CArgDescriptions::eString);

    arg_desc->AddDefaultKey("lt", "Lookup", "Type of lookup to perform",
                            CArgDescriptions::eString, "info");
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
                            "Maximum number of records to retrieve",
                            CArgDescriptions::eInteger, "-1");

    arg_desc->AddDefaultKey("out", "OutputFile", "File to dump output to",
                            CArgDescriptions::eOutputFile, "-");

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CEntrez2ClientApp::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    CArgs args = GetArgs();

    string db          = args["db"].AsString();
    string query       = args["query"].AsString();
    string lt          = args["lt"].AsString();
    CNcbiOstream& ostr = args["out"].AsOutputFile();

    int start          = args["start"].AsInteger();
    int max_num        = args["max"].AsInteger();

    m_Client.Reset(new CEntrez2Client());
    m_Client->SetDefaultRequest().SetTool("Entrez2 Command-line Test");
    m_ObjOstream.reset(CObjectOStream::Open(eSerial_AsnText, ostr));
    m_Ostream = &ostr;

    //
    // process each lookup type
    //

    if (lt == "info") {
        LOG_POST(Info << "performing look-up type: get-info");
        x_GetInfo(*m_Client);
    } else if (lt == "count") {
        LOG_POST(Info << "performing look-up type: eval-boolean (count only)");
        x_GetCount(*m_Client, query, db);
    } else if (lt == "parse") {
        LOG_POST(Info << "performing look-up type: eval-boolean (show parsed expression)");
        x_GetParsedQuery(*m_Client, query, db);
    } else if (lt == "uids") {
        LOG_POST(Info << "performing look-up type: eval-boolean (UID list)");
        x_GetUids(*m_Client, query, db);
    } else if (lt == "docsum") {
        LOG_POST(Info << "performing look-up type: get-docsum");
        x_GetDocsums(*m_Client, query, db, start, max_num);
    } else if (lt == "termpos") {
        LOG_POST(Info << "performing look-up type: get-termpos");
        x_GetTermPositions(*m_Client, query, db);
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
                                  const string& query, const string& db)
{
    CRef<CEntrez2_boolean_reply> reply(x_EvalBoolean(client, query, db,
                                                     true, true));
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
// display term positions for a given query
//
void CEntrez2ClientApp::x_GetTermPositions(CEntrez2Client& /* client */,
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
void CEntrez2ClientApp::x_GetLinkCounts(CEntrez2Client& /* client */,
                                        const string& /* query */,
                                        const string& /* db */)
{
    LOG_POST(Error << "get-link-counts query unimplemented");
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
        *m_Ostream << reply.GetCount() << " uids match" << endl;
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
    return CEntrez2ClientApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/02/02 19:49:54  grichenk
 * Fixed more warnings
 *
 * Revision 1.4  2004/05/19 17:21:09  gorelenk
 * Added include of PCH - ncbi_pch.hpp
 *
 * Revision 1.3  2003/11/20 15:41:17  ucko
 * Update for new (saner) treatment of ASN.1 NULLs.
 *
 * Revision 1.2  2003/07/31 18:12:26  dicuccio
 * Code clean-up.  Added limits for docsum retrieval (start pos / max number of
 * records)
 *
 * Revision 1.1  2003/07/31 17:38:51  dicuccio
 * Added subproject demo with a single applicaiton, a command-line demo app
 *
 * ===========================================================================
 */

