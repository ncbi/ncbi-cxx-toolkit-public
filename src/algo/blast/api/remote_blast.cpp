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
* ===========================================================================
*/

/// @file remote_blast.cpp
/// Queueing and Polling code for Remote Blast API.

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <serial/iterator.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_options_builder.hpp>
#include <algo/blast/api/search_strategy.hpp>

#include <objects/blast/blastclient.hpp>
#include <objects/blast/blast__.hpp>
#include <objects/blast/names.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/blast/blastclient.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include "psiblast_aux_priv.hpp"    // For CPsiBlastValidate::Pssm()
#include <util/format_guess.hpp>    // for CFormatGuess
#include <serial/objistrxml.hpp>    // for CObjectIStreamXml
#include <algo/blast/api/objmgr_query_data.hpp>

#if defined(NCBI_OS_UNIX)
#include <unistd.h>
#endif

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)


// Static functions


/// Error value type used by Blast4 ASN.1 objects.
typedef list< CRef<objects::CBlast4_error> > TErrorList;


/// Determine whether the search is still running.
/// @param reply Reply from get-search-results request.
/// @return True if search needs more time, false if done or failed.
static bool
s_SearchPending(CRef<objects::CBlast4_reply> reply)
{
    const list< CRef<objects::CBlast4_error> > & errors = reply->GetErrors();
    
    TErrorList::const_iterator i;
    
    for(i = errors.begin(); i != errors.end(); i++) {
        if ((*i)->GetCode() == eBlast4_error_code_search_pending) {
            return true;
        }
    }
    return false;
}


void CRemoteBlast::x_SearchErrors(CRef<objects::CBlast4_reply> reply)
{
    const list< CRef<CBlast4_error> > & errors = reply->GetErrors();
    
    TErrorList::const_iterator i;
    
    for(i = errors.begin(); i != errors.end(); i++) {
        string msg;
        
        if ((*i)->CanGetMessage() && (! (*i)->GetMessage().empty())) {
            msg = ": ";
            msg += (*i)->GetMessage();
        }
        
        switch((*i)->GetCode()) {
        case eBlast4_error_code_conversion_warning:
            m_Warn.push_back(string("conversion_warning") + msg);
            break;
            
        case eBlast4_error_code_internal_error:
            m_Errs.push_back(string("internal_error") + msg);
            break;
            
        case eBlast4_error_code_not_implemented:
            m_Errs.push_back(string("not_implemented") + msg);
            break;
            
        case eBlast4_error_code_not_allowed:
            m_Errs.push_back(string("not_allowed") + msg);
            break;
            
        case eBlast4_error_code_bad_request:
            m_Errs.push_back(string("bad_request") + msg);
            break;
            
        case eBlast4_error_code_bad_request_id:
            m_Errs.push_back(string("Invalid/unknown RID (bad_request_id)") +
                             msg);
            break;
        }
    }
}



// CBlast4Option methods

void CRemoteBlast::x_CheckConfig(void)
{
    // If not configured, throw an exception - the associated string
    // will contain a list of the missing pieces.
    
    if (0 != m_NeedConfig) {
        string cfg("Configuration required:");
        
        if (eProgram & m_NeedConfig) {
            cfg += " <program>";
        }
        
        if (eService & m_NeedConfig) {
            cfg += " <service>";
        }
        
        if (eQueries & m_NeedConfig) {
            cfg += " <queries>";
        }
        
        if (eSubject & m_NeedConfig) {
            cfg += " <subject>";
        }
        
        NCBI_THROW(CRemoteBlastException, eIncompleteConfig, cfg);
    }
}

CRef<objects::CBlast4_request>
CRemoteBlast::GetSearchStrategy()
{
    CRef<CBlast4_request_body> body(x_GetBlast4SearchRequestBody());
    x_CheckConfig();
    string errors(GetErrors());
    if ( !errors.empty() ) {
        NCBI_THROW(CRemoteBlastException, eIncompleteConfig, errors);
    }
    CRef<CBlast4_request> retval(new CBlast4_request);
    if ( !m_ClientId.empty() ) {
        retval->SetIdent(m_ClientId);
    }
    retval->SetBody(*body);
    return retval;
}

CRef<objects::CBlast4_reply>
CRemoteBlast::x_SendRequest(CRef<objects::CBlast4_request_body> body)
{
    // If not configured, throw.
    x_CheckConfig();
    
    // Create the request; optionally echo it
    
    CRef<CBlast4_request> request(new CBlast4_request);
    if ( !m_ClientId.empty() ) {
        request->SetIdent(m_ClientId);
    }
    request->SetBody(*body);
    
    if (eDebug == m_Verbose) {
        NcbiCout << MSerial_AsnText << *request << endl;
    }
    
    // submit to server, get reply; optionally echo it
    
    CRef<CBlast4_reply> reply(new CBlast4_reply);
    
    try {
        CStopWatch sw(CStopWatch::eStart);
        
        if (eDebug == m_Verbose) {
            NcbiCout << "Starting network transaction (" << sw.Elapsed() << ")" << endl;
        }
        
        CBlast4Client().Ask(*request, *reply);
        
        if (eDebug == m_Verbose) {
            NcbiCout << "Done network transaction (" << sw.Elapsed() << ")" << endl;
        }
    }
    catch(const CEofException&) {
        NCBI_THROW(CRemoteBlastException, eServiceNotAvailable,
                   "No response from server, cannot complete request.");
    }
    
    if (eDebug == m_Verbose) {
        NcbiCout << MSerial_AsnText << *reply << endl;
    }
    
    return reply;
}

CRef<objects::CBlast4_reply>
CRemoteBlast::x_GetSearchResults(void)
{
    CRef<CBlast4_get_search_results_request>
        gsrr(new CBlast4_get_search_results_request);
    
    gsrr->SetRequest_id(m_RID);
    
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_search_results(*gsrr);
    
    return x_SendRequest(body);
}

// Pre:  start, wait, or done
// Post: failed or done

// Returns: true if done

bool CRemoteBlast::SubmitSync(int seconds)
{
    // eFailed: no work to do, already an error.
    // eDone:   already done, just return.
    
    EImmediacy immed = ePollAsync;
    
    switch(x_GetState()) {
    case eStart:
        x_SubmitSearch();
        if (! m_Errs.empty()) {
            break;
        }
        immed = ePollImmed;
        // fall through
        
    case eWait:
        x_PollUntilDone(immed, seconds);
        break;
    default:
        break;
    }
    
    return (x_GetState() == eDone);
}



// Pre:  start
// Post: failed, wait or done

// Returns: true if no error so far

bool CRemoteBlast::Submit(void)
{
    switch(x_GetState()) {
    case eStart:
        x_SubmitSearch();
    default: break;
    }
    
    return m_Errs.empty();
}

//
// The following table summarizes how to determine the status of a given
// RID/search submission:
//
//                           | CheckDone()   |  CheckDone()
//                           | returns true  |  returns false
// ------------------------------------------------------------
// GetErrors() == kEmptyStr  |    DONE       |   PENDING
// ------------------------------------------------------------
// GetErrors() != kEmptyStr  |    FAILED     |   UNKNOWN RID
// ------------------------------------------------------------
//
CRemoteBlast::ESearchStatus
CRemoteBlast::CheckStatus()
{
    ESearchStatus retval = eStatus_Unknown;

    bool done = CheckDone();
    string errors = GetErrors();

    if (done && errors == kEmptyStr) {
        retval = eStatus_Done;
    } else if (!done && errors == kEmptyStr) {
        retval = eStatus_Pending;
    } else if (!done && errors.find("bad_request_id") != NPOS) {
        retval = eStatus_Unknown;
    } else if (done && errors != kEmptyStr) {
        retval = eStatus_Failed;
    } 
    return retval;
}

