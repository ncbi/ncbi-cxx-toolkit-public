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
 *   Example of using e-utils
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/eutils/api/eutils.hpp>
#include <objtools/eutils/api/einfo.hpp>
#include <objtools/eutils/api/esearch.hpp>
#include <objtools/eutils/api/egquery.hpp>
#include <objtools/eutils/api/efetch.hpp>
#include <objtools/eutils/api/epost.hpp>
#include <objtools/eutils/api/elink.hpp>
#include <objtools/eutils/api/esummary.hpp>
#include <objtools/eutils/api/espell.hpp>
#include <objtools/eutils/api/ehistory.hpp>
#include <objtools/eutils/einfo/einfo__.hpp>
#include <objtools/eutils/esearch/esearch__.hpp>
#include <objtools/eutils/egquery/egquery__.hpp>
#include <objtools/eutils/elink/elink__.hpp>
#include <objtools/eutils/esummary/esummary__.hpp>
#include <objtools/eutils/espell/espell__.hpp>
#include <objtools/eutils/ehistory/ehistory__.hpp>


/////////////////////////////////////////////////////////////////////////////
//
//  EUtils demo application
//


BEGIN_NCBI_SCOPE
USING_SCOPE(NCBI_NS_NCBI::objects);


class CEUtilsApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
    virtual void Exit(void);
private:
    void CallEInfo(const CArgs& args);
    void CallESearch(const CArgs& args);
    void CallEPost(const CArgs& args);
    void CallEGQuery(const CArgs& args);
    void CallELink(const CArgs& args);
    void CallESummary(const CArgs& args);
    void CallEFetch(const CArgs& args);
    void CallESpell(const CArgs& args);
    void CallEHistory(const CArgs& args);

    CEFetch_Request* x_CreateLitRequest(const CArgs& args);
    CEFetch_Request* x_CreateSeqRequest(const CArgs& args);
    CEFetch_Request* x_CreateTaxRequest(const CArgs& args);

    CRef<CEUtils_ConnContext>& x_GetCtx(void);
    void x_SetHttpMethod(CEUtils_Request& req, const CArgs& args);
    void x_DumpRequest(CEUtils_Request& req);

    CRef<CEUtils_ConnContext> m_Ctx;
    bool                      m_DumpRequests;
};


CRef<CEUtils_ConnContext>& CEUtilsApp::x_GetCtx(void)
{
    if ( !m_Ctx ) {
        m_Ctx.Reset(new CEUtils_ConnContext);
    }
    return m_Ctx;
}


void CEUtilsApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddFlag("einfo", "Call einfo utility", true);
    arg_desc->AddFlag("efetch", "Call efetch utility", true);
    // arg_desc->SetDependency("einfo", CArgDescriptions::eExcludes, "efetch");
    arg_desc->AddFlag("esearch", "Call esearch utility", true);
    arg_desc->AddFlag("epost", "Call epost utility", true);
    arg_desc->AddFlag("elink", "Call elink utility", true);
    arg_desc->AddFlag("egquery", "Call egquery utility", true);
    arg_desc->AddFlag("esummary", "Call esummary utility", true);
    arg_desc->AddFlag("espell", "Call espell utility", true);
    arg_desc->AddFlag("ehistory", "Call ehistory utility", true);

    // Switch HTTP method
    arg_desc->AddDefaultKey("http", "Method",
        "HTTP method used to send requests",
        CArgDescriptions::eString, "post");
    arg_desc->SetConstraint("http", &(*new CArgAllow_Strings,
        "post", "get"));

    // Debug flag
    arg_desc->AddFlag("dump", "Print raw incoming data", true);

    // Context setup
    arg_desc->AddOptionalKey("webenv", "WebEnv", "Web environment",
        CArgDescriptions::eString);
    arg_desc->AddOptionalKey("query_key", "query_key", "Query key",
        CArgDescriptions::eString);
    arg_desc->AddOptionalKey("tool", "tool", "Client tool",
        CArgDescriptions::eString);
    arg_desc->AddOptionalKey("email", "email", "Client e-mail",
        CArgDescriptions::eString);

    // Arguments for all queries
    arg_desc->AddOptionalKey("db", "db", "Database name",
        CArgDescriptions::eString);
    arg_desc->AddOptionalKey("id", "id", "ID to fetch",
        CArgDescriptions::eString);
    arg_desc->AddOptionalKey("term", "term", "Term to search for",
        CArgDescriptions::eString);
    arg_desc->AddOptionalKey("retstart", "RetStart", "First item to fetch",
        CArgDescriptions::eInteger);
    arg_desc->SetConstraint("retstart", new CArgAllow_Integers(1, kMax_Int));
    arg_desc->AddOptionalKey("retmax", "RetMax", "Number of items to fetch",
        CArgDescriptions::eInteger);
    arg_desc->SetConstraint("retmax", new CArgAllow_Integers(1, kMax_Int));
    arg_desc->AddDefaultKey("retmode", "RetMode", "Data format",
        CArgDescriptions::eString, "text");
    arg_desc->SetConstraint("retmode", &(*new CArgAllow_Strings,
        "text", "xml", "asn", "html"));
    arg_desc->AddOptionalKey("rettype", "RetType", "Fetched data type",
        CArgDescriptions::eString);
    arg_desc->SetConstraint("rettype", &(*new CArgAllow_Strings,
        // Literature
        "uilist", "abstract", "citation", "medline", "full",
        // Sequence
        "native", "fasta", "gb", "gbc", "gbwithparts", "est", "gss",
        "gp", "gpc", "seqid", "acc", "chr", "flt", "rsr", "brief", "docset"));
    arg_desc->AddOptionalKey("reldate", "RelDate", "Age of date in days",
        CArgDescriptions::eInteger);
    // taxonomy only
    arg_desc->AddOptionalKey("report", "Report", "Taxonomy data format",
        CArgDescriptions::eString);
    arg_desc->SetConstraint("report", &(*new CArgAllow_Strings,
        "uilist", "brief", "docsum", "xml"));
    // esearch flag
    arg_desc->AddOptionalKey("usehistory", "UseHistory", "Use history",
        CArgDescriptions::eBoolean);

    // Dependencies
    // ESearch
    arg_desc->SetDependency("esearch", CArgDescriptions::eRequires, "db");
    arg_desc->SetDependency("esearch", CArgDescriptions::eRequires, "term");
    arg_desc->SetDependency("usehistory", CArgDescriptions::eRequires, "esearch");
    // EGQuery
    arg_desc->SetDependency("egquery", CArgDescriptions::eRequires, "term");
    // EFetch
    arg_desc->SetDependency("efetch", CArgDescriptions::eRequires, "db");
    arg_desc->SetDependency("epost", CArgDescriptions::eRequires, "db");
    arg_desc->SetDependency("epost", CArgDescriptions::eRequires, "id");
    // ELink
    arg_desc->SetDependency("elink", CArgDescriptions::eRequires, "db");
    // ESummary
    arg_desc->SetDependency("esummary", CArgDescriptions::eRequires, "db");
    // ESpell
    arg_desc->SetDependency("espell", CArgDescriptions::eRequires, "db");
    arg_desc->SetDependency("espell", CArgDescriptions::eRequires, "term");

    // elink arguments
    // dbfrom
    arg_desc->AddOptionalKey("dbfrom", "dbfrom", "origination db for elink",
        CArgDescriptions::eString);
    // cmd
    arg_desc->AddOptionalKey("cmd", "Command", "elink command",
        CArgDescriptions::eString);
    arg_desc->SetConstraint("cmd", &(*new CArgAllow_Strings,
        "prlinks", "llinks", "llinkslib", "lcheck", "ncheck",
        "neighbor", "neighbor_history", "acheck"));
    // linkname
    arg_desc->AddOptionalKey("linkname", "Linkname", "elink linkname",
        CArgDescriptions::eString);
    // holding
    arg_desc->AddOptionalKey("holding", "holding", "elink holding",
        CArgDescriptions::eString);
    // version
    arg_desc->AddOptionalKey("version", "Version", "elink DTD version",
        CArgDescriptions::eString);

    // Program description
    string prog_description =
        "Test loading data from EUtils";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());
}


void CEUtilsApp::x_SetHttpMethod(CEUtils_Request& req, const CArgs& args)
{
    if ( args["http"].AsString() == "get" ) {
        req.SetRequestMethod(CEUtils_Request::eHttp_Get);
    }
}


void CEUtilsApp::x_DumpRequest(CEUtils_Request& req)
{
    string data;
    req.Read(&data);
    cout << data << endl;
}


