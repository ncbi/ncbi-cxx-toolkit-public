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
* Author:  Tom Madden
*
* ===========================================================================
*/

/// @file search_strategy.cpp
/// Imports and exports search strategies

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <serial/iterator.hpp>
#include <algo/blast/api/search_strategy.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_options_builder.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>

#include "psiblast_aux_priv.hpp"

#include <objects/blast/blast__.hpp>
#include <objects/blast/names.hpp>
#include <objects/seqset/Seq_entry.hpp>

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

CImportStrategy::CImportStrategy(CRef<objects::CBlast4_request> request)
 : m_Request(request)
{

    if (m_Request.Empty()) {
         NCBI_THROW(CBlastException, eInvalidArgument, "CBlast4_request empty");
    }
    if (m_Request->CanGetBody() && !m_Request->GetBody().IsQueue_search() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, "No body in CBlast4_request");
    }
    m_Data.reset(new CImportStrategyData);
    m_Data->valid = false;
}


void 
CImportStrategy::FetchData() const
{
    if (m_Data->valid == false)
    {
        const CBlast4_queue_search_request& req(m_Request->GetBody().GetQueue_search());

        CBlastOptionsBuilder bob(req.GetProgram(), req.GetService(),
                                      CBlastOptions::eBoth);

        // Create the BLAST options
        const CBlast4_parameters* algo_opts(0);
        const CBlast4_parameters* prog_opts(0);

        if (req.CanGetAlgorithm_options()) {
                algo_opts = &req.GetAlgorithm_options();
        }
        if (req.CanGetProgram_options()) {
                prog_opts = &req.GetProgram_options();
        }

        // The option builder is invalid until the next call.
        m_Data->m_OptionsHandle = bob.GetSearchOptions(algo_opts, prog_opts, &m_Data->m_Task);
        m_Data->m_QueryRange = bob.GetRestrictedQueryRange();
        m_Data->m_FilteringID = bob.GetDbFilteringAlgorithmId();
        m_Data->valid = true;
    }
}

CRef<blast::CBlastOptionsHandle> 
CImportStrategy::GetOptionsHandle() const
{
    if (!m_Data->valid)
           FetchData();
    
    return m_Data->m_OptionsHandle;
}

string 
CImportStrategy::GetTask() const
{
    if (!m_Data->valid)
           FetchData();
    
    return m_Data->m_Task;
}

string 
CImportStrategy::GetProgram() const
{
    const CBlast4_queue_search_request& req(m_Request->GetBody().GetQueue_search());
    return req.GetProgram();
}

string 
CImportStrategy::GetCreatedBy() const
{
    string ident(m_Request->GetIdent());
    return ident;
}

TSeqRange 
CImportStrategy::GetQueryRange() const
{
    if (!m_Data->valid)
           FetchData();
    
    return m_Data->m_QueryRange;
}

int 
CImportStrategy::GetDBFilteringID() const
{
    if (!m_Data->valid)
           FetchData();
    
    return m_Data->m_FilteringID;
}

string 
CImportStrategy::GetService() const
{
    const CBlast4_queue_search_request& req(m_Request->GetBody().GetQueue_search());
    return req.GetService();
}

CRef<objects::CBlast4_queries>
CImportStrategy::GetQueries()
{
    CBlast4_queue_search_request& req(m_Request->SetBody().SetQueue_search());
    CRef<objects::CBlast4_queries> retval(&req.SetQueries());
    return retval;
}

CRef<objects::CBlast4_subject> 
CImportStrategy::GetSubject()
{
    CBlast4_queue_search_request& req(m_Request->SetBody().SetQueue_search());
    CRef<objects::CBlast4_subject> retval(&req.SetSubject());
    return retval;
}

objects::CBlast4_parameters&
CImportStrategy::GetAlgoOptions()
{
    CBlast4_queue_search_request& req(m_Request->SetBody().SetQueue_search());
    return req.SetAlgorithm_options();
}