bool CRemoteBlast::x_IsUnknownRID(void)
{
    bool retval = false;
    if (NStr::Find(GetErrors(), "bad_request_id") != NPOS) {
        retval = true;
    }
    return retval;
}

// Pre:  start, wait or done
// Post: wait, done, or failed

// Returns: true if done

bool CRemoteBlast::CheckDone(void)
{
    switch(x_GetState()) {
    case eFailed:
    case eDone:
        break;
        
    case eStart:
        Submit();
        break;
        
    case eWait:
        x_CheckResults();
    }
    
    int state = x_GetState();
    return (state == eDone || (state == eFailed && !x_IsUnknownRID()));
}

CRemoteBlast::TGSRR * CRemoteBlast::x_GetGSRR(void)
{
    TGSRR* rv = NULL;

    if (m_ReadFile)
    {
        rv = &(m_Archive->SetResults());
    } 
    else if (SubmitSync() &&
        m_Reply.NotEmpty() &&
        m_Reply->CanGetBody() &&
        m_Reply->GetBody().IsGet_search_results()) {
        
        rv = & (m_Reply->SetBody().SetGet_search_results());
    }

    return rv;
}

CRef<objects::CSeq_align_set> CRemoteBlast::GetAlignments(void)
{
    CRef<CSeq_align_set> rv;
    
    TGSRR * gsrr = x_GetGSRR();
    
    if (gsrr && gsrr->CanGetAlignments()) {
        rv = & (gsrr->SetAlignments());
    }
    
    return rv;
}

TSeqAlignVector CRemoteBlast::GetSeqAlignSets()
{
    CRef<CSeq_align_set> al = GetAlignments();
    
    TSeqAlignVector rv;
    
    if (al.Empty() || al->Get().empty()) {
        return rv;
    }
    
    CRef<CSeq_align_set> cur_set;
    CConstRef<CSeq_id> current_id;
    
    ITERATE(CSeq_align_set::Tdata, it, al->Get()) {
        // index 0 = query, index 1 = subject
        const int query_index = 0;
        CConstRef<CSeq_id> this_id( & (*it)->GetSeq_id(query_index) );
        
        if (current_id.Empty() || (CSeq_id::e_YES != this_id->Compare(*current_id))) {
            if (cur_set.NotEmpty()) {
                rv.push_back(cur_set);
            }
            cur_set.Reset(new CSeq_align_set);
            current_id = this_id;
        }
        
        cur_set->Set().push_back(*it);
    }
    
    if (cur_set.NotEmpty()) {
        rv.push_back(cur_set);
    }
    
    return rv;
}

CRef<objects::CBlast4_phi_alignments> CRemoteBlast::GetPhiAlignments(void)
{
    CRef<CBlast4_phi_alignments> rv;
    
    TGSRR * gsrr = x_GetGSRR();
    
    if (gsrr && gsrr->CanGetPhi_alignments()) {
        rv = & (gsrr->SetPhi_alignments());
    }
    
    return rv;
}

// N.B.: this function assumes that the BLAST 4 server sends the query masked
// locations for each query adjacent to one another in the list of masks (i.e.:
// masks-for-query1-frameA, masks-for-query1-frameB, ...,
// masks-for-query2-frameA, masks-for-query2-frameB, ... etc).
TSeqLocInfoVector
CRemoteBlast::GetMasks(void)
{
    TSeqLocInfoVector retval;
    retval.resize(GetQueries()->GetNumQueries());

    TGSRR::TMasks network_masks = x_GetMasks();
    if (network_masks.empty()) {
        return retval;
    }

    EBlastProgramType program = NetworkProgram2BlastProgramType(m_Program,
                                                                m_Service);
    CConstRef<CSeq_id> previous_seqid;
    size_t query_index = 0;

    ITERATE(TGSRR::TMasks, masks_for_frame, network_masks) {

        _ASSERT(masks_for_frame->NotEmpty());

        CConstRef<CSeq_id> current_seqid
            ((*masks_for_frame)->GetLocations().front()->GetId());
        if (previous_seqid.Empty()) {
            previous_seqid = current_seqid;
        }

        // determine which query are we setting the masks for...
        TMaskedQueryRegions* mqr = NULL;
        if (CSeq_id::e_YES == current_seqid->Compare(*previous_seqid)) {
            mqr = &retval[query_index];
        } else {
            mqr = &retval[++query_index];
            previous_seqid = current_seqid;
        }

        // all the masks for a given query and frame are in a single
        // Packed-seqint
        _ASSERT((*masks_for_frame)->GetLocations().size() == (size_t) 1);
        _ASSERT((*masks_for_frame)->GetLocations().front().NotEmpty());
        CRef<CSeq_loc> masks =
            (*masks_for_frame)->GetLocations().front();
        _ASSERT(masks->IsPacked_int());

        const CPacked_seqint& packed_int = masks->GetPacked_int();
        const EBlast4_frame_type frame = (*masks_for_frame)->GetFrame();
        ITERATE(CPacked_seqint::Tdata, mask, packed_int.Get()) {
            CRef<CSeq_interval> si
                (new CSeq_interval(const_cast<CSeq_id&>((*mask)->GetId()), 
                                   (*mask)->GetFrom(), (*mask)->GetTo()));
            CRef<CSeqLocInfo> sli
                (new CSeqLocInfo(si, NetworkFrame2FrameNumber(frame, program)));
            mqr->push_back(sli);
        }
    }

    _ASSERT(query_index == GetQueries()->GetNumQueries() - 1);

    return retval;
}

CRemoteBlast::TGSRR::TMasks CRemoteBlast::x_GetMasks(void)
{
    TGSRR::TMasks rv;
    
    TGSRR * gsrr = x_GetGSRR();
    
    if (gsrr && gsrr->CanGetMasks()) {
        rv = gsrr->SetMasks();
    }

    return rv;
}

list< CRef<objects::CBlast4_ka_block > > CRemoteBlast::GetKABlocks(void)
{ 
    list< CRef<CBlast4_ka_block > > rv;
        
    TGSRR * gsrr = x_GetGSRR();
    
    if (gsrr && gsrr->CanGetKa_blocks()) {
        rv = (gsrr->SetKa_blocks());
    }
    
    return rv;
}

list< string > CRemoteBlast::GetSearchStats(void)
{
    list< string > rv;
    
    TGSRR * gsrr = x_GetGSRR();
    
    if (gsrr && gsrr->CanGetSearch_stats()) {
        rv = (gsrr->SetSearch_stats());
    }
    
    return rv;
}

CRef<objects::CPssmWithParameters> CRemoteBlast::GetPSSM(void)
{
    CRef<CPssmWithParameters> rv;
    
    TGSRR * gsrr = x_GetGSRR();
    
    if (gsrr && gsrr->CanGetPssm()) {
        rv = & (gsrr->SetPssm());
    }
    
    return rv;
}


// Internal CRemoteBlast methods

CRemoteBlast::EState CRemoteBlast::x_GetState(void)
{
    // CBlast4Option states:
    
    // 0. start  (no rid, no errors)
    // 1. failed (errors)
    // 2. wait   (has rid, no errors, still pending)
    // 3. done   (has rid, no errors, not pending)
    
    EState rv = eDone;
    
    if (! m_Errs.empty()) {
        rv = eFailed;
    } else if (m_RID.empty()) {
        rv = eStart;
    } else if (m_Pending) {
        rv = eWait;
    }
    
    return rv;
}

