/* ===========================================================================
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
 *  Author: Tim Boemker
 *
 *  Demonstration of ASN interface to BLAST.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_system.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <objects/blast/blast__.hpp>
#include <objects/scoremat/scoremat__.hpp>
#include <objects/blast/blastclient.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/gbloader.hpp>
#include <objmgr/bioseq_handle.hpp>

using namespace ncbi;
using namespace objects;

//  ==========================================================================
//
//  write prints requests and replies.
//
//  ==========================================================================

template<class T>
void write(ostream& os, CRef<T> t)
{
    auto_ptr<CObjectOStream> x(
        CObjectOStream::Open(eSerial_AsnText, os));

    *x << *t;
}

template<class T>
void write(ostream& os, const list<CRef<T> >& t)
{
    auto_ptr<CObjectOStream> x(
        CObjectOStream::Open(eSerial_AsnText, os));

    for(list<CRef<T> >::const_iterator i = t.begin(); i != t.end(); ++i)
        *x << **i;
}

//  ==========================================================================
//
//  submit a request & return the reply
//
//  ==========================================================================

static CRef<CBlast4_reply>
submit(CRef<CBlast4_request_body> body, bool echo = true)
{
    // create a request and, if echoing is enabled, print it to stdout
    // to show what a request object looks like.

    CRef<CBlast4_request> request(new CBlast4_request);
    request->SetBody(*body);
    if(echo)
        write(cout, request);

    // submit the request to the server & get the reply.  if echoing is
    // enabled, print the reply to stdout to show what a reply object
    // looks like.

    CRef<CBlast4_reply> reply(new CBlast4_reply);
    CBlast4Client().Ask(*request, *reply);
    if(echo)
        write(cout, reply);

    return reply;
}

//  ==========================================================================
//
//  sample "get-databases" command
//
//  ==========================================================================

static void
get_databases()
{
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_databases();

    submit(body);
}

//  ==========================================================================
//
//  sample "get-matrices" command
//
//  ==========================================================================

static void
get_matrices()
{
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_matrices();

    submit(body);
}

//  ==========================================================================
//
//  sample "get-parameters" command
//
//  ==========================================================================

static void
get_parameters()
{
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_parameters();

    submit(body);
}

//  ==========================================================================
//
//  sample "get-paramsets" command
//
//  ==========================================================================

static void
get_paramsets()
{
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_paramsets();

    submit(body);
}

//  ==========================================================================
//
//  sample "get-programs" command
//
//  ==========================================================================

static void
get_programs()
{
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_programs();

    submit(body);
}

//  ==========================================================================
//
//  sample "get-search-results" command
//
//  ==========================================================================

static CRef<CBlast4_reply>
get_search_results(string id, bool echo = true)
{
    CRef<CBlast4_get_search_results_request> gsr(
        new CBlast4_get_search_results_request);
    gsr->SetRequest_id(id);

    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_search_results(*gsr);

    return submit(body, echo);
}

//  ==========================================================================
//
//  sample "get-sequences" command
//
//  ==========================================================================

static void
get_sequences()
{
    CRef<CSeq_id> id(new CSeq_id);
    id->SetGi(3091);

    CRef<CBlast4_database> db(new CBlast4_database);
    db->SetName("nr");
    db->SetType(eBlast4_residue_type_protein);

    CRef<CBlast4_get_sequences_request> gsr(new CBlast4_get_sequences_request);
    gsr->SetDatabase(*db);
    gsr->SetSeq_ids().push_back(id);

    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_sequences(*gsr);

    submit(CRef<CBlast4_request_body>(body));
}

//  ==========================================================================
//
//  sample "finish-params" and "queue-search" commands
//
//  ==========================================================================

static void
setp(list<CRef<CBlast4_parameter> >& l, string n, int x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetInteger(x);

    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);

    l.push_back(p);
}

static void
setp(list<CRef<CBlast4_parameter> >& l, string n, string x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetString(x);

    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);

    l.push_back(p);
}

static void
setp(list<CRef<CBlast4_parameter> >& l, string n, const char* x)
{
    setp(l, n, string(x));
}

static void
setp(list<CRef<CBlast4_parameter> >& l, string n,
    CRef<CBlast4_cutoff> x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetCutoff(*x);

    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);

    l.push_back(p);
}

static void
setp(list<CRef<CBlast4_parameter> >& l, string n,
     CRef<CScore_matrix_parameters> x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetMatrix(*x);

    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);

    l.push_back(p);
}

static void
finish_params()
{
    CRef<CBlast4_finish_params_request> q(new CBlast4_finish_params_request);
    q->SetProgram("blastn");
    q->SetService("plain");

    CRef<CBlast4_cutoff> cutoff(new CBlast4_cutoff);
    cutoff->SetE_value(10);
    
    CRef<CScore_matrix_parameters> smp(new CScore_matrix_parameters);

    CRef<CScore_matrix> matrix(new CScore_matrix);
    matrix->SetIs_protein(false);
    
    CRef<CObject_id> objid(new CObject_id);
    objid->SetId(42);
    
    matrix->SetIdentifier(*objid);
    matrix->SetComments().push_back("This is unknown DNA");
    matrix->SetComments().push_back("Das is nicht fur das dumkoffen.");
    matrix->SetNrows(10);
    matrix->SetNcolumns(20);
    matrix->SetRow_labels().push_back("To err is etc.");
    matrix->SetRow_labels().push_back("Row label");
    
    int i = 1;
    matrix->SetScores().push_back(i += 17);
    matrix->SetScores().push_back(i += 17);
    matrix->SetScores().push_back(i += 17);
    matrix->SetScores().push_back(i += 17);
    matrix->SetScores().push_back(i += 17);
    
    matrix->SetPosFreqs().push_back(1200);
    matrix->SetPosFreqs().push_back(1300);
    matrix->SetPosFreqs().push_back(1400);
    matrix->SetPosFreqs().push_back(1500);
    
    smp->SetMatrix(*matrix);
    
    list<CRef<CBlast4_parameter> > & l = q->SetParams();
    
    setp(l, "matrix", smp);
    
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetFinish_params(*q);

    submit(body);
}

static CRef<CBioseq>
get_seq(int gi)
{
    CObjectManager objmgr;
    CScope scope(objmgr);
    objmgr.RegisterDataLoader(*new CGBDataLoader, CObjectManager::eDefault);
    scope.AddDefaults();

    CRef<CSeq_id> id(new CSeq_id);
    id->SetGi(gi);

    CBioseq* s =
        const_cast<CBioseq*>(&scope.GetBioseqHandle(*id).GetBioseq());

    return CRef<CBioseq>(s);
}

static string
queue_search()
{
    CRef<CBioseq> seq(get_seq(3091));

    CRef<CSeq_entry> seqentry(new CSeq_entry);
    seqentry->SetSeq(*seq);

    CRef<CBioseq_set> seqset(new CBioseq_set);
    seqset->SetSeq_set().push_back(seqentry);

    CRef<CBlast4_subject> subject(new CBlast4_subject);
    subject->SetDatabase("nr");

    CRef<CBlast4_queue_search_request> q(new CBlast4_queue_search_request);
    q->SetProgram("blastp");
    q->SetService("plain");
    q->SetQueries(*seqset);
    q->SetSubject(*subject);

    CRef<CBlast4_cutoff> cutoff(new CBlast4_cutoff);
    cutoff->SetE_value(2e4);

    list<CRef<CBlast4_parameter> >& l = q->SetParams();
    setp(l, "cutoff", cutoff);
    setp(l, "filter", "L;");
    setp(l, "gap-open", 9);
    setp(l, "gap-extend", 1);
    setp(l, "word-size", 2);
    setp(l, "matrix", "PAM30");

    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetQueue_search(*q);

    CRef<CBlast4_reply> reply = submit(body);
    return reply->GetBody().GetQueue_search().GetRequest_id();
}

static bool
search_pending(CRef<CBlast4_reply> reply)
{
    const list<CRef<CBlast4_error> >& errors = reply->GetErrors();
    for(list<CRef<CBlast4_error> >::const_iterator i = errors.begin();
            i != errors.end(); ++i)
        if((*i)->GetCode() == eBlast4_error_code_search_pending)
            return true;
    return false;
}


static void
queue_search_and_poll_for_results()
{
    string request_id = queue_search();

    // the request is virtually certain not to be ready yet; we check
    // now because we want to show the reply that indicates that the
    // search is still pending.

    get_search_results(request_id, true);

    // NCBI requests that clients wait 30 seconds before the first check
    // and between subsequent checks.

    bool ready = false;
    while(!ready) {
        SleepSec(30);
        ready = !search_pending(get_search_results(request_id, false));
    }

    // since the initial get_search_results is virtually certain not
    // to have succeeded, and since the subsequent get_search_results
    // calls had echoing suppressed, we can be pretty sure that we
    // have not echoed the search results.  make the call once more
    // just for the echo.  (a real client would not do this.)

    get_search_results(request_id, true);
}

//  ==========================================================================
//
//  CBlastcliApplication
//
//  ==========================================================================


class CBlastcliApplication : public CNcbiApplication {
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
    string usage;
};


void
CBlastcliApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "ASN.1 interface to BLAST 1.0 demonstration client");

    arg_desc->AddExtra(1, 1,
        "get-databases | get-matrices | get-sequences |\n"
        "queue-search | get-search-results RID",
        CArgDescriptions::eString);

    arg_desc->PrintUsage(usage);

    SetupArgDescriptions(arg_desc.release());
}

int
CBlastcliApplication::Run(void)
{
    CArgs args = GetArgs();
    string a = args[1].AsString();
    if(a == "finish-params")
        finish_params();
    else if(a == "get-databases")
        get_databases();
    else if(a == "get-matrices")
        get_matrices();
    else if(a == "get-parameters")
        get_parameters();
    else if(a == "get-paramsets")
        get_paramsets();
    else if(a == "get-programs")
        get_programs();
    else if(a == "get-sequences")
        get_sequences();
    else if(a == "queue-search")
        queue_search_and_poll_for_results();
    else if(a == "get-search-results")
        get_search_results(args[2].AsString());
    else {
        cerr << usage << endl;
        exit(-1);
    }
    return 0;
}

void
CBlastcliApplication::Exit()
{
    SetDiagStream(0);
}

int
main(int argc, const char* argv[])
{
    return CBlastcliApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