void CEUtilsApp::CallEInfo(const CArgs& args)
{
    string db = args["db"] ? args["db"].AsString() : kEmptyStr;
    CEInfo_Request req(db, x_GetCtx());
    x_SetHttpMethod(req, args);

    cout << req.GetScriptName() << "?" << req.GetQueryString() << endl;

    if ( m_DumpRequests ) {
        x_DumpRequest(req);
        return;
    }

    CRef<einfo::CEInfoResult> result = req.GetEInfoResult();
    _ASSERT(result);
    cout << MSerial_Xml << *result << endl;
    if ( result->IsDbList() ) {
        ITERATE(einfo::CDbList::TDbName, it, result->GetDbList().GetDbName()) {
            req.SetDatabase(*it);
            cout << req.GetScriptName() << "?" << req.GetQueryString() << endl;
            CRef<einfo::CEInfoResult> db_info = req.GetEInfoResult();
            _ASSERT(db_info);
            cout << MSerial_Xml << *db_info << endl;
        }
    }
}


void CEUtilsApp::CallESearch(const CArgs& args)
{
    // Prepare request
    CESearch_Request req(args["db"].AsString(), x_GetCtx());
    x_SetHttpMethod(req, args);

    req.SetTerm(args["term"].AsString());
    if ( args["usehistory"] ) {
        req.SetUseHistory(args["usehistory"].AsBoolean());
    }
    if ( args["reldate"] ) {
        req.SetRelDate(args["reldate"].AsInteger());
    }

    // Print query string
    cout << req.GetScriptName() << "?" << req.GetQueryString() << endl;

    if ( m_DumpRequests ) {
        x_DumpRequest(req);
        return;
    }

    // Get and show the results
    CRef<esearch::CESearchResult> result = req.GetESearchResult();
    _ASSERT(result);
    cout << MSerial_Xml << *result << endl;
    cout << "WebEnv=" << x_GetCtx()->GetWebEnv() << endl;
    cout << "query_key=" << x_GetCtx()->GetQueryKey() << endl;
}


void CEUtilsApp::CallEGQuery(const CArgs& args)
{
    // Prepare request
    CEGQuery_Request req(x_GetCtx());
    x_SetHttpMethod(req, args);

    req.SetTerm(args["term"].AsString());

    // Print query string
    cout << req.GetScriptName() << "?" << req.GetQueryString() << endl;

    if ( m_DumpRequests ) {
        x_DumpRequest(req);
        return;
    }

    // Get and show the results
    CRef<egquery::CResult> result = req.GetResult();
    _ASSERT(result);
    cout << MSerial_Xml << *result << endl;
}


void CEUtilsApp::CallEPost(const CArgs& args)
{
    // Prepare request
    CEPost_Request req(args["db"].AsString(), x_GetCtx());
    x_SetHttpMethod(req, args);

    req.GetId().SetIds(args["id"].AsString());

    // Print query string
    cout << req.GetScriptName() << "?" << req.GetQueryString() << endl;

    if ( m_DumpRequests ) {
        x_DumpRequest(req);
        return;
    }

    // Get and show the results
    CRef<epost::CEPostResult> result = req.GetEPostResult();
    _ASSERT(result);
    cout << MSerial_Xml << *result << endl;
    cout << "WebEnv=" << x_GetCtx()->GetWebEnv() << endl;
    cout << "query_key=" << x_GetCtx()->GetQueryKey() << endl;
}