CRef<objects::CBlast4_request_body>
CRemoteBlast::x_GetBlast4SearchRequestBody()
{
    CRef<CBlast4_request_body> retval;

    if (m_QSR.Empty()) {
        m_Errs.push_back("No request exists and no RID was specified.");
        return retval;
    }
    
    x_SetAlgoOpts();
    x_QueryMaskingLocationsToNetwork();
    
    retval.Reset(new CBlast4_request_body);
    retval->SetQueue_search(*m_QSR);
    return retval;
}

void CRemoteBlast::x_SubmitSearch(void)
{
    CRef<CBlast4_request_body> body(x_GetBlast4SearchRequestBody());
    CRef<CBlast4_reply> reply;
    
    try {
        reply = x_SendRequest(body);
    }
    catch(const CEofException&) {
        m_Errs.push_back("No response from server, cannot complete request.");
        return;
    }
    
    if (reply->CanGetBody()  &&
        reply->GetBody().GetQueue_search().CanGetRequest_id()) {
        
        m_RID = reply->GetBody().GetQueue_search().GetRequest_id();
    }
    
    x_SearchErrors(reply);
    
    if (m_Errs.empty()) {
        m_Pending = true;
    }
}

void CRemoteBlast::x_CheckResults(void)
{
    if (! m_Errs.empty()) {
        m_Pending = false;
    }
    
    if (! m_Pending) {
        return;
    }
    
    CRef<CBlast4_reply> r;
    
    bool try_again = true;
    
    while(try_again) {
        try {
            r = x_GetSearchResults();
            m_Pending = s_SearchPending(r);
            try_again = false;
        }
        catch(const CEofException&) {
            --m_ErrIgn;
            
            if (m_ErrIgn == 0) {
                m_Errs.push_back("No response from server, "
                                 "cannot complete request.");
                return;
            }
            
            SleepSec(10);
        }
    }
    
    if (! m_Pending) {
        x_SearchErrors(r);
        
        if (! m_Errs.empty()) {
            return;
        } else if (r->CanGetBody() && r->GetBody().IsGet_search_results()) {
            m_Reply = r;
        } else {
            m_Errs.push_back("Results were not a get-search-results reply");
        }
    }
}

// The input here is a hint as to whether the request might be ready.
// If the flag is true, then we are polling immediately after
// submission.  In this case, the results will not be ready, and so we
// skip the first results check to reduce net traffic.  If the flag is
// false, then the user is using the asynchronous interface, and we do
// not know how long it has been since the request was submitted.  In
// this case, we check the results before sleeping.
//
// If this was always set to 'true' then async mode would -always-
// sleep.  This is undesireable in the case where (for example) 100
// requests are batched together - the mandatory sleeps would add to a
// total of 1000 seconds, more than a quarter hour.
//
// If it were always specified as 'false', then synchronous mode would
// shoot off an immediate 'check results' as soon as the "submit"
// returned, which creates unnecessary traffic.
//
// Futher optimizations are no doubt possible.

void CRemoteBlast::x_PollUntilDone(EImmediacy immed, int timeout)
{
    if (eDebug == m_Verbose)
        cout << "polling " << 0 << endl;
    
    // Configuration - internal for now
    
    double start_sec = 10.0;
    double increment = 1.30;
    double max_sleep = 300.0;
    double max_time  = timeout;
    
    if (eDebug == m_Verbose)
        cout << "polling " << start_sec << "/" << increment << "/" << max_sleep << "/" << max_time << "/" << endl;
    
    // End config
    
    double sleep_next = start_sec;
    double sleep_totl = 0.0;
    
    if (eDebug == m_Verbose)
        cout << "line " << __LINE__ << " sleep next " << sleep_next << " sleep totl " << sleep_totl << endl;
    
    if (ePollAsync == immed) {
        x_CheckResults();
    }
    
    while (m_Pending && (sleep_totl < max_time)) {
        if (eDebug == m_Verbose)
            cout << " about to sleep " << sleep_next << endl;
        
        double max_left = max_time - sleep_totl;
        
        // Don't oversleep
        if (sleep_next > max_left) {
            sleep_next = max_left;
            
            // But never sleep less than 2
            if (sleep_next < 2.0)
                sleep_next = 2.0;
        }
        
        SleepSec(int(sleep_next));
        sleep_totl += sleep_next;
        
        if (eDebug == m_Verbose)
            cout << " done, total = " << sleep_totl << endl;
        
        if (sleep_next < max_sleep) {
            sleep_next *= increment;
            if (sleep_next > max_sleep) {
                sleep_next = max_sleep;
            }
        }
        
        if (eDebug == m_Verbose)
            cout << " next sleep time = " << sleep_next << endl;
        
        x_CheckResults();
    }
}

void CRemoteBlast::x_Init(CNcbiIstream& f)
{
      
      m_Archive.Reset(new CBlast4_archive);
      switch (ncbi::CFormatGuess().Format(f)) {
         case ncbi::CFormatGuess::eTextASN:
            f >> ncbi::MSerial_AsnText >> *m_Archive;
            break;
         case ncbi::CFormatGuess::eBinaryASN:
            f >> ncbi::MSerial_AsnBinary >> *m_Archive;
            break;
         case ncbi::CFormatGuess::eXml:
            f >> ncbi::MSerial_Xml >> *m_Archive;
            break;
         default:
            NCBI_THROW(CBlastException, eInvalidArgument,
                       "BLAST archive must be one of text ASN.1, binary ASN.1 or XML.");
      }     
      m_ReadFile = true;
      m_ErrIgn     = 5;
      m_Verbose    = eSilent;
      m_DbFilteringAlgorithmId = -1;
}

void CRemoteBlast::x_Init(CBlastOptionsHandle * opts)
{
    string p;
    string s;
    opts->GetOptions().GetRemoteProgramAndService_Blast3(p, s);
    
    x_Init(opts, p, s);
}

void CRemoteBlast::x_Init(CBlastOptionsHandle * opts_handle,
                          const string        & program,
                          const string        & service)
{
    if ((! opts_handle) || program.empty() || service.empty()) {
        if (! opts_handle) {
            NCBI_THROW(CBlastException, eInvalidArgument,
                       "NULL argument specified: options handle");
        }
        if (program.empty()) {
            NCBI_THROW(CBlastException, eInvalidArgument,
                       "NULL argument specified: program");
        }
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "NULL argument specified: service");
    }
    
    m_CBOH.Reset( opts_handle );
    m_ErrIgn     = 5;
    m_Pending    = false;
    m_Verbose    = eSilent;
    m_NeedConfig = eNeedAll;
    m_QueryMaskingLocations.clear();
    m_ReadFile = false;
    m_DbFilteringAlgorithmId = -1;
    
    m_QSR.Reset(new CBlast4_queue_search_request);
    
    m_QSR->SetProgram(m_Program = program);
    m_QSR->SetService(m_Service = service);
    
    m_NeedConfig = ENeedConfig(m_NeedConfig & ~(eProgram | eService));
    
    if (! (opts_handle && opts_handle->SetOptions().GetBlast4AlgoOpts())) {
        // This happens if you do not specify eRemote for the
        // CBlastOptions subclass constructor.
        
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "CRemoteBlast: No remote API options.");
    }
    m_ClientId = kEmptyStr;
}

void CRemoteBlast::x_Init(const string & RID)
{
    if (RID.empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Empty RID string specified");
    }
    
    m_RID        = RID;
    m_ErrIgn     = 5;
    m_Pending    = true;
    m_Verbose    = eSilent;
    m_NeedConfig = eNoConfig;
    m_QueryMaskingLocations.clear();
    m_ReadFile = false;
    m_DbFilteringAlgorithmId = -1;
}

