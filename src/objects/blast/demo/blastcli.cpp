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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <objects/blast/blast__.hpp>
#include <objects/blast/client.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/gbloader.hpp>
#include <objects/objmgr/bioseq_handle.hpp>

using namespace ncbi;
using namespace objects;

//	==========================================================================
//
//	write prints requests and replies.
//
//	==========================================================================

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

//	==========================================================================
//
//	sample "get-databases" command
//
//	==========================================================================

static CRef<CBlast4_request_body>
get_databases()
{
	CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_databases();

	return body;
}

//	==========================================================================
//
//	sample "get-matrices" command
//
//	==========================================================================

static CRef<CBlast4_request_body>
get_matrices()
{
	CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_matrices();

	return body;
}

//	==========================================================================
//
//	sample "get-search-results" command
//
//	==========================================================================

static CRef<CBlast4_request_body>
get_search_results(string id)
{
	CRef<CBlast4_get_search_results_request> gsr(
		new CBlast4_get_search_results_request);
	gsr->SetRequest_id(id);

	CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_search_results(*gsr);

	return body;
}

//	==========================================================================
//
//	sample "get-sequences" command
//
//	==========================================================================

static CRef<CBlast4_request_body>
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

	return body;
}

//	==========================================================================
//
//	sample "finish-params" and "queue-search" commands
//
//	==========================================================================

static void
setp(list<CRef<CBlast4_parameter> >& l, string n, bool x)
{
	CRef<CBlast4_value> v(new CBlast4_value);
	v->SetBoolean(x);

	CRef<CBlast4_parameter> p(new CBlast4_parameter);
	p->SetName(n);
	p->SetValue(*v);

	l.push_back(p);
}

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
setp(list<CRef<CBlast4_parameter> >& l, string n, double x)
{
	CRef<CBlast4_value> v(new CBlast4_value);
	v->SetReal(x);

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
	CRef<CBlast4_matrix> x)
{
	CRef<CBlast4_value> v(new CBlast4_value);
	v->SetMatrix(*x);

	CRef<CBlast4_parameter> p(new CBlast4_parameter);
	p->SetName(n);
	p->SetValue(*v);

	l.push_back(p);
}

static void
setp(list<CRef<CBlast4_parameter> >& l, string n,
	EBlast4_strand_type x)
{
	CRef<CBlast4_value> v(new CBlast4_value);
	v->SetStrand_type(x);

	CRef<CBlast4_parameter> p(new CBlast4_parameter);
	p->SetName(n);
	p->SetValue(*v);

	l.push_back(p);
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

static CRef<CBlast4_request_body>
finish_params()
{
	CRef<CBlast4_finish_params_request> q(new CBlast4_finish_params_request);
	q->SetProgram(eBlast4_program_blastn);
	q->SetService(eBlast4_service_plain);

    CRef<CBlast4_cutoff> cutoff(new CBlast4_cutoff);
    cutoff->SetE_value(10);

	list<CRef<CBlast4_parameter> >& l = q->SetParams();

	CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetFinish_params(*q);

	return body;
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

	return const_cast<CBioseq*>(&scope.GetBioseqHandle(*id).GetBioseq());
}

static CRef<CBlast4_request_body>
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
	q->SetProgram(eBlast4_program_blastp);
	q->SetService(eBlast4_service_plain);
    q->SetQueries(*seqset);
	q->SetSubject(*subject);

/*
    CRef<CBlast4_cutoff> cutoff(new CBlast4_cutoff);
    cutoff->SetE_value(2e4);

	list<CRef<CBlast4_parameter> >& l = q->SetParams();
    setp(l, "cutoff", cutoff);
    setp(l, "filter", "L;");
    setp(l, "gap-open", 9);
    setp(l, "gap-extend", 1);
    setp(l, "word-size", 2);
	setp(l, "matrix", "PAM30");
*/

	CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetQueue_search(*q);

	return body;
}

//	==========================================================================
//
//	CBlastcliApplication
//
//	==========================================================================


class CBlastcliApplication : public CNcbiApplication {
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
	CRef<CBlast4_request_body> MakeRequestBody();
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
	CRef<CBlast4_request_body> body(MakeRequestBody());
	CRef<CBlast4_request> request4(new CBlast4_request);
	request4->SetBody(*body);
	write(cout, request4);
	CRef<CBlast4_reply> reply4(new CBlast4_reply);
	CBlastClient().Ask(*request4, *reply4);
	write(cout, reply4);
	return 0;
}

CRef<CBlast4_request_body>
CBlastcliApplication::MakeRequestBody()
{
    CArgs args = GetArgs();
	string a = args[1].AsString();
	if(a == "finish-params")
		return finish_params();
	else if(a == "get-databases")
		return get_databases();
	else if(a == "get-matrices")
		return get_matrices();
	else if(a == "get-sequences")
		return get_sequences();
	else if(a == "queue-search")
		return queue_search();
	else if(a == "get-search-results")
		return get_search_results(args[2].AsString());
	else {
		cerr << usage << endl;
		exit(-1);
	}
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