objects::CBlast4_parameters&
CImportStrategy::GetProgramOptions()
{
    CBlast4_queue_search_request& req(m_Request->SetBody().SetQueue_search());
    return req.SetProgram_options();
}

/*
 * CExportStrategy
 */
CExportStrategy::CExportStrategy(CRef<CBlastOptionsHandle> opts_handle, const string & client_id)
								:m_QueueSearchRequest(new CBlast4_queue_search_request),
								 m_ClientId(client_id)
{
	x_Process_BlastOptions(opts_handle);
}

CExportStrategy::CExportStrategy(CRef<IQueryFactory>         query,
             					CRef<CBlastOptionsHandle>  	opts_handle,
             					CRef<CSearchDatabase> 		db,
             					const string & 				client_id)
								:m_QueueSearchRequest(new CBlast4_queue_search_request),
								 m_ClientId(client_id)
{
	x_Process_BlastOptions(opts_handle);
	x_Process_Query(query);
	x_Process_SearchDb(db);
}

CExportStrategy::CExportStrategy(CRef<IQueryFactory>       	query,
								 CRef<CBlastOptionsHandle> 	opts_handle,
								 CRef<IQueryFactory>       	subject,
								 const string & 			client_id)
								:m_QueueSearchRequest(new CBlast4_queue_search_request),
								 m_ClientId(client_id)
{
	x_Process_BlastOptions(opts_handle);
	x_Process_Query(query);
	x_Process_Subject(subject);
}

CExportStrategy::CExportStrategy(CRef<CPssmWithParameters>	pssm,
             					 CRef<CBlastOptionsHandle>  opts_handle,
             					 CRef<CSearchDatabase> 		db,
             					 const string & 			client_id)
								:m_QueueSearchRequest(new CBlast4_queue_search_request),
								 m_ClientId(client_id)
{
	x_Process_BlastOptions(opts_handle);
	x_Process_Pssm(pssm);
	x_Process_SearchDb(db);
}

CRef<objects::CBlast4_request> CExportStrategy::GetSearchStrategy(void)
{
	CRef<CBlast4_request> retval(new CBlast4_request);
	if (!m_ClientId.empty())
	{
        retval->SetIdent(m_ClientId);
    }

    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetQueue_search(*m_QueueSearchRequest);
    retval->SetBody(*body);
    return retval;
}

void CExportStrategy::ExportSearchStrategy_ASN1(CNcbiOstream* out)
{
	*out << MSerial_AsnText << *GetSearchStrategy();
}

void CExportStrategy::x_Process_BlastOptions(CRef<CBlastOptionsHandle>  & opts_handle)
{
    if (opts_handle.Empty())
    {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Empty reference for CBlastOptionsHandle.");
    }

    string program;
    string service;
    opts_handle->GetOptions().GetRemoteProgramAndService_Blast3(program, service);

    if (program.empty())
    {
            NCBI_THROW(CBlastException, eInvalidArgument,
                       "NULL argument specified: program");
    }

    if (service.empty())
    {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "NULL argument specified: service");
    }

    m_QueueSearchRequest->SetProgram(program);
    m_QueueSearchRequest->SetService(service);

    CBlast4_parameters *	algo_opts = opts_handle->SetOptions().GetBlast4AlgoOpts();
    if (NULL == algo_opts )
    {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "NULL argument specified: algo options");
    }

    m_QueueSearchRequest->SetAlgorithm_options().Set() = *algo_opts;
}