void CRemoteBlast::x_SetAlgoOpts(void)
{
    CBlast4_parameters * algo_opts =
        m_CBOH->SetOptions().GetBlast4AlgoOpts();
    
    m_QSR->SetAlgorithm_options().Set() = *algo_opts;
}

// the "int" version is not actually used (no program options need it.)
void CRemoteBlast::x_SetOneParam(objects::CBlast4Field & field,
                                 const int * x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetInteger(*x);
    
    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(field.GetName());
    p->SetValue(*v);
    _ASSERT(field.Match(*p));
    
    m_QSR->SetProgram_options().Set().push_back(p);
}

void CRemoteBlast::x_SetOneParam(objects::CBlast4Field & field,
                                 CRef<objects::CBlast4_mask> mask)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetQuery_mask(*mask);
        
    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    // as dictated by internal/blast/interfaces/blast4/params.hpp
    p->SetName(field.GetName());
    p->SetValue(*v);
    _ASSERT(field.Match(*p));
    
    m_QSR->SetProgram_options().Set().push_back(p);
}

void CRemoteBlast::x_SetOneParam(objects::CBlast4Field & field,
                                 const list<int> * x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetInteger_list() = *x;
        
    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(field.GetName());
    p->SetValue(*v);
    _ASSERT(field.Match(*p));
    
    m_QSR->SetProgram_options().Set().push_back(p);
}

void CRemoteBlast::x_SetOneParam(objects::CBlast4Field & field,
                                 const char ** x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetString().assign((x && (*x)) ? (*x) : "");
        
    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(field.GetName());
    p->SetValue(*v);
    _ASSERT(field.Match(*p));
        
    m_QSR->SetProgram_options().Set().push_back(p);
}

void CRemoteBlast::SetQueries(CRef<objects::CBioseq_set> bioseqs)
{
    if (bioseqs.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Empty reference for query.");
    }
    
    m_Queries.Reset(new CBlast4_queries);
    m_Queries->SetBioseq_set(*bioseqs);
    
    m_QSR->SetQueries(*m_Queries);
    m_NeedConfig = ENeedConfig(m_NeedConfig & (~ eQueries));
}

void CRemoteBlast::SetQueries(CRef<objects::CBioseq_set> bioseqs,
                              const TSeqLocInfoVector& masking_locations)
{
    SetQueries(bioseqs);
    x_SetMaskingLocationsForQueries(masking_locations);
}

void CRemoteBlast::SetQueryMasks(const TSeqLocInfoVector& masking_locations)
{
    if (!m_QSR->IsSetQueries())
    {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Queries must be set before setting the masks.");
    }
    x_SetMaskingLocationsForQueries(masking_locations);
}

void CRemoteBlast::SetQueries(CRemoteBlast::TSeqLocList& seqlocs)
{
    if (seqlocs.empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Empty list for query.");
    }
    
    m_Queries.Reset(new CBlast4_queries);
    m_Queries->SetSeq_loc_list() = seqlocs;
    
    m_QSR->SetQueries(*m_Queries);
    m_NeedConfig = ENeedConfig(m_NeedConfig & (~ eQueries));
}

void CRemoteBlast::SetQueries(CRemoteBlast::TSeqLocList& seqlocs,
                              const TSeqLocInfoVector& masking_locations)
{
    SetQueries(seqlocs);
    x_SetMaskingLocationsForQueries(masking_locations);
}

void 
CRemoteBlast::x_SetMaskingLocationsForQueries(const TSeqLocInfoVector&
                                              masking_locations)
{
    _ASSERT(m_QSR->CanGetQueries());
    if (masking_locations.empty()) {
        return;
    }

    if (m_QSR->GetQueries().GetNumQueries() != masking_locations.size()) {
        CNcbiOstrstream oss;
        oss << "Mismatched number of queries (" 
             << m_QSR->GetQueries().GetNumQueries() 
             << ") and masking locations (" << masking_locations.size() << ")";
        NCBI_THROW(CBlastException, eInvalidArgument,
                   CNcbiOstrstreamToString(oss));
    }

    m_QueryMaskingLocations = const_cast<TSeqLocInfoVector&>(masking_locations);
}

/** Creates a Blast4-mask which is supposed to contain all masked locations for
 * a given query sequence and frame, all of which are in the packed_int
 * argument.
 */
static CRef<CBlast4_mask> 
s_CreateBlastMask(const CPacked_seqint& packed_int, EBlastProgramType program)
{
    CRef<CBlast4_mask> retval(new CBlast4_mask);

    CRef<CSeq_loc> seqloc(new CSeq_loc);
    ITERATE(CPacked_seqint::Tdata, masked_region, packed_int.Get()) {
        CRef<CSeq_interval> seqint
            (new CSeq_interval(const_cast<CSeq_id&>((*masked_region)->GetId()), 
                          (*masked_region)->GetFrom(), 
                          (*masked_region)->GetTo()));
        if ((*masked_region)->CanGetStrand() && 
            (*masked_region)->GetStrand() == eNa_strand_minus) {
            // skip this as locations on the negative strand are not
            // represented in the remote masking locations
            continue;   
        }
        seqloc->SetPacked_int().Set().push_back(seqint);
    }
    retval->SetLocations().push_back(seqloc);

    /// The frame can only be notset for protein queries or plus1 for
    /// nucleotide queries
    EBlast4_frame_type frame =
        (Blast_QueryIsNucleotide(program) || Blast_QueryIsTranslated(program))
        ? eBlast4_frame_type_plus1
        : eBlast4_frame_type_notset;
    retval->SetFrame(frame);

    return retval;
}

CBlast4_get_search_results_reply::TMasks
CRemoteBlast::ConvertToRemoteMasks(const TSeqLocInfoVector& masking_locations,
                                   EBlastProgramType program,
                                   vector<string>* warnings /* = NULL */)
{
    CBlast4_get_search_results_reply::TMasks retval;

    ITERATE(TSeqLocInfoVector, query_masks, masking_locations) {

        if (query_masks->empty()) {
            continue;
        }
        if (query_masks->HasNegativeStrandMasks() && warnings) {
            string w("Ignoring masked locations on negative strand for query");
            w += " '" + query_masks->front()->GetSeqId().AsFastaString();
            w += "'";
            warnings->push_back(w);
        }

        CRef<CPacked_seqint> packed_int = 
            query_masks->ConvertToCPacked_seqint();
        if (packed_int.Empty()) {
            continue;
        }
        CRef<CBlast4_mask> network_mask = 
            s_CreateBlastMask(*packed_int, program);
        _ASSERT(network_mask.NotEmpty());
        retval.push_back(network_mask);
    }
    return retval;
}

// Puts in each Blast4-mask all the masks that correspond to the same query 
// and the same frame.
void
CRemoteBlast::x_QueryMaskingLocationsToNetwork()
{
    if (m_QueryMaskingLocations.empty()) {
        return;
    }

    m_CBOH->GetOptions().GetRemoteProgramAndService_Blast3(m_Program, 
                                                           m_Service);
    EBlastProgramType program = NetworkProgram2BlastProgramType(m_Program,
                                                                m_Service);

    const CBlast4_get_search_results_reply::TMasks& network_masks = 
        CRemoteBlast::ConvertToRemoteMasks(m_QueryMaskingLocations,
                                           program, &m_Warn);
    ITERATE(CBlast4_get_search_results_reply::TMasks, itr, network_masks) {
        x_SetOneParam(B4Param_LCaseMask, *itr);
    }

}

