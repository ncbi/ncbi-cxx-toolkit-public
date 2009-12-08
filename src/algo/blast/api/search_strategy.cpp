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

/// @file search_strateg.cpp
/// Imports and exports search strategies

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <serial/iterator.hpp>
#include <algo/blast/api/search_strategy.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_options_builder.hpp>

#include <objects/blast/blast__.hpp>
#include <objects/blast/names.hpp>

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
 : m_Request(request), m_OptionsSet(false)
{

    if (m_Request.Empty()) {
         NCBI_THROW(CBlastException, eInvalidArgument, "CBlast4_request empty");
    }
    if (m_Request->CanGetBody() && !m_Request->GetBody().IsQueue_search() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, "No body in CBlast4_request");
    }
}

CRef<blast::CBlastOptionsHandle> 
CImportStrategy::GetOptionsHandle()
{
    if (!m_OptionsSet)
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
        m_OptionsHandle = bob.GetSearchOptions(algo_opts, prog_opts, &m_Task);
        m_QueryRange = bob.GetRestrictedQueryRange();
        m_OptionsSet = true;
    }
    return m_OptionsHandle;
}

string 
CImportStrategy::GetTask()
{
    if (!m_OptionsSet)
    {
        // This populates task;
        CRef<blast::CBlastOptionsHandle> handle = GetOptionsHandle();
    }

    return m_Task;
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
CImportStrategy::GetQueryRange()
{
    if (!m_OptionsSet)
    {
        // This populates query range;
        CRef<blast::CBlastOptionsHandle> handle = GetOptionsHandle();
    }

    return m_QueryRange;
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




END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