void CExportStrategy::x_Process_SearchDb(CRef<CSearchDatabase> & db)
{
    if (db.Empty())
    {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Empty reference for CSearchDatabase.");
    }

	if (db->GetDatabaseName().empty())
	{
	    NCBI_THROW(CBlastException, eInvalidArgument,
	               "Error: No database specified");
	}

	// Set database Name
	CRef<CBlast4_subject> subject_p(new CBlast4_subject);
	subject_p->SetDatabase(db->GetDatabaseName());
	m_QueueSearchRequest->SetSubject(*subject_p);

	// Set Entrez Query Limitation
	string entrez_query_limit = db->GetEntrezQueryLimitation();
	if(!entrez_query_limit.empty())
	{
		CRef<CBlast4_parameter> p(new CBlast4_parameter);
		p->SetName(B4Param_EntrezQuery.GetName());

		CRef<CBlast4_value> v(new CBlast4_value);
		v->SetString().assign(entrez_query_limit);
		p->SetValue(*v);
		_ASSERT(B4Param_EntrezQuery.Match(*p));

		m_QueueSearchRequest->SetProgram_options().Set().push_back(p);
	}

    // Set the GI List Limitation
    const CSearchDatabase::TGiList& gi_list_limit = db->GetGiListLimitation();
    if (!gi_list_limit.empty())
    {
    	x_AddParameterToProgramOptions(B4Param_GiList, gi_list_limit);
    }

    // Set the negative GI list
    const CSearchDatabase::TGiList& neg_gi_list = db->GetNegativeGiListLimitation();
    if (!neg_gi_list.empty())
    {
    	x_AddParameterToProgramOptions(B4Param_NegativeGiList, neg_gi_list);
    }

    // Set the filtering algorithms
    int algo_id = db->GetFilteringAlgorithm();
    if (algo_id != -1)
    {
       	x_AddParameterToProgramOptions(B4Param_DbFilteringAlgorithmId, algo_id);
    }
}

/* Prerequisite for calling x_Process_Pssm:-
 * Must call x_Process_BlastOptions first
 */

void CExportStrategy::x_Process_Pssm(CRef<CPssmWithParameters> & pssm)
{
    if (pssm.Empty())
    {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Empty reference for query pssm.");
    }

    // Throw exception if pssm is invalid
    CPsiBlastValidate::Pssm(*pssm);

    string psi_program("blastp");
    string old_service("plain");
    string new_service("psi");

    if (m_QueueSearchRequest->GetProgram() != psi_program)
    {
        NCBI_THROW(CBlastException, eNotSupported,
                   "PSI-Blast is only supported for blastp.");
    }

    if ((m_QueueSearchRequest->GetService() != old_service) &&
        (m_QueueSearchRequest->GetService() != new_service))
    {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   string("PSI-Blast cannot also be ") +
                   m_QueueSearchRequest->GetService() + ".");
    }

    CRef<CBlast4_queries> queries_p(new CBlast4_queries);
    queries_p->SetPssm(*pssm);

    m_QueueSearchRequest->SetQueries(*queries_p);
    m_QueueSearchRequest->SetService(new_service);
}