void CRemoteBlast::SetQueries(CRef<objects::CPssmWithParameters> pssm)
{
    if (pssm.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Empty reference for query pssm.");
    }
    
    CPsiBlastValidate::Pssm(*pssm);
    
    string psi_program("blastp");
    string old_service("plain");
    string new_service("psi");
    
    if (m_QSR->GetProgram() != psi_program) {
        NCBI_THROW(CBlastException, eNotSupported,
                   "PSI-Blast is only supported for blastp.");
    }
    
    if (m_QSR->GetService().empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Internal error: service is not set.");
    }
    
    if ((m_QSR->GetService() != old_service) &&
        (m_QSR->GetService() != new_service)) {
        
        // Allowing "psi" allows the matrix to be set, then replaced.
        
        NCBI_THROW(CBlastException, eInvalidArgument,
                   string("PSI-Blast cannot also be ") +
                   m_QSR->GetService() + ".");
    }
    
    CRef<CBlast4_queries> queries_p(new CBlast4_queries);
    queries_p->SetPssm(*pssm);
    
    m_QSR->SetQueries(*queries_p);
    m_NeedConfig = ENeedConfig(m_NeedConfig & (~ eQueries));
    
    m_QSR->SetService(new_service);
}

string CRemoteBlast::GetErrors(void)
{
    if (m_Errs.empty()) {
        return string();
    }
    
    string rvalue = m_Errs[0];
    
    for(unsigned i = 1; i<m_Errs.size(); i++) {
        rvalue += "\n";
        rvalue += m_Errs[i];
    }
    
    return rvalue;
}

string CRemoteBlast::GetWarnings(void)
{
    if (m_Warn.empty()) {
        return string();
    }
    
    string rvalue = m_Warn[0];
    
    for(unsigned i = 1; i<m_Warn.size(); i++) {
        rvalue += "\n";
        rvalue += m_Warn[i];
    }
    
    return rvalue;
}

const vector<string> & CRemoteBlast::GetWarningVector()
{
    return m_Warn;
}

const vector<string> & CRemoteBlast::GetErrorVector()
{
    return m_Errs;
}

CRemoteBlast::CRemoteBlast(CNcbiIstream&  f)
{
    x_Init(f);
}

CRemoteBlast::CRemoteBlast(const string & RID)
{
    x_Init(RID);
}

CRemoteBlast::CRemoteBlast(CBlastOptionsHandle * algo_opts)
{
    x_Init(algo_opts);
}

CRemoteBlast::CRemoteBlast(CRef<IQueryFactory>         queries,
                           CRef<CBlastOptionsHandle>   opts_handle,
                           const CSearchDatabase     & db)
{
    x_Init(opts_handle, db);
    x_InitQueries(queries);
}

static void
s_FlattenBioseqSet(const CBioseq_set & bss, list< CRef<CBioseq> > & seqs)
{
    if (bss.CanGetSeq_set()) {
        ITERATE(CBioseq_set::TSeq_set, iter, bss.GetSeq_set()) {
            if (iter->NotEmpty()) {
                const CSeq_entry & entry = **iter;
                
                if (entry.IsSeq()) {
                    CBioseq & bs = const_cast<CBioseq &>(entry.GetSeq());
                    seqs.push_back(CRef<CBioseq>(& bs));
                } else {
                    _ASSERT(entry.IsSet());
                    s_FlattenBioseqSet(entry.GetSet(), seqs);
                }
            }
        }
    }
}

CRemoteBlast::CRemoteBlast(CRef<IQueryFactory>       queries,
                           CRef<CBlastOptionsHandle> opts_handle,
                           CRef<IQueryFactory>       subjects)
{
    x_Init(&* opts_handle);
    x_InitQueries(queries);
    SetSubjectSequences(subjects);
}

void CRemoteBlast::x_InitQueries(CRef<IQueryFactory> queries)
{
    if (queries.Empty()) {
        NCBI_THROW(CBlastException,
                   eInvalidArgument,
                   "Error: No queries specified");
    }
    
    CRef<IRemoteQueryData> Q(queries->MakeRemoteQueryData());
    CRef<CBioseq_set> bss = Q->GetBioseqSet();
    IRemoteQueryData::TSeqLocs sll = Q->GetSeqLocs();

    if (bss.Empty() && sll.empty()) {
        NCBI_THROW(CBlastException,
                   eInvalidArgument,
                   "Error: No query data.");
    }

    // Check if there are any range restrictions applied and if local IDs are
    // being used to determine how to specify the query sequence(s)
    
    bool has_local_ids = false;
    
    if ( !sll.empty() ) {
        // Only one range restriction can be sent in this protocol
        if (sll.front()->IsInt()) {
            const int kStart((int)sll.front()->GetStart(eExtreme_Positional));
            const int kStop((int)sll.front()->GetStop(eExtreme_Positional));
            const int kRangeLength = kStop - kStart + 1;

            _ASSERT(bss->CanGetSeq_set());
            _ASSERT( !bss->GetSeq_set().empty() );
            _ASSERT(bss->GetSeq_set().front()->IsSeq());
            _ASSERT(bss->GetSeq_set().front()->GetSeq().CanGetInst());
            const int kFullLength =
                bss->GetSeq_set().front()->GetSeq().GetInst().GetLength();

            if (kFullLength != kRangeLength) {
                x_SetOneParam(B4Param_RequiredStart, &kStart);
                x_SetOneParam(B4Param_RequiredEnd, &kStop);
            }
        }
    
        ITERATE(IRemoteQueryData::TSeqLocs, itr, sll) {
            if (IsLocalId((*itr)->GetId())) {
                has_local_ids = true;
                break;
            }
        }
    } 

    TSeqLocInfoVector user_specified_masks;
    x_ExtractUserSpecifiedMasks(queries, user_specified_masks);
    
    if (has_local_ids) {
        SetQueries(bss, user_specified_masks);
    } else {
        SetQueries(sll, user_specified_masks);
    }
}

void
CRemoteBlast::x_ExtractUserSpecifiedMasks(CRef<IQueryFactory> query_factory,
                                          TSeqLocInfoVector& masks)
{
    masks.clear();
    CObjMgr_QueryFactory* objmgrqf = NULL;
    if ( (objmgrqf = dynamic_cast<CObjMgr_QueryFactory*>(&*query_factory))) {
        masks = objmgrqf->ExtractUserSpecifiedMasks();
    }
}

CRemoteBlast::CRemoteBlast(CRef<objects::CPssmWithParameters>   pssm,
                           CRef<CBlastOptionsHandle>            opts_handle,
                           const CSearchDatabase              & db)
{
    if (pssm.Empty()) {
        NCBI_THROW(CBlastException,
                   eInvalidArgument,
                   "Error: No PSSM specified");
    }
    
    x_Init(opts_handle, db);
    
    SetQueries(pssm);
}

void CRemoteBlast::x_Init(CRef<CBlastOptionsHandle>   opts_handle,
                          const CSearchDatabase     & db)
{
    if (opts_handle.Empty()) {
        NCBI_THROW(CBlastException,
                   eInvalidArgument,
                   "Error: No options specified");
    }
    
    if (db.GetDatabaseName().empty()) {
        NCBI_THROW(CBlastException,
                   eInvalidArgument,
                   "Error: No database specified");
    }
    
    x_Init(&* opts_handle);
    
    SetDatabase(db.GetDatabaseName());
    SetEntrezQuery(db.GetEntrezQueryLimitation().c_str());
    // Set the GI list restriction
    {{
        const CSearchDatabase::TGiList& tmplist = db.GetGiListLimitation();
        if ( !tmplist.empty() ) {
            list<Int4> gilist;
            copy(tmplist.begin(), tmplist.end(), back_inserter(gilist));
            SetGIList(gilist);
        }
    }}

    // Set the negative GI list
    {{
        const CSearchDatabase::TGiList& tmplist = 
            db.GetNegativeGiListLimitation();
        if ( !tmplist.empty() ) {
            list<Int4> gilist;
            copy(tmplist.begin(), tmplist.end(), back_inserter(gilist));
            SetNegativeGIList(gilist);
        }
    }}

    // Set the filtering algorithms
    SetDbFilteringAlgorithmId(db.GetFilteringAlgorithm());
}