void CEUtilsApp::CallELink(const CArgs& args)
{
    // Prepare request
    CELink_Request req(args["db"].AsString(), x_GetCtx());
    x_SetHttpMethod(req, args);

    if ( args["dbfrom"] ) {
        req.SetDbFrom(args["dbfrom"].AsString());
    }
    // If WebEnv was set (as an command line argument or by previous requests)
    // use it instead of id.
    if ( req.GetConnContext()->GetWebEnv().empty()  &&  args["id"] ) {
        req.GetIdGroups().SetGroups(args["id"].AsString());
    }
    if ( args["cmd"] ) {
        string cmd = args["cmd"].AsString();
        if (cmd == "prlinks") {
            req.SetCommand(CELink_Request::eCmd_prlinks);
        }
        else if (cmd == "llinks" ) {
            req.SetCommand(CELink_Request::eCmd_llinks);
        }
        else if (cmd == "llinkslib") {
            req.SetCommand(CELink_Request::eCmd_llinkslib);
        }
        else if (cmd == "lcheck") {
            req.SetCommand(CELink_Request::eCmd_lcheck);
        }
        else if (cmd == "ncheck") {
            req.SetCommand(CELink_Request::eCmd_ncheck);
        }
        else if (cmd == "neighbor") {
            req.SetCommand(CELink_Request::eCmd_neighbor);
        }
        else if (cmd == "neighbor_history") {
            req.SetCommand(CELink_Request::eCmd_neighbor_history);
        }
        else if (cmd == "acheck") {
            req.SetCommand(CELink_Request::eCmd_acheck);
        }
        else {
            ERR_POST(Error << "Unsupported elink command: " << cmd);
        }
    }
    if ( args["linkname"] ) {
        req.SetLinkName(args["linkname"].AsString());
    }
    if ( args["holding"] ) {
        req.SetHolding(args["holding"].AsString());
    }
    if ( args["version"] ) {
        req.SetHolding(args["version"].AsString());
    }
    if ( args["reldate"] ) {
        req.SetRelDate(args["reldate"].AsInteger());
    }

    // Print query string
    cout << req.GetScriptName() << "?" << req.GetQueryString() << endl;

    if ( m_DumpRequests ) {
        x_DumpRequest(req);
        return;
    }

    // Get and show the results
    CRef<elink::CELinkResult> result = req.GetELinkResult();
    _ASSERT(result);
    cout << MSerial_Xml << *result << endl;
}


void CEUtilsApp::CallESummary(const CArgs& args)
{
    // Prepare request
    string db = args["db"] ? args["db"].AsString() : kEmptyStr;
    CESummary_Request req(db, x_GetCtx());
    x_SetHttpMethod(req, args);

    // If WebEnv was set (as an command line argument or by previous requests)
    // use it instead of id.
    if ( req.GetConnContext()->GetWebEnv().empty()  &&  args["id"] ) {
        req.GetId().SetIds(args["id"].AsString());
    }
    if ( args["retstart"] ) {
        req.SetRetStart(args["retstart"].AsInteger());
    }
    if ( args["retmax"] ) {
        req.SetRetMax(args["retmax"].AsInteger());
    }

    // Print query string
    cout << req.GetScriptName() << "?" << req.GetQueryString() << endl;

    if ( m_DumpRequests ) {
        x_DumpRequest(req);
        return;
    }

    try {
        // Get and show the results. GetESummaryResult() may fail if the
        // selected database uses an incompatible DTD, so report exceptions
        // if any (in real life this case must be handled by using XmlWrapp
        // library to parse the data).
        CRef<esummary::CESummaryResult> result = req.GetESummaryResult();
        _ASSERT(result);
        cout << MSerial_Xml << *result << endl;
    }
    catch (CSerialException& ex) {
        cout << "Deserialization error: " << ex.what() << endl;
    }
}


void CEUtilsApp::CallESpell(const CArgs& args)
{
    // Prepare request
    string db = args["db"] ? args["db"].AsString() : kEmptyStr;
    CESpell_Request req(db, x_GetCtx());
    x_SetHttpMethod(req, args);

    req.SetTerm(args["term"].AsString());

    // Print query string
    cout << req.GetScriptName() << "?" << req.GetQueryString() << endl;

    if ( m_DumpRequests ) {
        x_DumpRequest(req);
        return;
    }

    // Get and show the results
    CRef<espell::CESpellResult> result = req.GetESpellResult();
    _ASSERT(result);
    cout << MSerial_Xml << *result << endl;
}


void CEUtilsApp::CallEHistory(const CArgs& args)
{
    // Prepare request
    string db;
    if ( args["db"] ) {
        db = args["db"].AsString();
    }
    CEHistory_Request req(db, x_GetCtx());
    x_SetHttpMethod(req, args);

    // Print query string
    cout << req.GetScriptName() << "?" << req.GetQueryString() << endl;

    if ( m_DumpRequests ) {
        x_DumpRequest(req);
        return;
    }

    // Get and show the results
    CRef<ehistory::CEHistoryResult> result = req.GetEHistoryResult();
    _ASSERT(result);
    cout << MSerial_Xml << *result << endl;
}