void CExportStrategy::x_Process_Query(CRef<IQueryFactory> & query)
{
    if (query.Empty())
    {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Error: No queries specified");
    }

    CRef<IRemoteQueryData> remote_query(query->MakeRemoteQueryData());
    CRef<CBioseq_set> bioseq_set = remote_query->GetBioseqSet();
    IRemoteQueryData::TSeqLocs seqloc_list = remote_query->GetSeqLocs();

    if (bioseq_set.Empty() && seqloc_list.empty())
    {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Error: No query data.");
    }

    // Check if there are any range restrictions applied and if local IDs are
    // being used to determine how to specify the query sequence(s)

    bool has_local_ids = false;

    if (!seqloc_list.empty())
    {
        // Only one range restriction can be sent in this protocol
        if (seqloc_list.front()->IsInt())
        {
            const int kStart((int)seqloc_list.front()->GetStart(eExtreme_Positional));
            const int kStop((int)seqloc_list.front()->GetStop(eExtreme_Positional));
            const int kRangeLength = kStop - kStart + 1;

            _ASSERT(bioseq_set->CanGetSeq_set());
            _ASSERT(!bioseq_set->GetSeq_set().empty());
            _ASSERT(bioseq_set->GetSeq_set().front()->IsSeq());
            _ASSERT(bioseq_set->GetSeq_set().front()->GetSeq().CanGetInst());
            const int kFullLength =
                bioseq_set->GetSeq_set().front()->GetSeq().GetInst().GetLength();

            if (kFullLength != kRangeLength)
            {
            	x_AddParameterToProgramOptions(B4Param_RequiredStart, kStart);
            	x_AddParameterToProgramOptions(B4Param_RequiredEnd, kStop);
            }
        }

        ITERATE(IRemoteQueryData::TSeqLocs, itr, seqloc_list)
        {
            if (IsLocalId((*itr)->GetId()))
            {
                has_local_ids = true;
                break;
            }
        }
    }

    CObjMgr_QueryFactory* objmgrqf = dynamic_cast<CObjMgr_QueryFactory*>(&*query);
    if ( NULL != objmgrqf )
    {
    	TSeqLocInfoVector user_specified_masks = objmgrqf->ExtractUserSpecifiedMasks();
        if (!user_specified_masks.empty())
        {
        	EBlastProgramType program = NetworkProgram2BlastProgramType(
        									m_QueueSearchRequest->GetProgram(),
        									m_QueueSearchRequest->GetService());

        	CBlast4_get_search_results_reply::TMasks network_masks =
        	    CRemoteBlast::ConvertToRemoteMasks(user_specified_masks, program);

        	NON_CONST_ITERATE(CBlast4_get_search_results_reply::TMasks, itr, network_masks)
        	{
            	CRef<CBlast4_parameter> p(new CBlast4_parameter);
            	p->SetName(B4Param_LCaseMask.GetName());

            	CRef<CBlast4_value> v(new CBlast4_value);
            	v->SetQuery_mask(**itr);
            	p->SetValue(*v);
            	_ASSERT(B4Param_LCaseMask.Match(*p));

            	m_QueueSearchRequest->SetProgram_options().Set().push_back(p);
        	}
        }
    }

    CRef<CBlast4_queries> Q(new CBlast4_queries);

    if (has_local_ids)
    {
        Q->SetBioseq_set(*bioseq_set);
    }
    else
    {
    	Q->SetSeq_loc_list() = seqloc_list;
    }
    m_QueueSearchRequest->SetQueries(*Q);

}

void CExportStrategy::x_Process_Subject(CRef<IQueryFactory> & subject)
{
    CRef<IRemoteQueryData> remote_query(subject->MakeRemoteQueryData());
    CRef<CBioseq_set> bioseq_set = remote_query->GetBioseqSet();

    if (bioseq_set.Empty())
    {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Error: No query data.");
    }

    list< CRef<CBioseq> > bioseq_list;
    FlattenBioseqSet(*bioseq_set, bioseq_list);

    CRef<CBlast4_subject> subject_bioseq(new CBlast4_subject);
    subject_bioseq->SetSequences() = bioseq_list;

    m_QueueSearchRequest->SetSubject(*subject_bioseq);
}

// This method add CBlast4Parameters (integer only) to program options list
void CExportStrategy::x_AddParameterToProgramOptions(objects::CBlast4Field & field,
                                 	 	    		 const int int_value)
{
	CRef<CBlast4_parameter> p(new CBlast4_parameter);
	p->SetName(field.GetName());

	CRef<CBlast4_value> v(new CBlast4_value);
	v->SetInteger(int_value);
	p->SetValue(*v);
	_ASSERT(field.Match(*p));

	m_QueueSearchRequest->SetProgram_options().Set().push_back(p);
}

void CExportStrategy::x_AddParameterToProgramOptions(objects::CBlast4Field & field,
                                 	 	    		 const vector<int> & int_list)
{
	list<int> tmp_list;
    copy(int_list.begin(), int_list.end(), back_inserter(tmp_list));

    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(field.GetName());

    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetInteger_list() = tmp_list;
    p->SetValue(*v);
    _ASSERT(field.Match(*p));

    m_QueueSearchRequest->SetProgram_options().Set().push_back(p);
}


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
