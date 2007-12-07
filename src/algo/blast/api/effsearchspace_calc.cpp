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
* Author:  Christiam Camacho
*
*/

/// @file effsearchspace_calc.cpp
/// Defines auxiliary class to calculate the effective search space

#include <ncbi_pch.hpp>

#include <algo/blast/api/effsearchspace_calc.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/core/blast_setup.h>
#include "blast_memento_priv.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Memento class to save, null out, and restore the filtering options of the
/// CBlastOptionsMemento object passed to its constructor
/// This prevents side effects (like filtering the query sequence) to occur
/// during calculation of the effective search space
class CFilteringMemento
{
public:
    /// Parametrized constructor
    /// @param opts_memento snapshopt of the BLAST options [in]
    CFilteringMemento(CBlastOptionsMemento* opts_memento)
        : m_OptsMemento(opts_memento), m_FilterString(0), m_FilterOpts(0)
    {
        m_FilterString = opts_memento->m_QueryOpts->filter_string;
        m_FilterOpts = opts_memento->m_QueryOpts->filtering_options;
        opts_memento->m_QueryOpts->filter_string = NULL;
        SBlastFilterOptionsNew(&opts_memento->m_QueryOpts->filtering_options,
                               eEmpty);
    }

    /// Destructor
    ~CFilteringMemento()
    {
        m_OptsMemento->m_QueryOpts->filter_string = m_FilterString;
        SBlastFilterOptionsFree(m_OptsMemento->m_QueryOpts->filtering_options);
        m_OptsMemento->m_QueryOpts->filtering_options = m_FilterOpts;
    }

private:
    CBlastOptionsMemento* m_OptsMemento;    /**< snapshopt of BLAST options */
    char* m_FilterString;                   /**< original filtering string
                                              specified in m_OptsMemento */
    SBlastFilterOptions* m_FilterOpts;      /**< original filtering options
                                              specified in m_OptsMemento */
};

CEffectiveSearchSpaceCalculator::CEffectiveSearchSpaceCalculator
    (CRef<IQueryFactory> query_factory,
     const CBlastOptions& options, 
     Int4 db_num_seqs, 
     Int8 db_num_bases,
     BlastScoreBlk* sbp)
    : m_QueryFactory(query_factory), m_Program(options.GetProgramType())
{
    bool delete_sbp = false;
    CRef<ILocalQueryData> local_data =
        m_QueryFactory->MakeLocalQueryData(&options);
    m_QueryInfo = local_data->GetQueryInfo();

    auto_ptr<CBlastOptionsMemento> opts_memento
        (const_cast<CBlastOptionsMemento*>(options.CreateSnapshot()));
    {{
        TSearchMessages messages;

        CFilteringMemento
            fm(const_cast<CBlastOptionsMemento*>(opts_memento.get()));
        if (sbp == NULL)
        {
              sbp = CSetupFactory::CreateScoreBlock(opts_memento.get(), local_data, NULL, messages);
              delete_sbp = true;
        }
        _ASSERT(!messages.HasMessages());
    }}

    CBlastEffectiveLengthsParameters eff_len_params;

    /* Initialize the effective length parameters with real values of
       database length and number of sequences */
    BlastEffectiveLengthsParametersNew(opts_memento->m_EffLenOpts, 
                                       db_num_bases, db_num_seqs, 
                                       &eff_len_params);

    Int2 status = 
        BLAST_CalcEffLengths(m_Program, opts_memento->m_ScoringOpts,
                             eff_len_params, sbp, m_QueryInfo, NULL);

    if (delete_sbp == true)
        sbp = BlastScoreBlkFree(sbp);

    if (status) {
        NCBI_THROW(CBlastException, eCoreBlastError, 
                   "BLAST_CalcEffLengths failed");
    }        
}

Int8
CEffectiveSearchSpaceCalculator::GetEffSearchSpace(size_t query_index) const
{
    _ASSERT((Int4)query_index < m_QueryInfo->num_queries);
    return BlastQueryInfoGetEffSearchSpace(m_QueryInfo, m_Program, query_index);
}

Int8
CEffectiveSearchSpaceCalculator::GetEffSearchSpaceForContext(size_t ctx_index) const
{
    _ASSERT((Int4)ctx_index <= m_QueryInfo->last_context);
    return m_QueryInfo->contexts[ctx_index].eff_searchsp;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