CEFetch_Request* CEUtilsApp::x_CreateLitRequest(const CArgs& args)
{
    // Prepare literature request
    string db = args["db"].AsString();
    CEFetch_Literature_Request::ELiteratureDB ldb;
    if (db == "pubmed") {
        ldb = CEFetch_Literature_Request::eDB_pubmed;
    }
    else if (db == "pmc") {
        ldb = CEFetch_Literature_Request::eDB_pmc;
    }
    else if (db == "journals") {
        ldb = CEFetch_Literature_Request::eDB_journals;
    }
    else if (db == "omim") {
        ldb = CEFetch_Literature_Request::eDB_omim;
    }
    else {
        return 0;
    }
    auto_ptr<CEFetch_Literature_Request> lit_req(
        new CEFetch_Literature_Request(ldb, x_GetCtx()));

    string rettype = args["rettype"] ? args["rettype"].AsString() : kEmptyStr;
    if (rettype == "uilist") {
        lit_req->SetRetType(CEFetch_Literature_Request::eRetType_uilist);
    }
    else if (rettype == "abstract") {
        lit_req->SetRetType(CEFetch_Literature_Request::eRetType_abstract);
    }
    else if (rettype == "citation") {
        lit_req->SetRetType(CEFetch_Literature_Request::eRetType_citation);
    }
    else if (rettype == "medline") {
        lit_req->SetRetType(CEFetch_Literature_Request::eRetType_medline);
    }
    else if (rettype == "full") {
        lit_req->SetRetType(CEFetch_Literature_Request::eRetType_full);
    }
    else if ( !rettype.empty() ) {
        ERR_POST(Error << "Rettype " << rettype <<
            " is incompatible with the selected database " << db);
        return 0;
    }

    return lit_req.release();
}


CEFetch_Request* CEUtilsApp::x_CreateSeqRequest(const CArgs& args)
{
    // Prepare sequence request
    string db = args["db"].AsString();
    CEFetch_Sequence_Request::ESequenceDB sdb;
    if (db == "gene") {
        sdb = CEFetch_Sequence_Request::eDB_gene;
    }
    else if (db == "genome") {
        sdb = CEFetch_Sequence_Request::eDB_genome;
    }
    else if (db == "nucleotide") {
        sdb = CEFetch_Sequence_Request::eDB_nucleotide;
    }
    else if (db == "nuccore") {
        sdb = CEFetch_Sequence_Request::eDB_nuccore;
    }
    else if (db == "nucest") {
        sdb = CEFetch_Sequence_Request::eDB_nucest;
    }
    else if (db == "nucgss") {
        sdb = CEFetch_Sequence_Request::eDB_nucgss;
    }
    else if (db == "protein") {
        sdb = CEFetch_Sequence_Request::eDB_protein;
    }
    else if (db == "popset") {
        sdb = CEFetch_Sequence_Request::eDB_popset;
    }
    else if (db == "snp") {
        sdb = CEFetch_Sequence_Request::eDB_snp;
    }
    else if (db == "sequences") {
        sdb = CEFetch_Sequence_Request::eDB_sequences;
    }
    else {
        return 0;
    }
    auto_ptr<CEFetch_Sequence_Request> seq_req(
        new CEFetch_Sequence_Request(sdb, x_GetCtx()));

    string rettype = args["rettype"] ? args["rettype"].AsString() : kEmptyStr;
    if (rettype == "native") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_native);
    }
    else if (rettype == "fasta") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_fasta);
    }
    else if (rettype == "gb") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_gb);
    }
    else if (rettype == "gbc") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_gbc);
    }
    else if (rettype == "gbwithparts") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_gbwithparts);
    }
    else if (rettype == "est") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_est);
    }
    else if (rettype == "gss") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_gss);
    }
    else if (rettype == "gp") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_gp);
    }
    else if (rettype == "gpc") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_gpc);
    }
    else if (rettype == "seqid") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_seqid);
    }
    else if (rettype == "acc") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_acc);
    }
    else if (rettype == "chr") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_chr);
    }
    else if (rettype == "flt") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_flt);
    }
    else if (rettype == "rsr") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_rsr);
    }
    else if (rettype == "brief") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_brief);
    }
    else if (rettype == "docset") {
        seq_req->SetRetType(CEFetch_Sequence_Request::eRetType_docset);
    }
    else if ( !rettype.empty() ) {
        ERR_POST(Error << "Rettype " << rettype <<
            " is incompatible with the selected database " << db);
        return 0;
    }

    return seq_req.release();
}