CRemoteBlast::~CRemoteBlast()
{
}

void CRemoteBlast::SetGIList(const list<Int4> & gi_list)
{
    if (gi_list.empty()) {
        return;
    } else {
        NCBI_THROW(CBlastException, eNotSupported, 
           "Submitting gi lists remotely is currently not supported");
    }
    x_SetOneParam(B4Param_GiList, & gi_list);
    
    m_GiList.clear();
    copy(gi_list.begin(), gi_list.end(), back_inserter(m_GiList));
}

void CRemoteBlast::SetDbFilteringAlgorithmId(int algo_id)
{
    if (algo_id == -1) 
        return;

    x_SetOneParam(B4Param_DbFilteringAlgorithmId, &algo_id);
    m_DbFilteringAlgorithmId = algo_id;
}

void CRemoteBlast::SetNegativeGIList(const list<Int4> & gi_list)
{
    if (gi_list.empty()) {
        return;
    } else {
        NCBI_THROW(CBlastException, eNotSupported, 
           "Submitting negative gi lists remotely is currently not supported");
    }
    x_SetOneParam(B4Param_NegativeGiList, & gi_list);
    
    m_NegativeGiList.clear();
    copy(gi_list.begin(), gi_list.end(), back_inserter(m_NegativeGiList));
}

void CRemoteBlast::x_SetDatabase(const string & x)
{
   EBlast4_residue_type rtype(eBlast4_residue_type_unknown);

    if (m_Program == "blastp" ||
        m_Program == "blastx" ||
        (m_Program == "tblastn" && m_Service == "rpsblast")) {

        rtype = eBlast4_residue_type_protein;
    } else {
        rtype = eBlast4_residue_type_nucleotide;
    }

    m_Dbs.Reset(new CBlast4_database);
    m_Dbs->SetName(x);
    m_Dbs->SetType(rtype);

    m_SubjectSequences.clear();
}

void CRemoteBlast::SetDatabase(const string & x)
{
    if (x.empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "NULL specified for database.");
    }
        
    CRef<CBlast4_subject> subject_p(new CBlast4_subject);
    subject_p->SetDatabase(x);
    m_QSR->SetSubject(*subject_p);
    m_NeedConfig = ENeedConfig(m_NeedConfig & (~ eSubject));

    x_SetDatabase(x);
}

void CRemoteBlast::SetSubjectSequences(CRef<IQueryFactory> subjects)
{
    CRef<IRemoteQueryData> Q(subjects->MakeRemoteQueryData());
    CRef<CBioseq_set> bss = Q->GetBioseqSet();
    
    if (bss.Empty()) {
        NCBI_THROW(CBlastException,
                   eInvalidArgument,
                   "Error: No query data.");
    }
    
    list< CRef<CBioseq> > seqs;
    s_FlattenBioseqSet(*bss, seqs);
    
    SetSubjectSequences(seqs);
}

void 
CRemoteBlast::SetSubjectSequences(const list< CRef< objects::CBioseq > > & subj)
{
    CRef<CBlast4_subject> subject_p(new CBlast4_subject);
    subject_p->SetSequences() = subj;
    
    m_QSR->SetSubject(*subject_p);
    m_NeedConfig = ENeedConfig(m_NeedConfig & (~ eSubject));
    
    m_SubjectSequences = subj;
    m_Dbs.Reset();
}

void CRemoteBlast::SetEntrezQuery(const char * x)
{
    if (!x) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "NULL specified for entrez query.");
    }
    
    if (*x) { // Ignore empty strings.
        x_SetOneParam(B4Param_EntrezQuery, &x);
        m_EntrezQuery.assign(x);
    }
}

bool CRemoteBlast::SubmitSync(void)
{
    return SubmitSync( x_DefaultTimeout() );
}

const string & CRemoteBlast::GetRID(void)
{
    return m_RID;
}

void CRemoteBlast::SetVerbose(EDebugMode verb)
{
    m_Verbose = verb;
}

/// The default timeout is 3.5 hours.
const int CRemoteBlast::x_DefaultTimeout(void)
{
    return int(3600*3.5);
}

static const string 
    kNoRIDSpecified("Cannot fetch query info: No RID was specified.");

static const string 
    kNoArchiveFile("Cannot fetch query info: No archive file.");

void
CRemoteBlast::x_GetRequestInfo()
{
    if(m_ReadFile == true)
        x_GetRequestInfoFromFile();
    else
        x_GetRequestInfoFromRID();
    
}


void
CRemoteBlast::x_GetRequestInfoFromFile()
{
    // Archive file must be present to fetch.
    if (!m_Archive || m_Archive.Empty()) {
        NCBI_THROW(CRemoteBlastException, eServiceNotAvailable,
                   kNoArchiveFile);
    }
    
    if (m_Archive->CanGetRequest())
    {
        CRef<objects::CBlast4_request> request(&m_Archive->SetRequest());
        CImportStrategy strategy(request);
        m_Program   = strategy.GetProgram();
        m_Service   = strategy.GetService();
        m_CreatedBy = strategy.GetCreatedBy();
        m_Queries    = strategy.GetQueries();
        m_AlgoOpts.Reset( & strategy.GetAlgoOptions() );
        m_ProgramOpts.Reset( & strategy.GetProgramOptions() );

        if (strategy.GetSubject()->IsDatabase())
            x_SetDatabase(strategy.GetSubject()->GetDatabase());
        else
            m_SubjectSequences = strategy.GetSubject()->SetSequences();

        // Ignore return value, want side effect of setting fields.
        GetSearchOptions();
        return;
    }
    
    NCBI_THROW(CRemoteBlastException, eServiceNotAvailable,
               "Could not get information from archive file.");
}

void
CRemoteBlast::x_GetRequestInfoFromRID()
{
    // Must have an RID to do this.
    
    if (m_RID.empty()) {
        NCBI_THROW(CRemoteBlastException, eServiceNotAvailable,
                   kNoRIDSpecified);
    }
    
    // First... poll until done.
    
    x_PollUntilDone(ePollAsync, x_DefaultTimeout());
    
    if (x_GetState() != eDone) {
        NCBI_THROW(CRemoteBlastException, eServiceNotAvailable,
                   "Polling terminated, but search is in incomplete state.");
    }
    
    // Build the request
    
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    CRef<CBlast4_request> request(new CBlast4_request);
    if ( !m_ClientId.empty() ) {
        request->SetIdent(m_ClientId);
    }
    
    body->SetGet_request_info().SetRequest_id(m_RID);
    request->SetBody(*body);
    
    CRef<CBlast4_reply> reply(new CBlast4_reply);
    
    if (eDebug == m_Verbose) {
        NcbiCout << MSerial_AsnText << *request << endl;
    }
    
    try {
        CStopWatch sw(CStopWatch::eStart);
        
        if (eDebug == m_Verbose) {
            NcbiCout << "Starting network transaction (" << sw.Elapsed() << ")" << endl;
        }
        
        // Send request.
        CBlast4Client().Ask(*request, *reply);
        
        if (eDebug == m_Verbose) {
            NcbiCout << "Done network transaction (" << sw.Elapsed() << ")" << endl;
        }
    }
    catch(const CEofException&) {
        NCBI_THROW(CRemoteBlastException, eServiceNotAvailable,
                   "No response from server, cannot complete request.");
    }

    if (eDebug == m_Verbose) {
        NcbiCout << MSerial_AsnText << *reply << endl;
    }
    
    if (reply->CanGetBody()) {
        if (reply->GetBody().IsGet_request_info()) {
            CRef<CBlast4_get_request_info_reply> grir
                (& reply->SetBody().SetGet_request_info());
            
            m_Dbs.Reset( & grir->SetDatabase() );
            
            m_Program   = grir->GetProgram();
            m_Service   = grir->GetService();
            m_CreatedBy = grir->GetCreated_by();
            
            m_Queries    .Reset( & grir->SetQueries() );
            m_AlgoOpts   .Reset( & grir->SetAlgorithm_options() );
            m_ProgramOpts.Reset( & grir->SetProgram_options() );
            
            return;
        }
    }
    
    NCBI_THROW(CRemoteBlastException, eServiceNotAvailable,
               "Could not get information from search.");
}


