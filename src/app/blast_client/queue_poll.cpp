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
 * Author:  Kevin Bealer
 *
 * File Description:
 *   Queueing and Polling code for blast_client.
 *
 */

// Local
#include <ncbi_pch.hpp>
#include "queue_poll.hpp"

// Corelib
#include <corelib/ncbi_system.hpp>

// Objects
#include <objects/blast/blast__.hpp>
#include <objects/blast/blastclient.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

// Object Manager
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

// Objtools
#include <objtools/readers/fasta.hpp>

// Use _exit() if available.
#if defined(NCBI_OS_UNIX)
#include <unistd.h>
#endif

USING_NCBI_SCOPE;

using namespace ncbi::objects;

typedef list< CRef<CBlast4_error> > TErrorList;


/////////////////////////////////////////////////////////////////////////////
//
//  Helper Functions
//

#define BLAST4_POLL_DELAY_SEC 15
#define BLAST4_IGNORE_ERRS    5

static inline bool
s_IsAmino(const string & program)
{
    // Should the FASTA be NUC or PROT data?
        
    return (program == "blastp")  ||  (program == "tblastn");
}

void
s_Setp(list<CRef<CBlast4_parameter> >& l, string n, CRef<CBlast4_cutoff> x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetCutoff(*x);

    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);

    l.push_back(p);
}

void
s_Setp(list<CRef<CBlast4_parameter> >& l, string n, const string x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetString(x);

    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);

    l.push_back(p);
}

void
s_Setp(list<CRef<CBlast4_parameter> >& l, string n, const int & x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetInteger(x);
    
    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);
    
    l.push_back(p);
}

void
s_Setp(list<CRef<CBlast4_parameter> >& l, string n, const bool & x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetBoolean(x);
    
    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);
    
    l.push_back(p);
}

void
s_Setp(list<CRef<CBlast4_parameter> >& l, string n, const double & x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetReal(x);
    
    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);
    
    l.push_back(p);
}

template <class T1, class T2, class T3>
void
s_SetpOpt(T1 & params, T2 & name, T3 & object)
{
    if (object.Exists()) {
        s_Setp(params, name, object.GetValue());
    }
}

template <class T>
void
s_Output(CNcbiOstream & os, CRef<T> t)
{
    auto_ptr<CObjectOStream> x(CObjectOStream::Open(eSerial_AsnText, os));
    *x << *t;
    os.flush();
}


/////////////////////////////////////////////////////////////////////////////
//
//  Queueing and Polling
//


static CRef<CBioseq_set>
s_SetupQuery(CNcbiIstream    & query_in,
             CRef<CScope>      scope,
             TReadFastaFlags   fasta_flags)
{
    CRef<CSeq_entry> seqentry = ReadFasta(query_in, fasta_flags, 0, 0);
    
    scope->AddTopLevelSeqEntry(*seqentry);
    
    CRef<CBioseq_set> seqset(new CBioseq_set);
    seqset->SetSeq_set().push_back(seqentry);
    
    return seqset;
}

//#include <unistd.h>

class some_kind_of_nothing : public CEofException {
};

static CRef<CBlast4_reply>
s_Submit(CRef<CBlast4_request_body> body, bool echo = true)
{
    static int errors_ignored = 0;
    
    // Create the request; optionally echo it

    CRef<CBlast4_request> request(new CBlast4_request);
    request->SetBody(*body);
    
    if (echo) {
        s_Output(NcbiCout, request);
    }
    
    // submit to server, get reply; optionally echo it
    
    CRef<CBlast4_reply> reply(new CBlast4_reply);
    
    try {
        //throw some_kind_of_nothing();
        CBlast4Client().Ask(*request, *reply);
    }
    catch(const CEofException&) {
        if (errors_ignored >= BLAST4_IGNORE_ERRS) {
            ERR_POST(Error << "Unexpected EOF when contacting netblast server"
                     " - unable to complete request.");
#if defined(NCBI_OS_UNIX)
            // Use _exit() avoid coredump.
            _exit(-1);
#else
            exit(-1);
#endif
        } else {
            errors_ignored ++;
//             ERR_POST(Error << "Unexpected EOF when contacting netblast server"
//                      " ::: ignoring (" << errors_ignored << "/" << BLAST4_IGNORE_ERRS << ").");
            
            CRef<CBlast4_reply> empty_result;
            return empty_result;
        }
    }
    
    if (echo) {
        s_Output(NcbiCout, reply);
    }
    
    return reply;
}

class CSearchParamBuilder : public COptionWalker
{
public:
    CSearchParamBuilder(list< CRef<CBlast4_parameter> > & algo,
                        list< CRef<CBlast4_parameter> > & prog)
        : m_Algo(algo),
          m_Prog(prog)
    {}
    
    template <class T>
    void Same(T          & valobj,
              CUserOpt,
              CNetName     nb_name,
              CArgKey,
              COptDesc,
              EListPick    lp)
    {
        if (EListAlgo == lp) {
            s_SetpOpt(m_Algo, nb_name, valobj);
        } else if (EListProg == lp) {
            s_SetpOpt(m_Prog, nb_name, valobj);
        }
    }
    
