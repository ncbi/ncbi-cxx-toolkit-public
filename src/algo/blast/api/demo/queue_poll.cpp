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
 */

/** @file queue_poll.cpp
 * Queueing and Polling code for remote_blast.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>

// CRemoteBlast
#include <algo/blast/api/remote_blast.hpp>

// Local
#include "queue_poll.hpp"

// Corelib
#include <corelib/ncbi_system.hpp>

// Objects
#include <objects/blast/Blast4_subject.hpp>
#include <objects/blast/Blast4_queue_search_reques.hpp>
#include <objects/blast/Blast4_parameter.hpp>
#include <objects/blast/Blast4_parameters.hpp>
#include <objects/blast/Blast4_value.hpp>
#include <objects/blast/blastclient.hpp>
#include <objects/blast/Blast4_queue_search_reply.hpp>
#include <objects/blast/Blas_get_searc_resul_reque.hpp>
#include <objects/blast/Blas_get_searc_resul_reply.hpp>
#include <objects/blast/Blast4_error.hpp>
#include <objects/blast/Blast4_error_code.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

// Object Manager
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

// Objtools
#include <objtools/readers/fasta.hpp>

// Use _exit() if available.
#if defined(NCBI_OS_UNIX)
#include <unistd.h>
#endif

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

/// A list of CBlast4_errors.
typedef list< CRef<CBlast4_error> > TErrorList;


//--------------------------------------------------------------------
//  Helper Functions
//--------------------------------------------------------------------

/// Is the query protein?
static inline bool
s_QueryIsAmino(const string & program)
{
    // Should the FASTA be NUC or PROT data?
        
    return (program == "blastp")  ||  (program == "tblastn");
}

/// Push a key-cutoff pair onto a list of parameters.
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

/// Push a key-string pair onto a list of parameters.
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

/// Push a key-integer pair onto a list of parameters.
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

/// Push a key-boolean pair onto a list of parameters.
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

/// Push a key-double pair onto a list of parameters.
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

/// Push the name-object pair onto params.
template <class T1, class T2, class T3>
void
s_SetpOpt(T1 & params, T2 & name, T3 & object)
{
    if (object.Exists()) {
        s_Setp(params, name, object.GetValue());
    }
}

/// Dump t to os as text ASN.1.
template <class T>
void
s_Output(CNcbiOstream & os, CRef<T> t)
{
    auto_ptr<CObjectOStream> x(CObjectOStream::Open(eSerial_AsnText, os));
    *x << *t;
    os.flush();
}


//--------------------------------------------------------------------
//  Queueing and Polling
//--------------------------------------------------------------------


/// Read a fasta query from a CNcbiIstream and return a CBioseq_set.
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

/// Set options appropriately for local and remote searches.
#ifndef SKIP_DOXYGEN_PROCESSING
class CSearchParamBuilder : public COptionWalker
{
public:
    template <class T>

    void Local(T &,
               CUserOpt,
               CArgKey,
               COptDesc)
    { }
    
    template <class ValueT, class MethodT, class OptsT>
    void Same(ValueT   & valobj,
               CUserOpt,
               MethodT    param,
               CArgKey,
               COptDesc,
               OptsT    & cboh)
    {
        if (valobj.Exists()) {
            param.Set(cboh, valobj.GetValue());
        }
    }
    
    template <class ValueT, class MethodT, class OptsT>
    void Remote(ValueT & valobj,
                MethodT param,
                OptsT & cboh)
    {
        if (valobj.Exists()) {
            param.Set(cboh, valobj.GetValue());
        }
    }
    
    bool NeedRemote(void) { return true; }
};
#endif

/// enumerates classes of defaults
enum EDefaultsFlag {
    eNone = 0,
    eMegablastDefaults,
    eBlastnDefaults
};

/**
 * catch-all no-op method.
 * @todo dead code?
 */

template<class T>
void SetAdditionalDefaults(T &, EDefaultsFlag)
{
}

/// Apply appropriate default nucleotide options.
template<>
void SetAdditionalDefaults<CBlastNucleotideOptionsHandle>
    (CBlastNucleotideOptionsHandle & obj, EDefaultsFlag flag)
{
    switch(flag) {
    case eMegablastDefaults:
        obj.SetTraditionalMegablastDefaults();
        break;
        
    case eBlastnDefaults:
        obj.SetTraditionalBlastnDefaults();
        break;
        
    case eNone:
        break;
    }
}

/// Apply the supplied options to the CRemoteBlast object. 
template<class T> void
s_SetSearchParams(CRef<CRemoteBlast>  & cb4o,
                  CNetblastSearchOpts & opts,
                  EDefaultsFlag         flag)
{
    CRef<T> cboh;
    
    cboh.Reset(new T(CBlastOptions::eRemote));
    
    if (flag != eNone) {
        SetAdditionalDefaults(*cboh, flag);
    }
    
    cb4o.Reset(new CRemoteBlast(cboh));
    
    CSearchParamBuilder spb;
    
    opts.Apply(spb, cboh, cb4o);
}

