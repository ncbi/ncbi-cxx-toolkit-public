#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
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
 * Author:  Kevin Bealer
 *
 */

/** @file remote_search.cpp
 * This file implements the uniform Blast search interface in terms of
 * the blast4 network API via the CRemoteBlast library.
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <algo/blast/api/remote_search.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/// Supporting elements

//
// Factory
//

CRef<ISeqSearch>
CRemoteSearchFactory::GetSeqSearch()
{
    return CRef<ISeqSearch>(new CRemoteSeqSearch());
}

CRef<IPssmSearch>
CRemoteSearchFactory::GetPssmSearch()
{
    return CRef<IPssmSearch>(new CRemotePssmSearch());
}

CRef<CBlastOptionsHandle>
CRemoteSearchFactory::GetOptions(EProgram program)
{
    CRef<CBlastOptionsHandle> opts
        (CBlastOptionsFactory::Create(program, CBlastOptions::eRemote));
    
    return opts;
}

//
// Seq Search
//

CRemoteBlast & CRemoteSeqSearch::x_RemoteBlast()
{
    if (m_RemoteBlast.Empty()) {
        // Verify all parts accounted for....
        if (m_SearchOpts.Empty()) {
            NCBI_THROW(CSearchException, eConfigErr, "No options specified");
        }
        
        if (m_Queries.Empty()) {
            NCBI_THROW(CSearchException, eConfigErr, "No queries specified");
        }
        
        if (m_Subject.Empty() || m_Subject->GetDatabaseName().empty()) {
            NCBI_THROW(CSearchException, eConfigErr, 
                       "No database name specified");
        }
        
        // .. Done...
        
        m_RemoteBlast.Reset(new CRemoteBlast(& * m_SearchOpts));
        m_RemoteBlast->SetDatabase(m_Subject->GetDatabaseName());
        const string& kEntrezQuery = m_Subject->GetEntrezQueryLimitation();
        if ( !kEntrezQuery.empty() ) {
            m_RemoteBlast->SetEntrezQuery(kEntrezQuery.c_str());
        }

        const CSearchDatabase::TGiList& kGiList = 
            m_Subject->GetGiListLimitation();
        if ( !kGiList.empty() ) {
            list<TGi> temp(kGiList.begin(), kGiList.end());
            m_RemoteBlast->SetGIList(temp);
        }
        
        CRef<CBioseq_set> bss        = m_Queries->GetBioseqSet();
        IRemoteQueryData::TSeqLocs sll = m_Queries->GetSeqLocs();
        
        if ((bss.Empty()) && (sll.empty())) {
            NCBI_THROW(CSearchException, eConfigErr, 
                       "Empty queries object specified.");
        }
        
        if (bss.NotEmpty()) {
            m_RemoteBlast->SetQueries(bss);
        } else {
            _ASSERT(! sll.empty());
            m_RemoteBlast->SetQueries(sll);
        }
    }
    
    return *m_RemoteBlast;
}

/// Build a result set from results in a remote blast search.
///
/// The remote blast object will be queried for results and these will
/// be used to build a CSearchResultSet.  If the search has not yet
/// completed, this function will wait until it has.
///
/// @param rb The remote blast object representing the search.
/// @return The results of the search as a CSearchResultSet.
static CRef<CSearchResultSet>
s_BuildResultsRemote(CRemoteBlast & rb);

CRef<CSearchResultSet>
CRemoteSeqSearch::Run()
{
    // Calling Run() directly always queues a new search.
    m_RemoteBlast.Reset();
    //x_RemoteBlast().SetVerbose();
    x_RemoteBlast().SubmitSync();
    
    const vector<string> & w = x_RemoteBlast().GetWarningVector();
    m_Warnings.insert(m_Warnings.end(), w.begin(), w.end());
    
    return s_BuildResultsRemote(*m_RemoteBlast);
}

void CRemoteSeqSearch::SetOptions(CRef<CBlastOptionsHandle> opts)
{
    m_SearchOpts = opts;
}

void CRemoteSeqSearch::SetSubject(CConstRef<CSearchDatabase> subject)
{
    m_Subject = subject;
}

void CRemoteSeqSearch::SetQueryFactory(CRef<IQueryFactory> query_factory)
{
    if (query_factory.Empty()) {
        NCBI_THROW(CSearchException, eConfigErr, 
                   "CRemoteSeqSearch: empty query factory was specified.");
    }
    
    m_Queries.Reset(query_factory->MakeRemoteQueryData());
}

/// CRemoteBlast does not separate each hit to the query in discontinuous
/// Seq-aligns, so we do it here. This functionality might be merged with
/// CRemoteBlast::GetSeqAlignSets() in the future
static TSeqAlignVector
s_SplitAlignVectorBySubjects(TSeqAlignVector seqaligns)
{
    // For each query...
    NON_CONST_ITERATE(TSeqAlignVector, itr, seqaligns) {
        CRef<CSeq_align_set> seq_align = *itr;

        CRef<CSeq_align_set> new_seq_align(new CSeq_align_set);

        // set the current Seq-id to an invalid gi
        CConstRef<CSeq_id> current_subject(new CSeq_id(CSeq_id::e_Gi, 1));
        // list of HSPs for a single query-subject pair
        CRef<CSeq_align> current_hsp_list;

        // for each HSP ...
        ITERATE(CSeq_align_set::Tdata, hsp_itr, seq_align->Get()) {

            const int kSubjectIndex = 1;
            CConstRef<CSeq_id> subj_id(& (*hsp_itr)->GetSeq_id(kSubjectIndex));

            // new subject sequence (hit) found
            if (subj_id->Compare(*current_subject) == CSeq_id::e_NO) {

                current_subject = subj_id;

                if (current_hsp_list.NotEmpty()) {
                    new_seq_align->Set().push_back(current_hsp_list);
                }
                current_hsp_list.Reset(new CSeq_align);
                current_hsp_list->SetType(CSeq_align::eType_disc);
                current_hsp_list->SetDim(2);
                current_hsp_list->SetSegs().SetDisc().Set().push_back(*hsp_itr);

            } else {
                // same subject sequence as in previous iteration
                current_hsp_list->SetSegs().SetDisc().Set().push_back(*hsp_itr);
            }
        }
        if (current_hsp_list.NotEmpty()) {
            new_seq_align->Set().push_back(current_hsp_list);
        }

        *itr = new_seq_align;
    }
    return seqaligns;
}

static CRef<CSearchResultSet>
s_BuildResultsRemote(CRemoteBlast & rb)
{
    // This cascades the warnings and errors: all queries get all
    // errors and warnings.  At the moment, none of the remote (or for
    // that matter, local) code seems to have a way to categorize
    // errors by type and query.
    
    // If the query number were known, and the error number were
    // known, it is possible that the user could (in some cases) cope
    // with the error or possibly salvage data from the non-failing
    // requests.
    
    // Comments:
    //
    // 1. In how many (if any) client code scenarios does error
    //    recovery makes sense?
    //
    // 2. What kinds of errors that are recoverable?
    //
    // 3. Does the user ever need to know more than that a request
    //    found results, found nothing, or produced an error message?
    //
    // 4. If a single query fails, how do we avoid pairing the fatal
    //    error message with non-failing requests.
    
    TQueryMessages msgs;
    CRef<CSearchMessage> msg;
    
    // Convert warnings and errors into CSearchMessage objects.
    
    ITERATE(vector<string>, iter, rb.GetWarningVector()) {
        msg.Reset(new CSearchMessage(eBlastSevError, -1, *iter));
        msgs.push_back(msg);
    }
    
    ITERATE(vector<string>, iter, rb.GetErrorVector()) {
        msg.Reset(new CSearchMessage(eBlastSevError, -1, *iter));
        msgs.push_back(msg);
    }
    
    TSeqAlignVector aligns =
        s_SplitAlignVectorBySubjects(rb.GetSeqAlignSets());
    
    // Cascade the messages -- this will result in a lot of CRef<>
    // sharing but hopefully not too much actual computation.
    
    TSearchMessages msg_vec;
    
    for(size_t i = 0; i<aligns.size(); i++) {
        msg_vec.push_back(msgs);
    }
    
    return CRef<CSearchResultSet>(new CSearchResultSet(aligns, msg_vec));
}


//
// Psi Search
//

void CRemotePssmSearch::SetOptions(CRef<CBlastOptionsHandle> opts)
{
    m_SearchOpts  = opts;
    m_RemoteBlast.Reset(new CRemoteBlast(& * opts));
}

void CRemotePssmSearch::SetSubject(CConstRef<CSearchDatabase> subject)
{
    m_Subject = subject;
}

CRemoteBlast & CRemotePssmSearch::x_RemoteBlast()
{
    if (m_RemoteBlast.Empty()) {
        // Verify all parts accounted for....
        if (m_SearchOpts.Empty()) {
            NCBI_THROW(CSearchException, eConfigErr, "No options specified");
        }
        
        if (m_Pssm.Empty()) {
            NCBI_THROW(CSearchException, eConfigErr, "No queries specified");
        }
        
        if (m_Subject.Empty() || m_Subject->GetDatabaseName().empty()) {
            NCBI_THROW(CSearchException, eConfigErr, 
                       "No database name specified");
        }
        
        // .. Done...
        
        m_RemoteBlast.Reset(new CRemoteBlast(& * m_SearchOpts));
        m_RemoteBlast->SetDatabase(m_Subject->GetDatabaseName());
        m_RemoteBlast->SetQueries(m_Pssm);

        const string& kEntrezQuery = m_Subject->GetEntrezQueryLimitation();
        if ( !kEntrezQuery.empty() ) {
            m_RemoteBlast->SetEntrezQuery(kEntrezQuery.c_str());
        }

        const CSearchDatabase::TGiList& kGiList = 
            m_Subject->GetGiListLimitation();
        if ( !kGiList.empty() ) {
            list<TGi> temp(kGiList.begin(), kGiList.end());
            m_RemoteBlast->SetGIList(temp);
        }
    }
    
    return *m_RemoteBlast;
}

CRef<CSearchResultSet>
CRemotePssmSearch::Run()
{
    // Calling Run() directly always queues a new search.
    m_RemoteBlast.Reset();
    //x_RemoteBlast().SetVerbose();
    
    x_RemoteBlast().SubmitSync();
    
    const vector<string> & w = x_RemoteBlast().GetWarningVector();
    m_Warnings.insert(m_Warnings.end(), w.begin(), w.end());
    
    return s_BuildResultsRemote(*m_RemoteBlast);
}


void CRemotePssmSearch::SetQuery(CRef<objects::CPssmWithParameters> pssm)
{
    if (pssm.Empty()) {
        NCBI_THROW(CSearchException, eConfigErr, 
                   "CRemotePssmSearch: empty query object was specified.");
    }
    
    m_Pssm = pssm;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
