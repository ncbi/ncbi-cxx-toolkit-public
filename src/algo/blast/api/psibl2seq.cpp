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
 * Author:  Christiam Camacho
 *
 */

/** @file psibl2seq.cpp
 * Implementation of CPsiBl2Seq.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/psibl2seq.hpp>
#include "psiblast_impl.hpp"
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CPsiBl2Seq::CPsiBl2Seq(CRef<objects::CPssmWithParameters> pssm,
                       CRef<IQueryFactory> subject,
                       CConstRef<CPSIBlastOptionsHandle> options)
{
    x_InitSubject(subject, options.GetPointer());
    m_Impl = new CPsiBlastImpl(pssm, m_Subject, options);
}

CPsiBl2Seq::CPsiBl2Seq(CRef<objects::CPssmWithParameters> pssm,
                       CRef<CLocalDbAdapter> subject,
                       CConstRef<CPSIBlastOptionsHandle> options)
{
    m_Subject = subject;
    m_Impl = new CPsiBlastImpl(pssm, m_Subject, options);
}

CPsiBl2Seq::CPsiBl2Seq(CRef<IQueryFactory> query,
                       CRef<IQueryFactory> subject,
                       CConstRef<CBlastProteinOptionsHandle> options)
{
    x_InitSubject(subject, options.GetPointer());
    m_Impl = new CPsiBlastImpl(query, m_Subject, options);
}

void
CPsiBl2Seq::x_InitSubject(CRef<IQueryFactory> subject, 
                          const CBlastOptionsHandle* options)
{
    if ( !options ) {
        NCBI_THROW(CBlastException, eInvalidArgument, "Missing options");
    }
    CConstRef<CBlastOptionsHandle> opts_handle(options);
    m_Subject.Reset(new CLocalDbAdapter(subject, opts_handle));
}

CPsiBl2Seq::~CPsiBl2Seq()
{
    if (m_Impl) {
        delete m_Impl;
    }
}

CRef<CSearchResultSet>
CPsiBl2Seq::Run()
{
    m_Impl->SetResultType(eSequenceComparison);
    return m_Impl->Run();
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