/// Instantiate appropriate options handle based on program and service.
static CRef<CRemoteBlast>
s_SetBlast4Params(string              & program,
                  string              & service,
                  string              & database,
                  CNetblastSearchOpts & opts,
                  CRef<CBioseq_set>     query,
                  string              & err)
{
    CRef<CRemoteBlast> cb4o;
    
    // NOTE: If adding new types here, add them also to the code in
    // search_opts.hpp that handles argument setting, i.e. the
    // OPT_HANDLER_SUPPORT_ALL() macro.
    
    EDefaultsFlag flag = eNone;
    
    if (program == "blastp" && service == "plain") {
        s_SetSearchParams<CBlastProteinOptionsHandle>(cb4o, opts, flag);
    }
    else if (program  == "blastn" &&
             (service == "plain"  || service == "megablast")) {
        
        if (service == "plain") {
            flag = eBlastnDefaults;
        }
        else if (service == "megablast") {
            flag = eMegablastDefaults;
        }
        
        s_SetSearchParams<CBlastNucleotideOptionsHandle>(cb4o, opts, flag);
    }
    else if (program == "tblastn" && service == "plain") {
        s_SetSearchParams<CTBlastnOptionsHandle>(cb4o, opts, flag);
    }
    else if (program == "tblastx" && service == "plain") {
        s_SetSearchParams<CTBlastxOptionsHandle>(cb4o, opts, flag);
    }
    else if (program == "blastx" && service == "plain") {
        s_SetSearchParams<CBlastxOptionsHandle>(cb4o, opts, flag);
    }
    else if (program == "blastx" && service == "plain") {
        s_SetSearchParams<CBlastxOptionsHandle>(cb4o, opts, flag);
    }
    
    if (cb4o.Empty()) {
        err +="Combination of program [";
        err += program;
        err += "] and service [";
        err += service;
        err += "] is not supported.\n";
    } else {
        cb4o->SetDatabase(database);
        
        // Should the following adjustment / workaround be in...
        // ... CRemoteBlast ?
        // ... blast4 server?
        // ... right here?
        
        if (query->GetSeq_set().front()->IsSeq()) {
            cb4o->SetQueries(query);
        } else {
            const CBioseq_set * myset = & query->GetSeq_set().front()->GetSet();
            CBioseq_set * myset2 = (CBioseq_set *) myset;
            
            cb4o->SetQueries(CRef<CBioseq_set>(myset2));
        }
        
        cb4o->SetQueries (query);
    }
    
    return cb4o;
}

/// Enqueue a search request and return a CRemoteBlast object.
static CRef<CRemoteBlast>
s_QueueSearch(string              & program,
              string              & service,
              string              & database,
              CNetblastSearchOpts & opts,
              CRef<CBioseq_set>     query,
              string              & err)
{
    CRef<CRemoteBlast> cb4o
        = s_SetBlast4Params(program,
                            service,
                            database,
                            opts,
                            query,
                            err);
    
    if (cb4o.NotEmpty()) {
        string cb4err(cb4o->GetErrors());
        
        if (cb4err.size()) {
            err += cb4err + "\n";
        }
    }
    
    return cb4o;
}

/// Display an alignment with CDisplaySeqalign.
static void
s_ShowAlign(CNcbiOstream         & os,
            CRef<CRemoteBlast>   cb4o,
            CRef<CScope>           scope,
            CAlignParms          & alparms,
            bool                   /*gapped*/)
{
    CRef<CSeq_align_set> alignments = cb4o->GetAlignments();
    
    if (alignments.Empty()) {
        os << "This search did not find any matches.\n";
        return;
    }
   
    AutoPtr<CDisplaySeqalign> dsa_ptr;
    
    // if (async_mode || (! gapped)) {
    
    // 1. The "prepare" function needs to be called in ungapped mode.
    // 2. It's safe to call this even if it is not needed (i.e. gapped mode).
    // 3. If not needed, it takes almost no time compared to the actual display,
    //    the ratio seems to be about 80,000 to 1.
    // 4. We can't tell if we are in gapped mode in the async case.
    // 5. So, we always call the prepare function.
    
    CRef<CSeq_align_set> newalign =
        CDisplaySeqalign::PrepareBlastUngappedSeqalign(*alignments);
    
    dsa_ptr = new CDisplaySeqalign(*newalign, * scope);
    
    // } else {
    //     dsa_ptr = new CDisplaySeqalign(*alignments, none1, none2, 0, * scope);
    // }
    
    alparms.AdjustDisplay(*dsa_ptr);
    
    dsa_ptr->DisplaySeqalign(os);
}