    template <class T>
    void Local(T &,
               CUserOpt,
               CArgKey,
               COptDesc)
    { }
    
    template <class T> void Remote(T & valobj, CNetName net_name, EListPick lp)
    {
        if (EListAlgo == lp) {
            s_SetpOpt(m_Algo, net_name, valobj);
        } else if (EListProg == lp) {
            s_SetpOpt(m_Prog, net_name, valobj);
        }
    }
    
    bool NeedRemote(void) { return true; }
    
private:
    list< CRef<CBlast4_parameter> > & m_Algo;
    list< CRef<CBlast4_parameter> > & m_Prog;
};

static void
s_SetSearchParams(CNetblastSearchOpts             & opts,
                  list< CRef<CBlast4_parameter> > & algo,
                  list< CRef<CBlast4_parameter> > & prog)
{
    CSearchParamBuilder spb(algo, prog);
    
    opts.Apply(spb);
}

// Stolen from: CRemoteBlast::SetQueries(CRef<objects::CBioseq_set> bioseqs)

bool s_SetQueries(CRef<CBlast4_queue_search_request> qsr,
                  CRef<CBioseq_set>                  bioseqs)
{
    if (bioseqs.Empty()) {
        return false;
    }
    
    CRef<CBlast4_queries> queries_p(new CBlast4_queries);
    queries_p->SetBioseq_set(*bioseqs);
    
    qsr->SetQueries(*queries_p);
    
    return true;
}

static string
s_QueueSearch(string              & program,
              string              & database,
              string              & service,
              CNetblastSearchOpts & opts,
              CRef<CBioseq_set>     query,
              bool                  verbose,
              string              & err)
{
    string returned_rid;
    
    CRef<CBlast4_subject> subject(new CBlast4_subject);
    subject->SetDatabase(database);
    
    CRef<CBlast4_queue_search_request> qsr(new CBlast4_queue_search_request);
    qsr->SetProgram(program);
    qsr->SetService(service);
    
    qsr->SetSubject(*subject);
    
    if (query->GetSeq_set().front()->IsSeq()) {
        //qsr->SetQueries(*query);
        s_SetQueries(qsr, query);
    } else {
        CRef<CBioseq_set> myset(& query->SetSeq_set().front()->SetSet());
        s_SetQueries(qsr, myset);
    }
    
    list< CRef<CBlast4_parameter> > & algo = qsr->SetAlgorithm_options().Set();
    list< CRef<CBlast4_parameter> > & prog = qsr->SetProgram_options().Set();
    
    s_SetSearchParams(opts, algo, prog);
    
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetQueue_search(*qsr);
    
    CRef<CBlast4_reply> reply = s_Submit(body, verbose);
    
    if (reply.NotEmpty() &&
        reply->CanGetBody() &&
        reply->GetBody().GetQueue_search().CanGetRequest_id()) {
        
        returned_rid = reply->GetBody().GetQueue_search().GetRequest_id();
    }
    
    if (reply.NotEmpty() &&
        reply->CanGetErrors()) {
        const CBlast4_reply::TErrors & errs = reply->GetErrors();
        
        CBlast4_reply::TErrors::const_iterator i;
        
        for (i = errs.begin(); i != errs.end(); i++) {
            if ((*i)->CanGetMessage() &&
                (*i)->GetMessage().size()) {
                if (err.size()) {
                    err += "\n";
                }
                err += (*i)->GetMessage();
            }
        }
    }
    
    return returned_rid;
}

static CRef<CBlast4_reply>
s_GetSearchResults(const string & RID, bool echo = true)
{
    CRef<CBlast4_get_search_results_request>
        gsrr(new CBlast4_get_search_results_request);
    
    gsrr->SetRequest_id(RID);
        
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_search_results(*gsrr);
        
    return s_Submit(body, echo);
}

static bool
s_SearchPending(CRef<CBlast4_reply> reply)
{
    // The reply can be empty on certain types of errors.
    if (reply.Empty())
        return true;
    
    const list< CRef<CBlast4_error> > & errors = reply->GetErrors();
   
    TErrorList::const_iterator i;
    for(i = errors.begin(); i != errors.end(); i++) {
        if ((*i)->GetCode() == eBlast4_error_code_search_pending) {
            return true;
        }
    }
    return false;
}

static void
s_ShowAlign(CNcbiOstream       & os,
            CBlast4_reply_body & reply,
            CRef<CScope>         scope,
            CAlignParms        & alparms,
            bool                 gapped)
{
    CBlast4_get_search_results_reply & cgsrr(reply.SetGet_search_results());
    
    if (! cgsrr.CanGetAlignments()) {
        os << "This search did not find any matches.\n";
        return;
    }
    
    CSeq_align_set & alignment(cgsrr.SetAlignments());

    AutoPtr<CDisplaySeqalign> dsa_ptr;
    
    if (! gapped) {
        CRef<CSeq_align_set> newalign =
            CDisplaySeqalign::PrepareBlastUngappedSeqalign(alignment);
        
        dsa_ptr = new CDisplaySeqalign(*newalign,  * scope);
    } else {
        dsa_ptr = new CDisplaySeqalign(alignment,  * scope);
    }
    
    alparms.AdjustDisplay(*dsa_ptr);
    
    dsa_ptr->DisplaySeqalign(os);
}

