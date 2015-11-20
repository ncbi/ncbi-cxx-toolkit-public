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
* Author:  Aaron M. Ucko
*
* File Description:
*   test of datatool-generated ID1 client class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <serial/objostrasn.hpp>

#include <objects/id1/Entry_complexities.hpp>
#include <objects/id1/ID1Seq_hist.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/id1/id1_client.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CTestID1ClientApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);

private:
    void RunCommand(const string& command);

    CRef<CID1Client>         m_Client;
    CNcbiOstream*            m_Out;
    auto_ptr<CObjectOStream> m_OutAsn;
};


void CTestID1ClientApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "simple ID1 client to test datatool changes");

    arg_desc->AddDefaultKey("in", "Filename",
                            "Read commands from <Filename> (default: stdin)",
                            CArgDescriptions::eInputFile, "-",
                            CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("out", "Filename",
                            "Write output to <Filename> (default: stdout)",
                            CArgDescriptions::eOutputFile, "-",
                            CArgDescriptions::fPreOpen);

    SetupArgDescriptions(arg_desc.release());
}


int CTestID1ClientApp::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    const CArgs&  args = GetArgs();
    CNcbiIstream& in   = args["in"].AsInputFile();
    m_Out    = &args["out"].AsOutputFile();
    m_OutAsn.reset(new CObjectOStreamAsn(*m_Out));
    m_Client = new CID1Client;
    string command;
    while ( !in.eof() ) {
        *m_Out << "-- " << flush;
        NcbiGetlineEOL(in, command);
        command = NStr::TruncateSpaces(command);
        if (command == "exit"  ||  command == "quit") {
            break;
        }
        if ( !command.empty() ) {
            try {
                RunCommand(command);
            } STD_CATCH_ALL("test_id1_client")
        }
    }
    *m_Out << endl;
    return 0;
}


void CTestID1ClientApp::RunCommand(const string& command)
{
    list<string> args;
    NStr::Split(command, " ", args,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    string verb = args.front();
    args.pop_front();

    if (verb == "help") {
        *m_Out << "commands:" << endl;
        *m_Out << "  connect" << endl;
        *m_Out << "  disconnect" << endl;
        *m_Out << "  quit, exit" << endl;
        *m_Out << "  reconnect" << endl;
        *m_Out << "  getgi <accession>" << endl;
        *m_Out << "  getgihist <gi number>" << endl;
        *m_Out << "  getgirev <gi number>" << endl;
        *m_Out << "  getgistate <gi number>" << endl;
        *m_Out << "  getsefromgi <gi number>" << endl;
        *m_Out << "  getseqidsfromgi <gi number>" << endl;
    } else if (verb == "connect") {
        m_Client->Connect();
    } else if (verb == "disconnect") {
        m_Client->Disconnect();
    } else if (verb == "reconnect") {
        m_Client->Reset();
    } else if (verb == "reset") {
        m_Client.Reset(new CID1Client);
    } else if (verb == "getgi") {
        CSeq_id id(args.front());
        *m_Out << m_Client->AskGetgi(id) << endl;
    } else if (verb == "getsefromgi") {
        CID1server_maxcomplex maxplex;
        maxplex.SetMaxplex(eEntry_complexities_entry);
        maxplex.SetGi(NStr::StringToNumeric<TGi>(args.front()));
        *m_OutAsn << *m_Client->AskGetsefromgi(maxplex);
        m_OutAsn->Flush();
    } else if (verb == "getseqidsfromgi") {
        TGi gi = NStr::StringToNumeric<TGi>(args.front());
        CID1server_back::TIds ids = m_Client->AskGetseqidsfromgi(gi);
        ITERATE (CID1server_back::TIds, id, ids) {
            *m_Out << (*id)->DumpAsFasta() << endl;
        }
    } else if (verb == "getgihist") {
        TGi gi = NStr::StringToNumeric<TGi>(args.front());
        typedef list< CRef<CID1Seq_hist> > THistory;
        THistory hist = m_Client->AskGetgihist(gi);
        ITERATE (THistory, iter, hist) {
            *m_OutAsn << **iter;
        }
        m_OutAsn->Flush();
    } else if (verb == "getgirev") {
        TGi gi = NStr::StringToNumeric<TGi>(args.front());
        typedef list< CRef<CID1Seq_hist> > THistory;
        THistory hist = m_Client->AskGetgirev(gi);
        ITERATE (THistory, iter, hist) {
            *m_OutAsn << **iter;
        }
        m_OutAsn->Flush();
    } else if (verb == "getgistate") {
        TGi gi = NStr::StringToNumeric<TGi>(args.front());
        *m_Out << m_Client->AskGetgistate(gi) << endl;
    } else {
        ERR_POST("Unrecognized command \"" << command << "\"");
    }
}


int main(int argc, const char* argv[])
{
    return CTestID1ClientApp().AppMain(argc, argv);
}