CEFetch_Request* CEUtilsApp::x_CreateTaxRequest(const CArgs& args)
{
    // Prepare taxonomy request
    string db = args["db"].AsString();
    if (db != "taxonomy") {
        return 0;
    }
    auto_ptr<CEFetch_Taxonomy_Request> tax_req(
        new CEFetch_Taxonomy_Request(x_GetCtx()));

    if ( args["report"] ) {
        string report = args["report"].AsString();
        if (report == "uilist") {
            tax_req->SetReport(CEFetch_Taxonomy_Request::eReport_uilist);
        }
        else if (report == "brief") {
            tax_req->SetReport(CEFetch_Taxonomy_Request::eReport_brief);
        }
        else if (report == "docsum") {
            tax_req->SetReport(CEFetch_Taxonomy_Request::eReport_docsum);
        }
        else if (report == "xml") {
            tax_req->SetReport(CEFetch_Taxonomy_Request::eReport_xml);
        }
        else {
            ERR_POST(Error << "Unsupported taxonomy report: " << report);
            return 0;
        }
    }
    return tax_req.release();
}


void CEUtilsApp::CallEFetch(const CArgs& args)
{
    // Try to create literature request
    auto_ptr<CEFetch_Request> req(x_CreateLitRequest(args));
    if ( !req.get() ) {
        // Try to create sequence request
        req.reset(x_CreateSeqRequest(args));
    }
    if ( !req.get() ) {
        // Try to create taxonomy request
        req.reset(x_CreateTaxRequest(args));
    }
    if ( !req.get() ) {
        // the database is not related to any known request type
        ERR_POST(Error << "Can not connect to database "
            << args["db"].AsString() << " using the specified arguments.");
        return;
    }
    x_SetHttpMethod(*req, args);

    // If WebEnv was set (as an command line argument or by previous requests)
    // use it instead of id.
    if ( req->GetConnContext()->GetWebEnv().empty()  &&  args["id"] ) {
        req->GetId().SetIds(args["id"].AsString());
    }

    string retmode_str = args["retmode"].AsString();
    CEFetch_Request::ERetMode retmode = CEFetch_Request::eRetMode_none;
    if (retmode_str == "text") {
        retmode = CEFetch_Request::eRetMode_text;
    }
    else if (retmode_str == "xml") {
        retmode = CEFetch_Request::eRetMode_xml;
    }
    else if (retmode_str == "html") {
        retmode = CEFetch_Request::eRetMode_html;
    }
    else if (retmode_str == "asn") {
        retmode = CEFetch_Request::eRetMode_asn;
    }
    else {
        ERR_POST(Error << "Unknown retmode: " << retmode_str);
    }
    req->SetRetMode(retmode);

    cout << req->GetScriptName() << "?" << req->GetQueryString() << endl;

    if ( m_DumpRequests ) {
        x_DumpRequest(*req);
        return;
    }

    // efetch can return different object types, just print plain content
    string content;
    req->Read(&content);
    cout << content << endl;
}


int CEUtilsApp::Run(void)
{
    // Process command line args
    const CArgs& args = GetArgs();

    m_DumpRequests = args["dump"];

    // Set connection context parameters
    if (args["webenv"]) {
        x_GetCtx()->SetWebEnv(args["webenv"].AsString());
    }
    if (args["query_key"]) {
        x_GetCtx()->SetQueryKey(args["query_key"].AsString());
    }
    if (args["tool"]) {
        x_GetCtx()->SetTool(args["tool"].AsString());
    }
    if (args["email"]) {
        x_GetCtx()->SetEmail(args["email"].AsString());
    }

    // Call the requested utils
    if ( args["einfo"] ) {
        CallEInfo(args);
    }
    if ( args["egquery"] ) {
        CallEGQuery(args);
    }
    if ( args["espell"] ) {
        CallESpell(args);
    }
    if ( args["esearch"] ) {
        CallESearch(args);
    }
    if ( args["epost"] ) {
        CallEPost(args);
    }
    if ( args["elink"] ) {
        CallELink(args);
    }
    if ( args["esummary"] ) {
        CallESummary(args);
    }
    if ( args["efetch"] ) {
        CallEFetch(args);
    }
    // EHistory is the last one - shows all other requests if any
    if ( args["ehistory"] ) {
        CallEHistory(args);
    }
    return 0;
}


void CEUtilsApp::Exit(void)
{
}


END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CEUtilsApp().AppMain(argc, argv);
}
