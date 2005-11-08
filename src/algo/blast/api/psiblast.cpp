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
 * Author:  Christiam Camacho
 *
 */

/** @file psiblast.cpp
 * Implementation of CPsiBlast.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/psiblast.hpp>
#include "subject_psi_database.hpp"
#include "psiblast_impl.hpp"

#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CPsiBlast::CPsiBlast(CRef<IQueryFactory> query_factory,
                     const CSearchDatabase& dbinfo, 
                     CConstRef<CPSIBlastOptionsHandle> options)
: m_Subject(new CBlastSubjectDb(dbinfo)), m_Impl(0)
{
    try {
        m_Impl = new CPsiBlastImpl
            (query_factory, 
             m_Subject, 
             CConstRef<CBlastProteinOptionsHandle>(options.GetPointer()));
    } catch (const CBlastException&) {
        delete m_Subject;
        throw;
    }
}

CPsiBlast::CPsiBlast(CRef<objects::CPssmWithParameters> pssm,
                     const CSearchDatabase& dbinfo, 
                     CConstRef<CPSIBlastOptionsHandle> options)
: m_Subject(new CBlastSubjectDb(dbinfo)), m_Impl(0)
{
    try { m_Impl = new CPsiBlastImpl(pssm, m_Subject, options); }
    catch (const CBlastException&) {
        delete m_Subject;
        throw;
    }
}

CPsiBlast::~CPsiBlast()
{
    if (m_Impl) {
        delete m_Impl;
    }
    if (m_Subject) {
        delete m_Subject;
    }
}

void
CPsiBlast::SetPssm(CConstRef<objects::CPssmWithParameters> pssm)
{
    m_Impl->SetPssm(pssm);
}

CConstRef<objects::CPssmWithParameters>
CPsiBlast::GetPssm() const
{
    return m_Impl->GetPssm();
}

CRef<CSearchResults>
CPsiBlast::Run()
{
    return m_Impl->Run();
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