/// Show search status or display results.
void ShowResults(CRef<CRemoteBlast>  cb4o,
                 CRef<CScope>          scope,
                 CAlignParms         & alparms,
                 bool                  async_mode,
                 bool                  raw_asn)
{
    bool display = false;
    
    if (async_mode) {
        // Note that in ASYNC mode, we ONLY report status.  The user
        // can rerun without -async_mode to get results.  This is to
        // facilitate script writers (...is this a good decision?)
        
        bool   rid_done  = cb4o->CheckDone();
        string err       = cb4o->GetErrors();
        
        if (! err.empty()) {
            cout << "STATUS [error:" << err << "] RID [" << cb4o->GetRID() << "]" << endl;
        } else {
            string status = (rid_done ? "done" : "pending");
            cout << "STATUS [" << status << "] RID [" << cb4o->GetRID() << "]" << endl;
        }
    } else {
        cb4o->SubmitSync();
        string err = cb4o->GetErrors();
        
        if (! err.empty()) {
            cout << "STATUS [error:" << err << "] RID [" << cb4o->GetRID() << "]" << endl;
        } else {
            display = true;
        }
    }
    
    if (display) {
        bool gapped = true;
        
        // if (opts.Gapped().Exists()) {
        //     gapped = opts.Gapped().GetValue();
        // }
        
        if (raw_asn) {
            s_Output(NcbiCout, cb4o->GetAlignments());
        } else {
            s_ShowAlign(NcbiCout, cb4o, scope, alparms, gapped);
        }
    }
}

/// Submit a search and poll for results.
Int4
QueueAndPoll(string                program,       ///< program name
             string                service,       ///< service name
             string                database,      ///< database name
             CNetblastSearchOpts & opts,          ///< search options
             CNcbiIstream        & query_in,      ///< where to read query from
             bool                  verbose,       ///< verbose mode?
             bool                  trust_defline, ///< believe the query defline?
             bool                  raw_asn,       ///< emit raw asn when formatting?
             CAlignParms         & alparms,       ///< parameters for CDisplaySeqalign
             bool                  async_mode,    ///< asynchronous submission?
             string                get_RID)
{
    Int4 err_ret = 0;
        
    // Read the FASTA input data
    string fasta_line1;
    string fasta_block;
        
    // Queue and poll
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();

    CRef<CScope>         scope (new CScope(*objmgr));
    scope->AddDefaults();
        
    string err;
    
    CRef<CRemoteBlast> cb4o;
    
    CRef<CBioseq_set> cbss;
    
    bool amino = s_QueryIsAmino(program);
    
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
    
    if (get_RID.empty()) {
        cb4o = s_QueueSearch(program,
                             service,
                             database,
                             opts,
                             cbss,
                             err);
    
        if (cb4o.Empty() && err.empty()) {
            err = "Internal: Did not get search object from s_QueueSearch()\n";
        }
        
        if (err.empty() && verbose) {
            cb4o->SetVerbose(CRemoteBlast::eDebug);
        }
        
        if (err.empty()) {
            if (async_mode) {
                cb4o->Submit();
            } else {
                cb4o->SubmitSync();
            }
        } else {
            err_ret = 1;
        }
    } else {
        cb4o.Reset( new CRemoteBlast(get_RID) );
    }
    
    if (! err.empty()) {
        cerr << err << endl;
    } else {
        ShowResults(cb4o, scope, alparms, async_mode, raw_asn);
    }
    
    return err_ret;
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.15  2005/02/14 19:06:26  jianye
 * modification due to constructor change in showalign.cpp
 *
 * Revision 1.14  2004/11/02 17:53:02  camacho
 * Add SKIP_DOXYGEN_PROCESSING to rcsid string
 *
 * Revision 1.13  2004/11/01 18:25:15  coulouri
 * doxygen fixes
 *
 * Revision 1.12  2004/08/25 17:13:49  bealer
 * - Fix solaris warnings.
 *
 * Revision 1.11  2004/08/24 21:58:58  bealer
 * - Special casing for megablast vs. blastn.
 *
 * Revision 1.10  2004/08/18 15:58:43  bealer
 * - Prevent warning message.
 *
 * Revision 1.9  2004/08/02 20:09:25  bealer
 * - Remove Unix dependencies.
 *
 * Revision 1.8  2004/08/02 14:58:05  bealer
 * - Minor spelling glitch.
 *
 * Revision 1.7  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.6  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.5  2004/05/19 14:52:02  camacho
 * 1. Added doxygen tags to enable doxygen processing of algo/blast/core
 * 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
 *    location
 * 3. Added use of @todo doxygen keyword
 *
 * Revision 1.4  2004/04/19 14:37:52  bealer
 * - Fix compiler warnings.
 *
 * Revision 1.3  2004/03/22 20:46:21  bealer
 * - Fix non-literate comments to look less like doxygen comments.
 *
 * Revision 1.2  2004/02/18 20:28:18  bealer
 * - Always call prepare function.
 * - Set verbosity flag earlier.
 *
 * Revision 1.1  2004/02/18 17:04:41  bealer
 * - Adapt blast_client code for Remote Blast API, merging code into the
 *   remote_blast demo application.
 *
 * ===========================================================================
 */