static Int4
s_PollForResults(const string & RID,
                 bool           verbose,
                 bool           raw_asn,
                 CRef<CScope>   scope,
                 CAlignParms  & alparms,
                 bool           gapped)
{
    CRef<CBlast4_reply> r(s_GetSearchResults(RID, verbose));
    
    Int4 EOFtime    = 0;
    Int4 MaxEOFtime = 120;
    
    bool pending = s_SearchPending(r);
    
    while (pending  &&  (EOFtime < MaxEOFtime)) {
        SleepSec(BLAST4_POLL_DELAY_SEC);
        
        try {
            r = s_GetSearchResults(RID, verbose);
            pending = s_SearchPending(r);
        }
        catch(CEofException &) {
            EOFtime += BLAST4_POLL_DELAY_SEC;
        }
    }
    
    bool raw_output = false;
    
    if (raw_asn) {
        raw_output = true;
    }
    
    if (! (r->CanGetBody()  &&  r->GetBody().IsGet_search_results())) {
        raw_output = true;
    }
    
    if (raw_output) {
        s_Output(NcbiCout, r);
    } else {
        CBlast4_reply_body & repbody = r->SetBody();
        s_ShowAlign(NcbiCout, repbody, scope, alparms, gapped);
    }
    
    return 0;
}

Int4
QueueAndPoll(string                program,
             string                database,
             string                service,
             CNetblastSearchOpts & opts,
             CNcbiIstream        & query_in,
             bool                  verb,
             bool                  trust_defline,
             bool                  raw_asn,
             CAlignParms         & alparms)
{
    Int4 err_ret = 0;
        
    // Read the FASTA input data
    string fasta_line1;
    string fasta_block;
        
    // Queue and poll
    string RID;
        

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CScope>         scope (new CScope(*objmgr));

    CGBDataLoader::RegisterInObjectManager(*objmgr);    
    scope->AddDefaults();
        
    CRef<CBioseq_set> cbss;
    
    bool amino         = s_IsAmino(program);
    
    int flags = fReadFasta_AllSeqIds; // | fReadFasta_OneSeq;
    
    if (amino) {
        flags |= fReadFasta_AssumeProt;
    } else {
        flags |= fReadFasta_AssumeNuc;
    }
    
    if (! trust_defline) {
        flags |= fReadFasta_NoParseID;
    }
    
    cbss = s_SetupQuery(query_in, scope, flags);
    
    string err;
    
    RID = s_QueueSearch(program,
                        database,
                        service,
                        opts,
                        cbss,
                        verb,
                        err);
    
    if (RID.size()) {
        alparms.SetRID(RID);
        
        if (! err_ret) {
            bool gapped = true;
            
            if (opts.Gapped().Exists()) {
                gapped = opts.Gapped().GetValue();
            }
            
            err_ret =
                s_PollForResults(RID, verb, raw_asn, scope, alparms, gapped);
        }
    } else {
        ERR_POST(Error << "Could not queue request.");
        
        if (err.size()) {
            ERR_POST(Error << err);
        }
        
        err_ret = -1;
    }
    
    return err_ret;
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.12  2005/02/14 19:09:02  jianye
 * modification due to constructor change in showalign.cpp
 *
 * Revision 1.11  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.10  2004/06/07 14:17:01  camacho
 * Use anonymous exception to avoid warning
 *
 * Revision 1.9  2004/05/21 21:41:38  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.8  2004/05/05 18:20:39  bealer
 * - Update for new ASN.1
 *
 * Revision 1.7  2004/01/30 23:49:59  bealer
 * - Add better handling for results-not-found case.
 *
 * Revision 1.6  2004/01/05 17:59:31  vasilche
 * Moved genbank loader and its readers sources to new location in objtools.
 * Genbank is now in library libncbi_xloader_genbank.
 * Id1 reader is now in library libncbi_xreader_id1.
 * OBJMGR_LIBS macro updated correspondingly.
 *
 * Old headers temporarily will contain redirection to new location
 * for compatibility:
 * objmgr/gbloader.hpp > objtools/data_loaders/genbank/gbloader.hpp
 * objmgr/reader_id1.hpp > objtools/data_loaders/genbank/readers/id1/reader_id1.hpp
 *
 * Revision 1.5  2003/12/29 19:48:30  bealer
 * - Change code to accomodate first half of new ASN changes.
 *
 * Revision 1.4  2003/12/24 01:01:56  ucko
 * Bandaid to compile with current ASN.1 spec.  All parameters are
 * currently classified as algorithm options, and still need to be
 * divided up properly.
 *
 * Revision 1.3  2003/11/05 19:17:45  bealer
 * - Remove .data() from string argument passing.
 *
 * Revision 1.2  2003/09/26 20:01:43  bealer
 * - Fix Solaris compile errors.
 *
 * Revision 1.1  2003/09/26 16:53:49  bealer
 * - Add blast_client project for netblast protocol, initial code commit.
 *
 * ===========================================================================
 */