CRef<CBlast4_database>
CRemoteBlast::GetDatabases()
{
    if (! m_Dbs.Empty()) {
        return m_Dbs;
    }
    
    x_GetRequestInfo();
    
    return m_Dbs;
}

bool
CRemoteBlast::IsDbSearch()
{
    if (m_Dbs.Empty() && m_SubjectSequences.empty())
       x_GetRequestInfo();

    if (! m_Dbs.Empty()) {
       return true;
    }
    return false;
}

list< CRef<objects::CBioseq> > 
CRemoteBlast::GetSubjectSequences()
{
    if (! m_SubjectSequences.empty()) {
        return m_SubjectSequences;
    }
    
    x_GetRequestInfo();
    
    return m_SubjectSequences;
}

string
CRemoteBlast::GetProgram()
{
    if (! m_Program.empty()) {
        return m_Program;
    }
    
    x_GetRequestInfo();
    
    return m_Program;
}

string
CRemoteBlast::GetService()
{
    if (! m_Service.empty()) {
        return m_Service;
    }
    
    x_GetRequestInfo();
    
    return m_Service;
}

string
CRemoteBlast::GetCreatedBy()
{
    if (! m_CreatedBy.empty()) {
        return m_CreatedBy;
    }
    
    x_GetRequestInfo();
    
    return m_CreatedBy;
}

CRef<CBlast4_queries>
CRemoteBlast::GetQueries()
{
    if (! m_Queries.Empty()) {
        return m_Queries;
    }
    
    x_GetRequestInfo();
    
    return m_Queries;
}

EBlastProgramType
NetworkProgram2BlastProgramType(const string& program, const string& service)
{
    _ASSERT(!program.empty());
    _ASSERT(!service.empty());

    EBlastProgramType retval = eBlastTypeUndefined;
    Int2 rv = BlastProgram2Number(program.c_str(), &retval);
    _ASSERT(rv == 0);
    rv += 0;    // to eliminate compiler warning
    _ASSERT(retval != eBlastTypeUndefined);

    if (service == "rpsblast") {

        if (program == "blastp") {
            retval = eBlastTypeRpsBlast;
        } else if (program == "tblastn") {
            retval = eBlastTypeRpsTblastn;
        } else {
            abort();
        }

    } 
    
    if (service == "psi") {
        _ASSERT(program == "blastp");
        retval = eBlastTypePsiBlast;
    }

    return retval;
}


EBlast4_frame_type
FrameNumber2NetworkFrame(int frame, EBlastProgramType program)
{
    if (Blast_QueryIsTranslated(program)) {
        switch (frame) {
        case  1: return eBlast4_frame_type_plus1;
        case  2: return eBlast4_frame_type_plus2;
        case  3: return eBlast4_frame_type_plus3;
        case -1: return eBlast4_frame_type_minus1;
        case -2: return eBlast4_frame_type_minus2;
        case -3: return eBlast4_frame_type_minus3;
        default: abort();
        }
        _TROUBLE;
    }
    
    if (Blast_QueryIsNucleotide(program)) {
        _ASSERT(frame == -1 || frame == 1);
        // For some reason, the return value here is not set...
        return eBlast4_frame_type_notset;
    }
    
    return eBlast4_frame_type_notset;
}

CSeqLocInfo::ETranslationFrame
NetworkFrame2FrameNumber(objects::EBlast4_frame_type frame, 
                         EBlastProgramType program)
{
    if (Blast_QueryIsTranslated(program)) {
        switch (frame) {
        case eBlast4_frame_type_plus1:  return CSeqLocInfo::eFramePlus1;
        case eBlast4_frame_type_plus2:  return CSeqLocInfo::eFramePlus2;
        case eBlast4_frame_type_plus3:  return CSeqLocInfo::eFramePlus3;
        case eBlast4_frame_type_minus1: return CSeqLocInfo::eFrameMinus1;
        case eBlast4_frame_type_minus2: return CSeqLocInfo::eFrameMinus2;
        case eBlast4_frame_type_minus3: return CSeqLocInfo::eFrameMinus3;
        default: abort();
        }
        _TROUBLE;
    }
    
    // The BLAST formatter expects nucleotide masks to have a 'not-set' strand,
    // which implies that they're on the plus strand. If they're set to
    // anything else, it won't display them.
    //if (Blast_QueryIsNucleotide(program)) {
    //    _ASSERT(frame == eBlast4_frame_type_plus1);
    //    return CSeqLocInfo::eFramePlus1;
    //}
    
    return CSeqLocInfo::eFrameNotSet;
}

CRef<CBlastOptionsHandle> CRemoteBlast::GetSearchOptions()
{
    if (m_CBOH.Empty()) {
        string program_s = GetProgram();
        string service_s = GetService();
        
        CBlastOptionsBuilder bob(program_s, service_s);
        
        m_CBOH = bob.GetSearchOptions(m_AlgoOpts, m_ProgramOpts, &m_Task);
        
        if (bob.HaveEntrezQuery()) {
            m_EntrezQuery = bob.GetEntrezQuery();
        }
        
        if (bob.HaveFirstDbSeq()) {
            m_FirstDbSeq = bob.GetFirstDbSeq();
        }
        
        if (bob.HaveFinalDbSeq()) {
            m_FinalDbSeq = bob.GetFinalDbSeq();
        }
        
        if (bob.HaveGiList()) {
            m_GiList = bob.GetGiList();
        }

        if (bob.HasDbFilteringAlgorithmId() &&
            bob.GetDbFilteringAlgorithmId() != -1) {
            m_DbFilteringAlgorithmId = bob.GetDbFilteringAlgorithmId();
        }

        if (bob.HaveNegativeGiList()) {
            m_NegativeGiList = bob.GetNegativeGiList();
        }
    }
    
    return m_CBOH;
}

/// Extract the query IDs from a CBioseq_set
/// @param bss CBioseq_set object used as source [in]
/// @param query_ids where the query_ids will be added [in|out]
static void s_ExtractQueryIdsFromBioseqSet(const CBioseq_set& bss,
                                           CSearchResultSet::TQueryIdVector&
                                           query_ids)
{
    // sacrifice speed for protection against infinite loops
    CTypeConstIterator<objects::CBioseq> itr(ConstBegin(bss, eDetectLoops)); 
    for (; itr; ++itr) {
        query_ids.push_back(FindBestChoice(itr->GetId(), CSeq_id::BestRank));
    }
}

