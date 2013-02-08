#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "";
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
 * Author:  Greg Boratyn
 *
 */

/** @file deltablast.cpp
 * Implementation of CDeltaBlast.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/psiblast.hpp>

// PSSM Engine includes
#include <algo/blast/api/pssm_engine.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/Pssm.hpp>

#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/deltablast.hpp>


/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast);

CDeltaBlast::CDeltaBlast(CRef<IQueryFactory> query_factory,
                         CRef<CLocalDbAdapter> blastdb,
                         CRef<CLocalDbAdapter> domain_db,
                         CConstRef<CDeltaBlastOptionsHandle> options)
    : m_Queries(query_factory),
      m_Subject(blastdb),
      m_DomainDb(domain_db),
      m_Options(options)
{
    x_Validate();
}


CDeltaBlast::CDeltaBlast(CRef<IQueryFactory> query_factory,
                         CRef<CLocalDbAdapter> blastdb,
                         CRef<CLocalDbAdapter> domain_db,
                         CConstRef<CDeltaBlastOptionsHandle> options,
                         CRef<CBlastRPSOptionsHandle> rps_options)
    : m_Queries(query_factory),
      m_Subject(blastdb),
      m_DomainDb(domain_db),
      m_Options(options),
      m_RpsOptions(rps_options)
{
    x_Validate();
}


CRef<CSearchResultSet> CDeltaBlast::Run(void)
{
    CPSIBlastOptions opts;
    PSIBlastOptionsNew(&opts);
    opts->inclusion_ethresh = m_Options->GetDomainInclusionThreshold();

    // Make domain search
    m_DomainResults = x_FindDomainHits();

    CRef<ILocalQueryData> query_data =
        m_Queries->MakeLocalQueryData(&m_Options->GetOptions());

    // get query sequences
    BLAST_SequenceBlk* seq_blk = query_data->GetSequenceBlk();
    BlastQueryInfo* query_info = query_data->GetQueryInfo();
    vector<Uint1*> query_seq(query_data->GetNumQueries(), NULL);
    vector<size_t> query_lens(query_data->GetNumQueries(), 0);
    for (size_t i=0;i < query_data->GetNumQueries();i++) {
        query_seq[i] =
            seq_blk->sequence_start + query_info->contexts[i].query_offset + 1;

        query_lens[i] = query_info->contexts[i].query_length;
    }

    _ASSERT(m_DomainResults->size() == query_seq.size());

    // TO DO: Allow for more information in diagnostics
    CPSIDiagnosticsRequest diags;
    diags.Reset(PSIDiagnosticsRequestNewEx(false));
    
    // for each results from single query
    for (size_t i=0;i < m_DomainResults->size();i++) {
    
        CRef<CCddInputData> pssm_input(
                               new CCddInputData(query_seq[i],
                                          query_lens[i],
                                         (*m_DomainResults)[i].GetSeqAlign(),
                                         *opts.Get(),
                                         m_DomainDb->GetDatabaseName(),
                                         m_Options->GetMatrixName(),
                                         m_Options->GetGapOpeningCost(),
                                         m_Options->GetGapExtensionCost(),
                                         diags));
                                                     
    
        CRef<CPssmEngine> pssm_engine;
        pssm_engine.Reset(new CPssmEngine(pssm_input.GetNonNullPointer()));

        // compute pssm
        m_Pssm.push_back(pssm_engine->Run());

        // pssm may not have query id set if there were no CDD hits
        // in such case set query id in the PSSM
        if (!m_Pssm.back()->GetPssm().GetQuery().GetSeq().GetFirstId()) {
            CRef<CSeq_id> query_id(const_cast<CSeq_id*>(
                                       query_data->GetSeq_loc(i)->GetId()));

            m_Pssm.back()->SetPssm().SetQuery().SetSeq().SetId().push_back(
                                                                    query_id);
        }
        
        // Run psiblast with computed pssm
        CConstRef<CPSIBlastOptionsHandle> psi_opts(
                                             m_Options.GetNonNullPointer());
        CPsiBlast psiblast(m_Pssm.back(), m_Subject, psi_opts);
        psiblast.SetNumberOfThreads(GetNumberOfThreads());
        CRef<CSearchResultSet> results = psiblast.Run();

        if (m_Results.Empty()) {
            m_Results = results;
        }
        else {
            NON_CONST_ITERATE (CSearchResultSet, res, *results) {
                m_Results->push_back(*res);
            }
        }
    }

    // return search results
    return m_Results;
}


CConstRef<CPssmWithParameters> CDeltaBlast::GetPssm(int index) const
{
    if (index >= (int)m_Pssm.size()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "PSSM index too large");
    }
    CConstRef<CPssmWithParameters> pssm(m_Pssm[index].GetNonNullPointer());
    return pssm;
}

CRef<CPssmWithParameters> CDeltaBlast::GetPssm(int index)
{
    if (index >= (int)m_Pssm.size()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "PSSM index too large");
    }
    CRef<CPssmWithParameters> pssm(m_Pssm[index].GetNonNullPointer());
    return pssm;
}

CRef<CSearchResultSet> CDeltaBlast::x_FindDomainHits(void)
{
    CRef<CBlastOptionsHandle> opts;

    // if the m_RpsOptions is set, then use it here
    if (m_RpsOptions.NotEmpty()) {
        opts.Reset(dynamic_cast<CBlastOptionsHandle*>(
                                    m_RpsOptions.GetNonNullPointer()));
    }
    else {
        // otherwise create new options handle
        opts = CBlastOptionsFactory::Create(eRPSBlast);
        opts->SetEvalueThreshold(m_Options->GetDomainInclusionThreshold());
        opts->SetFilterString("F");
    }

    CLocalBlast blaster(m_Queries, opts, m_DomainDb);
    return blaster.Run();
}

void CDeltaBlast::x_Validate(void)
{
    if (m_Options.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing options");
    }

    if (m_Queries.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing query");
    }

    if (m_Subject.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Missing database or subject sequences");
    }

    if (m_DomainDb.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing domain database");
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