void 
CRemoteBlast::x_ExtractQueryIds(CSearchResultSet::TQueryIdVector& query_ids)
{
    query_ids.clear();
    CRef<CBlast4_queries> queries = GetQueries();
    query_ids.reserve(queries->GetNumQueries());
    _ASSERT(queries);

    if (queries->IsPssm()) {
        const CSeq_entry& seq_entry = queries->GetPssm().GetQuery();
        if (seq_entry.IsSeq()) {
            query_ids.push_back(FindBestChoice(seq_entry.GetSeq().GetId(), 
                                               CSeq_id::BestRank));
        } else {
            _ASSERT(seq_entry.IsSet());
            s_ExtractQueryIdsFromBioseqSet(seq_entry.GetSet(), query_ids);
        }
    } else if (queries->IsSeq_loc_list()) {
        query_ids.reserve(queries->GetSeq_loc_list().size());
        ITERATE(CBlast4_queries::TSeq_loc_list, i, queries->GetSeq_loc_list()) {
            CConstRef<CSeq_id> id((*i)->GetId());
            query_ids.push_back(id);
        }
    } else {
        _ASSERT(queries->IsBioseq_set());
        s_ExtractQueryIdsFromBioseqSet(queries->GetBioseq_set(), query_ids);
    }
}

/// Submit the search and return the results.
/// @return Search results.
CRef<CSearchResultSet> CRemoteBlast::GetResultSet()
{
    CRef<CSearchResultSet> retval;
    if (m_ReadFile == false)
       SubmitSync();
    
    TSeqAlignVector alignments = GetSeqAlignSets();
    
    /* Process errors and warnings */
    TSearchMessages search_messages;
    {
        const vector<string> & W = GetWarningVector();
        const vector<string> & E = GetErrorVector();
        
        TQueryMessages query_messages;
        
        // Represents the context of the error, not the error id.
        int err = kBlastMessageNoContext;
        
        ITERATE(vector<string>, itw, W) {
            CRef<CSearchMessage>
                sm(new CSearchMessage(eBlastSevWarning, err, *itw));
            
            query_messages.push_back(sm);
        }
        
        ITERATE(vector<string>, ite, E) {
            err = kBlastMessageNoContext;
            
            CRef<CSearchMessage>
                sm(new CSearchMessage(eBlastSevError, err, *ite));
            
            query_messages.push_back(sm);
        }

        // Since there is no way to report per-query messages, all
        // warnings and errors are applied to all queries.
        search_messages.insert(search_messages.end(), 
                               alignments.empty() ? 1 : alignments.size(), 
                               query_messages);

        if (eDebug == m_Verbose) {
            NcbiCout << "Error/Warning messages: '" 
                     << search_messages.ToString() << "'" << endl;
        }
    }

    CSearchResultSet::TQueryIdVector query_ids;
    int i = 0;
    ITERATE(TSeqAlignVector, itr, alignments) {
        CRef<CSeq_align> first_aln = (*itr)->Get().front();
        query_ids.push_back(CConstRef<CSeq_id>(&first_aln->GetSeq_id(0)));
        if (eDebug == m_Verbose) {
            NcbiCout << "Query # " << ++i << ": '"
                     << query_ids.back()->AsFastaString() << "'" << endl 
                     << MSerial_AsnText << **itr << endl;
        }
    }
    
    
    if (alignments.empty()) {
        // this is required by the CSearchResultSet ctor
        alignments.resize(1);    
        try { x_ExtractQueryIds(query_ids); } 
        catch (const CRemoteBlastException& e) {
            if (e.GetMsg() == kNoRIDSpecified) {
                retval.Reset(new CSearchResultSet(alignments, search_messages));
                return retval;
            }
            throw;
        }
    }

    /* Build the ancillary data structure */
    CSearchResultSet::TAncillaryVector ancill_vector;
    {
        /* Get the effective search space */
        const string kTarget("Effective search space used: ");
        list<string> search_stats = GetSearchStats();
        Int8 effective_search_space = 0;
        NON_CONST_ITERATE(list<string>, itr, search_stats) {
            if (NStr::Find(*itr, kTarget) != NPOS) {
                NStr::ReplaceInPlace(*itr, kTarget, kEmptyStr);
                effective_search_space = 
                    NStr::StringToInt8(*itr, NStr::fConvErr_NoThrow);
                break;
            }
        }

        /* Get the Karlin-Altschul parameters */
        bool found_gapped = false, found_ungapped = false;
        pair<double, double> lambdas, Ks, Hs;
        TKarlinAltschulBlocks ka_blocks = GetKABlocks();

        ITERATE(TKarlinAltschulBlocks, itr, ka_blocks) {
            if ((*itr)->GetGapped()) {
                lambdas.second = (*itr)->GetLambda();
                Ks.second = (*itr)->GetK();
                Hs.second = (*itr)->GetH();
                found_gapped = true;
            } else {
                lambdas.first = (*itr)->GetLambda();
                Ks.first = (*itr)->GetK();
                Hs.first = (*itr)->GetH();
                found_ungapped = true;
            }

            if (found_gapped && found_ungapped) {
                break;
            }
        }

        // N.B.: apparently the BLAST3 protocol doesn't send PSI-BLAST Karlin &
        // Altschul parameters, so we don't set the is_psiblast
        // CBlastAncillaryData constructor argument
        CRef<CBlastAncillaryData> ancillary_data
            (new CBlastAncillaryData(lambdas, Ks, Hs, effective_search_space));
        ancill_vector.insert(ancill_vector.end(), alignments.size(),
                             ancillary_data);
    }
    
    TSeqLocInfoVector masks = GetMasks();
    retval.Reset(new CSearchResultSet(query_ids, alignments, search_messages,
                                      ancill_vector, &masks));
    retval->SetRID(GetRID());
    return retval;
}

CRef<objects::CBlast4_request> 
ExtractBlast4Request(CNcbiIstream& in)
{
    // First try to read a Blast4-get-search-strategy-reply...
    CRef<CBlast4_get_search_strategy_reply> b4_ss_reply;
    bool succeeded = false;
    try {
        switch (CFormatGuess().Format(in)) {
        case CFormatGuess::eBinaryASN:
            b4_ss_reply.Reset(new CBlast4_get_search_strategy_reply);
            in >> MSerial_AsnBinary >> *b4_ss_reply;
            succeeded = true;
            break;

        case CFormatGuess::eTextASN:
            b4_ss_reply.Reset(new CBlast4_get_search_strategy_reply);
            in >> MSerial_AsnText >> *b4_ss_reply;
            succeeded = true;
            break;

        case CFormatGuess::eXml:
            {
                auto_ptr<CObjectIStream> is(
                    CObjectIStream::Open(eSerial_Xml, in));
                dynamic_cast<CObjectIStreamXml*>
                    (is.get())->SetEnforcedStdXml(true);
                b4_ss_reply.Reset(new CBlast4_get_search_strategy_reply);
                *is >> *b4_ss_reply;
                succeeded = true;
            }
            break;

        default:
            _ASSERT(b4_ss_reply.Empty());
        }
    } catch (const CException&) {
        succeeded = false;
    }

    CRef<CBlast4_request> retval;
    if (succeeded) {
        retval.Reset(&b4_ss_reply->Set());
        return retval;
    }
    b4_ss_reply.Reset();
    in.seekg(0);

    // Go for broke and try the Blast4-request...
    retval.Reset(new CBlast4_request);
    switch (CFormatGuess().Format(in)) {
    case CFormatGuess::eBinaryASN:
        in >> MSerial_AsnBinary >> *retval;
        break;

    case CFormatGuess::eTextASN:
        in >> MSerial_AsnText >> *retval;
        break;

    case CFormatGuess::eXml:
        {
            auto_ptr<CObjectIStream> is(
                CObjectIStream::Open(eSerial_Xml, in));
            dynamic_cast<CObjectIStreamXml*>
                (is.get())->SetEnforcedStdXml(true);
            *is >> *retval;
        }
        break;

    default:
        NCBI_THROW(CSerialException, eInvalidData, 
                   "Unrecognized input format ");
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
